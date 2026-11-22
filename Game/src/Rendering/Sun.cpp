#include "Sun.h"
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
		constexpr float StarExtent = 2.9f;
		constexpr float WardExtent = 1.15f;
		constexpr float LayerBack = 1.0f;
		constexpr float LayerFront = 2.0f;
	}

	SunRenderer::SunRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator)
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
		Rdn::Shader* quadVertex = compile("res/Shaders/Game/SunQuad.vert");
		Rdn::Shader* quadFragment = compile("res/Shaders/Game/SunQuad.frag");
		Rdn::Shader* stripVertex = compile("res/Shaders/Game/SunStrip.vert");
		Rdn::Shader* stripFragment = compile("res/Shaders/Game/SunStrip.frag");
		if (!quadVertex || !quadFragment || !stripVertex || !stripFragment)
		{
			RDN_LOG_ERROR("The sun shaders failed to compile, the Ember Heart star won't be drawn");
			return;
		}
		const auto pipeline = [&](Rdn::Shader* vertex, Rdn::Shader* fragment)
		{
			auto created = std::make_unique<Rdn::Pipeline>();
			m_Allocator.CreatePipeline(created.get(), Rdn::PipelineDesc{
				.ShaderStages = { vertex, fragment },
				.CullMode = Rdn::CullMode::None,
				.DepthTest = false,
				.DepthWrite = false,
				.Blend = Rdn::BlendMode::Premultiplied,
				.ColorAttachmentFormats = { SceneColorFormat, EmissionFormat },
				.PushDescriptorSets = { 0 },
			});
			return created;
		};
		m_QuadPipeline = pipeline(quadVertex, quadFragment);
		m_StripPipeline = pipeline(stripVertex, stripFragment);
		const auto ring = [&](uint32_t size)
		{
			auto buffer = std::make_unique<Rdn::GpuRingBuffer>();
			m_Allocator.CreateGpuRingBuffer(buffer.get(), Rdn::GpuRingBufferDesc{
				.ElementSize = size,
				.ElementCount = m_Device.GetFramesInFlightCount(),
				.UsageFlags = Rdn::BufferUsage::StorageBuffer,
				.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
				.Mapped = true,
			});
			return buffer;
		};
		m_QuadBuffer = ring(uint32_t(MaxQuads * sizeof(QuadGpu)));
		m_VertexBuffer = ring(uint32_t(MaxVertices * sizeof(VertexGpu)));
		m_DepthSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_DepthSampler.get(), Rdn::SamplerDesc{ .MinFilter = Rdn::Filter::Nearest, .MagFilter = Rdn::Filter::Nearest, .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		m_Vertices.reserve(MaxVertices);
		m_Quads.reserve(MaxQuads);
	}

	SunRenderer::~SunRenderer()
	{
		m_Device.WaitIdle();
		for (Rdn::Pipeline* pipeline : { m_QuadPipeline.get(), m_StripPipeline.get() })
			if (pipeline)
				m_Allocator.ReleaseResource(pipeline);
		for (Rdn::GpuRingBuffer* buffer : { m_QuadBuffer.get(), m_VertexBuffer.get() })
			if (buffer)
				m_Allocator.ReleaseResource(buffer);
		if (m_DepthSampler)
			m_Allocator.ReleaseResource(m_DepthSampler.get());
		for (const std::unique_ptr<Rdn::Shader>& shader : m_Shaders)
			m_Allocator.ReleaseResource(shader.get());
	}

	void SunRenderer::BeginFrame(float time)
	{
		m_Time = time;
		m_Stars.clear();
		m_Hazes.clear();
		m_Waves.clear();
		m_Wards.clear();
		m_Vertices.clear();
	}

	void SunRenderer::Draw(const SunStar& star)
	{
		if (m_QuadPipeline && star.Radius > 0.0f)
			m_Stars.push_back(star);
	}

	void SunRenderer::Draw(const SunHaze& haze)
	{
		if (m_QuadPipeline && haze.Power > 0.0f)
			m_Hazes.push_back(haze);
	}

	void SunRenderer::Draw(const SunWave& wave)
	{
		if (m_QuadPipeline && wave.Intensity > 0.0f && wave.Radius > 0.0f)
			m_Waves.push_back(wave);
	}

	void SunRenderer::Draw(const SunWard& ward)
	{
		if (m_QuadPipeline && ward.Radius > 0.0f)
			m_Wards.push_back(ward);
	}

	void SunRenderer::Draw(std::span<const SunKnot> knots, const SunRibbon& ribbon)
	{
		const size_t count = knots.size();
		if (!m_StripPipeline || count < 2 || m_Vertices.size() + count > MaxVertices || ribbon.Intensity <= 0.0f)
			return;
		const auto direction = [&](size_t i)
		{
			const glm::vec3 d = knots[i + 1].Position - knots[i].Position;
			const float l = glm::length(d);
			return l > 1e-6f ? d / l : glm::vec3(1.0f, 0.0f, 0.0f);
		};
		float u = float(ribbon.Seed & 0x3FFu) * 0.173f;
		for (size_t i = 0; i < count; i++)
		{
			const glm::vec3 tangent = i == 0 ? direction(0) : i + 1 == count ? direction(count - 2) : glm::normalize(direction(i - 1) + direction(i) + glm::vec3(1e-6f));
			const SunKnot& knot = knots[i];
			m_Vertices.push_back({
				.PositionWidth = glm::vec4(knot.Position, 0.5f * knot.Width),
				.Body = glm::vec4(knot.Heat, ribbon.Intensity * knot.Alpha, ribbon.Opacity * knot.Alpha, float(i) / float(count - 1)),
				.Glow = glm::vec4(0.0f, 0.0f, 0.0f, u),
				.Tangent = glm::vec4(tangent, i + 1 == count ? 1.0f : 0.0f),
				.Extra = glm::vec4(float(ribbon.Look), float(ribbon.Seed % 997u), ribbon.Flow, LayerFront),
			});
			if (i + 1 < count)
				u += glm::length(knots[i + 1].Position - knot.Position);
		}
	}

	void SunRenderer::Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera)
	{
		const uint32_t vertices = uint32_t(m_Vertices.size());
		if (!Enabled || !m_QuadPipeline || !m_StripPipeline || (vertices < 2 && m_Stars.empty() && m_Hazes.empty() && m_Waves.empty() && m_Wards.empty()))
			return;
		const glm::mat4& view = camera.View;
		const glm::vec3 forward = glm::vec3(view[0][2], view[1][2], view[2][2]);
		const auto depthOf = [&](const glm::vec3& point) { return glm::dot(point - camera.Eye, forward); };

		for (size_t i = 0; i + 1 < m_Vertices.size(); i++)
		{
			VertexGpu& vertex = m_Vertices[i];
			if (vertex.Tangent.w > 0.5f || m_Stars.empty())
				continue;
			const glm::vec3 mid = 0.5f * (glm::vec3(vertex.PositionWidth) + glm::vec3(m_Vertices[i + 1].PositionWidth));
			const SunStar* nearest = &m_Stars.front();
			float best = 1e30f;
			for (const SunStar& star : m_Stars)
			{
				const float distance = glm::dot(mid - star.Center, mid - star.Center);
				if (distance < best)
				{
					best = distance;
					nearest = &star;
				}
			}
			vertex.Extra.w = depthOf(mid) > depthOf(nearest->Center) ? LayerBack : LayerFront;
		}

		m_Quads.clear();
		for (const SunHaze& h : m_Hazes)
		{
			if (m_Quads.size() >= MaxQuads)
				break;
			m_Quads.push_back({
				.CenterSize = glm::vec4(h.Center, h.Extent),
				.Axis = glm::vec4(0.0f),
				.A = glm::vec4(h.Radius, h.Top, h.DarkStart, h.DarkEnd),
				.B = glm::vec4(h.Color, h.Power),
				.C = glm::vec4(h.Shafts, h.Darkness, 0.0f, 0.0f),
				.D = glm::vec4(0.0f),
				.E = glm::vec4(0.0f),
				.Info = glm::uvec4(uint32_t(QuadKind::Haze), 0u, 0u, 0u),
			});
		}
		const uint32_t hazeQuads = uint32_t(m_Quads.size());
		for (const SunStar& s : m_Stars)
		{
			if (m_Quads.size() >= MaxQuads)
				break;
			m_Quads.push_back({
				.CenterSize = glm::vec4(s.Center, s.Radius * StarExtent),
				.Axis = s.Beat,
				.A = glm::vec4(s.Radius, s.Spin, s.Brightness, s.Corona),
				.B = s.Flares[0],
				.C = s.Flares[1],
				.D = s.Flares[2],
				.E = glm::vec4(s.Activity, s.Swell, 0.0f, 0.0f),
				.Info = glm::uvec4(uint32_t(QuadKind::Star), 0u, s.Seed, 0u),
			});
		}
		for (const SunWard& w : m_Wards)
		{
			if (m_Quads.size() >= MaxQuads)
				break;
			m_Quads.push_back({
				.CenterSize = glm::vec4(w.Center, w.Radius * WardExtent),
				.Axis = glm::vec4(0.0f),
				.A = glm::vec4(w.Radius, w.StarRadius, w.Glow, w.Pulse),
				.B = w.Impacts[0],
				.C = w.Impacts[1],
				.D = glm::vec4(w.Strength[0], w.Strength[1], 0.0f, 0.0f),
				.E = glm::vec4(0.0f),
				.Info = glm::uvec4(uint32_t(QuadKind::Ward), 0u, 0u, 0u),
			});
		}
		const uint32_t starQuads = uint32_t(m_Quads.size()) - hazeQuads;
		for (const SunWave& w : m_Waves)
		{
			if (m_Quads.size() >= MaxQuads)
				break;
			m_Quads.push_back({
				.CenterSize = glm::vec4(w.Origin, (w.Radius + 3.0f * w.Width) * 1.05f),
				.Axis = glm::vec4(glm::normalize(w.Outward), 0.0f),
				.A = glm::vec4(w.Radius, w.Width, w.Intensity, w.Spread),
				.B = glm::vec4(0.0f),
				.C = glm::vec4(0.0f),
				.D = glm::vec4(0.0f),
				.E = glm::vec4(0.0f),
				.Info = glm::uvec4(uint32_t(QuadKind::Wave), 0u, w.Seed, 0u),
			});
		}
		const uint32_t quads = uint32_t(m_Quads.size());
		const uint32_t waveQuads = quads - hazeQuads - starQuads;

		if (quads > 0)
			std::memcpy(static_cast<uint8_t*>(m_QuadBuffer->GetMappedPtr()) + m_QuadBuffer->GetOffset(frameIndex), m_Quads.data(), quads * sizeof(QuadGpu));
		if (vertices > 0)
			std::memcpy(static_cast<uint8_t*>(m_VertexBuffer->GetMappedPtr()) + m_VertexBuffer->GetOffset(frameIndex), m_Vertices.data(), vertices * sizeof(VertexGpu));
		const Constants constants{
			.ViewProjection = camera.ViewProjection,
			.Eye = glm::vec4(camera.Eye, m_Time),
			.Camera = glm::vec4(camera.Near, camera.Far, std::abs(camera.Projection[1][1]) * 0.5f * float(extent.Height), Emission),
			.Screen = glm::vec4(float(extent.Width), float(extent.Height), 0.0f, SoftDepth),
			.Forward = glm::vec4(forward, 0.0f),
		};
		graph.AddPass("Sun", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(scene, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.AddColorAttachment(emission, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.UseImage(depth, Rdn::RGImageUsage::Sampled);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				const auto bind = [&](const Rdn::Pipeline& pipeline, const Rdn::GpuRingBuffer& buffer, uint64_t size)
				{
					cmd.BindPipeline(pipeline.GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, pipeline.GetLayout(), 0, Rdn::DescriptorWrite()
						.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, buffer.GetHandle(), buffer.GetOffset(frameIndex), size))
						.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(depth), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_DepthSampler->GetHandle())));
				};
				const auto strips = [&](float layer)
				{
					if (vertices < 2)
						return;
					Constants layered = constants;
					layered.Screen.z = layer;
					bind(*m_StripPipeline, *m_VertexBuffer, uint64_t(vertices) * sizeof(VertexGpu));
					cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_StripPipeline->GetLayout(), &layered, sizeof(layered));
					cmd.Draw(6, vertices - 1, 0, 0);
				};
				const auto quadRange = [&](uint32_t first, uint32_t count)
				{
					if (count == 0)
						return;
					bind(*m_QuadPipeline, *m_QuadBuffer, uint64_t(quads) * sizeof(QuadGpu));
					cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_QuadPipeline->GetLayout(), &constants, sizeof(constants));
					cmd.Draw(6, count, 0, first);
				};
				quadRange(0, hazeQuads);
				strips(LayerBack);
				quadRange(hazeQuads, starQuads);
				strips(LayerFront);
				quadRange(hazeQuads + starQuads, waveQuads);
			});
	}
}
