#pragma once
#include "Rendering/Particles.h"
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace Game
{
	inline constexpr size_t SunFlareSites = 3;

	struct SunStar
	{
		glm::vec3 Center{ 0.0f };
		float Radius = 1.0f;
		float Spin = 0.0f;
		float Brightness = 1.0f;
		float Corona = 1.0f;
		float Activity = 0.0f;
		float Swell = 0.0f;
		glm::vec4 Beat{ 0.0f, 1.0f, 0.0f, -1.0f };
		std::array<glm::vec4, SunFlareSites> Flares{};
		uint32_t Seed = 0;
	};

	inline constexpr size_t SunWardImpacts = 2;

	struct SunWard
	{
		glm::vec3 Center{ 0.0f };
		float Radius = 2.0f;
		float StarRadius = 1.0f;
		float Glow = 0.02f;
		float Pulse = 0.0f;
		std::array<glm::vec4, SunWardImpacts> Impacts{ glm::vec4(0.0f, 1.0f, 0.0f, -1.0f), glm::vec4(0.0f, 1.0f, 0.0f, -1.0f) };
		std::array<float, SunWardImpacts> Strength{};
	};

	struct SunHaze
	{
		glm::vec3 Center{ 0.0f };
		float Radius = 1.0f;
		float Top = 0.0f;
		float DarkStart = 6.0f;
		float DarkEnd = 14.0f;
		float Darkness = 0.85f;
		glm::vec3 Color{ 1.0f, 0.55f, 0.2f };
		float Power = 1.0f;
		float Shafts = 0.6f;
		float Extent = 12.0f;
	};

	struct SunWave
	{
		glm::vec3 Origin{ 0.0f };
		glm::vec3 Outward{ 0.0f, 1.0f, 0.0f };
		float Radius = 1.0f;
		float Width = 0.2f;
		float Intensity = 1.0f;
		float Spread = 1.0f;
		uint32_t Seed = 0;
	};

	enum class SunRibbonLook : uint8_t
	{
		Plasma,
		Spark,
	};

	struct SunKnot
	{
		glm::vec3 Position{ 0.0f };
		float Width = 0.1f;
		float Heat = 0.5f;
		float Alpha = 1.0f;
	};

	struct SunRibbon
	{
		SunRibbonLook Look = SunRibbonLook::Plasma;
		float Intensity = 1.0f;
		float Opacity = 0.3f;
		float Flow = 1.0f;
		uint32_t Seed = 0;
	};

	class SunRenderer
	{
	public:
		static constexpr uint32_t MaxQuads = 32;
		static constexpr uint32_t MaxVertices = 8192;

		SunRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator);
		~SunRenderer();
		SunRenderer(const SunRenderer&) = delete;
		SunRenderer& operator=(const SunRenderer&) = delete;

		void BeginFrame(float time);
		void Draw(const SunStar& star);
		void Draw(const SunHaze& haze);
		void Draw(const SunWave& wave);
		void Draw(const SunWard& ward);
		void Draw(std::span<const SunKnot> knots, const SunRibbon& ribbon);
		void Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera);

		float Emission = 0.3f;
		bool Enabled = true;

	private:
		enum class QuadKind : uint32_t
		{
			Star,
			Haze,
			Wave,
			Ward,
		};

		struct QuadGpu
		{
			glm::vec4 CenterSize;
			glm::vec4 Axis;
			glm::vec4 A;
			glm::vec4 B;
			glm::vec4 C;
			glm::vec4 D;
			glm::vec4 E;
			glm::uvec4 Info;
		};

		struct VertexGpu
		{
			glm::vec4 PositionWidth;
			glm::vec4 Body;
			glm::vec4 Glow;
			glm::vec4 Tangent;
			glm::vec4 Extra;
		};

		struct Constants
		{
			glm::mat4 ViewProjection;
			glm::vec4 Eye;
			glm::vec4 Camera;
			glm::vec4 Screen;
			glm::vec4 Forward;
		};

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::vector<std::unique_ptr<Rdn::Shader>> m_Shaders;
		std::unique_ptr<Rdn::Pipeline> m_QuadPipeline;
		std::unique_ptr<Rdn::Pipeline> m_StripPipeline;
		std::unique_ptr<Rdn::GpuRingBuffer> m_QuadBuffer;
		std::unique_ptr<Rdn::GpuRingBuffer> m_VertexBuffer;
		std::unique_ptr<Rdn::Sampler> m_DepthSampler;
		std::vector<SunStar> m_Stars;
		std::vector<SunHaze> m_Hazes;
		std::vector<SunWave> m_Waves;
		std::vector<SunWard> m_Wards;
		std::vector<VertexGpu> m_Vertices;
		std::vector<QuadGpu> m_Quads;
		float m_Time = 0.0f;
	};
}
