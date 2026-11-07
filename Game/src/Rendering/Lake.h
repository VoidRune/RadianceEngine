#pragma once
#include "Rendering/GBuffer.h"
#include "Rendering/Lighting.h"
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
	inline constexpr uint32_t MaxLakeLights = 24;
	inline constexpr Rdn::Format LakeColorFormat = Rdn::Format::R16G16B16A16_Sfloat;

	struct LakeLight
	{
		glm::vec3 Position{ 0.0f };
		float Radius = 1.0f;
		glm::vec3 Color{ 0.0f };
	};

	struct LakeModels
	{
		const Rdn::GpuRingBuffer* Instances = nullptr;
		uint64_t InstanceBytes = 0;
		const Rdn::GpuRingBuffer* Palette = nullptr;
		uint64_t PaletteBytes = 0;
		const Rdn::GpuBuffer* Vertices = nullptr;
		const Rdn::GpuBuffer* Indices = nullptr;
		const Rdn::GpuBuffer* Materials = nullptr;
		const Rdn::DescriptorSet* Textures = nullptr;
		const Rdn::Sampler* RepeatSampler = nullptr;
		Rdn::ImageViewHandle Caustics;
		Rdn::ImageViewHandle Noise;
	};

	struct LakeDraw
	{
		uint32_t FirstIndex = 0;
		uint32_t IndexCount = 0;
		uint32_t FirstInstance = 0;
		uint32_t InstanceCount = 0;
	};

	struct LakeDraws
	{
		std::vector<LakeDraw> Under;
		std::vector<LakeDraw> Glow;
		std::vector<LakeDraw> Mirror;
	};

	class LakeRenderer
	{
	public:
		LakeRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Shader* modelVertex);
		~LakeRenderer();
		LakeRenderer(const LakeRenderer&) = delete;
		LakeRenderer& operator=(const LakeRenderer&) = delete;

		bool IsReady() const { return m_Composite != nullptr; }
		void BeginFrame() { m_Lights.clear(); m_Beast = glm::vec4(0.0f); }
		void SetBeast(const glm::vec4& beast) { m_Beast = beast; }
		void AddLight(const LakeLight& light);
		void Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, const GBuffer& gbuffer, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera, const SceneLighting& lighting, float time, bool halfResolution, const LakeModels& models, const LakeDraws& draws);

	private:
		struct FrameGpu
		{
			glm::mat4 ViewProjection;
			glm::mat4 InverseViewProjection;
			glm::vec4 Eye;
			glm::vec4 SunDirection;
			glm::vec4 SunRadiance;
			glm::vec4 SkyRadiance;
			glm::vec4 WaterColor;
			glm::vec4 Screen;
			glm::vec4 Plane;
			glm::vec4 Aurora;
			glm::vec4 LightCount;
			glm::vec4 Beast;
			std::array<glm::vec4, MaxLakeLights * 2> Lights;
		};

		struct MirrorConstants
		{
			glm::mat4 ViewProjection;
			glm::vec4 Focus;
			glm::vec4 SunDirection;
			glm::vec4 SunRadiance;
			glm::vec4 SkyRadiance;
		};

		Rdn::Shader* Compile(const char* path);

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::vector<std::unique_ptr<Rdn::Shader>> m_Shaders;
		std::unique_ptr<Rdn::Pipeline> m_Under;
		std::unique_ptr<Rdn::Pipeline> m_Glow;
		std::unique_ptr<Rdn::Pipeline> m_Mirror;
		std::unique_ptr<Rdn::ComputePipeline> m_Composite;
		std::unique_ptr<Rdn::GpuRingBuffer> m_Frame;
		std::unique_ptr<Rdn::Sampler> m_Sampler;
		std::vector<LakeLight> m_Lights;
		glm::vec4 m_Beast{ 0.0f };
	};
}
