#include "Distortion.h"
#include "Rendering/Lighting.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Game
{
	ScreenDistortion::ScreenDistortion(Rdn::Device& device, Rdn::ResourceAllocator& allocator)
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
		Rdn::Shader* vertex = compile("res/Shaders/Game/Distortion.vert");
		Rdn::Shader* fragment = compile("res/Shaders/Game/Distortion.frag");
		if (!vertex || !fragment)
		{
			RDN_LOG_ERROR("The distortion shaders failed to compile, nothing will be distorted");
			return;
		}
		m_Pipeline = std::make_unique<Rdn::Pipeline>();
		m_Allocator.CreatePipeline(m_Pipeline.get(), Rdn::PipelineDesc{
			.ShaderStages = { vertex, fragment },
			.CullMode = Rdn::CullMode::None,
			.DepthTest = false,
			.DepthWrite = false,
			.Blend = Rdn::BlendMode::Additive,
			.ColorAttachmentFormats = { DistortionFormat },
			.PushDescriptorSets = { 0 },
		});
		m_Buffer = std::make_unique<Rdn::GpuRingBuffer>();
		m_Allocator.CreateGpuRingBuffer(m_Buffer.get(), Rdn::GpuRingBufferDesc{
			.ElementSize = uint32_t(MaxDistorters * sizeof(DistorterGpu)),
			.ElementCount = m_Device.GetFramesInFlightCount(),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer,
			.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
			.Mapped = true,
		});
		m_DepthSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_DepthSampler.get(), Rdn::SamplerDesc{ .MinFilter = Rdn::Filter::Nearest, .MagFilter = Rdn::Filter::Nearest, .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
	}

	ScreenDistortion::~ScreenDistortion()
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

	void ScreenDistortion::BeginFrame()
	{
		m_Distorters.clear();
		m_Lenses.clear();
	}

	void ScreenDistortion::Draw(const Distorter& distorter)
	{
		if (!m_Pipeline || m_Distorters.size() >= MaxDistorters || distorter.Strength == 0.0f)
			return;
		if (distorter.Shape == DistortionShape::Lens)
		{
			if (m_Lenses.size() >= MaxLenses)
				return;
			m_Lenses.push_back(distorter);
		}
		m_Distorters.push_back({
			.CenterYaw = glm::vec4(distorter.Center, distorter.Yaw),
			.Extent = glm::vec4(distorter.Extent, distorter.Strength, std::max(distorter.Soft, 1e-3f)),
			.Params = distorter.Params,
			.Info = glm::uvec4(uint32_t(distorter.Shape), uint32_t(distorter.Align), distorter.Seed, 0u),
		});
	}

	DistortionView ScreenDistortion::Render(Rdn::RenderGraph& graph, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera, float time)
	{
		DistortionView view;
		if (!Enabled || !m_Pipeline || m_Distorters.empty())
			return view;
		const uint32_t count = uint32_t(m_Distorters.size());
		std::memcpy(static_cast<uint8_t*>(m_Buffer->GetMappedPtr()) + m_Buffer->GetOffset(frameIndex), m_Distorters.data(), count * sizeof(DistorterGpu));

		const float pixelScale = std::abs(camera.Projection[1][1]) * 0.5f * float(extent.Height);
		for (const Distorter& lens : m_Lenses)
		{
			const glm::vec4 clip = camera.ViewProjection * glm::vec4(lens.Center, 1.0f);
			if (clip.w <= camera.Near)
				continue;
			const glm::vec2 uv = glm::vec2(clip) / clip.w * 0.5f + 0.5f;
			const float pixels = pixelScale / clip.w;
			view.Lenses[view.LensCount++] = { glm::vec4(uv, lens.Params.x * pixels, lens.Params.y * pixels), glm::vec4(lens.Color, clip.w) };
		}

		const Rdn::Extent2D half = HalfExtent(extent);
		const glm::mat4& matrix = camera.View;
		const Constants constants{
			.ViewProjection = camera.ViewProjection,
			.CameraRight = glm::vec4(matrix[0][0], matrix[1][0], matrix[2][0], time),
			.CameraUp = glm::vec4(matrix[0][1], matrix[1][1], matrix[2][1], camera.Near),
			.Screen = glm::vec4(float(half.Width), float(half.Height), camera.Far, 0.0f),
		};
		view.Offsets = graph.CreateImage("Distortion", Rdn::RGImageDesc{ .Size = { half.Width, half.Height, 1 }, .Format = DistortionFormat });
		view.Depth = depth;
		view.Near = camera.Near;
		view.Far = camera.Far;
		view.Active = true;
		const Rdn::RGImage target = view.Offsets;
		graph.AddPass("Distortion", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(target, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::Store, { 0.0f, 0.0f, 0.0f, 0.0f });
				builder.UseImage(depth, Rdn::RGImageUsage::Sampled);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindPipeline(m_Pipeline->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Pipeline->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_Buffer->GetHandle(), m_Buffer->GetOffset(frameIndex), uint64_t(count) * sizeof(DistorterGpu)))
					.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(depth), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_DepthSampler->GetHandle())));
				cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Pipeline->GetLayout(), &constants, sizeof(constants));
				cmd.Draw(6, count, 0, 0);
			});
		return view;
	}
}
