#pragma once
#include "GameShared/Content/Definitions.h"
#include "GameShared/Content/Progression.h"
#include "GameShared/World/Level.h"
#include <cstdint>
#include <iterator>

namespace Game
{
	enum class GameMode : uint8_t
	{
		Hub,
		Mission,
		Horde,
		Arena,
		Royale,
		Count,
	};

	inline bool IsPvpMode(GameMode mode)
	{
		return mode == GameMode::Arena || mode == GameMode::Royale;
	}

	enum class MissionModifier : uint8_t
	{
		Fortified,
		Brutal,
		Frenzied,
		Champions,
		Barrage,
		Teeming,
		Withered,
		Exposed,
		Drained,
		Count,
	};

	struct ModifierDef
	{
		MissionModifier Id = MissionModifier::Fortified;
		const char* Name = "";
		const char* Description = "";
		bool Penalty = false;
		float Quantity = 0.0f;
		float Rarity = 0.0f;
	};

	inline constexpr ModifierDef MissionModifiers[] = {
		{ MissionModifier::Fortified, "Fortified", "Monsters have 60% more health", false, 0.18f, 0.08f },
		{ MissionModifier::Brutal, "Brutal", "Monsters deal 40% more damage", false, 0.2f, 0.1f },
		{ MissionModifier::Frenzied, "Frenzied", "Monsters move 20% faster and attack 15% more often", false, 0.15f, 0.08f },
		{ MissionModifier::Champions, "Champions", "Elite monsters are twice as common", false, 0.2f, 0.15f },
		{ MissionModifier::Barrage, "Barrage", "Monster projectiles split into one more", false, 0.12f, 0.06f },
		{ MissionModifier::Teeming, "Teeming", "40% more monsters", false, 0.3f, 0.05f },
		{ MissionModifier::Withered, "Withered", "You regenerate and leech 50% less", true, 0.15f, 0.1f },
		{ MissionModifier::Exposed, "Exposed", "You have 20% less resistance and 30% less armor", true, 0.18f, 0.1f },
		{ MissionModifier::Drained, "Drained", "You regenerate mana 40% slower", true, 0.15f, 0.08f },
	};
	static_assert(std::size(MissionModifiers) == size_t(MissionModifier::Count));

	using ModifierMask = uint16_t;
	inline constexpr ModifierMask AllModifiers = ModifierMask((1u << uint32_t(MissionModifier::Count)) - 1u);

	struct MissionSettings
	{
		GameMode Mode = GameMode::Hub;
		MapKind Map = MapKind::Hub;
		uint8_t Tier = 1;
		ModifierMask Modifiers = 0;
		uint32_t Seed = 1;
	};

	struct MissionRewards
	{
		float Quantity = 1.0f;
		float Rarity = 0.0f;
	};

	struct PlayerPenalties
	{
		float Regeneration = 1.0f;
		float Resist = 0.0f;
		float Armor = 1.0f;
		float ManaRegen = 1.0f;
	};

	inline const ModifierDef& GetModifier(MissionModifier modifier)
	{
		return MissionModifiers[size_t(modifier) < std::size(MissionModifiers) ? size_t(modifier) : 0];
	}

	inline bool HasModifier(ModifierMask mask, MissionModifier modifier)
	{
		return (mask & ModifierMask(1u << uint32_t(modifier))) != 0;
	}

	inline uint32_t CountModifiers(ModifierMask mask)
	{
		uint32_t count = 0;
		for (uint32_t i = 0; i < uint32_t(MissionModifier::Count); i++)
			count += (mask >> i) & 1u;
		return count;
	}

	inline MissionRewards GetMissionRewards(uint32_t tier, ModifierMask mask)
	{
		MissionRewards rewards;
		rewards.Rarity = 0.04f * float(std::clamp(tier, 1u, MaxMissionTier) - 1u);
		for (const ModifierDef& modifier : MissionModifiers)
			if (HasModifier(mask, modifier.Id))
			{
				rewards.Quantity += modifier.Quantity;
				rewards.Rarity += modifier.Rarity;
			}
		return rewards;
	}

	inline PlayerPenalties GetPlayerPenalties(ModifierMask mask)
	{
		PlayerPenalties penalties;
		if (HasModifier(mask, MissionModifier::Withered))
			penalties.Regeneration = 0.5f;
		if (HasModifier(mask, MissionModifier::Exposed))
		{
			penalties.Resist = 0.2f;
			penalties.Armor = 0.7f;
		}
		if (HasModifier(mask, MissionModifier::Drained))
			penalties.ManaRegen = 0.6f;
		return penalties;
	}

	inline constexpr float EliteHealthScale = 1.4f;
	inline constexpr float EliteHealthBonus = 200.0f;

	inline EnemyScaling GetMonsterScaling(EntityKind kind, uint32_t level, uint32_t players, bool elite, ModifierMask mask)
	{
		const float crowd = float(players > 0 ? players - 1 : 0);
		const float scale = LevelScale(level);
		const EnemyDef* def = FindEnemyDef(kind);
		const float base = def ? float(std::max(def->Health, 1)) : 60.0f;
		EnemyScaling scaling;
		scaling.Health = scale * (1.0f + 0.6f * crowd) * (elite ? MonsterToughness(level) * (base * EliteHealthScale + EliteHealthBonus) / base : 1.0f) * (HasModifier(mask, MissionModifier::Fortified) ? 1.6f : 1.0f);
		scaling.Damage = MonsterDamageScale(level) * (elite ? 1.5f : 1.0f) * (HasModifier(mask, MissionModifier::Brutal) ? 1.4f : 1.0f);
		scaling.Speed = (elite ? 1.1f : 1.0f) * (HasModifier(mask, MissionModifier::Frenzied) ? 1.2f : 1.0f);
		scaling.Cooldown = HasModifier(mask, MissionModifier::Frenzied) ? 0.85f : 1.0f;
		scaling.Elite = elite;
		scaling.Level = std::clamp(level, 1u, MaxItemLevel);
		scaling.ExtraProjectiles = HasModifier(mask, MissionModifier::Barrage) ? 1 : 0;
		return scaling;
	}
}
