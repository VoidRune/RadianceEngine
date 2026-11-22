#include "Blast.h"
#include "Rendering/GBuffer.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Game
{
	namespace
	{
		constexpr float SoftDepth = 0.12f;
		constexpr Rdn::CullMode BlastCull = Rdn::CullMode::Back;
	}

	BlastRenderer::BlastRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator)
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
		Rdn::Shader* vertex = compile("res/Shaders/Game/Blast.vert");
		Rdn::Shader* fragment = compile("res/Shaders/Game/Blast.frag");
		if (!vertex || !fragment)
		{
			RDN_LOG_ERROR("The blast shaders failed to compile, fireball explosions won't be drawn");
			return;
		}
		const auto pipeline = [&](bool depthWrite, Rdn::CullMode cull)
		{
			auto created = std::make_unique<Rdn::Pipeline>();
			m_Allocator.CreatePipeline(created.get(), Rdn::PipelineDesc{
				.ShaderStages = { vertex, fragment },
				.CullMode = cull,
				.DepthWrite = depthWrite,
				.Blend = Rdn::BlendMode::Premultiplied,
				.ColorAttachmentFormats = { SceneColorFormat, EmissionFormat },
				.DepthAttachmentFormat = DepthFormat,
				.PushDescriptorSets = { 0 },
			});
			return created;
		};
		m_Solid = pipeline(true, BlastCull);
		m_Ribbon = pipeline(true, Rdn::CullMode::None);
		m_Smoke = pipeline(false, BlastCull);
		m_Buffer = std::make_unique<Rdn::GpuRingBuffer>();
		m_Allocator.CreateGpuRingBuffer(m_Buffer.get(), Rdn::GpuRingBufferDesc{
			.ElementSize = uint32_t(MaxBlasts * sizeof(BlastGpu)),
			.ElementCount = m_Device.GetFramesInFlightCount(),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer,
			.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
			.Mapped = true,
		});
		m_DepthSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_DepthSampler.get(), Rdn::SamplerDesc{ .MinFilter = Rdn::Filter::Nearest, .MagFilter = Rdn::Filter::Nearest, .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		m_Blasts.reserve(MaxBlasts);
	}

	BlastRenderer::~BlastRenderer()
	{
		m_Device.WaitIdle();
		for (Rdn::Pipeline* pipeline : { m_Solid.get(), m_Ribbon.get(), m_Smoke.get() })
			if (pipeline)
				m_Allocator.ReleaseResource(pipeline);
		if (m_Buffer)
			m_Allocator.ReleaseResource(m_Buffer.get());
		if (m_DepthSampler)
			m_Allocator.ReleaseResource(m_DepthSampler.get());
		for (const std::unique_ptr<Rdn::Shader>& shader : m_Shaders)
			m_Allocator.ReleaseResource(shader.get());
	}

	void BlastRenderer::BeginFrame(float time)
	{
		m_Time = time;
		m_Blasts.clear();
	}

	void BlastRenderer::Draw(const FireBlast& blast)
	{
		const bool burning = blast.Fire >= 0.0f && blast.Fire < 1.0f;
		const bool smoking = blast.Smoke >= 0.0f && blast.Smoke < 1.0f && blast.SmokeAmount > 0.0f;
		if (!m_Solid || m_Blasts.size() >= MaxBlasts || blast.Radius <= 0.0f || (!burning && !smoking))
			return;
		m_Blasts.push_back({
			.GroundRadius = glm::vec4(blast.Ground, blast.Radius),
			.Look = glm::vec4(burning ? blast.Fire : 2.0f, blast.Power, blast.Violet, blast.Spin),
			.Shape = glm::vec4(blast.Height, float(blast.Seed % 4093u), smoking ? blast.Smoke : 2.0f, blast.SmokeAmount),
		});
	}

	void BlastRenderer::Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera)
	{
		const uint32_t count = uint32_t(m_Blasts.size());
		if (!m_Solid || !m_Ribbon || !m_Smoke || count == 0)
			return;
		std::memcpy(static_cast<uint8_t*>(m_Buffer->GetMappedPtr()) + m_Buffer->GetOffset(frameIndex), m_Blasts.data(), count * sizeof(BlastGpu));
		const Constants constants{
			.ViewProjection = camera.ViewProjection,
			.Eye = glm::vec4(camera.Eye, m_Time),
			.Camera = glm::vec4(camera.Near, camera.Far, SoftDepth, Emission),
			.Screen = glm::vec4(float(extent.Width), float(extent.Height), 0.0f, 0.0f),
		};
		const Rdn::RGImage own = graph.CreateImage("Blast depth", Rdn::RGImageDesc{ .Size = { extent.Width, extent.Height, 1 }, .Format = DepthFormat });
		graph.AddPass("Blast", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(scene, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.AddColorAttachment(emission, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.SetDepthAttachment(own, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::DontCare);
				builder.UseImage(depth, Rdn::RGImageUsage::Sampled);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				const auto draw = [&](const Rdn::Pipeline& pipeline, uint32_t vertices, uint32_t first)
				{
					cmd.BindPipeline(pipeline.GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, pipeline.GetLayout(), 0, Rdn::DescriptorWrite()
						.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_Buffer->GetHandle(), m_Buffer->GetOffset(frameIndex), uint64_t(count) * sizeof(BlastGpu)))
						.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(depth), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_DepthSampler->GetHandle())));
					cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, pipeline.GetLayout(), &constants, sizeof(constants));
					cmd.Draw(vertices, count, first, 0);
				};
				draw(*m_Solid, DomeVertices + FireVertices, 0);
				draw(*m_Ribbon, JetVertices, DomeVertices + FireVertices);
				draw(*m_Smoke, SmokeVertices, SolidVertices);
			});
	}
}
