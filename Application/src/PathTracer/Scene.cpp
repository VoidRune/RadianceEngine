#include "Scene.h"
#include "ObjLoader.h"
#include "RadianceEngine/Core/Log.h"
#include "RadianceEngine/Core/Timer.h"
#include "stb/stb_image.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace
{
	struct GpuMaterial
	{
		glm::vec4 Color;
		glm::vec4 Emission;
		float Metallic;
		float Roughness;
		float Transmission;
		uint32_t TextureIndex;
	};

	struct GpuMeshPrimitive
	{
		uint64_t VertexAddress;
		uint64_t IndexAddress;
		uint32_t MaterialIndex;
	};

	static_assert(sizeof(Vertex) == 32);
	static_assert(sizeof(GpuMaterial) == 48);
	static_assert(sizeof(GpuMeshPrimitive) == 24);

	constexpr uint32_t MaxInstanceCustomIndex = (1u << 24) - 1;
	constexpr Rdn::BufferUsage GeometryUsage = Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::ShaderDeviceAddress
		| Rdn::BufferUsage::AccelerationStructureBuildInputReadOnly | Rdn::BufferUsage::TransferDst;
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
		.Format = Rdn::Format::R8G8B8A8_Unorm,
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

	std::vector<GpuMaterial> materials;
	materials.reserve(m_Materials.size());
	for (const Material& material : m_Materials)
	{
		const uint32_t texture = material.Texture.IsValid() && material.Texture.Index < m_Textures.size() ? material.Texture.Index : 0u;
		materials.push_back({ glm::vec4(material.Color, 1.0f), glm::vec4(material.Emission, 1.0f), material.Metallic, material.Roughness, material.Transmission, texture });
	}

	std::vector<GpuMeshPrimitive> primitives;
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
		for (const ModelPart& part : model.Parts)
		{
			const MaterialId material = instance.Material.IsValid() ? instance.Material : part.Material;
			primitives.push_back({ model.VertexAddress, model.IndexAddress + uint64_t(part.FirstIndex) * sizeof(uint32_t), materialIndex(material) });
		}
	}

	if (m_MaterialBuffer)
	{
		m_Device->WaitIdle();
		m_Allocator->ReleaseResource(m_MaterialBuffer.get());
		m_Allocator->ReleaseResource(m_PrimitiveBuffer.get());
	}
	auto upload = [&](std::unique_ptr<Rdn::GpuBuffer>& buffer, const void* data, uint64_t bytes) {
		buffer = std::make_unique<Rdn::GpuBuffer>();
		m_Allocator->CreateGpuBuffer(buffer.get(), { .Size = std::max<uint64_t>(bytes, 64),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::TransferDst, .MemoryProperty = Rdn::MemoryProperty::DeviceLocal });
		if (bytes > 0)
			m_Allocator->SetDeviceLocalBufferData(buffer.get(), data, bytes);
	};
	upload(m_MaterialBuffer, materials.data(), materials.size() * sizeof(GpuMaterial));
	upload(m_PrimitiveBuffer, primitives.data(), primitives.size() * sizeof(GpuMeshPrimitive));

	m_Allocator->BuildTopLevelAS(m_TopLevelAS.get());
	RDN_LOG("Scene: {} instances, {} instanced parts, {} models, {} materials, {} textures",
		m_TopLevelAS->GetInstanceCount(), primitives.size(), m_Models.size(), m_Materials.size(), m_Textures.size());
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
	for (uint32_t i = 0; i < m_Textures.size(); i++)
		write.AddWrite(Rdn::ImageWrite(2, Rdn::DescriptorType::CombinedImageSampler, m_Textures[i]->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, textureSampler, i));
	return write;
}
