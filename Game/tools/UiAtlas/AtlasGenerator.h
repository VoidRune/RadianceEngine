#pragma once
#include "UI/VectorAtlas.h"
#include <string>

namespace Game
{
	bool GenerateVectorAtlas(const VectorAtlasDesc& desc, VectorAtlas& atlas, std::string& error);
}
