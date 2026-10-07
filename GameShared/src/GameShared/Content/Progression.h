#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace Game
{
	inline constexpr uint32_t MaxItemLevel = 30;
	inline constexpr uint32_t MaxMissionTier = 10;
	inline constexpr uint8_t AffixTierCount = 5;
	inline constexpr uint32_t AffixTierLevels[AffixTierCount] = { 1, 6, 12, 18, 25 };
	inline constexpr uint8_t MaxGemLevel = 20;
	inline constexpr uint8_t CorruptedGemLevel = 21;
	inline constexpr uint8_t MaxGemQuality = 20;

	inline float LevelScale(uint32_t level)
	{
		return std::pow(1.075f, float(std::clamp(level, 1u, MaxItemLevel) - 1));
	}

	inline float MonsterDamageScale(uint32_t level)
	{
		return std::pow(LevelScale(level), 0.6f);
	}

	inline float MonsterToughness(uint32_t level)
	{
		return 1.0f + 0.025f * float(std::clamp(level, 1u, MaxItemLevel) - 1);
	}

	inline uint32_t MonsterLevelForTier(uint32_t tier)
	{
		return std::clamp(3u * std::clamp(tier, 1u, MaxMissionTier) - 2u, 1u, MaxItemLevel);
	}

	inline uint8_t MaxAffixTier(uint32_t itemLevel)
	{
		uint8_t tier = 1;
		for (uint8_t i = 0; i < AffixTierCount; i++)
			if (itemLevel >= AffixTierLevels[i])
				tier = uint8_t(i + 1);
		return tier;
	}

	enum class Material : uint8_t
	{
		Gold,
		Shard,
		Starsteel,
		ArcaneDust,
		VoidShard,
		Count,
	};

	struct MaterialDef
	{
		Material Id = Material::Gold;
		const char* Name = "";
		const char* Description = "";
		uint32_t Color = 0xFFFFFF;
	};

	inline constexpr MaterialDef MaterialDefs[] = {
		{ Material::Gold, "Gold", "Every craft costs gold. Monsters drop it and salvaged items give it back.", 0xF2C14E },
		{ Material::Shard, "Forge Shard", "Tempers, imbues and reforges gear. Salvage items you don't need to get more.", 0xE8893F },
		{ Material::Starsteel, "Starsteel", "Hones an affix to a higher tier and ascends an item's rarity. Bosses carry the most.", 0x9CC8FF },
		{ Material::ArcaneDust, "Arcane Dust", "Empowers and polishes skill gems.", 0xC79BFF },
		{ Material::VoidShard, "Void Shard", "Corrupts a skill gem, for better or worse. Very rare.", 0xE45AA9 },
	};
	static_assert(std::size(MaterialDefs) == size_t(Material::Count));

	using MaterialStock = std::array<uint32_t, size_t(Material::Count)>;
	inline constexpr uint32_t MaxMaterialStack = 100000000;

	inline const MaterialDef& GetMaterial(Material material)
	{
		return MaterialDefs[size_t(material) < std::size(MaterialDefs) ? size_t(material) : 0];
	}
}
