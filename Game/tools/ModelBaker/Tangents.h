#pragma once
#include "Rendering/ModelData.h"
#include <vector>

namespace Game
{
	uint32_t PackTangent(const glm::vec4& tangent);
	void GenerateTangents(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices);
}
