#pragma once
#include "GameShared/Content/Pvp.h"
#include "GameShared/Core/Math.h"
#include "GameShared/Entity/EntityRegistry.h"
#include "GameShared/Simulation/GameStatus.h"
#include "GameShared/World/Level.h"
#include <array>
#include <cstdint>
#include <vector>

namespace Game
{
	class World;

	struct MatchScore
	{
		uint16_t Kills = 0;
		uint16_t Deaths = 0;
		uint8_t Placement = 0;
		bool Entered = false;
		bool Out = false;
	};

	class MatchDirector
	{
	public:
		MatchDirector() { m_Teams.fill(NoTeam); }

		void Begin(World& world);
		void Update(World& world, float dt);
		void OnPlayerSpawned(World& world, Entity player, uint32_t playerId);
		void OnPlayerRemoved(World& world, uint32_t playerId);
		void OnPlayerKilled(World& world, Entity player, uint32_t victim, uint32_t killer);
		Vec2 FindSpawn(World& world, uint32_t playerId);
		bool CanRespawn() const { return m_Phase == MatchPhase::Countdown || (m_Phase == MatchPhase::Fight && m_Mode == GameMode::Arena); }
		bool IsFighting() const { return m_Phase == MatchPhase::Fight; }
		void Describe(const World& world, GameStatus& status) const;
		void Watch(World& world, Entity player, int32_t direction);

		MatchPhase GetPhase() const { return m_Phase; }
		float GetPhaseTime() const { return m_PhaseTime; }
		uint8_t GetTeam(uint32_t playerId) const { return playerId < MaxPlayers ? m_Teams[playerId] : NoTeam; }
		bool IsPresent(uint32_t playerId) const { return playerId < MaxPlayers && m_Present[playerId]; }
		const MatchScore& GetScore(uint32_t playerId) const { return m_Scores[playerId < MaxPlayers ? playerId : 0]; }
		const StormState& GetStorm() const { return m_Storm; }
		uint32_t GetWinner() const { return m_Winner; }
		uint8_t GetWinningTeam() const { return m_WinningTeam; }
		uint32_t GetEntrants() const { return m_Entrants; }
		uint32_t CountAlive() const;
		MatchReward GetReward(const World& world, uint32_t playerId) const;
		const std::vector<Vec2>& GetSpawnPoints() const { return m_SpawnPoints; }

	private:
		struct PadState
		{
			PickupPad Pad;
			Entity Loot;
			float Timer = 0.0f;
		};

		void BeginArena(World& world);
		void BeginRoyale(World& world);
		void StartFight(World& world);
		void EndMatch(World& world, uint32_t winner, uint8_t team);
		void UpdateArena(World& world, float dt);
		void UpdateRoyale(World& world, float dt);
		void UpdateStorm(World& world, float dt);
		void UpdateWatchers(World& world);
		void SpawnChest(World& world, Vec2 position, ChestKind kind);
		void SpawnGuardians(World& world, Vec2 position);
		void DropCache(World& world);
		void DropBelongings(World& world, Entity player);
		void PlanStorm(World& world);
		void FinishRoyaleIfDecided(World& world);
		Vec2 RandomPointIn(World& world, Vec2 center, float radius, float clearance) const;
		void SetPhase(MatchPhase phase);

		GameMode m_Mode = GameMode::Arena;
		MatchRules m_Rules;
		MatchPhase m_Phase = MatchPhase::Countdown;
		float m_PhaseTime = 0.0f;
		float m_Clock = 0.0f;
		std::array<uint8_t, MaxPlayers> m_Teams{};
		std::array<bool, MaxPlayers> m_Present{};
		std::array<MatchScore, MaxPlayers> m_Scores{};
		std::array<uint16_t, TeamCount> m_TeamScores{};
		uint32_t m_Winner = NoPlayer;
		uint8_t m_WinningTeam = NoTeam;
		uint32_t m_Entrants = 0;
		std::vector<uint32_t> m_Fallen;
		std::vector<Vec2> m_SpawnPoints;
		std::vector<PadState> m_Pads;
		StormState m_Storm;
		Vec2 m_ShrinkFrom;
		float m_ShrinkFromRadius = 0.0f;
		float m_ShrinkDuration = 1.0f;
		float m_StormTick = 0.0f;
		float m_TimeScale = 1.0f;
		uint16_t m_NextPack = 1;
		uint32_t m_SpawnCounter = 0;
		bool m_Deferring = false;
	};
}
