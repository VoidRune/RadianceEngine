#pragma once
#include <RadianceEngine/Graphics/Device.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <RadianceEngine/Graphics/Resources/TopLevelAS.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

template<typename Tag>
struct SceneId
{
	uint32_t Index = UINT32_MAX;

	bool IsValid() const { return Index != UINT32_MAX; }
	bool operator==(const SceneId&) const = default;
};

using ModelId = SceneId<struct ModelTag>;
using MaterialId = SceneId<struct MaterialTag>;
using TextureId = SceneId<struct TextureTag>;
using MediumId = SceneId<struct MediumTag>;

struct Vertex
{
	glm::vec3 Position{ 0.0f };
	glm::vec3 Normal{ 0.0f };
	glm::vec2 UV{ 0.0f };
};

struct MeshData
{
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;
};

struct ModelPart
{
	uint32_t FirstIndex = 0;
	uint32_t IndexCount = 0;
	MaterialId Material = {};
};

struct Medium
{
	glm::vec3 Absorption{ 0.0f };
	glm::vec3 Scattering{ 0.0f };
	float Anisotropy = 0.0f;
};

struct Material
{
	glm::vec3 BaseColor{ 1.0f };
	glm::vec3 Emission{ 0.0f };
	float Metallic = 0.0f;
	float Roughness = 0.5f;
	float Transmission = 0.0f;
	float IOR = 1.5f;
	float Clearcoat = 0.0f;
	float ClearcoatRoughness = 0.03f;
	glm::vec3 Sheen{ 0.0f };
	TextureId Texture = {};
	MediumId Medium = {};
	bool NullSurface = false;
};

struct Transform
{
	glm::vec3 Position{ 0.0f };
	glm::vec3 Rotation{ 0.0f };
	glm::vec3 Scale{ 1.0f };

	glm::mat4 ToMatrix() const;
};

class Scene
{
public:
	Scene(Rdn::Device* device, Rdn::ResourceAllocator* allocator);
	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;

	ModelId LoadModel(const std::filesystem::path& path);
	ModelId AddModel(const MeshData& mesh, std::span<const ModelPart> parts = {});
	TextureId LoadTexture(const std::filesystem::path& path);
	TextureId AddTexture(uint32_t width, uint32_t height, std::span<const uint32_t> rgba8);
	MaterialId AddMaterial(const Material& material);
	MediumId AddMedium(const Medium& medium);
	void SetGlobalMedium(MediumId medium);
	void AddInstance(ModelId model, const Transform& transform = {}, MaterialId material = {});
	void Build();

	Rdn::AccelerationStructureHandle GetTopLevelAS() const { return m_TopLevelAS->GetHandle(); }
	Rdn::DescriptorWrite GetDescriptorWrite(Rdn::SamplerHandle textureSampler) const;

private:
	struct GpuModel
	{
		Rdn::GpuBuffer Vertices;
		Rdn::GpuBuffer Indices;
		Rdn::BottomLevelAS BottomLevelAS;
		uint64_t VertexAddress = 0;
		uint64_t IndexAddress = 0;
		std::vector<ModelPart> Parts;
		std::vector<glm::vec3> CpuPositions;
		std::vector<uint32_t> CpuIndices;
	};

	struct Instance
	{
		ModelId Model;
		glm::mat4 Transform;
		MaterialId Material;
	};

	Rdn::Device* m_Device;
	Rdn::ResourceAllocator* m_Allocator;

	std::vector<std::unique_ptr<GpuModel>> m_Models;
	std::vector<std::unique_ptr<Rdn::GpuImage>> m_Textures;
	std::unordered_map<std::string, TextureId> m_TextureCache;
	std::vector<Material> m_Materials;
	std::vector<Instance> m_Instances;
	std::vector<Medium> m_Media;
	MediumId m_GlobalMedium = {};

	std::unique_ptr<Rdn::GpuBuffer> m_MaterialBuffer;
	std::unique_ptr<Rdn::GpuBuffer> m_PrimitiveBuffer;
	std::unique_ptr<Rdn::GpuBuffer> m_LightBuffer;
	std::unique_ptr<Rdn::GpuBuffer> m_MediumBuffer;
	std::unique_ptr<Rdn::TopLevelAS> m_TopLevelAS;
};
