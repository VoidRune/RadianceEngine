#include "GltfImporter.h"
#include "EnvironmentBaker.h"
#include "Tangents.h"
#include "TextureBaker.h"
#include "Rendering/ModelSurfaces.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iterator>
#include <map>
#include <optional>
#include <string_view>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tinygltf/tiny_gltf.h"

namespace Game
{
	namespace
	{
		constexpr uint64_t HashSeed = 0xCBF29CE484222325ull;
		constexpr float MatchTolerance = 1e-3f;

		struct Geometry
		{
			std::vector<ModelVertex> Vertices;
			std::vector<uint32_t> Indices;
		};

		struct Track
		{
			std::vector<float> Times;
			std::vector<float> Values;
			int Components = 0;
			bool Step = false;
			bool Cubic = false;
		};

		uint64_t Hash(uint64_t hash, const void* data, size_t size)
		{
			const uint8_t* bytes = static_cast<const uint8_t*>(data);
			for (size_t i = 0; i < size; i++)
				hash = (hash ^ bytes[i]) * 0x100000001B3ull;
			return hash;
		}

		glm::vec3 FlipX(const glm::vec3& value)
		{
			return { -value.x, value.y, value.z };
		}

		glm::quat FlipX(const glm::quat& value)
		{
			return glm::quat(value.w, value.x, -value.y, -value.z);
		}

		glm::mat4 FlipX(const glm::mat4& value)
		{
			glm::mat4 result = value;
			for (int column = 0; column < 4; column++)
				for (int row = 0; row < 4; row++)
					if ((column == 0) != (row == 0))
						result[column][row] = -value[column][row];
			return result;
		}

		glm::mat4 PoseMatrix(const JointPose& pose)
		{
			return glm::translate(glm::mat4(1.0f), pose.Translation) * glm::mat4_cast(pose.Rotation) * glm::scale(glm::mat4(1.0f), pose.Scale);
		}

		JointPose NodePose(const tinygltf::Node& node)
		{
			JointPose pose;
			if (node.matrix.size() == 16)
			{
				glm::mat4 matrix(1.0f);
				for (int i = 0; i < 16; i++)
					matrix[i / 4][i % 4] = float(node.matrix[size_t(i)]);
				pose.Translation = glm::vec3(matrix[3]);
				pose.Scale = { glm::length(glm::vec3(matrix[0])), glm::length(glm::vec3(matrix[1])), glm::length(glm::vec3(matrix[2])) };
				const glm::mat3 rotation(glm::vec3(matrix[0]) / pose.Scale.x, glm::vec3(matrix[1]) / pose.Scale.y, glm::vec3(matrix[2]) / pose.Scale.z);
				pose.Rotation = glm::normalize(glm::quat_cast(rotation));
			}
			else
			{
				if (node.translation.size() == 3)
					pose.Translation = { float(node.translation[0]), float(node.translation[1]), float(node.translation[2]) };
				if (node.rotation.size() == 4)
					pose.Rotation = glm::normalize(glm::quat(float(node.rotation[3]), float(node.rotation[0]), float(node.rotation[1]), float(node.rotation[2])));
				if (node.scale.size() == 3)
					pose.Scale = { float(node.scale[0]), float(node.scale[1]), float(node.scale[2]) };
			}
			return { FlipX(pose.Translation), FlipX(pose.Rotation), pose.Scale };
		}

		std::vector<int> ParentsOf(const tinygltf::Model& model)
		{
			std::vector<int> parents(model.nodes.size(), -1);
			for (size_t i = 0; i < model.nodes.size(); i++)
				for (const int child : model.nodes[i].children)
					if (child >= 0 && size_t(child) < parents.size())
						parents[size_t(child)] = int(i);
			return parents;
		}

		glm::mat4 NodeGlobal(const tinygltf::Model& model, const std::vector<int>& parents, int node)
		{
			glm::mat4 global(1.0f);
			for (; node >= 0; node = parents[size_t(node)])
				global = PoseMatrix(NodePose(model.nodes[size_t(node)])) * global;
			return global;
		}

		bool NearlyEqual(const glm::mat4& a, const glm::mat4& b)
		{
			for (int column = 0; column < 4; column++)
				for (int row = 0; row < 4; row++)
					if (std::abs(a[column][row] - b[column][row]) > MatchTolerance)
						return false;
			return true;
		}

		float ReadComponent(const uint8_t* data, int type, bool normalized)
		{
			switch (type)
			{
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
			{
				float value;
				std::memcpy(&value, data, sizeof(value));
				return value;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				return normalized ? float(data[0]) / 255.0f : float(data[0]);
			case TINYGLTF_COMPONENT_TYPE_BYTE:
				return normalized ? std::max(float(int8_t(data[0])) / 127.0f, -1.0f) : float(int8_t(data[0]));
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			{
				uint16_t value;
				std::memcpy(&value, data, sizeof(value));
				return normalized ? float(value) / 65535.0f : float(value);
			}
			case TINYGLTF_COMPONENT_TYPE_SHORT:
			{
				int16_t value;
				std::memcpy(&value, data, sizeof(value));
				return normalized ? std::max(float(value) / 32767.0f, -1.0f) : float(value);
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			{
				uint32_t value;
				std::memcpy(&value, data, sizeof(value));
				return float(value);
			}
			default:
				return 0.0f;
			}
		}

		std::vector<float> ReadFloats(const tinygltf::Model& model, int index, int& components)
		{
			components = 0;
			if (index < 0 || size_t(index) >= model.accessors.size())
				return {};
			const tinygltf::Accessor& accessor = model.accessors[size_t(index)];
			if (accessor.bufferView < 0 || size_t(accessor.bufferView) >= model.bufferViews.size())
				return {};
			const tinygltf::BufferView& view = model.bufferViews[size_t(accessor.bufferView)];
			if (view.buffer < 0 || size_t(view.buffer) >= model.buffers.size())
				return {};
			const tinygltf::Buffer& buffer = model.buffers[size_t(view.buffer)];
			const int count = tinygltf::GetNumComponentsInType(uint32_t(accessor.type));
			const int size = tinygltf::GetComponentSizeInBytes(uint32_t(accessor.componentType));
			const int stride = accessor.ByteStride(view);
			if (count <= 0 || size <= 0 || stride <= 0)
				return {};
			const size_t offset = view.byteOffset + accessor.byteOffset;
			if (accessor.count > 0 && offset + (accessor.count - 1) * size_t(stride) + size_t(count * size) > buffer.data.size())
				return {};
			components = count;
			std::vector<float> values(accessor.count * size_t(count));
			for (size_t i = 0; i < accessor.count; i++)
				for (int c = 0; c < count; c++)
					values[i * size_t(count) + size_t(c)] = ReadComponent(buffer.data.data() + offset + i * size_t(stride) + size_t(c * size), accessor.componentType, accessor.normalized);
			return values;
		}

		std::vector<glm::vec4> ReadVectors(const tinygltf::Model& model, int index)
		{
			int components = 0;
			const std::vector<float> values = ReadFloats(model, index, components);
			std::vector<glm::vec4> vectors(components > 0 ? values.size() / size_t(components) : 0, glm::vec4(0.0f));
			for (size_t i = 0; i < vectors.size(); i++)
				for (int c = 0; c < std::min(components, 4); c++)
					vectors[i][c] = values[i * size_t(components) + size_t(c)];
			return vectors;
		}

		std::vector<uint32_t> ReadIndices(const tinygltf::Model& model, int index, size_t vertexCount)
		{
			std::vector<uint32_t> indices;
			if (index < 0)
			{
				indices.resize(vertexCount);
				for (size_t i = 0; i < vertexCount; i++)
					indices[i] = uint32_t(i);
				return indices;
			}
			const tinygltf::Accessor& accessor = model.accessors[size_t(index)];
			const tinygltf::BufferView& view = model.bufferViews[size_t(accessor.bufferView)];
			const tinygltf::Buffer& buffer = model.buffers[size_t(view.buffer)];
			const int size = tinygltf::GetComponentSizeInBytes(uint32_t(accessor.componentType));
			const int stride = accessor.ByteStride(view);
			const size_t offset = view.byteOffset + accessor.byteOffset;
			if (size <= 0 || stride <= 0 || (accessor.count > 0 && offset + (accessor.count - 1) * size_t(stride) + size_t(size) > buffer.data.size()))
				return {};
			indices.resize(accessor.count);
			for (size_t i = 0; i < accessor.count; i++)
			{
				const uint8_t* data = buffer.data.data() + offset + i * size_t(stride);
				uint32_t value = 0;
				if (size == 1)
					value = data[0];
				else if (size == 2)
				{
					uint16_t shortValue;
					std::memcpy(&shortValue, data, sizeof(shortValue));
					value = shortValue;
				}
				else
					std::memcpy(&value, data, sizeof(value));
				indices[i] = value;
			}
			return indices;
		}

		Track ReadTrack(const tinygltf::Model& model, const tinygltf::AnimationSampler& sampler)
		{
			Track track;
			int components = 0;
			track.Times = ReadFloats(model, sampler.input, components);
			track.Values = ReadFloats(model, sampler.output, track.Components);
			track.Step = sampler.interpolation == "STEP";
			track.Cubic = sampler.interpolation == "CUBICSPLINE";
			return track;
		}

		glm::vec4 SampleTrack(const Track& track, float time)
		{
			const size_t keys = track.Times.size();
			const size_t stride = size_t(track.Components);
			const auto value = [&](size_t key)
			{
				const size_t base = (track.Cubic ? key * 3 + 1 : key) * stride;
				glm::vec4 result(0.0f);
				for (size_t c = 0; c < std::min<size_t>(stride, 4); c++)
					if (base + c < track.Values.size())
						result[int(c)] = track.Values[base + c];
				return result;
			};
			if (keys == 0 || stride == 0)
				return glm::vec4(0.0f);
			if (time <= track.Times.front())
				return value(0);
			if (time >= track.Times.back())
				return value(keys - 1);
			const size_t next = size_t(std::upper_bound(track.Times.begin(), track.Times.end(), time) - track.Times.begin());
			const size_t previous = next - 1;
			if (track.Step)
				return value(previous);
			const float span = track.Times[next] - track.Times[previous];
			const float t = span > 0.0f ? (time - track.Times[previous]) / span : 0.0f;
			const glm::vec4 a = value(previous);
			glm::vec4 b = value(next);
			if (stride == 4)
			{
				if (glm::dot(a, b) < 0.0f)
					b = -b;
				return glm::normalize(glm::mix(a, b, t));
			}
			return glm::mix(a, b, t);
		}

		uint32_t PackWeights(glm::vec4 weights)
		{
			weights = glm::max(weights, glm::vec4(0.0f));
			const float sum = weights.x + weights.y + weights.z + weights.w;
			weights = sum > 1e-6f ? weights / sum : glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
			int quantized[4];
			int total = 0;
			int largest = 0;
			for (int i = 0; i < 4; i++)
			{
				quantized[i] = int(std::lround(weights[i] * 255.0f));
				total += quantized[i];
				if (weights[i] > weights[largest])
					largest = i;
			}
			quantized[largest] += 255 - total;
			uint32_t packed = 0;
			for (int i = 0; i < 4; i++)
				packed |= uint32_t(std::clamp(quantized[i], 0, 255)) << (8 * i);
			return packed;
		}

		bool AppendPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, const glm::mat4& transform, const std::vector<uint8_t>* remap, int32_t rigidJoint, Geometry& geometry, std::string& error)
		{
			const auto attribute = [&](const char* name)
			{
				const auto it = primitive.attributes.find(name);
				return it != primitive.attributes.end() ? it->second : -1;
			};
			const std::vector<glm::vec4> positions = ReadVectors(model, attribute("POSITION"));
			if (positions.empty())
				return true;
			const std::vector<glm::vec4> normals = ReadVectors(model, attribute("NORMAL"));
			const std::vector<glm::vec4> uvs = ReadVectors(model, attribute("TEXCOORD_0"));
			int colorComponents = 0;
			const std::vector<float> colors = ReadFloats(model, attribute("COLOR_0"), colorComponents);
			const std::vector<glm::vec4> joints = remap ? ReadVectors(model, attribute("JOINTS_0")) : std::vector<glm::vec4>();
			const std::vector<glm::vec4> weights = remap ? ReadVectors(model, attribute("WEIGHTS_0")) : std::vector<glm::vec4>();
			if (remap && (joints.size() != positions.size() || weights.size() != positions.size()))
			{
				error = "a skinned primitive is missing JOINTS_0 or WEIGHTS_0";
				return false;
			}
			const std::vector<uint32_t> indices = ReadIndices(model, primitive.indices, positions.size());
			if (indices.size() % 3 != 0)
			{
				error = "a primitive's index count isn't a multiple of 3";
				return false;
			}

			const uint32_t base = uint32_t(geometry.Vertices.size());
			const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
			for (size_t i = 0; i < positions.size(); i++)
			{
				ModelVertex vertex;
				vertex.Position = glm::vec3(transform * glm::vec4(FlipX(glm::vec3(positions[i])), 1.0f));
				const glm::vec3 normal = i < normals.size() ? FlipX(glm::vec3(normals[i])) : glm::vec3(0.0f, 1.0f, 0.0f);
				vertex.Normal = glm::length(normal) > 1e-6f ? glm::normalize(normalMatrix * normal) : glm::vec3(0.0f, 1.0f, 0.0f);
				if (i < uvs.size())
				{
					vertex.U = uvs[i].x;
					vertex.V = uvs[i].y;
				}
				if (colorComponents >= 3 && (i + 1) * size_t(colorComponents) <= colors.size())
				{
					vertex.Color = 0;
					for (int c = 0; c < 4; c++)
					{
						const float value = c < colorComponents ? colors[i * size_t(colorComponents) + size_t(c)] : 1.0f;
						vertex.Color |= uint32_t(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)) << (8 * c);
					}
				}
				if (remap)
				{
					for (int c = 0; c < 4; c++)
					{
						const int source = int(joints[i][c]);
						if (source < 0 || size_t(source) >= remap->size())
						{
							error = "a vertex uses a joint outside its skin";
							return false;
						}
						vertex.Joints |= uint32_t((*remap)[size_t(source)]) << (8 * c);
					}
					vertex.Weights = PackWeights(weights[i]);
				}
				else if (rigidJoint >= 0)
				{
					vertex.Joints = uint32_t(rigidJoint) * 0x01010101u;
					vertex.Weights = 255;
				}
				else
					vertex.Weights = 255;
				geometry.Vertices.push_back(vertex);
			}
			for (size_t i = 0; i + 2 < indices.size(); i += 3)
			{
				if (indices[i] >= positions.size() || indices[i + 1] >= positions.size() || indices[i + 2] >= positions.size())
				{
					error = "a primitive's index is out of range";
					return false;
				}
				geometry.Indices.push_back(base + indices[i]);
				geometry.Indices.push_back(base + indices[i + 2]);
				geometry.Indices.push_back(base + indices[i + 1]);
			}
			return true;
		}

		void Downscale(ModelTexture& texture)
		{
			while ((texture.Width > ModelTextureMaxSize || texture.Height > ModelTextureMaxSize) && texture.Width % 2 == 0 && texture.Height % 2 == 0)
			{
				const uint32_t width = texture.Width / 2;
				const uint32_t height = texture.Height / 2;
				std::vector<uint8_t> pixels(size_t(width) * height * 4);
				for (uint32_t y = 0; y < height; y++)
					for (uint32_t x = 0; x < width; x++)
						for (uint32_t c = 0; c < 4; c++)
						{
							const auto at = [&](uint32_t sx, uint32_t sy) { return uint32_t(texture.Pixels[(size_t(sy) * texture.Width + sx) * 4 + c]); };
							pixels[(size_t(y) * width + x) * 4 + c] = uint8_t((at(2 * x, 2 * y) + at(2 * x + 1, 2 * y) + at(2 * x, 2 * y + 1) + at(2 * x + 1, 2 * y + 1) + 2) / 4);
						}
				texture.Pixels = std::move(pixels);
				texture.Width = width;
				texture.Height = height;
			}
		}

		const SwatchMaterial& FindSwatchMaterial(char key)
		{
			for (const SwatchMaterial& material : SwatchMaterials)
				if (material.Key == key)
					return material;
			return SwatchMaterials[0];
		}
	}

	bool GltfImporter::LoadModel(const std::filesystem::path& path, tinygltf::Model& model, std::string& error)
	{
		tinygltf::TinyGLTF loader;
		std::string warning;
		const std::string file = path.string();
		const bool loaded = path.extension() == ".glb" ? loader.LoadBinaryFromFile(&model, &error, &warning, file) : loader.LoadASCIIFromFile(&model, &error, &warning, file);
		if (!warning.empty())
			m_Warnings.push_back(file + ": " + warning);
		if (!loaded)
			error = file + ": " + (error.empty() ? std::string("can't be read") : error);
		return loaded;
	}

	bool GltfImporter::Import(const std::filesystem::path& path, const ModelSource& source, std::string& error)
	{
		const ModelSourceKind kind = source.Kind;
		if (kind == ModelSourceKind::PropFolder)
		{
			std::vector<std::filesystem::path> files;
			std::error_code listError;
			for (const auto& entry : std::filesystem::directory_iterator(path, listError))
				if (entry.is_regular_file() && (entry.path().extension() == ".gltf" || entry.path().extension() == ".glb"))
					files.push_back(entry.path());
			if (listError)
			{
				error = path.string() + ": " + listError.message();
				return false;
			}
			std::sort(files.begin(), files.end());
			const ModelSource prop{ source.Path, ModelSourceKind::Prop };
			for (const std::filesystem::path& file : files)
				if (!Import(file, prop, error))
					return false;
			return true;
		}
		if (kind == ModelSourceKind::Scan)
			return ImportScan(path, source, error);
		if (kind == ModelSourceKind::Material)
			return ImportMaterial(path, source, error);
		if (kind == ModelSourceKind::Environment)
			return ImportEnvironment(path, source, error);
		tinygltf::Model model;
		if (!LoadModel(path, model, error))
			return false;
		const std::string file = path.string();
		const std::string stem = path.stem().string();
		bool imported = false;
		switch (kind)
		{
		case ModelSourceKind::Character:
			imported = ImportCharacter(model, stem, error);
			break;
		case ModelSourceKind::Animations:
			imported = ImportAnimations(model, stem, error);
			break;
		default:
			imported = ImportProp(model, stem, error);
			break;
		}
		if (!imported)
			error = file + ": " + error;
		return imported;
	}

	int32_t GltfImporter::FindJoint(const std::string& name) const
	{
		const std::vector<std::string>& names = m_Output.Skeleton.Names;
		const auto it = std::find(names.begin(), names.end(), name);
		return it != names.end() ? int32_t(it - names.begin()) : -1;
	}

	bool GltfImporter::SetupSkeleton(const tinygltf::Model& model, const tinygltf::Skin& skin, std::string& error)
	{
		if (skin.joints.empty() || skin.joints.size() > MaxModelJoints)
		{
			error = "the skin has " + std::to_string(skin.joints.size()) + " joints, at most " + std::to_string(MaxModelJoints) + " are supported";
			return false;
		}
		const std::vector<int> parents = ParentsOf(model);
		const auto isJoint = [&](int node) { return std::find(skin.joints.begin(), skin.joints.end(), node) != skin.joints.end(); };
		const auto depth = [&](int node)
		{
			int levels = 0;
			for (int parent = parents[size_t(node)]; parent >= 0; parent = parents[size_t(parent)])
				levels++;
			return levels;
		};
		std::vector<int> ordered = skin.joints;
		std::stable_sort(ordered.begin(), ordered.end(), [&](int a, int b) { return depth(a) < depth(b); });
		int components = 0;
		const std::vector<float> inverseBind = ReadFloats(model, skin.inverseBindMatrices, components);

		ModelSkeleton& skeleton = m_Output.Skeleton;
		for (const int node : ordered)
		{
			const std::string& name = model.nodes[size_t(node)].name;
			if (FindJoint(name) >= 0)
			{
				error = "the skin has two joints named " + name;
				return false;
			}
			int parent = parents[size_t(node)];
			while (parent >= 0 && !isJoint(parent))
				parent = parents[size_t(parent)];
			if (parent < 0 && parents[size_t(node)] >= 0 && !NearlyEqual(NodeGlobal(model, parents, parents[size_t(node)]), glm::mat4(1.0f)))
				m_Warnings.push_back("the armature above " + name + " isn't at the origin, its transform is ignored");
			const auto found = std::find(ordered.begin(), ordered.end(), parent);
			skeleton.Names.push_back(name);
			skeleton.Parents.push_back(parent >= 0 ? int32_t(found - ordered.begin()) : -1);
			skeleton.Rest.push_back(NodePose(model.nodes[size_t(node)]));
			const size_t source = size_t(std::find(skin.joints.begin(), skin.joints.end(), node) - skin.joints.begin());
			glm::mat4 matrix(1.0f);
			if (components == 16 && inverseBind.size() >= (source + 1) * 16)
				for (int i = 0; i < 16; i++)
					matrix[i / 4][i % 4] = inverseBind[source * 16 + size_t(i)];
			skeleton.InverseBind.push_back(FlipX(matrix));
		}
		return true;
	}

	bool GltfImporter::CheckSkin(const tinygltf::Model& model, const tinygltf::Skin& skin, const std::string& file, std::string& error)
	{
		int components = 0;
		const std::vector<float> inverseBind = ReadFloats(model, skin.inverseBindMatrices, components);
		for (size_t i = 0; i < skin.joints.size(); i++)
		{
			const std::string& name = model.nodes[size_t(skin.joints[i])].name;
			const int32_t joint = FindJoint(name);
			if (joint < 0)
			{
				error = "joint " + name + " isn't in the shared skeleton";
				return false;
			}
			glm::mat4 matrix(1.0f);
			if (components == 16 && inverseBind.size() >= (i + 1) * 16)
				for (int c = 0; c < 16; c++)
					matrix[c / 4][c % 4] = inverseBind[i * 16 + size_t(c)];
			if (!NearlyEqual(FlipX(matrix), m_Output.Skeleton.InverseBind[size_t(joint)]))
			{
				m_Warnings.push_back(file + ": joint " + name + " is bound differently than the shared skeleton, the mesh may look distorted");
				return true;
			}
		}
		return true;
	}

	std::string GltfImporter::UniqueMeshName(const std::string& name, const std::string& file)
	{
		std::string unique = name.empty() ? file : name;
		if (m_MeshNames.count(unique))
			unique += "@" + file;
		for (uint32_t i = 2; m_MeshNames.count(unique); i++)
			unique = name + "@" + file + std::to_string(i);
		m_MeshNames.insert(unique);
		return unique;
	}

	bool CheckSwatchTables(std::string& error)
	{
		const auto known = [](char key) { return std::any_of(std::begin(SwatchMaterials), std::end(SwatchMaterials), [key](const SwatchMaterial& material) { return material.Key == key; }); };
		for (const SwatchTable& table : SwatchTables)
			for (const char* row : table.Rows)
			{
				const std::string_view text = row ? row : "";
				if (text.size() != SwatchGrid || !std::all_of(text.begin(), text.end(), known))
				{
					error = std::string("the swatch table for ") + table.Texture + " has a row that isn't " + std::to_string(SwatchGrid) + " known material keys: \"" + std::string(text) + "\"";
					return false;
				}
			}
		for (const SwatchOverride& entry : SwatchOverrides)
			if (!known(entry.Key))
			{
				error = std::string("the swatch override for ") + entry.Mesh + " uses an unknown material key";
				return false;
			}
		return true;
	}

	uint32_t GltfImporter::StoreTexture(ModelTexture texture)
	{
		if (texture.Pixels.empty())
			return NoTexture;
		uint64_t hash = Hash(HashSeed, &texture.Width, sizeof(texture.Width));
		hash = Hash(hash, &texture.Height, sizeof(texture.Height));
		hash = Hash(hash, &texture.MipCount, sizeof(texture.MipCount));
		hash = Hash(hash, &texture.Format, sizeof(texture.Format));
		hash = Hash(hash, &texture.Nearest, sizeof(texture.Nearest));
		hash = Hash(hash, &texture.Repeat, sizeof(texture.Repeat));
		hash = Hash(hash, texture.Pixels.data(), texture.Pixels.size());
		if (const auto it = m_TextureByHash.find(hash); it != m_TextureByHash.end())
			return it->second;
		const uint32_t index = uint32_t(m_Output.Textures.size());
		m_Output.Textures.push_back(std::move(texture));
		m_TextureByHash.emplace(hash, index);
		return index;
	}

	uint32_t GltfImporter::ImportTexture(const tinygltf::Model& model, int textureIndex, bool surface)
	{
		if (textureIndex < 0 || size_t(textureIndex) >= model.textures.size())
			return NoTexture;
		const tinygltf::Texture& texture = model.textures[size_t(textureIndex)];
		if (texture.source < 0 || size_t(texture.source) >= model.images.size())
			return NoTexture;
		const tinygltf::Image& image = model.images[size_t(texture.source)];
		if (image.width <= 0 || image.height <= 0 || image.bits != 8 || (image.component != 3 && image.component != 4) || image.image.size() < size_t(image.width) * size_t(image.height) * size_t(image.component))
		{
			m_Warnings.push_back("image " + image.name + image.uri + " isn't 8-bit RGB(A), it's skipped");
			return NoTexture;
		}
		ModelTexture result;
		result.Name = !image.name.empty() ? image.name : std::filesystem::path(image.uri).stem().string();
		result.Width = uint32_t(image.width);
		result.Height = uint32_t(image.height);
		result.Format = surface ? TextureFormat::Rgba8 : TextureFormat::Rgba8Srgb;
		result.Pixels.resize(size_t(result.Width) * result.Height * 4);
		for (size_t i = 0; i < size_t(result.Width) * result.Height; i++)
			for (size_t c = 0; c < 4; c++)
				result.Pixels[i * 4 + c] = c < size_t(image.component) ? image.image[i * size_t(image.component) + c] : uint8_t(255);
		Downscale(result);
		return StoreTexture(std::move(result));
	}

	uint32_t GltfImporter::SwatchSurface(uint32_t texture)
	{
		const std::string& name = m_Output.Textures[texture].Name;
		const auto table = std::find_if(std::begin(SwatchTables), std::end(SwatchTables), [&](const SwatchTable& entry) { return name == entry.Texture; });
		if (table == std::end(SwatchTables))
			return NoTexture;
		ModelTexture surface;
		surface.Name = name + "_surface";
		surface.Width = SwatchGrid;
		surface.Height = SwatchGrid;
		surface.Format = TextureFormat::Rgba8;
		surface.Nearest = true;
		surface.Pixels.resize(size_t(SwatchCells) * 4);
		for (uint32_t row = 0; row < SwatchGrid; row++)
			for (uint32_t column = 0; column < SwatchGrid; column++)
			{
				const SwatchMaterial& material = FindSwatchMaterial(table->Rows[row][column]);
				uint8_t* pixel = &surface.Pixels[(size_t(row) * SwatchGrid + column) * 4];
				pixel[0] = 255;
				pixel[1] = uint8_t(std::lround(material.Roughness * 255.0f));
				pixel[2] = uint8_t(std::lround(material.Metallic * 255.0f));
				pixel[3] = 255;
			}
		return StoreTexture(std::move(surface));
	}

	void GltfImporter::ApplyMaterial(const tinygltf::Model& model, int materialIndex, ModelMesh& mesh)
	{
		if (materialIndex >= 0 && size_t(materialIndex) < model.materials.size())
		{
			const tinygltf::Material& material = model.materials[size_t(materialIndex)];
			const tinygltf::PbrMetallicRoughness& pbr = material.pbrMetallicRoughness;
			if (pbr.baseColorFactor.size() == 4)
				mesh.Color = { float(pbr.baseColorFactor[0]), float(pbr.baseColorFactor[1]), float(pbr.baseColorFactor[2]), float(pbr.baseColorFactor[3]) };
			if (material.emissiveFactor.size() == 3)
				mesh.Emissive = { float(material.emissiveFactor[0]), float(material.emissiveFactor[1]), float(material.emissiveFactor[2]) };
			mesh.Roughness = float(pbr.roughnessFactor);
			mesh.Metallic = float(pbr.metallicFactor);
			mesh.Texture = ImportTexture(model, pbr.baseColorTexture.index, false);
			mesh.Surface = ImportTexture(model, pbr.metallicRoughnessTexture.index, true);
			if (mesh.Surface == NoTexture && mesh.Texture != NoTexture)
				if ((mesh.Surface = SwatchSurface(mesh.Texture)) != NoTexture)
				{
					mesh.Roughness = 1.0f;
					mesh.Metallic = 1.0f;
				}
		}
		for (const SwatchOverride& entry : SwatchOverrides)
			if (mesh.Name == entry.Mesh)
			{
				const SwatchMaterial& material = FindSwatchMaterial(entry.Key);
				mesh.Surface = NoTexture;
				mesh.Roughness = material.Roughness;
				mesh.Metallic = material.Metallic;
			}
	}

	bool GltfImporter::ImportCharacter(const tinygltf::Model& model, const std::string& file, std::string& error)
	{
		bool any = false;
		if (m_Output.Skeleton.Names.empty())
		{
			if (model.skins.empty())
			{
				error = "there's no skin to take the skeleton from";
				return false;
			}
			if (!SetupSkeleton(model, model.skins[0], error))
				return false;
		}
		const std::vector<int> parents = ParentsOf(model);
		for (size_t n = 0; n < model.nodes.size(); n++)
		{
			const tinygltf::Node& node = model.nodes[n];
			if (node.mesh < 0 || size_t(node.mesh) >= model.meshes.size())
				continue;
			std::vector<uint8_t> remap;
			int32_t rigidJoint = -1;
			glm::mat4 transform(1.0f);
			if (node.skin >= 0 && size_t(node.skin) < model.skins.size())
			{
				const tinygltf::Skin& skin = model.skins[size_t(node.skin)];
				if (!CheckSkin(model, skin, file, error))
					return false;
				remap.resize(skin.joints.size());
				for (size_t i = 0; i < skin.joints.size(); i++)
					remap[i] = uint8_t(FindJoint(model.nodes[size_t(skin.joints[i])].name));
			}
			else
			{
				for (int parent = parents[n]; parent >= 0 && rigidJoint < 0; parent = parents[size_t(parent)])
					rigidJoint = FindJoint(model.nodes[size_t(parent)].name);
				rigidJoint = std::max(rigidJoint, 0);
				transform = NodeGlobal(model, parents, int(n));
			}

			const tinygltf::Mesh& mesh = model.meshes[size_t(node.mesh)];
			for (size_t p = 0; p < mesh.primitives.size(); p++)
			{
				const tinygltf::Primitive& primitive = mesh.primitives[p];
				if (primitive.mode != TINYGLTF_MODE_TRIANGLES && primitive.mode != -1)
					continue;
				Geometry geometry;
				if (!AppendPrimitive(model, primitive, transform, remap.empty() ? nullptr : &remap, rigidJoint, geometry, error))
					return false;
				if (geometry.Indices.empty())
					continue;
				ModelMesh part;
				part.Name = UniqueMeshName(mesh.primitives.size() > 1 ? mesh.name + "." + std::to_string(p) : mesh.name, file);
				part.Skinned = true;
				ApplyMaterial(model, primitive.material, part);
				part.FirstIndex = uint32_t(m_Output.Indices.size());
				part.IndexCount = uint32_t(geometry.Indices.size());
				const uint32_t base = uint32_t(m_Output.Vertices.size());
				part.Min = glm::vec3(1e9f);
				part.Max = glm::vec3(-1e9f);
				for (const ModelVertex& vertex : geometry.Vertices)
				{
					part.Min = glm::min(part.Min, vertex.Position);
					part.Max = glm::max(part.Max, vertex.Position);
				}
				m_Output.Vertices.insert(m_Output.Vertices.end(), geometry.Vertices.begin(), geometry.Vertices.end());
				for (const uint32_t index : geometry.Indices)
					m_Output.Indices.push_back(base + index);
				m_Output.Meshes.push_back(std::move(part));
				any = true;
			}
		}
		if (!any)
			error = "no skinned meshes found";
		return any;
	}

	bool GltfImporter::ImportProp(const tinygltf::Model& model, const std::string& file, std::string& error)
	{
		const std::vector<int> parents = ParentsOf(model);
		std::map<int, Geometry> byMaterial;
		for (size_t n = 0; n < model.nodes.size(); n++)
		{
			const tinygltf::Node& node = model.nodes[n];
			if (node.mesh < 0 || size_t(node.mesh) >= model.meshes.size())
				continue;
			const glm::mat4 global = NodeGlobal(model, parents, int(n));
			for (const tinygltf::Primitive& primitive : model.meshes[size_t(node.mesh)].primitives)
			{
				if (primitive.mode != TINYGLTF_MODE_TRIANGLES && primitive.mode != -1)
					continue;
				if (!AppendPrimitive(model, primitive, global, nullptr, -1, byMaterial[primitive.material], error))
					return false;
			}
		}
		uint32_t part = 0;
		for (const auto& [material, geometry] : byMaterial)
		{
			if (geometry.Indices.empty())
				continue;
			ModelMesh mesh;
			mesh.Name = UniqueMeshName(part == 0 ? file : file + "." + std::to_string(part), file);
			ApplyMaterial(model, material, mesh);
			mesh.FirstIndex = uint32_t(m_Output.Indices.size());
			mesh.IndexCount = uint32_t(geometry.Indices.size());
			mesh.Min = glm::vec3(1e9f);
			mesh.Max = glm::vec3(-1e9f);
			const uint32_t base = uint32_t(m_Output.Vertices.size());
			for (const ModelVertex& vertex : geometry.Vertices)
			{
				mesh.Min = glm::min(mesh.Min, vertex.Position);
				mesh.Max = glm::max(mesh.Max, vertex.Position);
			}
			m_Output.Vertices.insert(m_Output.Vertices.end(), geometry.Vertices.begin(), geometry.Vertices.end());
			for (const uint32_t index : geometry.Indices)
				m_Output.Indices.push_back(base + index);
			m_Output.Meshes.push_back(std::move(mesh));
			part++;
		}
		if (part == 0)
			error = "no meshes found";
		return part > 0;
	}

	bool GltfImporter::ImportAnimations(const tinygltf::Model& model, const std::string& file, std::string& error)
	{
		if (m_Output.Skeleton.Names.empty())
		{
			if (model.skins.empty())
			{
				error = "there's no skin to take the skeleton from";
				return false;
			}
			if (!SetupSkeleton(model, model.skins[0], error))
				return false;
		}
		const ModelSkeleton& skeleton = m_Output.Skeleton;
		const uint32_t joints = skeleton.JointCount();
		std::vector<int32_t> nodeJoint(model.nodes.size(), -1);
		for (size_t n = 0; n < model.nodes.size(); n++)
			nodeJoint[n] = FindJoint(model.nodes[n].name);

		for (const tinygltf::Animation& animation : model.animations)
		{
			if (!m_ClipNames.insert(animation.name).second)
				continue;
			struct Channel
			{
				int32_t Joint = -1;
				int Path = 0;
				Track Samples;
			};
			std::vector<Channel> channels;
			float duration = 0.0f;
			for (const tinygltf::AnimationChannel& channel : animation.channels)
			{
				if (channel.target_node < 0 || size_t(channel.target_node) >= nodeJoint.size() || nodeJoint[size_t(channel.target_node)] < 0 || channel.sampler < 0 || size_t(channel.sampler) >= animation.samplers.size())
					continue;
				const int path = channel.target_path == "translation" ? 0 : channel.target_path == "rotation" ? 1 : channel.target_path == "scale" ? 2 : -1;
				if (path < 0)
					continue;
				Channel entry{ nodeJoint[size_t(channel.target_node)], path, ReadTrack(model, animation.samplers[size_t(channel.sampler)]) };
				if (entry.Samples.Times.empty() || entry.Samples.Components != (path == 1 ? 4 : 3))
					continue;
				duration = std::max(duration, entry.Samples.Times.back());
				channels.push_back(std::move(entry));
			}
			ModelClip clip;
			clip.Name = animation.name;
			clip.FrameRate = ModelClipFrameRate;
			clip.FrameCount = uint32_t(std::lround(duration * clip.FrameRate)) + 1;
			clip.Frames.resize(size_t(clip.FrameCount) * joints);
			for (uint32_t frame = 0; frame < clip.FrameCount; frame++)
				std::copy(skeleton.Rest.begin(), skeleton.Rest.end(), clip.Frames.begin() + ptrdiff_t(frame * joints));
			for (const Channel& channel : channels)
				for (uint32_t frame = 0; frame < clip.FrameCount; frame++)
				{
					const glm::vec4 value = SampleTrack(channel.Samples, std::min(float(frame) / clip.FrameRate, duration));
					JointPose& pose = clip.Frames[size_t(frame) * joints + size_t(channel.Joint)];
					if (channel.Path == 0)
						pose.Translation = FlipX(glm::vec3(value));
					else if (channel.Path == 1)
						pose.Rotation = glm::normalize(FlipX(glm::quat(value.w, value.x, value.y, value.z)));
					else
						pose.Scale = glm::vec3(value);
				}
			m_Output.Clips.push_back(std::move(clip));
		}
		if (model.animations.empty())
			m_Warnings.push_back(file + " has no animations");
		return true;
	}

	void GltfImporter::AddMesh(ModelMesh mesh, std::vector<ModelVertex>& vertices, const std::vector<uint32_t>& indices)
	{
		mesh.FirstIndex = uint32_t(m_Output.Indices.size());
		mesh.IndexCount = uint32_t(indices.size());
		mesh.Min = glm::vec3(1e9f);
		mesh.Max = glm::vec3(-1e9f);
		for (const ModelVertex& vertex : vertices)
		{
			mesh.Min = glm::min(mesh.Min, vertex.Position);
			mesh.Max = glm::max(mesh.Max, vertex.Position);
		}
		const uint32_t base = uint32_t(m_Output.Vertices.size());
		m_Output.Vertices.insert(m_Output.Vertices.end(), vertices.begin(), vertices.end());
		for (const uint32_t index : indices)
			m_Output.Indices.push_back(base + index);
		m_Output.Meshes.push_back(std::move(mesh));
	}

	namespace
	{
		std::string Lowercase(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
			return text;
		}

		std::optional<std::filesystem::path> FindFile(const std::filesystem::path& folder, std::string_view tag)
		{
			std::vector<std::filesystem::path> matches;
			std::error_code error;
			for (const auto& entry : std::filesystem::recursive_directory_iterator(folder, error))
				if (entry.is_regular_file() && Lowercase(entry.path().filename().string()).find(tag) != std::string::npos)
					matches.push_back(entry.path());
			std::sort(matches.begin(), matches.end());
			return matches.empty() ? std::nullopt : std::optional<std::filesystem::path>(matches.front());
		}

		std::string StripResolution(std::string name)
		{
			for (const char* suffix : { "_1k", "_2k", "_4k", "_8k" })
				if (name.size() > 3 && name.ends_with(suffix))
					return name.substr(0, name.size() - 3);
			return name;
		}
	}

	GltfImporter::ScanMaterial GltfImporter::BakeScanMaterial(const tinygltf::Model& model, int materialIndex, const std::filesystem::path& folder, const ModelSource& source)
	{
		ScanMaterial result;
		if (materialIndex < 0 || size_t(materialIndex) >= model.materials.size())
			return result;
		const tinygltf::Material& material = model.materials[size_t(materialIndex)];
		const std::string name = Lowercase(material.name);
		if (material.alphaMode == "BLEND" && (name.find("glass") != std::string::npos || name.find("flame") != std::string::npos))
		{
			result.Skip = true;
			return result;
		}
		const tinygltf::PbrMetallicRoughness& pbr = material.pbrMetallicRoughness;
		if (pbr.baseColorFactor.size() == 4)
			result.Color = { float(pbr.baseColorFactor[0]), float(pbr.baseColorFactor[1]), float(pbr.baseColorFactor[2]), float(pbr.baseColorFactor[3]) };
		result.Roughness = float(pbr.roughnessFactor);
		result.Metallic = float(pbr.metallicFactor);
		if (material.alphaMode == "MASK")
			result.AlphaCutoff = float(material.alphaCutoff);
		else if (material.alphaMode == "BLEND")
			result.AlphaCutoff = 0.5f;
		const auto image = [&](int textureIndex) -> std::optional<RgbaImage>
		{
			if (textureIndex < 0 || size_t(textureIndex) >= model.textures.size())
				return std::nullopt;
			const tinygltf::Texture& texture = model.textures[size_t(textureIndex)];
			if (texture.source < 0 || size_t(texture.source) >= model.images.size())
				return std::nullopt;
			const tinygltf::Image& data = model.images[size_t(texture.source)];
			if (data.width <= 0 || data.height <= 0 || data.bits != 8 || data.component < 1 || data.image.size() < size_t(data.width) * size_t(data.height) * size_t(data.component))
			{
				m_Warnings.push_back(folder.string() + ": image " + data.name + data.uri + " isn't 8-bit, it's skipped");
				return std::nullopt;
			}
			return MakeRgbaImage(data.image.data(), data.width, data.height, data.component);
		};
		const std::optional<RgbaImage> albedo = image(pbr.baseColorTexture.index);
		const std::optional<RgbaImage> normal = image(material.normalTexture.index);
		std::optional<RgbaImage> surface = image(pbr.metallicRoughnessTexture.index);
		const int occlusionIndex = material.occlusionTexture.index;
		if (surface && occlusionIndex != pbr.metallicRoughnessTexture.index)
		{
			const std::optional<RgbaImage> occlusion = image(occlusionIndex);
			const bool matches = occlusion && occlusion->Width == surface->Width && occlusion->Height == surface->Height;
			for (size_t i = 0; i < size_t(surface->Width) * surface->Height; i++)
				surface->Pixels[i * 4] = matches ? occlusion->Pixels[i * 4] : uint8_t(255);
		}
		std::optional<RgbaImage> alpha;
		if (result.AlphaCutoff > 0.0f)
		{
			if (const std::optional<std::filesystem::path> file = FindFile(folder, "_alpha"))
			{
				RgbaImage mask;
				if (LoadRgbaImage(*file, mask))
					alpha = std::move(mask);
			}
			if (!alpha && (!albedo || std::all_of(albedo->Pixels.begin() + 3, albedo->Pixels.end(), [i = size_t(0)](uint8_t value) mutable { return (i++ % 4) != 0 || value == 255; })))
				m_Warnings.push_back(folder.string() + ": " + material.name + " is alpha tested but has no alpha mask");
		}
		const PbrBakeInput input{
			.Name = folder.filename().string() + "_" + material.name,
			.Albedo = albedo ? &*albedo : nullptr,
			.Alpha = alpha ? &*alpha : nullptr,
			.Normal = normal ? &*normal : nullptr,
			.Surface = surface ? &*surface : nullptr,
			.MaxSize = source.TextureSize ? source.TextureSize : 512,
			.AlphaCutoff = result.AlphaCutoff,
			.Repeat = true,
		};
		PbrBakeOutput output = BakePbrTextures(input);
		if (albedo || alpha)
			result.Texture = StoreTexture(std::move(output.Albedo));
		result.Normal = StoreTexture(std::move(output.Normal));
		result.Surface = StoreTexture(std::move(output.Surface));
		return result;
	}

	bool GltfImporter::ImportScan(const std::filesystem::path& folder, const ModelSource& source, std::string& error)
	{
		const std::optional<std::filesystem::path> file = FindFile(folder, ".gltf");
		if (!file)
		{
			error = folder.string() + " has no glTF file";
			return false;
		}
		tinygltf::Model model;
		if (!LoadModel(*file, model, error))
			return false;
		const std::string name = folder.filename().string();
		const std::vector<int> parents = ParentsOf(model);
		std::map<int, ScanMaterial> materials;
		const auto material = [&](int index) -> const ScanMaterial&
		{
			auto it = materials.find(index);
			if (it == materials.end())
				it = materials.emplace(index, BakeScanMaterial(model, index, folder, source)).first;
			return it->second;
		};
		struct Group
		{
			std::string Name;
			std::map<int, Geometry> ByMaterial;
		};
		std::vector<Group> groups;
		if (!source.SplitNodes)
			groups.push_back({ name, {} });
		for (size_t n = 0; n < model.nodes.size(); n++)
		{
			const tinygltf::Node& node = model.nodes[n];
			if (node.mesh < 0 || size_t(node.mesh) >= model.meshes.size())
				continue;
			if (source.SplitNodes)
				groups.push_back({ name + "/" + (node.name.empty() ? std::to_string(n) : node.name), {} });
			Group& group = groups.back();
			const glm::mat4 global = NodeGlobal(model, parents, int(n));
			for (const tinygltf::Primitive& primitive : model.meshes[size_t(node.mesh)].primitives)
			{
				if ((primitive.mode != TINYGLTF_MODE_TRIANGLES && primitive.mode != -1) || material(primitive.material).Skip)
					continue;
				if (!AppendPrimitive(model, primitive, global, nullptr, -1, group.ByMaterial[primitive.material], error))
				{
					error = file->string() + ": " + error;
					return false;
				}
			}
		}
		uint32_t count = 0;
		for (Group& group : groups)
		{
			glm::vec3 low(1e9f);
			glm::vec3 high(-1e9f);
			for (const auto& [index, geometry] : group.ByMaterial)
				for (const ModelVertex& vertex : geometry.Vertices)
				{
					low = glm::min(low, vertex.Position);
					high = glm::max(high, vertex.Position);
				}
			const glm::vec3 pivot = source.SplitNodes ? glm::vec3(0.5f * (low.x + high.x), low.y, 0.5f * (low.z + high.z)) : glm::vec3(0.0f);
			uint32_t part = 0;
			for (auto& [index, geometry] : group.ByMaterial)
			{
				if (geometry.Indices.empty())
					continue;
				for (ModelVertex& vertex : geometry.Vertices)
					vertex.Position -= pivot;
				const ScanMaterial& scan = material(index);
				if (scan.Normal != NoTexture)
					GenerateTangents(geometry.Vertices, geometry.Indices);
				ModelMesh mesh;
				mesh.Name = UniqueMeshName(part == 0 ? group.Name : group.Name + "." + std::to_string(part), name);
				mesh.Texture = scan.Texture;
				mesh.Surface = scan.Surface;
				mesh.Normal = scan.Normal;
				mesh.Color = scan.Color;
				mesh.Roughness = scan.Roughness;
				mesh.Metallic = scan.Metallic;
				mesh.AlphaCutoff = scan.AlphaCutoff;
				AddMesh(std::move(mesh), geometry.Vertices, geometry.Indices);
				part++;
				count++;
			}
		}
		if (count == 0)
		{
			error = file->string() + ": no meshes found";
			return false;
		}
		return true;
	}

	bool GltfImporter::ImportMaterial(const std::filesystem::path& folder, const ModelSource& source, std::string& error)
	{
		const std::optional<std::filesystem::path> albedoFile = FindFile(folder, "_diff");
		if (!albedoFile)
		{
			error = folder.string() + " has no _diff texture";
			return false;
		}
		const auto load = [&](std::string_view tag) -> std::optional<RgbaImage>
		{
			const std::optional<std::filesystem::path> file = FindFile(folder, tag);
			RgbaImage image;
			if (!file || !LoadRgbaImage(*file, image))
				return std::nullopt;
			return image;
		};
		const std::optional<RgbaImage> albedo = load("_diff");
		const std::optional<RgbaImage> normal = load("_nor_gl");
		const std::optional<RgbaImage> surface = load("_arm");
		const std::optional<RgbaImage> height = load("_disp");
		if (!albedo)
		{
			error = albedoFile->string() + " can't be read";
			return false;
		}
		const std::string name = folder.filename().string();
		const PbrBakeInput input{
			.Name = name,
			.Albedo = &*albedo,
			.Alpha = height ? &*height : nullptr,
			.Normal = normal ? &*normal : nullptr,
			.Surface = surface ? &*surface : nullptr,
			.MaxSize = source.TextureSize ? source.TextureSize : 1024,
			.HeightInAlpha = true,
			.Repeat = true,
		};
		PbrBakeOutput output = BakePbrTextures(input);
		ModelMaterial material;
		material.Name = name;
		material.Albedo = StoreTexture(std::move(output.Albedo));
		material.Normal = StoreTexture(std::move(output.Normal));
		material.Surface = StoreTexture(std::move(output.Surface));
		material.Size = source.Size > 0.0f ? source.Size : 1.0f;
		m_Output.Materials.push_back(std::move(material));
		return true;
	}

	bool GltfImporter::ImportEnvironment(const std::filesystem::path& path, const ModelSource& source, std::string& error)
	{
		ModelEnvironment environment;
		std::string report;
		if (!BakeEnvironment(path, StripResolution(path.stem().string()), source.TextureSize ? source.TextureSize : 512, environment, report, error))
			return false;
		m_Reports.push_back(report);
		m_Output.Environments.push_back(std::move(environment));
		return true;
	}
}
