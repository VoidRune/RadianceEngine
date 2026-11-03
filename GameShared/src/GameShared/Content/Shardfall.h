#pragma once
#include "GameShared/Content/Missions.h"
#include "GameShared/Entity/Components.h"
#include <algorithm>
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
}
