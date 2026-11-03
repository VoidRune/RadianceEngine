#include "RunDirector.h"
#include "GameShared/Simulation/World.h"
#include "GameShared/World/Collision.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace Game
{
	namespace
	{
		constexpr float BudgetRate = 0.85f;
		constexpr float BudgetReserve = 40.0f;
		constexpr float KillRefund = 0.6f;
		constexpr float RiseSeconds = 80.0f;
		constexpr float BreatherSeconds = 12.0f;
		constexpr float MemberStagger = 0.09f;
		constexpr float PackSpread = 1.6f;
		constexpr float RecycleDistance = 30.0f;
		constexpr float RecycleSeconds = 10.0f;
		constexpr float TreasureInterval = 180.0f;
		constexpr float TreasureFirst = 150.0f;
		constexpr float HeadStartBudget = 6.0f;
		constexpr uint32_t OnlyGroupCost = 6;
		constexpr float SightClearance = 9.0f;

		struct PackCard
		{
			float From = 0.0f;
			float Weight = 1.0f;
			std::array<std::pair<EntityKind, uint8_t>, 3> Members{};
		};

		constexpr PackCard Packs[] = {
			{ 0.0f, 3.0f, { { { EntityKind::QuartzCrawler, 3 } } } },
			{ 0.5f, 2.0f, { { { EntityKind::QuartzCrawler, 8 } } } },
			{ 1.0f, 2.0f, { { { EntityKind::JadeCrawler, 2 }, { EntityKind::QuartzCrawler, 4 } } } },
			{ 2.0f, 1.6f, { { { EntityKind::SapphireCrawler, 2 }, { EntityKind::QuartzCrawler, 5 } } } },
			{ 2.5f, 1.4f, { { { EntityKind::SapphireCrawler, 1 }, { EntityKind::JadeCrawler, 2 }, { EntityKind::QuartzCrawler, 6 } } } },
			{ 3.5f, 1.2f, { { { EntityKind::RubyCrawler, 2 }, { EntityKind::QuartzCrawler, 4 } } } },
			{ 5.0f, 0.8f, { { { EntityKind::AmethystCrawler, 1 }, { EntityKind::QuartzCrawler, 6 } } } },
			{ 6.0f, 1.0f, { { { EntityKind::RubyCrawler, 2 }, { EntityKind::SapphireCrawler, 2 }, { EntityKind::JadeCrawler, 4 } } } },
		};

		constexpr std::pair<EntityKind, uint32_t> Caps[] = { { EntityKind::RubyCrawler, 8 }, { EntityKind::AmethystCrawler, 3 }, { EntityKind::PrismaticCrawler, 1 } };

		float Intensity(float clock)
		{
			const float cycle = std::fmod(clock, RiseSeconds + BreatherSeconds);
			return cycle < RiseSeconds ? 0.6f + 0.9f * (cycle / RiseSeconds) : 0.2f;
		}

		float PackCost(const PackCard& pack)
		{
			float cost = 0.0f;
			for (const auto& [kind, count] : pack.Members)
				if (count > 0)
					cost += float(FindEnemyDef(kind)->Cost) * float(count);
			return cost;
		}

		uint32_t PackSize(const PackCard& pack)
		{
			uint32_t size = 0;
			for (const auto& member : pack.Members)
				size += member.second;
			return size;
		}

		float PackRadius(const PackCard& pack)
		{
			float radius = 0.0f;
			for (const auto& [kind, count] : pack.Members)
				if (count > 0)
					radius = std::max(radius, FindEnemyDef(kind)->Radius);
			return radius;
		}
	}

	void RunDirector::Begin(const RunRules& rules)
	{
		m_Rules = rules;
		m_Status.ActClock = float(rules.StartMinute) * 60.0f;
		m_Status.RunClock = m_Status.ActClock;
		m_Treasure = std::max(TreasureFirst - m_Status.ActClock, 5.0f);
		m_Budget = std::min(m_Budget + float(rules.StartMinute) * HeadStartBudget, BudgetReserve);
	}

	void RunDirector::Update(World& world, float dt)
	{
		const uint32_t players = world.CountPlayers();
		if (players == 0)
			return;
		if (!m_Started)
		{
			m_Started = true;
			while (m_Status.Level < m_Rules.StartLevel)
				world.LevelUp(++m_Status.Level, false);
			m_Status.XpGoal = XpToNext(m_Status.Level, players);
		}
		m_Status.RunClock += dt;
		if (m_Status.Phase == RunPhase::Act)
			m_Status.ActClock += dt;
		GainXp(world, m_Orbs.Update(world, dt), players);
		Spend(world, dt, players);
		Arrive(world, dt, players);
		Recycle(world, dt);
	}

	void RunDirector::OnKilled(World&, const EnemyDef& def, Vec2 position, uint64_t seed)
	{
		m_Orbs.Drop(position, def.Xp, seed);
		m_Budget = std::min(m_Budget + float(def.Cost) * KillRefund, BudgetReserve);
	}

	EnemyScaling RunDirector::Scale(uint32_t players) const
	{
		const float minutes = m_Status.ActClock / 60.0f;
		return { .Health = TrashHealth(minutes, players), .Damage = 1.0f + 0.05f * minutes, .Level = 1 + uint32_t(minutes) };
	}

	void RunDirector::GainXp(World& world, uint32_t xp, uint32_t players)
	{
		m_XpCarry += float(xp) * float(m_Rules.Xp) / 100.0f;
		const uint32_t whole = uint32_t(m_XpCarry);
		m_XpCarry -= float(whole);
		m_Status.Xp += whole;
		m_Status.XpGoal = XpToNext(m_Status.Level, players);
		while (m_Status.Xp >= m_Status.XpGoal)
		{
			m_Status.Xp -= m_Status.XpGoal;
			m_Status.Level++;
			m_Status.XpGoal = XpToNext(m_Status.Level, players);
			world.LevelUp(m_Status.Level);
		}
	}

	void RunDirector::Spend(World& world, float dt, uint32_t players)
	{
		EntityRegistry& entities = world.GetEntities();
		uint32_t alive = 0;
		std::array<uint32_t, std::size(Caps)> counts{};
		entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			const Health* health = entities.TryGet<Health>(entity);
			if (!health || health->IsDead())
				return;
			alive++;
			for (size_t i = 0; i < std::size(Caps); i++)
				counts[i] += enemy.Type == Caps[i].first ? 1u : 0u;
		});
		for (const Arrival& arrival : m_Arrivals)
			for (size_t i = 0; i < std::size(Caps); i++)
				counts[i] += arrival.Kind == Caps[i].first ? 1u : 0u;
		const uint32_t pending = uint32_t(m_Arrivals.size());
		const float rate = float(m_Rules.Spawns) / 100.0f;
		m_Status.Alive = uint16_t(alive);
		m_Status.Goal = uint16_t(std::min<uint32_t>(uint32_t(std::lround(float(GetShardfallPopulation(m_Status.ActClock, players)) * rate)), ShardfallEnemyCap));
		const float minutes = m_Status.ActClock / 60.0f;
		m_Budget = std::min(m_Budget + BudgetRate * (1.0f + 0.1f * minutes) * PartyScale(players) * Intensity(m_Status.ActClock) * rate * dt, BudgetReserve);
		Random& random = world.GetRandom();

		if (m_Rules.Only != 0)
		{
			const EntityKind kind = RunOnlyKinds[m_Rules.Only - 1];
			uint32_t count = 0;
			entities.Each<Enemy>([&](Entity, Enemy& enemy) { count += enemy.Type == kind ? 1u : 0u; });
			for (const Arrival& arrival : m_Arrivals)
				count += arrival.Kind == kind ? 1u : 0u;
			if (rate > 0.0f && (kind == EntityKind::PrismaticCrawler || alive + pending < m_Status.Goal))
				SpendOnly(world, kind, alive, pending, count);
			return;
		}
		if ((m_Treasure -= dt) <= 0.0f && counts[2] == 0 && rate > 0.0f)
		{
			m_Treasure = TreasureInterval;
			if (const std::optional<Vec2> socket = ChooseSocket(world, FindEnemyDef(EntityKind::PrismaticCrawler)->Radius))
			{
				m_Arrivals.push_back({ EntityKind::PrismaticCrawler, *socket, 0.0f });
				entities.Each<PlayerController>([&](Entity, PlayerController& controller) { world.Notify(controller.PlayerId, "A prismatic crawler surfaces. Catch it before it burrows away", true); });
			}
		}
		if (alive + pending >= m_Status.Goal)
			return;

		const auto open = [&](const PackCard& pack)
		{
			if (minutes < pack.From || alive + pending + PackSize(pack) > ShardfallEnemyCap)
				return false;
			for (const auto& [kind, count] : pack.Members)
				for (size_t c = 0; c < std::size(Caps); c++)
					if (count > 0 && kind == Caps[c].first && counts[c] + count > Caps[c].second)
						return false;
			return true;
		};
		if (!m_Saving || !open(Packs[*m_Saving]))
		{
			m_Saving.reset();
			float total = 0.0f;
			for (const PackCard& pack : Packs)
				total += open(pack) ? pack.Weight : 0.0f;
			if (total <= 0.0f)
				return;
			float pick = random.Float() * total;
			for (size_t i = 0; i < std::size(Packs); i++)
			{
				if (!open(Packs[i]))
					continue;
				m_Saving = i;
				if ((pick -= Packs[i].Weight) <= 0.0f)
					break;
			}
		}
		const bool afford = PackCost(Packs[*m_Saving]) <= m_Budget;
		if (!afford && alive + pending >= uint32_t(m_Status.Goal) / 3u)
			return;
		const PackCard& pack = afford ? Packs[*m_Saving] : Packs[0];
		const std::optional<Vec2> socket = ChooseSocket(world, PackRadius(pack));
		if (!socket)
			return;
		if (afford)
		{
			m_Budget -= PackCost(pack);
			m_Saving.reset();
		}
		float delay = 0.0f;
		for (const auto& [kind, count] : pack.Members)
			for (uint32_t i = 0; i < count; i++)
			{
				m_Arrivals.push_back({ kind, *socket + FromAngle(random.Float() * 2.0f * Pi) * (PackSpread * std::sqrt(random.Float())), delay });
				delay += MemberStagger;
			}
	}

	void RunDirector::SpendOnly(World& world, EntityKind kind, uint32_t alive, uint32_t pending, uint32_t count)
	{
		const EnemyDef* def = FindEnemyDef(kind);
		uint32_t cap = ShardfallEnemyCap;
		for (const auto& [capped, limit] : Caps)
			cap = capped == kind ? limit : cap;
		const uint32_t cost = std::max<uint32_t>(def->Cost, 1u);
		const uint32_t group = std::min({ std::max(OnlyGroupCost / cost, 1u), cap > count ? cap - count : 0u, ShardfallEnemyCap > alive + pending ? ShardfallEnemyCap - alive - pending : 0u });
		if (group == 0)
			return;
		const bool afford = float(cost * group) <= m_Budget;
		if (kind != EntityKind::PrismaticCrawler && !afford && alive + pending >= uint32_t(m_Status.Goal) / 3u)
			return;
		const std::optional<Vec2> socket = ChooseSocket(world, def->Radius);
		if (!socket)
			return;
		if (afford)
			m_Budget -= float(cost * group);
		Random& random = world.GetRandom();
		for (uint32_t i = 0; i < group; i++)
			m_Arrivals.push_back({ kind, *socket + FromAngle(random.Float() * 2.0f * Pi) * (PackSpread * std::sqrt(random.Float())), float(i) * MemberStagger });
	}

	void RunDirector::Arrive(World& world, float dt, uint32_t players)
	{
		for (size_t i = 0; i < m_Arrivals.size();)
		{
			Arrival& arrival = m_Arrivals[i];
			if ((arrival.Delay -= dt) > 0.0f)
			{
				i++;
				continue;
			}
			const EnemyDef* def = FindEnemyDef(arrival.Kind);
			const Entity entity = world.SpawnEnemy(arrival.Kind, world.FindFreeSpotNear(arrival.Position, def->Radius), Scale(players));
			if (Enemy* enemy = world.GetEntities().TryGet<Enemy>(entity))
				enemy->Sprout = SproutSeconds;
			m_Arrivals.erase(m_Arrivals.begin() + ptrdiff_t(i));
		}
	}

	void RunDirector::Recycle(World& world, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			const EnemyDef* def = FindEnemyDef(enemy.Type);
			Transform* transform = entities.TryGet<Transform>(entity);
			if (!def || !transform || def->Behavior == EnemyBehavior::Treasure || def->Behavior == EnemyBehavior::Egg || enemy.Sprout > 0.0f)
				return;
			const float travel = world.GetNavigation(def->Radius).GetTravelDistance(transform->Position);
			enemy.Far = std::isfinite(travel) && travel <= RecycleDistance ? 0.0f : enemy.Far + dt;
			if (enemy.Far < RecycleSeconds)
				return;
			enemy.Far = 0.0f;
			const std::optional<Vec2> socket = ChooseSocket(world, def->Radius);
			if (!socket)
				return;
			transform->Position = world.FindFreeSpotNear(*socket, def->Radius);
			enemy.Anchor = transform->Position;
			enemy.Sprout = SproutSeconds;
			if (MotionHistory* history = entities.TryGet<MotionHistory>(entity))
			{
				history->Positions.fill(transform->Position);
				history->FirstTick = world.GetTick() + 1;
			}
		});
	}

	std::optional<Vec2> RunDirector::ChooseSocket(World& world, float radius)
	{
		const FlowField& field = world.GetNavigation(radius);
		Random& random = world.GetRandom();
		std::vector<Vec2> players;
		world.GetEntities().Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (controller.State.Alive)
				if (const Transform* transform = world.GetEntities().TryGet<Transform>(entity))
					players.push_back(transform->Position);
		});
		std::optional<Vec2> chosen;
		std::optional<Vec2> nearest;
		float nearestMiss = 1e30f;
		uint32_t inBand = 0;
		for (const Vec2 socket : world.GetLevel().Sockets)
		{
			const float distance = field.GetTravelDistance(socket);
			if (!std::isfinite(distance))
				continue;
			bool seen = false;
			for (const Vec2 player : players)
				seen = seen || (LengthSquared(player - socket) < SightClearance * SightClearance && HasLineOfFire(world.GetLevel(), player, socket));
			if (seen)
				continue;
			const float miss = distance < SocketNearest ? SocketNearest - distance : std::max(distance - SocketFarthest, 0.0f);
			if (miss == 0.0f && random.Below(++inBand) == 0)
				chosen = socket;
			if (miss < nearestMiss)
			{
				nearestMiss = miss;
				nearest = socket;
			}
		}
		return chosen ? chosen : nearest;
	}
}
