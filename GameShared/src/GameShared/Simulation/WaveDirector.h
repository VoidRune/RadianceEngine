#pragma once
#include "GameShared/Entity/Components.h"
#include "GameShared/Simulation/GameStatus.h"
#include <cstdint>
#include <vector>

namespace Game
{
	class World;

	class WaveDirector
	{
	public:
		WaveDirector(float firstWaveDelay, uint32_t firstWave);

		void Update(World& world, float dt);
		const GameStatus& GetStatus() const { return m_Status; }

	private:
		struct SpawnOrder
		{
			EntityKind Kind = EntityKind::Chaser;
			bool Elite = false;
		};

		void StartWave(World& world, uint32_t players);

		GameStatus m_Status;
		uint16_t m_FirstWave = 1;
		uint32_t m_Players = 1;
		std::vector<SpawnOrder> m_SpawnQueue;
		float m_SpawnTimer = 0.0f;
		float m_HoldTime = 0.0f;
	};
}
