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
	struct LightningShape
	{
		glm::vec3 From{ 0.0f };
		glm::vec3 To{ 0.0f };
		uint32_t Seed = 1;
		uint32_t Detail = 0;
		uint32_t CoarseLevels = 3;
		float Core = 0.04f;
		float Glow = 0.4f;
		float Jag = 0.18f;
		float Step = 0.08f;
		float Branches = 0.8f;
		float BranchLength = 0.35f;
		float BranchSpread = 0.7f;
		uint32_t Depth = 2;
		float Reach = 1.0f;
		float Flatten = 0.0f;
		float Taper = 0.0f;
		float Intensity = 1.0f;
		glm::vec3 CoreColor{ 1.6f, 1.7f, 2.0f };
		glm::vec3 GlowColor{ 0.3f, 0.4f, 1.0f };
	};

	class LightningRenderer
	{
	public:
		static constexpr uint32_t MaxVertices = 12288;
		static constexpr uint32_t MaxLevels = 7;

		LightningRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator);
		~LightningRenderer();
		LightningRenderer(const LightningRenderer&) = delete;
		LightningRenderer& operator=(const LightningRenderer&) = delete;

		void BeginFrame(float time);
		void Draw(const LightningShape& bolt);
		void Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera);
		uint32_t GetVertexCount() const { return m_LastVertices; }

		float Emission = 0.6f;

	private:
		struct VertexGpu
		{
			glm::vec4 PositionWidth;
			glm::vec4 Glow;
			glm::vec4 Core;
			glm::vec4 Tangent;
		};

		struct Knot
		{
			glm::vec3 Position{ 0.0f };
			float Width = 1.0f;
			float Strength = 1.0f;
		};

		struct Constants
		{
			glm::mat4 ViewProjection;
			glm::vec4 Eye;
			glm::vec4 Camera;
			glm::vec4 Screen;
			glm::vec4 Right;
		};

		void Channel(const LightningShape& bolt, const glm::vec3& from, const glm::vec3& to, uint32_t seed, float width, float strength, uint32_t depth, float reach, bool taper);
		void Strip(const LightningShape& bolt, float width, float strength, uint32_t seed);

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::vector<std::unique_ptr<Rdn::Shader>> m_Shaders;
		std::unique_ptr<Rdn::Pipeline> m_Pipeline;
		std::unique_ptr<Rdn::GpuRingBuffer> m_VertexBuffer;
		std::unique_ptr<Rdn::Sampler> m_DepthSampler;
		std::vector<VertexGpu> m_Vertices;
		std::vector<Knot> m_Knots;
		float m_Time = 0.0f;
		uint32_t m_LastVertices = 0;
	};
}
