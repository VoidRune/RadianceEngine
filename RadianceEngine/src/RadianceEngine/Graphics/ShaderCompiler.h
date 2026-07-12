#pragma once
#include "Resources/Shader.h"
#include <string>

namespace Rdn::ShaderCompiler
{
	bool Compile(const std::string& filePath, ShaderDesc& shaderDesc);
	bool CompileFromSource(const std::string& source, ShaderStage shaderStage, ShaderDesc& shaderDesc, const std::string& debugName);
}