#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"
#include <filesystem>
#include <vector>
#include <string>

namespace Rdn
{
	struct ShaderSourceFile
	{
		std::string Path;
		std::filesystem::file_time_type WriteTime;
	};

	struct ShaderDesc
	{
		std::vector<uint32_t> SpirV = {};
		std::string EntryPoint{ "main" };
		ShaderStage ShaderStage = {};
		std::vector<ShaderSourceFile> SourceFiles = {};
	};

	class Shader : NonCopyable
	{
	public:
		ShaderModuleHandle GetHandle() const { return m_Module; }
		const std::string& GetEntryPoint() const { return m_EntryPoint; }
		ShaderStage GetShaderStage() const { return m_ShaderStage; }

	private:
		ShaderModuleHandle m_Module{};
		std::string m_EntryPoint{ "main" };
		ShaderStage m_ShaderStage = {};
		uint32_t m_PushConstantSize = 0;
		uint32_t m_StageOutputs = 0;
		Extent3D m_WorkgroupSize{ 1, 1, 1 };

		struct DescriptorLayoutBinding
		{
			uint32_t SetIndex = 0;
			uint32_t Binding = 0;
			uint32_t DescriptorCount = 0;
			DescriptorType Type = {};
			ShaderStage Stage = {};
			bool IsBindless = false;
		};
		std::vector<DescriptorLayoutBinding> m_LayoutBindings = {};

		friend class ResourceAllocator;
	};

}