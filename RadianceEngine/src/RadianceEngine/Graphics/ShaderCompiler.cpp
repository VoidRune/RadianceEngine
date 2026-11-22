#include "ShaderCompiler.h"
#include "RadianceEngine/Core/Log.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>

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

		constexpr uint32_t CacheMagic = 0x56505352;
		constexpr uint32_t CacheVersion = 1;
		constexpr auto CacheLifetime = std::chrono::days(14);

		std::filesystem::path s_CacheDirectory;
		std::mutex s_CacheMutex;

		uint64_t Hash(std::string_view data, uint64_t hash = 0xcbf29ce484222325ull)
		{
			for (const char c : data)
				hash = (hash ^ uint8_t(c)) * 0x100000001b3ull;
			return hash;
		}

		uint64_t HashValue(uint64_t value, uint64_t hash)
		{
			return Hash(std::string_view(reinterpret_cast<const char*>(&value), sizeof(value)), hash);
		}

		std::optional<uint64_t> HashFile(const std::string& path)
		{
			std::ifstream file(path, std::ios::binary);
			if (!file)
				return std::nullopt;
			const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			return Hash(content);
		}

		uint64_t CacheKey(const std::string& source, const StageInfo& stage, const std::string& name)
		{
			unsigned int spvVersion = 0, spvRevision = 0;
			shaderc_get_spv_version(&spvVersion, &spvRevision);
#if defined(DEBUG)
			constexpr uint64_t debug = 1;
#else
			constexpr uint64_t debug = 0;
#endif
			uint64_t key = HashValue(CacheVersion, HashValue(debug, HashValue(uint64_t(stage.Kind), HashValue((uint64_t(spvVersion) << 32) | spvRevision, Hash(name)))));
			return Hash(source, key);
		}

		std::filesystem::path CachePath(uint64_t key)
		{
			return s_CacheDirectory / std::format("{:016x}.spv", key);
		}

		template<typename T>
		bool ReadValue(std::ifstream& file, T& value)
		{
			return bool(file.read(reinterpret_cast<char*>(&value), sizeof(T)));
		}

		template<typename T>
		void WriteValue(std::ofstream& file, const T& value)
		{
			file.write(reinterpret_cast<const char*>(&value), sizeof(T));
		}

		bool LoadCached(uint64_t key, const StageInfo& stage, ShaderDesc& shaderDesc)
		{
			if (s_CacheDirectory.empty())
				return false;
			const std::filesystem::path path = CachePath(key);
			std::ifstream file(path, std::ios::binary);
			uint32_t magic = 0, version = 0, dependencyCount = 0;
			if (!file || !ReadValue(file, magic) || !ReadValue(file, version) || magic != CacheMagic || version != CacheVersion || !ReadValue(file, dependencyCount) || dependencyCount > 256)
				return false;

			std::vector<ShaderSourceFile> dependencies;
			for (uint32_t i = 0; i < dependencyCount; i++)
			{
				uint32_t length = 0;
				uint64_t hash = 0;
				if (!ReadValue(file, length) || length > 4096)
					return false;
				std::string dependency(length, '\0');
				if (!file.read(dependency.data(), length) || !ReadValue(file, hash) || HashFile(dependency) != hash)
					return false;
				std::error_code error;
				const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(dependency, error);
				dependencies.push_back({ std::move(dependency), writeTime });
			}

			uint32_t wordCount = 0;
			if (!ReadValue(file, wordCount) || wordCount == 0 || wordCount > (64u << 20))
				return false;
			std::vector<uint32_t> spirV(wordCount);
			if (!file.read(reinterpret_cast<char*>(spirV.data()), std::streamsize(wordCount) * sizeof(uint32_t)) || spirV[0] != 0x07230203)
				return false;
			file.close();

			std::error_code error;
			std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now(), error);
			for (ShaderSourceFile& dependency : dependencies)
			{
				if (std::ranges::none_of(shaderDesc.SourceFiles, [&](const ShaderSourceFile& source) { return source.Path == dependency.Path; }))
					shaderDesc.SourceFiles.push_back(std::move(dependency));
			}
			shaderDesc.SpirV = std::move(spirV);
			shaderDesc.ShaderStage = stage.Stage;
			return true;
		}

		void StoreCached(uint64_t key, const std::string& name, const ShaderDesc& shaderDesc)
		{
			if (s_CacheDirectory.empty())
				return;
			std::vector<std::pair<const std::string*, uint64_t>> dependencies;
			for (const ShaderSourceFile& source : shaderDesc.SourceFiles)
			{
				if (source.Path == name)
					continue;
				const std::optional<uint64_t> hash = HashFile(source.Path);
				if (!hash)
					return;
				dependencies.emplace_back(&source.Path, *hash);
			}

			const std::filesystem::path path = CachePath(key);
			std::filesystem::path temporary = path;
			temporary += std::format(".{}.tmp", std::hash<std::thread::id>{}(std::this_thread::get_id()) ^ uint64_t(std::chrono::steady_clock::now().time_since_epoch().count()));
			{
				std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
				if (!file)
					return;
				WriteValue(file, CacheMagic);
				WriteValue(file, CacheVersion);
				WriteValue(file, uint32_t(dependencies.size()));
				for (const auto& [dependency, hash] : dependencies)
				{
					WriteValue(file, uint32_t(dependency->size()));
					file.write(dependency->data(), std::streamsize(dependency->size()));
					WriteValue(file, hash);
				}
				WriteValue(file, uint32_t(shaderDesc.SpirV.size()));
				file.write(reinterpret_cast<const char*>(shaderDesc.SpirV.data()), std::streamsize(shaderDesc.SpirV.size() * sizeof(uint32_t)));
				if (!file)
				{
					file.close();
					std::error_code error;
					std::filesystem::remove(temporary, error);
					return;
				}
			}
			std::error_code error;
			std::filesystem::rename(temporary, path, error);
			if (error)
				std::filesystem::remove(temporary, error);
		}

		bool CompileSource(const std::string& source, const StageInfo& stage, const std::string& name, ShaderDesc& shaderDesc)
		{
			const uint64_t key = CacheKey(source, stage, name);
			{
				std::lock_guard lock(s_CacheMutex);
				if (LoadCached(key, stage, shaderDesc))
					return true;
			}

			shaderc::CompileOptions options;
			options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
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
			std::lock_guard lock(s_CacheMutex);
			StoreCached(key, name, shaderDesc);
			return true;
		}
	}

	void SetCacheDirectory(const std::filesystem::path& directory)
	{
		std::lock_guard lock(s_CacheMutex);
		s_CacheDirectory.clear();
		if (directory.empty())
			return;
		std::error_code error;
		std::filesystem::create_directories(directory, error);
		if (error)
		{
			RDN_LOG_WARNING("Shader cache disabled: can't create {} ({})", directory.generic_string(), error.message());
			return;
		}
		s_CacheDirectory = directory;

		const auto cutoff = std::filesystem::file_time_type::clock::now() - CacheLifetime;
		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory, error))
		{
			const std::filesystem::path& path = entry.path();
			const bool cacheFile = path.extension() == ".spv" || path.extension() == ".tmp";
			if (cacheFile && entry.last_write_time(error) < cutoff)
				std::filesystem::remove(path, error);
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
