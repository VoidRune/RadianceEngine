#include "MatchDirector.h"
#include "GameShared/Simulation/Combat.h"
#include "GameShared/Simulation/World.h"
#include "GameShared/World/Collision.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>

namespace Game
{
	void MatchDirector::Begin(World& world)
	{
		const WorldSettings& settings = world.GetSettings();
		m_Mode = settings.Mode;
		m_Rules = SanitizeRules(settings.Rules);
		m_Teams.fill(NoTeam);
		m_Seats = m_Rules.Seats;
		m_SpawnPoints = world.GetLevel().SpawnPoints;
		SetPhase(MatchPhase::Countdown);
		BeginBrawl(world);
	}

	bool MatchDirector::CanRespawn() const
	{
		return m_Mode == GameMode::Brawl && m_Phase != MatchPhase::Ended && (m_Brawl.Phase == BrawlPhase::Draft || m_Brawl.Phase == BrawlPhase::Pick);
	}

	bool MatchDirector::IsFighting() const
	{
		return m_Mode == GameMode::Brawl && m_Phase != MatchPhase::Ended && (m_Brawl.Phase == BrawlPhase::Draft || m_Brawl.Phase == BrawlPhase::Pick || m_Brawl.Phase == BrawlPhase::Fight);
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

	void MatchDirector::Bench(World& world, Entity player)
	{
		EntityRegistry& entities = world.GetEntities();
		PlayerController* controller = entities.TryGet<PlayerController>(player);
		if (!controller)
			return;
		controller->State.Alive = false;
		controller->RespawnTimer = 0.0f;
		if (Health* health = entities.TryGet<Health>(player))
			health->Current = 0;
		controller->Watching = NoPlayer;
	}

	uint8_t MatchDirector::SmallestTeam(uint32_t playerId) const
	{
		std::array<uint32_t, MaxTeams> members{};
		for (uint32_t id = 0; id < MaxPlayers; id++)
			if (id != playerId && m_Present[id] && m_Teams[id] < m_Rules.Teams)
				members[m_Teams[id]]++;
		uint8_t smallest = 0;
		for (uint8_t team = 1; team < m_Rules.Teams; team++)
			if (members[team] < members[smallest])
				smallest = team;
		return smallest;
	}

	uint32_t MatchDirector::CountTeams(bool aliveOnly) const
	{
		std::array<bool, MaxTeams> counted{};
		for (uint32_t id = 0; id < MaxPlayers; id++)
			if (m_Scores[id].Entered && (!aliveOnly || !m_Scores[id].Out) && m_Teams[id] < MaxTeams)
				counted[m_Teams[id]] = true;
		return uint32_t(std::count(counted.begin(), counted.end(), true));
	}

	void MatchDirector::SetSeat(uint32_t playerId, uint8_t seat)
	{
		if (playerId < MaxPlayers)
			m_Seats[playerId] = seat == SpectatorSeat || seat < m_Rules.Teams ? seat : NoTeam;
	}

	bool MatchDirector::JoinMatch(World& world, Entity player, uint32_t playerId)
	{
		if (!IsSpectator(playerId) || m_Phase == MatchPhase::Ended)
			return false;
		m_Seats[playerId] = NoTeam;
		OnPlayerSpawned(world, player, playerId);
		return true;
	}

	void MatchDirector::OnPlayerSpawned(World& world, Entity player, uint32_t playerId)
	{
		if (playerId >= MaxPlayers)
			return;
		if (IsSpectator(playerId))
		{
			m_Teams[playerId] = NoTeam;
			Bench(world, player);
			return;
		}
		if (!m_Present[playerId])
		{
			m_Scores[playerId] = {};
			m_Teams[playerId] = NoTeam;
			m_Present[playerId] = true;
			const BrawlSpells spells = m_Brawlers[playerId].Loadout.Spells;
			m_Brawlers[playerId] = {};
			m_Brawlers[playerId].Loadout.Spells = spells;
		}
		PlayerController* controller = world.GetEntities().TryGet<PlayerController>(player);
		if (!controller)
			return;
		if (IsTeamBrawl() && m_Teams[playerId] == NoTeam)
			m_Teams[playerId] = m_Seats[playerId] < m_Rules.Teams ? m_Seats[playerId] : SmallestTeam(playerId);
		if (m_Phase == MatchPhase::Ended || !CanRespawn())
		{
			Bench(world, player);
			return;
		}
		controller->State.Invulnerable = SpawnProtection;
		FitBrawlArena();
		if (m_Brawl.Phase == BrawlPhase::Draft && IsApexDraft() && !m_Brawlers[playerId].Loadout.OfferPending && (m_Brawlers[playerId].Loadout.Cards & ApexMask) == 0)
		{
			OfferApexPick(world, playerId);
			m_CardRevision++;
		}
	}

	void MatchDirector::OnPlayerRemoved(World&, uint32_t playerId)
	{
		if (playerId >= MaxPlayers)
			return;
		MatchScore& score = m_Scores[playerId];
		if (score.Entered && !score.Out)
			score.Out = true;
		m_Present[playerId] = false;
		m_Scores[playerId] = {};
		m_Teams[playerId] = NoTeam;
		m_Seats[playerId] = NoTeam;
		m_Brawlers[playerId] = {};
		m_Ledgers[playerId] = {};
		FitBrawlArena();
	}

	void MatchDirector::OnPlayerKilled(World& world, Entity player, uint32_t victim, uint32_t killer)
	{
		if (victim < MaxPlayers)
			OnBrawlerKilled(world, player, victim, IsPresent(killer) ? killer : NoPlayer);
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

	void MatchDirector::Update(World& world, float dt)
	{
		m_PhaseTime += dt;
		if (m_Phase == MatchPhase::Fight)
			m_Clock += dt;
		if (m_Phase != MatchPhase::Ended)
			UpdateBrawl(world, dt);
		UpdateWatchers(world);
	}

	Vec2 MatchDirector::FindSpawn(World& world, uint32_t playerId)
	{
		const float angle = float(m_SpawnCounter++) * 2.39996f + float(playerId) * 0.7f;
		return world.FindFreeSpotNear(m_Brawl.Plan.Center + FromAngle(angle) * (m_Brawl.Plan.StartRadius * BrawlSpawnRing), PlayerRadius);
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
		status.MatchTime = m_Phase == MatchPhase::Ended ? std::max(MatchEndSeconds - m_PhaseTime, 0.0f) : std::max(m_Brawl.Timer, 0.0f);
		status.Brawl = m_Brawl;
		status.Brawl.Players = uint8_t(std::min(CountPresent(), 255u));
		uint32_t ready = 0;
		for (uint32_t id = 0; id < MaxPlayers; id++)
			ready += m_Present[id] && m_Brawlers[id].Loadout.Ready ? 1u : 0u;
		status.Brawl.Ready = uint8_t(ready);
		status.TeamScores = m_TeamScores;
		status.Winner = m_Winner;
		status.WinningTeam = m_WinningTeam;
		uint32_t players = 0;
		const_cast<EntityRegistry&>(world.GetEntities()).Each<PlayerController>([&](Entity, PlayerController& controller) { players += IsSpectator(controller.PlayerId) ? 0u : 1u; });
		status.Entrants = uint8_t(std::min(m_Phase == MatchPhase::Countdown ? players : m_Entrants, 255u));
		status.Alive = uint8_t(std::min(m_Phase == MatchPhase::Countdown ? players : CountAlive(), 255u));
	}
}
