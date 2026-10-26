#pragma once
#include "Rendering/Animation.h"
#include "Rendering/Lighting.h"
#include "Rendering/ModelLibrary.h"
#include "Rendering/Particles.h"
#include "Rendering/PostProcess.h"
#include "Rendering/RadianceCascades.h"
#include <GameShared/Content/Props.h>
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Game
{
	inline constexpr uint32_t NoPalette = UINT32_MAX;
	inline constexpr uint32_t MeshLayered = 4;
	inline constexpr uint32_t MeshWet = 8;
	inline constexpr uint32_t MeshVertexTint = 16;
	inline constexpr uint32_t MeshLavaGlow = 32;
	inline constexpr uint32_t MeshWater = 128;
	inline constexpr uint32_t MeshGround = 256;
	inline constexpr uint32_t MeshSplat = 512;
	inline constexpr uint32_t MeshVeins = 1024;
	inline constexpr uint32_t MeshAccentTrim = 2048;
	inline constexpr uint32_t MeshAccentDye = 4096;
	inline constexpr uint32_t MeshCrystal = 8192;
	inline constexpr uint32_t MeshIce = 16384;
	inline constexpr uint32_t MeshCliff = 32768;

	struct LevelMeshDesc
	{
		uint32_t FirstIndex = 0;
		uint32_t IndexCount = 0;
		uint32_t Material = 0;
		uint32_t Layer = 0;
		uint32_t Flags = 0;
	};

	struct BoxSurface
	{
		float Roughness = 0.8f;
		float Metallic = 0.0f;
	};

	struct BoxInstance
	{
		glm::vec4 CenterYaw;
		glm::vec4 HalfExtents;
		glm::vec4 ColorEmission;
		glm::vec4 Surface;
	};

	struct ModelInstance
	{
		glm::vec4 Rows[3];
		glm::vec4 Tint;
		glm::vec4 Shading;
		glm::uvec4 Info;
		glm::vec4 Accent{ 0.0f };
	};

	struct EffectMaterial
	{
		glm::vec3 Color{ 1.0f };
		float Roughness = 0.5f;
		float Metallic = 0.0f;
		glm::vec3 Emission{ 0.0f };
	};

	class WorldRenderer
	{
	public:
		WorldRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Format colorFormat);
		~WorldRenderer();
		WorldRenderer(const WorldRenderer&) = delete;
		WorldRenderer& operator=(const WorldRenderer&) = delete;

		bool LoadModels(std::span<const ModelPack> packs, std::string& error);
		const ModelLibrary& GetModels() const { return m_Models; }
		uint32_t AddEffectMesh(std::string_view name, const std::vector<ModelVertex>& vertices, const std::vector<uint32_t>& indices, const EffectMaterial& material, uint32_t flags = 0);
		std::optional<uint32_t> FindEffectMesh(std::string_view name) const;
		uint32_t AddLevelGeometry(const std::vector<ModelVertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<LevelMeshDesc>& meshes);
		void ClearLevelGeometry();
		void SetSkyMask(uint32_t width, uint32_t height, const std::vector<uint8_t>& openness, glm::vec2 origin, glm::vec2 size);
		std::optional<uint32_t> FindMaterialSlot(std::string_view name) const;
		uint32_t GetPlainMaterial() const { return m_PlainMaterial; }
		void SetFrozenLiquid(bool frozen) { m_FrozenLiquid = frozen; }
		std::optional<int32_t> FindEnvironment(std::string_view name) const;
		ParticleSystem& GetParticles() { return *m_Particles; }
		PostProcess& GetPost() { return *m_Post; }
		DebugView GetDebugView() const { return m_View; }
		void SetDebugView(DebugView view) { m_View = view; }
		ShadowQuality GetShadowQuality() const { return m_ShadowQuality; }
		void SetShadowQuality(ShadowQuality quality) { m_ShadowQuality = quality; }
		GiQuality GetGiQuality() const { return m_GiQuality; }
		void SetGiQuality(GiQuality quality) { m_GiQuality = quality; }
		float GetGuardBand() const { return HasGi() ? GiPresets[size_t(m_GiQuality)].GuardBand : 0.0f; }

		void Begin(const WorldCamera& camera, const SceneLighting& lighting, float focusHeight = 0.0f, bool fadeBelowFocus = true);
		void Draw(MeshKind mesh, const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec3& color, float yaw = 0.0f, float emission = 0.0f, float ground = 0.0f, const BoxSurface& surface = {});
		void DrawBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec3& color, float yaw = 0.0f, float emission = 0.0f, float ground = 0.0f, const BoxSurface& surface = {}) { Draw(MeshKind::Box, center, halfExtents, color, yaw, emission, ground, surface); }
		void DrawShadowCaster(MeshKind mesh, const glm::vec3& center, const glm::vec3& halfExtents, float yaw = 0.0f);
		void DrawLava(const glm::vec4& area, float height, float time);
		uint32_t AddPalette(const JointMatrices& skin, uint32_t joints);
		void DrawModel(uint32_t mesh, const glm::mat4& transform, uint32_t palette = NoPalette, const glm::vec4& tint = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), float emission = 0.0f, float ground = 0.0f, const glm::vec4& accent = glm::vec4(0.0f), uint32_t accentFlags = 0);
		ModelInstance MakeModelInstance(uint32_t mesh, const glm::mat4& transform, uint32_t palette = NoPalette, const glm::vec4& tint = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), float emission = 0.0f, float ground = 0.0f, const glm::vec4& accent = glm::vec4(0.0f), uint32_t accentFlags = 0) const;
		void SubmitModel(uint32_t mesh, const ModelInstance& instance, bool shadowOnly = false);
		void DrawSprite(const ParticleSprite& sprite) { m_Particles->DrawSprite(sprite); }
		void DrawDecal(const GroundDecal& decal) { m_Particles->DrawDecal(decal); }
		void Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D extent, uint32_t frameIndex, const std::array<float, 4>& clearColor);

		bool IsReady() const { return m_Pipeline != nullptr; }
		uint32_t GetInstanceCount() const { return m_LastCount; }

	private:
		struct MeshRange
		{
			uint32_t FirstVertex = 0;
			uint32_t VertexCount = 0;
		};

		struct DrawMesh
		{
			uint32_t FirstIndex = 0;
			uint32_t IndexCount = 0;
			uint32_t Material = 0;
			uint32_t Layer = 0;
			uint32_t Flags = 0;
			bool Skinned = false;
			bool Glows = false;
			bool AlphaTested = false;
		};

		struct MaterialGpu
		{
			uint32_t Albedo = 0;
			uint32_t Surface = 0;
			uint32_t Normal = 0;
			uint32_t Flags = 0;
			glm::vec4 Color{ 1.0f };
			glm::vec4 Params{ 1.0f, 1.0f, 1.0f, 0.0f };
			glm::vec4 Extra{ 0.0f };
		};

		struct EnvironmentGpu
		{
			std::unique_ptr<Rdn::GpuImage> Image;
			std::array<glm::vec4, 9> Irradiance{};
			uint32_t MipCount = 1;
		};

		struct PushConstants
		{
			glm::mat4 ViewProjection;
			glm::vec4 Focus;
			glm::vec4 Eye{ 0.0f };
		};

		static constexpr uint32_t BaseConstantsSize = uint32_t(sizeof(glm::mat4) + sizeof(glm::vec4));

		struct LavaConstants
		{
			glm::mat4 ViewProjection;
			glm::vec4 Params;
		};

		bool HasGi() const { return m_GiQuality != GiQuality::Off && m_Gi && m_Gi->IsReady() && m_Deferred && m_Deferred->HasGi(); }
		bool CreatePipeline();
		bool CreateModelPipeline();
		bool CreateLavaPipeline();
		void EnsureLavaTexture();
		void CreateShadowPipelines();
		void RenderShadows(Rdn::RenderGraph& graph, Rdn::RGImage shadowMap, const ShadowView& shadow, uint32_t boxCount, uint32_t modelCount, uint32_t paletteCount, uint32_t frameIndex);
		void RenderBackFaces(Rdn::RenderGraph& graph, Rdn::RGImage target, uint32_t boxCount, uint32_t modelCount, uint32_t paletteCount, uint32_t frameIndex);
		void CreateMeshes();
		void EnsureCapacity(uint32_t instances);
		void EnsureModelCapacity(uint32_t instances, uint32_t joints);
		void ReleaseModelData();
		void UploadGeometry(const std::vector<ModelVertex>& levelVertices, const std::vector<uint32_t>& levelIndices);
		void UploadMaterials();
		void BindTextures();
		EnvironmentView GetEnvironmentView() const;

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::unique_ptr<Rdn::Shader> m_VertexShader;
		std::unique_ptr<Rdn::Shader> m_FragmentShader;
		std::unique_ptr<Rdn::Pipeline> m_Pipeline;
		std::unique_ptr<Rdn::Pipeline> m_ShadowPipeline;
		std::unique_ptr<Rdn::Pipeline> m_ModelShadowPipeline;
		std::unique_ptr<Rdn::Pipeline> m_BackPipeline;
		std::unique_ptr<Rdn::Pipeline> m_ModelBackPipeline;
		std::unique_ptr<Rdn::GpuRingBuffer> m_InstanceBuffer;
		std::unique_ptr<Rdn::GpuBuffer> m_MeshBuffer;
		std::array<MeshRange, size_t(MeshKind::Count)> m_Meshes{};
		uint32_t m_Capacity = 0;
		uint32_t m_LastCount = 0;
		std::array<std::vector<BoxInstance>, size_t(MeshKind::Count)> m_Instances;
		std::array<std::vector<BoxInstance>, size_t(MeshKind::Count)> m_ShadowCasters;
		std::array<std::pair<uint32_t, uint32_t>, size_t(MeshKind::Count) * 2> m_BoxRanges{};
		std::vector<BoxInstance> m_Packed;
		PushConstants m_Constants{};
		std::unique_ptr<Rdn::Shader> m_LavaVertexShader;
		std::unique_ptr<Rdn::Shader> m_LavaFragmentShader;
		std::unique_ptr<Rdn::Pipeline> m_LavaPipeline;
		std::unique_ptr<Rdn::Shader> m_IcewaterFragmentShader;
		std::unique_ptr<Rdn::Pipeline> m_IcewaterPipeline;
		bool m_FrozenLiquid = false;
		std::unique_ptr<Rdn::GpuImage> m_LavaTexture;
		std::vector<BoxInstance> m_LavaQuads;
		std::pair<uint32_t, uint32_t> m_LavaRange{};

		ModelLibrary m_Models;
		std::unique_ptr<Rdn::Shader> m_ModelVertexShader;
		std::unique_ptr<Rdn::Shader> m_ModelFragmentShader;
		std::unique_ptr<Rdn::Shader> m_ModelShadowShader;
		std::unique_ptr<Rdn::Shader> m_ModelBackShader;
		std::unique_ptr<Rdn::Pipeline> m_ModelPipeline;
		std::unique_ptr<Rdn::Shader> m_CrystalFragmentShader;
		std::unique_ptr<Rdn::Pipeline> m_CrystalPipeline;
		std::unique_ptr<Rdn::Pipeline> m_ModelAlphaShadowPipeline;
		std::unique_ptr<Rdn::GpuBuffer> m_ModelVertices;
		std::unique_ptr<Rdn::GpuBuffer> m_ModelIndices;
		std::unique_ptr<Rdn::GpuBuffer> m_MaterialBuffer;
		std::vector<std::unique_ptr<Rdn::GpuImage>> m_Textures;
		std::vector<const Rdn::Sampler*> m_TextureSamplers;
		std::unique_ptr<Rdn::DescriptorSet> m_TextureSet;
		std::unique_ptr<Rdn::DescriptorSet> m_ShadowTextureSet;
		std::unique_ptr<Rdn::DescriptorSet> m_BackTextureSet;
		std::vector<DrawMesh> m_DrawMeshes;
		std::vector<MaterialGpu> m_Materials;
		std::vector<ModelVertex> m_EffectVertices;
		std::vector<uint32_t> m_EffectIndices;
		std::vector<std::pair<std::string, uint32_t>> m_EffectMeshes;
		std::vector<ModelVertex> m_LevelVertices;
		std::vector<uint32_t> m_LevelIndices;
		uint32_t m_LibraryMeshes = 0;
		uint32_t m_TilingMaterialBase = 0;
		uint32_t m_PlainMaterial = 0;
		std::vector<EnvironmentGpu> m_Environments;
		std::unique_ptr<Rdn::GpuImage> m_BlackEnvironment;
		std::unique_ptr<Rdn::GpuImage> m_SkyMask;
		glm::vec4 m_SkyMaskTransform{ 0.0f, 0.0f, 0.0f, 0.0f };
		std::unique_ptr<Rdn::Sampler> m_ModelSampler;
		std::unique_ptr<Rdn::Sampler> m_NearestSampler;
		std::unique_ptr<Rdn::Sampler> m_RepeatSampler;
		std::unique_ptr<Rdn::Sampler> m_EnvironmentSampler;
		std::unique_ptr<Rdn::GpuRingBuffer> m_ModelInstanceBuffer;
		std::unique_ptr<Rdn::GpuRingBuffer> m_PaletteBuffer;
		uint32_t m_ModelCapacity = 0;
		uint32_t m_PaletteCapacity = 0;
		std::vector<std::vector<ModelInstance>> m_ModelDraws;
		std::vector<std::vector<ModelInstance>> m_ModelShadowDraws;
		std::vector<ModelInstance> m_PackedModels;
		std::vector<glm::mat4> m_Palettes;
		WorldCamera m_Camera;
		SceneLighting m_Lighting;
		float m_FocusHeight = 0.0f;
		DebugView m_View = DebugView::Lit;
		ShadowQuality m_ShadowQuality = ShadowQuality::High;
		GiQuality m_GiQuality = GiQuality::Medium;
		glm::vec2 m_ShadowExtent{ 0.0f };
		std::vector<std::pair<uint32_t, uint32_t>> m_ModelRanges;
		std::vector<std::pair<uint32_t, uint32_t>> m_ModelShadowRanges;
		std::unique_ptr<ParticleSystem> m_Particles;
		std::unique_ptr<DeferredLighting> m_Deferred;
		std::unique_ptr<RadianceCascades> m_Gi;
		std::unique_ptr<PostProcess> m_Post;
	};
}
