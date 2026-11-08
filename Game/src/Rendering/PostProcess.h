#pragma once
#include "Rendering/Distortion.h"
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace Game
{
	enum class Tonemapper : uint8_t
	{
		AgX,
		Aces,
		Neutral,
		SoftClip,
		Linear,
		Count,
	};

	inline constexpr const char* TonemapperNames[] = { "agx", "aces", "neutral", "softclip", "linear" };
	inline constexpr float TonemapperExposure[] = { 0.87f, 1.47f, 1.0f, 1.0f, 1.0f };
	static_assert(std::size(TonemapperNames) == size_t(Tonemapper::Count) && std::size(TonemapperExposure) == size_t(Tonemapper::Count));
	inline constexpr const char* GradingLutPath = "res/Grading/Default.cube";

	enum class AntiAliasing : uint8_t
	{
		Off,
		Smaa,
		Ssaa15,
		Ssaa2,
		Count,
	};

	inline constexpr const char* AntiAliasingNames[] = { "off", "smaa", "ssaa1.5", "ssaa2" };
	inline constexpr float SupersampleFactors[] = { 1.0f, 1.0f, 1.5f, 2.0f };
	static_assert(std::size(AntiAliasingNames) == size_t(AntiAliasing::Count) && std::size(SupersampleFactors) == size_t(AntiAliasing::Count));

	struct ColorGrade
	{
		glm::vec3 Slope{ 1.0f };
		glm::vec3 Offset{ 0.0f };
		glm::vec3 Power{ 1.0f };
		float Saturation = 1.0f;
		float MidSaturation = 0.0f;
		glm::vec3 Shadows{ 1.0f };
		glm::vec3 Highlights{ 1.0f };

		bool operator==(const ColorGrade&) const = default;
	};

	class PostProcess
	{
	public:
		PostProcess(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Format targetFormat);
		~PostProcess();
		PostProcess(const PostProcess&) = delete;
		PostProcess& operator=(const PostProcess&) = delete;

		void Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage target, Rdn::Extent2D source, Rdn::Extent2D output, const DistortionView& distortion = {}, Rdn::Extent2D display = {});
		void SetGrade(const ColorGrade& grade);
		bool LoadGradingLut(const char* path, std::string& error);
		const ColorGrade& GetGrade() const { return m_Grade; }
		bool HasCustomLut() const { return m_CustomLut; }
		bool IsReady() const { return m_Composite != nullptr; }

		float BloomStrength = 0.3f;
		float Exposure = 1.0f;
		float SceneExposure = 1.0f;
		float Vignette = 0.0f;
		float Desaturation = 0.0f;
		float Sharpness = 0.3f;
		Tonemapper Tonemap = Tonemapper::SoftClip;
		bool Grading = true;
		bool Raw = false;
		bool Smaa = true;

	private:
		void UploadGradingLut(uint32_t size, const std::vector<glm::vec3>& colors);
		void RenderSmaa(Rdn::RenderGraph& graph, Rdn::RGImage color, Rdn::RGImage output, Rdn::Extent2D size);

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::vector<std::unique_ptr<Rdn::Shader>> m_Shaders;
		std::unique_ptr<Rdn::ComputePipeline> m_Down;
		std::unique_ptr<Rdn::ComputePipeline> m_Up;
		std::unique_ptr<Rdn::Pipeline> m_Composite;
		std::unique_ptr<Rdn::Pipeline> m_Upscale;
		std::unique_ptr<Rdn::Pipeline> m_Downsample;
		std::unique_ptr<Rdn::Pipeline> m_SmaaEdges;
		std::unique_ptr<Rdn::Pipeline> m_SmaaWeights;
		std::unique_ptr<Rdn::Pipeline> m_SmaaBlend;
		std::unique_ptr<Rdn::GpuImage> m_SmaaArea;
		std::unique_ptr<Rdn::GpuImage> m_SmaaSearch;
		Rdn::Format m_TargetFormat = Rdn::Format::Undefined;
		std::unique_ptr<Rdn::Sampler> m_Sampler;
		std::unique_ptr<Rdn::Sampler> m_PointSampler;
		std::unique_ptr<Rdn::GpuImage> m_GradingLut;
		ColorGrade m_Grade;
		bool m_CustomLut = false;
	};
}
