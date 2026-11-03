#pragma once
#include "GameShared/Content/Shardfall.h"
#include "GameShared/Core/Math.h"
#include "GameShared/Simulation/OrbField.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace Game
{
	class World;
	struct EnemyDef;
	struct EnemyScaling;

	class RunDirector
	{
	public:
		void Begin(const RunRules& rules);
		void Update(World& world, float dt);
		void OnKilled(World& world, const EnemyDef& def, Vec2 position, uint64_t seed);
		const RunStatus& GetStatus() const { return m_Status; }
		const RunRules& GetRules() const { return m_Rules; }
		const OrbField& GetOrbs() const { return m_Orbs; }
		uint32_t GetLevel() const { return m_Status.Level; }

	private:
		struct Arrival
		{
			EntityKind Kind = EntityKind::QuartzCrawler;
			Vec2 Position;
			float Delay = 0.0f;
		};

		void Spend(World& world, float dt, uint32_t players);
		void SpendOnly(World& world, EntityKind kind, uint32_t alive, uint32_t pending, uint32_t count);
		void Arrive(World& world, float dt, uint32_t players);
		void Recycle(World& world, float dt);
		void GainXp(World& world, uint32_t xp, uint32_t players);
		std::optional<Vec2> ChooseSocket(World& world, float radius);
		EnemyScaling Scale(uint32_t players) const;

		RunRules m_Rules;
		RunStatus m_Status;
		OrbField m_Orbs;
		std::vector<Arrival> m_Arrivals;
		float m_Budget = 4.0f;
		float m_Treasure = 150.0f;
		std::optional<size_t> m_Saving;
		float m_XpCarry = 0.0f;
		bool m_Started = false;
	};
}
