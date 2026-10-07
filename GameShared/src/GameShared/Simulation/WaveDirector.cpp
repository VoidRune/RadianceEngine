#include "WaveDirector.h"
#include "GameShared/Simulation/World.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float DefeatDuration = 4.0f;
		constexpr uint32_t MaxAliveEnemies = 22;
		constexpr float MinSpawnDistance = 10.0f;
		constexpr float UpgradeHoldLimit = 25.0f;

		float SpawnInterval(uint32_t wave)
		{
			return std::max(1.1f - 0.03f * float(wave), 0.5f);
		}
	}

	WaveDirector::WaveDirector(float firstWaveDelay, uint32_t firstWave)
		: m_FirstWave(uint16_t(std::clamp(firstWave, 1u, 99u)))
	{
		m_Status.Wave = uint16_t(m_FirstWave - 1);
		m_Status.PhaseTime = firstWaveDelay;
	}

	void WaveDirector::Update(World& world, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		uint32_t players = 0;
		uint32_t alivePlayers = 0;
		entities.Each<PlayerController>([&](Entity, PlayerController& controller)
		{
			players++;
			alivePlayers += controller.State.Alive ? 1 : 0;
		});
		uint32_t aliveEnemies = 0;
		entities.Each<Enemy>([&](Entity entity, Enemy&)
		{
			if (const Health* health = entities.TryGet<Health>(entity); health && !health->IsDead())
				aliveEnemies++;
		});

		switch (m_Status.Phase)
		{
		case GamePhase::Intermission:
		{
			const uint32_t pending = world.CountPendingUpgrades();
			m_Status.PendingChoices = uint8_t(std::min(pending, 255u));
			if (pending > 0 && m_HoldTime < UpgradeHoldLimit)
			{
				m_HoldTime += dt;
				break;
			}
			if (players > 0 && (m_Status.PhaseTime -= dt) <= 0.0f)
				StartWave(world, players);
			break;
		}
		case GamePhase::Wave:
			if (players > 0 && alivePlayers == 0)
			{
				m_Status.Phase = GamePhase::Defeat;
				m_Status.PhaseTime = DefeatDuration;
				m_SpawnQueue.clear();
				world.Emit({ WorldEventType::Defeat });
				break;
			}
			m_SpawnTimer -= dt;
			if (m_SpawnTimer <= 0.0f && !m_SpawnQueue.empty() && aliveEnemies < MaxAliveEnemies)
			{
				const SpawnOrder order = m_SpawnQueue.back();
				const EnemyDef* def = FindEnemyDef(order.Kind);
				const float radius = def ? def->Radius * (order.Elite ? EliteSize : 1.0f) : 0.0f;
				if (const std::optional<Vec2> position = world.FindEnemySpawn(MinSpawnDistance, radius))
				{
					m_SpawnQueue.pop_back();
					world.SpawnEnemy(order.Kind, *position, GetEnemyScaling(m_Status.Wave, m_Players, order.Elite));
					m_SpawnTimer = SpawnInterval(m_Status.Wave);
					aliveEnemies++;
				}
			}
			if (m_SpawnQueue.empty() && aliveEnemies == 0)
			{
				m_Status.Phase = GamePhase::Intermission;
				m_Status.PhaseTime = world.GetSettings().IntermissionSeconds;
				m_HoldTime = 0.0f;
				world.Emit({ WorldEventType::WaveCleared });
				const uint32_t every = world.GetSettings().UpgradeEvery;
				if (every > 0 && m_Status.Wave % every == 0)
					world.OfferUpgrades();
			}
			break;
		case GamePhase::Defeat:
			if ((m_Status.PhaseTime -= dt) <= 0.0f && world.GetSettings().RestartOnDefeat)
			{
				world.ResetRun();
				m_Status = { .Phase = GamePhase::Intermission, .Wave = uint16_t(m_FirstWave - 1), .PhaseTime = world.GetSettings().IntermissionSeconds };
				m_HoldTime = 0.0f;
				aliveEnemies = 0;
			}
			break;
		}
		m_Status.EnemiesLeft = uint16_t(std::min<size_t>(aliveEnemies + m_SpawnQueue.size(), UINT16_MAX));
	}

	void WaveDirector::StartWave(World& world, uint32_t players)
	{
		const uint32_t wave = ++m_Status.Wave;
		m_Status.Phase = GamePhase::Wave;
		m_Status.PhaseTime = 0.0f;
		m_Status.PendingChoices = 0;
		m_Players = std::max(players, 1u);

		const float crowd = 0.6f + 0.4f * float(m_Players);
		const auto count = [&](float base) { return uint32_t(std::lround(base * crowd)); };
		const auto introduce = [&](uint32_t from, float base) { return wave >= from ? std::max(count(base), 1u) : 0u; };
		const float eliteChance = wave >= 4 ? std::min(0.04f * float(wave - 3), 0.35f) : 0.0f;
		Random& random = world.GetRandom();
		m_SpawnQueue.clear();
		const auto add = [&](EntityKind kind, uint32_t amount)
		{
			for (uint32_t i = 0; i < amount; i++)
				m_SpawnQueue.push_back({ kind, random.Float() < eliteChance });
		};
		const float w = float(wave);
		add(EntityKind::Chaser, count(2.0f + 1.0f * w + 0.04f * w * w));
		add(EntityKind::Shooter, introduce(2, 0.4f * (w - 1.0f) + 0.015f * w * w));
		add(EntityKind::Brute, introduce(3, 0.25f * (w - 2.0f) + 0.01f * w * w));
		add(EntityKind::Caster, introduce(4, 0.3f * (w - 3.0f) + 0.01f * w * w));
		for (size_t i = m_SpawnQueue.size(); i > 1; i--)
			std::swap(m_SpawnQueue[i - 1], m_SpawnQueue[random.Below(uint32_t(i))]);
		m_SpawnTimer = 0.0f;
		world.Emit({ WorldEventType::WaveStarted });
	}
}
