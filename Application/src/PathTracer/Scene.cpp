#include "Scene.h"
#include "GltfLoader.h"
#include "ObjLoader.h"
#include "TextureLoader.h"
#include "RadianceEngine/Core/Log.h"
#include "RadianceEngine/Core/Timer.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <deque>
#include <format>
#include <future>
#include <thread>

namespace
{
	struct GpuMaterial
	{
		glm::vec3 BaseColor;
		float Metallic;
		glm::vec3 Emission;
		float Roughness;
		glm::vec3 Sheen;
		float Transmission;
		glm::vec3 SpecularColor;
		float Alpha;
		float IOR;
		float Clearcoat;
		float ClearcoatRoughness;
		float NormalScale;
		uint32_t BaseColorTexture;
		uint32_t MetallicRoughnessTexture;
		uint32_t NormalTexture;
		uint32_t EmissiveTexture;
		glm::vec4 UVTransform;
		glm::vec2 UVOffset;
		float AlphaCutoff;
		int32_t MediumIndex;
		uint32_t Flags;
		float EmissionLuminance;
		uint32_t Pad[2];
	};

	struct GpuMeshPrimitive
	{
		uint64_t VertexAddress;
		uint64_t IndexAddress;
		uint32_t MaterialIndex;
	};

	struct GpuMedium
	{
		glm::vec3 SigmaA;
		float Anisotropy;
		glm::vec3 SigmaS;
		float Pad;
	};

	struct GpuLightTriangle
	{
		glm::vec3 P0;
		float Probability;
		glm::vec3 P1;
		uint32_t MaterialIndex;
		glm::vec3 P2;
		uint32_t Alias;
		glm::vec2 UV0;
		glm::vec2 UV1;
		glm::vec2 UV2;
		glm::vec2 Pad;
	};

	struct GpuLightHeader
	{
		uint32_t Count;
		float TotalPower;
		uint32_t Pad[2];
	};

	struct GpuMediumHeader
	{
		int32_t GlobalMedium;
		uint32_t Count;
		uint32_t Pad[2];
	};

	struct GpuEnvironmentHeader
	{
		uint32_t Width;
		uint32_t Height;
		float Total;
		float Intensity;
		float RotationCos;
		float RotationSin;
		uint32_t Pad[2];
	};

	static_assert(sizeof(Vertex) == 48);
	static_assert(sizeof(GpuMaterial) == 144 && offsetof(GpuMaterial, UVTransform) == 96 && offsetof(GpuMaterial, Flags) == 128);
	static_assert(sizeof(GpuMeshPrimitive) == 24);
	static_assert(sizeof(GpuMedium) == 32);
	static_assert(sizeof(GpuLightTriangle) == 80);
	static_assert(sizeof(GpuLightHeader) == 16 && sizeof(GpuMediumHeader) == 16 && sizeof(GpuEnvironmentHeader) == 32);

	constexpr uint32_t MaterialNullSurface = 1u << 0;
	constexpr uint32_t MaterialAlphaMask = 1u << 1;
	constexpr uint32_t MaterialAlphaBlend = 1u << 2;
	constexpr uint32_t MaterialSpecularGlossiness = 1u << 3;
	constexpr uint32_t MaterialDoubleSided = 1u << 4;
	constexpr uint32_t MaterialThinWalled = 1u << 5;
	constexpr uint32_t MaxInstanceCustomIndex = (1u << 24) - 1;
	constexpr uint32_t DefaultEnvironmentWidth = 1024;
	constexpr Rdn::BufferUsage GeometryUsage = Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::ShaderDeviceAddress
		| Rdn::BufferUsage::AccelerationStructureBuildInputReadOnly | Rdn::BufferUsage::TransferDst;

	float Luminance(const glm::vec3& color)
	{
		return glm::dot(color, glm::vec3(0.2126f, 0.7152f, 0.0722f));
	}

	template<typename Entry>
	void BuildAliasTable(std::span<const double> weights, double total, std::vector<Entry>& entries)
	{
		const size_t count = weights.size();
		std::vector<double> scaled(count);
		std::vector<uint32_t> small, large;
		for (size_t i = 0; i < count; i++)
		{
			scaled[i] = total > 0.0 ? weights[i] * double(count) / total : 1.0;
			entries[i].Probability = 1.0f;
			entries[i].Alias = uint32_t(i);
			(scaled[i] < 1.0 ? small : large).push_back(uint32_t(i));
		}
		while (!small.empty() && !large.empty())
		{
			const uint32_t lesser = small.back();
			const uint32_t greater = large.back();
			small.pop_back();
			entries[lesser].Probability = float(scaled[lesser]);
			entries[lesser].Alias = greater;
			scaled[greater] += scaled[lesser] - 1.0;
			if (scaled[greater] < 1.0)
			{
				large.pop_back();
				small.push_back(greater);
			}
		}
	}

	template<typename Header, typename Element>
	std::vector<uint8_t> PackBuffer(const Header& header, const std::vector<Element>& elements)
	{
		std::vector<uint8_t> bytes(sizeof(Header) + elements.size() * sizeof(Element));
		std::memcpy(bytes.data(), &header, sizeof(Header));
		if (!elements.empty())
			std::memcpy(bytes.data() + sizeof(Header), elements.data(), elements.size() * sizeof(Element));
		return bytes;
	}

	glm::vec3 DefaultSky(const glm::vec3& direction)
	{
		const glm::vec3 sunDirection = glm::normalize(glm::vec3(1.0f));
		const glm::vec3 groundColor(0.9f);
		const glm::vec3 horizonColor(1.0f);
		const glm::vec3 zenithColor(0.08f, 0.37f, 0.73f);

		const float sun = std::pow(std::max(0.0f, glm::dot(direction, sunDirection)), 100.0f) * 100.0f;
		const float skyGradient = std::pow(glm::smoothstep(0.0f, 0.4f, direction.y), 0.45f);
		const float groundToSky = glm::smoothstep(-0.01f, 0.0f, direction.y);
		const glm::vec3 sky = glm::mix(horizonColor, zenithColor, skyGradient);
		return glm::mix(groundColor, sky, groundToSky) + sun * float(groundToSky >= 1.0f);
	}

	glm::vec3 EquirectDirection(float u, float v)
	{
		const float phi = (u - 0.5f) * glm::two_pi<float>();
		const float theta = v * glm::pi<float>();
		return { std::sin(theta) * std::sin(phi), std::cos(theta), std::sin(theta) * std::cos(phi) };
	}

	std::string TextureCacheKey(const TextureSource& source)
	{
		return std::format("{}|{}|{}", source.Path.lexically_normal().generic_string(), source.Srgb ? "srgb" : "linear", int(source.AddressMode));
	}

	std::string ToLower(std::string text)
	{
		std::ranges::transform(text, text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
		return text;
	}
}

glm::mat4 Transform::ToMatrix() const
{
	glm::mat4 matrix = glm::translate(glm::mat4(1.0f), Position);
	matrix = glm::rotate(matrix, glm::radians(Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	matrix = glm::rotate(matrix, glm::radians(Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	matrix = glm::rotate(matrix, glm::radians(Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	return glm::scale(matrix, Scale);
}

void SceneImport::AddBounds(const glm::vec3& min, const glm::vec3& max, const glm::mat4& transform)
{
	for (int corner = 0; corner < 8; corner++)
	{
		const glm::vec3 local((corner & 1) ? max.x : min.x, (corner & 2) ? max.y : min.y, (corner & 4) ? max.z : min.z);
		const glm::vec3 world = glm::vec3(transform * glm::vec4(local, 1.0f));
		BoundsMin = glm::min(BoundsMin, world);
		BoundsMax = glm::max(BoundsMax, world);
	}
}

Scene::Scene(Rdn::Device* device, Rdn::ResourceAllocator* allocator)
	: m_Device(device)
	, m_Allocator(allocator)
{
	const std::pair<std::unique_ptr<Rdn::Sampler>*, Rdn::SamplerAddressMode> samplers[] = {
		{ &m_RepeatSampler, Rdn::SamplerAddressMode::Repeat },
		{ &m_ClampSampler, Rdn::SamplerAddressMode::ClampToEdge },
		{ &m_MirrorSampler, Rdn::SamplerAddressMode::MirroredRepeat },
	};
	for (const auto& [sampler, addressMode] : samplers)
	{
		*sampler = std::make_unique<Rdn::Sampler>();
		m_Allocator->CreateSampler(sampler->get(), { .MinFilter = Rdn::Filter::Linear, .MagFilter = Rdn::Filter::Linear, .AddressMode = addressMode });
	}

	m_Materials.push_back(Material{});
	const uint32_t white = 0xFFFFFFFF;
	AddTexture(1, 1, { &white, 1 });

	m_TopLevelAS = std::make_unique<Rdn::TopLevelAS>();
	m_Allocator->CreateTopLevelAS(m_TopLevelAS.get());
}

std::optional<SceneImport> Scene::Import(const std::filesystem::path& path, const glm::mat4& transform)
{
	const std::string extension = ToLower(path.extension().string());
	if (extension == ".gltf" || extension == ".glb")
		return LoadGltf(*this, path, transform);

	if (extension == ".obj")
	{
		const ModelId model = LoadModel(path);
		if (!model.IsValid())
			return std::nullopt;
		AddInstance(model, transform);

		SceneImport result;
		glm::vec3 min, max;
		GetModelBounds(model, min, max);
		result.AddBounds(min, max, transform);
		return result;
	}

	RDN_LOG_ERROR("Import: {} has an unsupported format (expected .gltf, .glb or .obj)", path.generic_string());
	return std::nullopt;
}

ModelId Scene::LoadModel(const std::filesystem::path& path)
{
	const Rdn::Timer timer;
	std::optional<ObjModel> obj = LoadObj(path);
	if (!obj)
		return {};

	std::vector<MaterialId> materials;
	for (const ObjMaterial& source : obj->Materials)
	{
		Material material = source.Material;
		if (!source.DiffuseTexture.empty())
			material.BaseColorTexture = LoadTexture(source.DiffuseTexture);
		materials.push_back(AddMaterial(material));
	}

	std::vector<ModelPart> parts;
	for (const ObjPart& part : obj->Parts)
	{
		const bool hasMaterial = part.MaterialIndex >= 0 && size_t(part.MaterialIndex) < materials.size();
		parts.push_back({ part.FirstIndex, part.IndexCount, hasMaterial ? materials[part.MaterialIndex] : MaterialId{} });
	}

	const ModelId model = AddModel(obj->Mesh, parts);
	if (model.IsValid())
	{
		RDN_LOG("Loaded {}: {} vertices, {} triangles, {} parts, {} materials in {:.0f} ms", path.generic_string(),
			obj->Mesh.Vertices.size(), obj->Mesh.Indices.size() / 3, parts.size(), materials.size(), timer.ElapsedMilliseconds());
	}
	return model;
}

ModelId Scene::AddModel(const MeshData& mesh, std::span<const ModelPart> parts)
{
	const ModelPart wholeMesh{ 0, uint32_t(mesh.Indices.size()), {} };
	if (parts.empty())
		parts = { &wholeMesh, 1 };

	if (mesh.Vertices.empty() || mesh.Indices.empty() || mesh.Indices.size() % 3 != 0)
	{
		RDN_LOG_ERROR("AddModel: needs vertices and whole triangles ({} vertices, {} indices)", mesh.Vertices.size(), mesh.Indices.size());
		return {};
	}
	for (const ModelPart& part : parts)
	{
		if (part.FirstIndex % 3 != 0 || part.IndexCount % 3 != 0 || uint64_t(part.FirstIndex) + part.IndexCount > mesh.Indices.size())
		{
			RDN_LOG_ERROR("AddModel: part [{}, +{}) is not a triangle range inside {} indices", part.FirstIndex, part.IndexCount, mesh.Indices.size());
			return {};
		}
	}
	if (const auto largest = std::ranges::max_element(mesh.Indices); *largest >= mesh.Vertices.size())
	{
		RDN_LOG_ERROR("AddModel: index {} is out of range for {} vertices", *largest, mesh.Vertices.size());
		return {};
	}

	auto model = std::make_unique<GpuModel>();
	const uint64_t vertexBytes = mesh.Vertices.size() * sizeof(Vertex);
	const uint64_t indexBytes = mesh.Indices.size() * sizeof(uint32_t);
	m_Allocator->CreateGpuBuffer(&model->Vertices, { .Size = vertexBytes, .UsageFlags = GeometryUsage, .MemoryProperty = Rdn::MemoryProperty::DeviceLocal });
	m_Allocator->CreateGpuBuffer(&model->Indices, { .Size = indexBytes, .UsageFlags = GeometryUsage, .MemoryProperty = Rdn::MemoryProperty::DeviceLocal });
	m_Allocator->SetDeviceLocalBufferData(&model->Vertices, mesh.Vertices.data(), vertexBytes);
	m_Allocator->SetDeviceLocalBufferData(&model->Indices, mesh.Indices.data(), indexBytes);

	Rdn::BottomLevelASDesc blas{
		.VertexBuffer = model->Vertices.GetHandle(),
		.IndexBuffer = model->Indices.GetHandle(),
		.VertexStride = sizeof(Vertex),
		.VertexCount = uint32_t(mesh.Vertices.size()),
		.VertexFormat = Rdn::Format::R32G32B32_Sfloat,
	};
	for (const ModelPart& part : parts)
		blas.Geometries.push_back({ part.FirstIndex, part.IndexCount / 3, IsOpaque(part.Material) });
	m_Allocator->CreateBottomLevelAS(&model->BottomLevelAS, blas);

	model->VertexAddress = m_Allocator->GetBufferDeviceAddress(&model->Vertices);
	model->IndexAddress = m_Allocator->GetBufferDeviceAddress(&model->Indices);
	model->Parts.assign(parts.begin(), parts.end());
	model->CpuPositions.reserve(mesh.Vertices.size());
	model->CpuUVs.reserve(mesh.Vertices.size());
	model->BoundsMin = mesh.Vertices[0].Position;
	model->BoundsMax = mesh.Vertices[0].Position;
	for (const Vertex& vertex : mesh.Vertices)
	{
		model->CpuPositions.push_back(vertex.Position);
		model->CpuUVs.push_back(vertex.UV);
		model->BoundsMin = glm::min(model->BoundsMin, vertex.Position);
		model->BoundsMax = glm::max(model->BoundsMax, vertex.Position);
	}
	model->CpuIndices = mesh.Indices;
	m_Models.push_back(std::move(model));
	return { uint32_t(m_Models.size() - 1) };
}

TextureId Scene::LoadTexture(const std::filesystem::path& path, bool srgb)
{
	const TextureSource source{ path, {}, srgb };
	return LoadTextures({ &source, 1 })[0];
}

std::vector<TextureId> Scene::LoadTextures(std::span<const TextureSource> sources)
{
	const Rdn::Timer timer;
	std::vector<TextureId> result(sources.size());
	std::vector<size_t> pending;
	std::vector<std::pair<size_t, size_t>> duplicates;
	std::unordered_map<std::string, size_t> firstRequest;
	for (size_t i = 0; i < sources.size(); i++)
	{
		if (!sources[i].Data.empty())
		{
			pending.push_back(i);
			continue;
		}
		const std::string key = TextureCacheKey(sources[i]);
		if (const auto cached = m_TextureCache.find(key); cached != m_TextureCache.end())
		{
			result[i] = cached->second;
			continue;
		}
		const auto [first, inserted] = firstRequest.try_emplace(key, i);
		if (inserted)
			pending.push_back(i);
		else
			duplicates.emplace_back(i, first->second);
	}

	struct Decoded
	{
		std::optional<TextureData> Texture;
		std::string Error;
	};
	auto decode = [sources](size_t index) {
		const TextureSource& source = sources[index];
		Decoded decoded;
		std::vector<uint8_t> file;
		std::span<const uint8_t> bytes = source.Data;
		if (bytes.empty())
		{
			if (!ReadBinaryFile(source.Path, file, decoded.Error))
				return decoded;
			bytes = file;
		}
		decoded.Texture = DecodeTexture(bytes, source.Srgb, decoded.Error);
		return decoded;
	};

	const size_t window = std::max(2u, 2 * std::thread::hardware_concurrency());
	std::deque<std::pair<size_t, std::future<Decoded>>> inFlight;
	size_t launched = 0;
	auto launch = [&] {
		const size_t index = pending[launched++];
		inFlight.emplace_back(index, std::async(std::launch::async, decode, index));
	};
	while (launched < pending.size() && inFlight.size() < window)
		launch();

	uint32_t loaded = 0;
	uint64_t texelBytes = 0;
	while (!inFlight.empty())
	{
		auto [index, future] = std::move(inFlight.front());
		inFlight.pop_front();
		if (launched < pending.size())
			launch();

		const TextureSource& source = sources[index];
		const std::string name = source.Data.empty() ? source.Path.generic_string() : std::format("an embedded image of {} bytes", source.Data.size());
		const Decoded decoded = future.get();
		TextureId texture = {};
		if (!decoded.Texture)
		{
			RDN_LOG_ERROR("Failed to load texture {}: {}", name, decoded.Error);
		}
		else if (texture = AddTexture(*decoded.Texture, name, source.AddressMode); texture.IsValid())
		{
			loaded++;
			texelBytes += decoded.Texture->Pixels.size();
		}
		result[index] = texture;
		if (source.Data.empty())
			m_TextureCache.emplace(TextureCacheKey(source), texture);
	}
	for (const auto& [index, first] : duplicates)
		result[index] = result[first];

	if (pending.size() > 1)
		RDN_LOG("Loaded {} textures ({:.0f} MiB of texel data) in {:.0f} ms", loaded, texelBytes / (1024.0 * 1024.0), timer.ElapsedMilliseconds());
	return result;
}

TextureId Scene::AddTexture(uint32_t width, uint32_t height, std::span<const uint32_t> rgba8, bool srgb)
{
	if (width == 0 || height == 0 || rgba8.size() != size_t(width) * height)
	{
		RDN_LOG_ERROR("AddTexture: {}x{} needs {} pixels, got {}", width, height, size_t(width) * height, rgba8.size());
		return {};
	}

	TextureData texture;
	texture.Width = width;
	texture.Height = height;
	texture.MipLevels = FullMipCount(width, height);
	texture.Format = srgb ? Rdn::Format::R8G8B8A8_Srgb : Rdn::Format::R8G8B8A8_Unorm;
	texture.Pixels.resize(rgba8.size_bytes());
	std::memcpy(texture.Pixels.data(), rgba8.data(), rgba8.size_bytes());
	return AddTexture(texture, std::format("a {}x{} texture", width, height), Rdn::SamplerAddressMode::Repeat);
}

TextureId Scene::AddTexture(const TextureData& texture, const std::string& name, Rdn::SamplerAddressMode addressMode)
{
	if (m_Textures.size() >= Rdn::ResourceAllocator::MaxBindlessDescriptors)
	{
		RDN_LOG_ERROR("Texture {} is skipped, the scene already has {} textures", name, m_Textures.size());
		return {};
	}
	if (IsBlockCompressed(texture.Format) && !m_Device->GetProperties().TextureCompressionBC)
	{
		RDN_LOG_ERROR("Texture {} is block compressed, but this GPU doesn't support BC textures", name);
		return {};
	}

	auto image = std::make_unique<Rdn::GpuImage>();
	m_Allocator->CreateGpuImage(image.get(), Rdn::GpuImageDesc{
		.ImageSize = { texture.Width, texture.Height, 1 },
		.Format = texture.Format,
		.UsageFlags = Rdn::ImageUsage::TransferDst | Rdn::ImageUsage::Sampled,
		.AspectFlags = Rdn::ImageAspect::Color,
		.MipLevels = texture.MipLevels,
	});
	m_Allocator->SetImageData(image.get(), texture.Pixels.data(), texture.Pixels.size(), Rdn::ImageLayout::ShaderReadOnlyOptimal);
	m_Textures.push_back(std::move(image));
	m_TextureAddressModes.push_back(addressMode);
	return { uint32_t(m_Textures.size() - 1) };
}

std::unordered_map<uint32_t, glm::vec3> Scene::AverageTextureColors(const std::vector<uint32_t>& textures)
{
	std::unordered_map<uint32_t, glm::vec3> averages;
	if (textures.empty())
		return averages;

	Rdn::GpuBuffer readback;
	m_Allocator->CreateGpuBuffer(&readback, { .Size = textures.size() * sizeof(glm::vec4), .UsageFlags = Rdn::BufferUsage::TransferDst,
		.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCached, .Mapped = true });
	std::vector<std::unique_ptr<Rdn::GpuImage>> targets(textures.size());
	for (std::unique_ptr<Rdn::GpuImage>& target : targets)
	{
		target = std::make_unique<Rdn::GpuImage>();
		m_Allocator->CreateGpuImage(target.get(), Rdn::GpuImageDesc{
			.ImageSize = { 1, 1, 1 },
			.Format = Rdn::Format::R32G32B32A32_Sfloat,
			.UsageFlags = Rdn::ImageUsage::TransferDst | Rdn::ImageUsage::TransferSrc,
			.AspectFlags = Rdn::ImageAspect::Color,
		});
	}

	m_Device->ImmediateSubmit([&](Rdn::CommandBuffer& cmd) {
		for (size_t i = 0; i < textures.size(); i++)
		{
			const Rdn::GpuImage& source = *m_Textures[textures[i]];
			const Rdn::ImageHandle target = targets[i]->GetHandle();
			const uint32_t level = source.GetMipLevels() - 1;
			const Rdn::Extent3D size = source.GetImageSize();
			const Rdn::Extent3D levelSize{ std::max(1u, size.Width >> level), std::max(1u, size.Height >> level), 1 };

			cmd.Barrier(Rdn::ImageBarrier{
				.Handle = source.GetHandle(), .OldLayout = Rdn::ImageLayout::ShaderReadOnlyOptimal, .NewLayout = Rdn::ImageLayout::TransferSrcOptimal,
				.SrcStage = Rdn::PipelineStage::AllCommands, .DstStage = Rdn::PipelineStage::AllTransfer,
				.SrcAccess = Rdn::AccessMask::None, .DstAccess = Rdn::AccessMask::TransferRead, .BaseMip = level, .MipCount = 1 });
			cmd.Barrier(Rdn::ImageBarrier{
				.Handle = target, .OldLayout = Rdn::ImageLayout::Undefined, .NewLayout = Rdn::ImageLayout::TransferDstOptimal,
				.SrcStage = Rdn::PipelineStage::None, .DstStage = Rdn::PipelineStage::AllTransfer,
				.SrcAccess = Rdn::AccessMask::None, .DstAccess = Rdn::AccessMask::TransferWrite });
			cmd.BlitImage(source.GetHandle(), levelSize, target, { 1, 1, 1 }, Rdn::Filter::Linear, level, 0);
			cmd.Barrier(Rdn::ImageBarrier{
				.Handle = target, .OldLayout = Rdn::ImageLayout::TransferDstOptimal, .NewLayout = Rdn::ImageLayout::TransferSrcOptimal,
				.SrcStage = Rdn::PipelineStage::AllTransfer, .DstStage = Rdn::PipelineStage::AllTransfer,
				.SrcAccess = Rdn::AccessMask::TransferWrite, .DstAccess = Rdn::AccessMask::TransferRead });
			cmd.CopyImageToBuffer(target, readback.GetHandle(), { 1, 1, 1 }, 0, i * sizeof(glm::vec4));
			cmd.Barrier(Rdn::ImageBarrier{
				.Handle = source.GetHandle(), .OldLayout = Rdn::ImageLayout::TransferSrcOptimal, .NewLayout = Rdn::ImageLayout::ShaderReadOnlyOptimal,
				.SrcStage = Rdn::PipelineStage::AllTransfer, .DstStage = Rdn::PipelineStage::AllCommands,
				.SrcAccess = Rdn::AccessMask::None, .DstAccess = Rdn::AccessMask::MemoryRead, .BaseMip = level, .MipCount = 1 });
		}
		cmd.Barrier(Rdn::GlobalBarrier{
			.SrcStage = Rdn::PipelineStage::AllTransfer, .DstStage = Rdn::PipelineStage::Host,
			.SrcAccess = Rdn::AccessMask::TransferWrite, .DstAccess = Rdn::AccessMask::HostRead });
	});

	const glm::vec4* colors = static_cast<const glm::vec4*>(readback.GetMappedPtr());
	for (size_t i = 0; i < textures.size(); i++)
	{
		const glm::vec3 color = glm::vec3(colors[i]);
		const bool finite = !glm::any(glm::isnan(color)) && !glm::any(glm::isinf(color));
		averages[textures[i]] = finite ? glm::max(color, glm::vec3(0.0f)) : glm::vec3(1.0f);
	}
	m_Allocator->ReleaseResource(&readback);
	for (std::unique_ptr<Rdn::GpuImage>& target : targets)
		m_Allocator->ReleaseResource(target.get());
	return averages;
}

const Rdn::Sampler& Scene::GetSampler(Rdn::SamplerAddressMode addressMode) const
{
	switch (addressMode)
	{
	case Rdn::SamplerAddressMode::ClampToEdge: return *m_ClampSampler;
	case Rdn::SamplerAddressMode::MirroredRepeat: return *m_MirrorSampler;
	default: return *m_RepeatSampler;
	}
}

MaterialId Scene::AddMaterial(const Material& material)
{
	m_Materials.push_back(material);
	return { uint32_t(m_Materials.size() - 1) };
}

MediumId Scene::AddMedium(const Medium& medium)
{
	m_Media.push_back(medium);
	return { uint32_t(m_Media.size() - 1) };
}

void Scene::SetGlobalMedium(MediumId medium)
{
	m_GlobalMedium = medium;
}

bool Scene::SetEnvironment(const std::filesystem::path& path)
{
	const Rdn::Timer timer;
	std::vector<uint8_t> file;
	std::string error;
	std::optional<HdrImage> image;
	if (ReadBinaryFile(path, file, error))
		image = DecodeHdrImage(file, error);
	if (!image)
	{
		RDN_LOG_ERROR("Failed to load environment {}: {}", path.generic_string(), error);
		return false;
	}

	size_t brightest = 0;
	float peak = 0.0f;
	for (size_t i = 0; i < image->Pixels.size(); i++)
	{
		float& value = image->Pixels[i];
		value = std::isfinite(value) ? std::max(value, 0.0f) : 0.0f;
		if (i % 4 == 3)
		{
			const float luminance = Luminance({ image->Pixels[i - 3], image->Pixels[i - 2], image->Pixels[i - 1] });
			if (luminance > peak)
			{
				peak = luminance;
				brightest = i / 4;
			}
		}
	}
	CreateEnvironment(image->Width, image->Height, image->Pixels);

	const glm::vec3 peakDirection = EquirectDirection((brightest % image->Width + 0.5f) / image->Width, (brightest / image->Width + 0.5f) / image->Height);
	RDN_LOG("Loaded environment {} ({}x{}) in {:.0f} ms, brightest texel {:.0f} at {:.1f} degrees elevation and {:.1f} degrees azimuth",
		path.generic_string(), image->Width, image->Height, timer.ElapsedMilliseconds(), peak,
		glm::degrees(std::asin(peakDirection.y)), glm::degrees(std::atan2(peakDirection.x, peakDirection.z)));
	return true;
}

void Scene::CreateEnvironment(uint32_t width, uint32_t height, std::span<const float> rgba)
{
	if (m_EnvironmentImage)
	{
		m_Device->WaitIdle();
		m_Allocator->ReleaseResource(m_EnvironmentImage.get());
	}
	m_EnvironmentImage = std::make_unique<Rdn::GpuImage>();
	m_Allocator->CreateGpuImage(m_EnvironmentImage.get(), Rdn::GpuImageDesc{
		.ImageSize = { width, height, 1 },
		.Format = Rdn::Format::R32G32B32A32_Sfloat,
		.UsageFlags = Rdn::ImageUsage::TransferDst | Rdn::ImageUsage::Sampled,
		.AspectFlags = Rdn::ImageAspect::Color,
	});
	m_Allocator->SetImageData(m_EnvironmentImage.get(), rgba.data(), rgba.size_bytes(), Rdn::ImageLayout::ShaderReadOnlyOptimal);

	m_EnvironmentWidth = width;
	m_EnvironmentHeight = height;
	const size_t count = size_t(width) * height;
	std::vector<double> weights(count);
	double total = 0.0;
	for (uint32_t y = 0; y < height; y++)
	{
		const double sinTheta = std::sin(glm::pi<double>() * (y + 0.5) / height);
		for (uint32_t x = 0; x < width; x++)
		{
			const size_t index = size_t(y) * width + x;
			const float* pixel = &rgba[index * 4];
			weights[index] = std::max(Luminance({ pixel[0], pixel[1], pixel[2] }), 0.0f) * sinTheta;
			total += weights[index];
		}
	}
	m_EnvironmentTotal = float(total);

	m_EnvironmentEntries.resize(count);
	BuildAliasTable(std::span<const double>(weights), total, m_EnvironmentEntries);
	for (size_t i = 0; i < count; i++)
		m_EnvironmentEntries[i].Density = total > 0.0 ? float(weights[i] * double(count) / total) : 1.0f;
}

bool Scene::IsOpaque(MaterialId material) const
{
	const size_t index = material.IsValid() && material.Index < m_Materials.size() ? material.Index : 0;
	return m_Materials[index].AlphaMode == AlphaMode::Opaque;
}

void Scene::AddInstance(ModelId model, const Transform& transform, MaterialId material)
{
	AddInstance(model, transform.ToMatrix(), material);
}

void Scene::AddInstance(ModelId model, const glm::mat4& transform, MaterialId material)
{
	if (!model.IsValid() || model.Index >= m_Models.size())
	{
		RDN_LOG_ERROR("AddInstance: invalid model (did LoadModel fail?)");
		return;
	}
	m_Instances.push_back({ model, transform, material });
}

void Scene::GetModelBounds(ModelId model, glm::vec3& min, glm::vec3& max) const
{
	min = glm::vec3(0.0f);
	max = glm::vec3(0.0f);
	if (model.IsValid() && model.Index < m_Models.size())
	{
		min = m_Models[model.Index]->BoundsMin;
		max = m_Models[model.Index]->BoundsMax;
	}
}

void Scene::Build()
{
	if (!m_EnvironmentImage)
	{
		const uint32_t width = DefaultEnvironmentWidth;
		const uint32_t height = width / 2;
		std::vector<float> pixels(size_t(width) * height * 4);
		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				const glm::vec3 color = DefaultSky(EquirectDirection((x + 0.5f) / width, (y + 0.5f) / height));
				std::memcpy(&pixels[(size_t(y) * width + x) * 4], &color, sizeof(color));
				pixels[(size_t(y) * width + x) * 4 + 3] = 1.0f;
			}
		}
		CreateEnvironment(width, height, pixels);
	}

	auto materialIndex = [&](MaterialId material) {
		return material.IsValid() && material.Index < m_Materials.size() ? material.Index : 0u;
	};
	auto mediumIndex = [&](MediumId medium) {
		return medium.IsValid() && medium.Index < m_Media.size() ? int32_t(medium.Index) : -1;
	};
	auto textureIndex = [&](TextureId texture) {
		return texture.IsValid() && texture.Index < m_Textures.size() ? texture.Index : 0u;
	};

	std::vector<uint32_t> emissiveTextures;
	for (const Material& material : m_Materials)
	{
		const uint32_t texture = textureIndex(material.EmissiveTexture);
		if (texture != 0 && Luminance(material.Emission) > 0.0f && std::ranges::find(emissiveTextures, texture) == emissiveTextures.end())
			emissiveTextures.push_back(texture);
	}
	const std::unordered_map<uint32_t, glm::vec3> emissiveAverages = AverageTextureColors(emissiveTextures);
	std::vector<float> emissionLuminance;
	emissionLuminance.reserve(m_Materials.size());
	for (const Material& material : m_Materials)
	{
		const auto average = emissiveAverages.find(textureIndex(material.EmissiveTexture));
		emissionLuminance.push_back(Luminance(material.Emission * (average != emissiveAverages.end() ? average->second : glm::vec3(1.0f))));
	}

	std::vector<GpuMaterial> materials;
	materials.reserve(m_Materials.size());
	for (const Material& material : m_Materials)
	{
		uint32_t flags = 0;
		if (material.NullSurface)
			flags |= MaterialNullSurface;
		if (material.AlphaMode == AlphaMode::Mask)
			flags |= MaterialAlphaMask;
		else if (material.AlphaMode == AlphaMode::Blend)
			flags |= MaterialAlphaBlend;
		if (material.SpecularGlossiness)
			flags |= MaterialSpecularGlossiness;
		if (material.DoubleSided)
			flags |= MaterialDoubleSided;
		if (material.ThinWalled)
			flags |= MaterialThinWalled;

		const float c = std::cos(material.UVRotation);
		const float s = std::sin(material.UVRotation);
		GpuMaterial& gpu = materials.emplace_back();
		gpu.BaseColor = material.BaseColor;
		gpu.Metallic = material.Metallic;
		gpu.Emission = material.Emission;
		gpu.Roughness = material.SpecularGlossiness ? material.Glossiness : material.Roughness;
		gpu.Sheen = material.Sheen;
		gpu.Transmission = material.Transmission;
		gpu.SpecularColor = material.SpecularColor;
		gpu.Alpha = material.Alpha;
		gpu.IOR = material.IOR;
		gpu.Clearcoat = material.Clearcoat;
		gpu.ClearcoatRoughness = material.ClearcoatRoughness;
		gpu.NormalScale = material.NormalScale;
		gpu.BaseColorTexture = textureIndex(material.BaseColorTexture);
		gpu.MetallicRoughnessTexture = textureIndex(material.MetallicRoughnessTexture);
		gpu.NormalTexture = textureIndex(material.NormalTexture);
		gpu.EmissiveTexture = textureIndex(material.EmissiveTexture);
		gpu.UVTransform = { c * material.UVScale.x, -s * material.UVScale.x, s * material.UVScale.y, c * material.UVScale.y };
		gpu.UVOffset = material.UVOffset;
		gpu.AlphaCutoff = material.AlphaCutoff;
		gpu.MediumIndex = mediumIndex(material.Medium);
		gpu.Flags = flags;
		gpu.EmissionLuminance = emissionLuminance[materials.size() - 1];
	}

	std::vector<GpuMeshPrimitive> primitives;
	std::vector<GpuLightTriangle> lights;
	std::vector<double> lightWeights;
	double totalPower = 0.0;
	m_TopLevelAS->ClearInstances();
	for (const Instance& instance : m_Instances)
	{
		const GpuModel& model = *m_Models[instance.Model.Index];
		if (primitives.size() + model.Parts.size() > MaxInstanceCustomIndex)
		{
			RDN_LOG_ERROR("Scene::Build: more than {} instanced parts, the rest is skipped", MaxInstanceCustomIndex);
			break;
		}

		const glm::mat4& m = instance.Transform;
		m_TopLevelAS->AddInstance(Rdn::TopLevelASInstance{
			.BottomLevelASAddress = model.BottomLevelAS.GetDeviceAddress(),
			.InstanceCustomIndex = uint32_t(primitives.size()),
			.TransformMatrix = {
				{ m[0][0], m[1][0], m[2][0], m[3][0] },
				{ m[0][1], m[1][1], m[2][1], m[3][1] },
				{ m[0][2], m[1][2], m[2][2], m[3][2] },
			},
			.ForceNoOpaque = instance.Material.IsValid() && !IsOpaque(instance.Material),
		});

		const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(m)));
		for (const ModelPart& part : model.Parts)
		{
			const uint32_t index = materialIndex(instance.Material.IsValid() ? instance.Material : part.Material);
			primitives.push_back({ model.VertexAddress, model.IndexAddress + uint64_t(part.FirstIndex) * sizeof(uint32_t), index });

			const Material& material = m_Materials[index];
			const float luminance = emissionLuminance[index];
			if (luminance <= 0.0f || material.NullSurface)
				continue;

			for (uint32_t i = part.FirstIndex; i < part.FirstIndex + part.IndexCount; i += 3)
			{
				const uint32_t i0 = model.CpuIndices[i], i1 = model.CpuIndices[i + 1], i2 = model.CpuIndices[i + 2];
				const glm::vec3& a = model.CpuPositions[i0];
				const glm::vec3& b = model.CpuPositions[i1];
				const glm::vec3& c = model.CpuPositions[i2];
				glm::vec3 p0 = glm::vec3(m * glm::vec4(a, 1.0f));
				glm::vec3 p1 = glm::vec3(m * glm::vec4(b, 1.0f));
				glm::vec3 p2 = glm::vec3(m * glm::vec4(c, 1.0f));
				glm::vec2 uv1 = model.CpuUVs[i1];
				glm::vec2 uv2 = model.CpuUVs[i2];
				const glm::vec3 worldCross = glm::cross(p1 - p0, p2 - p0);
				const float area = 0.5f * glm::length(worldCross);
				if (!(area > 0.0f))
					continue;
				if (glm::dot(worldCross, normalMatrix * glm::cross(b - a, c - a)) < 0.0f)
				{
					std::swap(p1, p2);
					std::swap(uv1, uv2);
				}

				totalPower += double(luminance) * area;
				lightWeights.push_back(double(luminance) * area);
				lights.push_back({ p0, 1.0f, p1, index, p2, 0u, model.CpuUVs[i0], uv1, uv2, glm::vec2(0.0f) });
			}
		}
	}
	BuildAliasTable(std::span<const double>(lightWeights), totalPower, lights);

	std::vector<GpuMedium> media;
	for (const Medium& medium : m_Media)
	{
		media.push_back({ glm::max(medium.Absorption, glm::vec3(0.0f)), std::clamp(medium.Anisotropy, -0.99f, 0.99f),
			glm::max(medium.Scattering, glm::vec3(0.0f)), 0.0f });
	}

	if (m_MaterialBuffer)
	{
		m_Device->WaitIdle();
		for (auto* buffer : { m_MaterialBuffer.get(), m_PrimitiveBuffer.get(), m_LightBuffer.get(), m_MediumBuffer.get(), m_EnvironmentBuffer.get() })
			m_Allocator->ReleaseResource(buffer);
	}
	auto upload = [&](std::unique_ptr<Rdn::GpuBuffer>& buffer, const void* data, uint64_t bytes) {
		std::vector<uint8_t> padded(std::max<uint64_t>(bytes, 64), 0);
		if (bytes > 0)
			std::memcpy(padded.data(), data, bytes);
		buffer = std::make_unique<Rdn::GpuBuffer>();
		m_Allocator->CreateGpuBuffer(buffer.get(), { .Size = padded.size(),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::TransferDst, .MemoryProperty = Rdn::MemoryProperty::DeviceLocal });
		m_Allocator->SetDeviceLocalBufferData(buffer.get(), padded.data(), padded.size());
	};
	const float rotation = glm::radians(m_EnvironmentRotation);
	const GpuEnvironmentHeader environmentHeader{ m_EnvironmentWidth, m_EnvironmentHeight, m_EnvironmentTotal, std::max(m_EnvironmentIntensity, 0.0f),
		std::cos(rotation), std::sin(rotation), { 0, 0 } };
	const std::vector<uint8_t> lightData = PackBuffer(GpuLightHeader{ uint32_t(lights.size()), float(totalPower), { 0, 0 } }, lights);
	const std::vector<uint8_t> mediumData = PackBuffer(GpuMediumHeader{ mediumIndex(m_GlobalMedium), uint32_t(media.size()), { 0, 0 } }, media);
	const std::vector<uint8_t> environmentData = PackBuffer(environmentHeader, m_EnvironmentEntries);
	upload(m_MaterialBuffer, materials.data(), materials.size() * sizeof(GpuMaterial));
	upload(m_PrimitiveBuffer, primitives.data(), primitives.size() * sizeof(GpuMeshPrimitive));
	upload(m_LightBuffer, lightData.data(), lightData.size());
	upload(m_MediumBuffer, mediumData.data(), mediumData.size());
	upload(m_EnvironmentBuffer, environmentData.data(), environmentData.size());

	m_Allocator->BuildTopLevelAS(m_TopLevelAS.get());
	RDN_LOG("Scene: {} instances, {} instanced parts, {} models, {} materials, {} textures, {} media, {} emissive triangles, {}x{} environment",
		m_TopLevelAS->GetInstanceCount(), primitives.size(), m_Models.size(), m_Materials.size(), m_Textures.size(), m_Media.size(), lights.size(),
		m_EnvironmentWidth, m_EnvironmentHeight);
}

Rdn::DescriptorWrite Scene::GetDescriptorWrite() const
{
	Rdn::DescriptorWrite write;
	if (!m_MaterialBuffer)
	{
		RDN_LOG_ERROR("Scene::GetDescriptorWrite: call Build() first");
		return write;
	}

	write.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_PrimitiveBuffer->GetHandle()));
	write.AddWrite(Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, m_MaterialBuffer->GetHandle()));
	write.AddWrite(Rdn::BufferWrite(2, Rdn::DescriptorType::StorageBuffer, m_LightBuffer->GetHandle()));
	write.AddWrite(Rdn::BufferWrite(3, Rdn::DescriptorType::StorageBuffer, m_MediumBuffer->GetHandle()));
	write.AddWrite(Rdn::BufferWrite(4, Rdn::DescriptorType::StorageBuffer, m_EnvironmentBuffer->GetHandle()));
	write.AddWrite(Rdn::ImageWrite(5, Rdn::DescriptorType::CombinedImageSampler, m_EnvironmentImage->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal,
		m_ClampSampler->GetHandle()));
	for (uint32_t i = 0; i < m_Textures.size(); i++)
	{
		write.AddWrite(Rdn::ImageWrite(6, Rdn::DescriptorType::CombinedImageSampler, m_Textures[i]->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal,
			GetSampler(m_TextureAddressModes[i]).GetHandle(), i));
	}
	return write;
}
