#include "UiRenderer.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <algorithm>
#include <cstring>

namespace Game
{
	UiRenderer::UiRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Format targetFormat)
		: m_Device(device), m_Allocator(allocator)
	{
		if (!CreatePipeline(targetFormat))
			RDN_LOG_ERROR("The UI shaders failed to compile, the UI won't be drawn");
		m_Sampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_Sampler.get(), Rdn::SamplerDesc{ .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		const uint32_t white = 0xFFFFFFFFu;
		CreateTexture(1, 1, &white);
		EnsureCapacity(4096);
	}

	UiRenderer::~UiRenderer()
	{
		m_Device.WaitIdle();
		if (m_Buffer)
			m_Allocator.ReleaseResource(m_Buffer.get());
		for (uint32_t i = 0; i < m_TextureCount; i++)
			m_Allocator.ReleaseResource(m_Textures[i].get());
		m_Allocator.ReleaseResource(m_Sampler.get());
		if (m_Pipeline)
		{
			m_Allocator.ReleaseResource(m_Pipeline.get());
			m_Allocator.ReleaseResource(m_VertexShader.get());
			m_Allocator.ReleaseResource(m_FragmentShader.get());
		}
	}

	bool UiRenderer::CreatePipeline(Rdn::Format targetFormat)
	{
		Rdn::ShaderDesc vertexDesc;
		Rdn::ShaderDesc fragmentDesc;
		if (!Rdn::ShaderCompiler::Compile("res/Shaders/Game/Ui.vert", vertexDesc) || !Rdn::ShaderCompiler::Compile("res/Shaders/Game/Ui.frag", fragmentDesc))
			return false;
		m_VertexShader = std::make_unique<Rdn::Shader>();
		m_FragmentShader = std::make_unique<Rdn::Shader>();
		m_Allocator.CreateShader(m_VertexShader.get(), vertexDesc);
		m_Allocator.CreateShader(m_FragmentShader.get(), fragmentDesc);
		m_Pipeline = std::make_unique<Rdn::Pipeline>();
		m_Allocator.CreatePipeline(m_Pipeline.get(), Rdn::PipelineDesc{
			.ShaderStages = { m_VertexShader.get(), m_FragmentShader.get() },
			.CullMode = Rdn::CullMode::None,
			.DepthTest = false,
			.DepthWrite = false,
			.Blend = Rdn::BlendMode::Alpha,
			.ColorAttachmentFormats = { targetFormat },
			.PushDescriptorSets = { 0 },
		});
		return true;
	}

	std::optional<uint32_t> UiRenderer::CreateTexture(uint32_t width, uint32_t height, const void* rgba, uint32_t mipLevels)
	{
		if (m_TextureCount >= MaxTextures || width == 0 || height == 0)
			return std::nullopt;
		auto image = std::make_unique<Rdn::GpuImage>();
		m_Allocator.CreateGpuImage(image.get(), Rdn::GpuImageDesc{
			.ImageSize = { width, height, 1 },
			.Format = Rdn::Format::R8G8B8A8_Unorm,
			.UsageFlags = Rdn::ImageUsage::Sampled | Rdn::ImageUsage::TransferDst | Rdn::ImageUsage::TransferSrc,
			.MipLevels = std::max(mipLevels, 1u),
		});
		m_Allocator.SetImageData(image.get(), rgba, uint64_t(width) * height * 4, Rdn::ImageLayout::ShaderReadOnlyOptimal);
		m_Textures[m_TextureCount] = std::move(image);
		return m_TextureCount++;
	}

	void UiRenderer::EnsureCapacity(uint32_t primitives)
	{
		if (primitives <= m_Capacity)
			return;
		if (m_Buffer)
		{
			m_Device.WaitIdle();
			m_Allocator.ReleaseResource(m_Buffer.get());
		}
		m_Capacity = std::max((primitives + primitives / 2 + 1023) / 1024 * 1024, 4096u);
		m_Buffer = std::make_unique<Rdn::GpuRingBuffer>();
		m_Allocator.CreateGpuRingBuffer(m_Buffer.get(), Rdn::GpuRingBufferDesc{
			.ElementSize = uint32_t(m_Capacity * sizeof(UiPrimitive)),
			.ElementCount = m_Device.GetFramesInFlightCount(),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer,
			.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
			.Mapped = true,
		});
	}

	void UiRenderer::Render(Rdn::RenderGraph& graph, Rdn::RGImage target, uint32_t frameIndex, std::span<const UiPrimitive> primitives, glm::vec2 screenSize, float scale, float time, bool clear)
	{
		const uint32_t count = m_Pipeline ? uint32_t(primitives.size()) : 0;
		if (count == 0 && !clear)
			return;
		if (count > 0)
		{
			EnsureCapacity(count);
			std::memcpy(static_cast<uint8_t*>(m_Buffer->GetMappedPtr()) + m_Buffer->GetOffset(frameIndex), primitives.data(), size_t(count) * sizeof(UiPrimitive));
		}
		m_Constants = { screenSize, scale, m_DistanceRange, time };

		graph.AddPass("UI", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(target, clear ? Rdn::AttachmentLoadOp::Clear : Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store, { 0.03f, 0.028f, 0.035f, 1.0f });
			},
			[this, count, frameIndex](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
			{
				if (count == 0)
					return;
				Rdn::DescriptorWrite write;
				write.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_Buffer->GetHandle(), m_Buffer->GetOffset(frameIndex), uint64_t(count) * sizeof(UiPrimitive)));
				for (uint32_t slot = 0; slot < MaxTextures; slot++)
				{
					const Rdn::GpuImage& image = *m_Textures[slot < m_TextureCount ? slot : 0];
					write.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::CombinedImageSampler, image.GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_Sampler->GetHandle(), slot));
				}
				cmd.BindPipeline(m_Pipeline->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Pipeline->GetLayout(), 0, write);
				cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Pipeline->GetLayout(), &m_Constants, sizeof(m_Constants));
				cmd.Draw(6, count, 0, 0);
			});
	}
}
