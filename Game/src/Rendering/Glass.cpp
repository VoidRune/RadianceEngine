#include "Glass.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>

namespace Game
{
	namespace
	{
		constexpr float MaxOffset = 0.07f;
		constexpr int32_t RegionPad = 3;
		constexpr float CrackedFacet = 0.01f;
	}

	GlassRenderer::GlassRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator)
		: m_Device(device), m_Allocator(allocator)
	{
		Rdn::Shader* first = Compile("res/Shaders/Game/GlassPyramid.comp", "FIRST_LEVEL");
		Rdn::Shader* next = Compile("res/Shaders/Game/GlassPyramid.comp");
		Rdn::Shader* vertex = Compile("res/Shaders/Game/Glass.vert", nullptr, Rdn::ShaderStage::Vertex);
		Rdn::Shader* back = Compile("res/Shaders/Game/GlassBack.frag", nullptr, Rdn::ShaderStage::Fragment);
		Rdn::Shader* front = Compile("res/Shaders/Game/Glass.frag", nullptr, Rdn::ShaderStage::Fragment);
		Rdn::Shader* cracked = Compile("res/Shaders/Game/Glass.frag", "CRACKED", Rdn::ShaderStage::Fragment);
		Rdn::Shader* latticeVertex = Compile("res/Shaders/Game/HexShield.vert", nullptr, Rdn::ShaderStage::Vertex);
		Rdn::Shader* lattice = Compile("res/Shaders/Game/HexShield.frag", nullptr, Rdn::ShaderStage::Fragment);
		if (!first || !next || !vertex || !back || !front || !cracked || !latticeVertex || !lattice)
		{
			RDN_LOG_ERROR("The glass shaders failed to compile, refractive objects won't be drawn");
			return;
		}
		m_FirstLevel = std::make_unique<Rdn::ComputePipeline>();
		m_Allocator.CreateComputePipeline(m_FirstLevel.get(), Rdn::ComputePipelineDesc{ .ComputeShader = first, .PushDescriptorSets = { 0 } });
		m_NextLevel = std::make_unique<Rdn::ComputePipeline>();
		m_Allocator.CreateComputePipeline(m_NextLevel.get(), Rdn::ComputePipelineDesc{ .ComputeShader = next, .PushDescriptorSets = { 0 } });
		m_Back = std::make_unique<Rdn::Pipeline>();
		m_Allocator.CreatePipeline(m_Back.get(), Rdn::PipelineDesc{
			.ShaderStages = { vertex, back },
			.CullMode = Rdn::CullMode::None,
			.ColorAttachmentFormats = { GlassBackFormat },
			.DepthAttachmentFormat = DepthFormat,
			.PushDescriptorSets = { 0 },
		});
		for (auto [pipeline, fragment] : { std::pair{ &m_Front, front }, std::pair{ &m_CrackedFront, cracked } })
		{
			*pipeline = std::make_unique<Rdn::Pipeline>();
			m_Allocator.CreatePipeline(pipeline->get(), Rdn::PipelineDesc{
				.ShaderStages = { vertex, fragment },
				.CullMode = Rdn::CullMode::None,
				.Blend = Rdn::BlendMode::Premultiplied,
				.ColorAttachmentFormats = { SceneColorFormat, EmissionFormat },
				.DepthAttachmentFormat = DepthFormat,
				.PushDescriptorSets = { 0 },
			});
		}
		m_Lattice = std::make_unique<Rdn::Pipeline>();
		m_Allocator.CreatePipeline(m_Lattice.get(), Rdn::PipelineDesc{
			.ShaderStages = { latticeVertex, lattice },
			.CullMode = Rdn::CullMode::None,
			.DepthTest = false,
			.DepthWrite = false,
			.Blend = Rdn::BlendMode::Premultiplied,
			.ColorAttachmentFormats = { SceneColorFormat, EmissionFormat },
			.PushDescriptorSets = { 0 },
		});
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
		m_Frame = ring(uint32_t(sizeof(FrameGpu)));
		m_Buffer = ring(uint32_t(MaxFacets * sizeof(FacetGpu)));
		m_ShieldBuffer = ring(uint32_t(MaxShields * sizeof(ShieldGpu)));
		m_CellBuffer = ring(uint32_t(MaxCells * sizeof(CellGpu)));
		m_LinearSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_LinearSampler.get(), Rdn::SamplerDesc{ .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		m_PointSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_PointSampler.get(), Rdn::SamplerDesc{ .MinFilter = Rdn::Filter::Nearest, .MagFilter = Rdn::Filter::Nearest, .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
	}

	GlassRenderer::~GlassRenderer()
	{
		m_Device.WaitIdle();
		for (Rdn::ComputePipeline* pipeline : { m_FirstLevel.get(), m_NextLevel.get() })
			if (pipeline)
				m_Allocator.ReleaseResource(pipeline);
		for (Rdn::Pipeline* pipeline : { m_Back.get(), m_Front.get(), m_CrackedFront.get(), m_Lattice.get() })
			if (pipeline)
				m_Allocator.ReleaseResource(pipeline);
		for (Rdn::GpuRingBuffer* buffer : { m_Frame.get(), m_Buffer.get(), m_ShieldBuffer.get(), m_CellBuffer.get() })
			if (buffer)
				m_Allocator.ReleaseResource(buffer);
		for (Rdn::Sampler* sampler : { m_LinearSampler.get(), m_PointSampler.get() })
			if (sampler)
				m_Allocator.ReleaseResource(sampler);
		for (const std::unique_ptr<Rdn::Shader>& shader : m_Shaders)
			m_Allocator.ReleaseResource(shader.get());
	}

	Rdn::Shader* GlassRenderer::Compile(const char* path, const char* define, Rdn::ShaderStage stage)
	{
		Rdn::ShaderDesc desc;
		if (define ? !CompileVariant(path, define, desc, stage) : !Rdn::ShaderCompiler::Compile(path, desc))
			return nullptr;
		m_Shaders.push_back(std::make_unique<Rdn::Shader>());
		m_Allocator.CreateShader(m_Shaders.back().get(), desc);
		return m_Shaders.back().get();
	}

	void GlassRenderer::BeginFrame()
	{
		m_Facets.clear();
		m_Shields.clear();
		m_Cells.clear();
	}

	uint32_t GlassRenderer::AddShield(const EnergyShield& shield)
	{
		if (!m_Lattice || m_Shields.size() >= MaxShields)
			return UINT32_MAX;
		m_Shields.push_back({
			.Sphere = glm::vec4(shield.Center, shield.Radius),
			.Forward = glm::vec4(shield.Forward, 0.0f),
			.Right = glm::vec4(shield.Right, 0.0f),
			.Rise = glm::vec4(shield.Rise, 0.0f),
			.Color = glm::vec4(shield.Color, shield.Intensity),
			.Impacts = shield.Impacts,
			.Info = glm::uvec4(shield.Seed, glm::floatBitsToUint(shield.Shatter), glm::floatBitsToUint(shield.Lattice), 0u),
		});
		return uint32_t(m_Shields.size() - 1);
	}

	void GlassRenderer::DrawCell(const EnergyCell& cell)
	{
		if (cell.Shield >= m_Shields.size() || m_Cells.size() >= MaxCells || cell.Fade <= 0.0f || cell.Form <= 0.0f)
			return;
		m_Cells.push_back({
			.Center = glm::vec4(cell.Center, cell.Size),
			.Right = glm::vec4(cell.Right, cell.Lift),
			.Up = glm::vec4(cell.Up, cell.Form),
			.Plane = glm::vec4(cell.Plane, cell.Flare, cell.Crack),
			.State = glm::vec4(cell.Flicker, cell.Fade, cell.Spin, 0.0f),
			.Info = glm::uvec4(cell.Shield, cell.Seed, cell.Attached ? 1u : 0u, 0u),
		});
	}

	void GlassRenderer::Draw(const GlassFacet& facet)
	{
		if (!m_Front || m_Facets.size() >= MaxFacets || facet.Fade <= 0.0f)
			return;
		m_Facets.push_back({
			.A = glm::vec4(facet.A, facet.Crack),
			.B = glm::vec4(facet.B, facet.Flash),
			.C = glm::vec4(facet.C, facet.Fade),
			.Absorb = glm::vec4(facet.Absorption, facet.Thickness),
			.Edge = glm::vec4(facet.Edge, facet.Glint),
			.Optics = glm::vec4(facet.Ior, facet.Dispersion, facet.Roughness, facet.Floor),
			.Crack = glm::vec4(facet.CrackOrigin, facet.Wedge, facet.Scatter),
			.Info = glm::uvec4(facet.Seed, 0u, 0u, 0u),
		});
	}

	void GlassRenderer::Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex,
		const WorldCamera& camera, const SceneLighting& lighting, const EnvironmentView& environment, float time)
	{
		if ((m_Facets.empty() && m_Cells.empty()) || !m_Front)
			return;
		glm::vec2 low(1.0f);
		glm::vec2 high(0.0f);
		bool clipped = false;
		const auto include = [&](const glm::vec3& point)
		{
			const glm::vec4 clip = camera.ViewProjection * glm::vec4(point, 1.0f);
			if (clip.w <= camera.Near)
			{
				clipped = true;
				return;
			}
			const glm::vec2 uv = glm::vec2(clip) / clip.w * 0.5f + 0.5f;
			low = glm::min(low, uv);
			high = glm::max(high, uv);
		};
		for (const FacetGpu& facet : m_Facets)
			for (const glm::vec4& corner : { facet.A, facet.B, facet.C })
				include(glm::vec3(corner));
		for (const CellGpu& cell : m_Cells)
		{
			const float reach = cell.Center.w * 1.2f + std::abs(cell.Right.w);
			const glm::vec3 center(cell.Center);
			for (const glm::vec2 corner : { glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, -1.0f), glm::vec2(-1.0f, 1.0f), glm::vec2(1.0f, 1.0f) })
				include(center + (glm::vec3(cell.Right) * corner.x + glm::vec3(cell.Up) * corner.y) * reach);
		}
		if (clipped)
		{
			low = glm::vec2(0.0f);
			high = glm::vec2(1.0f);
		}
		if (low.x >= high.x || low.y >= high.y || high.x <= 0.0f || high.y <= 0.0f || low.x >= 1.0f || low.y >= 1.0f)
			return;
		const glm::vec2 margin(MaxOffset * float(extent.Height) / float(extent.Width), MaxOffset);
		const glm::vec2 regionLow = glm::clamp(low - margin, 0.0f, 1.0f);
		const glm::vec2 regionHigh = glm::clamp(high + margin, 0.0f, 1.0f);

		std::array<Rdn::Extent2D, GlassPyramidLevels> sizes{};
		sizes[0] = HalfExtent(extent);
		for (uint32_t level = 1; level < GlassPyramidLevels; level++)
			sizes[level] = HalfExtent(sizes[level - 1]);
		std::array<glm::ivec2, GlassPyramidLevels> origins{};
		origins[1] = glm::ivec2(0, int32_t(sizes[0].Height));
		for (uint32_t level = 2; level < GlassPyramidLevels; level++)
			origins[level] = origins[level - 1] + glm::ivec2(int32_t(sizes[level - 1].Width), 0);
		const glm::uvec2 atlas(sizes[0].Width, sizes[0].Height + sizes[1].Height);
		std::array<glm::ivec2, GlassPyramidLevels> first{};
		std::array<glm::ivec2, GlassPyramidLevels> last{};
		for (uint32_t level = 0; level < GlassPyramidLevels; level++)
		{
			const glm::vec2 size(float(sizes[level].Width), float(sizes[level].Height));
			first[level] = glm::max(glm::ivec2(glm::floor(regionLow * size)) - RegionPad, glm::ivec2(0));
			last[level] = glm::min(glm::ivec2(glm::ceil(regionHigh * size)) + RegionPad, glm::ivec2(size) - 1);
		}

		const glm::mat4& view = camera.View;
		FrameGpu frame{
			.ViewProjection = camera.ViewProjection,
			.Eye = glm::vec4(camera.Eye, time),
			.Forward = glm::vec4(view[0][2], view[1][2], view[2][2], camera.Near),
			.Screen = glm::vec4(float(extent.Width), float(extent.Height), camera.Far, MaxOffset),
			.Region = glm::vec4(regionLow, regionHigh),
			.Levels = {},
			.Atlas = glm::vec4(1.0f / float(atlas.x), 1.0f / float(atlas.y), 0.0f, 0.0f),
			.SunDirection = glm::vec4(-glm::normalize(lighting.SunDirection), 0.0f),
			.SunRadiance = glm::vec4(lighting.SunColor * lighting.SunIntensity, 0.0f),
			.SkyRadiance = glm::vec4(lighting.SkyColor, 0.0f),
			.GroundRadiance = glm::vec4(lighting.GroundColor, 0.0f),
			.Environment = environment.Params,
		};
		for (uint32_t level = 0; level < GlassPyramidLevels; level++)
			frame.Levels[level] = glm::vec4(glm::vec2(origins[level]) / glm::vec2(atlas), glm::vec2(float(sizes[level].Width), float(sizes[level].Height)) / glm::vec2(atlas));
		std::memcpy(static_cast<uint8_t*>(m_Frame->GetMappedPtr()) + m_Frame->GetOffset(frameIndex), &frame, sizeof(frame));
		const uint32_t count = uint32_t(m_Facets.size());
		const uint32_t intact = uint32_t(std::stable_partition(m_Facets.begin(), m_Facets.end(), [](const FacetGpu& facet) { return facet.A.w <= CrackedFacet; }) - m_Facets.begin());
		std::memcpy(static_cast<uint8_t*>(m_Buffer->GetMappedPtr()) + m_Buffer->GetOffset(frameIndex), m_Facets.data(), count * sizeof(FacetGpu));
		const uint32_t shields = uint32_t(m_Shields.size());
		const uint32_t cells = uint32_t(m_Cells.size());
		std::memcpy(static_cast<uint8_t*>(m_ShieldBuffer->GetMappedPtr()) + m_ShieldBuffer->GetOffset(frameIndex), m_Shields.data(), shields * sizeof(ShieldGpu));
		std::memcpy(static_cast<uint8_t*>(m_CellBuffer->GetMappedPtr()) + m_CellBuffer->GetOffset(frameIndex), m_Cells.data(), cells * sizeof(CellGpu));

		const Rdn::RGImage pyramid = graph.CreateImage("Glass pyramid", Rdn::RGImageDesc{ .Size = { atlas.x, atlas.y, 1 }, .Format = GlassPyramidFormat });
		for (uint32_t level = 0; level < GlassPyramidLevels; level++)
		{
			const PyramidConstants constants{
				.Target = glm::ivec4(origins[level], first[level]),
				.Count = glm::ivec4(last[level] - first[level] + 1, 0, 0),
				.Source = glm::ivec4(level > 0 ? origins[level - 1] : glm::ivec2(0), 0, 0),
				.Bounds = level > 0 ? glm::ivec4(first[level - 1], last[level - 1]) : glm::ivec4(0),
				.Scale = glm::vec4(1.0f / float(sizes[level].Width), 1.0f / float(sizes[level].Height), 0.0f, 0.0f),
			};
			const Rdn::ComputePipeline* pipeline = level == 0 ? m_FirstLevel.get() : m_NextLevel.get();
			graph.AddPass("Glass pyramid " + std::to_string(level), Rdn::PassType::Compute,
				[&](Rdn::PassBuilder& builder)
				{
					if (level == 0)
					{
						builder.UseImage(scene, Rdn::RGImageUsage::Sampled);
						builder.UseImage(pyramid, Rdn::RGImageUsage::StorageWrite);
					}
					else
						builder.UseImage(pyramid, Rdn::RGImageUsage::StorageReadWrite);
				},
				[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
				{
					Rdn::DescriptorWrite write;
					write.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::StorageImage, context.GetImageView(pyramid), Rdn::ImageLayout::General));
					if (level == 0)
						write.AddWrite(Rdn::ImageWrite(0, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(scene), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_LinearSampler->GetHandle()));
					cmd.BindComputePipeline(pipeline->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, pipeline->GetLayout(), 0, write);
					cmd.PushConstants(Rdn::ShaderStage::Compute, pipeline->GetLayout(), &constants, sizeof(constants));
					cmd.DispatchThreads(pipeline, uint32_t(constants.Count.x), uint32_t(constants.Count.y));
				});
		}

		const auto frameWrite = [this, frameIndex]() { return Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_Frame->GetHandle(), m_Frame->GetOffset(frameIndex), sizeof(FrameGpu)); };
		const auto facetWrite = [this, frameIndex, count]() { return Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, m_Buffer->GetHandle(), m_Buffer->GetOffset(frameIndex), uint64_t(count) * sizeof(FacetGpu)); };
		if (count > 0)
		{
			const Rdn::RGImage back = graph.CreateImage("Glass back faces", Rdn::RGImageDesc{ .Size = Rdn::Extent3D(extent), .Format = GlassBackFormat });
			const Rdn::RGImage glassDepth = graph.CreateImage("Glass depth", Rdn::RGImageDesc{ .Size = Rdn::Extent3D(extent), .Format = DepthFormat });
			graph.AddPass("Glass back faces", Rdn::PassType::Graphics,
				[&](Rdn::PassBuilder& builder)
				{
					builder.AddColorAttachment(back, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::Store, { 0.0f, 0.0f, 0.0f, 0.0f });
					builder.SetDepthAttachment(glassDepth, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::Store);
				},
				[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
				{
					const uint32_t pass = 0;
					cmd.BindPipeline(m_Back->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Back->GetLayout(), 0, Rdn::DescriptorWrite().AddWrite(frameWrite()).AddWrite(facetWrite()));
					cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Back->GetLayout(), &pass, sizeof(pass));
					cmd.Draw(3, count, 0, 0);
				});
			const Rdn::ImageWrite environmentWrite(5, Rdn::DescriptorType::CombinedImageSampler, environment.Image->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, environment.Sampler->GetHandle());
			graph.AddPass("Glass", Rdn::PassType::Graphics,
				[&](Rdn::PassBuilder& builder)
				{
					builder.AddColorAttachment(scene, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
					builder.AddColorAttachment(emission, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
					builder.SetDepthAttachment(glassDepth, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::DontCare);
					builder.UseImage(pyramid, Rdn::RGImageUsage::Sampled);
					builder.UseImage(back, Rdn::RGImageUsage::Sampled);
					builder.UseImage(depth, Rdn::RGImageUsage::Sampled);
				},
				[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
				{
					const uint32_t pass = 1;
					const Rdn::DescriptorWrite write = Rdn::DescriptorWrite()
						.AddWrite(frameWrite()).AddWrite(facetWrite())
						.AddWrite(Rdn::ImageWrite(2, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(pyramid), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_LinearSampler->GetHandle()))
						.AddWrite(Rdn::ImageWrite(3, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(back), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_PointSampler->GetHandle()))
						.AddWrite(Rdn::ImageWrite(4, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(depth), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_PointSampler->GetHandle()))
						.AddWrite(environmentWrite);
					for (const auto& [pipeline, first, instances] : { std::tuple(m_Front.get(), 0u, intact), std::tuple(m_CrackedFront.get(), intact, count - intact) })
					{
						if (instances == 0)
							continue;
						cmd.BindPipeline(pipeline->GetHandle());
						cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, pipeline->GetLayout(), 0, write);
						cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, pipeline->GetLayout(), &pass, sizeof(pass));
						cmd.Draw(3, instances, 0, first);
					}
				});
		}
		if (cells == 0)
			return;
		graph.AddPass("Hex shields", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(scene, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.AddColorAttachment(emission, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				builder.UseImage(pyramid, Rdn::RGImageUsage::Sampled);
				builder.UseImage(depth, Rdn::RGImageUsage::Sampled);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindPipeline(m_Lattice->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Lattice->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(frameWrite())
					.AddWrite(Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, m_CellBuffer->GetHandle(), m_CellBuffer->GetOffset(frameIndex), uint64_t(cells) * sizeof(CellGpu)))
					.AddWrite(Rdn::ImageWrite(2, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(pyramid), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_LinearSampler->GetHandle()))
					.AddWrite(Rdn::ImageWrite(4, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(depth), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_PointSampler->GetHandle()))
					.AddWrite(Rdn::BufferWrite(6, Rdn::DescriptorType::StorageBuffer, m_ShieldBuffer->GetHandle(), m_ShieldBuffer->GetOffset(frameIndex), uint64_t(shields) * sizeof(ShieldGpu))));
				cmd.Draw(18, cells, 0, 0);
			});
	}
}
