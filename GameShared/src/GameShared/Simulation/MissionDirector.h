#pragma once
#include "GameShared/Core/Math.h"
#include "GameShared/Entity/EntityRegistry.h"
#include "GameShared/Simulation/GameStatus.h"
#include <cstdint>

namespace Game
{
	class World;
	struct Enemy;

	class MissionDirector
	{
	public:
		void Begin(World& world);
		void Update(World& world, float dt);
		void OnKilled(World& world, Vec2 position, bool boss);
		bool ConsumeRevive();
		void Describe(const World& world, GameStatus& status) const;

		MissionPhase GetPhase() const { return m_Phase; }
		float GetPhaseTime() const { return m_PhaseTime; }
		uint32_t GetSpawned() const { return m_Spawned; }
		uint32_t GetKills() const { return m_Kills; }
		uint32_t GetGoal() const { return m_Goal; }
		Entity GetBoss() const { return m_Boss; }

	private:
		void SpawnPacks(World& world);
		void AwakenBoss(World& world);
		void GrantVictory(World& world, Vec2 position);
		void SetPhase(MissionPhase phase);

		MissionPhase m_Phase = MissionPhase::Explore;
		uint32_t m_Spawned = 0;
		uint32_t m_Kills = 0;
		uint32_t m_Goal = 0;
		uint8_t m_Revives = 0;
		Entity m_Boss;
		uint32_t m_BossNetId = 0;
		float m_PhaseTime = 0.0f;
	};
}
