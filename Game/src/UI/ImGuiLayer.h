#pragma once
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <cstdint>
#include <memory>

namespace Game
{
	class ImGuiLayer
	{
	public:
		ImGuiLayer(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Format targetFormat);
		~ImGuiLayer();
		ImGuiLayer(const ImGuiLayer&) = delete;
		ImGuiLayer& operator=(const ImGuiLayer&) = delete;

		void BeginFrame(float dt, Rdn::Extent2D displaySize);
		void EndFrame();
		void Render(Rdn::RenderGraph& graph, Rdn::RGImage target, uint32_t frameIndex, bool clear);

		bool WantsKeyboard() const;
		bool WantsMouse() const;

	private:
		void CreateFont();
		void CreatePipeline(Rdn::Format targetFormat);
		void ForwardInput();
		void EnsureCapacity(uint64_t vertexBytes, uint64_t indexBytes);

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::unique_ptr<Rdn::Shader> m_VertexShader;
		std::unique_ptr<Rdn::Shader> m_FragmentShader;
		std::unique_ptr<Rdn::Pipeline> m_Pipeline;
		std::unique_ptr<Rdn::GpuImage> m_FontImage;
		std::unique_ptr<Rdn::Sampler> m_Sampler;
		std::unique_ptr<Rdn::GpuRingBuffer> m_VertexBuffer;
		std::unique_ptr<Rdn::GpuRingBuffer> m_IndexBuffer;
		uint64_t m_VertexCapacity = 0;
		uint64_t m_IndexCapacity = 0;
	};
}
