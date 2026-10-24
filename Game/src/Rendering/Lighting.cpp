#include "Lighting.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>

namespace Game
{
	namespace
	{
		constexpr float ReceiverLow = -4.0f;
		constexpr float ReceiverHigh = 8.0f;
		constexpr float ExtentStep = 0.5f;
		constexpr float CasterReach = 30.0f;
		constexpr float NormalOffsetTexels = 1.5f;
		constexpr float DepthBiasTexels = 1.0f;
	}

	bool CompileVariant(const char* path, const char* define, Rdn::ShaderDesc& desc, Rdn::ShaderStage stage)
	{
		std::ifstream file(path, std::ios::binary);
		std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		const size_t line = source.find('\n');
		if (!file || line == std::string::npos)
			return false;
		source.insert(line + 1, std::string("#define ") + define + "\n#line 2\n");
		return Rdn::ShaderCompiler::CompileFromSource(source, stage, desc, path);
	}

	ShadowView FitShadowView(const WorldCamera& camera, const glm::vec3& sunDirection, float groundHeight, uint32_t resolution, glm::vec2& extent)
	{
		const glm::vec3 forward = glm::normalize(sunDirection);
		const glm::vec3 up = std::abs(forward.z) < 0.9f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
		const glm::mat4 view = glm::lookAtLH(glm::vec3(0.0f), forward, up);
		const glm::mat4 inverse = glm::inverse(camera.ViewProjection);
		glm::vec3 low(1e9f);
		glm::vec3 high(-1e9f);
		for (const glm::vec2 corner : { glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, -1.0f), glm::vec2(-1.0f, 1.0f), glm::vec2(1.0f, 1.0f) })
		{
			glm::vec4 nearPoint = inverse * glm::vec4(corner, 0.0f, 1.0f);
			glm::vec4 farPoint = inverse * glm::vec4(corner, 1.0f, 1.0f);
			nearPoint /= nearPoint.w;
			farPoint /= farPoint.w;
			const glm::vec3 direction = glm::vec3(farPoint - nearPoint);
			for (const float plane : { groundHeight + ReceiverLow, groundHeight + ReceiverHigh })
			{
				const float t = std::abs(direction.y) > 1e-6f ? std::clamp((plane - nearPoint.y) / direction.y, 0.0f, 1.0f) : 1.0f;
				const glm::vec3 point = glm::vec3(view * glm::vec4(glm::vec3(nearPoint) + direction * t, 1.0f));
				low = glm::min(low, point);
				high = glm::max(high, point);
			}
		}
		const glm::vec2 needed = glm::ceil((glm::vec2(high - low) + ExtentStep) / ExtentStep) * ExtentStep;
		for (int axis = 0; axis < 2; axis++)
			if (extent[axis] < needed[axis] || extent[axis] > needed[axis] + 2.0f * ExtentStep)
				extent[axis] = needed[axis];
		const glm::vec2 texel = extent / float(resolution);
		const glm::vec2 origin = glm::floor(glm::vec2(low) / texel) * texel;
		const float nearPlane = std::floor(low.z - CasterReach);
		const float farPlane = std::ceil(high.z + 1.0f);
		const glm::mat4 projection = glm::orthoLH_ZO(origin.x, origin.x + extent.x, origin.y, origin.y + extent.y, nearPlane, farPlane);
		return { .ViewProjection = projection * view, .TexelSize = std::max(texel.x, texel.y), .DepthRange = farPlane - nearPlane };
	}
	DeferredLighting::DeferredLighting(Rdn::Device& device, Rdn::ResourceAllocator& allocator)
		: m_Device(device), m_Allocator(allocator)
	{
		Rdn::ShaderDesc desc;
		if (!Rdn::ShaderCompiler::Compile("res/Shaders/Game/Lighting.comp", desc))
		{
			RDN_LOG_ERROR("The lighting shader failed to compile, the world won't be lit");
			return;
		}
		m_Shader = std::make_unique<Rdn::Shader>();
		m_Allocator.CreateShader(m_Shader.get(), desc);
		m_Pipeline = std::make_unique<Rdn::ComputePipeline>();
		m_Allocator.CreateComputePipeline(m_Pipeline.get(), Rdn::ComputePipelineDesc{ .ComputeShader = m_Shader.get(), .PushDescriptorSets = { 0 } });
		if (Rdn::ShaderDesc giDesc; CompileVariant("res/Shaders/Game/Lighting.comp", "GLOBAL_ILLUMINATION", giDesc))
		{
			m_GiShader = std::make_unique<Rdn::Shader>();
			m_Allocator.CreateShader(m_GiShader.get(), giDesc);
			m_GiPipeline = std::make_unique<Rdn::ComputePipeline>();
			m_Allocator.CreateComputePipeline(m_GiPipeline.get(), Rdn::ComputePipelineDesc{ .ComputeShader = m_GiShader.get(), .PushDescriptorSets = { 0 } });
		}
		else
			RDN_LOG_ERROR("The global illumination variant of the lighting shader failed to compile, indirect light falls back to the sky ambient");
		m_Frame = std::make_unique<Rdn::GpuRingBuffer>();
		m_Allocator.CreateGpuRingBuffer(m_Frame.get(), Rdn::GpuRingBufferDesc{
			.ElementSize = uint32_t(sizeof(FrameGpu)),
			.ElementCount = m_Device.GetFramesInFlightCount(),
			.UsageFlags = Rdn::BufferUsage::UniformBuffer,
			.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
			.Mapped = true,
		});
		m_Sampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_Sampler.get(), Rdn::SamplerDesc{ .MinFilter = Rdn::Filter::Nearest, .MagFilter = Rdn::Filter::Nearest, .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		m_LinearSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_LinearSampler.get(), Rdn::SamplerDesc{ .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
	}

	DeferredLighting::~DeferredLighting()
	{
		m_Device.WaitIdle();
		for (Rdn::ComputePipeline* pipeline : { m_Pipeline.get(), m_GiPipeline.get() })
			if (pipeline)
				m_Allocator.ReleaseResource(pipeline);
		for (Rdn::Shader* shader : { m_Shader.get(), m_GiShader.get() })
			if (shader)
				m_Allocator.ReleaseResource(shader);
		if (m_Frame)
			m_Allocator.ReleaseResource(m_Frame.get());
		for (Rdn::Sampler* sampler : { m_Sampler.get(), m_LinearSampler.get() })
			if (sampler)
				m_Allocator.ReleaseResource(sampler);
	}

	void DeferredLighting::Render(Rdn::RenderGraph& graph, const GBuffer& gbuffer, Rdn::RGImage scene, Rdn::RGImage shadowMap, Rdn::RGImage giSource, Rdn::RGImage emitters, Rdn::Extent2D extent, uint32_t frameIndex,
		const WorldCamera& camera, const SceneLighting& lighting, const EnvironmentView& environment, const ShadowView& shadow, ShadowQuality quality, float fadeHeight, float edgeBand, const glm::vec3& background, DebugView view)
	{
		if (!m_Pipeline)
			return;
		const bool gi = giSource && m_GiPipeline;
		const Rdn::ComputePipeline* pipeline = gi ? m_GiPipeline.get() : m_Pipeline.get();
		const glm::mat4 toTexture = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 1.0f));
		const FrameGpu frame{
			.InverseViewProjection = glm::inverse(camera.ViewProjection),
			.CameraPosition = glm::vec4(camera.Eye, fadeHeight),
			.SunDirection = glm::vec4(-glm::normalize(lighting.SunDirection), 0.0f),
			.SunRadiance = glm::vec4(lighting.SunColor * lighting.SunIntensity, 0.0f),
			.SkyRadiance = glm::vec4(lighting.SkyColor, 0.0f),
			.GroundRadiance = glm::vec4(lighting.GroundColor, 0.0f),
			.Background = glm::vec4(background, 1.0f),
			.Options = glm::uvec4(uint32_t(view), extent.Width, extent.Height, uint32_t(quality)),
			.ShadowMatrix = toTexture * shadow.ViewProjection,
			.ShadowParams = glm::vec4(shadow.TexelSize * NormalOffsetTexels, shadow.TexelSize * DepthBiasTexels / shadow.DepthRange, 0.0f, 0.0f),
			.GiParams = glm::vec4(lighting.EmissionGi, lighting.ParticleGi, std::max(edgeBand, 1.0f), 0.0f),
			.EnvironmentSh = environment.Irradiance,
			.EnvironmentParams = environment.Params,
			.SkyMaskTransform = environment.SkyMaskTransform,
			.Backdrop = glm::vec4(lighting.Floating ? 1.0f : 0.0f, lighting.FocusHeight, lighting.Time, lighting.Leviathan),
			.Haze = glm::vec4(lighting.Haze, 0.0f),
		};
		const Rdn::ImageWrite environmentWrite(11, Rdn::DescriptorType::CombinedImageSampler, environment.Image->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, environment.Sampler->GetHandle());
		const Rdn::ImageWrite skyMaskWrite(12, Rdn::DescriptorType::CombinedImageSampler, environment.SkyMask->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_LinearSampler->GetHandle());
		m_Frame->Write(frameIndex, frame);
		graph.AddPass("Lighting", Rdn::PassType::Compute,
			[&](Rdn::PassBuilder& builder)
			{
				for (const Rdn::RGImage image : { gbuffer.Albedo, gbuffer.Surface, gbuffer.Emission, gbuffer.Depth, shadowMap })
					builder.UseImage(image, Rdn::RGImageUsage::Sampled);
				builder.UseImage(scene, Rdn::RGImageUsage::StorageWrite);
				if (!gi)
					return;
				builder.UseImage(emitters, Rdn::RGImageUsage::Sampled);
				builder.UseImage(giSource, Rdn::RGImageUsage::StorageWrite);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				const auto sampled = [&](uint32_t binding, Rdn::RGImage image) { return Rdn::ImageWrite(binding, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(image), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_Sampler->GetHandle()); };
				const auto storage = [&](uint32_t binding, Rdn::RGImage image) { return Rdn::ImageWrite(binding, Rdn::DescriptorType::StorageImage, context.GetImageView(image), Rdn::ImageLayout::General); };
				Rdn::DescriptorWrite write;
				write.AddWrite(sampled(0, gbuffer.Albedo)).AddWrite(sampled(1, gbuffer.Surface)).AddWrite(sampled(2, gbuffer.Emission)).AddWrite(sampled(3, gbuffer.Depth))
					.AddWrite(storage(4, scene))
					.AddWrite(Rdn::BufferWrite(5, Rdn::DescriptorType::UniformBuffer, m_Frame->GetHandle(), m_Frame->GetOffset(frameIndex), sizeof(FrameGpu)))
					.AddWrite(sampled(6, shadowMap))
					.AddWrite(environmentWrite).AddWrite(skyMaskWrite);
				if (gi)
					write.AddWrite(sampled(7, emitters)).AddWrite(storage(8, giSource));
				cmd.BindComputePipeline(pipeline->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, pipeline->GetLayout(), 0, write);
				cmd.DispatchThreads(pipeline, extent.Width, extent.Height);
			});
	}
}
