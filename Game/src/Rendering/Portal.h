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
	struct PortalSheet
	{
		glm::vec3 Base{ 0.0f };
		glm::vec3 Right{ 1.0f, 0.0f, 0.0f };
		glm::vec3 Normal{ 0.0f, 0.0f, -1.0f };
		float HalfWidth = 0.8f;
		float Spring = 2.7f;
		float Apex = 3.5f;
		float Intensity = 1.0f;
		float Surge = 0.0f;
		uint32_t Seed = 0;
	};

	class PortalRenderer
	{
	public:
		static constexpr uint32_t MaxSheets = 8;

		PortalRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator);
		~PortalRenderer();
		PortalRenderer(const PortalRenderer&) = delete;
		PortalRenderer& operator=(const PortalRenderer&) = delete;

		void BeginFrame(float time);
		void Draw(const PortalSheet& sheet);
		void Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera, Rdn::ImageViewHandle noise, const Rdn::Sampler& repeat);

		float Emission = 0.25f;

	private:
		struct SheetGpu
		{
			glm::vec4 BaseHalf;
			glm::vec4 RightSpring;
			glm::vec4 NormalApex;
			glm::vec4 Params;
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
		std::unique_ptr<Rdn::Pipeline> m_Pipeline;
		std::unique_ptr<Rdn::GpuRingBuffer> m_Buffer;
		std::unique_ptr<Rdn::Sampler> m_DepthSampler;
		std::vector<SheetGpu> m_Sheets;
		float m_Time = 0.0f;
	};
}
