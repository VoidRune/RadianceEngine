#include "GltfLoader.h"
#include "RadianceEngine/Core/Log.h"
#include "RadianceEngine/Core/Timer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <numeric>
#include <set>
#include <tuple>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "tinygltf/tiny_gltf.h"

namespace
{
	struct Warnings
	{
		uint32_t UnsupportedModes = 0;
		uint32_t InvalidPrimitives = 0;
		uint32_t SecondaryUVs = 0;
		uint32_t MixedTransforms = 0;
		uint32_t MissingImages = 0;
		uint32_t IgnoredTextures = 0;
		uint32_t InvalidInstancing = 0;
	};

	struct UvTransform
	{
		glm::vec2 Offset{ 0.0f };
		glm::vec2 Scale{ 1.0f };
		float Rotation = 0.0f;

		bool operator==(const UvTransform&) const = default;
	};

	struct TextureRef
	{
		int Index = -1;
		int TexCoord = 0;
		const tinygltf::Value* Transform = nullptr;
	};

	struct PendingMaterial
	{
		Material Material;
		int BaseColor = -1;
		int MetallicRoughness = -1;
		int Normal = -1;
		int Emissive = -1;
	};

	struct Attributes
	{
		int Position = -1;
		int Normal = -1;
		int TexCoord = -1;
		int Tangent = -1;

		auto operator<=>(const Attributes&) const = default;
	};

	struct VertexRange
	{
		uint32_t First = 0;
		uint32_t Count = 0;
		bool HasTangents = false;
		bool NeedsTangents = false;
		std::vector<uint32_t> Triangles;
	};

	bool KeepEncodedImage(tinygltf::Image* image, const int, std::string*, std::string*, int, int, const unsigned char* bytes, int size, void*)
	{
		if (image->bufferView < 0)
			image->image.assign(bytes, bytes + size);
		image->as_is = true;
		return true;
	}

	std::string ToUtf8(const std::filesystem::path& path)
	{
		const std::u8string text = path.u8string();
		return std::string(text.begin(), text.end());
	}

	std::filesystem::path FromUtf8(const std::string& text)
	{
		return std::filesystem::path(std::u8string(text.begin(), text.end()));
	}

	std::string_view TrimNewlines(std::string_view text)
	{
		while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
			text.remove_suffix(1);
		return text;
	}

	const tinygltf::Value* FindExtension(const tinygltf::ExtensionMap& extensions, const char* name)
	{
		const auto it = extensions.find(name);
		return it != extensions.end() && it->second.IsObject() ? &it->second : nullptr;
	}

	float GetNumber(const tinygltf::Value& object, const char* key, float fallback)
	{
		const tinygltf::Value& value = object.Get(key);
		return value.IsNumber() ? float(value.GetNumberAsDouble()) : fallback;
	}

	template<int N>
	glm::vec<N, float> GetVector(const tinygltf::Value& object, const char* key, const glm::vec<N, float>& fallback)
	{
		const tinygltf::Value& value = object.Get(key);
		if (!value.IsArray() || value.ArrayLen() < size_t(N))
			return fallback;
		glm::vec<N, float> result;
		for (int i = 0; i < N; i++)
		{
			const tinygltf::Value& element = value.Get(size_t(i));
			if (!element.IsNumber())
				return fallback;
			result[i] = float(element.GetNumberAsDouble());
		}
		return result;
	}

	template<typename Info>
	TextureRef MakeRef(const Info& info)
	{
		return { info.index, info.texCoord, FindExtension(info.extensions, "KHR_texture_transform") };
	}

	TextureRef MakeRef(const tinygltf::Value& object, const char* key)
	{
		const tinygltf::Value& info = object.Get(key);
		if (!info.IsObject() || !info.Get("index").IsNumber())
			return {};

		TextureRef ref;
		ref.Index = info.Get("index").GetNumberAsInt();
		if (info.Get("texCoord").IsNumber())
			ref.TexCoord = info.Get("texCoord").GetNumberAsInt();
		const tinygltf::Value& extensions = info.Get("extensions");
		if (extensions.IsObject() && extensions.Get("KHR_texture_transform").IsObject())
			ref.Transform = &extensions.Get("KHR_texture_transform");
		return ref;
	}

	UvTransform ReadTransform(const TextureRef& ref)
	{
		UvTransform transform;
		if (ref.Transform)
		{
			transform.Offset = GetVector<2>(*ref.Transform, "offset", glm::vec2(0.0f));
			transform.Scale = GetVector<2>(*ref.Transform, "scale", glm::vec2(1.0f));
			transform.Rotation = GetNumber(*ref.Transform, "rotation", 0.0f);
		}
		return transform;
	}

	int TexCoordSet(const TextureRef& ref)
	{
		if (ref.Transform && ref.Transform->Get("texCoord").IsNumber())
			return ref.Transform->Get("texCoord").GetNumberAsInt();
		return ref.TexCoord;
	}

	struct TextureRequest
	{
		int Image = -1;
		bool Srgb = true;
		Rdn::SamplerAddressMode AddressMode = Rdn::SamplerAddressMode::Repeat;
	};

	class TextureRequests
	{
	public:
		TextureRequests(const tinygltf::Model& model, Warnings& warnings)
			: m_Model(model)
			, m_Warnings(warnings)
		{
		}

		int Request(const TextureRef& ref, bool srgb)
		{
			if (ref.Index < 0)
				return -1;
			const int image = ResolveImage(ref.Index);
			if (image < 0)
			{
				m_Warnings.MissingImages++;
				return -1;
			}
			const Rdn::SamplerAddressMode addressMode = AddressMode(ref.Index);
			const auto [it, inserted] = m_Lookup.try_emplace({ image, srgb, int(addressMode) }, int(m_Requests.size()));
			if (inserted)
				m_Requests.push_back({ image, srgb, addressMode });
			return it->second;
		}

		const std::vector<TextureRequest>& GetRequests() const { return m_Requests; }

	private:
		Rdn::SamplerAddressMode AddressMode(int textureIndex) const
		{
			const int samplerIndex = m_Model.textures[textureIndex].sampler;
			if (samplerIndex < 0 || size_t(samplerIndex) >= m_Model.samplers.size())
				return Rdn::SamplerAddressMode::Repeat;
			const tinygltf::Sampler& sampler = m_Model.samplers[samplerIndex];
			if (sampler.wrapS != sampler.wrapT)
				return Rdn::SamplerAddressMode::Repeat;
			switch (sampler.wrapS)
			{
			case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE: return Rdn::SamplerAddressMode::ClampToEdge;
			case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT: return Rdn::SamplerAddressMode::MirroredRepeat;
			default: return Rdn::SamplerAddressMode::Repeat;
			}
		}

		int ResolveImage(int textureIndex) const
		{
			if (size_t(textureIndex) >= m_Model.textures.size())
				return -1;
			const tinygltf::Texture& texture = m_Model.textures[textureIndex];
			if (const tinygltf::Value* dds = FindExtension(texture.extensions, "MSFT_texture_dds"))
			{
				const tinygltf::Value& source = dds->Get("source");
				if (source.IsNumber() && source.GetNumberAsInt() >= 0 && size_t(source.GetNumberAsInt()) < m_Model.images.size())
					return source.GetNumberAsInt();
			}
			return texture.source >= 0 && size_t(texture.source) < m_Model.images.size() ? texture.source : -1;
		}

		const tinygltf::Model& m_Model;
		Warnings& m_Warnings;
		std::vector<TextureRequest> m_Requests;
		std::map<std::tuple<int, bool, int>, int> m_Lookup;
	};

	PendingMaterial ConvertMaterial(const tinygltf::Material& source, Scene& scene, TextureRequests& textures, Warnings& warnings)
	{
		PendingMaterial result;
		Material& material = result.Material;
		const tinygltf::PbrMetallicRoughness& pbr = source.pbrMetallicRoughness;
		if (pbr.baseColorFactor.size() == 4)
		{
			material.BaseColor = { float(pbr.baseColorFactor[0]), float(pbr.baseColorFactor[1]), float(pbr.baseColorFactor[2]) };
			material.Alpha = float(pbr.baseColorFactor[3]);
		}
		material.Metallic = float(pbr.metallicFactor);
		material.Roughness = float(pbr.roughnessFactor);
		if (source.emissiveFactor.size() == 3)
			material.Emission = { float(source.emissiveFactor[0]), float(source.emissiveFactor[1]), float(source.emissiveFactor[2]) };
		material.NormalScale = float(source.normalTexture.scale);
		material.AlphaMode = source.alphaMode == "MASK" ? AlphaMode::Mask : source.alphaMode == "BLEND" ? AlphaMode::Blend : AlphaMode::Opaque;
		material.AlphaCutoff = float(source.alphaCutoff);
		material.DoubleSided = source.doubleSided;

		TextureRef baseColor = MakeRef(pbr.baseColorTexture);
		TextureRef metallicRoughness = MakeRef(pbr.metallicRoughnessTexture);
		bool metallicRoughnessSrgb = false;
		const TextureRef normal = MakeRef(source.normalTexture);
		const TextureRef emissive = MakeRef(source.emissiveTexture);

		if (const tinygltf::Value* specularGlossiness = FindExtension(source.extensions, "KHR_materials_pbrSpecularGlossiness"))
		{
			const glm::vec4 diffuse = GetVector<4>(*specularGlossiness, "diffuseFactor", glm::vec4(1.0f));
			material.SpecularGlossiness = true;
			material.BaseColor = glm::vec3(diffuse);
			material.Alpha = diffuse.w;
			material.SpecularColor = GetVector<3>(*specularGlossiness, "specularFactor", glm::vec3(1.0f));
			material.Glossiness = GetNumber(*specularGlossiness, "glossinessFactor", 1.0f);
			baseColor = MakeRef(*specularGlossiness, "diffuseTexture");
			metallicRoughness = MakeRef(*specularGlossiness, "specularGlossinessTexture");
			metallicRoughnessSrgb = true;
		}
		if (const tinygltf::Value* strength = FindExtension(source.extensions, "KHR_materials_emissive_strength"))
			material.Emission *= GetNumber(*strength, "emissiveStrength", 1.0f);
		if (const tinygltf::Value* ior = FindExtension(source.extensions, "KHR_materials_ior"))
			material.IOR = GetNumber(*ior, "ior", 1.5f);
		if (const tinygltf::Value* clearcoat = FindExtension(source.extensions, "KHR_materials_clearcoat"))
		{
			material.Clearcoat = GetNumber(*clearcoat, "clearcoatFactor", 0.0f);
			material.ClearcoatRoughness = GetNumber(*clearcoat, "clearcoatRoughnessFactor", 0.0f);
			warnings.IgnoredTextures += MakeRef(*clearcoat, "clearcoatTexture").Index >= 0 || MakeRef(*clearcoat, "clearcoatRoughnessTexture").Index >= 0;
		}
		if (const tinygltf::Value* sheen = FindExtension(source.extensions, "KHR_materials_sheen"))
			material.Sheen = GetVector<3>(*sheen, "sheenColorFactor", glm::vec3(0.0f));
		if (const tinygltf::Value* transmission = FindExtension(source.extensions, "KHR_materials_transmission"))
		{
			material.Transmission = GetNumber(*transmission, "transmissionFactor", 0.0f);
			material.ThinWalled = true;
			warnings.IgnoredTextures += MakeRef(*transmission, "transmissionTexture").Index >= 0;
		}
		if (const tinygltf::Value* volume = FindExtension(source.extensions, "KHR_materials_volume"); volume && material.Transmission > 0.0f)
		{
			if (GetNumber(*volume, "thicknessFactor", 0.0f) > 0.0f)
			{
				material.ThinWalled = false;
				const float distance = GetNumber(*volume, "attenuationDistance", std::numeric_limits<float>::infinity());
				const glm::vec3 color = glm::clamp(GetVector<3>(*volume, "attenuationColor", glm::vec3(1.0f)), glm::vec3(1e-4f), glm::vec3(1.0f));
				if (std::isfinite(distance) && distance > 0.0f && glm::any(glm::lessThan(color, glm::vec3(1.0f))))
					material.Medium = scene.AddMedium({ .Absorption = -glm::log(color) / distance });
			}
		}

		result.BaseColor = textures.Request(baseColor, true);
		result.MetallicRoughness = textures.Request(metallicRoughness, metallicRoughnessSrgb);
		result.Normal = textures.Request(normal, false);
		result.Emissive = textures.Request(emissive, true);

		if (FindExtension(source.extensions, "KHR_materials_unlit"))
		{
			material.Emission = material.BaseColor;
			material.BaseColor = glm::vec3(0.0f);
			result.Emissive = result.BaseColor;
		}

		const TextureRef* used[] = { &baseColor, &normal, &metallicRoughness, &emissive };
		const TextureRef* first = nullptr;
		for (const TextureRef* ref : used)
		{
			if (ref->Index < 0)
				continue;
			warnings.SecondaryUVs += TexCoordSet(*ref) != 0;
			if (!first)
				first = ref;
			else if (!(ReadTransform(*ref) == ReadTransform(*first)))
				warnings.MixedTransforms++;
		}
		if (first)
		{
			const UvTransform transform = ReadTransform(*first);
			material.UVOffset = transform.Offset;
			material.UVScale = transform.Scale;
			material.UVRotation = transform.Rotation;
		}
		return result;
	}

	float ReadComponent(const uint8_t* data, int componentType, bool normalized)
	{
		switch (componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
		{
			float value;
			std::memcpy(&value, data, sizeof(value));
			return value;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			return normalized ? data[0] / 255.0f : float(data[0]);
		case TINYGLTF_COMPONENT_TYPE_BYTE:
			return normalized ? std::max(int8_t(data[0]) / 127.0f, -1.0f) : float(int8_t(data[0]));
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		{
			uint16_t value;
			std::memcpy(&value, data, sizeof(value));
			return normalized ? value / 65535.0f : float(value);
		}
		case TINYGLTF_COMPONENT_TYPE_SHORT:
		{
			int16_t value;
			std::memcpy(&value, data, sizeof(value));
			return normalized ? std::max(value / 32767.0f, -1.0f) : float(value);
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

	uint32_t ReadIndex(const uint8_t* data, int componentType)
	{
		switch (componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			return data[0];
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		{
			uint16_t value;
			std::memcpy(&value, data, sizeof(value));
			return value;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		{
			uint32_t value;
			std::memcpy(&value, data, sizeof(value));
			return value;
		}
		default:
			return UINT32_MAX;
		}
	}

	bool LocateElements(const tinygltf::Model& model, int bufferViewIndex, size_t byteOffset, size_t explicitStride, size_t count, size_t elementSize,
		const uint8_t*& data, size_t& stride)
	{
		if (bufferViewIndex < 0 || size_t(bufferViewIndex) >= model.bufferViews.size())
			return false;
		const tinygltf::BufferView& view = model.bufferViews[bufferViewIndex];
		if (view.buffer < 0 || size_t(view.buffer) >= model.buffers.size())
			return false;
		const tinygltf::Buffer& buffer = model.buffers[view.buffer];
		stride = explicitStride ? explicitStride : (view.byteStride ? view.byteStride : elementSize);
		data = buffer.data.data() + view.byteOffset + byteOffset;
		if (count == 0)
			return true;
		const size_t end = byteOffset + (count - 1) * stride + elementSize;
		return end <= view.byteLength && view.byteOffset + view.byteLength <= buffer.data.size();
	}

	bool ReadAccessor(const tinygltf::Model& model, int accessorIndex, int components, std::vector<float>& out)
	{
		if (accessorIndex < 0 || size_t(accessorIndex) >= model.accessors.size())
			return false;
		const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
		const int accessorComponents = tinygltf::GetNumComponentsInType(uint32_t(accessor.type));
		const int componentSize = tinygltf::GetComponentSizeInBytes(uint32_t(accessor.componentType));
		if (accessorComponents <= 0 || componentSize <= 0)
			return false;

		const int copied = std::min(components, accessorComponents);
		const size_t elementSize = size_t(componentSize) * accessorComponents;
		out.assign(accessor.count * components, 0.0f);
		auto readElement = [&](const uint8_t* element, size_t target) {
			for (int c = 0; c < copied; c++)
				out[target * components + c] = ReadComponent(element + c * componentSize, accessor.componentType, accessor.normalized);
		};

		if (accessor.bufferView >= 0)
		{
			const uint8_t* data = nullptr;
			size_t stride = 0;
			if (!LocateElements(model, accessor.bufferView, accessor.byteOffset, 0, accessor.count, elementSize, data, stride))
				return false;
			if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && accessorComponents == components && stride == elementSize)
			{
				if (accessor.count > 0)
					std::memcpy(out.data(), data, accessor.count * elementSize);
			}
			else
			{
				for (size_t i = 0; i < accessor.count; i++)
					readElement(data + i * stride, i);
			}
		}

		if (accessor.sparse.isSparse && accessor.sparse.count > 0)
		{
			const auto& sparse = accessor.sparse;
			const int indexSize = tinygltf::GetComponentSizeInBytes(uint32_t(sparse.indices.componentType));
			const uint8_t* indices = nullptr;
			const uint8_t* values = nullptr;
			size_t indexStride = 0, valueStride = 0;
			if (indexSize <= 0
				|| !LocateElements(model, sparse.indices.bufferView, sparse.indices.byteOffset, indexSize, sparse.count, indexSize, indices, indexStride)
				|| !LocateElements(model, sparse.values.bufferView, sparse.values.byteOffset, elementSize, sparse.count, elementSize, values, valueStride))
				return false;
			for (int i = 0; i < sparse.count; i++)
			{
				const uint32_t target = ReadIndex(indices + size_t(i) * indexStride, sparse.indices.componentType);
				if (target >= accessor.count)
					return false;
				readElement(values + size_t(i) * valueStride, target);
			}
		}
		return true;
	}

	bool ReadIndices(const tinygltf::Model& model, int accessorIndex, std::vector<uint32_t>& out)
	{
		if (accessorIndex < 0 || size_t(accessorIndex) >= model.accessors.size())
			return false;
		const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
		const int componentSize = tinygltf::GetComponentSizeInBytes(uint32_t(accessor.componentType));
		if (accessor.type != TINYGLTF_TYPE_SCALAR || accessor.bufferView < 0 || accessor.sparse.isSparse
			|| (componentSize != 1 && componentSize != 2 && componentSize != 4) || accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
			return false;

		const uint8_t* data = nullptr;
		size_t stride = 0;
		if (!LocateElements(model, accessor.bufferView, accessor.byteOffset, componentSize, accessor.count, componentSize, data, stride))
			return false;
		out.resize(accessor.count);
		for (size_t i = 0; i < accessor.count; i++)
			out[i] = ReadIndex(data + i * stride, accessor.componentType);
		return true;
	}

	glm::vec3 SafeNormalize(const glm::vec3& v)
	{
		const float length = glm::length(v);
		return length > 0.0f && std::isfinite(length) ? v / length : glm::vec3(0.0f);
	}

	glm::vec3 AnyPerpendicular(const glm::vec3& n)
	{
		const glm::vec3 axis = std::abs(n.x) < 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
		return SafeNormalize(glm::cross(axis, n));
	}

	void GenerateTangents(std::span<Vertex> vertices, std::span<const uint32_t> triangles)
	{
		std::vector<glm::vec3> tangents(vertices.size(), glm::vec3(0.0f));
		std::vector<glm::vec3> bitangents(vertices.size(), glm::vec3(0.0f));
		for (size_t i = 0; i + 2 < triangles.size(); i += 3)
		{
			const uint32_t a = triangles[i], b = triangles[i + 1], c = triangles[i + 2];
			const glm::vec3 e1 = vertices[b].Position - vertices[a].Position;
			const glm::vec3 e2 = vertices[c].Position - vertices[a].Position;
			const glm::vec2 d1 = vertices[b].UV - vertices[a].UV;
			const glm::vec2 d2 = vertices[c].UV - vertices[a].UV;
			const float determinant = d1.x * d2.y - d2.x * d1.y;
			if (!(std::abs(determinant) > 1e-20f))
				continue;
			const glm::vec3 tangent = (e1 * d2.y - e2 * d1.y) / determinant;
			const glm::vec3 bitangent = (e2 * d1.x - e1 * d2.x) / determinant;
			for (uint32_t vertex : { a, b, c })
			{
				tangents[vertex] += tangent;
				bitangents[vertex] += bitangent;
			}
		}

		for (size_t i = 0; i < vertices.size(); i++)
		{
			const glm::vec3 n = vertices[i].Normal;
			glm::vec3 t = SafeNormalize(tangents[i] - n * glm::dot(n, tangents[i]));
			if (glm::dot(t, t) == 0.0f)
				t = AnyPerpendicular(n);
			const float handedness = glm::dot(glm::cross(n, t), bitangents[i]) < 0.0f ? 1.0f : -1.0f;
			vertices[i].Tangent = glm::vec4(t, handedness);
		}
	}

	bool ReadVertices(const tinygltf::Model& model, const Attributes& attributes, std::vector<Vertex>& vertices, bool& hasNormals, bool& hasTangents)
	{
		std::vector<float> positions, normals, uvs, tangents;
		if (!ReadAccessor(model, attributes.Position, 3, positions))
			return false;
		const size_t count = positions.size() / 3;
		auto readOptional = [&](int accessor, int components, std::vector<float>& out) {
			if (accessor < 0 || !ReadAccessor(model, accessor, components, out) || out.size() != count * components)
			{
				out.clear();
				return false;
			}
			return true;
		};
		hasNormals = readOptional(attributes.Normal, 3, normals);
		const bool hasUVs = readOptional(attributes.TexCoord, 2, uvs);
		hasTangents = readOptional(attributes.Tangent, 4, tangents);

		vertices.resize(count);
		for (size_t i = 0; i < count; i++)
		{
			Vertex& vertex = vertices[i];
			vertex.Position = { positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2] };
			if (hasNormals)
				vertex.Normal = SafeNormalize({ normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2] });
			if (hasUVs)
				vertex.UV = { uvs[i * 2], uvs[i * 2 + 1] };
			if (hasTangents)
				vertex.Tangent = glm::vec4(SafeNormalize({ tangents[i * 4], tangents[i * 4 + 1], tangents[i * 4 + 2] }), tangents[i * 4 + 3] < 0.0f ? -1.0f : 1.0f);
		}
		return true;
	}

	bool ReadTriangles(const tinygltf::Model& model, const tinygltf::Primitive& primitive, int mode, size_t vertexCount, std::vector<uint32_t>& triangles)
	{
		std::vector<uint32_t> indices;
		if (primitive.indices >= 0)
		{
			if (!ReadIndices(model, primitive.indices, indices))
				return false;
		}
		else
		{
			indices.resize(vertexCount);
			std::iota(indices.begin(), indices.end(), 0u);
		}
		if (std::ranges::any_of(indices, [&](uint32_t index) { return index >= vertexCount; }))
			return false;

		if (mode == TINYGLTF_MODE_TRIANGLES)
		{
			indices.resize(indices.size() / 3 * 3);
			triangles = std::move(indices);
		}
		else if (mode == TINYGLTF_MODE_TRIANGLE_STRIP)
		{
			for (size_t i = 0; i + 2 < indices.size(); i++)
				triangles.insert(triangles.end(), { indices[i], indices[i + 1 + i % 2], indices[i + 2 - i % 2] });
		}
		else
		{
			for (size_t i = 1; i + 1 < indices.size(); i++)
				triangles.insert(triangles.end(), { indices[i], indices[i + 1], indices[0] });
		}
		return true;
	}

	void ConvertMesh(const tinygltf::Model& model, const tinygltf::Mesh& source, std::span<const MaterialId> materials, std::span<const uint8_t> normalMapped,
		const std::function<MaterialId()>& defaultMaterial, Warnings& warnings, MeshData& mesh, std::vector<ModelPart>& parts)
	{
		std::map<Attributes, VertexRange> ranges;
		for (const tinygltf::Primitive& primitive : source.primitives)
		{
			const int mode = primitive.mode < 0 ? TINYGLTF_MODE_TRIANGLES : primitive.mode;
			if (mode != TINYGLTF_MODE_TRIANGLES && mode != TINYGLTF_MODE_TRIANGLE_STRIP && mode != TINYGLTF_MODE_TRIANGLE_FAN)
			{
				warnings.UnsupportedModes++;
				continue;
			}
			auto attribute = [&](const char* name) {
				const auto it = primitive.attributes.find(name);
				return it != primitive.attributes.end() ? it->second : -1;
			};
			const Attributes attributes{ attribute("POSITION"), attribute("NORMAL"), attribute("TEXCOORD_0"), attribute("TANGENT") };
			if (attributes.Position < 0 || size_t(attributes.Position) >= model.accessors.size())
			{
				warnings.InvalidPrimitives++;
				continue;
			}

			const bool validMaterial = primitive.material >= 0 && size_t(primitive.material) < materials.size();
			const MaterialId material = validMaterial ? materials[primitive.material] : defaultMaterial();
			const bool needsTangents = validMaterial && normalMapped[primitive.material];

			std::vector<uint32_t> triangles;
			if (!ReadTriangles(model, primitive, mode, model.accessors[attributes.Position].count, triangles))
			{
				warnings.InvalidPrimitives++;
				continue;
			}
			if (triangles.empty())
				continue;

			uint32_t baseVertex = 0;
			const auto shared = ranges.find(attributes);
			if (shared != ranges.end())
			{
				baseVertex = shared->second.First;
				shared->second.NeedsTangents |= needsTangents && !shared->second.HasTangents;
				if (!shared->second.HasTangents)
					shared->second.Triangles.insert(shared->second.Triangles.end(), triangles.begin(), triangles.end());
			}
			else
			{
				std::vector<Vertex> vertices;
				bool hasNormals = false, hasTangents = false;
				if (!ReadVertices(model, attributes, vertices, hasNormals, hasTangents))
				{
					warnings.InvalidPrimitives++;
					continue;
				}

				baseVertex = uint32_t(mesh.Vertices.size());
				if (hasNormals)
				{
					VertexRange& range = ranges[attributes];
					range.First = baseVertex;
					range.Count = uint32_t(vertices.size());
					range.HasTangents = hasTangents;
					range.NeedsTangents = needsTangents && !hasTangents;
					if (!hasTangents)
						range.Triangles = triangles;
					mesh.Vertices.insert(mesh.Vertices.end(), vertices.begin(), vertices.end());
				}
				else
				{
					std::vector<Vertex> flat;
					flat.reserve(triangles.size());
					for (size_t i = 0; i < triangles.size(); i += 3)
					{
						const Vertex& a = vertices[triangles[i]];
						const Vertex& b = vertices[triangles[i + 1]];
						const Vertex& c = vertices[triangles[i + 2]];
						const glm::vec3 normal = SafeNormalize(glm::cross(b.Position - a.Position, c.Position - a.Position));
						for (const Vertex* corner : { &a, &b, &c })
						{
							flat.push_back(*corner);
							flat.back().Normal = normal;
						}
					}
					std::iota(triangles.begin(), triangles.end(), 0u);
					if (needsTangents && !hasTangents)
						GenerateTangents(flat, triangles);
					mesh.Vertices.insert(mesh.Vertices.end(), flat.begin(), flat.end());
				}
			}

			ModelPart& part = parts.emplace_back();
			part.FirstIndex = uint32_t(mesh.Indices.size());
			part.IndexCount = uint32_t(triangles.size());
			part.Material = material;
			for (uint32_t index : triangles)
				mesh.Indices.push_back(baseVertex + index);
		}

		for (auto& [attributes, range] : ranges)
		{
			if (range.NeedsTangents)
				GenerateTangents(std::span(mesh.Vertices).subspan(range.First, range.Count), range.Triangles);
		}
	}

	glm::mat4 LocalMatrix(const tinygltf::Node& node)
	{
		if (node.matrix.size() == 16)
		{
			glm::mat4 matrix;
			for (int i = 0; i < 16; i++)
				matrix[i / 4][i % 4] = float(node.matrix[i]);
			return matrix;
		}

		glm::mat4 matrix(1.0f);
		if (node.translation.size() == 3)
			matrix = glm::translate(matrix, glm::vec3(float(node.translation[0]), float(node.translation[1]), float(node.translation[2])));
		if (node.rotation.size() == 4)
			matrix *= glm::mat4_cast(glm::quat(float(node.rotation[3]), float(node.rotation[0]), float(node.rotation[1]), float(node.rotation[2])));
		if (node.scale.size() == 3)
			matrix = glm::scale(matrix, glm::vec3(float(node.scale[0]), float(node.scale[1]), float(node.scale[2])));
		return matrix;
	}

	std::vector<glm::mat4> InstanceMatrices(const tinygltf::Model& model, const tinygltf::Node& node, Warnings& warnings)
	{
		const tinygltf::Value* instancing = FindExtension(node.extensions, "EXT_mesh_gpu_instancing");
		if (!instancing || !instancing->Get("attributes").IsObject())
			return { glm::mat4(1.0f) };

		const tinygltf::Value& attributes = instancing->Get("attributes");
		auto read = [&](const char* name, int components, std::vector<float>& out) {
			const tinygltf::Value& accessor = attributes.Get(name);
			return accessor.IsNumber() && ReadAccessor(model, accessor.GetNumberAsInt(), components, out);
		};
		std::vector<float> translations, rotations, scales;
		const bool hasTranslation = read("TRANSLATION", 3, translations);
		const bool hasRotation = read("ROTATION", 4, rotations);
		const bool hasScale = read("SCALE", 3, scales);
		size_t count = SIZE_MAX;
		if (hasTranslation)
			count = std::min(count, translations.size() / 3);
		if (hasRotation)
			count = std::min(count, rotations.size() / 4);
		if (hasScale)
			count = std::min(count, scales.size() / 3);
		if (count == SIZE_MAX)
		{
			warnings.InvalidInstancing++;
			return { glm::mat4(1.0f) };
		}

		std::vector<glm::mat4> matrices(count, glm::mat4(1.0f));
		for (size_t i = 0; i < count; i++)
		{
			if (hasTranslation)
				matrices[i] = glm::translate(matrices[i], glm::vec3(translations[i * 3], translations[i * 3 + 1], translations[i * 3 + 2]));
			if (hasRotation)
				matrices[i] *= glm::mat4_cast(glm::normalize(glm::quat(rotations[i * 4 + 3], rotations[i * 4], rotations[i * 4 + 1], rotations[i * 4 + 2])));
			if (hasScale)
				matrices[i] = glm::scale(matrices[i], glm::vec3(scales[i * 3], scales[i * 3 + 1], scales[i * 3 + 2]));
		}
		return matrices;
	}
}

std::optional<SceneImport> LoadGltf(Scene& scene, const std::filesystem::path& path, const glm::mat4& transform)
{
	const Rdn::Timer timer;
	tinygltf::TinyGLTF loader;
	loader.SetImageLoader(KeepEncodedImage, nullptr);
	tinygltf::Model model;
	std::string error, warning;
	const std::string utf8Path = ToUtf8(path);
	std::string extension = path.extension().string();
	std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return char(std::tolower(c)); });
	const bool loaded = extension == ".glb"
		? loader.LoadBinaryFromFile(&model, &error, &warning, utf8Path)
		: loader.LoadASCIIFromFile(&model, &error, &warning, utf8Path);
	if (!warning.empty())
		RDN_LOG_WARNING("{}: {}", path.generic_string(), TrimNewlines(warning));
	if (!loaded)
	{
		RDN_LOG_ERROR("Failed to load {}: {}", path.generic_string(), TrimNewlines(error));
		return std::nullopt;
	}
	const double parseMilliseconds = timer.ElapsedMilliseconds();

	static const std::set<std::string> supportedExtensions = {
		"KHR_materials_emissive_strength", "KHR_materials_transmission", "KHR_materials_ior", "KHR_materials_volume",
		"KHR_materials_clearcoat", "KHR_materials_sheen", "KHR_materials_unlit", "KHR_materials_pbrSpecularGlossiness",
		"KHR_texture_transform", "KHR_mesh_quantization", "MSFT_texture_dds", "EXT_mesh_gpu_instancing",
	};
	for (const std::string& required : model.extensionsRequired)
	{
		if (supportedExtensions.contains(required))
			continue;
		if (required == "KHR_draco_mesh_compression" || required == "EXT_meshopt_compression")
		{
			RDN_LOG_ERROR("Failed to load {}: it requires {}, which isn't supported", path.generic_string(), required);
			return std::nullopt;
		}
		RDN_LOG_WARNING("{} requires {}, which isn't supported, so it may render incorrectly", path.generic_string(), required);
	}

	Warnings warnings;
	TextureRequests textureRequests(model, warnings);
	std::vector<PendingMaterial> pending;
	pending.reserve(model.materials.size());
	for (const tinygltf::Material& material : model.materials)
		pending.push_back(ConvertMaterial(material, scene, textureRequests, warnings));

	const std::filesystem::path directory = path.parent_path();
	std::vector<TextureSource> sources;
	for (const TextureRequest& request : textureRequests.GetRequests())
	{
		const tinygltf::Image& image = model.images[request.Image];
		TextureSource& source = sources.emplace_back();
		source.Srgb = request.Srgb;
		source.AddressMode = request.AddressMode;
		if (image.bufferView >= 0 && size_t(image.bufferView) < model.bufferViews.size())
		{
			const tinygltf::BufferView& view = model.bufferViews[image.bufferView];
			if (view.buffer >= 0 && size_t(view.buffer) < model.buffers.size() && view.byteOffset + view.byteLength <= model.buffers[view.buffer].data.size())
				source.Data = std::span(model.buffers[view.buffer].data).subspan(view.byteOffset, view.byteLength);
		}
		else if (!image.image.empty())
		{
			source.Data = image.image;
		}
		else
		{
			std::string decoded;
			tinygltf::URIDecode(image.uri, &decoded, nullptr);
			source.Path = directory / FromUtf8(decoded);
		}
	}
	const std::vector<TextureId> textures = scene.LoadTextures(sources);
	auto texture = [&](int request) { return request >= 0 ? textures[request] : TextureId{}; };

	std::vector<MaterialId> materials;
	std::vector<uint8_t> normalMapped;
	for (PendingMaterial& material : pending)
	{
		material.Material.BaseColorTexture = texture(material.BaseColor);
		material.Material.MetallicRoughnessTexture = texture(material.MetallicRoughness);
		material.Material.NormalTexture = texture(material.Normal);
		material.Material.EmissiveTexture = texture(material.Emissive);
		materials.push_back(scene.AddMaterial(material.Material));
		normalMapped.push_back(material.Material.NormalTexture.IsValid());
	}
	MaterialId gltfDefaultMaterial = {};
	auto defaultMaterial = [&] {
		if (!gltfDefaultMaterial.IsValid())
			gltfDefaultMaterial = scene.AddMaterial({ .Metallic = 1.0f, .Roughness = 1.0f });
		return gltfDefaultMaterial;
	};

	std::vector<int> roots;
	if (!model.scenes.empty())
	{
		const size_t sceneIndex = model.defaultScene >= 0 && size_t(model.defaultScene) < model.scenes.size() ? size_t(model.defaultScene) : 0;
		roots = model.scenes[sceneIndex].nodes;
	}
	else
	{
		std::vector<uint8_t> isChild(model.nodes.size(), 0);
		for (const tinygltf::Node& node : model.nodes)
		{
			for (int child : node.children)
			{
				if (child >= 0 && size_t(child) < model.nodes.size())
					isChild[child] = 1;
			}
		}
		for (size_t i = 0; i < model.nodes.size(); i++)
		{
			if (!isChild[i])
				roots.push_back(int(i));
		}
	}

	struct MeshInstance
	{
		int Mesh;
		glm::mat4 Transform;
	};
	SceneImport result;
	std::vector<MeshInstance> meshInstances;
	uint32_t punctualLights = 0;
	const glm::mat4 root = transform * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, -1.0f));
	std::vector<std::pair<int, glm::mat4>> stack;
	for (auto it = roots.rbegin(); it != roots.rend(); ++it)
		stack.emplace_back(*it, root);
	size_t visits = 0;
	while (!stack.empty() && visits++ <= model.nodes.size())
	{
		const auto [nodeIndex, parent] = stack.back();
		stack.pop_back();
		if (nodeIndex < 0 || size_t(nodeIndex) >= model.nodes.size())
			continue;

		const tinygltf::Node& node = model.nodes[nodeIndex];
		const glm::mat4 world = parent * LocalMatrix(node);
		if (node.mesh >= 0 && size_t(node.mesh) < model.meshes.size())
		{
			for (const glm::mat4& instance : InstanceMatrices(model, node, warnings))
				meshInstances.push_back({ node.mesh, world * instance });
		}
		if (node.camera >= 0 && size_t(node.camera) < model.cameras.size() && model.cameras[node.camera].type == "perspective")
		{
			const tinygltf::Camera& camera = model.cameras[node.camera];
			result.Cameras.push_back({
				.Name = camera.name.empty() ? node.name : camera.name,
				.Position = glm::vec3(world[3]),
				.Forward = SafeNormalize(glm::vec3(world * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f))),
				.VerticalFov = glm::degrees(float(camera.perspective.yfov)),
			});
		}
		if (node.light >= 0)
			punctualLights++;
		for (auto child = node.children.rbegin(); child != node.children.rend(); ++child)
			stack.emplace_back(*child, world);
	}

	std::vector<ModelId> models(model.meshes.size());
	std::vector<uint8_t> converted(model.meshes.size(), 0);
	size_t primitiveCount = 0, triangleCount = 0;
	for (const MeshInstance& instance : meshInstances)
	{
		if (std::exchange(converted[instance.Mesh], 1))
			continue;
		MeshData mesh;
		std::vector<ModelPart> parts;
		ConvertMesh(model, model.meshes[instance.Mesh], materials, normalMapped, defaultMaterial, warnings, mesh, parts);
		if (parts.empty())
			continue;
		models[instance.Mesh] = scene.AddModel(mesh, parts);
		primitiveCount += parts.size();
		triangleCount += mesh.Indices.size() / 3;
	}

	size_t instanceCount = 0;
	for (const MeshInstance& instance : meshInstances)
	{
		const ModelId modelId = models[instance.Mesh];
		if (!modelId.IsValid())
			continue;
		scene.AddInstance(modelId, instance.Transform);
		glm::vec3 min, max;
		scene.GetModelBounds(modelId, min, max);
		result.AddBounds(min, max, instance.Transform);
		instanceCount++;
	}

	if (warnings.UnsupportedModes > 0)
		RDN_LOG_WARNING("{}: skipped {} point or line primitives", path.generic_string(), warnings.UnsupportedModes);
	if (warnings.InvalidPrimitives > 0)
		RDN_LOG_WARNING("{}: skipped {} primitives with missing or invalid data", path.generic_string(), warnings.InvalidPrimitives);
	if (warnings.SecondaryUVs > 0)
		RDN_LOG_WARNING("{}: {} textures use a second UV set, they are sampled with TEXCOORD_0", path.generic_string(), warnings.SecondaryUVs);
	if (warnings.MixedTransforms > 0)
		RDN_LOG_WARNING("{}: {} textures have their own KHR_texture_transform, each material uses one transform for all its textures", path.generic_string(), warnings.MixedTransforms);
	if (warnings.MissingImages > 0)
		RDN_LOG_WARNING("{}: {} textures have no loadable image source (KTX2 and WebP aren't supported)", path.generic_string(), warnings.MissingImages);
	if (warnings.IgnoredTextures > 0)
		RDN_LOG_WARNING("{}: ignored {} transmission and clearcoat textures, their factors are used", path.generic_string(), warnings.IgnoredTextures);
	if (warnings.InvalidInstancing > 0)
		RDN_LOG_WARNING("{}: {} nodes have unreadable EXT_mesh_gpu_instancing data", path.generic_string(), warnings.InvalidInstancing);
	if (punctualLights > 0)
		RDN_LOG_WARNING("{}: ignored {} KHR_lights_punctual lights, only emissive surfaces and the environment emit light", path.generic_string(), punctualLights);

	RDN_LOG("Loaded {}: {} meshes ({} primitives, {} triangles), {} instances, {} materials, {} textures, {} cameras in {:.0f} ms (parsing {:.0f} ms)",
		path.generic_string(), std::ranges::count(converted, 1), primitiveCount, triangleCount, instanceCount, model.materials.size(), sources.size(),
		result.Cameras.size(), timer.ElapsedMilliseconds(), parseMilliseconds);
	if (instanceCount == 0)
	{
		RDN_LOG_ERROR("{} has no renderable meshes", path.generic_string());
		return std::nullopt;
	}
	return result;
}
