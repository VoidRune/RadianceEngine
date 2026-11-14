#pragma once
#include "GameShared/Content/Brawl.h"
#include "GameShared/Content/GameMode.h"
#include "GameShared/Content/Pvp.h"
#include "GameShared/Content/Shardfall.h"
#include <array>
#include <cstdint>

namespace Game
{
	struct GameStatus
	{
		GameMode Mode = GameMode::Hub;
		MapKind Map = MapKind::Hub;
		MatchPhase Match = MatchPhase::Countdown;
		float MatchTime = 0.0f;
		std::array<uint16_t, MaxTeams> TeamScores{};
		uint32_t Winner = NoPlayer;
		uint8_t WinningTeam = NoTeam;
		uint8_t Alive = 0;
		uint8_t Entrants = 0;
		BrawlStatus Brawl;
		RunStatus Run;
		uint16_t HubFalls = 0;
	};
}
