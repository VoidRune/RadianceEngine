#include "PostProcess.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <glm/gtc/packing.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

namespace Game
{
	namespace
	{
		constexpr Rdn::Format BloomFormat = Rdn::Format::R16G16B16A16_Sfloat;
		constexpr uint32_t MaxBloomLevels = 6;
		constexpr uint32_t MinBloomSize = 8;
		constexpr uint32_t GradingLutSize = 32;
		constexpr uint32_t FlagGrading = 1;
		constexpr uint32_t FlagRaw = 2;

		struct DownConstants
		{
			glm::vec2 SourceTexel;
			float FirstLevel;
			float Padding;
		};

		struct UpConstants
		{
			glm::vec2 LowerTexel;
			float Spread;
			float Padding;
		};

		struct CompositeConstants
		{
			float BloomStrength;
			float Exposure;
			uint32_t Tonemap;
			uint32_t Flags;
			glm::vec4 Crop;
			glm::vec4 Danger;
		};

		glm::vec3 ApplyGrade(const ColorGrade& grade, const glm::vec3& color)
		{
			constexpr glm::vec3 Rec709{ 0.2126f, 0.7152f, 0.0722f };
			glm::vec3 graded = glm::pow(glm::max(color * grade.Slope + grade.Offset, glm::vec3(0.0f)), grade.Power);
			const float tone = glm::clamp(glm::dot(graded, Rec709), 0.0f, 1.0f);
			const float shadow = 1.0f - glm::smoothstep(0.0f, 0.5f, tone);
			const float highlight = glm::smoothstep(0.5f, 1.0f, tone);
			graded *= glm::mix(glm::vec3(1.0f), grade.Shadows, shadow) * glm::mix(glm::vec3(1.0f), grade.Highlights, highlight);
			const float luma = glm::dot(graded, Rec709);
			const float saturation = grade.Saturation + grade.MidSaturation * (1.0f - std::abs(2.0f * tone - 1.0f));
			return glm::clamp(glm::vec3(luma) + saturation * (graded - glm::vec3(luma)), 0.0f, 1.0f);
		}
	}

	PostProcess::PostProcess(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Format targetFormat)
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
		Rdn::Shader* down = compile("res/Shaders/Game/BloomDown.comp");
		Rdn::Shader* up = compile("res/Shaders/Game/BloomUp.comp");
		Rdn::Shader* vertex = compile("res/Shaders/Game/Composite.vert");
		Rdn::Shader* fragment = compile("res/Shaders/Game/Composite.frag");
		if (!down || !up || !vertex || !fragment)
		{
			RDN_LOG_ERROR("The post-processing shaders failed to compile, the world won't be drawn");
			return;
		}
		m_Down = std::make_unique<Rdn::ComputePipeline>();
		m_Allocator.CreateComputePipeline(m_Down.get(), Rdn::ComputePipelineDesc{ .ComputeShader = down, .PushDescriptorSets = { 0 } });
		m_Up = std::make_unique<Rdn::ComputePipeline>();
		m_Allocator.CreateComputePipeline(m_Up.get(), Rdn::ComputePipelineDesc{ .ComputeShader = up, .PushDescriptorSets = { 0 } });
		m_Composite = std::make_unique<Rdn::Pipeline>();
		m_Allocator.CreatePipeline(m_Composite.get(), Rdn::PipelineDesc{
			.ShaderStages = { vertex, fragment },
			.CullMode = Rdn::CullMode::None,
			.DepthTest = false,
			.DepthWrite = false,
			.ColorAttachmentFormats = { targetFormat },
			.PushDescriptorSets = { 0 },
		});
		m_Sampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_Sampler.get(), Rdn::SamplerDesc{ .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		SetGrade(m_Grade);
	}

	PostProcess::~PostProcess()
	{
		m_Device.WaitIdle();
		for (Rdn::ComputePipeline* pipeline : { m_Down.get(), m_Up.get() })
			if (pipeline)
				m_Allocator.ReleaseResource(pipeline);
		if (m_Composite)
			m_Allocator.ReleaseResource(m_Composite.get());
		if (m_Sampler)
			m_Allocator.ReleaseResource(m_Sampler.get());
		if (m_GradingLut)
			m_Allocator.ReleaseResource(m_GradingLut.get());
		for (const std::unique_ptr<Rdn::Shader>& shader : m_Shaders)
			m_Allocator.ReleaseResource(shader.get());
	}

	void PostProcess::SetGrade(const ColorGrade& grade)
	{
		m_Grade = grade;
		std::vector<glm::vec3> colors;
		colors.reserve(size_t(GradingLutSize) * GradingLutSize * GradingLutSize);
		const float scale = 1.0f / float(GradingLutSize - 1);
		for (uint32_t b = 0; b < GradingLutSize; b++)
			for (uint32_t g = 0; g < GradingLutSize; g++)
				for (uint32_t r = 0; r < GradingLutSize; r++)
					colors.push_back(ApplyGrade(grade, glm::vec3(float(r), float(g), float(b)) * scale));
		UploadGradingLut(GradingLutSize, colors);
	}

	bool PostProcess::LoadGradingLut(const char* path, std::string& error)
	{
		std::ifstream file(path);
		if (!file)
		{
			error = std::string("can't open ") + path;
			return false;
		}
		uint32_t size = 0;
		glm::vec3 low(0.0f);
		glm::vec3 high(1.0f);
		std::vector<glm::vec3> colors;
		std::string line;
		while (std::getline(file, line))
		{
			std::istringstream stream(line);
			std::string key;
			if (!(stream >> key) || key[0] == '#')
				continue;
			if (std::isalpha(uint8_t(key[0])))
			{
				if (key == "LUT_1D_SIZE")
				{
					error = std::string(path) + " is a 1D LUT, the game needs a 3D one";
					return false;
				}
				if (key == "LUT_3D_SIZE")
					stream >> size;
				else if (key == "DOMAIN_MIN")
					stream >> low.r >> low.g >> low.b;
				else if (key == "DOMAIN_MAX")
					stream >> high.r >> high.g >> high.b;
			}
			else
			{
				glm::vec3 color(0.0f);
				std::istringstream values(line);
				if (!(values >> color.r >> color.g >> color.b))
				{
					error = std::string(path) + " has an unreadable line: " + line;
					return false;
				}
				colors.push_back(color);
			}
		}
		if (size < 2 || size > 128 || colors.size() != size_t(size) * size * size)
		{
			error = std::string(path) + " needs LUT_3D_SIZE between 2 and 128 followed by size^3 colors";
			return false;
		}
		if (low != glm::vec3(0.0f) || high != glm::vec3(1.0f))
		{
			error = std::string(path) + " has a domain other than 0..1, which the game doesn't support";
			return false;
		}
		UploadGradingLut(size, colors);
		m_CustomLut = true;
		return true;
	}

	void PostProcess::UploadGradingLut(uint32_t size, const std::vector<glm::vec3>& colors)
	{
		std::vector<uint16_t> texels;
		texels.reserve(colors.size() * 4);
		for (const glm::vec3& color : colors)
		{
			for (int c = 0; c < 3; c++)
				texels.push_back(glm::packHalf1x16(color[c]));
			texels.push_back(glm::packHalf1x16(1.0f));
		}
		if (m_GradingLut)
		{
			m_Device.WaitIdle();
			if (m_GradingLut->GetImageSize().Width != size)
			{
				m_Allocator.ReleaseResource(m_GradingLut.get());
				m_GradingLut.reset();
			}
		}
		if (!m_GradingLut)
		{
			m_GradingLut = std::make_unique<Rdn::GpuImage>();
			m_Allocator.CreateGpuImage(m_GradingLut.get(), Rdn::GpuImageDesc{
				.ImageSize = { size, size, size },
				.Format = Rdn::Format::R16G16B16A16_Sfloat,
				.UsageFlags = Rdn::ImageUsage::Sampled | Rdn::ImageUsage::TransferDst,
			});
		}
		m_Allocator.SetImageData(m_GradingLut.get(), texels.data(), texels.size() * sizeof(uint16_t), Rdn::ImageLayout::ShaderReadOnlyOptimal);
	}

	void PostProcess::Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage target, Rdn::Extent2D extent, Rdn::Extent2D output)
	{
		if (!m_Composite)
			return;
		const auto sampled = [this](uint32_t binding, Rdn::ImageViewHandle view) { return Rdn::ImageWrite(binding, Rdn::DescriptorType::CombinedImageSampler, view, Rdn::ImageLayout::ShaderReadOnlyOptimal, m_Sampler->GetHandle()); };
		const auto storage = [](uint32_t binding, Rdn::ImageViewHandle view) { return Rdn::ImageWrite(binding, Rdn::DescriptorType::StorageImage, view, Rdn::ImageLayout::General); };

		std::vector<Rdn::RGImage> levels;
		std::vector<Rdn::Extent2D> sizes;
		Rdn::RGImage source = emission;
		Rdn::Extent2D sourceSize = extent;
		while (levels.size() < MaxBloomLevels)
		{
			const Rdn::Extent2D size{ std::max((sourceSize.Width + 1) / 2, 1u), std::max((sourceSize.Height + 1) / 2, 1u) };
			if (!levels.empty() && std::min(size.Width, size.Height) < MinBloomSize)
				break;
			const Rdn::RGImage level = graph.CreateImage("Bloom down " + std::to_string(levels.size()), Rdn::RGImageDesc{ .Size = { size.Width, size.Height, 1 }, .Format = BloomFormat });
			const DownConstants constants{ { 1.0f / float(sourceSize.Width), 1.0f / float(sourceSize.Height) }, levels.empty() ? 1.0f : 0.0f, 0.0f };
			graph.AddPass("Bloom down " + std::to_string(levels.size()), Rdn::PassType::Compute,
				[&](Rdn::PassBuilder& builder)
				{
					builder.UseImage(source, Rdn::RGImageUsage::Sampled);
					builder.UseImage(level, Rdn::RGImageUsage::StorageWrite);
				},
				[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
				{
					cmd.BindComputePipeline(m_Down->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Down->GetLayout(), 0, Rdn::DescriptorWrite().AddWrite(sampled(0, context.GetImageView(source))).AddWrite(storage(1, context.GetImageView(level))));
					cmd.PushConstants(Rdn::ShaderStage::Compute, m_Down->GetLayout(), &constants, sizeof(constants));
					cmd.DispatchThreads(m_Down.get(), size.Width, size.Height);
				});
			levels.push_back(level);
			sizes.push_back(size);
			source = level;
			sourceSize = size;
		}

		Rdn::RGImage lower = levels.back();
		for (size_t i = levels.size() - 1; i-- > 0;)
		{
			const Rdn::RGImage current = levels[i];
			const Rdn::Extent2D size = sizes[i];
			const Rdn::RGImage result = graph.CreateImage("Bloom up " + std::to_string(i), Rdn::RGImageDesc{ .Size = { size.Width, size.Height, 1 }, .Format = BloomFormat });
			const UpConstants constants{ { 1.0f / float(sizes[i + 1].Width), 1.0f / float(sizes[i + 1].Height) }, 1.0f, 0.0f };
			graph.AddPass("Bloom up " + std::to_string(i), Rdn::PassType::Compute,
				[&](Rdn::PassBuilder& builder)
				{
					builder.UseImage(lower, Rdn::RGImageUsage::Sampled);
					builder.UseImage(current, Rdn::RGImageUsage::Sampled);
					builder.UseImage(result, Rdn::RGImageUsage::StorageWrite);
				},
				[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
				{
					cmd.BindComputePipeline(m_Up->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Compute, m_Up->GetLayout(), 0, Rdn::DescriptorWrite()
						.AddWrite(sampled(0, context.GetImageView(lower))).AddWrite(sampled(1, context.GetImageView(current))).AddWrite(storage(2, context.GetImageView(result))));
					cmd.PushConstants(Rdn::ShaderStage::Compute, m_Up->GetLayout(), &constants, sizeof(constants));
					cmd.DispatchThreads(m_Up.get(), size.Width, size.Height);
				});
			lower = result;
		}

		const glm::vec2 rendered(float(extent.Width), float(extent.Height));
		const glm::vec2 visible(float(output.Width), float(output.Height));
		const CompositeConstants constants{ BloomStrength, Exposure * SceneExposure * TonemapperExposure[size_t(Tonemap)], uint32_t(Tonemap), (Grading ? FlagGrading : 0u) | (Raw ? FlagRaw : 0u),
			glm::vec4(visible / rendered, glm::floor((rendered - visible) * 0.5f) / rendered), glm::vec4(Vignette, Desaturation, 0.0f, 0.0f) };
		const Rdn::RGImage bloom = lower;
		graph.AddPass("Composite", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(target, Rdn::AttachmentLoadOp::DontCare, Rdn::AttachmentStoreOp::Store);
				builder.UseImage(scene, Rdn::RGImageUsage::Sampled);
				builder.UseImage(bloom, Rdn::RGImageUsage::Sampled);
			},
			[=, this](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindPipeline(m_Composite->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Composite->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(sampled(0, context.GetImageView(scene))).AddWrite(sampled(1, context.GetImageView(bloom))).AddWrite(sampled(2, m_GradingLut->GetImageView())));
				cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Composite->GetLayout(), &constants, sizeof(constants));
				cmd.Draw(3, 1, 0, 0);
			});
	}
}
