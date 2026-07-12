#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"
#include <vector>
#include <string>

namespace Rdn
{
	struct ShaderDesc
	{
		std::vector<uint32_t> SpirV = {};
		std::string EntryPoint{ "main" };
		ShaderStage ShaderStage = {};
	};

	class Shader
	{
	public:
		ShaderModuleHandle GetHandle() { return m_Module; }
		std::string GetEntryPoint() { return m_EntryPoint; }
		ShaderStage GetShaderStage() { return m_ShaderStage; }

	private:
		ShaderModuleHandle m_Module;
		std::string m_EntryPoint{ "main" };
		ShaderStage m_ShaderStage = {};
		uint32_t m_PushConstantSize = 0;
		uint32_t m_StageOutputs = 0;

		class DescriptorLayoutBinding
		{
		public:
			uint32_t SetIndex;
			uint32_t Binding;
			uint32_t DescriptorCount;
			DescriptorType Type;
			ShaderStage Stage;
			bool IsBindless;
		};
		std::vector<DescriptorLayoutBinding> m_LayoutBindings = {};

		friend class ResourceAllocator;
	};

}