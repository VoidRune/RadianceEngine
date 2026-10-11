#pragma once
#include "Rendering/ModelData.h"
#include <filesystem>
#include <string>

namespace Game
{
	struct SkyImage
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		std::vector<float> Pixels;
	};

	bool BakeEnvironment(const std::filesystem::path& path, const std::string& name, uint32_t width, ModelEnvironment& environment, std::string& report, std::string& error, SkyImage* sky = nullptr);
	bool WriteSkyImage(const std::filesystem::path& path, const SkyImage& sky);
}
