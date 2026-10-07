#pragma once
#include "GameShared/Content/Props.h"
#include <cstdint>
#include <filesystem>
#include <string>

namespace Game
{
	inline constexpr const char* PropLibraryPath = "res/Data/Props.json";

	bool LoadPropLibrary(const std::filesystem::path& path, std::string& error);
	bool SavePropLibrary(const std::filesystem::path& path, std::string& error);
	void ResetPropLibrary();
	uint32_t GetPropLibraryHash();
	uint32_t CountCustomProps();
}
