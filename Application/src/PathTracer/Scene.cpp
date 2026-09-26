#include "Scene.h"
#include "ObjLoader.h"
#include "RadianceEngine/Core/Log.h"
#include "RadianceEngine/Core/Timer.h"
#include "stb/stb_image.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstring>

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
		float IOR;
		float Clearcoat;
		float ClearcoatRoughness;
		uint32_t TextureIndex;
		int32_t MediumIndex;
		uint32_t Flags;
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
		float Cdf;
		glm::vec3 P1;
		float Pad0;
		glm::vec3 P2;
		float Pad1;
		glm::vec3 Emission;
		float Pad2;
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

	static_assert(sizeof(Vertex) == 32);
	static_assert(sizeof(GpuMaterial) == 80);
	static_assert(sizeof(GpuMeshPrimitive) == 24);
	static_assert(sizeof(GpuMedium) == 32);
	static_assert(sizeof(GpuLightTriangle) == 64);
	static_assert(sizeof(GpuLightHeader) == 16 && sizeof(GpuMediumHeader) == 16);

	constexpr uint32_t MaterialNullSurface = 1;
	constexpr uint32_t MaxInstanceCustomIndex = (1u << 24) - 1;
	constexpr Rdn::BufferUsage GeometryUsage = Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::ShaderDeviceAddress
		| Rdn::BufferUsage::AccelerationStructureBuildInputReadOnly | Rdn::BufferUsage::TransferDst;

	float Luminance(const glm::vec3& color)
	{
		return glm::dot(color, glm::vec3(0.2126f, 0.7152f, 0.0722f));
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
}

glm::mat4 Transform::ToMatrix() const
{
	glm::mat4 matrix = glm::translate(glm::mat4(1.0f), Position);
	matrix = glm::rotate(matrix, glm::radians(Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	matrix = glm::rotate(matrix, glm::radians(Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	matrix = glm::rotate(matrix, glm::radians(Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	return glm::scale(matrix, Scale);
}

Scene::Scene(Rdn::Device* device, Rdn::ResourceAllocator* allocator)
	: m_Device(device)
	, m_Allocator(allocator)
{
	m_Materials.push_back(Material{});
	const uint32_t white = 0xFFFFFFFF;
	AddTexture(1, 1, { &white, 1 });

	m_TopLevelAS = std::make_unique<Rdn::TopLevelAS>();
	m_Allocator->CreateTopLevelAS(m_TopLevelAS.get());
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
			material.Texture = LoadTexture(source.DiffuseTexture);
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
		blas.Geometries.push_back({ part.FirstIndex, part.IndexCount / 3 });
	m_Allocator->CreateBottomLevelAS(&model->BottomLevelAS, blas);

	model->VertexAddress = m_Allocator->GetBufferDeviceAddress(&model->Vertices);
	model->IndexAddress = m_Allocator->GetBufferDeviceAddress(&model->Indices);
	model->Parts.assign(parts.begin(), parts.end());
	model->CpuPositions.reserve(mesh.Vertices.size());
	for (const Vertex& vertex : mesh.Vertices)
		model->CpuPositions.push_back(vertex.Position);
	model->CpuIndices = mesh.Indices;
	m_Models.push_back(std::move(model));
	return { uint32_t(m_Models.size() - 1) };
}

TextureId Scene::LoadTexture(const std::filesystem::path& path)
{
	const std::string key = path.lexically_normal().generic_string();
	if (const auto cached = m_TextureCache.find(key); cached != m_TextureCache.end())
		return cached->second;

	TextureId texture = {};
	int width = 0, height = 0, channels = 0;
	stbi_uc* pixels = stbi_load(key.c_str(), &width, &height, &channels, 4);
	if (!pixels)
	{
		RDN_LOG_ERROR("Failed to load texture {}: {}", key, stbi_failure_reason());
	}
	else
	{
		texture = AddTexture(uint32_t(width), uint32_t(height), { reinterpret_cast<const uint32_t*>(pixels), size_t(width) * size_t(height) });
	}
	stbi_image_free(pixels);

	m_TextureCache.emplace(key, texture);
	return texture;
}

TextureId Scene::AddTexture(uint32_t width, uint32_t height, std::span<const uint32_t> rgba8)
{
	if (width == 0 || height == 0 || rgba8.size() != size_t(width) * height)
	{
		RDN_LOG_ERROR("AddTexture: {}x{} needs {} pixels, got {}", width, height, size_t(width) * height, rgba8.size());
		return {};
	}
	if (m_Textures.size() >= Rdn::ResourceAllocator::MaxBindlessDescriptors)
	{
		RDN_LOG_ERROR("AddTexture: the scene already has {} textures", m_Textures.size());
		return {};
	}

	auto texture = std::make_unique<Rdn::GpuImage>();
	m_Allocator->CreateGpuImage(texture.get(), Rdn::GpuImageDesc{
		.ImageSize = { width, height, 1 },
		.Format = Rdn::Format::R8G8B8A8_Srgb,
		.UsageFlags = Rdn::ImageUsage::TransferDst | Rdn::ImageUsage::Sampled,
		.AspectFlags = Rdn::ImageAspect::Color,
	});
	m_Allocator->SetImageData(texture.get(), rgba8.data(), width * height * 4, Rdn::ImageLayout::ShaderReadOnlyOptimal);
	m_Textures.push_back(std::move(texture));
	return { uint32_t(m_Textures.size() - 1) };
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

void Scene::AddInstance(ModelId model, const Transform& transform, MaterialId material)
{
	if (!model.IsValid() || model.Index >= m_Models.size())
	{
		RDN_LOG_ERROR("AddInstance: invalid model (did LoadModel fail?)");
		return;
	}
	m_Instances.push_back({ model, transform.ToMatrix(), material });
}

void Scene::Build()
{
	auto materialIndex = [&](MaterialId material) {
		return material.IsValid() && material.Index < m_Materials.size() ? material.Index : 0u;
	};
	auto mediumIndex = [&](MediumId medium) {
		return medium.IsValid() && medium.Index < m_Media.size() ? int32_t(medium.Index) : -1;
	};

	std::vector<GpuMaterial> materials;
	materials.reserve(m_Materials.size());
	for (const Material& material : m_Materials)
	{
		const uint32_t texture = material.Texture.IsValid() && material.Texture.Index < m_Textures.size() ? material.Texture.Index : 0u;
		materials.push_back({ material.BaseColor, material.Metallic, material.Emission, material.Roughness, material.Sheen, material.Transmission,
			material.IOR, material.Clearcoat, material.ClearcoatRoughness, texture, mediumIndex(material.Medium),
			material.NullSurface ? MaterialNullSurface : 0u, { 0, 0 } });
	}

	std::vector<GpuMeshPrimitive> primitives;
	std::vector<GpuLightTriangle> lights;
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
		});

		const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(m)));
		for (const ModelPart& part : model.Parts)
		{
			const uint32_t index = materialIndex(instance.Material.IsValid() ? instance.Material : part.Material);
			primitives.push_back({ model.VertexAddress, model.IndexAddress + uint64_t(part.FirstIndex) * sizeof(uint32_t), index });

			const Material& material = m_Materials[index];
			const float luminance = Luminance(material.Emission);
			if (luminance <= 0.0f || material.NullSurface)
				continue;

			for (uint32_t i = part.FirstIndex; i < part.FirstIndex + part.IndexCount; i += 3)
			{
				const glm::vec3& a = model.CpuPositions[model.CpuIndices[i]];
				const glm::vec3& b = model.CpuPositions[model.CpuIndices[i + 1]];
				const glm::vec3& c = model.CpuPositions[model.CpuIndices[i + 2]];
				glm::vec3 p0 = glm::vec3(m * glm::vec4(a, 1.0f));
				glm::vec3 p1 = glm::vec3(m * glm::vec4(b, 1.0f));
				glm::vec3 p2 = glm::vec3(m * glm::vec4(c, 1.0f));
				const glm::vec3 worldCross = glm::cross(p1 - p0, p2 - p0);
				const float area = 0.5f * glm::length(worldCross);
				if (area <= 0.0f)
					continue;
				if (glm::dot(worldCross, normalMatrix * glm::cross(b - a, c - a)) < 0.0f)
					std::swap(p1, p2);

				totalPower += double(luminance) * area;
				lights.push_back({ p0, float(totalPower), p1, 0.0f, p2, 0.0f, material.Emission, 0.0f });
			}
		}
	}
	for (GpuLightTriangle& light : lights)
		light.Cdf = float(light.Cdf / totalPower);
	if (!lights.empty())
		lights.back().Cdf = 1.0f;

	std::vector<GpuMedium> media;
	for (const Medium& medium : m_Media)
	{
		media.push_back({ glm::max(medium.Absorption, glm::vec3(0.0f)), std::clamp(medium.Anisotropy, -0.99f, 0.99f),
			glm::max(medium.Scattering, glm::vec3(0.0f)), 0.0f });
	}

	if (m_MaterialBuffer)
	{
		m_Device->WaitIdle();
		for (auto* buffer : { m_MaterialBuffer.get(), m_PrimitiveBuffer.get(), m_LightBuffer.get(), m_MediumBuffer.get() })
			m_Allocator->ReleaseResource(buffer);
	}
	auto upload = [&](std::unique_ptr<Rdn::GpuBuffer>& buffer, const void* data, uint64_t bytes) {
		buffer = std::make_unique<Rdn::GpuBuffer>();
		m_Allocator->CreateGpuBuffer(buffer.get(), { .Size = std::max<uint64_t>(bytes, 64),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::TransferDst, .MemoryProperty = Rdn::MemoryProperty::DeviceLocal });
		if (bytes > 0)
			m_Allocator->SetDeviceLocalBufferData(buffer.get(), data, bytes);
	};
	const std::vector<uint8_t> lightData = PackBuffer(GpuLightHeader{ uint32_t(lights.size()), float(totalPower), { 0, 0 } }, lights);
	const std::vector<uint8_t> mediumData = PackBuffer(GpuMediumHeader{ mediumIndex(m_GlobalMedium), uint32_t(media.size()), { 0, 0 } }, media);
	upload(m_MaterialBuffer, materials.data(), materials.size() * sizeof(GpuMaterial));
	upload(m_PrimitiveBuffer, primitives.data(), primitives.size() * sizeof(GpuMeshPrimitive));
	upload(m_LightBuffer, lightData.data(), lightData.size());
	upload(m_MediumBuffer, mediumData.data(), mediumData.size());

	m_Allocator->BuildTopLevelAS(m_TopLevelAS.get());
	RDN_LOG("Scene: {} instances, {} instanced parts, {} models, {} materials, {} textures, {} media, {} emissive triangles",
		m_TopLevelAS->GetInstanceCount(), primitives.size(), m_Models.size(), m_Materials.size(), m_Textures.size(), m_Media.size(), lights.size());
}

Rdn::DescriptorWrite Scene::GetDescriptorWrite(Rdn::SamplerHandle textureSampler) const
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
	for (uint32_t i = 0; i < m_Textures.size(); i++)
		write.AddWrite(Rdn::ImageWrite(4, Rdn::DescriptorType::CombinedImageSampler, m_Textures[i]->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, textureSampler, i));
	return write;
}
