#include "MatchDirector.h"
#include "GameShared/Content/Missions.h"
#include "GameShared/Simulation/Combat.h"
#include "GameShared/Simulation/World.h"
#include "GameShared/World/Collision.h"
#include "GameShared/World/Navigation.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>

namespace Game
{
	namespace
	{
		constexpr float PadHealthRespawn = 15.0f;
		constexpr float PadPowerRespawn = 30.0f;
		constexpr float PadLootLifetime = 1e6f;
		constexpr float StormTickSeconds = 0.5f;
		constexpr uint32_t GeneratedSpawns = 24;
		constexpr uint32_t GeneratedChests = 30;
		constexpr uint32_t GeneratedVaults = 4;
		constexpr uint32_t GuardianLevel = 4;

		LootKind PadLoot(Random& random, PadKind kind)
		{
			switch (kind)
			{
			case PadKind::Health:
				return LootKind::Health;
			case PadKind::Mana:
				return LootKind::Mana;
			default:
			{
				constexpr LootKind Buffs[] = { LootKind::Might, LootKind::Haste, LootKind::Swiftness, LootKind::Vitality };
				return Buffs[random.Below(uint32_t(std::size(Buffs)))];
			}
			}
		}

		std::vector<Vec2> SampleSpots(World& world, const FlowField& reach, uint32_t count, float spacing, float clearance, const std::vector<Vec2>& avoid)
		{
			const Level& level = world.GetLevel();
			Random& random = world.GetRandom();
			float totalArea = 0.0f;
			for (const Region& region : level.Regions)
				if (region.EnemySpawns)
					totalArea += region.GetArea();
			std::vector<Vec2> spots;
			if (totalArea <= 0.0f)
				return spots;
			for (uint32_t attempt = 0; attempt < 8000 && spots.size() < count; attempt++)
			{
				float pick = random.Float() * totalArea;
				const Region* chosen = nullptr;
				for (const Region& region : level.Regions)
				{
					if (!region.EnemySpawns)
						continue;
					chosen = &region;
					pick -= region.GetArea();
					if (pick <= 0.0f)
						break;
				}
				const Vec2 spot = { chosen->Min.x + random.Float() * (chosen->Max.x - chosen->Min.x), chosen->Min.y + random.Float() * (chosen->Max.y - chosen->Min.y) };
				if (!IsFree(level, spot, clearance) || !std::isfinite(reach.GetTravelDistance(spot)))
					continue;
				const auto near = [&](Vec2 other) { return LengthSquared(other - spot) < spacing * spacing; };
				if (std::any_of(spots.begin(), spots.end(), near) || std::any_of(avoid.begin(), avoid.end(), near))
					continue;
				spots.push_back(spot);
			}
			return spots;
		}
	}

	void MatchDirector::Begin(World& world)
	{
		const WorldSettings& settings = world.GetSettings();
		m_Mode = settings.Mode;
		m_Rules = SanitizeRules(settings.Rules);
		m_Teams.fill(NoTeam);
		m_SpawnPoints = world.GetLevel().SpawnPoints;
		SetPhase(MatchPhase::Countdown);
		if (m_Mode == GameMode::Arena)
			BeginArena(world);
		else
			BeginRoyale(world);
	}

	void MatchDirector::BeginArena(World& world)
	{
		if (m_SpawnPoints.empty())
			m_SpawnPoints.push_back(world.GetLevel().PlayerSpawn);
		for (const PickupPad& pad : world.GetLevel().Pads)
			m_Pads.push_back({ pad, Entity{}, 0.0f });
	}

	void MatchDirector::BeginRoyale(World& world)
	{
		const Level& level = world.GetLevel();
		FlowField reach;
		const Vec2 start[] = { level.PlayerSpawn };
		reach.Build(level, world.GetNavGrid(), NarrowBodyRadius, start);
		if (m_SpawnPoints.empty())
			m_SpawnPoints = SampleSpots(world, reach, GeneratedSpawns, 8.0f, 0.6f, {});
		if (m_SpawnPoints.empty())
			m_SpawnPoints.push_back(level.PlayerSpawn);

		std::vector<LootSpot> spots = level.LootSpots;
		if (spots.empty())
		{
			std::vector<Vec2> taken = m_SpawnPoints;
			for (const Vec2 spot : SampleSpots(world, reach, GeneratedVaults, 22.0f, 1.2f, taken))
			{
				spots.push_back({ spot, true });
				taken.push_back(spot);
			}
			for (const Vec2 spot : SampleSpots(world, reach, GeneratedChests, 4.5f, 0.7f, taken))
				spots.push_back({ spot, false });
		}
		for (const LootSpot& spot : spots)
		{
			SpawnChest(world, spot.Position, spot.Vault ? ChestKind::Vault : ChestKind::Common);
			if (spot.Vault)
				SpawnGuardians(world, spot.Position);
		}

		const TileMap& map = level.Map;
		Vec2 min = { 1e9f, 1e9f };
		Vec2 max = { -1e9f, -1e9f };
		for (int32_t y = 0; y < map.GetHeight(); y++)
			for (int32_t x = 0; x < map.GetWidth(); x++)
				if (map.IsWalkable(x, y))
				{
					min = { std::min(min.x, float(x)), std::min(min.y, float(y)) };
					max = { std::max(max.x, float(x + 1)), std::max(max.y, float(y + 1)) };
				}
		if (min.x > max.x)
			min = max = level.PlayerSpawn;
		m_Storm.Center = (min + max) * 0.5f;
		m_Storm.Radius = Length(max - min) * 0.5f + 1.0f;
		m_Storm.Target = m_Storm.Center;
		m_Storm.TargetRadius = m_Storm.Radius;
		m_Storm.Stage = 0;
		m_Storm.Shrinking = false;
		m_Storm.Timer = StormStages[0].Wait;
		m_Storm.Damage = 0.0f;
	}

	void MatchDirector::SetPhase(MatchPhase phase)
	{
		m_Phase = phase;
		m_PhaseTime = 0.0f;
	}

	uint32_t MatchDirector::CountAlive() const
	{
		uint32_t alive = 0;
		for (const MatchScore& score : m_Scores)
			alive += score.Entered && !score.Out ? 1 : 0;
		return alive;
	}

	void MatchDirector::OnPlayerSpawned(World& world, Entity player, uint32_t playerId)
	{
		if (playerId >= MaxPlayers)
			return;
		if (!m_Present[playerId])
		{
			m_Scores[playerId] = {};
			m_Teams[playerId] = NoTeam;
			m_Present[playerId] = true;
		}
		EntityRegistry& entities = world.GetEntities();
		if (m_Mode == GameMode::Arena && m_Rules.Teams && m_Teams[playerId] == NoTeam)
		{
			std::array<uint32_t, TeamCount> members{};
			entities.Each<PlayerController>([&](Entity, PlayerController& other)
			{
				const uint8_t team = GetTeam(other.PlayerId);
				if (other.PlayerId != playerId && team < TeamCount)
					members[team]++;
			});
			m_Teams[playerId] = members[1] < members[0] ? 1 : 0;
		}
		PlayerController* controller = entities.TryGet<PlayerController>(player);
		if (!controller)
			return;
		const bool late = m_Phase == MatchPhase::Ended || (m_Mode == GameMode::Royale && m_Phase == MatchPhase::Fight);
		if (late)
		{
			controller->State.Alive = false;
			controller->RespawnTimer = 0.0f;
			if (Health* health = entities.TryGet<Health>(player))
				health->Current = 0;
			controller->Watching = NoPlayer;
			return;
		}
		controller->State.Invulnerable = SpawnProtection;
		if (m_Mode == GameMode::Arena && m_Phase == MatchPhase::Fight && !m_Scores[playerId].Entered)
		{
			m_Scores[playerId].Entered = true;
			m_Entrants++;
		}
	}

	void MatchDirector::OnPlayerRemoved(World& world, uint32_t playerId)
	{
		if (playerId >= MaxPlayers)
			return;
		MatchScore& score = m_Scores[playerId];
		if (m_Mode == GameMode::Royale && m_Phase == MatchPhase::Fight && score.Entered && !score.Out)
		{
			score.Out = true;
			score.Placement = uint8_t(CountAlive() + 1);
			m_Fallen.push_back(playerId);
			FinishRoyaleIfDecided(world);
		}
		m_Present[playerId] = false;
		m_Scores[playerId] = {};
		m_Teams[playerId] = NoTeam;
	}

	void MatchDirector::OnPlayerKilled(World& world, Entity player, uint32_t victim, uint32_t killer)
	{
		if (victim >= MaxPlayers)
			return;
		const bool fighting = m_Phase == MatchPhase::Fight;
		if (fighting)
			m_Scores[victim].Deaths++;
		if (!IsPresent(killer))
			killer = NoPlayer;
		const bool teamKill = m_Mode == GameMode::Arena && m_Rules.Teams && killer < MaxPlayers && m_Teams[killer] == m_Teams[victim];
		if (fighting && killer < MaxPlayers && killer != victim && !teamKill)
		{
			m_Scores[killer].Kills++;
			if (m_Mode == GameMode::Arena && m_Rules.Teams && m_Teams[killer] < TeamCount)
				m_TeamScores[m_Teams[killer]]++;
		}
		if (m_Mode == GameMode::Arena)
		{
			if (!fighting)
				return;
			if (m_Rules.Teams)
			{
				for (uint8_t team = 0; team < TeamCount; team++)
					if (m_TeamScores[team] >= m_Rules.ScoreGoal)
						EndMatch(world, NoPlayer, team);
			}
			else if (killer < MaxPlayers && m_Scores[killer].Kills >= m_Rules.ScoreGoal)
				EndMatch(world, killer, NoTeam);
			return;
		}
		if (!fighting)
			return;
		MatchScore& score = m_Scores[victim];
		if (!score.Out)
		{
			score.Out = true;
			score.Placement = uint8_t(CountAlive() + 1);
			m_Fallen.push_back(victim);
		}
		DropBelongings(world, player);
		if (PlayerController* controller = world.GetEntities().TryGet<PlayerController>(player))
			controller->Watching = killer < MaxPlayers && killer != victim ? killer : NoPlayer;
		if (!m_Deferring)
			FinishRoyaleIfDecided(world);
	}

	void MatchDirector::FinishRoyaleIfDecided(World& world)
	{
		if (m_Phase != MatchPhase::Fight)
			return;
		const uint32_t alive = CountAlive();
		if (alive > 1 || (alive == 1 && m_Entrants < 2))
			return;
		uint32_t winner = NoPlayer;
		for (uint32_t id = 0; id < MaxPlayers; id++)
			if (m_Scores[id].Entered && !m_Scores[id].Out)
				winner = id;
		if (winner == NoPlayer && m_Entrants >= 2 && !m_Fallen.empty())
			winner = m_Fallen.back();
		if (winner != NoPlayer)
			m_Scores[winner].Placement = 1;
		EndMatch(world, winner, NoTeam);
	}

	void MatchDirector::EndMatch(World& world, uint32_t winner, uint8_t team)
	{
		if (m_Phase == MatchPhase::Ended)
			return;
		m_Winner = winner;
		m_WinningTeam = team;
		SetPhase(MatchPhase::Ended);
		world.Emit({ WorldEventType::MatchEnded, winner, team });
	}

	void MatchDirector::StartFight(World& world)
	{
		SetPhase(MatchPhase::Fight);
		m_Clock = 0.0f;
		m_Entrants = 0;
		std::vector<Entity> fallen;
		world.GetEntities().Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (controller.PlayerId >= MaxPlayers)
				return;
			m_Scores[controller.PlayerId] = { .Entered = true };
			m_Entrants++;
			if (!controller.State.Alive)
				fallen.push_back(entity);
		});
		for (const Entity entity : fallen)
			world.RespawnPlayer(entity);
		if (m_Mode == GameMode::Royale)
		{
			m_TimeScale = std::clamp(0.7f + 0.06f * float(m_Entrants), 0.8f, 1.4f);
			PlanStorm(world);
			FinishRoyaleIfDecided(world);
		}
		world.Emit({ WorldEventType::MatchStarted });
	}

	void MatchDirector::Update(World& world, float dt)
	{
		m_PhaseTime += dt;
		switch (m_Phase)
		{
		case MatchPhase::Countdown:
			if (m_PhaseTime >= (m_Mode == GameMode::Arena ? ArenaCountdown : RoyaleCountdown))
				StartFight(world);
			break;
		case MatchPhase::Fight:
			m_Clock += dt;
			if (m_Mode == GameMode::Arena)
				UpdateArena(world, dt);
			else
				UpdateRoyale(world, dt);
			break;
		case MatchPhase::Ended:
			break;
		}
		if (m_Mode == GameMode::Royale)
			UpdateWatchers(world);
	}

	void MatchDirector::UpdateArena(World& world, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		for (PadState& pad : m_Pads)
		{
			if (entities.IsAlive(pad.Loot))
				continue;
			pad.Timer -= dt;
			if (pad.Timer > 0.0f)
				continue;
			pad.Loot = world.SpawnLoot(PadLoot(world.GetRandom(), pad.Pad.Kind), pad.Pad.Position);
			if (Loot* loot = entities.TryGet<Loot>(pad.Loot))
				loot->Lifetime = PadLootLifetime;
			pad.Timer = pad.Pad.Kind == PadKind::Power ? PadPowerRespawn : PadHealthRespawn;
		}
		if (m_Clock < float(m_Rules.Minutes) * 60.0f)
			return;
		if (m_Rules.Teams)
		{
			const uint8_t leader = m_TeamScores[0] > m_TeamScores[1] ? 0 : m_TeamScores[1] > m_TeamScores[0] ? 1 : NoTeam;
			EndMatch(world, NoPlayer, leader);
			return;
		}
		uint32_t leader = NoPlayer;
		bool tied = false;
		for (uint32_t id = 0; id < MaxPlayers; id++)
		{
			if (!m_Scores[id].Entered)
				continue;
			if (leader == NoPlayer)
			{
				leader = id;
				continue;
			}
			const MatchScore& best = m_Scores[leader];
			const MatchScore& score = m_Scores[id];
			if (score.Kills > best.Kills || (score.Kills == best.Kills && score.Deaths < best.Deaths))
			{
				leader = id;
				tied = false;
			}
			else if (score.Kills == best.Kills && score.Deaths == best.Deaths)
				tied = true;
		}
		EndMatch(world, tied ? NoPlayer : leader, NoTeam);
	}

	void MatchDirector::UpdateRoyale(World& world, float dt)
	{
		UpdateStorm(world, dt);
		FinishRoyaleIfDecided(world);
	}

	Vec2 MatchDirector::RandomPointIn(World& world, Vec2 center, float radius, float clearance) const
	{
		const Level& level = world.GetLevel();
		Random& random = world.GetRandom();
		for (int32_t attempt = 0; attempt < 60; attempt++)
		{
			const Vec2 point = center + FromAngle(random.Float() * 2.0f * Pi) * (radius * std::sqrt(random.Float()));
			if (IsFree(level, point, clearance))
				return point;
		}
		for (int32_t attempt = 0; attempt < 60; attempt++)
		{
			const Vec2 point = center + FromAngle(random.Float() * 2.0f * Pi) * (radius * std::sqrt(random.Float()));
			if (level.Map.IsWalkable(int32_t(std::floor(point.x)), int32_t(std::floor(point.y))))
				return point;
		}
		return center;
	}

	void MatchDirector::PlanStorm(World& world)
	{
		const StormStage& stage = StormStages[std::min<size_t>(m_Storm.Stage, StormStageCount - 1)];
		m_Storm.TargetRadius = m_Storm.Radius * stage.Scale;
		m_Storm.Target = RandomPointIn(world, m_Storm.Center, std::max(m_Storm.Radius - m_Storm.TargetRadius - 1.0f, 0.0f), 1.0f);
		m_Storm.Timer = stage.Wait * m_TimeScale;
		m_Storm.Shrinking = false;
		m_Storm.Damage = stage.Damage;
	}

	void MatchDirector::UpdateStorm(World& world, float dt)
	{
		m_Storm.Timer = std::max(m_Storm.Timer - dt, 0.0f);
		if (!m_Storm.Shrinking)
		{
			if (m_Storm.Timer <= 0.0f && m_Storm.Stage < StormStageCount)
			{
				m_Storm.Shrinking = true;
				m_ShrinkFrom = m_Storm.Center;
				m_ShrinkFromRadius = m_Storm.Radius;
				m_ShrinkDuration = std::max(StormStages[m_Storm.Stage].Shrink * m_TimeScale, 1.0f);
				m_Storm.Timer = m_ShrinkDuration;
				world.Emit({ WorldEventType::StormClosing, NoPlayer, m_Storm.Stage });
			}
		}
		else
		{
			const float t = 1.0f - std::clamp(m_Storm.Timer / m_ShrinkDuration, 0.0f, 1.0f);
			m_Storm.Center = Lerp(m_ShrinkFrom, m_Storm.Target, t);
			m_Storm.Radius = m_ShrinkFromRadius + (m_Storm.TargetRadius - m_ShrinkFromRadius) * t;
			if (m_Storm.Timer <= 0.0f)
			{
				m_Storm.Center = m_Storm.Target;
				m_Storm.Radius = m_Storm.TargetRadius;
				m_Storm.Stage++;
				if (m_Storm.Stage < StormStageCount)
				{
					PlanStorm(world);
					if (m_Storm.Stage == 2 || m_Storm.Stage == 4)
						DropCache(world);
				}
				else
				{
					m_Storm.Shrinking = false;
					m_Storm.Timer = 0.0f;
					m_Storm.Damage = StormFinalDamage;
				}
			}
		}

		m_StormTick += dt;
		if (m_StormTick < StormTickSeconds)
			return;
		m_StormTick -= StormTickSeconds;
		EntityRegistry& entities = world.GetEntities();
		struct Victim
		{
			Entity Handle;
			int32_t Damage = 0;
			float Health = 0.0f;
		};
		std::vector<Victim> victims;
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			const Transform* transform = entities.TryGet<Transform>(entity);
			const Health* health = entities.TryGet<Health>(entity);
			if (!controller.State.Alive || !transform || !health || m_Storm.Contains(transform->Position))
				return;
			victims.push_back({ entity, std::max(1, int32_t(std::ceil(float(controller.Traits.MaxHealth) * m_Storm.Damage * StormTickSeconds))), float(health->Current) / float(std::max(health->Max, 1)) });
		});
		std::sort(victims.begin(), victims.end(), [](const Victim& a, const Victim& b) { return a.Health < b.Health; });
		m_Deferring = true;
		for (const Victim& victim : victims)
			ApplyDamage(world, victim.Handle, { .Amount = victim.Damage, .Damages = Element::Poison, .Unmitigated = true });
		m_Deferring = false;
		FinishRoyaleIfDecided(world);
	}

	void MatchDirector::DropCache(World& world)
	{
		const Vec2 position = RandomPointIn(world, m_Storm.Target, m_Storm.TargetRadius * 0.7f, 1.2f);
		SpawnChest(world, position, ChestKind::Cache);
		const Region* region = world.GetLevel().FindRegion(position);
		world.Notify(NoPlayer, std::string("An Ember Cache crashed down ") + (region ? "in " + region->Name : std::string("inside the storm's eye")), true);
		world.Emit({ WorldEventType::CacheDropped });
	}

	void MatchDirector::SpawnChest(World& world, Vec2 position, ChestKind kind)
	{
		Chest chest;
		chest.Kind = kind;
		chest.Items = GetChestDef(kind).Items;
		const Vec2 spot = IsFree(world.GetLevel(), position, 0.6f) ? position : world.FindFreeSpotNear(position, 0.6f);
		world.SpawnChest(spot, chest);
	}

	void MatchDirector::SpawnGuardians(World& world, Vec2 position)
	{
		Random& random = world.GetRandom();
		const uint16_t pack = m_NextPack++;
		const uint32_t count = 2 + random.Below(2);
		for (uint32_t i = 0; i < count; i++)
		{
			const EntityKind kind = i == 0 ? (random.Below(3) == 0 ? EntityKind::Brute : EntityKind::Caster) : (random.Below(2) == 0 ? EntityKind::Chaser : EntityKind::Shooter);
			const EnemyDef* def = FindEnemyDef(kind);
			const float angle = (float(i) + random.Float() * 0.5f) * 2.0f * Pi / float(count);
			const Vec2 spot = world.FindFreeSpotNear(position + FromAngle(angle) * (2.2f + random.Float()), def->Radius);
			const Entity entity = world.SpawnEnemy(kind, spot, GetMonsterScaling(kind, GuardianLevel, 1, false, 0));
			if (Enemy* enemy = world.GetEntities().TryGet<Enemy>(entity))
			{
				enemy->Awake = false;
				enemy->Pack = pack;
				enemy->Home = spot;
				enemy->Wander = random.Float() * 3.0f;
			}
		}
	}

	void MatchDirector::DropBelongings(World& world, Entity player)
	{
		EntityRegistry& entities = world.GetEntities();
		Inventory* inventory = entities.TryGet<Inventory>(player);
		const Transform* transform = entities.TryGet<Transform>(player);
		const PlayerController* controller = entities.TryGet<PlayerController>(player);
		if (!inventory || !transform || !controller)
			return;
		const Vec2 origin = transform->Position;
		const uint32_t playerId = controller->PlayerId;
		std::vector<Item> items;
		const auto take = [&](auto& slots)
		{
			for (Item& item : slots)
				if (item.IsValid())
				{
					items.push_back(item);
					item = {};
				}
		};
		take(inventory->Gear);
		take(inventory->Gems);
		take(inventory->Pack);
		world.RefreshTraits(player);
		world.Emit({ WorldEventType::InventoryChanged, playerId });
		Random& random = world.GetRandom();
		for (size_t i = 0; i < items.size(); i++)
		{
			const float angle = float(i) * 2.39996f + random.Float() * 0.3f;
			const float distance = 0.9f + 0.3f * std::sqrt(float(i));
			world.SpawnItem(items[i], world.PlaceDrop(origin, origin + FromAngle(angle) * distance), NoPlayer);
		}
		world.SpawnLoot(LootKind::Health, origin);
	}

	Vec2 MatchDirector::FindSpawn(World& world, uint32_t playerId)
	{
		if (m_SpawnPoints.empty())
			return world.FindFreeSpotNear(world.GetLevel().PlayerSpawn, PlayerRadius);
		EntityRegistry& entities = world.GetEntities();
		const uint8_t team = GetTeam(playerId);
		std::vector<Vec2> others;
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (!controller.State.Alive || controller.PlayerId == playerId)
				return;
			if (team != NoTeam && GetTeam(controller.PlayerId) == team)
				return;
			if (const Transform* transform = entities.TryGet<Transform>(entity))
				others.push_back(transform->Position);
		});
		Random& random = world.GetRandom();
		Vec2 best = m_SpawnPoints[(random.Below(uint32_t(m_SpawnPoints.size())) + m_SpawnCounter++) % m_SpawnPoints.size()];
		float bestScore = -1.0f;
		if (!others.empty())
			for (const Vec2 spot : m_SpawnPoints)
			{
				float nearest = 1e18f;
				for (const Vec2 other : others)
					nearest = std::min(nearest, LengthSquared(other - spot));
				const float score = std::sqrt(nearest) * (0.85f + 0.3f * random.Float());
				if (score > bestScore)
				{
					bestScore = score;
					best = spot;
				}
			}
		return world.FindFreeSpotNear(best, PlayerRadius);
	}

	void MatchDirector::Watch(World& world, Entity player, int32_t direction)
	{
		EntityRegistry& entities = world.GetEntities();
		std::vector<uint32_t> alive;
		entities.Each<PlayerController>([&](Entity, PlayerController& other)
		{
			if (other.State.Alive)
				alive.push_back(other.PlayerId);
		});
		PlayerController* controller = entities.TryGet<PlayerController>(player);
		if (!controller || controller->State.Alive)
			return;
		if (alive.empty())
		{
			controller->Watching = NoPlayer;
			return;
		}
		std::sort(alive.begin(), alive.end());
		const auto it = std::find(alive.begin(), alive.end(), controller->Watching);
		if (it == alive.end())
		{
			controller->Watching = alive.front();
			return;
		}
		const int32_t count = int32_t(alive.size());
		controller->Watching = alive[size_t(((int32_t(it - alive.begin()) + direction) % count + count) % count)];
	}

	void MatchDirector::UpdateWatchers(World& world)
	{
		EntityRegistry& entities = world.GetEntities();
		std::vector<uint32_t> alive;
		std::vector<Entity> stale;
		entities.Each<PlayerController>([&](Entity, PlayerController& other)
		{
			if (other.State.Alive)
				alive.push_back(other.PlayerId);
		});
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (!controller.State.Alive && std::find(alive.begin(), alive.end(), controller.Watching) == alive.end())
				stale.push_back(entity);
		});
		for (const Entity entity : stale)
			Watch(world, entity, 0);
	}

	void MatchDirector::Describe(const World& world, GameStatus& status) const
	{
		status.Match = m_Phase;
		switch (m_Phase)
		{
		case MatchPhase::Countdown:
			status.MatchTime = std::max((m_Mode == GameMode::Arena ? ArenaCountdown : RoyaleCountdown) - m_PhaseTime, 0.0f);
			break;
		case MatchPhase::Fight:
			status.MatchTime = m_Mode == GameMode::Arena ? std::max(float(m_Rules.Minutes) * 60.0f - m_Clock, 0.0f) : m_Clock;
			break;
		case MatchPhase::Ended:
			status.MatchTime = std::max(MatchEndSeconds - m_PhaseTime, 0.0f);
			break;
		}
		status.TeamScores = m_TeamScores;
		status.Winner = m_Winner;
		status.WinningTeam = m_WinningTeam;
		uint32_t players = 0;
		const_cast<EntityRegistry&>(world.GetEntities()).Each<PlayerController>([&](Entity, PlayerController&) { players++; });
		status.Entrants = uint8_t(std::min(m_Phase == MatchPhase::Countdown ? players : m_Entrants, 255u));
		status.Alive = uint8_t(std::min(m_Phase == MatchPhase::Countdown ? players : CountAlive(), 255u));
		status.Storm = m_Storm;
	}

	MatchReward MatchDirector::GetReward(const World&, uint32_t playerId) const
	{
		if (m_Phase != MatchPhase::Ended || playerId >= MaxPlayers || !m_Scores[playerId].Entered || m_Clock < RewardMinimumSeconds)
			return {};
		const MatchScore& score = m_Scores[playerId];
		if (m_Mode == GameMode::Royale)
			return GetRoyaleReward(score.Placement > 0 ? score.Placement : m_Entrants, m_Entrants, score.Kills);
		const bool won = m_Rules.Teams ? m_WinningTeam != NoTeam && m_Teams[playerId] == m_WinningTeam : m_Winner == playerId;
		return GetArenaReward(won, m_Entrants, score.Kills);
	}
}
