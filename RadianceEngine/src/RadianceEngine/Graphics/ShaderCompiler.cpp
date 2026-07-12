#include "ShaderCompiler.h"
#include "RadianceEngine/Core/Log.h"
#include <fstream>
#include <filesystem>

#include <shaderc/shaderc.hpp>

namespace Rdn::ShaderCompiler
{
	bool ReadFile(const std::string& filePath, std::string& dataText)
	{
		std::ifstream in(filePath, std::ios::in | std::ios::binary);
		if (in.is_open())
		{
			in.seekg(0, std::ios::end);
			dataText.resize(in.tellg());
			in.seekg(0, std::ios::beg);
			in.read(dataText.data(), dataText.size());
			in.close();
		}
		else
		{
			return false;
		}
		return true;
	}

	class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface {
	public:
		shaderc_include_result* GetInclude(const char* requested_source,
			shaderc_include_type type,
			const char* requesting_source,
			size_t include_depth) override {

			std::filesystem::path fullPath = requesting_source;
			std::string parentPath = fullPath.parent_path().string();
			std::string path = parentPath + "/" + requested_source;
			std::string dataText;
			if (!ReadFile(parentPath + "/" + requested_source, dataText))
			{
				RDN_LOG_ERROR(std::string("Failed to compile shader: Could not open file ") + path);
				return nullptr;
			}

			auto* result = new shaderc_include_result;
			result->source_name = requested_source;
			result->source_name_length = strlen(requested_source);
			char* content_ptr = new char[dataText.size() + 1];
			strcpy(content_ptr, dataText.c_str());
			result->content = content_ptr;
			result->content_length = dataText.size();
			result->user_data = nullptr;

			return result;
		}

		void ReleaseInclude(shaderc_include_result* data) override {
			delete[] data->content;
			delete data;
		}
	};

	bool Compile(const std::string& filePath, ShaderDesc& shaderDesc)
	{
		std::string dataText;
		if (!ReadFile(filePath, dataText))
		{
			RDN_LOG_ERROR(std::string("Failed to compile shader: Could not open file ") + filePath.c_str());
			return false;
		}

		std::filesystem::path path = filePath;
		std::string extension = path.extension().string();

		auto shaderStage = ShaderStage::Vertex;
		if (extension == ".vert") shaderStage = ShaderStage::Vertex;
		else if (extension == ".frag") shaderStage = ShaderStage::Fragment;
		else if (extension == ".comp") shaderStage = ShaderStage::Compute;
		else if (extension == ".rgen") shaderStage = ShaderStage::RayGen;
		else if (extension == ".rmiss") shaderStage = ShaderStage::RayMiss;
		else if (extension == ".rchit") shaderStage = ShaderStage::RayClosestHit;
		else
		{
			RDN_LOG_ERROR(std::string("Failed to descipher shader stage from file: ") + filePath.c_str());
		}

		return CompileFromSource(dataText, shaderStage, shaderDesc, filePath);
	}

	bool CompileFromSource(const std::string& source, ShaderStage shaderStage, ShaderDesc& shaderDesc, const std::string& debugName)
	{
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetIncluder(std::make_unique<ShaderIncluder>());

		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
		const bool optimize = true;
		if (optimize)
			options.SetOptimizationLevel(shaderc_optimization_level_performance);

		auto shaderStageShaderc = shaderc_glsl_vertex_shader;

		switch (shaderStage)
		{
		case Rdn::ShaderStage::Vertex: shaderStageShaderc = shaderc_glsl_vertex_shader; break;
		case Rdn::ShaderStage::Fragment: shaderStageShaderc = shaderc_glsl_fragment_shader; break;
		case Rdn::ShaderStage::Compute: shaderStageShaderc = shaderc_glsl_compute_shader; break;
		case Rdn::ShaderStage::RayGen: shaderStageShaderc = shaderc_glsl_raygen_shader; break;
		case Rdn::ShaderStage::RayMiss: shaderStageShaderc = shaderc_glsl_miss_shader; break;
		case Rdn::ShaderStage::RayClosestHit: shaderStageShaderc = shaderc_glsl_closesthit_shader; break;
		default:
		{
			RDN_LOG_ERROR(std::string("Unknown shader stage: ") + debugName);
		}
		}

		auto precompileResult = compiler.PreprocessGlsl(source.data(), shaderStageShaderc, debugName.c_str(), options);
		if (precompileResult.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			RDN_LOG_ERROR(std::string("Unknown shader stage: ") + debugName);
		}

		shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source.data(), shaderStageShaderc, debugName.c_str(), options);
		if (module.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			RDN_LOG_ERROR("Failed to compile shader: {}", module.GetErrorMessage());
			return false;
		}

		shaderDesc.SpirV = std::vector<uint32_t>(module.cbegin(), module.cend());
		shaderDesc.ShaderStage = shaderStage;

		return true;
	}

}