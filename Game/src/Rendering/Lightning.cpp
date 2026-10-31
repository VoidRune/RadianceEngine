#include "Lightning.h"
#include "Rendering/GBuffer.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace Game
{
	namespace
	{
		constexpr float CapLength = 0.6f;
		constexpr float SoftDepth = 0.12f;
		constexpr uint32_t TopBranches = 12;
		constexpr uint32_t SubBranches = 4;

		uint32_t Hash(uint32_t x)
		{
			x ^= x >> 16;
			x *= 0x7FEB352Du;
			x ^= x >> 15;
			x *= 0x846CA68Bu;
			x ^= x >> 16;
			return x;
		}

		float Random(uint32_t seed, uint32_t index)
		{
			return float(Hash(seed * 0x9E3779B9u + index * 0x85EBCA6Bu + 0x632BE5ABu) & 0xFFFFFFu) / 16777215.0f;
		}

		float Signed(uint32_t seed, uint32_t index)
		{
			return Random(seed, index) * 2.0f - 1.0f;
		}
	}

	LightningRenderer::LightningRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator)
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
		Rdn::Shader* vertex = compile("res/Shaders/Game/Lightning.vert");
		Rdn::Shader* fragment = compile("res/Shaders/Game/Lightning.frag");
		if (!vertex || !fragment)
		{
			RDN_LOG_ERROR("The lightning shaders failed to compile, lightning won't be drawn");
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
		m_VertexBuffer = std::make_unique<Rdn::GpuRingBuffer>();
		m_Allocator.CreateGpuRingBuffer(m_VertexBuffer.get(), Rdn::GpuRingBufferDesc{
			.ElementSize = uint32_t(MaxVertices * sizeof(VertexGpu)),
			.ElementCount = m_Device.GetFramesInFlightCount(),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer,
			.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
			.Mapped = true,
		});
		m_DepthSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_DepthSampler.get(), Rdn::SamplerDesc{ .MinFilter = Rdn::Filter::Nearest, .MagFilter = Rdn::Filter::Nearest, .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		m_Vertices.reserve(MaxVertices);
		m_Knots.reserve((1u << MaxLevels) + 1);
	}

	LightningRenderer::~LightningRenderer()
	{
		m_Device.WaitIdle();
		if (m_Pipeline)
			m_Allocator.ReleaseResource(m_Pipeline.get());
		if (m_VertexBuffer)
			m_Allocator.ReleaseResource(m_VertexBuffer.get());
		if (m_DepthSampler)
			m_Allocator.ReleaseResource(m_DepthSampler.get());
		for (const std::unique_ptr<Rdn::Shader>& shader : m_Shaders)
			m_Allocator.ReleaseResource(shader.get());
	}

	void LightningRenderer::BeginFrame(float time)
	{
		m_Time = time;
		m_Vertices.clear();
	}

	void LightningRenderer::Draw(const LightningShape& bolt)
	{
		if (!m_Pipeline || bolt.Intensity <= 0.0f || bolt.Reach <= 0.0f)
			return;
		Channel(bolt, bolt.From, bolt.To, bolt.Seed, 1.0f, 1.0f, bolt.Depth, std::min(bolt.Reach, 1.0f), false);
	}

	void LightningRenderer::Channel(const LightningShape& bolt, const glm::vec3& from, const glm::vec3& to, uint32_t seed, float width, float strength, uint32_t depth, float reach, bool taper)
	{
		const glm::vec3 span = to - from;
		const float length = glm::length(span);
		if (length < 1e-3f || reach <= 0.0f)
			return;
		const glm::vec3 axis = span / length;
		const glm::vec3 side = glm::normalize(std::abs(axis.y) > 0.95f ? glm::cross(axis, glm::vec3(1.0f, 0.0f, 0.0f)) : glm::cross(axis, glm::vec3(0.0f, 1.0f, 0.0f)));
		const glm::vec3 lift = glm::cross(side, axis);
		uint32_t levels = 1;
		while (levels < MaxLevels && length / float(1u << levels) > bolt.Step)
			levels++;
		const uint32_t count = 1u << levels;
		const uint32_t detail = seed ^ Hash(bolt.Detail * 0x27D4EB2Fu + 0x165667B1u);

		std::array<glm::vec3, (1u << MaxLevels) + 1> points;
		std::array<float, (1u << MaxLevels) + 1> along;
		points[0] = from;
		points[count] = to;
		uint32_t level = 0;
		for (uint32_t stride = count / 2; stride >= 1; stride /= 2, level++)
		{
			const uint32_t key = level < bolt.CoarseLevels ? seed : detail;
			const float amount = bolt.Jag * length * float(2u * stride) / float(count);
			for (uint32_t i = stride; i < count; i += 2 * stride)
			{
				const glm::vec3& a = points[i - stride];
				const glm::vec3& b = points[i + stride];
				points[i] = 0.5f * (a + b) + side * (Signed(key, i * 2u) * amount) + lift * (Signed(key, i * 2u + 1u) * amount * (1.0f - bolt.Flatten));
			}
		}
		along[0] = 0.0f;
		for (uint32_t i = 1; i <= count; i++)
			along[i] = along[i - 1] + glm::length(points[i] - points[i - 1]);
		const float total = along[count];
		const float limit = reach * total;

		m_Knots.clear();
		for (uint32_t i = 0; i <= count; i++)
		{
			const float t = along[i] / total;
			const float shrink = taper ? 1.0f - 0.75f * t : 1.0f - bolt.Taper * t;
			const float fade = taper ? 1.0f - 0.85f * std::pow(t, 1.3f) : 1.0f;
			if (along[i] <= limit)
			{
				m_Knots.push_back({ points[i], shrink, fade });
				continue;
			}
			const float k = (limit - along[i - 1]) / std::max(along[i] - along[i - 1], 1e-6f);
			if (k > 0.01f)
				m_Knots.push_back({ glm::mix(points[i - 1], points[i], k), shrink, fade });
			break;
		}
		Strip(bolt, width, strength, seed);

		if (depth == 0 || bolt.Branches <= 0.0f)
			return;
		const uint32_t cap = depth == bolt.Depth ? TopBranches : SubBranches;
		const float chance = bolt.Branches * (total / float(count)) * (depth == bolt.Depth ? 1.0f : 0.6f);
		uint32_t made = 0;
		for (uint32_t i = 2; i + 2 < count && made < cap; i++)
		{
			if (along[i] > limit)
				break;
			const float t = along[i] / total;
			if (t < 0.06f || t > 0.9f || Random(detail ^ 0xB5297A4Du, i) >= chance)
				continue;
			const glm::vec3 tangent = glm::normalize(points[i + 1] - points[i - 1]);
			const float around = Random(detail ^ 0x68E31DA4u, i) * 6.2832f;
			const glm::vec3 out = side * std::cos(around) + lift * (std::sin(around) * (1.0f - bolt.Flatten));
			const float spread = bolt.BranchSpread * (0.45f + 0.55f * Random(detail ^ 0x1B56C4E9u, i));
			const glm::vec3 direction = glm::normalize(tangent + out * spread);
			const float branch = length * bolt.BranchLength * (0.35f + 0.65f * Random(detail ^ 0x2D2D2D2Du, i)) * (1.0f - 0.5f * t);
			const float grown = std::clamp((limit - along[i]) / std::max(branch, 1e-3f), 0.0f, 1.0f);
			Channel(bolt, points[i], points[i] + direction * branch, Hash(detail + i * 7919u), width * 0.5f, strength * 0.7f, depth - 1, grown, true);
			made++;
		}
	}

	void LightningRenderer::Strip(const LightningShape& bolt, float width, float strength, uint32_t seed)
	{
		const size_t count = m_Knots.size();
		if (count < 2 || m_Vertices.size() + count + 2 > MaxVertices)
			return;
		const float half = 0.5f * bolt.Glow * width;
		const float core = std::clamp(bolt.Core / std::max(bolt.Glow, 1e-4f), 0.0f, 1.0f);
		const auto direction = [&](size_t i)
		{
			const glm::vec3 d = m_Knots[i + 1].Position - m_Knots[i].Position;
			const float l = glm::length(d);
			return l > 1e-6f ? d / l : glm::vec3(1.0f, 0.0f, 0.0f);
		};
		const auto push = [&](const glm::vec3& position, const glm::vec3& tangent, float w, float s, float fraction, float u)
		{
			const float light = bolt.Intensity * strength * s;
			m_Vertices.push_back({
				.PositionWidth = glm::vec4(position, half * w),
				.Glow = glm::vec4(bolt.GlowColor * light, fraction),
				.Core = glm::vec4(bolt.CoreColor * light, u),
				.Tangent = glm::vec4(tangent, 0.0f),
			});
		};
		float u = float(seed & 0xFFFu) * 0.37f;
		const glm::vec3 first = direction(0);
		push(m_Knots[0].Position - first * (half * m_Knots[0].Width * CapLength), first, m_Knots[0].Width, 0.0f, 0.0f, u - half * CapLength);
		for (size_t i = 0; i < count; i++)
		{
			const glm::vec3 tangent = i == 0 ? first : i + 1 == count ? direction(count - 2) : glm::normalize(direction(i - 1) + direction(i) + glm::vec3(1e-6f));
			push(m_Knots[i].Position, tangent, m_Knots[i].Width, m_Knots[i].Strength, core, u);
			if (i + 1 < count)
				u += glm::length(m_Knots[i + 1].Position - m_Knots[i].Position);
		}
		const glm::vec3 last = direction(count - 2);
		push(m_Knots[count - 1].Position + last * (half * m_Knots[count - 1].Width * CapLength), last, m_Knots[count - 1].Width, 0.0f, 0.0f, u + half * CapLength);
		m_Vertices.back().Tangent.w = 1.0f;
	}

	void LightningRenderer::Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera)
	{
		const uint32_t vertices = uint32_t(m_Vertices.size());
		m_LastVertices = vertices;
		if (!m_Pipeline || vertices < 2)
			return;
		std::memcpy(static_cast<uint8_t*>(m_VertexBuffer->GetMappedPtr()) + m_VertexBuffer->GetOffset(frameIndex), m_Vertices.data(), vertices * sizeof(VertexGpu));
		const glm::mat4& view = camera.View;
		const Constants constants{
			.ViewProjection = camera.ViewProjection,
			.Eye = glm::vec4(camera.Eye, m_Time),
			.Camera = glm::vec4(camera.Near, camera.Far, std::abs(camera.Projection[1][1]) * 0.5f * float(extent.Height), Emission),
			.Screen = glm::vec4(float(extent.Width), float(extent.Height), 1.0f / float(extent.Width), 1.0f / float(extent.Height)),
			.Right = glm::vec4(view[0][0], view[1][0], view[2][0], SoftDepth),
		};
		graph.AddPass("Lightning", Rdn::PassType::Graphics,
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
					.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_VertexBuffer->GetHandle(), m_VertexBuffer->GetOffset(frameIndex), uint64_t(vertices) * sizeof(VertexGpu)))
					.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(depth), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_DepthSampler->GetHandle())));
				cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Pipeline->GetLayout(), &constants, sizeof(constants));
				cmd.Draw(6, vertices - 1, 0, 0);
			});
	}
}
