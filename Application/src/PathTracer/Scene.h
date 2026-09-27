#pragma once
#include <RadianceEngine/Graphics/Device.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <RadianceEngine/Graphics/Resources/TopLevelAS.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

struct TextureData;

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
	glm::vec4 Tangent{ 0.0f };
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

enum class AlphaMode : uint32_t
{
	Opaque,
	Mask,
	Blend,
};

struct Material
{
	glm::vec3 BaseColor{ 1.0f };
	float Alpha = 1.0f;
	glm::vec3 Emission{ 0.0f };
	float Metallic = 0.0f;
	float Roughness = 0.5f;
	float Transmission = 0.0f;
	float IOR = 1.5f;
	float Clearcoat = 0.0f;
	float ClearcoatRoughness = 0.03f;
	glm::vec3 Sheen{ 0.0f };
	TextureId BaseColorTexture = {};
	TextureId MetallicRoughnessTexture = {};
	TextureId NormalTexture = {};
	TextureId EmissiveTexture = {};
	float NormalScale = 1.0f;
	bool SpecularGlossiness = false;
	glm::vec3 SpecularColor{ 1.0f };
	float Glossiness = 1.0f;
	::AlphaMode AlphaMode = ::AlphaMode::Opaque;
	float AlphaCutoff = 0.5f;
	bool DoubleSided = false;
	bool ThinWalled = false;
	glm::vec2 UVScale{ 1.0f };
	glm::vec2 UVOffset{ 0.0f };
	float UVRotation = 0.0f;
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

struct SceneCamera
{
	std::string Name;
	glm::vec3 Position{ 0.0f };
	glm::vec3 Forward{ 0.0f, 0.0f, 1.0f };
	float VerticalFov = 50.0f;
};

struct SceneImport
{
	std::vector<SceneCamera> Cameras;
	glm::vec3 BoundsMin{ std::numeric_limits<float>::max() };
	glm::vec3 BoundsMax{ std::numeric_limits<float>::lowest() };

	void AddBounds(const glm::vec3& min, const glm::vec3& max, const glm::mat4& transform);
};

struct TextureSource
{
	std::filesystem::path Path;
	std::span<const uint8_t> Data;
	bool Srgb = true;
	Rdn::SamplerAddressMode AddressMode = Rdn::SamplerAddressMode::Repeat;
};

class Scene
{
public:
	Scene(Rdn::Device* device, Rdn::ResourceAllocator* allocator);
	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;

	std::optional<SceneImport> Import(const std::filesystem::path& path, const glm::mat4& transform = glm::mat4(1.0f));
	ModelId LoadModel(const std::filesystem::path& path);
	ModelId AddModel(const MeshData& mesh, std::span<const ModelPart> parts = {});
	TextureId LoadTexture(const std::filesystem::path& path, bool srgb = true);
	std::vector<TextureId> LoadTextures(std::span<const TextureSource> sources);
	TextureId AddTexture(uint32_t width, uint32_t height, std::span<const uint32_t> rgba8, bool srgb = true);
	MaterialId AddMaterial(const Material& material);
	MediumId AddMedium(const Medium& medium);
	void SetGlobalMedium(MediumId medium);
	bool SetEnvironment(const std::filesystem::path& path);
	void SetEnvironmentIntensity(float intensity) { m_EnvironmentIntensity = intensity; }
	void SetEnvironmentRotation(float degrees) { m_EnvironmentRotation = degrees; }
	void AddInstance(ModelId model, const Transform& transform = {}, MaterialId material = {});
	void AddInstance(ModelId model, const glm::mat4& transform, MaterialId material = {});
	void Build();

	uint32_t GetTextureCount() const { return uint32_t(m_Textures.size()); }
	void GetModelBounds(ModelId model, glm::vec3& min, glm::vec3& max) const;
	Rdn::AccelerationStructureHandle GetTopLevelAS() const { return m_TopLevelAS->GetHandle(); }
	Rdn::DescriptorWrite GetDescriptorWrite() const;

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
		std::vector<glm::vec2> CpuUVs;
		std::vector<uint32_t> CpuIndices;
		glm::vec3 BoundsMin{ 0.0f };
		glm::vec3 BoundsMax{ 0.0f };
	};

	struct Instance
	{
		ModelId Model;
		glm::mat4 Transform;
		MaterialId Material;
	};

	TextureId AddTexture(const TextureData& texture, const std::string& name, Rdn::SamplerAddressMode addressMode);
	std::unordered_map<uint32_t, glm::vec3> AverageTextureColors(const std::vector<uint32_t>& textures);
	void CreateEnvironment(uint32_t width, uint32_t height, std::span<const float> rgba);
	bool IsOpaque(MaterialId material) const;
	const Rdn::Sampler& GetSampler(Rdn::SamplerAddressMode addressMode) const;

	Rdn::Device* m_Device;
	Rdn::ResourceAllocator* m_Allocator;

	std::vector<std::unique_ptr<GpuModel>> m_Models;
	std::vector<std::unique_ptr<Rdn::GpuImage>> m_Textures;
	std::vector<Rdn::SamplerAddressMode> m_TextureAddressModes;
	std::unordered_map<std::string, TextureId> m_TextureCache;
	std::unique_ptr<Rdn::Sampler> m_RepeatSampler;
	std::unique_ptr<Rdn::Sampler> m_ClampSampler;
	std::unique_ptr<Rdn::Sampler> m_MirrorSampler;
	std::vector<Material> m_Materials;
	std::vector<Instance> m_Instances;
	std::vector<Medium> m_Media;
	MediumId m_GlobalMedium = {};

	struct EnvironmentEntry
	{
		float Probability;
		uint32_t Alias;
		float Density;
	};

	std::unique_ptr<Rdn::GpuImage> m_EnvironmentImage;
	std::vector<EnvironmentEntry> m_EnvironmentEntries;
	uint32_t m_EnvironmentWidth = 0;
	uint32_t m_EnvironmentHeight = 0;
	float m_EnvironmentTotal = 0.0f;
	float m_EnvironmentIntensity = 1.0f;
	float m_EnvironmentRotation = 0.0f;

	std::unique_ptr<Rdn::GpuBuffer> m_MaterialBuffer;
	std::unique_ptr<Rdn::GpuBuffer> m_PrimitiveBuffer;
	std::unique_ptr<Rdn::GpuBuffer> m_LightBuffer;
	std::unique_ptr<Rdn::GpuBuffer> m_MediumBuffer;
	std::unique_ptr<Rdn::GpuBuffer> m_EnvironmentBuffer;
	std::unique_ptr<Rdn::TopLevelAS> m_TopLevelAS;
};
