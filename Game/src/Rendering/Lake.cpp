#include "Lake.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstring>

namespace Game
{
	LakeRenderer::LakeRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Shader* modelVertex)
		: m_Device(device), m_Allocator(allocator)
	{
		Rdn::Shader* vertex = Compile("res/Shaders/Game/UnderIce.vert");
		Rdn::Shader* under = Compile("res/Shaders/Game/UnderIce.frag");
		Rdn::Shader* glow = Compile("res/Shaders/Game/UnderIceGlow.frag");
		Rdn::Shader* mirror = Compile("res/Shaders/Game/IceMirror.frag");
		Rdn::Shader* composite = Compile("res/Shaders/Game/IceComposite.comp");
		if (!vertex || !under || !glow || !mirror || !composite || !modelVertex)
		{
			RDN_LOG_ERROR("The lake shaders failed to compile, the Frozen Lake loses its under-ice world");
			return;
		}
		m_Under = std::make_unique<Rdn::Pipeline>();
		m_Allocator.CreatePipeline(m_Under.get(), Rdn::PipelineDesc{
			.ShaderStages = { vertex, under },
			.CullMode = Rdn::CullMode::None,
			.ColorAttachmentFormats = { LakeColorFormat },
			.DepthAttachmentFormat = DepthFormat,
			.PushDescriptorSets = { 0 },
		});
		m_Glow = std::make_unique<Rdn::Pipeline>();
		m_Allocator.CreatePipeline(m_Glow.get(), Rdn::PipelineDesc{
			.ShaderStages = { vertex, glow },
			.CullMode = Rdn::CullMode::None,
			.DepthWrite = false,
			.Blend = Rdn::BlendMode::Additive,
			.ColorAttachmentFormats = { LakeColorFormat },
			.DepthAttachmentFormat = DepthFormat,
			.PushDescriptorSets = { 0 },
		});
		m_Mirror = std::make_unique<Rdn::Pipeline>();
		m_Allocator.CreatePipeline(m_Mirror.get(), Rdn::PipelineDesc{
			.ShaderStages = { modelVertex, mirror },
			.CullMode = Rdn::CullMode::None,
			.ColorAttachmentFormats = { LakeColorFormat },
			.DepthAttachmentFormat = DepthFormat,
			.PushDescriptorSets = { 0 },
		});
		m_Composite = std::make_unique<Rdn::ComputePipeline>();
		m_Allocator.CreateComputePipeline(m_Composite.get(), Rdn::ComputePipelineDesc{ .ComputeShader = composite, .PushDescriptorSets = { 0 } });
		m_Frame = std::make_unique<Rdn::GpuRingBuffer>();
		m_Allocator.CreateGpuRingBuffer(m_Frame.get(), Rdn::GpuRingBufferDesc{
			.ElementSize = uint32_t(sizeof(FrameGpu)),
			.ElementCount = m_Device.GetFramesInFlightCount(),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer,
			.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
			.Mapped = true,
		});
		m_Sampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_Sampler.get(), Rdn::SamplerDesc{ .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
	}

	LakeRenderer::~LakeRenderer()
	{
		m_Device.WaitIdle();
		for (Rdn::Pipeline* pipeline : { m_Under.get(), m_Glow.get(), m_Mirror.get() })
			if (pipeline)
				m_Allocator.ReleaseResource(pipeline);
		if (m_Composite)
			m_Allocator.ReleaseResource(m_Composite.get());
		if (m_Frame)
			m_Allocator.ReleaseResource(m_Frame.get());
		if (m_Sampler)
			m_Allocator.ReleaseResource(m_Sampler.get());
		for (const std::unique_ptr<Rdn::Shader>& shader : m_Shaders)
			m_Allocator.ReleaseResource(shader.get());
	}

	Rdn::Shader* LakeRenderer::Compile(const char* path)
	{
		Rdn::ShaderDesc desc;
		if (!Rdn::ShaderCompiler::Compile(path, desc))
			return nullptr;
		m_Shaders.push_back(std::make_unique<Rdn::Shader>());
		m_Allocator.CreateShader(m_Shaders.back().get(), desc);
		return m_Shaders.back().get();
	}

	void LakeRenderer::AddLight(const LakeLight& light)
	{
		if (m_Lights.size() < MaxLakeLights)
			m_Lights.push_back(light);
	}

	void LakeRenderer::Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, const GBuffer& gbuffer, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera, const SceneLighting& lighting, float time, bool halfResolution, const LakeModels& models, const LakeDraws& draws)
	{
		if (!IsReady())
			return;
		const glm::vec3 toSun = -glm::normalize(lighting.SunDirection);
		const glm::vec3 sun = lighting.SunColor * lighting.SunIntensity;
		const LakeLook& look = lighting.Lake;
		FrameGpu frame{
			.ViewProjection = camera.ViewProjection,
			.InverseViewProjection = glm::inverse(camera.ViewProjection),
			.Eye = glm::vec4(camera.Eye, time),
			.SunDirection = glm::vec4(toSun, 0.0f),
			.SunRadiance = glm::vec4(sun, 0.0f),
			.SkyRadiance = glm::vec4(lighting.SkyColor, 0.0f),
			.WaterColor = glm::vec4(look.WaterColor, look.WaterFog),
			.Screen = glm::vec4(float(extent.Width), float(extent.Height), 1.0f / float(extent.Width), 1.0f / float(extent.Height)),
			.Plane = glm::vec4(look.Plane, look.IceMirror, look.WaterMirror, look.WaterDepth),
			.Aurora = glm::vec4(look.Aurora, look.PoolFloor, look.Caustics, 0.0f),
			.LightCount = glm::vec4(float(m_Lights.size()), float(halfResolution ? HalfExtent(extent).Width : extent.Width), float(halfResolution ? HalfExtent(extent).Height : extent.Height), 0.0f),
			.Beast = m_Beast,
			.Lights = {},
		};
		for (size_t i = 0; i < m_Lights.size(); i++)
		{
			frame.Lights[i * 2] = glm::vec4(m_Lights[i].Position, m_Lights[i].Radius);
			frame.Lights[i * 2 + 1] = glm::vec4(m_Lights[i].Color, 0.0f);
		}
		std::memcpy(static_cast<uint8_t*>(m_Frame->GetMappedPtr()) + m_Frame->GetOffset(frameIndex), &frame, sizeof(frame));

		const Rdn::Extent2D size = halfResolution ? HalfExtent(extent) : extent;
		const Rdn::Extent2D mirrorSize = size;
		const Rdn::RGImage under = graph.CreateImage("Under ice", Rdn::RGImageDesc{ .Size = { size.Width, size.Height, 1 }, .Format = LakeColorFormat });
		const Rdn::RGImage underDepth = graph.CreateImage("Under ice depth", Rdn::RGImageDesc{ .Size = { size.Width, size.Height, 1 }, .Format = DepthFormat });
		const Rdn::RGImage mirror = graph.CreateImage("Ice mirror", Rdn::RGImageDesc{ .Size = { mirrorSize.Width, mirrorSize.Height, 1 }, .Format = LakeColorFormat });
		const Rdn::RGImage mirrorDepth = graph.CreateImage("Ice mirror depth", Rdn::RGImageDesc{ .Size = { mirrorSize.Width, mirrorSize.Height, 1 }, .Format = DepthFormat });
		const uint64_t frameBytes = sizeof(FrameGpu);
		const glm::vec4 clearWater = glm::vec4(look.WaterColor * 0.05f, 1.0f);
		graph.AddPass("Under ice", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(under, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::Store, { clearWater.r, clearWater.g, clearWater.b, 1.0f });
				builder.SetDepthAttachment(underDepth, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::DontCare);
				builder.UseImage(gbuffer.Emission, Rdn::RGImageUsage::Sampled);
			},
			[=, this, &draws](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				if (!models.Instances || !models.Textures)
					return;
				const auto shared = [&](Rdn::DescriptorWrite& write)
				{
					write.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_Frame->GetHandle(), m_Frame->GetOffset(frameIndex), frameBytes));
					write.AddWrite(Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, models.Instances->GetHandle(), models.Instances->GetOffset(frameIndex), models.InstanceBytes));
					write.AddWrite(Rdn::BufferWrite(2, Rdn::DescriptorType::StorageBuffer, models.Vertices->GetHandle(), 0, models.Vertices->GetSize()));
					write.AddWrite(Rdn::BufferWrite(6, Rdn::DescriptorType::StorageBuffer, models.Palette->GetHandle(), models.Palette->GetOffset(frameIndex), models.PaletteBytes));
				};
				cmd.BindIndexBuffer(models.Indices->GetHandle(), Rdn::IndexType::Uint32);
				if (!draws.Under.empty())
				{
					Rdn::DescriptorWrite write;
					shared(write);
					write.AddWrite(Rdn::BufferWrite(3, Rdn::DescriptorType::StorageBuffer, models.Materials->GetHandle(), 0, models.Materials->GetSize()));
					write.AddWrite(Rdn::ImageWrite(4, Rdn::DescriptorType::CombinedImageSampler, models.Caustics, Rdn::ImageLayout::ShaderReadOnlyOptimal, models.RepeatSampler->GetHandle()));
					write.AddWrite(Rdn::ImageWrite(5, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(gbuffer.Emission), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_Sampler->GetHandle()));
					cmd.BindPipeline(m_Under->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Under->GetLayout(), 0, write);
					cmd.BindDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Under->GetLayout(), 1, { models.Textures->GetHandle() });
					for (const LakeDraw& draw : draws.Under)
						cmd.DrawIndexed(draw.IndexCount, draw.InstanceCount, draw.FirstIndex, 0, draw.FirstInstance);
				}
				if (!draws.Glow.empty())
				{
					Rdn::DescriptorWrite write;
					shared(write);
					cmd.BindPipeline(m_Glow->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Glow->GetLayout(), 0, write);
					for (const LakeDraw& draw : draws.Glow)
						cmd.DrawIndexed(draw.IndexCount, draw.InstanceCount, draw.FirstIndex, 0, draw.FirstInstance);
				}
			});
		const float plane
 = look.Plane;
		glm::mat4 flip(1.0f);
		flip[1][1] = -1.0f;
		flip[3][1] = 2.0f * plane;
		const MirrorConstants mirrorConstants{
			.ViewProjection = camera.ViewProjection * flip,
			.Focus = glm::vec4(0.0f),
			.SunDirection = glm::vec4(toSun, plane),
			.SunRadiance = glm::vec4(sun, 0.0f),
			.SkyRadiance = glm::vec4(lighting.SkyColor, 0.0f),
		};
		graph.AddPass("Ice mirror", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(mirror, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::Store, { 0.0f, 0.0f, 0.0f, 0.0f });
				builder.SetDepthAttachment(mirrorDepth, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::DontCare);
			},
			[=, this, &draws](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
			{
				if (!models.Instances || !models.Textures || draws.Mirror.empty())
					return;
				Rdn::DescriptorWrite write;
				write.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, models.Instances->GetHandle(), models.Instances->GetOffset(frameIndex), models.InstanceBytes));
				write.AddWrite(Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, models.Vertices->GetHandle(), 0, models.Vertices->GetSize()));
				write.AddWrite(Rdn::BufferWrite(2, Rdn::DescriptorType::StorageBuffer, models.Palette->GetHandle(), models.Palette->GetOffset(frameIndex), models.PaletteBytes));
				write.AddWrite(Rdn::BufferWrite(3, Rdn::DescriptorType::StorageBuffer, models.Materials->GetHandle(), 0, models.Materials->GetSize()));
				cmd.BindPipeline(m_Mirror->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Mirror->GetLayout(), 0, write);
				cmd.BindDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Mirror->GetLayout(), 1, { models.Textures->GetHandle() });
				cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Mirror->GetLayout(), &mirrorConstants, sizeof(mirrorConstants));
				cmd.BindIndexBuffer(models.Indices->GetHandle(), Rdn::IndexType::Uint32);
				for (const LakeDraw& draw : draws.Mirror)
					cmd.DrawIndexed(draw.IndexCount, draw.InstanceCount, draw.FirstIndex, 0, draw.FirstInstance);
			});
		const uint32_t width = extent.Width;
		const uint32_t height = extent.Height;
		graph.AddPass("Ice composite", Rdn::PassType::Compute,
			[&](Rdn::PassBuilder& builder)
			{
				builder.UseImage(scene, Rdn::RGImageUsage::StorageReadWrite);
				builder.UseImage(gbuffer.Emission, Rdn::RGImageUsage::StorageReadWrite);
				builder.UseImage(gbuffer.Surface, Rdn::RGImageUsage::Sampled);
				builder.UseImage(gbuffer.Depth, Rdn::RGImageUsage::Sampled);
				builder.UseImage(under, Rdn::RGImageUsage::Sampled);
				builder.UseImage(mirror, Rdn::RGImageUsage::Sampled);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				Rdn::DescriptorWrite write;
				write.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_Frame->GetHandle(), m_Frame->GetOffset(frameIndex), frameBytes));
				write.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::StorageImage, context.GetImageView(scene), Rdn::ImageLayout::General));
				write.AddWrite(Rdn::ImageWrite(2, Rdn::DescriptorType::StorageImage, context.GetImageView(gbuffer.Emission), Rdn::ImageLayout::General));
				write.AddWrite(Rdn::ImageWrite(3, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(gbuffer.Surface), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_Sampler->GetHandle()));
				write.AddWrite(Rdn::ImageWrite(4, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(gbuffer.Depth), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_Sampler->GetHandle()));
				write.AddWrite(Rdn::ImageWrite(5, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(under), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_Sampler->GetHandle()));
				write.AddWrite(Rdn::ImageWrite(6, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(mirror), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_Sampler->GetHandle()));
				write.AddWrite(Rdn::ImageWrite(7, Rdn::DescriptorType::CombinedImageSampler, models.Noise, Rdn::ImageLayout::ShaderReadOnlyOptimal, models.RepeatSampler->GetHandle()));
				cmd.BindComputePipeline(m_Composite->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Composite->GetLayout(), 0, write);
				cmd.DispatchThreads(m_Composite.get(), width, height);
			});
	}
}
