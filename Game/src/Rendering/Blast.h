#pragma once
#include "Rendering/Particles.h"
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
#include <vector>

namespace Game
{
	struct FireBlast
	{
		glm::vec3 Ground{ 0.0f };
		float Radius = 1.0f;
		float Fire = 0.0f;
		float Smoke = 0.0f;
		float SmokeAmount = 1.0f;
		float Power = 0.0f;
		float Violet = 0.0f;
		float Spin = 1.0f;
		float Height = 0.3f;
		uint32_t Seed = 0;
	};

	class BlastRenderer
	{
	public:
		static constexpr uint32_t MaxBlasts = 32;
		static constexpr uint32_t PuffVertices = 16 * 10 * 6;
		static constexpr uint32_t DomeVertices = 64 * 18 * 6;
		static constexpr uint32_t FireVertices = 15 * PuffVertices;
		static constexpr uint32_t JetVertices = 20 * 16 * 6;
		static constexpr uint32_t SmokeVertices = 16 * PuffVertices;
		static constexpr uint32_t SolidVertices = DomeVertices + FireVertices + JetVertices;

		BlastRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator);
		~BlastRenderer();
		BlastRenderer(const BlastRenderer&) = delete;
		BlastRenderer& operator=(const BlastRenderer&) = delete;

		void BeginFrame(float time);
		void Draw(const FireBlast& blast);
		void Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera);

		float Emission = 0.25f;

	private:
		struct BlastGpu
		{
			glm::vec4 GroundRadius;
			glm::vec4 Look;
			glm::vec4 Shape;
		};

		struct Constants
		{
			glm::mat4 ViewProjection;
			glm::vec4 Eye;
			glm::vec4 Camera;
			glm::vec4 Screen;
		};

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::vector<std::unique_ptr<Rdn::Shader>> m_Shaders;
		std::unique_ptr<Rdn::Pipeline> m_Solid;
		std::unique_ptr<Rdn::Pipeline> m_Ribbon;
		std::unique_ptr<Rdn::Pipeline> m_Smoke;
		std::unique_ptr<Rdn::GpuRingBuffer> m_Buffer;
		std::unique_ptr<Rdn::Sampler> m_DepthSampler;
		std::vector<BlastGpu> m_Blasts;
		float m_Time = 0.0f;
	};
}
