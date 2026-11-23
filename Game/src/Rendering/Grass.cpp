#include "Grass.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <glm/gtc/packing.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Game
{
	namespace
	{
		constexpr std::array<uint32_t, GrassRenderer::Kinds> Capacity = { 280000, 160000, 16384, 65536 };
		constexpr int32_t MaxCandidatesPerAxis = 1600;
		constexpr float MaxReach = 70.0f;
		constexpr float MaxBladeHeight = 0.6f;
		constexpr glm::vec2 WindHeading{ 0.83f, 0.55f };
		constexpr float WindStrength = 0.2f;
		constexpr float LodStart = 16.5f;
		constexpr float LodEnd = 19.0f;
		constexpr float RegrowSeconds = 30.0f;
		constexpr float CoolSeconds = 1.5f;
		constexpr float ThawSeconds = 9.0f;
		constexpr float SpringSeconds = 0.55f;
		constexpr float TrailRadius = 0.42f;
		constexpr float TrailMatch = 0.45f;
		constexpr float TrailSpeed = 0.4f;

		struct GrassLook
		{
			float Spacing = 0.05f;
			float Width = 0.032f;
			uint32_t Near = 4;
			uint32_t Far = 3;
		};

		constexpr GrassLook Looks[] = {
			{},
			{ 0.075f, 0.046f, 3, 2 },
			{ 0.05f, 0.032f, 4, 3 },
		};
		static_assert(std::size(Looks) == size_t(GrassQuality::Count));

		constexpr glm::vec4 RootColor{ 0.034f, 0.062f, 0.019f, 0.6f };
		constexpr glm::vec4 TipColor{ 0.115f, 0.165f, 0.05f, 0.62f };
		constexpr glm::vec4 DryColor{ 0.2f, 0.17f, 0.08f, 0.7f };
		constexpr glm::vec4 BladeShape{ 0.32f, 0.34f, 0.0f, 0.35f };
		constexpr float NormalLift = 0.45f;
		constexpr float RootOcclusion = 0.3f;
		constexpr float MinPixels = 0.75f;
		constexpr float ScreenWiden = 0.6f;

		struct CommandGpu
		{
			uint32_t IndexCount = 0;
			uint32_t InstanceCount = 0;
			uint32_t FirstIndex = 0;
			int32_t VertexOffset = 0;
			uint32_t FirstInstance = 0;
		};
		static_assert(sizeof(CommandGpu) == 20);

		uint32_t BaseOf(size_t kind)
		{
			uint32_t base = 0;
			for (size_t i = 0; i < kind; i++)
				base += Capacity[i];
			return base;
		}

		void AddBlade(std::vector<uint32_t>& indices, uint32_t segments)
		{
			for (uint32_t s = 0; s + 1 < segments; s++)
			{
				const uint32_t v = 2 * s;
				indices.insert(indices.end(), { v, v + 1, v + 2, v + 1, v + 3, v + 2 });
			}
			const uint32_t last = 2 * (segments - 1);
			indices.insert(indices.end(), { last, last + 1, last + 2 });
		}

		void AddFlower(std::vector<uint32_t>& indices)
		{
			indices.insert(indices.end(), { 0u, 1u, 2u, 1u, 3u, 2u });
			for (uint32_t i = 0; i < 10; i++)
				indices.insert(indices.end(), { 4u, 5u + i, 5u + (i + 1) % 10 });
		}

		void AddClover(std::vector<uint32_t>& indices)
		{
			for (uint32_t leaf = 0; leaf < 3; leaf++)
			{
				const uint32_t b = leaf * 6;
				indices.insert(indices.end(), { b, b + 1, b + 2, b, b + 2, b + 3, b, b + 3, b + 4, b, b + 4, b + 5 });
			}
		}
	}

	GrassRenderer::GrassRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator)
		: m_Device(device), m_Allocator(allocator)
	{
		Rdn::ShaderDesc generate;
		Rdn::ShaderDesc reset;
		Rdn::ShaderDesc finish;
		Rdn::ShaderDesc effects;
		Rdn::ShaderDesc vertex;
		Rdn::ShaderDesc fragment;
		constexpr const char* Compute = "res/Shaders/Game/GrassGenerate.comp";
		if (!Rdn::ShaderCompiler::Compile(Compute, generate) || !CompileVariant(Compute, "RESET", reset) || !CompileVariant(Compute, "FINISH", finish) || !CompileVariant(Compute, "EFFECTS", effects)
			|| !Rdn::ShaderCompiler::Compile("res/Shaders/Game/Grass.vert", vertex) || !Rdn::ShaderCompiler::Compile("res/Shaders/Game/Grass.frag", fragment))
		{
			RDN_LOG_ERROR("The grass shaders failed to compile, grass won't be drawn");
			return;
		}
		for (const Rdn::ShaderDesc* desc : { &generate, &reset, &finish, &effects, &vertex, &fragment })
		{
			m_Shaders.push_back(std::make_unique<Rdn::Shader>());
			m_Allocator.CreateShader(m_Shaders.back().get(), *desc);
		}
		const auto compute = [&](Rdn::Shader* shader)
		{
			auto pipeline = std::make_unique<Rdn::ComputePipeline>();
			m_Allocator.CreateComputePipeline(pipeline.get(), Rdn::ComputePipelineDesc{ .ComputeShader = shader, .PushDescriptorSets = { 0 } });
			return pipeline;
		};
		m_Generate = compute(m_Shaders[0].get());
		m_Reset = compute(m_Shaders[1].get());
		m_Finish = compute(m_Shaders[2].get());
		m_Effects = compute(m_Shaders[3].get());
		m_Draw = std::make_unique<Rdn::Pipeline>();
		m_Allocator.CreatePipeline(m_Draw.get(), Rdn::PipelineDesc{
			.ShaderStages = { m_Shaders[4].get(), m_Shaders[5].get() },
			.CullMode = Rdn::CullMode::None,
			.ColorAttachmentFormats = { AlbedoFormat, SurfaceFormat, EmissionFormat },
			.DepthAttachmentFormat = DepthFormat,
			.PushDescriptorSets = { 0 },
		});

		const auto deviceBuffer = [&](uint64_t size, Rdn::BufferUsage usage)
		{
			auto buffer = std::make_unique<Rdn::GpuBuffer>();
			m_Allocator.CreateGpuBuffer(buffer.get(), Rdn::GpuBufferDesc{ .Size = size, .UsageFlags = usage | Rdn::BufferUsage::TransferDst, .MemoryProperty = Rdn::MemoryProperty::DeviceLocal });
			return buffer;
		};
		m_Blades = deviceBuffer(uint64_t(BaseOf(Kinds)) * 2 * sizeof(glm::vec4), Rdn::BufferUsage::StorageBuffer);
		m_Command = deviceBuffer(Kinds * sizeof(CommandGpu), Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::IndirectBuffer);

		std::vector<uint32_t> indices;
		const auto range = [&](auto&& add)
		{
			const uint32_t first = uint32_t(indices.size());
			add();
			return glm::uvec2(first, uint32_t(indices.size()) - first);
		};
		const glm::uvec2 flower = range([&] { AddFlower(indices); });
		const glm::uvec2 clover = range([&] { AddClover(indices); });
		for (size_t quality = 1; quality < size_t(GrassQuality::Count); quality++)
		{
			const GrassLook& look = Looks[quality];
			m_Ranges[quality] = { range([&] { AddBlade(indices, look.Near); }), range([&] { AddBlade(indices, look.Far); }), flower, clover };
		}
		m_Indices = deviceBuffer(indices.size() * sizeof(uint32_t), Rdn::BufferUsage::IndexBuffer);
		m_Allocator.SetDeviceLocalBufferData(m_Indices.get(), indices.data(), indices.size() * sizeof(uint32_t));

		m_Frame = std::make_unique<Rdn::GpuRingBuffer>();
		m_Allocator.CreateGpuRingBuffer(m_Frame.get(), Rdn::GpuRingBufferDesc{
			.ElementSize = uint32_t(sizeof(FrameGpu)),
			.ElementCount = m_Device.GetFramesInFlightCount(),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer,
			.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
			.Mapped = true,
		});
		m_FieldSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_FieldSampler.get(), Rdn::SamplerDesc{ .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		m_Ready = true;
	}

	GrassRenderer::~GrassRenderer()
	{
		m_Device.WaitIdle();
		for (Rdn::GpuBuffer* buffer : { m_Blades.get(), m_Command.get(), m_Indices.get() })
			if (buffer)
				m_Allocator.ReleaseResource(buffer);
		if (m_Frame)
			m_Allocator.ReleaseResource(m_Frame.get());
		for (Rdn::GpuImage* image : { m_FieldImage.get(), m_EffectImage.get() })
			if (image)
				m_Allocator.ReleaseResource(image);
		if (m_FieldSampler)
			m_Allocator.ReleaseResource(m_FieldSampler.get());
		for (Rdn::ComputePipeline* pipeline : { m_Generate.get(), m_Reset.get(), m_Finish.get(), m_Effects.get() })
			if (pipeline)
				m_Allocator.ReleaseResource(pipeline);
		if (m_Draw)
			m_Allocator.ReleaseResource(m_Draw.get());
		for (const std::unique_ptr<Rdn::Shader>& shader : m_Shaders)
			m_Allocator.ReleaseResource(shader.get());
	}

	void GrassRenderer::SetField(const GrassField& field)
	{
		m_Device.WaitIdle();
		for (std::unique_ptr<Rdn::GpuImage>* image : { &m_FieldImage, &m_EffectImage })
			if (*image)
			{
				m_Allocator.ReleaseResource(image->get());
				image->reset();
			}
		m_Previous.clear();
		if (field.IsEmpty() || field.Columns < 2 || field.Rows < 2 || field.Texels.size() != size_t(field.Columns) * size_t(field.Rows))
			return;
		std::vector<uint16_t> halves(field.Texels.size() * 4);
		glm::vec2 heights(1e9f, -1e9f);
		bool any = false;
		for (size_t i = 0; i < field.Texels.size(); i++)
		{
			const glm::vec4& texel = field.Texels[i];
			for (glm::length_t c = 0; c < 4; c++)
				halves[i * 4 + size_t(c)] = glm::packHalf1x16(texel[c]);
			if (texel.y > 0.0f)
			{
				heights = { std::min(heights.x, texel.x), std::max(heights.y, texel.x) };
				any = true;
			}
		}
		if (!any)
			return;
		m_FieldImage = std::make_unique<Rdn::GpuImage>();
		m_Allocator.CreateGpuImage(m_FieldImage.get(), Rdn::GpuImageDesc{ .ImageSize = { uint32_t(field.Columns), uint32_t(field.Rows), 1 }, .Format = Rdn::Format::R16G16B16A16_Sfloat, .UsageFlags = Rdn::ImageUsage::Sampled | Rdn::ImageUsage::TransferDst });
		m_Allocator.SetImageData(m_FieldImage.get(), halves.data(), halves.size() * sizeof(uint16_t), Rdn::ImageLayout::ShaderReadOnlyOptimal);
		m_FieldOrigin = field.Origin;
		m_FieldStep = field.Step;
		m_FieldSize = { field.Columns, field.Rows };
		m_FieldMin = field.Origin;
		m_FieldMax = field.Origin + glm::vec2(float(field.Columns - 1), float(field.Rows - 1)) * field.Step;
		m_FieldHeights = heights;

		m_EffectSize = { field.Columns * 2 - 1, field.Rows * 2 - 1 };
		const std::vector<uint16_t> clear(size_t(m_EffectSize.x) * size_t(m_EffectSize.y) * 4, 0);
		m_EffectImage = std::make_unique<Rdn::GpuImage>();
		m_Allocator.CreateGpuImage(m_EffectImage.get(), Rdn::GpuImageDesc{ .ImageSize = { uint32_t(m_EffectSize.x), uint32_t(m_EffectSize.y), 1 }, .Format = Rdn::Format::R16G16B16A16_Sfloat, .UsageFlags = Rdn::ImageUsage::Storage | Rdn::ImageUsage::Sampled | Rdn::ImageUsage::TransferDst });
		m_Allocator.SetImageData(m_EffectImage.get(), clear.data(), clear.size() * sizeof(uint16_t), Rdn::ImageLayout::General);
	}

	void GrassRenderer::BeginFrame(float time)
	{
		m_Delta = m_LastTime >= 0.0f ? std::clamp(time - m_LastTime, 0.0f, 0.1f) : 0.0f;
		m_LastTime = time;
		m_Time = time;
		m_Pushers.clear();
		m_Stamps.clear();
	}

	void GrassRenderer::Push(const glm::vec3& position, float radius, uint32_t key)
	{
		for (Pusher& pusher : m_Pushers)
			if (pusher.Key == key)
				return;
		m_Pushers.push_back({ glm::vec4(position, radius), key });
	}

	void GrassRenderer::Stamp(const GrassStamp& stamp)
	{
		if (m_FieldImage && m_Stamps.size() < MaxStamps)
			m_Stamps.push_back(stamp);
	}

	void GrassRenderer::AddTrails(float dt)
	{
		std::vector<glm::vec3> current;
		current.reserve(m_Pushers.size());
		for (const Pusher& pusher : m_Pushers)
		{
			const glm::vec3 position(pusher.PositionRadius);
			current.push_back(position);
			glm::vec2 direction(0.0f);
			float best = TrailMatch;
			for (const glm::vec3& previous : m_Previous)
				if (const float distance = glm::distance(glm::vec2(previous.x, previous.z), glm::vec2(position.x, position.z)); distance < best)
				{
					best = distance;
					direction = glm::vec2(position.x - previous.x, position.z - previous.z);
				}
			const bool moving = dt > 1e-4f && glm::length(direction) / dt > TrailSpeed;
			m_Stamps.insert(m_Stamps.begin(), GrassStamp{
				.Kind = GrassStampKind::Trail,
				.Center = position,
				.Extent = glm::vec2(TrailRadius),
				.Strength = moving ? 0.9f : 0.5f,
				.Direction = moving ? glm::normalize(direction) : glm::vec2(0.0f),
			});
		}
		m_Previous = std::move(current);
		if (m_Stamps.size() > MaxStamps)
			m_Stamps.resize(MaxStamps);
	}

	void GrassRenderer::Render(Rdn::RenderGraph& graph, const GBuffer& gbuffer, uint32_t frameIndex, const WorldCamera& camera, Rdn::Extent2D extent)
	{
		m_LastCandidates = 0;
		if (!m_Ready || !m_FieldImage || !m_EffectImage || Quality == GrassQuality::Off)
			return;
		const GrassLook& look = Looks[size_t(Quality)];

		const glm::mat4 inverse = glm::inverse(camera.ViewProjection);
		const auto unproject = [&](float x, float y, float z)
		{
			const glm::vec4 p = inverse * glm::vec4(x, y, z, 1.0f);
			return glm::vec3(p) / p.w;
		};
		glm::vec2 low(1e9f);
		glm::vec2 high(-1e9f);
		for (const glm::vec2 corner : { glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, -1.0f), glm::vec2(-1.0f, 1.0f), glm::vec2(1.0f, 1.0f) })
		{
			const glm::vec3 nearPoint = unproject(corner.x, corner.y, 0.0f);
			const glm::vec3 ray = unproject(corner.x, corner.y, 1.0f) - nearPoint;
			for (const float plane : { m_FieldHeights.x - 0.1f, m_FieldHeights.y + MaxBladeHeight })
			{
				float s = std::abs(ray.y) > 1e-6f ? (plane - nearPoint.y) / ray.y : 1.0f;
				s = s < 0.0f ? 1.0f : s;
				glm::vec3 hit = nearPoint + ray * s;
				const glm::vec2 offset(hit.x - camera.Eye.x, hit.z - camera.Eye.z);
				if (const float reach = glm::length(offset); reach > MaxReach)
					hit = glm::vec3(camera.Eye.x, 0.0f, camera.Eye.z) + glm::vec3(offset.x, 0.0f, offset.y) * (MaxReach / reach);
				low = glm::min(low, glm::vec2(hit.x, hit.z));
				high = glm::max(high, glm::vec2(hit.x, hit.z));
			}
		}
		low = glm::max(low - MaxBladeHeight, m_FieldMin);
		high = glm::min(high + MaxBladeHeight, m_FieldMax);
		if (low.x >= high.x || low.y >= high.y)
			return;
		const glm::ivec2 start(int32_t(std::floor(low.x / look.Spacing)), int32_t(std::floor(low.y / look.Spacing)));
		const glm::ivec2 end(int32_t(std::ceil(high.x / look.Spacing)), int32_t(std::ceil(high.y / look.Spacing)));
		const glm::ivec2 count = glm::min(end - start + 1, glm::ivec2(MaxCandidatesPerAxis));
		m_LastCandidates = uint32_t(count.x) * uint32_t(count.y);

		if (m_Pushers.size() > MaxPushers)
		{
			const glm::vec2 eye(camera.Eye.x, camera.Eye.z);
			std::partial_sort(m_Pushers.begin(), m_Pushers.begin() + MaxPushers, m_Pushers.end(), [&](const Pusher& a, const Pusher& b)
				{ return glm::distance(glm::vec2(a.PositionRadius.x, a.PositionRadius.z), eye) < glm::distance(glm::vec2(b.PositionRadius.x, b.PositionRadius.z), eye); });
			m_Pushers.resize(MaxPushers);
		}
		AddTrails(m_Delta);

		FrameGpu& frame = *reinterpret_cast<FrameGpu*>(static_cast<uint8_t*>(m_Frame->GetMappedPtr()) + m_Frame->GetOffset(frameIndex));
		frame.ViewProjection = camera.ViewProjection;
		frame.Eye = glm::vec4(camera.Eye, m_Time);
		frame.Region = glm::ivec4(start, count);
		for (size_t kind = 0; kind < Kinds; kind++)
		{
			const glm::uvec2 range = m_Ranges[size_t(Quality)][kind];
			frame.Draws[kind] = glm::uvec4(range.y, range.x, BaseOf(kind), Capacity[kind]);
		}
		frame.Counts = glm::uvec4(look.Near, look.Far, uint32_t(m_Stamps.size()), uint32_t(m_Pushers.size()));
		frame.Grid = glm::vec4(look.Spacing, (look.Spacing / 0.05f) * (look.Spacing / 0.05f), LodStart, LodEnd);
		frame.Field = glm::vec4(m_FieldOrigin, 1.0f / m_FieldStep, 0.0f);
		frame.FieldSize = glm::vec4(float(m_FieldSize.x), float(m_FieldSize.y), 1.0f / float(m_FieldSize.x), 1.0f / float(m_FieldSize.y));
		frame.Effects = glm::vec4(m_FieldOrigin, 2.0f / m_FieldStep, m_Delta);
		frame.EffectSize = glm::vec4(float(m_EffectSize.x), float(m_EffectSize.y), 1.0f / float(m_EffectSize.x), 1.0f / float(m_EffectSize.y));
		frame.Wind = glm::vec4(glm::normalize(WindHeading), WindStrength, 0.0f);
		frame.Shape = glm::vec4(BladeShape.x, BladeShape.y, look.Width, BladeShape.w);
		frame.Root = RootColor;
		frame.Tip = TipColor;
		frame.Dry = DryColor;
		const float pixel = 2.0f / (std::max(std::abs(camera.Projection[1][1]), 1e-3f) * float(std::max(extent.Height, 1u)));
		frame.Look = glm::vec4(NormalLift, RootOcclusion, pixel * MinPixels, ScreenWiden);
		frame.Decay = glm::vec4(1.0f / RegrowSeconds, 1.0f / CoolSeconds, 1.0f / ThawSeconds, 1.0f / SpringSeconds);
		for (size_t i = 0; i < m_Pushers.size(); i++)
			frame.Pushers[i] = m_Pushers[i].PositionRadius;
		for (size_t i = 0; i < m_Stamps.size(); i++)
		{
			const GrassStamp& stamp = m_Stamps[i];
			const bool wave = stamp.Kind == GrassStampKind::Wave;
			frame.Stamps[i * 3] = glm::vec4(stamp.Center, stamp.Yaw);
			frame.Stamps[i * 3 + 1] = glm::vec4(stamp.Extent, stamp.Width, float(stamp.Kind));
			frame.Stamps[i * 3 + 2] = glm::vec4(stamp.Strength, stamp.Heat, wave ? stamp.Facing : stamp.Direction.x, wave ? stamp.HalfArc : stamp.Direction.y);
		}

		const Rdn::RGBuffer command = graph.ImportBuffer("Grass command", m_Command.get());
		const Rdn::RGBuffer blades = graph.ImportBuffer("Grass blades", m_Blades.get());
		const Rdn::RGImage effects = graph.ImportImage("Grass effects", m_EffectImage.get());
		const auto frameWrite = [this, frameIndex]() { return Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_Frame->GetHandle(), m_Frame->GetOffset(frameIndex), sizeof(FrameGpu)); };
		const auto commandWrite = [this]() { return Rdn::BufferWrite(3, Rdn::DescriptorType::StorageBuffer, m_Command->GetHandle(), 0, m_Command->GetSize()); };
		const auto bladeWrite = [this]() { return Rdn::BufferWrite(2, Rdn::DescriptorType::StorageBuffer, m_Blades->GetHandle(), 0, m_Blades->GetSize()); };

		graph.AddPass("Grass reset", Rdn::PassType::Compute,
			[&](Rdn::PassBuilder& builder)
			{
				builder.UseBuffer(command, Rdn::RGBufferUsage::StorageWrite);
				builder.NeverCull();
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
			{
				cmd.BindComputePipeline(m_Reset->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Reset->GetLayout(), 0, Rdn::DescriptorWrite().AddWrite(frameWrite()).AddWrite(commandWrite()));
				cmd.Dispatch(1, 1, 1);
			});
		graph.AddPass("Grass effects", Rdn::PassType::Compute,
			[&](Rdn::PassBuilder& builder)
			{
				builder.UseImage(effects, Rdn::RGImageUsage::StorageReadWrite);
				builder.NeverCull();
			},
			[=, this, size = m_EffectSize](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindComputePipeline(m_Effects->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Effects->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(frameWrite())
					.AddWrite(Rdn::ImageWrite(4, Rdn::DescriptorType::StorageImage, context.GetImageView(effects), Rdn::ImageLayout::General)));
				cmd.DispatchThreads(m_Effects.get(), uint32_t(size.x), uint32_t(size.y));
			});
		graph.AddPass("Grass generate", Rdn::PassType::Compute,
			[&](Rdn::PassBuilder& builder)
			{
				builder.UseBuffer(command, Rdn::RGBufferUsage::StorageReadWrite);
				builder.UseBuffer(blades, Rdn::RGBufferUsage::StorageWrite);
				builder.UseImage(effects, Rdn::RGImageUsage::Sampled);
				builder.NeverCull();
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindComputePipeline(m_Generate->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Generate->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(frameWrite()).AddWrite(commandWrite()).AddWrite(bladeWrite())
					.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::CombinedImageSampler, m_FieldImage->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_FieldSampler->GetHandle()))
					.AddWrite(Rdn::ImageWrite(4, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(effects), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_FieldSampler->GetHandle())));
				cmd.DispatchThreads(m_Generate.get(), uint32_t(count.x), uint32_t(count.y));
			});
		graph.AddPass("Grass finish", Rdn::PassType::Compute,
			[&](Rdn::PassBuilder& builder)
			{
				builder.UseBuffer(command, Rdn::RGBufferUsage::StorageReadWrite);
				builder.NeverCull();
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
			{
				cmd.BindComputePipeline(m_Finish->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Finish->GetLayout(), 0, Rdn::DescriptorWrite().AddWrite(frameWrite()).AddWrite(commandWrite()));
				cmd.Dispatch(1, 1, 1);
			});
		graph.AddPass("GBuffer grass", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				for (const Rdn::RGImage attachment : { gbuffer.Albedo, gbuffer.Surface, gbuffer.Emission })
					builder.AddColorAttachment(attachment, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.SetDepthAttachment(gbuffer.Depth, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.UseBuffer(command, Rdn::RGBufferUsage::Indirect);
				builder.UseBuffer(blades, Rdn::RGBufferUsage::StorageRead);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
			{
				cmd.BindPipeline(m_Draw->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Draw->GetLayout(), 0, Rdn::DescriptorWrite().AddWrite(frameWrite()).AddWrite(bladeWrite()));
				cmd.BindIndexBuffer(m_Indices->GetHandle(), Rdn::IndexType::Uint32);
				for (uint32_t kind = 0; kind < Kinds; kind++)
					cmd.DrawIndexedIndirect(m_Command->GetHandle(), uint64_t(kind) * sizeof(CommandGpu), 1, sizeof(CommandGpu));
			});
	}
}
