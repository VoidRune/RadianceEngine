#pragma once
#include "Rendering/Particles.h"
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Game
{
	enum class TrailLook : uint8_t
	{
		Flame,
		Frost,
		Obsidian,
		Chain,
		Smoke,
		Wisp,
		Swipe,
	};

	struct TrailStyle
	{
		TrailLook Look = TrailLook::Swipe;
		glm::vec3 Head{ 1.0f };
		glm::vec3 Tail{ 0.5f };
		glm::vec4 Core{ 0.0f };
		float Width = 0.2f;
		glm::vec4 WidthCurve{ 1.0f, 0.8f, 0.5f, 0.0f };
		glm::vec4 AlphaCurve{ 1.0f, 0.75f, 0.4f, 0.0f };
		float Life = 0.35f;
		float Emission = 0.5f;
		float Additive = 1.0f;
		float Tiling = 1.0f;
		float Scroll = 1.0f;
		float Soft = 0.2f;
		float Taper = 0.15f;
		float Spacing = 0.12f;
	};

	class TrailRenderer
	{
	public:
		static constexpr uint32_t MaxTrails = 64;
		static constexpr uint32_t MaxPoints = 40;
		static constexpr uint32_t MaxVertices = 2048;
		static constexpr float Subdivide = 0.09f;

		TrailRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator);
		~TrailRenderer();
		TrailRenderer(const TrailRenderer&) = delete;
		TrailRenderer& operator=(const TrailRenderer&) = delete;

		void BeginFrame(float time);
		void Emit(uint64_t key, const glm::vec3& position, const TrailStyle& style, float strength = 1.0f);
		void Clear();
		void Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera);
		uint32_t GetTrailCount() const { return uint32_t(m_Trails.size()); }
		uint32_t GetVertexCount() const { return m_LastVertices; }

		bool Enabled = true;

	private:
		struct Point
		{
			glm::vec3 Position{ 0.0f };
			float Born = 0.0f;
			float Distance = 0.0f;
			float Strength = 1.0f;
		};

		struct Trail
		{
			uint64_t Key = 0;
			TrailStyle Style;
			std::array<Point, MaxPoints> Points{};
			uint32_t Newest = 0;
			uint32_t Count = 0;
			Point Live;
			uint32_t Seed = 0;
			float Released = -1.0f;
			bool Fed = false;
		};

		struct PointGpu
		{
			glm::vec4 PositionWidth;
			glm::vec4 Color;
			glm::vec4 Tangent;
			glm::vec4 Extra;
		};

		struct StyleGpu
		{
			glm::vec4 Core;
			glm::vec4 Look;
			glm::vec4 Pattern;
		};

		struct Constants
		{
			glm::mat4 ViewProjection;
			glm::vec4 Eye;
			glm::vec4 Camera;
			glm::vec4 Screen;
			glm::vec4 Right;
		};

		const Point& At(const Trail& trail, uint32_t age) const { return trail.Points[(trail.Newest + MaxPoints - age) % MaxPoints]; }
		void Commit(Trail& trail, const Point& point);
		uint32_t Build(const Trail& trail, uint32_t index, uint32_t budget);

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::vector<std::unique_ptr<Rdn::Shader>> m_Shaders;
		std::unique_ptr<Rdn::Pipeline> m_Pipeline;
		std::unique_ptr<Rdn::GpuRingBuffer> m_PointBuffer;
		std::unique_ptr<Rdn::GpuRingBuffer> m_StyleBuffer;
		std::unique_ptr<Rdn::Sampler> m_DepthSampler;
		std::vector<Trail> m_Trails;
		std::vector<PointGpu> m_Vertices;
		std::vector<StyleGpu> m_Styles;
		std::vector<uint32_t> m_Order;
		float m_Time = 0.0f;
		uint32_t m_Seed = 1;
		uint32_t m_LastVertices = 0;
	};
}
