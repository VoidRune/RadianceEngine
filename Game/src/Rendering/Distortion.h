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
	inline constexpr Rdn::Format DistortionFormat = Rdn::Format::R16G16_Sfloat;
	inline constexpr uint32_t MaxLenses = 2;

	enum class DistortionShape : uint8_t
	{
		Shockwave,
		Haze,
		Implosion,
		Lens,
	};

	enum class DistortionAlign : uint8_t
	{
		Billboard,
		Ground,
	};

	struct Distorter
	{
		glm::vec3 Center{ 0.0f };
		DistortionShape Shape = DistortionShape::Shockwave;
		DistortionAlign Align = DistortionAlign::Billboard;
		glm::vec2 Extent{ 1.0f };
		float Yaw = 0.0f;
		float Strength = 0.1f;
		float Soft = 0.15f;
		glm::vec4 Params{ 0.0f };
		glm::vec3 Color{ 0.0f };
		uint32_t Seed = 0;
	};

	struct LensDisk
	{
		glm::vec4 Disk{ 0.0f };
		glm::vec4 Ring{ 0.0f };
	};

	struct DistortionView
	{
		Rdn::RGImage Offsets{};
		Rdn::RGImage Depth{};
		std::array<LensDisk, MaxLenses> Lenses{};
		uint32_t LensCount = 0;
		float Near = 0.5f;
		float Far = 200.0f;
		bool Active = false;
	};

	class ScreenDistortion
	{
	public:
		static constexpr uint32_t MaxDistorters = 256;

		ScreenDistortion(Rdn::Device& device, Rdn::ResourceAllocator& allocator);
		~ScreenDistortion();
		ScreenDistortion(const ScreenDistortion&) = delete;
		ScreenDistortion& operator=(const ScreenDistortion&) = delete;

		void BeginFrame();
		void Draw(const Distorter& distorter);
		DistortionView Render(Rdn::RenderGraph& graph, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera, float time);
		uint32_t GetCount() const { return uint32_t(m_Distorters.size()); }

		bool Enabled = true;

	private:
		struct DistorterGpu
		{
			glm::vec4 CenterYaw;
			glm::vec4 Extent;
			glm::vec4 Params;
			glm::uvec4 Info;
		};

		struct Constants
		{
			glm::mat4 ViewProjection;
			glm::vec4 CameraRight;
			glm::vec4 CameraUp;
			glm::vec4 Screen;
		};

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::vector<std::unique_ptr<Rdn::Shader>> m_Shaders;
		std::unique_ptr<Rdn::Pipeline> m_Pipeline;
		std::unique_ptr<Rdn::GpuRingBuffer> m_Buffer;
		std::unique_ptr<Rdn::Sampler> m_DepthSampler;
		std::vector<DistorterGpu> m_Distorters;
		std::vector<Distorter> m_Lenses;
	};
}
