#include "Portal.h"
#include "Rendering/GBuffer.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <cmath>
#include <cstring>

namespace Game
{
	PortalRenderer::PortalRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator)
		: m_Device(device), m_Allocator(allocator)
	{
		const auto compile = [&](const char* path) -> Rdn::Shader*
		{
			Rdn::ShaderDesc desc;
			if (!Rdn::ShaderCompiler::Compile(path, desc))
				return nullptr;
			m_Shaders.push_back(std::make_unique<Rdn::Shader>());
			m_Allocator.CreateShader(m_Shaders.back().get(), desc);
			return m_Shaders.back().get();
		};
		Rdn::Shader* vertex = compile("res/Shaders/Game/PortalSheet.vert");
		Rdn::Shader* fragment = compile("res/Shaders/Game/PortalSheet.frag");
		if (!vertex || !fragment)
		{
			RDN_LOG_ERROR("The portal shaders failed to compile, portal sheets won't be drawn");
			return;
		}
		m_Pipeline = std::make_unique<Rdn::Pipeline>();
		m_Allocator.CreatePipeline(m_Pipeline.get(), Rdn::PipelineDesc{
			.ShaderStages = { vertex, fragment },
			.CullMode = Rdn::CullMode::None,
			.DepthTest = false,
			.DepthWrite = false,
			.Blend = Rdn::BlendMode::Premultiplied,
			.ColorAttachmentFormats = { SceneColorFormat, EmissionFormat },
			.PushDescriptorSets = { 0 },
		});
		m_Buffer = std::make_unique<Rdn::GpuRingBuffer>();
		m_Allocator.CreateGpuRingBuffer(m_Buffer.get(), Rdn::GpuRingBufferDesc{
			.ElementSize = uint32_t(MaxSheets * sizeof(SheetGpu)),
			.ElementCount = m_Device.GetFramesInFlightCount(),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer,
			.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
			.Mapped = true,
		});
		m_DepthSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_DepthSampler.get(), Rdn::SamplerDesc{ .MinFilter = Rdn::Filter::Nearest, .MagFilter = Rdn::Filter::Nearest, .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		m_Sheets.reserve(MaxSheets);
	}

	PortalRenderer::~PortalRenderer()
	{
		m_Device.WaitIdle();
		if (m_Pipeline)
			m_Allocator.ReleaseResource(m_Pipeline.get());
		if (m_Buffer)
			m_Allocator.ReleaseResource(m_Buffer.get());
		if (m_DepthSampler)
			m_Allocator.ReleaseResource(m_DepthSampler.get());
		for (const std::unique_ptr<Rdn::Shader>& shader : m_Shaders)
			m_Allocator.ReleaseResource(shader.get());
	}

	void PortalRenderer::BeginFrame(float time)
	{
		m_Time = time;
		m_Sheets.clear();
	}

	void PortalRenderer::Draw(const PortalSheet& sheet)
	{
		if (!m_Pipeline || m_Sheets.size() >= MaxSheets || sheet.Intensity <= 0.0f)
			return;
		m_Sheets.push_back({
			.BaseHalf = glm::vec4(sheet.Base, sheet.HalfWidth),
			.RightSpring = glm::vec4(glm::normalize(sheet.Right), sheet.Spring),
			.NormalApex = glm::vec4(glm::normalize(sheet.Normal), sheet.Apex),
			.Params = glm::vec4(sheet.Intensity, float(sheet.Seed % 997u), sheet.Surge, 0.0f),
		});
	}

	void PortalRenderer::Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera, Rdn::ImageViewHandle noise, const Rdn::Sampler& repeat)
	{
		const uint32_t count = uint32_t(m_Sheets.size());
		if (!m_Pipeline || count == 0)
			return;
		std::memcpy(static_cast<uint8_t*>(m_Buffer->GetMappedPtr()) + m_Buffer->GetOffset(frameIndex), m_Sheets.data(), count * sizeof(SheetGpu));
		const Constants constants{
			.ViewProjection = camera.ViewProjection,
			.Eye = glm::vec4(camera.Eye, m_Time),
			.Camera = glm::vec4(camera.Near, camera.Far, 0.0f, Emission),
			.Screen = glm::vec4(float(extent.Width), float(extent.Height), 0.0f, 0.0f),
		};
		graph.AddPass("Portal", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(scene, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.AddColorAttachment(emission, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.UseImage(depth, Rdn::RGImageUsage::Sampled);
			},
			[=, this, sampler = repeat.GetHandle()](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindPipeline(m_Pipeline->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Pipeline->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_Buffer->GetHandle(), m_Buffer->GetOffset(frameIndex), uint64_t(count) * sizeof(SheetGpu)))
					.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(depth), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_DepthSampler->GetHandle()))
					.AddWrite(Rdn::ImageWrite(2, Rdn::DescriptorType::CombinedImageSampler, noise, Rdn::ImageLayout::ShaderReadOnlyOptimal, sampler)));
				cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Pipeline->GetLayout(), &constants, sizeof(constants));
				cmd.Draw(6, count, 0, 0);
			});
	}
}
