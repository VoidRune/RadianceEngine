#include "ShaderCompiler.h"
#include "RadianceEngine/Core/Log.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <string_view>

#include <shaderc/shaderc.hpp>

namespace Rdn::ShaderCompiler
{
	namespace
	{
		struct StageInfo
		{
			std::string_view Extension;
			ShaderStage Stage;
			shaderc_shader_kind Kind;
		};

		constexpr std::array<StageInfo, 7> Stages = { {
			{ ".vert", ShaderStage::Vertex, shaderc_glsl_vertex_shader },
			{ ".frag", ShaderStage::Fragment, shaderc_glsl_fragment_shader },
			{ ".comp", ShaderStage::Compute, shaderc_glsl_compute_shader },
			{ ".rgen", ShaderStage::RayGen, shaderc_glsl_raygen_shader },
			{ ".rahit", ShaderStage::RayAnyHit, shaderc_glsl_anyhit_shader },
			{ ".rchit", ShaderStage::RayClosestHit, shaderc_glsl_closesthit_shader },
			{ ".rmiss", ShaderStage::RayMiss, shaderc_glsl_miss_shader },
		} };

		constexpr size_t MaxIncludeDepth = 32;

		const StageInfo* FindStage(std::string_view extension)
		{
			for (const StageInfo& info : Stages)
				if (info.Extension == extension)
					return &info;
			return nullptr;
		}

		const StageInfo* FindStage(ShaderStage stage)
		{
			for (const StageInfo& info : Stages)
				if (info.Stage == stage)
					return &info;
			return nullptr;
		}

		bool ReadSourceFile(const std::filesystem::path& path, std::string& content, std::vector<ShaderSourceFile>& sourceFiles)
		{
			std::error_code error;
			const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path, error);
			std::ifstream file(path, std::ios::binary);
			if (error || !file)
				return false;

			content.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
			std::string name = path.generic_string();
			if (std::ranges::none_of(sourceFiles, [&](const ShaderSourceFile& source) { return source.Path == name; }))
				sourceFiles.push_back({ std::move(name), writeTime });
			return true;
		}

		struct IncludeResult
		{
			std::string Name;
			std::string Content;
			shaderc_include_result Result = {};
		};

		class Includer final : public shaderc::CompileOptions::IncluderInterface
		{
		public:
			explicit Includer(std::vector<ShaderSourceFile>& sourceFiles)
				: m_SourceFiles(sourceFiles)
			{
			}

			shaderc_include_result* GetInclude(const char* requestedSource, shaderc_include_type, const char* requestingSource, size_t includeDepth) override
			{
				auto* include = new IncludeResult;
				const std::filesystem::path path = (std::filesystem::path(requestingSource).parent_path() / requestedSource).lexically_normal();
				if (includeDepth > MaxIncludeDepth)
					include->Content = std::format("include depth exceeds {} (recursive include?)", MaxIncludeDepth);
				else if (ReadSourceFile(path, include->Content, m_SourceFiles))
					include->Name = path.generic_string();
				else
					include->Content = std::format("cannot open {}", path.generic_string());

				include->Result = { include->Name.c_str(), include->Name.size(), include->Content.c_str(), include->Content.size(), include };
				return &include->Result;
			}

			void ReleaseInclude(shaderc_include_result* result) override
			{
				delete static_cast<IncludeResult*>(result->user_data);
			}

		private:
			std::vector<ShaderSourceFile>& m_SourceFiles;
		};

		const shaderc::Compiler& GetCompiler()
		{
			static const shaderc::Compiler compiler;
			return compiler;
		}

		std::string_view TrimNewlines(std::string_view message)
		{
			while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
				message.remove_suffix(1);
			return message;
		}

		bool CompileSource(const std::string& source, const StageInfo& stage, const std::string& name, ShaderDesc& shaderDesc)
		{
			shaderc::CompileOptions options;
			options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
			options.SetPreserveBindings(true);
#if defined(DEBUG)
			options.SetGenerateDebugInfo();
			options.SetOptimizationLevel(shaderc_optimization_level_zero);
#else
			options.SetOptimizationLevel(shaderc_optimization_level_performance);
#endif
			options.SetIncluder(std::make_unique<Includer>(shaderDesc.SourceFiles));

			const shaderc::SpvCompilationResult result = GetCompiler().CompileGlslToSpv(source, stage.Kind, name.c_str(), options);
			if (result.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				RDN_LOG_ERROR("Failed to compile shader {}:\n{}", name, TrimNewlines(result.GetErrorMessage()));
				return false;
			}
			if (result.GetNumWarnings() > 0)
			{
				RDN_LOG_WARNING("Shader {}:\n{}", name, TrimNewlines(result.GetErrorMessage()));
			}

			shaderDesc.SpirV.assign(result.cbegin(), result.cend());
			shaderDesc.ShaderStage = stage.Stage;
			return true;
		}
	}

	bool Compile(const std::string& filePath, ShaderDesc& shaderDesc)
	{
		shaderDesc.SpirV.clear();
		shaderDesc.SourceFiles.clear();

		const std::filesystem::path path = std::filesystem::path(filePath).lexically_normal();
		const std::string extension = path.extension().string();
		const StageInfo* stage = FindStage(std::string_view(extension));
		if (!stage)
		{
			RDN_LOG_ERROR("Failed to compile shader {}: unknown shader stage extension '{}'", filePath, extension);
			return false;
		}

		std::string source;
		if (!ReadSourceFile(path, source, shaderDesc.SourceFiles))
		{
			RDN_LOG_ERROR("Failed to compile shader {}: cannot open file", filePath);
			return false;
		}

		return CompileSource(source, *stage, path.generic_string(), shaderDesc);
	}

	bool CompileFromSource(const std::string& source, ShaderStage shaderStage, ShaderDesc& shaderDesc, const std::string& debugName)
	{
		shaderDesc.SpirV.clear();
		shaderDesc.SourceFiles.clear();

		const StageInfo* stage = FindStage(shaderStage);
		if (!stage)
		{
			RDN_LOG_ERROR("Failed to compile shader {}: unsupported shader stage {}", debugName, uint32_t(shaderStage));
			return false;
		}
		return CompileSource(source, *stage, debugName, shaderDesc);
	}

	bool AnySourceChanged(std::span<const ShaderSourceFile> sourceFiles)
	{
		for (const ShaderSourceFile& source : sourceFiles)
		{
			std::error_code error;
			const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(source.Path, error);
			if (!error && writeTime != source.WriteTime)
				return true;
		}
		return false;
	}
}
