#pragma once
#include "Rendering/Particles.h"
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace Game
{
	struct VoidSingularity
	{
		glm::vec3 Center{ 0.0f };
		float Radius = 0.6f;
		float Open = 0.0f;
		float Seam = 0.0f;
		float Pupil = 0.4f;
		float Iris = 0.0f;
		float Flash = 0.0f;
		float Glow = 1.0f;
		float Point = 0.0f;
		float Tilt = 0.0f;
		glm::vec3 Gaze{ 0.0f };
		float Gazing = 0.0f;
		uint32_t Seed = 0;
	};

	struct VoidDisk
	{
		glm::vec3 Center{ 0.0f };
		glm::vec3 Normal{ 0.0f, 1.0f, 0.0f };
		float Core = 0.6f;
		float Inner = 1.2f;
		float Outer = 2.6f;
		float Spin = 0.0f;
		float Brightness = 1.0f;
		float Dust = 0.6f;
		float Thin = 0.0f;
		float Reveal = 1.0f;
		uint32_t Seed = 0;
	};

	struct VoidShock
	{
		glm::vec3 Center{ 0.0f };
		float Radius = 1.0f;
		float Width = 0.4f;
		float Intensity = 1.0f;
		float Dark = 0.6f;
		float Spin = 0.0f;
		uint32_t Seed = 0;
	};

	enum class VoidRibbonLook : uint8_t
	{
		Tendril,
		Stream,
		Mote,
	};

	struct VoidKnot
	{
		glm::vec3 Position{ 0.0f };
		float Width = 0.1f;
		float Alpha = 1.0f;
	};

	struct VoidRibbon
	{
		VoidRibbonLook Look = VoidRibbonLook::Stream;
		glm::vec3 Body{ 0.02f, 0.0f, 0.05f };
		glm::vec3 Glow{ 0.6f, 0.3f, 1.2f };
		float Opacity = 0.8f;
		float Intensity = 1.0f;
		float Flow = 1.0f;
		uint32_t Seed = 0;
	};

	class VoidRenderer
	{
	public:
		static constexpr uint32_t MaxQuads = 64;
		static constexpr uint32_t MaxVertices = 8192;

		VoidRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator);
		~VoidRenderer();
		VoidRenderer(const VoidRenderer&) = delete;
		VoidRenderer& operator=(const VoidRenderer&) = delete;

		void BeginFrame(float time);
		void Draw(const VoidSingularity& singularity);
		void Draw(const VoidDisk& disk);
		void Draw(const VoidShock& shock);
		void Draw(std::span<const VoidKnot> knots, const VoidRibbon& ribbon);
		void Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera);
		uint32_t GetVertexCount() const { return m_LastVertices; }

		float Emission = 0.45f;

	private:
		enum class QuadKind : uint32_t
		{
			Singularity,
			Disk,
			Shock,
		};

		struct QuadGpu
		{
			glm::vec4 CenterSize;
			glm::vec4 Axis;
			glm::vec4 A;
			glm::vec4 B;
			glm::vec4 C;
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
			glm::vec4 Right;
		};

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::vector<std::unique_ptr<Rdn::Shader>> m_Shaders;
		std::unique_ptr<Rdn::Pipeline> m_QuadPipeline;
		std::unique_ptr<Rdn::Pipeline> m_StripPipeline;
		std::unique_ptr<Rdn::GpuRingBuffer> m_QuadBuffer;
		std::unique_ptr<Rdn::GpuRingBuffer> m_VertexBuffer;
		std::unique_ptr<Rdn::Sampler> m_DepthSampler;
		std::vector<VoidSingularity> m_Singularities;
		std::vector<VoidDisk> m_Disks;
		std::vector<VoidShock> m_Shocks;
		std::vector<VertexGpu> m_Vertices;
		std::vector<QuadGpu> m_Quads;
		float m_Time = 0.0f;
		uint32_t m_LastVertices = 0;
	};
}
