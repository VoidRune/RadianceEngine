#include "RadianceCascades.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace Game
{
	namespace
	{
		constexpr float BaseInterval = 0.125f;
		constexpr float Thickness = 1.0f;
		constexpr float NormalBias = 0.05f;
		constexpr float EmitterRadius = 0.4f;
		constexpr float HitBias = 0.03f;
		constexpr uint32_t BaseSteps = 4;
		constexpr uint32_t Steps = 8;
		constexpr uint32_t MaxCascades = 8;
		constexpr float Pi = 3.14159265f;
		constexpr float DomainTexelScale = 1.15f;
		constexpr float PlaneTolerance = 0.75f;
		constexpr uint32_t MaxDomainSize = 2048;
		constexpr float AnchorPeriod = 256.0f;
		constexpr float BandHeight = 0.2f;
		constexpr uint32_t LockedCascades = 0b1;

		glm::ivec2 PositiveMod(const glm::ivec2& value, int32_t divisor)
		{
			return ((value % divisor) + divisor) % divisor;
		}

		glm::ivec2 FloorDiv(const glm::ivec2& value, int32_t divisor)
		{
			return (value - PositiveMod(value, divisor)) / divisor;
		}

		std::array<glm::vec4, 4> HemisphereSolidAngles()
		{
			constexpr uint32_t Size = RadianceCascades::BaseDirections;
			constexpr uint32_t Rows = Size / 2;
			constexpr uint32_t Samples = 32;
			std::array<glm::vec4, 4> angles{};
			for (uint32_t j = 0; j < Rows; j++)
				for (uint32_t i = 0; i < Size; i++)
				{
					double sum = 0.0;
					for (uint32_t y = 0; y < Samples; y++)
						for (uint32_t x = 0; x < Samples; x++)
						{
							const double a = (double(i) + (double(x) + 0.5) / Samples) / Size * 2.0 - 1.0;
							const double b = (double(j) + (double(y) + 0.5) / Samples) / Rows * 2.0 - 1.0;
							const double px = (a + b) * 0.5;
							const double pz = (a - b) * 0.5;
							const double py = 1.0 - std::abs(px) - std::abs(pz);
							const double length = std::sqrt(px * px + py * py + pz * pz);
							sum += 0.5 / (length * length * length);
						}
					const float angle = float(sum * 4.0 / double(Size * Rows * Samples * Samples));
					angles[j][i] = angle;
					angles[j + Rows][i] = angle;
				}
			return angles;
		}
	}

	RadianceCascades::RadianceCascades(Rdn::Device& device, Rdn::ResourceAllocator& allocator)
		: m_Device(device), m_Allocator(allocator), m_SolidAngles(HemisphereSolidAngles())
	{
		m_Gather = CreatePipeline("res/Shaders/Game/RcGather.comp");
		m_Downsample = CreatePipeline("res/Shaders/Game/RcDownsample.comp");
		m_Place = CreatePipeline("res/Shaders/Game/RcPlace.comp");
		m_Trace = CreatePipeline("res/Shaders/Game/RcTrace.comp");
		m_Resolve = CreatePipeline("res/Shaders/Game/RcResolve.comp");
		m_Apply = CreatePipeline("res/Shaders/Game/RcApply.comp");
		if (!m_Gather || !m_Downsample || !m_Place || !m_Trace || !m_Resolve || !m_Apply)
		{
			RDN_LOG_ERROR("The radiance cascade shaders failed to compile, indirect light falls back to the sky ambient");
			return;
		}
		m_TraceReconnect = CreatePipeline("res/Shaders/Game/RcTrace.comp", "RECONNECT");
		if (!m_TraceReconnect)
			RDN_LOG_ERROR("The reconnecting radiance cascade trace failed to compile, light sources keep their interval rings");
		m_Frame = std::make_unique<Rdn::GpuRingBuffer>();
		m_Allocator.CreateGpuRingBuffer(m_Frame.get(), Rdn::GpuRingBufferDesc{
			.ElementSize = uint32_t(sizeof(FrameGpu)),
			.ElementCount = m_Device.GetFramesInFlightCount(),
			.UsageFlags = Rdn::BufferUsage::UniformBuffer,
			.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
			.Mapped = true,
		});
		m_Nearest = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_Nearest.get(), Rdn::SamplerDesc{ .MinFilter = Rdn::Filter::Nearest, .MagFilter = Rdn::Filter::Nearest, .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		m_Linear = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_Linear.get(), Rdn::SamplerDesc{ .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		m_Ready = true;
	}

	RadianceCascades::~RadianceCascades()
	{
		m_Device.WaitIdle();
		ReleaseHistory();
		for (Rdn::ComputePipeline* pipeline : { m_Gather.get(), m_Downsample.get(), m_Place.get(), m_Trace.get(), m_TraceReconnect.get(), m_Resolve.get(), m_Apply.get() })
			if (pipeline)
				m_Allocator.ReleaseResource(pipeline);
		for (const std::unique_ptr<Rdn::Shader>& shader : m_Shaders)
			m_Allocator.ReleaseResource(shader.get());
		if (m_Frame)
			m_Allocator.ReleaseResource(m_Frame.get());
		for (Rdn::Sampler* sampler : { m_Nearest.get(), m_Linear.get() })
			if (sampler)
				m_Allocator.ReleaseResource(sampler);
	}

	std::unique_ptr<Rdn::ComputePipeline> RadianceCascades::CreatePipeline(const char* path, const char* define)
	{
		Rdn::ShaderDesc desc;
		if (define ? !CompileVariant(path, define, desc) : !Rdn::ShaderCompiler::Compile(path, desc))
			return nullptr;
		m_Shaders.push_back(std::make_unique<Rdn::Shader>());
		m_Allocator.CreateShader(m_Shaders.back().get(), desc);
		auto pipeline = std::make_unique<Rdn::ComputePipeline>();
		m_Allocator.CreateComputePipeline(pipeline.get(), Rdn::ComputePipelineDesc{ .ComputeShader = m_Shaders.back().get(), .PushDescriptorSets = { 0 } });
		return pipeline;
	}

	void RadianceCascades::ReleaseHistory()
	{
		for (ProbeSet& set : m_History)
			for (std::unique_ptr<Rdn::GpuImage>& image : set)
				if (image)
				{
					m_Allocator.ReleaseResource(image.get());
					image.reset();
				}
		m_HistoryGrid = glm::uvec2(0u);
		m_HistoryValid = false;
	}

	void RadianceCascades::EnsureHistory(glm::uvec2 grid, uint32_t spacing)
	{
		if (grid == m_HistoryGrid && spacing == m_HistorySpacing && m_History[0][0])
			return;
		m_Device.WaitIdle();
		ReleaseHistory();
		constexpr Rdn::Format formats[] = { Rdn::Format::R32G32B32A32_Uint, Rdn::Format::R32G32_Uint, Rdn::Format::R16_Sfloat };
		for (ProbeSet& set : m_History)
			for (size_t i = 0; i < set.size(); i++)
			{
				set[i] = std::make_unique<Rdn::GpuImage>();
				m_Allocator.CreateGpuImage(set[i].get(), Rdn::GpuImageDesc{ .ImageSize = { grid.x, grid.y, 1 }, .Format = formats[i], .UsageFlags = Rdn::ImageUsage::Storage | Rdn::ImageUsage::Sampled });
			}
		m_HistoryGrid = grid;
		m_HistorySpacing = spacing;
	}

	glm::vec2 RadianceCascades::AnchorPixel(const WorldCamera& camera, Rdn::Extent2D extent, float height, float period)
	{
		const glm::vec2 size(float(extent.Width), float(extent.Height));
		const glm::vec2 center = size * 0.5f;
		const auto project = [&](const glm::vec3& point, glm::vec2& pixel)
		{
			const glm::vec4 clip = camera.ViewProjection * glm::vec4(point, 1.0f);
			if (clip.w <= 1e-4f)
				return false;
			pixel = (glm::vec2(clip) / clip.w * 0.5f + 0.5f) * size;
			return true;
		};
		glm::vec2 pixel = center;
		const bool visible = m_Anchored && project(m_Anchor, pixel);
		if (visible && glm::all(glm::lessThanEqual(glm::abs(pixel - center), glm::vec2(period))))
			return pixel;
		const glm::vec2 target = visible ? pixel - glm::round((pixel - center) / period) * period : center;
		const float plane = m_Anchored ? m_Anchor.y : height;
		const glm::mat4 inverse = glm::inverse(camera.ViewProjection);
		const glm::vec2 ndc = target / size * 2.0f - 1.0f;
		glm::vec4 nearPoint = inverse * glm::vec4(ndc, 0.0f, 1.0f);
		glm::vec4 farPoint = inverse * glm::vec4(ndc, 1.0f, 1.0f);
		nearPoint /= nearPoint.w;
		farPoint /= farPoint.w;
		const glm::vec3 direction = glm::vec3(farPoint - nearPoint);
		m_Anchored = std::abs(direction.y) > 1e-6f;
		if (m_Anchored)
			m_Anchor = glm::vec3(nearPoint) + direction * ((plane - nearPoint.y) / direction.y);
		return target;
	}

	bool RadianceCascades::UpdateDomain(Rdn::Extent2D extent, const WorldCamera& camera, float focusHeight)
	{
		bool reset = !m_DomainValid || extent.Width != m_DomainExtent.Width || extent.Height != m_DomainExtent.Height;
		if (reset || std::abs(focusHeight - m_Domain.Height) > PlaneTolerance)
		{
			m_Domain.Height = focusHeight;
			reset = true;
		}
		const glm::mat4 inverse = glm::inverse(camera.ViewProjection);
		const auto ground = [&](const glm::vec2& ndc)
		{
			glm::vec4 nearPoint = inverse * glm::vec4(ndc, 0.0f, 1.0f);
			glm::vec4 farPoint = inverse * glm::vec4(ndc, 1.0f, 1.0f);
			nearPoint /= nearPoint.w;
			farPoint /= farPoint.w;
			const glm::vec3 direction = glm::vec3(farPoint - nearPoint);
			const float t = direction.y < -1e-6f ? std::clamp((m_Domain.Height - nearPoint.y) / direction.y, 0.0f, 1.0f) : 1.0f;
			return glm::vec3(nearPoint) + direction * t;
		};
		glm::vec2 low(1e30f);
		glm::vec2 high(-1e30f);
		for (const glm::vec2 corner : { glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, -1.0f), glm::vec2(-1.0f, 1.0f), glm::vec2(1.0f, 1.0f) })
		{
			const glm::vec3 point = ground(corner);
			low = glm::min(low, glm::vec2(point.x, point.z));
			high = glm::max(high, glm::vec2(point.x, point.z));
		}
		const float depth = std::max((camera.View * glm::vec4(ground(glm::vec2(0.0f)), 1.0f)).z, camera.Near);
		const glm::vec2 span = high - low;
		const float texel = std::max(DomainTexelScale * 4.0f * depth / (std::abs(camera.Projection[1][1]) * float(extent.Height)), std::max(span.x, span.y) / float(MaxDomainSize - 16));
		if (reset || std::abs(texel - m_Domain.Texel) > 0.1f * m_Domain.Texel)
		{
			m_Domain.Texel = texel;
			reset = true;
		}
		const glm::uvec2 needed = glm::uvec2(glm::ceil(span / m_Domain.Texel)) + 3u;
		if (reset || glm::any(glm::greaterThan(needed, m_Domain.Size)) || glm::any(glm::greaterThan(m_Domain.Size, needed + needed / 4u + 32u)))
		{
			m_Domain.Size = (glm::uvec2(glm::vec2(needed) * 1.06f) + 15u) / 8u * 8u;
			reset = true;
		}
		m_Domain.Origin = glm::ivec2(glm::floor(low / m_Domain.Texel)) - 1;
		m_DomainExtent = extent;
		m_DomainValid = true;
		return reset;
	}

	Rdn::Extent2D RadianceCascades::LevelExtent(uint32_t level) const
	{
		const uint32_t block = 1u << level;
		return { (m_Domain.Size.x + block - 1) / block + 1, (m_Domain.Size.y + block - 1) / block + 1 };
	}

	glm::ivec2 RadianceCascades::Update(Rdn::Extent2D extent, const WorldCamera& camera, float focusHeight)
	{
		const glm::ivec2 anchor(glm::floor(AnchorPixel(camera, extent, focusHeight, AnchorPeriod)));
		m_DomainReset = UpdateDomain(extent, camera, focusHeight) || m_DomainReset;
		m_HistoryValid = m_HistoryValid && m_Rendered;
		m_Rendered = false;
		return anchor;
	}

	Rdn::RGImage RadianceCascades::CreateSource(Rdn::RenderGraph& graph, Rdn::Extent2D extent) const
	{
		return graph.CreateImage("GI source", Rdn::RGImageDesc{ .Size = { extent.Width, extent.Height, 1 }, .Format = GiRadianceFormat });
	}

	void RadianceCascades::Render(Rdn::RenderGraph& graph, const GBuffer& gbuffer, Rdn::RGImage source, Rdn::RGImage particleLight, Rdn::RGImage backDepth, Rdn::RGImage scene, Rdn::Extent2D extent, uint32_t frameIndex,
		const WorldCamera& camera, const SceneLighting& lighting, const EnvironmentView& environment, GiQuality quality, float fadeHeight, float edgeBand, DebugView view)
	{
		if (!m_Ready || quality == GiQuality::Off || !source || !m_DomainValid)
			return;
		const GiSettings& settings = GiPresets[size_t(quality)];
		const uint32_t cascades = std::clamp(settings.Cascades, 2u, MaxCascades);
		const Rdn::Extent2D baseLevel = LevelExtent(0);
		std::array<glm::vec4, 3> levelOffsets{};
		std::array<glm::ivec4, 3> levelOrigins{};
		glm::uvec2 atlasSize(baseLevel.Width, baseLevel.Height);
		uint32_t column = 0;
		for (uint32_t k = 0; k < LevelCount; k++)
		{
			const glm::vec2 offset(PositiveMod(-m_Domain.Origin, int32_t(1u << k)));
			levelOffsets[k / 2][(k % 2) * 2] = offset.x;
			levelOffsets[k / 2][(k % 2) * 2 + 1] = offset.y;
			if (k == 0)
				continue;
			const Rdn::Extent2D size = LevelExtent(k);
			levelOrigins[k / 2][(k % 2) * 2] = int32_t(baseLevel.Width);
			levelOrigins[k / 2][(k % 2) * 2 + 1] = int32_t(column);
			column += size.Height;
			atlasSize = glm::max(atlasSize, glm::uvec2(baseLevel.Width + size.Width, column));
		}
		const float texelSize = m_Domain.Texel;
		const glm::mat3 rotation(camera.View);
		const glm::vec3 corner(float(m_Domain.Origin.x) * texelSize, m_Domain.Height, float(m_Domain.Origin.y) * texelSize);
		const glm::mat3 basis(rotation * glm::vec3(texelSize, 0.0f, 0.0f), rotation * glm::vec3(0.0f, 0.0f, texelSize), rotation * (corner - camera.Eye));
		const Rdn::Extent2D half = HalfExtent(extent);
		const glm::vec4 lattice(camera.Eye.x / texelSize, camera.Eye.z / texelSize, camera.Eye.y - m_Domain.Height, BandHeight);
		const bool continuous = m_HistoryValid && !m_DomainReset;
		const FrameGpu frame{
			.View = camera.View,
			.InverseView = glm::inverse(camera.View),
			.DomainBasis = glm::mat4(basis),
			.DomainInverse = glm::mat4(glm::inverse(basis)),
			.Projection = glm::vec4(camera.Projection[0][0], camera.Projection[1][1], camera.Near, camera.Far),
			.Screen = glm::vec4(float(extent.Width), float(extent.Height), float(half.Width), float(half.Height)),
			.Domain = glm::vec4(float(m_Domain.Size.x), float(m_Domain.Size.y), texelSize, m_Domain.Height),
			.Sky = glm::vec4(lighting.SkyColor, 0.0f),
			.Ground = glm::vec4(lighting.GroundColor, 0.0f),
			.Trace = glm::vec4(Thickness, NormalBias, EmitterRadius, HitBias),
			.Shading = glm::vec4(fadeHeight, settings.Specular ? 1.0f : 0.0f, BaseInterval * float((2u << (cascades - 1)) - 1u), 0.0f),
			.Options = glm::uvec4(uint32_t(view), 0u, 0u, 0u),
			.SolidAngles = m_SolidAngles,
			.LevelOffsets = levelOffsets,
			.LevelOrigins = levelOrigins,
			.Environment = environment.Params,
			.SkyMaskTransform = environment.SkyMaskTransform,
			.Lattice = lattice,
			.PastLattice = continuous ? m_PastLattice : lattice,
		};
		m_Frame->Write(frameIndex, frame);
		m_PastLattice = lattice;
		const Rdn::ImageWrite environmentWrite(6, Rdn::DescriptorType::CombinedImageSampler, environment.Image->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, environment.Sampler->GetHandle());
		const Rdn::ImageWrite skyMaskWrite(7, Rdn::DescriptorType::CombinedImageSampler, environment.SkyMask->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_Linear->GetHandle());
		Rdn::ImageWrite applyEnvironmentWrite = environmentWrite;
		applyEnvironmentWrite.Binding = 11;
		const Rdn::BufferWrite frameWrite(0, Rdn::DescriptorType::UniformBuffer, m_Frame->GetHandle(), m_Frame->GetOffset(frameIndex), sizeof(FrameGpu));
		const auto sampled = [](const Rdn::PassContext& context, uint32_t binding, Rdn::RGImage image, const Rdn::Sampler& sampler, uint32_t element = 0)
		{
			return Rdn::ImageWrite(binding, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(image), Rdn::ImageLayout::ShaderReadOnlyOptimal, sampler.GetHandle(), element);
		};
		const auto storage = [](const Rdn::PassContext& context, uint32_t binding, Rdn::RGImage image)
		{
			return Rdn::ImageWrite(binding, Rdn::DescriptorType::StorageImage, context.GetImageView(image), Rdn::ImageLayout::General);
		};

		std::array<Rdn::RGImage, LevelCount> radiances{};
		std::array<Rdn::RGImage, LevelCount> emitters{};
		const Rdn::Extent3D baseSize{ baseLevel.Width, baseLevel.Height, 1 };
		const Rdn::RGImage depth = graph.CreateImage("GI depth 0", Rdn::RGImageDesc{ .Size = baseSize, .Format = GiRadianceFormat });
		const Rdn::RGImage heights = graph.CreateImage("GI heights", Rdn::RGImageDesc{ .Size = { atlasSize.x, atlasSize.y, 1 }, .Format = GiHeightFormat });
		radiances[0] = graph.CreateImage("GI radiance 0", Rdn::RGImageDesc{ .Size = baseSize, .Format = GiRadianceFormat });
		emitters[0] = graph.CreateImage("GI emitters 0", Rdn::RGImageDesc{ .Size = baseSize, .Format = GiRadianceFormat });
		const glm::vec4 gatherParams(lighting.ParticleGi, std::max(edgeBand, 1.0f), backDepth ? 1.0f : 0.0f, 0.0f);
		const Rdn::RGImage back = backDepth ? backDepth : gbuffer.Depth;
		graph.AddPass("GI gather", Rdn::PassType::Compute,
			[&](Rdn::PassBuilder& builder)
			{
				for (const Rdn::RGImage image : { source, gbuffer.Depth, gbuffer.Surface, particleLight })
					builder.UseImage(image, Rdn::RGImageUsage::Sampled);
				if (backDepth)
					builder.UseImage(backDepth, Rdn::RGImageUsage::Sampled);
				for (const Rdn::RGImage image : { depth, radiances[0], emitters[0], heights })
					builder.UseImage(image, Rdn::RGImageUsage::StorageWrite);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindComputePipeline(m_Gather->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Gather->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(frameWrite).AddWrite(sampled(context, 1, source, *m_Nearest)).AddWrite(sampled(context, 2, gbuffer.Depth, *m_Nearest)).AddWrite(sampled(context, 3, gbuffer.Surface, *m_Nearest))
					.AddWrite(sampled(context, 4, particleLight, *m_Linear)).AddWrite(storage(context, 5, radiances[0])).AddWrite(storage(context, 6, emitters[0])).AddWrite(storage(context, 7, depth))
					.AddWrite(sampled(context, 8, back, *m_Nearest)).AddWrite(storage(context, 9, heights)));
				cmd.PushConstants(Rdn::ShaderStage::Compute, m_Gather->GetLayout(), &gatherParams, sizeof(gatherParams));
				cmd.DispatchThreads(m_Gather.get(), baseLevel.Width, baseLevel.Height);
			});

		for (uint32_t k = 1; k < LevelCount; k++)
		{
			const Rdn::Extent2D size = LevelExtent(k);
			const Rdn::Extent3D extent3{ size.Width, size.Height, 1 };
			radiances[k] = graph.CreateImage("GI radiance " + std::to_string(k), Rdn::RGImageDesc{ .Size = extent3, .Format = GiRadianceFormat });
			emitters[k] = graph.CreateImage("GI emitters " + std::to_string(k), Rdn::RGImageDesc{ .Size = extent3, .Format = GiRadianceFormat });
			const Rdn::Extent2D previous = LevelExtent(k - 1);
			const glm::ivec2 parity = (PositiveMod(-m_Domain.Origin, int32_t(1u << k)) - PositiveMod(-m_Domain.Origin, int32_t(1u << (k - 1)))) >> int32_t(k - 1);
			const DownsampleConstants constants{
				.Parity = glm::ivec4(parity, 0, 0),
				.Origins = glm::ivec4(levelOrigins[(k - 1) / 2][((k - 1) % 2) * 2], levelOrigins[(k - 1) / 2][((k - 1) % 2) * 2 + 1], levelOrigins[k / 2][(k % 2) * 2], levelOrigins[k / 2][(k % 2) * 2 + 1]),
				.Sizes = glm::ivec4(int32_t(previous.Width), int32_t(previous.Height), int32_t(size.Width), int32_t(size.Height)),
			};
			graph.AddPass("GI pyramid " + std::to_string(k), Rdn::PassType::Compute,
				[&](Rdn::PassBuilder& builder)
				{
					for (const Rdn::RGImage image : { radiances[k - 1], emitters[k - 1] })
						builder.UseImage(image, Rdn::RGImageUsage::Sampled);
					for (const Rdn::RGImage image : { radiances[k], emitters[k] })
						builder.UseImage(image, Rdn::RGImageUsage::StorageWrite);
					builder.UseImage(heights, Rdn::RGImageUsage::StorageReadWrite);
				},
				[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
				{
					cmd.BindComputePipeline(m_Downsample->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Downsample->GetLayout(), 0, Rdn::DescriptorWrite()
						.AddWrite(storage(context, 0, heights)).AddWrite(sampled(context, 1, radiances[k - 1], *m_Nearest)).AddWrite(sampled(context, 2, emitters[k - 1], *m_Nearest))
						.AddWrite(storage(context, 4, radiances[k])).AddWrite(storage(context, 5, emitters[k])));
					cmd.PushConstants(Rdn::ShaderStage::Compute, m_Downsample->GetLayout(), &constants, sizeof(constants));
					cmd.DispatchThreads(m_Downsample.get(), size.Width, size.Height);
				});
		}

		struct Cascade
		{
			glm::uvec2 Grid{ 1u };
			uint32_t Spacing = 1;
			uint32_t Directions = BaseDirections;
			glm::vec2 Offset{ 0.0f };
		};
		const uint32_t baseSpacing = std::max(settings.ProbeSpacing, 2u);
		const glm::ivec2 anchorTexel = 1 - m_Domain.Origin;
		std::vector<Cascade> layout(cascades);
		glm::uvec2 textureSize(1u);
		for (uint32_t i = 0; i < cascades; i++)
		{
			const uint32_t spacing = baseSpacing << i;
			layout[i] = { glm::uvec2((baseLevel.Width + spacing - 1) / spacing + 1, (baseLevel.Height + spacing - 1) / spacing + 1), spacing, BaseDirections << i, glm::vec2(PositiveMod(anchorTexel, int32_t(spacing))) };
			textureSize = glm::max(textureSize, layout[i].Grid * layout[i].Directions);
		}
		const glm::vec2 texel = 1.0f / glm::vec2(textureSize);

		std::vector<Rdn::RGImage> placed(cascades);
		for (uint32_t i = 0; i < cascades; i++)
		{
			const Cascade& cascade = layout[i];
			placed[i] = graph.CreateImage("GI probes " + std::to_string(i), Rdn::RGImageDesc{ .Size = { cascade.Grid.x, cascade.Grid.y, 1 }, .Format = Rdn::Format::R32G32B32A32_Uint });
			const PlaceConstants constants{
				.Lattice = glm::vec4(float(cascade.Spacing), cascade.Offset, (LockedCascades >> i & 1u) != 0 ? 1.0f : 0.0f),
				.Grid = glm::uvec4(cascade.Grid, 0u, 0u),
			};
			const Rdn::RGImage target = placed[i];
			graph.AddPass("GI place " + std::to_string(i), Rdn::PassType::Compute,
				[&](Rdn::PassBuilder& builder)
				{
					builder.UseImage(depth, Rdn::RGImageUsage::Sampled);
					builder.UseImage(heights, Rdn::RGImageUsage::Sampled);
					builder.UseImage(target, Rdn::RGImageUsage::StorageWrite);
				},
				[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
				{
					cmd.BindComputePipeline(m_Place->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Place->GetLayout(), 0, Rdn::DescriptorWrite()
						.AddWrite(frameWrite).AddWrite(sampled(context, 1, depth, *m_Nearest)).AddWrite(sampled(context, 2, heights, *m_Nearest)).AddWrite(storage(context, 3, target)));
					cmd.PushConstants(Rdn::ShaderStage::Compute, m_Place->GetLayout(), &constants, sizeof(constants));
					cmd.DispatchThreads(m_Place.get(), constants.Grid.x, constants.Grid.y);
				});
		}

		std::vector<Rdn::RGImage> merged(cascades);
		for (uint32_t i = cascades; i-- > 0;)
		{
			merged[i] = graph.CreateImage("GI cascade " + std::to_string(i), Rdn::RGImageDesc{ .Size = { textureSize.x, textureSize.y, 1 }, .Format = GiRadianceFormat });
			const bool top = i + 1 == cascades;
			const Cascade& cascade = layout[i];
			const Cascade& upper = top ? cascade : layout[i + 1];
			const TraceConstants constants{
				.Cascade = glm::uvec4(cascade.Directions, i == 0 ? BaseSteps : Steps, top ? 1u : 0u, (i == 0 ? 1u : 0u) | (!top && (LockedCascades >> (i + 1) & 1u) != 0 ? 2u : 0u) | std::max(settings.TraceBudget, 1u) << 8),
				.Grid = glm::uvec4(cascade.Grid, upper.Grid),
				.Interval = glm::vec4(BaseInterval * float((1u << i) - 1u), BaseInterval * float((2u << i) - 1u), float(cascade.Spacing), float(upper.Spacing)),
				.UpperTexel = glm::vec4(texel, float(upper.Directions), std::sqrt(4.0f * Pi) / float(cascade.Directions)),
				.Offsets = glm::vec4(cascade.Offset, upper.Offset),
			};
			const Rdn::RGImage target = merged[i];
			const Rdn::RGImage upperImage = top ? radiances[0] : merged[i + 1];
			const Rdn::RGImage probes = placed[i];
			const Rdn::RGImage upperProbes = top ? placed[i] : placed[i + 1];
			const glm::uvec2 threads = cascade.Grid * cascade.Directions;
			const Rdn::ComputePipeline* trace = !top && (settings.Reconnect >> i & 1u) != 0 && m_TraceReconnect ? m_TraceReconnect.get() : m_Trace.get();
			graph.AddPass("GI cascade " + std::to_string(i), Rdn::PassType::Compute,
				[&](Rdn::PassBuilder& builder)
				{
					for (uint32_t k = 0; k < LevelCount; k++)
						for (const Rdn::RGImage image : { radiances[k], emitters[k] })
							builder.UseImage(image, Rdn::RGImageUsage::Sampled);
					builder.UseImage(heights, Rdn::RGImageUsage::Sampled);
					if (!top)
						for (const Rdn::RGImage image : { upperImage, upperProbes })
							builder.UseImage(image, Rdn::RGImageUsage::Sampled);
					builder.UseImage(probes, Rdn::RGImageUsage::Sampled);
					builder.UseImage(target, Rdn::RGImageUsage::StorageWrite);
				},
				[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
				{
					Rdn::DescriptorWrite write;
					write.AddWrite(frameWrite);
					for (uint32_t k = 0; k < LevelCount; k++)
						write.AddWrite(sampled(context, 2, radiances[k], *m_Linear, k)).AddWrite(sampled(context, 3, emitters[k], *m_Linear, k));
					write.AddWrite(sampled(context, 1, heights, *m_Nearest));
					write.AddWrite(sampled(context, 4, upperImage, *m_Linear)).AddWrite(storage(context, 5, target)).AddWrite(environmentWrite).AddWrite(skyMaskWrite)
						.AddWrite(sampled(context, 8, probes, *m_Nearest)).AddWrite(sampled(context, 9, upperProbes, *m_Nearest));
					cmd.BindComputePipeline(trace->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, trace->GetLayout(), 0, write);
					cmd.PushConstants(Rdn::ShaderStage::Compute, trace->GetLayout(), &constants, sizeof(constants));
					cmd.DispatchThreads(trace, threads.x, threads.y);
				});
		}

		const glm::uvec2 probes = layout[0].Grid;
		EnsureHistory(probes, layout[0].Spacing);
		const glm::ivec2 cell = FloorDiv(m_Domain.Origin + glm::ivec2(layout[0].Offset) - 1, int32_t(layout[0].Spacing));
		const ResolveConstants resolveConstants{
			.Probes = glm::vec4(float(layout[0].Spacing), layout[0].Offset, 0.0f),
			.History = glm::ivec4(cell - m_HistoryCell, m_HistoryValid && !m_DomainReset ? 1 : 0, 0),
		};
		m_HistoryCell = cell;
		m_HistoryValid = true;
		m_DomainReset = false;
		m_Rendered = true;
		const uint32_t written = m_HistoryFrame++ & 1u;
		const auto import = [&](uint32_t set)
		{
			const std::string suffix = set == 0 ? " A" : " B";
			return std::array<Rdn::RGImage, 3>{ graph.ImportImage("GI SH primary" + suffix, m_History[set][0].get()), graph.ImportImage("GI SH secondary" + suffix, m_History[set][1].get()),
				graph.ImportImage("GI probe depth" + suffix, m_History[set][2].get()) };
		};
		const std::array<Rdn::RGImage, 3> sh = import(written);
		const std::array<Rdn::RGImage, 3> past = import(written ^ 1u);
		const Rdn::RGImage base = merged[0];
		const Rdn::RGImage second = merged[1];
		const Rdn::RGImage baseProbes = placed[0];
		const Rdn::RGImage secondProbes = placed[1];
		graph.AddPass("GI resolve", Rdn::PassType::Compute,
			[&](Rdn::PassBuilder& builder)
			{
				builder.UseImage(base, Rdn::RGImageUsage::Sampled);
				builder.UseImage(baseProbes, Rdn::RGImageUsage::Sampled);
				for (const Rdn::RGImage image : past)
					builder.UseImage(image, Rdn::RGImageUsage::Sampled);
				for (const Rdn::RGImage image : sh)
					builder.UseImage(image, Rdn::RGImageUsage::StorageWrite);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindComputePipeline(m_Resolve->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Resolve->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(frameWrite).AddWrite(sampled(context, 1, base, *m_Nearest)).AddWrite(sampled(context, 2, baseProbes, *m_Nearest))
					.AddWrite(storage(context, 3, sh[0])).AddWrite(storage(context, 4, sh[1])).AddWrite(storage(context, 5, sh[2]))
					.AddWrite(sampled(context, 6, past[0], *m_Nearest)).AddWrite(sampled(context, 7, past[1], *m_Nearest)).AddWrite(sampled(context, 8, past[2], *m_Nearest)));
				cmd.PushConstants(Rdn::ShaderStage::Compute, m_Resolve->GetLayout(), &resolveConstants, sizeof(resolveConstants));
				cmd.DispatchThreads(m_Resolve.get(), probes.x, probes.y);
			});

		const ApplyConstants constants{
			.Grid = glm::uvec4(probes, layout[1].Grid),
			.Spacing = glm::vec4(float(layout[0].Spacing), float(layout[1].Spacing), float(LockedCascades & 1u), float(LockedCascades >> 1 & 1u)),
			.Texel = glm::vec4(texel, texel),
			.Offsets = glm::vec4(layout[0].Offset, layout[1].Offset),
		};
		graph.AddPass("GI apply", Rdn::PassType::Compute,
			[&](Rdn::PassBuilder& builder)
			{
				for (const Rdn::RGImage image : { sh[0], sh[1], baseProbes, secondProbes, base, second, gbuffer.Albedo, gbuffer.Surface, gbuffer.Depth })
					builder.UseImage(image, Rdn::RGImageUsage::Sampled);
				builder.UseImage(scene, Rdn::RGImageUsage::StorageReadWrite);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindComputePipeline(m_Apply->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Apply->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(frameWrite)
					.AddWrite(sampled(context, 1, sh[0], *m_Nearest)).AddWrite(sampled(context, 2, sh[1], *m_Nearest)).AddWrite(sampled(context, 3, baseProbes, *m_Nearest))
					.AddWrite(sampled(context, 4, secondProbes, *m_Nearest)).AddWrite(sampled(context, 5, base, *m_Linear)).AddWrite(sampled(context, 6, second, *m_Linear))
					.AddWrite(sampled(context, 7, gbuffer.Albedo, *m_Nearest)).AddWrite(sampled(context, 8, gbuffer.Surface, *m_Nearest)).AddWrite(sampled(context, 9, gbuffer.Depth, *m_Nearest))
					.AddWrite(storage(context, 10, scene)).AddWrite(applyEnvironmentWrite));
				cmd.PushConstants(Rdn::ShaderStage::Compute, m_Apply->GetLayout(), &constants, sizeof(constants));
				cmd.DispatchThreads(m_Apply.get(), extent.Width, extent.Height);
			});
	}
}
