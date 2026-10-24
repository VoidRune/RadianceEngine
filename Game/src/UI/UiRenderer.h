#pragma once
#include "UI/UiTypes.h"
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Game
{
	class UiRenderer
	{
	public:
		static constexpr uint32_t MaxTextures = 8;

		UiRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Format targetFormat);
		~UiRenderer();
		UiRenderer(const UiRenderer&) = delete;
		UiRenderer& operator=(const UiRenderer&) = delete;

		std::optional<uint32_t> CreateTexture(uint32_t width, uint32_t height, const void* rgba, uint32_t mipLevels = 1);
		void SetDistanceRange(float range) { m_DistanceRange = range; }
		void Render(Rdn::RenderGraph& graph, Rdn::RGImage target, uint32_t frameIndex, std::span<const UiPrimitive> primitives, glm::vec2 screenSize, float scale, float time, bool clear);

	private:
		bool CreatePipeline(Rdn::Format targetFormat);
		void EnsureCapacity(uint32_t primitives);

		struct Constants
		{
			glm::vec2 ScreenSize;
			float Scale;
			float DistanceRange;
			float Time;
		};

		enum class Program : uint8_t
		{
			Main,
			Sigil,
			Card,
		};

		struct Run
		{
			uint32_t First = 0;
			uint32_t Count = 0;
			Program Kind = Program::Main;
		};

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::unique_ptr<Rdn::Shader> m_VertexShader;
		std::unique_ptr<Rdn::Shader> m_FragmentShader;
		std::unique_ptr<Rdn::Shader> m_SigilShader;
		std::unique_ptr<Rdn::Shader> m_CardShader;
		std::unique_ptr<Rdn::Pipeline> m_Pipeline;
		std::unique_ptr<Rdn::Pipeline> m_SigilPipeline;
		std::unique_ptr<Rdn::Pipeline> m_CardPipeline;
		std::vector<Run> m_Runs;
		std::unique_ptr<Rdn::Sampler> m_Sampler;
		std::unique_ptr<Rdn::GpuRingBuffer> m_Buffer;
		std::array<std::unique_ptr<Rdn::GpuImage>, MaxTextures> m_Textures;
		uint32_t m_TextureCount = 0;
		uint32_t m_Capacity = 0;
		float m_DistanceRange = 8.0f;
		Constants m_Constants{};
	};
}
