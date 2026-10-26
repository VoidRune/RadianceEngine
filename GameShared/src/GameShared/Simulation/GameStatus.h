#pragma once
#include "GameShared/Content/Brawl.h"
#include "GameShared/Content/Missions.h"
#include "GameShared/Content/Pvp.h"
#include <array>
#include <cstdint>

namespace Game
{
	enum class GamePhase : uint8_t
	{
		Intermission,
		Wave,
		Defeat,
	};

	enum class MissionPhase : uint8_t
	{
		Explore,
		Boss,
		Victory,
		Failed,
	};

	struct GameStatus
	{
		GameMode Mode = GameMode::Horde;
		MapKind Map = MapKind::Town;
		uint8_t Tier = 0;
		ModifierMask Modifiers = 0;
		GamePhase Phase = GamePhase::Intermission;
		uint16_t Wave = 0;
		uint16_t EnemiesLeft = 0;
		float PhaseTime = 0.0f;
		uint8_t PendingChoices = 0;
		MissionPhase Mission = MissionPhase::Explore;
		uint16_t Kills = 0;
		uint16_t KillGoal = 0;
		uint8_t Revives = 0;
		uint32_t BossNetId = 0;
		uint8_t BossHealth = 0;
		float MissionTime = 0.0f;
		MatchPhase Match = MatchPhase::Countdown;
		float MatchTime = 0.0f;
		std::array<uint16_t, MaxTeams> TeamScores{};
		uint32_t Winner = NoPlayer;
		uint8_t WinningTeam = NoTeam;
		uint8_t Alive = 0;
		uint8_t Entrants = 0;
		StormState Storm;
		BrawlStatus Brawl;
		uint16_t HubFalls = 0;
	};
}
