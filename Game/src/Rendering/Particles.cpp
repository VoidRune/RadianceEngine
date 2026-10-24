#include "Particles.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <algorithm>
#include <cstring>
#include <numeric>

namespace Game
{
	namespace
	{
		constexpr uint32_t DispatchOffset = 16;
		constexpr uint32_t DrawOffset = 32;
		constexpr uint32_t ParticleStride = 64;
		constexpr uint32_t LightLayer = 2;

		struct StateGpu
		{
			uint32_t AliveCount = 0;
			uint32_t DeadCount = 0;
			uint32_t NextAliveCount = 0;
			uint32_t Padding = 0;
			glm::uvec4 Dispatch{ 0u, 1u, 1u, 0u };
			glm::uvec4 Draw{ 6u, 0u, 0u, 0u };
		};
		static_assert(offsetof(StateGpu, Dispatch) == DispatchOffset && offsetof(StateGpu, Draw) == DrawOffset);
	}

	ParticleSystem::ParticleSystem(Rdn::Device& device, Rdn::ResourceAllocator& allocator)
		: m_Device(device), m_Allocator(allocator)
	{
		if (!CreatePipelines())
		{
			RDN_LOG_ERROR("The particle shaders failed to compile, particles won't be drawn");
			return;
		}
		const auto deviceBuffer = [&](uint64_t size, Rdn::BufferUsage usage)
		{
			auto buffer = std::make_unique<Rdn::GpuBuffer>();
			m_Allocator.CreateGpuBuffer(buffer.get(), Rdn::GpuBufferDesc{ .Size = size, .UsageFlags = usage | Rdn::BufferUsage::TransferDst, .MemoryProperty = Rdn::MemoryProperty::DeviceLocal });
			return buffer;
		};
		m_State = deviceBuffer(sizeof(StateGpu), Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::IndirectBuffer);
		m_Particles = deviceBuffer(uint64_t(Capacity) * ParticleStride, Rdn::BufferUsage::StorageBuffer);
		m_Lists = deviceBuffer(uint64_t(Capacity) * 3 * sizeof(uint32_t), Rdn::BufferUsage::StorageBuffer);
		const auto ringBuffer = [&](uint32_t size)
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
		m_Frame = ringBuffer(uint32_t(sizeof(FrameGpu)));
		m_SpawnBuffer = ringBuffer(uint32_t(MaxBatchesPerFrame * sizeof(SpawnGpu)));
		m_SpriteBuffer = ringBuffer(uint32_t(MaxSprites * sizeof(SpriteGpu)));
		m_DecalBuffer = ringBuffer(uint32_t(MaxDecals * sizeof(DecalGpu)));
		m_DepthSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_DepthSampler.get(), Rdn::SamplerDesc{ .MinFilter = Rdn::Filter::Nearest, .MagFilter = Rdn::Filter::Nearest, .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		ResetBuffers();
		m_Ready = true;
	}

	ParticleSystem::~ParticleSystem()
	{
		m_Device.WaitIdle();
		for (Rdn::GpuBuffer* buffer : { m_State.get(), m_Particles.get(), m_Lists.get(), m_Materials.get() })
			if (buffer)
				m_Allocator.ReleaseResource(buffer);
		for (Rdn::GpuRingBuffer* buffer : { m_Frame.get(), m_SpawnBuffer.get(), m_SpriteBuffer.get(), m_DecalBuffer.get() })
			if (buffer)
				m_Allocator.ReleaseResource(buffer);
		if (m_DepthSampler)
			m_Allocator.ReleaseResource(m_DepthSampler.get());
		for (Rdn::ComputePipeline* pipeline : { m_Emit.get(), m_Prepare.get(), m_Simulate.get(), m_Finish.get() })
			if (pipeline)
				m_Allocator.ReleaseResource(pipeline);
		for (Rdn::Pipeline* pipeline : { m_Draw.get(), m_SpriteDraw.get(), m_LightDraw.get(), m_SpriteLightDraw.get(), m_DecalDraw.get() })
			if (pipeline)
				m_Allocator.ReleaseResource(pipeline);
		for (const std::unique_ptr<Rdn::Shader>& shader : m_Shaders)
			m_Allocator.ReleaseResource(shader.get());
	}

	std::unique_ptr<Rdn::ComputePipeline> ParticleSystem::CreateCompute(const char* path)
	{
		Rdn::ShaderDesc desc;
		if (!Rdn::ShaderCompiler::Compile(path, desc))
			return nullptr;
		m_Shaders.push_back(std::make_unique<Rdn::Shader>());
		m_Allocator.CreateShader(m_Shaders.back().get(), desc);
		auto pipeline = std::make_unique<Rdn::ComputePipeline>();
		m_Allocator.CreateComputePipeline(pipeline.get(), Rdn::ComputePipelineDesc{ .ComputeShader = m_Shaders.back().get(), .PushDescriptorSets = { 0 } });
		return pipeline;
	}

	Rdn::Shader* ParticleSystem::CompileShader(const char* path)
	{
		Rdn::ShaderDesc desc;
		if (!Rdn::ShaderCompiler::Compile(path, desc))
			return nullptr;
		m_Shaders.push_back(std::make_unique<Rdn::Shader>());
		m_Allocator.CreateShader(m_Shaders.back().get(), desc);
		return m_Shaders.back().get();
	}

	std::unique_ptr<Rdn::Pipeline> ParticleSystem::CreateDraw(Rdn::Shader* vertex, Rdn::Shader* fragment, std::vector<Rdn::Format> formats, std::vector<Rdn::ColorComponent> masks, Rdn::BlendMode blend, std::vector<Rdn::BlendMode> blends)
	{
		auto pipeline = std::make_unique<Rdn::Pipeline>();
		m_Allocator.CreatePipeline(pipeline.get(), Rdn::PipelineDesc{
			.ShaderStages = { vertex, fragment },
			.CullMode = Rdn::CullMode::None,
			.DepthTest = false,
			.DepthWrite = false,
			.Blend = blend,
			.ColorAttachmentFormats = std::move(formats),
			.ColorWriteMasks = std::move(masks),
			.ColorBlendModes = std::move(blends),
			.PushDescriptorSets = { 0 },
		});
		return pipeline;
	}

	bool ParticleSystem::CreatePipelines()
	{
		m_Emit = CreateCompute("res/Shaders/Game/ParticleEmit.comp");
		m_Prepare = CreateCompute("res/Shaders/Game/ParticlePrepare.comp");
		m_Simulate = CreateCompute("res/Shaders/Game/ParticleSimulate.comp");
		m_Finish = CreateCompute("res/Shaders/Game/ParticleFinish.comp");
		Rdn::Shader* vertex = CompileShader("res/Shaders/Game/Particle.vert");
		Rdn::Shader* fragment = CompileShader("res/Shaders/Game/Particle.frag");
		Rdn::Shader* light = CompileShader("res/Shaders/Game/ParticleLight.frag");
		Rdn::Shader* sprite = CompileShader("res/Shaders/Game/Sprite.vert");
		Rdn::Shader* decalVertex = CompileShader("res/Shaders/Game/Decal.vert");
		Rdn::Shader* decalFragment = CompileShader("res/Shaders/Game/Decal.frag");
		if (!m_Emit || !m_Prepare || !m_Simulate || !m_Finish || !vertex || !fragment || !light || !sprite || !decalVertex || !decalFragment)
			return false;
		using enum Rdn::ColorComponent;
		m_Draw = CreateDraw(vertex, fragment, { SceneColorFormat, EmissionFormat });
		m_SpriteDraw = CreateDraw(sprite, fragment, { SceneColorFormat, EmissionFormat });
		m_LightDraw = CreateDraw(vertex, light, { ParticleLightFormat }, {}, Rdn::BlendMode::Additive);
		m_SpriteLightDraw = CreateDraw(sprite, light, { ParticleLightFormat }, {}, Rdn::BlendMode::Additive);
		using enum Rdn::BlendMode;
		m_DecalDraw = CreateDraw(decalVertex, decalFragment, { AlbedoFormat, SurfaceFormat, EmissionFormat }, { R | G | B, B | A, All }, Premultiplied, { Premultiplied, Premultiplied, Max });
		return true;
	}

	void ParticleSystem::ResetBuffers()
	{
		const StateGpu state{ .DeadCount = Capacity };
		m_Allocator.SetDeviceLocalBufferData(m_State.get(), &state, sizeof(state));
		std::vector<uint32_t> lists(size_t(Capacity) * 3, 0u);
		std::iota(lists.begin(), lists.begin() + Capacity, 0u);
		m_Allocator.SetDeviceLocalBufferData(m_Lists.get(), lists.data(), lists.size() * sizeof(uint32_t));
		m_Parity = 0;
	}

	void ParticleSystem::SetMaterials(std::span<const ParticleMaterial> materials)
	{
		if (!m_Ready || materials.empty())
			return;
		m_Device.WaitIdle();
		if (m_Materials)
			m_Allocator.ReleaseResource(m_Materials.get());
		m_Materials = std::make_unique<Rdn::GpuBuffer>();
		m_Allocator.CreateGpuBuffer(m_Materials.get(), Rdn::GpuBufferDesc{ .Size = materials.size_bytes(), .UsageFlags = Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::TransferDst, .MemoryProperty = Rdn::MemoryProperty::DeviceLocal });
		m_Allocator.SetDeviceLocalBufferData(m_Materials.get(), materials.data(), materials.size_bytes());
	}

	void ParticleSystem::Spawn(const ParticleSpawn& spawn)
	{
		if (!m_Ready || spawn.Count == 0 || m_Pending.size() >= MaxBatchesPerFrame || m_QueuedCount >= MaxSpawnsPerFrame)
			return;
		const uint32_t count = std::min(spawn.Count, MaxSpawnsPerFrame - m_QueuedCount);
		m_QueuedCount += count;
		m_Seed = m_Seed * 1664525u + 1013904223u;
		const uint32_t flags = uint32_t(spawn.Shape) | (spawn.Outward ? 16u : 0u);
		m_Pending.push_back({
			.Origin = glm::vec4(spawn.Origin, spawn.Radius),
			.Direction = glm::vec4(spawn.Direction, spawn.Cone),
			.Velocity = glm::vec4(spawn.Velocity, spawn.Speed.x),
			.Ranges = glm::vec4(spawn.Speed.y, spawn.Life.x, spawn.Life.y, spawn.Size.x),
			.Tint = glm::vec4(spawn.Tint, spawn.Size.y),
			.Segment = glm::vec4(spawn.Segment, 0.0f),
			.Info = glm::uvec4(0u, count, spawn.Material, flags | (m_Seed & 0xFFFFFF00u)),
		});
	}

	void ParticleSystem::BeginFrame()
	{
		m_Sprites.clear();
		m_Decals.clear();
	}

	void ParticleSystem::DrawSprite(const ParticleSprite& sprite)
	{
		if (!m_Ready || m_Sprites.size() >= MaxSprites)
			return;
		m_Sprites.push_back({
			.PositionSize = glm::vec4(sprite.Position, sprite.Size),
			.Color = sprite.Color,
			.AxisStretch = glm::vec4(sprite.Axis, sprite.Stretch),
			.Params = sprite.Params,
			.Look = glm::vec4(sprite.Additive, sprite.Emission, sprite.Softness, sprite.Rotation),
			.Info = glm::uvec4(uint32_t(sprite.Shape), uint32_t(sprite.Align), sprite.Seed, 0u),
		});
	}

	void ParticleSystem::DrawDecal(const GroundDecal& decal)
	{
		if (!m_Ready || m_Decals.size() >= MaxDecals)
			return;
		m_Decals.push_back({
			.CenterYaw = glm::vec4(decal.Center, decal.Yaw),
			.Extent = glm::vec4(decal.Extent, decal.Depth, decal.Roughness),
			.Color = decal.Color,
			.Tint = decal.Tint,
			.Params = decal.Params,
			.Info = glm::uvec4(uint32_t(decal.Shape), decal.Seed, 0u, 0u),
		});
	}

	void ParticleSystem::Advance(float dt)
	{
		m_PendingDt += std::max(dt, 0.0f);
	}

	void ParticleSystem::Clear()
	{
		m_ResetRequested = true;
		m_Pending.clear();
		m_QueuedCount = 0;
	}

	void ParticleSystem::Upload(Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera)
	{
		m_Uploaded = m_Ready && m_Materials;
		if (!m_Uploaded)
			return;
		if (m_ResetRequested)
		{
			m_Device.WaitIdle();
			ResetBuffers();
			m_ResetRequested = false;
		}
		const float dt = std::min(m_PendingDt, 0.1f);
		m_PendingDt = 0.0f;
		m_Time = std::fmod(m_Time + dt, 1000.0f);

		uint32_t total = 0;
		for (SpawnGpu& spawn : m_Pending)
		{
			spawn.Info.x = total;
			total += spawn.Info.y;
		}
		const uint32_t batches = uint32_t(m_Pending.size());
		if (batches > 0)
			std::memcpy(static_cast<uint8_t*>(m_SpawnBuffer->GetMappedPtr()) + m_SpawnBuffer->GetOffset(frameIndex), m_Pending.data(), batches * sizeof(SpawnGpu));
		m_Pending.clear();
		m_QueuedCount = 0;
		const uint32_t sprites = uint32_t(m_Sprites.size());
		if (sprites > 0)
			std::memcpy(static_cast<uint8_t*>(m_SpriteBuffer->GetMappedPtr()) + m_SpriteBuffer->GetOffset(frameIndex), m_Sprites.data(), sprites * sizeof(SpriteGpu));
		const uint32_t decals = uint32_t(m_Decals.size());
		if (decals > 0)
			std::memcpy(static_cast<uint8_t*>(m_DecalBuffer->GetMappedPtr()) + m_DecalBuffer->GetOffset(frameIndex), m_Decals.data(), decals * sizeof(DecalGpu));

		const glm::mat4& view = camera.View;
		const float pixelAngle = 2.0f / (float(std::max(extent.Height, 1u)) * std::max(std::abs(camera.Projection[1][1]), 1e-4f));
		const FrameGpu frame{
			.ViewProjection = camera.ViewProjection,
			.InverseViewProjection = glm::inverse(camera.ViewProjection),
			.CameraRight = glm::vec4(view[0][0], view[1][0], view[2][0], m_Time),
			.CameraUp = glm::vec4(view[0][1], view[1][1], view[2][1], dt),
			.CameraForward = glm::vec4(view[0][2], view[1][2], view[2][2], camera.Near),
			.Screen = glm::vec4(float(extent.Width), float(extent.Height), camera.Far, pixelAngle),
			.Counts = glm::uvec4(total, batches, m_Parity, Capacity),
		};
		std::memcpy(static_cast<uint8_t*>(m_Frame->GetMappedPtr()) + m_Frame->GetOffset(frameIndex), &frame, sizeof(frame));
		m_FrameSpawns = total;
		m_FrameBatches = batches;
		m_FrameSprites = sprites;
		m_FrameDecals = decals;
	}

	void ParticleSystem::RenderDecals(Rdn::RenderGraph& graph, const GBuffer& gbuffer, uint32_t frameIndex)
	{
		if (!m_Uploaded || m_FrameDecals == 0)
			return;
		const uint32_t decals = m_FrameDecals;
		graph.AddPass("Decals", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(gbuffer.Albedo, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.AddColorAttachment(gbuffer.Surface, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.AddColorAttachment(gbuffer.Emission, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.UseImage(gbuffer.Depth, Rdn::RGImageUsage::Sampled);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindPipeline(m_DecalDraw->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_DecalDraw->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(Rdn::BufferWrite(4, Rdn::DescriptorType::StorageBuffer, m_Frame->GetHandle(), m_Frame->GetOffset(frameIndex), sizeof(FrameGpu)))
					.AddWrite(Rdn::ImageWrite(5, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(gbuffer.Depth), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_DepthSampler->GetHandle()))
					.AddWrite(Rdn::BufferWrite(6, Rdn::DescriptorType::StorageBuffer, m_DecalBuffer->GetHandle(), m_DecalBuffer->GetOffset(frameIndex), uint64_t(decals) * sizeof(DecalGpu))));
				cmd.Draw(6, decals, 0, 0);
			});
	}

	ParticleSystem::GraphBuffers ParticleSystem::ImportBuffers(Rdn::RenderGraph& graph) const
	{
		return {
			.State = graph.ImportBuffer("Particle state", m_State.get()),
			.Particles = graph.ImportBuffer("Particles", m_Particles.get()),
			.Lists = graph.ImportBuffer("Particle lists", m_Lists.get()),
			.Materials = graph.ImportBuffer("Particle materials", m_Materials.get()),
		};
	}

	void ParticleSystem::Simulate(Rdn::RenderGraph& graph, Rdn::RGImage depth, uint32_t frameIndex)
	{
		m_Simulated = m_Uploaded;
		if (!m_Uploaded)
			return;
		m_Uploaded = false;
		const uint32_t total = m_FrameSpawns;
		const uint32_t batches = m_FrameBatches;
		const GraphBuffers buffers = ImportBuffers(graph);
		const auto frameWrite = [this, frameIndex]() { return Rdn::BufferWrite(4, Rdn::DescriptorType::StorageBuffer, m_Frame->GetHandle(), m_Frame->GetOffset(frameIndex), sizeof(FrameGpu)); };
		const auto stateWrite = [this]() { return Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_State->GetHandle(), 0, m_State->GetSize()); };
		const auto particleWrite = [this]() { return Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, m_Particles->GetHandle(), 0, m_Particles->GetSize()); };
		const auto listWrite = [this]() { return Rdn::BufferWrite(2, Rdn::DescriptorType::StorageBuffer, m_Lists->GetHandle(), 0, m_Lists->GetSize()); };
		const auto materialWrite = [this]() { return Rdn::BufferWrite(3, Rdn::DescriptorType::StorageBuffer, m_Materials->GetHandle(), 0, m_Materials->GetSize()); };

		if (total > 0)
			graph.AddPass("Particles emit", Rdn::PassType::Compute,
				[&](Rdn::PassBuilder& builder)
				{
					builder.UseBuffer(buffers.State, Rdn::RGBufferUsage::StorageReadWrite);
					builder.UseBuffer(buffers.Particles, Rdn::RGBufferUsage::StorageWrite);
					builder.UseBuffer(buffers.Lists, Rdn::RGBufferUsage::StorageReadWrite);
					builder.NeverCull();
				},
				[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
				{
					cmd.BindComputePipeline(m_Emit->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Emit->GetLayout(), 0, Rdn::DescriptorWrite()
						.AddWrite(stateWrite()).AddWrite(particleWrite()).AddWrite(listWrite()).AddWrite(frameWrite())
						.AddWrite(Rdn::BufferWrite(6, Rdn::DescriptorType::StorageBuffer, m_SpawnBuffer->GetHandle(), m_SpawnBuffer->GetOffset(frameIndex), uint64_t(batches) * sizeof(SpawnGpu))));
					cmd.DispatchThreads(m_Emit.get(), total);
				});

		graph.AddPass("Particles prepare", Rdn::PassType::Compute,
			[&](Rdn::PassBuilder& builder)
			{
				builder.UseBuffer(buffers.State, Rdn::RGBufferUsage::StorageReadWrite);
				builder.NeverCull();
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
			{
				cmd.BindComputePipeline(m_Prepare->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Prepare->GetLayout(), 0, Rdn::DescriptorWrite().AddWrite(stateWrite()));
				cmd.Dispatch(1, 1, 1);
			});

		graph.AddPass("Particles simulate", Rdn::PassType::Compute,
			[&](Rdn::PassBuilder& builder)
			{
				builder.UseBuffer(buffers.State, Rdn::RGBufferUsage::StorageReadWrite);
				builder.UseBuffer(buffers.State, Rdn::RGBufferUsage::Indirect);
				builder.UseBuffer(buffers.Particles, Rdn::RGBufferUsage::StorageReadWrite);
				builder.UseBuffer(buffers.Lists, Rdn::RGBufferUsage::StorageReadWrite);
				builder.UseBuffer(buffers.Materials, Rdn::RGBufferUsage::StorageRead);
				builder.UseImage(depth, Rdn::RGImageUsage::Sampled);
				builder.NeverCull();
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindComputePipeline(m_Simulate->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Simulate->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(stateWrite()).AddWrite(particleWrite()).AddWrite(listWrite()).AddWrite(materialWrite()).AddWrite(frameWrite())
					.AddWrite(Rdn::ImageWrite(5, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(depth), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_DepthSampler->GetHandle())));
				cmd.DispatchIndirect(m_State->GetHandle(), DispatchOffset);
			});

		graph.AddPass("Particles finish", Rdn::PassType::Compute,
			[&](Rdn::PassBuilder& builder)
			{
				builder.UseBuffer(buffers.State, Rdn::RGBufferUsage::StorageReadWrite);
				builder.NeverCull();
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
			{
				cmd.BindComputePipeline(m_Finish->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Finish->GetLayout(), 0, Rdn::DescriptorWrite().AddWrite(stateWrite()));
				cmd.Dispatch(1, 1, 1);
			});
		m_Parity ^= 1u;
	}

	void ParticleSystem::RecordDraws(Rdn::CommandBuffer& cmd, const Rdn::PassContext& context, Rdn::RGImage depth, const Rdn::Pipeline& particles, const Rdn::Pipeline& sprites, bool light, uint32_t spriteCount, uint32_t frameIndex) const
	{
		const auto frameWrite = Rdn::BufferWrite(4, Rdn::DescriptorType::StorageBuffer, m_Frame->GetHandle(), m_Frame->GetOffset(frameIndex), sizeof(FrameGpu));
		const auto depthWrite = Rdn::ImageWrite(5, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(depth), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_DepthSampler->GetHandle());
		const auto particleLayer = [&](uint32_t layer)
		{
			cmd.BindPipeline(particles.GetHandle());
			cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, particles.GetLayout(), 0, Rdn::DescriptorWrite()
				.AddWrite(Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, m_Particles->GetHandle(), 0, m_Particles->GetSize()))
				.AddWrite(Rdn::BufferWrite(2, Rdn::DescriptorType::StorageBuffer, m_Lists->GetHandle(), 0, m_Lists->GetSize()))
				.AddWrite(Rdn::BufferWrite(3, Rdn::DescriptorType::StorageBuffer, m_Materials->GetHandle(), 0, m_Materials->GetSize()))
				.AddWrite(frameWrite).AddWrite(depthWrite));
			cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, particles.GetLayout(), &layer, sizeof(layer));
			cmd.DrawIndirect(m_State->GetHandle(), DrawOffset, 1, sizeof(glm::uvec4));
		};
		const auto spriteLayer = [&](uint32_t layer)
		{
			if (spriteCount == 0)
				return;
			cmd.BindPipeline(sprites.GetHandle());
			cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, sprites.GetLayout(), 0, Rdn::DescriptorWrite()
				.AddWrite(frameWrite).AddWrite(depthWrite)
				.AddWrite(Rdn::BufferWrite(6, Rdn::DescriptorType::StorageBuffer, m_SpriteBuffer->GetHandle(), m_SpriteBuffer->GetOffset(frameIndex), uint64_t(spriteCount) * sizeof(SpriteGpu))));
			cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, sprites.GetLayout(), &layer, sizeof(layer));
			cmd.Draw(6, spriteCount, 0, 0);
		};
		if (light)
		{
			particleLayer(LightLayer);
			spriteLayer(LightLayer);
			return;
		}
		particleLayer(0);
		spriteLayer(0);
		particleLayer(1);
	}

	void ParticleSystem::DrawLight(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::RGImage depth, uint32_t frameIndex)
	{
		const bool draw = m_Simulated;
		const uint32_t sprites = m_FrameSprites;
		const GraphBuffers buffers = draw ? ImportBuffers(graph) : GraphBuffers{};
		graph.AddPass("Particle light", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(target, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::Store, { 0.0f, 0.0f, 0.0f, 0.0f });
				if (!draw)
					return;
				builder.UseBuffer(buffers.State, Rdn::RGBufferUsage::Indirect);
				builder.UseBuffer(buffers.Particles, Rdn::RGBufferUsage::StorageRead);
				builder.UseBuffer(buffers.Lists, Rdn::RGBufferUsage::StorageRead);
				builder.UseBuffer(buffers.Materials, Rdn::RGBufferUsage::StorageRead);
				builder.UseImage(depth, Rdn::RGImageUsage::Sampled);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				if (draw)
					RecordDraws(cmd, context, depth, *m_LightDraw, *m_SpriteLightDraw, true, sprites, frameIndex);
			});
	}

	void ParticleSystem::Draw(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, uint32_t frameIndex)
	{
		if (!m_Simulated)
			return;
		m_Simulated = false;
		const uint32_t sprites = m_FrameSprites;
		const GraphBuffers buffers = ImportBuffers(graph);
		graph.AddPass("Particles draw", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(scene, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.AddColorAttachment(emission, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.UseBuffer(buffers.State, Rdn::RGBufferUsage::Indirect);
				builder.UseBuffer(buffers.Particles, Rdn::RGBufferUsage::StorageRead);
				builder.UseBuffer(buffers.Lists, Rdn::RGBufferUsage::StorageRead);
				builder.UseBuffer(buffers.Materials, Rdn::RGBufferUsage::StorageRead);
				builder.UseImage(depth, Rdn::RGImageUsage::Sampled);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				RecordDraws(cmd, context, depth, *m_Draw, *m_SpriteDraw, false, sprites, frameIndex);
			});
	}
}
