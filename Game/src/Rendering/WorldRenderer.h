#pragma once
#include <GameShared/Content/Props.h>
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Game
{
	struct BoxInstance
	{
		glm::vec4 CenterYaw;
		glm::vec4 HalfExtents;
		glm::vec4 ColorEmission;
	};

	class WorldRenderer
	{
	public:
		WorldRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Format colorFormat);
		~WorldRenderer();
		WorldRenderer(const WorldRenderer&) = delete;
		WorldRenderer& operator=(const WorldRenderer&) = delete;

		void Begin(const glm::mat4& viewProjection, const glm::vec3& lightDirection, float focusHeight = 0.0f);
		void Draw(MeshKind mesh, const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec3& color, float yaw = 0.0f, float emission = 0.0f, float ground = 0.0f);
		void DrawBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec3& color, float yaw = 0.0f, float emission = 0.0f, float ground = 0.0f) { Draw(MeshKind::Box, center, halfExtents, color, yaw, emission, ground); }
		void Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D extent, uint32_t frameIndex, const std::array<float, 4>& clearColor);

		bool IsReady() const { return m_Pipeline != nullptr; }
		uint32_t GetInstanceCount() const { return m_LastCount; }

	private:
		struct MeshRange
		{
			uint32_t FirstVertex = 0;
			uint32_t VertexCount = 0;
		};

		bool CreatePipeline(Rdn::Format colorFormat);
		void CreateMeshes();
		void EnsureCapacity(uint32_t instances);

		struct PushConstants
		{
			glm::mat4 ViewProjection;
			glm::vec4 LightDirection;
		};

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::unique_ptr<Rdn::Shader> m_VertexShader;
		std::unique_ptr<Rdn::Shader> m_FragmentShader;
		std::unique_ptr<Rdn::Pipeline> m_Pipeline;
		std::unique_ptr<Rdn::GpuRingBuffer> m_InstanceBuffer;
		std::unique_ptr<Rdn::GpuBuffer> m_MeshBuffer;
		std::array<MeshRange, size_t(MeshKind::Count)> m_Meshes{};
		uint32_t m_Capacity = 0;
		uint32_t m_LastCount = 0;
		std::array<std::vector<BoxInstance>, size_t(MeshKind::Count)> m_Instances;
		std::vector<BoxInstance> m_Packed;
		PushConstants m_Constants{};
	};
}
