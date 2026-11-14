#pragma once
#include "GameShared/Content/Brawl.h"
#include "GameShared/Content/GameMode.h"
#include "GameShared/World/Level.h"
#include "GameShared/Entity/Components.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Game
{
	inline constexpr uint32_t ShardfallMaxPlayers = 4;
	inline constexpr uint32_t ShardfallEnemyCap = 96;
	inline constexpr float ActSeconds = 360.0f;
	inline constexpr float ShardfallPlayerCrowd = 0.35f;
	inline constexpr float SocketNearest = 12.0f;
	inline constexpr float SocketFarthest = 16.0f;
	inline constexpr float SproutSeconds = 0.8f;
	inline constexpr float BroodReach = 5.0f;
	inline constexpr float BroodInterval = 6.0f;
	inline constexpr float ShieldScale = 0.5f;
	inline constexpr float EggHatchSeconds = 4.0f;
	inline constexpr float BurrowSeconds = 15.0f;
	inline constexpr float BurrowWarning = 0.9f;
	inline constexpr uint32_t RubyShards = 6;
	inline constexpr float OrbMagnet = 2.5f;
	inline constexpr float OrbRecall = 45.0f;
	inline constexpr float LevelDamage = 0.03f;
	inline constexpr int32_t LevelHealth = 4;

	enum class RunPhase : uint8_t
	{
		Act,
		Boss,
		Respite,
		Travel,
	};

	struct RunStatus
	{
		RunPhase Phase = RunPhase::Act;
		uint8_t Act = 1;
		float ActClock = 0.0f;
		float RunClock = 0.0f;
		uint16_t Alive = 0;
		uint16_t Goal = 0;
		uint16_t Level = 1;
		uint32_t Xp = 0;
		uint32_t XpGoal = 0;
	};

	inline constexpr EntityKind RunOnlyKinds[] = { EntityKind::QuartzCrawler, EntityKind::JadeCrawler, EntityKind::SapphireCrawler, EntityKind::RubyCrawler, EntityKind::AmethystCrawler, EntityKind::PrismaticCrawler };
	inline constexpr uint8_t MaxStartMinute = 12;
	inline constexpr uint8_t MaxStartLevel = 30;
	inline constexpr uint16_t MaxSpawnRate = 300;
	inline constexpr uint16_t MinXpRate = 25;
	inline constexpr uint16_t MaxXpRate = 500;

	struct RunRules
	{
		uint8_t StartMinute = 0;
		uint8_t StartLevel = 1;
		uint16_t Spawns = 100;
		uint16_t Xp = 100;
		uint8_t Only = 0;
		bool Immortal = false;

		bool operator==(const RunRules&) const = default;
	};

	inline RunRules SanitizeRunRules(RunRules rules)
	{
		rules.StartMinute = std::min(rules.StartMinute, MaxStartMinute);
		rules.StartLevel = std::clamp<uint8_t>(rules.StartLevel, 1, MaxStartLevel);
		rules.Spawns = std::min(rules.Spawns, MaxSpawnRate);
		rules.Xp = std::clamp(rules.Xp, MinXpRate, MaxXpRate);
		rules.Only = rules.Only > std::size(RunOnlyKinds) ? 0 : rules.Only;
		return rules;
	}

	inline uint32_t GetPlayerCap(GameMode mode)
	{
		return mode == GameMode::Shardfall ? ShardfallMaxPlayers : MaxPlayers;
	}

	inline float PartyScale(uint32_t players)
	{
		return 1.0f + ShardfallPlayerCrowd * float(std::max(players, 1u) - 1);
	}

	inline uint32_t XpToNext(uint32_t level, uint32_t players)
	{
		return uint32_t(std::lround(float(8 + 12 * (std::max(level, 1u) - 1)) * PartyScale(players)));
	}

	inline uint32_t GetShardfallPopulation(float actClock, uint32_t players)
	{
		const float progress = std::clamp(actClock / ActSeconds, 0.0f, 1.0f);
		return std::min(uint32_t((8.0f + 22.0f * progress) * PartyScale(players) + 0.5f), ShardfallEnemyCap);
	}

	inline float TrashHealth(float minutes, uint32_t players)
	{
		return (1.0f + 0.2f * minutes + 0.02f * minutes * minutes) * (1.0f + 0.15f * float(std::max(players, 1u) - 1));
	}

	using SpellLoadout = std::array<AbilityId, SkillSlots>;

	struct RunSpellDef
	{
		AbilityId Spell = AbilityId::EmberBolt;
		int32_t Damage = 0;
		float Cooldown = 0.0f;
		float Mana = 0.0f;
	};

	inline constexpr RunSpellDef RunSpells[] = {
		{ AbilityId::EmberBolt, 15, 0.55f, 0.0f },
		{ AbilityId::Comet, 60, 4.5f, 25.0f },
		{ AbilityId::Warp, 0, 9.0f, 15.0f },
		{ AbilityId::GlacialNova, 12, 7.0f, 25.0f },
		{ AbilityId::GravityWell, 30, 5.0f, 25.0f },
		{ AbilityId::ArcLightning, 26, 3.5f, 20.0f },
		{ AbilityId::DiamondWard, 0, 14.0f, 30.0f },
		{ AbilityId::FrostSpear, 20, 3.0f, 15.0f },
		{ AbilityId::ChainHook, 40, 4.0f, 12.0f },
		{ AbilityId::Firebomb, 30, 3.5f, 20.0f },
		{ AbilityId::ObsidianBlades, 7, 1.8f, 20.0f },
		{ AbilityId::StormStrike, 22, 4.0f, 25.0f },
		{ AbilityId::EarthSpike, 30, 5.0f, 25.0f },
		{ AbilityId::PurpleFireball, 21, 4.5f, 20.0f },
		{ AbilityId::HexShield, 0, 0.6f, 6.0f },
	};

	inline const RunSpellDef* FindRunSpell(AbilityId spell)
	{
		for (const RunSpellDef& def : RunSpells)
			if (def.Spell == spell)
				return &def;
		return nullptr;
	}

	inline SpellLoadout MakeRunLoadout(const BrawlSpells& chosen)
	{
		SpellLoadout loadout;
		loadout.fill(AbilityId::Count);
		std::copy(chosen.begin(), chosen.end(), loadout.begin());
		return loadout;
	}

	inline SkillTraits MakeRunSkill(AbilityId id)
	{
		SkillTraits skill = MakeBrawlSkill(id, 1.0f, BrawlMode::Damage);
		if (const RunSpellDef* def = FindRunSpell(id))
		{
			const int32_t base = GetAbility(id).BrawlDamage;
			skill.DamageScale = base > 0 ? float(def->Damage) / float(base) : 1.0f;
			skill.Cooldown = def->Cooldown;
			skill.ManaCost = def->Mana;
		}
		return skill;
	}

	inline PlayerTraits MakeRunTraits(const SpellLoadout& spells, uint32_t level)
	{
		PlayerTraits traits = MakeBrawlTraits(BrawlLoadout{}, BrawlMode::Damage);
		traits.Skills[0] = MakeRunSkill(BrawlBasic);
		for (uint32_t i = 0; i < SkillSlots; i++)
			traits.Skills[i + 1] = IsBrawlSpell(spells[i]) ? MakeRunSkill(spells[i]) : SkillTraits{};
		const uint32_t gained = level > 0 ? level - 1 : 0;
		traits.MaxHealth += LevelHealth * int32_t(gained);
		for (SkillTraits& skill : traits.Skills)
			skill.DamageScale *= 1.0f + LevelDamage * float(gained);
		return traits;
	}
}
