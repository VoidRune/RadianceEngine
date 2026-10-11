#pragma once
#include "Rendering/GBuffer.h"
#include "Rendering/Lighting.h"
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Game
{
	class RadianceCascades
	{
	public:
		static constexpr uint32_t LevelCount = 6;
		static constexpr uint32_t BaseDirections = 4;

		RadianceCascades(Rdn::Device& device, Rdn::ResourceAllocator& allocator);
		~RadianceCascades();
		RadianceCascades(const RadianceCascades&) = delete;
		RadianceCascades& operator=(const RadianceCascades&) = delete;

		glm::ivec2 Update(Rdn::Extent2D extent, const WorldCamera& camera, float focusHeight);
		Rdn::RGImage CreateSource(Rdn::RenderGraph& graph, Rdn::Extent2D extent) const;
		void Render(Rdn::RenderGraph& graph, const GBuffer& gbuffer, Rdn::RGImage source, Rdn::RGImage particleLight, Rdn::RGImage backDepth, Rdn::RGImage scene, Rdn::Extent2D extent, uint32_t frameIndex,
			const WorldCamera& camera, const SceneLighting& lighting, const EnvironmentView& environment, GiQuality quality, float fadeHeight, float edgeBand, DebugView view);
		bool IsReady() const { return m_Ready; }

	private:
		struct Domain
		{
			glm::ivec2 Origin{ 0 };
			glm::uvec2 Size{ 0u };
			float Texel = 0.0f;
			float Height = 0.0f;
		};

		struct FrameGpu
		{
			glm::mat4 View;
			glm::mat4 InverseView;
			glm::mat4 DomainBasis;
			glm::mat4 DomainInverse;
			glm::vec4 Projection;
			glm::vec4 Screen;
			glm::vec4 Domain;
			glm::vec4 Sky;
			glm::vec4 Ground;
			glm::vec4 Trace;
			glm::vec4 Shading;
			glm::uvec4 Options;
			std::array<glm::vec4, 4> SolidAngles;
			std::array<glm::vec4, 3> LevelOffsets;
			std::array<glm::ivec4, 3> LevelOrigins;
			glm::vec4 Environment;
			glm::vec4 SkyMaskTransform;
			glm::vec4 Lattice;
			glm::vec4 PastLattice;
		};

		struct PlaceConstants
		{
			glm::vec4 Lattice;
			glm::uvec4 Grid;
		};

		struct DownsampleConstants
		{
			glm::ivec4 Parity;
			glm::ivec4 Origins;
			glm::ivec4 Sizes;
		};

		struct TraceConstants
		{
			glm::uvec4 Cascade;
			glm::uvec4 Grid;
			glm::vec4 Interval;
			glm::vec4 UpperTexel;
			glm::vec4 Offsets;
		};

		struct ApplyConstants
		{
			glm::uvec4 Grid;
			glm::vec4 Spacing;
			glm::vec4 Texel;
			glm::vec4 Offsets;
		};

		struct ResolveConstants
		{
			glm::vec4 Probes;
			glm::ivec4 History;
		};

		using ProbeSet = std::array<std::unique_ptr<Rdn::GpuImage>, 3>;

		std::unique_ptr<Rdn::ComputePipeline> CreatePipeline(const char* path, const char* define = nullptr);
		glm::vec2 AnchorPixel(const WorldCamera& camera, Rdn::Extent2D extent, float height, float period);
		bool UpdateDomain(Rdn::Extent2D extent, const WorldCamera& camera, float focusHeight);
		Rdn::Extent2D LevelExtent(uint32_t level) const;
		void EnsureHistory(glm::uvec2 grid, uint32_t spacing);
		void ReleaseHistory();

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::vector<std::unique_ptr<Rdn::Shader>> m_Shaders;
		std::unique_ptr<Rdn::ComputePipeline> m_Gather;
		std::unique_ptr<Rdn::ComputePipeline> m_Downsample;
		std::unique_ptr<Rdn::ComputePipeline> m_Place;
		std::unique_ptr<Rdn::ComputePipeline> m_Trace;
		std::unique_ptr<Rdn::ComputePipeline> m_TraceReconnect;
		std::unique_ptr<Rdn::ComputePipeline> m_Resolve;
		std::unique_ptr<Rdn::ComputePipeline> m_Apply;
		std::unique_ptr<Rdn::GpuRingBuffer> m_Frame;
		std::unique_ptr<Rdn::Sampler> m_Nearest;
		std::unique_ptr<Rdn::Sampler> m_Linear;
		std::array<glm::vec4, 4> m_SolidAngles{};
		glm::vec3 m_Anchor{ 0.0f };
		bool m_Anchored = false;
		Domain m_Domain;
		Rdn::Extent2D m_DomainExtent{ 0, 0 };
		bool m_DomainValid = false;
		bool m_DomainReset = true;
		std::array<ProbeSet, 2> m_History;
		glm::uvec2 m_HistoryGrid{ 0u };
		uint32_t m_HistorySpacing = 0;
		glm::ivec2 m_HistoryCell{ 0 };
		glm::vec4 m_PastLattice{ 0.0f };
		uint32_t m_HistoryFrame = 0;
		bool m_HistoryValid = false;
		bool m_Rendered = false;
		bool m_Ready = false;
	};
}
