#pragma once
#include "GameShared/Content/Brawl.h"
#include "GameShared/Content/Cosmetics.h"
#include "GameShared/Content/Items.h"
#include <filesystem>
#include <optional>
#include <string>

namespace Game
{
	struct CharacterProfile
	{
		std::string Name;
		StarterKit Kit = StarterKit::Arcanist;
		GearSet Gear;
		BrawlSpells Spells = DefaultBrawlSpells;
		uint32_t Falls = 0;
		uint64_t Starlit = 0;
		uint8_t Honors = 0;
		Cosmetics Style;
	};

	inline constexpr const char* ProfileDirectory = "Saves";

	CharacterProfile MakeNewProfile(std::string name, StarterKit kit);
	std::filesystem::path GetProfilePath(const std::string& name);
	std::optional<CharacterProfile> LoadProfile(const std::filesystem::path& path, std::string& error);
	bool SaveProfile(const std::filesystem::path& path, const CharacterProfile& profile, std::string& error);
	std::string DescribeProfile(const CharacterProfile& profile);
}
