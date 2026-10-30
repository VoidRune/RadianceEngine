#pragma once
#include "Rendering/GBuffer.h"
#include "Rendering/Particles.h"
#include "Rendering/PostProcess.h"
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>

namespace Game
{
	enum class DebugView : uint8_t
	{
		Lit,
		Albedo,
		Normal,
		Roughness,
		Metallic,
		Occlusion,
		Emission,
		Sun,
		Ambient,
		Shadow,
		Irradiance,
		GiSource,
		Translucency,
		Count,
	};

	inline constexpr const char* DebugViewNames[] = { "lit", "albedo", "normal", "roughness", "metallic", "occlusion", "emission", "sun", "ambient", "shadow", "irradiance", "gisource", "translucency" };
	static_assert(std::size(DebugViewNames) == size_t(DebugView::Count));

	enum class GiQuality : uint8_t
	{
		Off,
		Low,
		Medium,
		High,
		Count,
	};

	struct GiSettings
	{
		uint32_t ProbeSpacing = 0;
		uint32_t Cascades = 0;
		bool Specular = false;
		float GuardBand = 0.0f;
		uint32_t Reconnect = 0;
		bool BackFaces = false;
		uint32_t TraceBudget = 0;
	};

	inline constexpr const char* GiQualityNames[] = { "off", "low", "medium", "high" };
	inline constexpr GiSettings GiPresets[] = { {}, { 8, 5, false, 0.0f, 0b00000, false, 3 }, { 4, 6, true, 0.05f, 0b01110, false, 3 }, { 2, 6, true, 0.05f, 0b11111, false, 4 } };
	static_assert(std::size(GiQualityNames) == size_t(GiQuality::Count) && std::size(GiPresets) == size_t(GiQuality::Count));
	inline constexpr Rdn::Format GiRadianceFormat = Rdn::Format::R16G16B16A16_Sfloat;
	inline constexpr Rdn::Format GiHeightFormat = Rdn::Format::R32G32_Sfloat;

	inline Rdn::Extent2D HalfExtent(Rdn::Extent2D extent)
	{
		return { std::max((extent.Width + 1) / 2, 1u), std::max((extent.Height + 1) / 2, 1u) };
	}

	enum class ShadowQuality : uint8_t
	{
		Off,
		Low,
		High,
		Count,
	};

	inline constexpr const char* ShadowQualityNames[] = { "off", "low", "high" };
	inline constexpr uint32_t ShadowResolution[] = { 1, 1024, 2048 };
	static_assert(std::size(ShadowQualityNames) == size_t(ShadowQuality::Count) && std::size(ShadowResolution) == size_t(ShadowQuality::Count));
	inline constexpr Rdn::Format ShadowFormat = Rdn::Format::D16_Unorm;
	inline constexpr float ShadowCasterHeight = 7.0f;

	struct ShadowView
	{
		glm::mat4 ViewProjection{ 1.0f };
		float TexelSize = 0.0f;
		float DepthRange = 1.0f;
	};

	ShadowView FitShadowView(const WorldCamera& camera, const glm::vec3& sunDirection, float groundHeight, uint32_t resolution, glm::vec2& extent);
	bool CompileVariant(const char* path, const char* define, Rdn::ShaderDesc& desc, Rdn::ShaderStage stage = Rdn::ShaderStage::Compute);

	inline bool IsRawView(DebugView view)
	{
		return (view >= DebugView::Albedo && view <= DebugView::Occlusion) || view == DebugView::Shadow;
	}

	inline constexpr uint32_t MaxSpellLights = 16;

	struct SpellLight
	{
		glm::vec3 Position{ 0.0f };
		float Radius = 3.0f;
		glm::vec3 Color{ 0.0f };
	};

	struct SceneLighting
	{
		glm::vec3 SunDirection{ 0.45f, -1.0f, 0.3f };
		glm::vec3 SunColor{ 1.0f, 0.97f, 0.92f };
		float SunIntensity = 1.95f;
		glm::vec3 SkyColor{ 0.31f, 0.34f, 0.38f };
		glm::vec3 GroundColor{ 0.16f, 0.14f, 0.12f };
		float EmissionGi = 2.5f;
		float ParticleGi = 12.0f;
		int32_t Environment = -1;
		float EnvironmentIntensity = 1.0f;
		float EnvironmentRotation = 0.0f;
		float Exposure = 1.0f;
		bool Floating = false;
		bool Daylight = false;
		bool Storm = false;
		glm::vec4 Eye{ 0.0f };
		float Time = 0.0f;
		float FocusHeight = 0.0f;
		float Leviathan = -1.0f;
		glm::vec3 Haze{ 0.07f, 0.055f, 0.13f };
		float StormBuild = 0.0f;
		ColorGrade Grade;
	};

	struct EnvironmentView
	{
		const Rdn::GpuImage* Image = nullptr;
		const Rdn::GpuImage* SkyMask = nullptr;
		const Rdn::Sampler* Sampler = nullptr;
		std::array<glm::vec4, 9> Irradiance{};
		glm::vec4 Params{ 0.0f };
		glm::vec4 SkyMaskTransform{ 0.0f };
	};

	class DeferredLighting
	{
	public:
		DeferredLighting(Rdn::Device& device, Rdn::ResourceAllocator& allocator);
		~DeferredLighting();
		DeferredLighting(const DeferredLighting&) = delete;
		DeferredLighting& operator=(const DeferredLighting&) = delete;

		void Render(Rdn::RenderGraph& graph, const GBuffer& gbuffer, Rdn::RGImage scene, Rdn::RGImage shadowMap, Rdn::RGImage giSource, Rdn::RGImage emitters, Rdn::Extent2D extent, uint32_t frameIndex,
			const WorldCamera& camera, const SceneLighting& lighting, const EnvironmentView& environment, const ShadowView& shadow, ShadowQuality quality, float fadeHeight, float edgeBand, const glm::vec3& background, DebugView view, std::span<const SpellLight> lights, bool translucency);
		bool IsReady() const { return m_Pipeline != nullptr; }
		bool HasGi() const { return m_GiPipeline != nullptr; }

	private:
		struct FrameGpu
		{
			glm::mat4 InverseViewProjection;
			glm::vec4 CameraPosition;
			glm::vec4 SunDirection;
			glm::vec4 SunRadiance;
			glm::vec4 SkyRadiance;
			glm::vec4 GroundRadiance;
			glm::vec4 Background;
			glm::uvec4 Options;
			glm::mat4 ShadowMatrix;
			glm::vec4 ShadowParams;
			glm::vec4 GiParams;
			std::array<glm::vec4, 9> EnvironmentSh;
			glm::vec4 EnvironmentParams;
			glm::vec4 SkyMaskTransform;
			glm::vec4 Backdrop;
			glm::vec4 Haze;
			glm::vec4 Eye;
			glm::vec4 Translucent;
			std::array<glm::vec4, MaxSpellLights * 2> Lights;
		};

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::unique_ptr<Rdn::Shader> m_Shader;
		std::unique_ptr<Rdn::ComputePipeline> m_Pipeline;
		std::unique_ptr<Rdn::Shader> m_GiShader;
		std::unique_ptr<Rdn::ComputePipeline> m_GiPipeline;
		std::unique_ptr<Rdn::GpuRingBuffer> m_Frame;
		std::unique_ptr<Rdn::Sampler> m_Sampler;
		std::unique_ptr<Rdn::Sampler> m_LinearSampler;
	};
}
