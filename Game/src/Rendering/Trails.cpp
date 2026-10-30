#include "Trails.h"
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
		constexpr uint32_t SaturatedFade = 3;
		constexpr float RestartDistance = 4.0f;

		float Curve(const glm::vec4& values, float t)
		{
			const float x = std::clamp(t, 0.0f, 1.0f) * 3.0f;
			const int i = std::min(int(x), 2);
			return glm::mix(values[i], values[i + 1], x - float(i));
		}

		glm::vec3 CatmullRom(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, float t)
		{
			const float t2 = t * t;
			const float t3 = t2 * t;
			return 0.5f * (2.0f * p1 + (p2 - p0) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (3.0f * p1 - p0 - 3.0f * p2 + p3) * t3);
		}

		struct Node
		{
			glm::vec3 Position{ 0.0f };
			float Age = 0.0f;
			float Distance = 0.0f;
			float Strength = 1.0f;
		};
	}

	TrailRenderer::TrailRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator)
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
		Rdn::Shader* vertex = compile("res/Shaders/Game/Trail.vert");
		Rdn::Shader* fragment = compile("res/Shaders/Game/Trail.frag");
		if (!vertex || !fragment)
		{
			RDN_LOG_ERROR("The trail shaders failed to compile, nothing will leave a trail");
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
		const auto ring = [&](std::unique_ptr<Rdn::GpuRingBuffer>& buffer, uint32_t size)
		{
			buffer = std::make_unique<Rdn::GpuRingBuffer>();
			m_Allocator.CreateGpuRingBuffer(buffer.get(), Rdn::GpuRingBufferDesc{
				.ElementSize = size,
				.ElementCount = m_Device.GetFramesInFlightCount(),
				.UsageFlags = Rdn::BufferUsage::StorageBuffer,
				.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
				.Mapped = true,
			});
		};
		ring(m_PointBuffer, uint32_t(MaxVertices * sizeof(PointGpu)));
		ring(m_StyleBuffer, uint32_t(MaxTrails * sizeof(StyleGpu)));
		m_DepthSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_DepthSampler.get(), Rdn::SamplerDesc{ .MinFilter = Rdn::Filter::Nearest, .MagFilter = Rdn::Filter::Nearest, .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		m_Trails.reserve(MaxTrails);
		m_Vertices.reserve(MaxVertices);
		m_Styles.reserve(MaxTrails);
	}

	TrailRenderer::~TrailRenderer()
	{
		m_Device.WaitIdle();
		if (m_Pipeline)
			m_Allocator.ReleaseResource(m_Pipeline.get());
		if (m_PointBuffer)
			m_Allocator.ReleaseResource(m_PointBuffer.get());
		if (m_StyleBuffer)
			m_Allocator.ReleaseResource(m_StyleBuffer.get());
		if (m_DepthSampler)
			m_Allocator.ReleaseResource(m_DepthSampler.get());
		for (const std::unique_ptr<Rdn::Shader>& shader : m_Shaders)
			m_Allocator.ReleaseResource(shader.get());
	}

	void TrailRenderer::Commit(Trail& trail, const Point& point)
	{
		if (trail.Count == MaxPoints && m_Time - At(trail, MaxPoints - 1).Born < trail.Style.Life)
			trail.Style.Spacing *= 1.25f;
		trail.Newest = (trail.Newest + 1) % MaxPoints;
		trail.Points[trail.Newest] = point;
		trail.Count = std::min(trail.Count + 1, MaxPoints);
	}

	void TrailRenderer::BeginFrame(float time)
	{
		m_Time = time;
		for (Trail& trail : m_Trails)
		{
			if (trail.Released < 0.0f && !trail.Fed)
			{
				if (glm::length(trail.Live.Position - At(trail, 0).Position) > 1e-4f)
					Commit(trail, trail.Live);
				trail.Released = time;
			}
			trail.Fed = false;
		}
		std::erase_if(m_Trails, [time](const Trail& trail) { return trail.Released >= 0.0f && time - trail.Points[trail.Newest].Born >= trail.Style.Life; });
	}

	void TrailRenderer::Emit(uint64_t key, const glm::vec3& position, const TrailStyle& style, float strength)
	{
		if (!Enabled || !m_Pipeline || style.Life <= 0.0f)
			return;
		const auto found = std::find_if(m_Trails.begin(), m_Trails.end(), [key](const Trail& trail) { return trail.Released < 0.0f && trail.Key == key; });
		Trail* trail = found != m_Trails.end() ? &*found : nullptr;
		if (trail && glm::length(position - At(*trail, 0).Position) > RestartDistance)
		{
			trail->Released = m_Time;
			trail = nullptr;
		}
		if (!trail)
		{
			if (m_Trails.size() >= MaxTrails)
			{
				const auto oldest = std::min_element(m_Trails.begin(), m_Trails.end(), [](const Trail& a, const Trail& b)
				{
					const float x = a.Released < 0.0f ? 1e9f : a.Released;
					const float y = b.Released < 0.0f ? 1e9f : b.Released;
					return x < y;
				});
				if (oldest->Released < 0.0f)
					return;
				m_Trails.erase(oldest);
			}
			Trail& fresh = m_Trails.emplace_back();
			fresh.Key = key;
			fresh.Style = style;
			fresh.Seed = (m_Seed++ * 2654435761u) ^ uint32_t(key * 0x9E3779B97F4A7C15ull >> 32);
			fresh.Points[0] = { position, m_Time, 0.0f, strength };
			fresh.Count = 1;
			fresh.Live = fresh.Points[0];
			fresh.Fed = true;
			return;
		}
		const float spacing = trail->Style.Spacing;
		trail->Style = style;
		trail->Style.Spacing = spacing;
		const Point& newest = At(*trail, 0);
		const float step = glm::length(position - newest.Position);
		trail->Live = { position, m_Time, newest.Distance + step, strength };
		if (step >= spacing)
			Commit(*trail, trail->Live);
		trail->Fed = true;
	}

	void TrailRenderer::Clear()
	{
		m_Trails.clear();
	}

	uint32_t TrailRenderer::Build(const Trail& trail, uint32_t index, uint32_t budget)
	{
		const TrailStyle& style = trail.Style;
		std::array<Node, MaxPoints + 2> nodes;
		uint32_t count = 0;
		const auto push = [&](const Point& point, float fade)
		{
			const Node node{ point.Position, (m_Time - point.Born) / style.Life, point.Distance, point.Strength * fade };
			if (count > 0 && node.Age >= 1.0f)
			{
				const Node& previous = nodes[count - 1];
				const float t = std::clamp((1.0f - previous.Age) / std::max(node.Age - previous.Age, 1e-5f), 0.0f, 1.0f);
				nodes[count++] = { glm::mix(previous.Position, node.Position, t), 1.0f, glm::mix(previous.Distance, node.Distance, t), glm::mix(previous.Strength, node.Strength, t) };
				return false;
			}
			if (node.Age >= 1.0f)
				return false;
			nodes[count++] = node;
			return true;
		};
		bool open = true;
		if (trail.Released < 0.0f && glm::length(trail.Live.Position - At(trail, 0).Position) > 1e-4f)
			open = push(trail.Live, 1.0f);
		const bool saturated = trail.Count == MaxPoints;
		for (uint32_t k = 0; open && k < trail.Count; k++)
		{
			const uint32_t fromOldest = trail.Count - 1 - k;
			const float fade = saturated ? std::min(float(fromOldest) / float(SaturatedFade), 1.0f) : 1.0f;
			open = push(At(trail, k), fade);
		}
		if (count < 2)
			return 0;

		const size_t first = m_Vertices.size();
		const bool smooth = budget >= count * 4;
		float head = 0.0f;
		const auto emit = [&](const glm::vec3& position, float age, float distance, float strength, float fromHead)
		{
			const float taper = style.Taper > 0.0f ? glm::smoothstep(0.0f, style.Taper, fromHead) : 1.0f;
			const float alpha = Curve(style.AlphaCurve, age) * std::min(strength, 1.0f);
			const float half = 0.5f * style.Width * Curve(style.WidthCurve, age) * strength * (0.25f + 0.75f * taper);
			m_Vertices.push_back({
				.PositionWidth = glm::vec4(position, half),
				.Color = glm::vec4(glm::mix(style.Head, style.Tail, std::clamp(age, 0.0f, 1.0f)), alpha * taper),
				.Tangent = glm::vec4(0.0f, 0.0f, 0.0f, fromHead),
				.Extra = glm::vec4(distance, age, float(index), 0.0f),
			});
		};
		for (uint32_t i = 0; i + 1 < count; i++)
		{
			const Node& a = nodes[i];
			const Node& b = nodes[i + 1];
			const float length = glm::length(b.Position - a.Position);
			const uint32_t steps = smooth ? std::clamp(uint32_t(std::ceil(length / Subdivide)), 1u, 8u) : 1u;
			const glm::vec3 before = i > 0 ? nodes[i - 1].Position : 2.0f * a.Position - b.Position;
			const glm::vec3 after = i + 2 < count ? nodes[i + 2].Position : 2.0f * b.Position - a.Position;
			for (uint32_t s = 0; s < steps; s++)
			{
				if (m_Vertices.size() - first + 1 >= budget)
					break;
				const float t = float(s) / float(steps);
				emit(steps > 1 ? CatmullRom(before, a.Position, b.Position, after, t) : a.Position, glm::mix(a.Age, b.Age, t), glm::mix(a.Distance, b.Distance, t), glm::mix(a.Strength, b.Strength, t), head + length * t);
			}
			head += length;
		}
		const Node& last = nodes[count - 1];
		emit(last.Position, last.Age, last.Distance, last.Strength, head);

		const size_t end = m_Vertices.size();
		for (size_t i = first; i < end; i++)
		{
			const glm::vec3 previous(m_Vertices[i > first ? i - 1 : i].PositionWidth);
			const glm::vec3 next(m_Vertices[i + 1 < end ? i + 1 : i].PositionWidth);
			const glm::vec3 direction = previous - next;
			const float length = glm::length(direction);
			m_Vertices[i].Tangent = glm::vec4(length > 1e-6f ? direction / length : glm::vec3(1.0f, 0.0f, 0.0f), m_Vertices[i].Tangent.w);
		}
		m_Vertices[end - 1].Extra.w = 1.0f;
		return uint32_t(end - first);
	}

	void TrailRenderer::Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera)
	{
		m_LastVertices = 0;
		if (!Enabled)
		{
			m_Trails.clear();
			return;
		}
		if (!m_Pipeline || m_Trails.empty())
			return;
		m_Order.resize(m_Trails.size());
		for (uint32_t i = 0; i < m_Order.size(); i++)
			m_Order[i] = i;
		std::sort(m_Order.begin(), m_Order.end(), [&](uint32_t a, uint32_t b)
		{
			const float x = m_Trails[a].Released < 0.0f ? 1e9f : m_Trails[a].Released;
			const float y = m_Trails[b].Released < 0.0f ? 1e9f : m_Trails[b].Released;
			return x > y;
		});
		m_Vertices.clear();
		m_Styles.clear();
		for (const uint32_t i : m_Order)
		{
			const uint32_t budget = MaxVertices - uint32_t(m_Vertices.size());
			if (budget < 2)
				break;
			const Trail& trail = m_Trails[i];
			if (Build(trail, uint32_t(m_Styles.size()), budget) == 0)
				continue;
			const TrailStyle& style = trail.Style;
			m_Styles.push_back({
				.Core = style.Core,
				.Look = glm::vec4(float(style.Look), style.Emission, style.Additive, std::max(style.Soft, 1e-3f)),
				.Pattern = glm::vec4(style.Tiling, style.Scroll, float(trail.Seed & 0xFFFFu), 0.0f),
			});
		}
		const uint32_t vertices = uint32_t(m_Vertices.size());
		if (vertices < 2)
			return;
		m_LastVertices = vertices;
		std::memcpy(static_cast<uint8_t*>(m_PointBuffer->GetMappedPtr()) + m_PointBuffer->GetOffset(frameIndex), m_Vertices.data(), vertices * sizeof(PointGpu));
		std::memcpy(static_cast<uint8_t*>(m_StyleBuffer->GetMappedPtr()) + m_StyleBuffer->GetOffset(frameIndex), m_Styles.data(), m_Styles.size() * sizeof(StyleGpu));
		const uint32_t styles = uint32_t(m_Styles.size());
		const glm::mat4& view = camera.View;
		const Constants constants{
			.ViewProjection = camera.ViewProjection,
			.Eye = glm::vec4(camera.Eye, m_Time),
			.Camera = glm::vec4(camera.Near, camera.Far, std::abs(camera.Projection[1][1]) * 0.5f * float(extent.Height), 0.0f),
			.Screen = glm::vec4(float(extent.Width), float(extent.Height), 1.0f / float(extent.Width), 1.0f / float(extent.Height)),
			.Right = glm::vec4(view[0][0], view[1][0], view[2][0], 0.0f),
		};
		graph.AddPass("Trails", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(scene, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.AddColorAttachment(emission, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.UseImage(depth, Rdn::RGImageUsage::Sampled);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindPipeline(m_Pipeline->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Pipeline->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_PointBuffer->GetHandle(), m_PointBuffer->GetOffset(frameIndex), uint64_t(vertices) * sizeof(PointGpu)))
					.AddWrite(Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, m_StyleBuffer->GetHandle(), m_StyleBuffer->GetOffset(frameIndex), uint64_t(styles) * sizeof(StyleGpu)))
					.AddWrite(Rdn::ImageWrite(2, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(depth), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_DepthSampler->GetHandle())));
				cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Pipeline->GetLayout(), &constants, sizeof(constants));
				cmd.Draw(6, vertices - 1, 0, 0);
			});
	}
}
