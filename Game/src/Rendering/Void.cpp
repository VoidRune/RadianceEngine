#include "Void.h"
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
		constexpr float SoftDepth = 0.15f;
		constexpr float SingularityExtent = 2.6f;
		constexpr float DiskExtent = 1.12f;
		constexpr float ShockExtent = 1.0f;
		constexpr float LayerBack = 1.0f;
		constexpr float LayerFront = 2.0f;
	}

	VoidRenderer::VoidRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator)
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
		Rdn::Shader* quadVertex = compile("res/Shaders/Game/VoidQuad.vert");
		Rdn::Shader* quadFragment = compile("res/Shaders/Game/VoidQuad.frag");
		Rdn::Shader* stripVertex = compile("res/Shaders/Game/VoidStrip.vert");
		Rdn::Shader* stripFragment = compile("res/Shaders/Game/VoidStrip.frag");
		if (!quadVertex || !quadFragment || !stripVertex || !stripFragment)
		{
			RDN_LOG_ERROR("The void shaders failed to compile, void effects won't be drawn");
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

	VoidRenderer::~VoidRenderer()
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

	void VoidRenderer::BeginFrame(float time)
	{
		m_Time = time;
		m_Singularities.clear();
		m_Disks.clear();
		m_Shocks.clear();
		m_Vertices.clear();
	}

	void VoidRenderer::Draw(const VoidSingularity& singularity)
	{
		if (m_QuadPipeline && singularity.Radius > 0.0f)
			m_Singularities.push_back(singularity);
	}

	void VoidRenderer::Draw(const VoidDisk& disk)
	{
		if (m_QuadPipeline && disk.Brightness > 0.0f && disk.Outer > disk.Inner)
			m_Disks.push_back(disk);
	}

	void VoidRenderer::Draw(const VoidShock& shock)
	{
		if (m_QuadPipeline && shock.Intensity > 0.0f && shock.Radius > 0.0f)
			m_Shocks.push_back(shock);
	}

	void VoidRenderer::Draw(std::span<const VoidKnot> knots, const VoidRibbon& ribbon)
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
			const VoidKnot& knot = knots[i];
			m_Vertices.push_back({
				.PositionWidth = glm::vec4(knot.Position, 0.5f * knot.Width),
				.Body = glm::vec4(ribbon.Body * (ribbon.Opacity * knot.Alpha), ribbon.Opacity * knot.Alpha),
				.Glow = glm::vec4(ribbon.Glow * (ribbon.Intensity * knot.Alpha), u),
				.Tangent = glm::vec4(tangent, i + 1 == count ? 1.0f : 0.0f),
				.Extra = glm::vec4(float(ribbon.Look), float(ribbon.Seed % 997u), ribbon.Flow, LayerFront),
			});
			if (i + 1 < count)
				u += glm::length(knots[i + 1].Position - knot.Position);
		}
	}

	void VoidRenderer::Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera)
	{
		const uint32_t vertices = uint32_t(m_Vertices.size());
		m_LastVertices = vertices;
		if (!m_QuadPipeline || !m_StripPipeline || (vertices < 2 && m_Singularities.empty() && m_Disks.empty() && m_Shocks.empty()))
			return;
		const glm::mat4& view = camera.View;
		const glm::vec3 forward = -glm::vec3(view[0][2], view[1][2], view[2][2]);
		const auto depthOf = [&](const glm::vec3& point) { return glm::dot(point - camera.Eye, forward); };

		for (size_t i = 0; i + 1 < m_Vertices.size(); i++)
		{
			VertexGpu& vertex = m_Vertices[i];
			if (vertex.Tangent.w > 0.5f || m_Singularities.empty())
				continue;
			const glm::vec3 mid = 0.5f * (glm::vec3(vertex.PositionWidth) + glm::vec3(m_Vertices[i + 1].PositionWidth));
			const VoidSingularity* nearest = nullptr;
			float best = 1e30f;
			for (const VoidSingularity& singularity : m_Singularities)
			{
				const glm::vec2 offset(mid.x - singularity.Center.x, mid.z - singularity.Center.z);
				const float distance = glm::dot(offset, offset);
				if (distance < best)
				{
					best = distance;
					nearest = &singularity;
				}
			}
			vertex.Extra.w = depthOf(mid) > depthOf(nearest->Center) ? LayerBack : LayerFront;
		}

		m_Quads.clear();
		const auto disk = [&](const VoidDisk& d, float layer)
		{
			if (m_Quads.size() >= MaxQuads)
				return;
			m_Quads.push_back({
				.CenterSize = glm::vec4(d.Center, d.Outer * DiskExtent),
				.Axis = glm::vec4(glm::normalize(d.Normal), 0.0f),
				.A = glm::vec4(d.Core, d.Inner, d.Outer, d.Spin),
				.B = glm::vec4(d.Brightness, d.Dust, d.Thin, d.Reveal),
				.C = glm::vec4(0.0f),
				.Info = glm::uvec4(uint32_t(QuadKind::Disk), uint32_t(layer), d.Seed, 0u),
			});
		};
		for (const VoidDisk& d : m_Disks)
			disk(d, LayerBack);
		const uint32_t backQuads = uint32_t(m_Quads.size());
		for (const VoidSingularity& s : m_Singularities)
		{
			if (m_Quads.size() >= MaxQuads)
				break;
			const glm::vec3 toward = glm::normalize(s.Center - camera.Eye);
			glm::vec3 right = glm::cross(toward, glm::vec3(0.0f, 1.0f, 0.0f));
			right = glm::dot(right, right) > 1e-6f ? glm::normalize(right) : glm::vec3(1.0f, 0.0f, 0.0f);
			const glm::vec3 up = glm::cross(right, toward);
			glm::vec2 gaze(0.0f);
			const glm::vec3 offset = s.Gaze - s.Center;
			const glm::vec2 projected(glm::dot(offset, right), glm::dot(offset, up));
			if (s.Gazing > 0.0f && glm::dot(projected, projected) > 1e-6f)
				gaze = glm::normalize(projected) * std::clamp(s.Gazing, 0.0f, 1.0f);
			m_Quads.push_back({
				.CenterSize = glm::vec4(s.Center, s.Radius * SingularityExtent),
				.Axis = glm::vec4(0.0f),
				.A = glm::vec4(s.Radius, s.Open, s.Seam, s.Pupil),
				.B = glm::vec4(s.Iris, s.Flash, s.Glow, s.Point),
				.C = glm::vec4(gaze, s.Tilt, 0.0f),
				.Info = glm::uvec4(uint32_t(QuadKind::Singularity), 0u, s.Seed, 0u),
			});
		}
		const uint32_t middleQuads = uint32_t(m_Quads.size()) - backQuads;
		for (const VoidDisk& d : m_Disks)
			disk(d, LayerFront);
		for (const VoidShock& s : m_Shocks)
		{
			if (m_Quads.size() >= MaxQuads)
				break;
			m_Quads.push_back({
				.CenterSize = glm::vec4(s.Center, (s.Radius + s.Width) * ShockExtent + 0.3f),
				.Axis = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
				.A = glm::vec4(s.Radius, s.Width, s.Intensity, s.Dark),
				.B = glm::vec4(s.Spin, 0.0f, 0.0f, 0.0f),
				.C = glm::vec4(0.0f),
				.Info = glm::uvec4(uint32_t(QuadKind::Shock), uint32_t(LayerFront), s.Seed, 0u),
			});
		}
		const uint32_t quads = uint32_t(m_Quads.size());
		const uint32_t frontQuads = quads - backQuads - middleQuads;

		if (quads > 0)
			std::memcpy(static_cast<uint8_t*>(m_QuadBuffer->GetMappedPtr()) + m_QuadBuffer->GetOffset(frameIndex), m_Quads.data(), quads * sizeof(QuadGpu));
		if (vertices > 0)
			std::memcpy(static_cast<uint8_t*>(m_VertexBuffer->GetMappedPtr()) + m_VertexBuffer->GetOffset(frameIndex), m_Vertices.data(), vertices * sizeof(VertexGpu));
		const Constants constants{
			.ViewProjection = camera.ViewProjection,
			.Eye = glm::vec4(camera.Eye, m_Time),
			.Camera = glm::vec4(camera.Near, camera.Far, std::abs(camera.Projection[1][1]) * 0.5f * float(extent.Height), Emission),
			.Screen = glm::vec4(float(extent.Width), float(extent.Height), 0.0f, SoftDepth),
			.Right = glm::vec4(view[0][0], view[1][0], view[2][0], 0.0f),
		};
		graph.AddPass("Void", Rdn::PassType::Graphics,
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
				strips(LayerBack);
				quadRange(0, backQuads);
				quadRange(backQuads, middleQuads);
				quadRange(backQuads + middleQuads, frontQuads);
				strips(LayerFront);
			});
	}
}
