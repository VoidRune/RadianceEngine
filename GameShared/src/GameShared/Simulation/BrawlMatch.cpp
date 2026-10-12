#include "MatchDirector.h"
#include "GameShared/Simulation/Combat.h"
#include "GameShared/Simulation/World.h"
#include <algorithm>
#include <cmath>
#include <tuple>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr float LavaTickSeconds = 0.25f;
		constexpr float ArenaWander = 1.5f;
	}

	uint32_t MatchDirector::CountPresent() const
	{
		return uint32_t(std::count(m_Present.begin(), m_Present.end(), true));
	}

	bool MatchDirector::AllBrawlersReady() const
	{
		uint32_t present = 0;
		for (uint32_t id = 0; id < MaxPlayers; id++)
		{
			if (!m_Present[id])
				continue;
			present++;
			if (!m_Brawlers[id].Loadout.Ready)
				return false;
		}
		return present > 0;
	}

	void MatchDirector::BeginBrawl(World& world)
	{
		m_Brawl = {};
		m_Brawl.Rounds = m_Rules.Rounds;
		m_Brawl.Phase = BrawlPhase::Draft;
		m_Brawl.Timer = BrawlDraftSeconds;
		m_Brawlers.fill({});
		PlanBrawlRound(world);
	}

	void MatchDirector::PlanBrawlRound(World& world)
	{
		Random& random = world.GetRandom();
		m_Brawl.Plan.Center = world.GetLevel().PlayerSpawn + FromAngle(random.Float() * 2.0f * Pi) * (ArenaWander * std::sqrt(random.Float()));
		m_Brawl.Plan.Seed = random.Next() | 1u;
		m_Brawl.Clock = -1e5f;
		FitBrawlArena();
	}

	void MatchDirector::FitBrawlArena()
	{
		if (m_Brawl.Phase == BrawlPhase::Draft || m_Brawl.Phase == BrawlPhase::Shop)
			m_Brawl.Plan.StartRadius = BrawlStartRadius(CountPresent());
	}

	void MatchDirector::PlaceBrawlers(World& world)
	{
		EntityRegistry& entities = world.GetEntities();
		std::vector<Entity> players;
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (controller.PlayerId < MaxPlayers && m_Present[controller.PlayerId])
				players.push_back(entity);
		});
		Random& random = world.GetRandom();
		for (size_t i = players.size(); i > 1; i--)
			std::swap(players[i - 1], players[random.Below(uint32_t(i))]);
		const float turn = random.Float() * 2.0f * Pi;
		const float radius = m_Brawl.Plan.StartRadius * BrawlSpawnRing;
		for (size_t i = 0; i < players.size(); i++)
		{
			const Entity player = players[i];
			world.RespawnPlayer(player);
			Transform* transform = entities.TryGet<Transform>(player);
			PlayerController* controller = entities.TryGet<PlayerController>(player);
			if (!transform || !controller)
				continue;
			const Vec2 spot = world.FindFreeSpotNear(m_Brawl.Plan.Center + FromAngle(turn + 2.0f * Pi * float(i) / float(players.size())) * radius, PlayerRadius);
			transform->Position = spot;
			transform->Facing = AngleOf(m_Brawl.Plan.Center - spot);
			controller->State.Invulnerable = 0.0f;
			controller->Watching = NoPlayer;
			if (MotionHistory* history = entities.TryGet<MotionHistory>(player))
			{
				history->Positions.fill(spot);
				history->FirstTick = world.GetTick() + 1;
			}
		}
	}

	void MatchDirector::UpdateBrawl(World& world, float dt)
	{
		switch (m_Brawl.Phase)
		{
		case BrawlPhase::Draft:
		case BrawlPhase::Shop:
			m_Brawl.Timer -= dt;
			if (m_Brawl.Timer <= 0.0f || AllBrawlersReady())
				StartBrawlCountdown(world);
			break;
		case BrawlPhase::Countdown:
			m_Brawl.Timer -= dt;
			m_Brawl.Clock = -std::max(m_Brawl.Timer, 0.0f);
			if (m_Brawl.Timer <= 0.0f)
				StartBrawlRound(world);
			break;
		case BrawlPhase::Fight:
		{
			m_Brawl.Clock += dt;
			m_Brawl.Timer = m_Brawl.Clock;
			UpdateLava(world, dt);
			const uint32_t alive = CountAlive();
			if (alive == 0 || (m_Entrants >= 2 && alive <= 1))
				EndBrawlRound(world);
			break;
		}
		case BrawlPhase::Over:
			m_Brawl.Clock += dt;
			m_Brawl.Timer -= dt;
			if (m_Brawl.Timer > 0.0f)
				break;
			if (m_Brawl.Round >= m_Brawl.Rounds)
				FinishBrawl(world);
			else
				OpenBrawlShop(world);
			break;
		}
	}

	void MatchDirector::StartBrawlCountdown(World& world)
	{
		Random& random = world.GetRandom();
		for (uint32_t id = 0; id < MaxPlayers; id++)
		{
			BrawlLoadout& loadout = m_Brawlers[id].Loadout;
			if (m_Present[id] && loadout.OfferPending)
			{
				std::vector<uint32_t> choices;
				for (uint32_t i = 0; i < BoonChoices; i++)
					if (loadout.Offer[i] != BoonId::Count)
						choices.push_back(i);
				if (!choices.empty())
					ChooseBoon(world, world.FindPlayer(id), id, choices[random.Below(uint32_t(choices.size()))]);
			}
			loadout.Ready = false;
			loadout.OfferPending = false;
		}
		m_Brawl.Phase = BrawlPhase::Countdown;
		m_Brawl.Timer = BrawlCountdownSeconds;
		m_Brawl.Clock = -BrawlCountdownSeconds;
		m_Brawl.RoundWinner = NoPlayer;
		PlaceBrawlers(world);
	}

	void MatchDirector::StartBrawlRound(World& world)
	{
		if (m_Phase == MatchPhase::Countdown)
		{
			SetPhase(MatchPhase::Fight);
			m_Clock = 0.0f;
		}
		m_Brawl.Phase = BrawlPhase::Fight;
		m_Brawl.Round++;
		m_Brawl.Timer = 0.0f;
		m_Brawl.Clock = 0.0f;
		m_Entrants = 0;
		m_Fallen.clear();
		m_LavaTick = 0.0f;
		world.GetEntities().Each<PlayerController>([&](Entity, PlayerController& controller)
		{
			if (controller.PlayerId >= MaxPlayers)
				return;
			MatchScore& score = m_Scores[controller.PlayerId];
			score.Entered = m_Present[controller.PlayerId] && controller.State.Alive;
			score.Out = false;
			m_Entrants += score.Entered ? 1u : 0u;
		});
		for (Brawler& brawler : m_Brawlers)
		{
			brawler.Afterburn = 0.0f;
			brawler.LavaBank = 0.0f;
			brawler.Burning = false;
		}
		if (m_Brawl.Round == 1)
			world.Emit({ WorldEventType::MatchStarted });
		world.Emit({ WorldEventType::RoundStarted, NoPlayer, m_Brawl.Round });
	}

	void MatchDirector::EndBrawlRound(World& world)
	{
		uint32_t winner = NoPlayer;
		uint32_t alive = 0;
		for (uint32_t id = 0; id < MaxPlayers; id++)
			if (m_Scores[id].Entered && !m_Scores[id].Out)
			{
				winner = id;
				alive++;
			}
		if (alive != 1)
			winner = alive == 0 && !m_Fallen.empty() ? m_Fallen.back() : NoPlayer;
		if (m_Entrants < 2)
			winner = NoPlayer;
		if (winner != NoPlayer)
		{
			m_Brawlers[winner].Points += RoundWinPoints;
			m_Brawlers[winner].RoundWins++;
		}
		m_Brawl.RoundWinner = winner;
		m_Brawl.Phase = BrawlPhase::Over;
		m_Brawl.Timer = BrawlOverSeconds;
		for (Brawler& brawler : m_Brawlers)
		{
			brawler.Burning = false;
			brawler.Afterburn = 0.0f;
		}
		world.Emit({ WorldEventType::RoundEnded, winner, m_Brawl.Round });
	}

	void MatchDirector::OpenBrawlShop(World& world)
	{
		m_Brawl.Phase = BrawlPhase::Shop;
		m_Brawl.Timer = BrawlShopSeconds;
		PlanBrawlRound(world);
		for (uint32_t id = 0; id < MaxPlayers; id++)
		{
			m_Brawlers[id].Loadout.Ready = false;
			m_Brawlers[id].Loadout.OfferPending = false;
			if (m_Present[id] && id != m_Brawl.RoundWinner)
				OfferBoons(world, id);
		}
		PlaceBrawlers(world);
	}

	void MatchDirector::FinishBrawl(World& world)
	{
		uint32_t best = NoPlayer;
		bool tied = false;
		const auto rank = [&](uint32_t id) { return std::tuple(m_Brawlers[id].Points, m_Brawlers[id].RoundWins, m_Scores[id].Kills); };
		for (uint32_t id = 0; id < MaxPlayers; id++)
		{
			if (!m_Present[id])
				continue;
			if (best == NoPlayer || rank(id) > rank(best))
			{
				best = id;
				tied = false;
			}
			else if (rank(id) == rank(best))
				tied = true;
		}
		EndMatch(world, tied ? NoPlayer : best, NoTeam);
	}

	void MatchDirector::UpdateLava(World& world, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		const Level& level = world.GetLevel();
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (controller.PlayerId >= MaxPlayers)
				return;
			Brawler& brawler = m_Brawlers[controller.PlayerId];
			const Transform* transform = entities.TryGet<Transform>(entity);
			if (!controller.State.Alive || !transform)
			{
				brawler.Burning = false;
				brawler.Afterburn = 0.0f;
				brawler.LavaBank = 0.0f;
				return;
			}
			const bool lava = controller.State.DashDistance <= 0.0f && IsBrawlLava(level, m_Brawl, transform->Position);
			float rate = 0.0f;
			if (lava)
			{
				rate = LavaDamagePerSecond;
				brawler.Afterburn = AfterburnSeconds;
			}
			else if (brawler.Afterburn > 0.0f)
			{
				brawler.Afterburn = std::max(brawler.Afterburn - dt, 0.0f);
				rate = AfterburnDamagePerSecond;
			}
			brawler.Burning = rate > 0.0f;
			brawler.LavaBank += rate * (1.0f - 0.2f * float(brawler.Loadout.Boons[size_t(BoonId::Firewalker)])) * dt;
		});
		m_LavaTick += dt;
		if (m_LavaTick < LavaTickSeconds)
			return;
		m_LavaTick -= LavaTickSeconds;
		struct Victim
		{
			Entity Handle;
			int32_t Damage = 0;
			float Health = 0.0f;
		};
		std::vector<Victim> victims;
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			const Health* health = entities.TryGet<Health>(entity);
			if (controller.PlayerId >= MaxPlayers || !controller.State.Alive || !health)
				return;
			Brawler& brawler = m_Brawlers[controller.PlayerId];
			const int32_t damage = int32_t(brawler.LavaBank);
			if (damage <= 0)
				return;
			brawler.LavaBank -= float(damage);
			victims.push_back({ entity, damage, float(health->Current) / float(std::max(health->Max, 1)) });
		});
		std::sort(victims.begin(), victims.end(), [](const Victim& a, const Victim& b) { return a.Health < b.Health; });
		for (const Victim& victim : victims)
			ApplyDamage(world, victim.Handle, { .Amount = victim.Damage, .Damages = Element::Fire, .Unmitigated = true });
	}

	void MatchDirector::OfferBoons(World& world, uint32_t playerId)
	{
		BrawlLoadout& loadout = m_Brawlers[playerId].Loadout;
		std::vector<BoonId> candidates;
		for (const BoonDef& boon : Boons)
			if (loadout.Boons[size_t(boon.Id)] < boon.MaxPicks)
				candidates.push_back(boon.Id);
		Random& random = world.GetRandom();
		for (size_t i = candidates.size(); i > 1; i--)
			std::swap(candidates[i - 1], candidates[random.Below(uint32_t(i))]);
		for (size_t i = 0; i < BoonChoices; i++)
			loadout.Offer[i] = i < candidates.size() ? candidates[i] : BoonId::Count;
		loadout.OfferPending = !candidates.empty();
	}

	void MatchDirector::OnBrawlerKilled(World& world, Entity player, uint32_t victim, uint32_t killer)
	{
		m_Brawlers[victim].Burning = false;
		if (m_Brawl.Phase != BrawlPhase::Fight)
			return;
		MatchScore& score = m_Scores[victim];
		score.Deaths++;
		if (killer < MaxPlayers && killer != victim)
		{
			m_Scores[killer].Kills++;
			m_Brawlers[killer].Points += KnockoutPoints;
		}
		if (score.Entered && !score.Out)
		{
			score.Out = true;
			m_Fallen.push_back(victim);
		}
		if (PlayerController* controller = world.GetEntities().TryGet<PlayerController>(player))
			controller->Watching = killer < MaxPlayers && killer != victim ? killer : NoPlayer;
	}

	bool MatchDirector::PickSpell(World& world, Entity player, uint32_t playerId, uint32_t slot, AbilityId spell)
	{
		if (m_Mode != GameMode::Brawl || playerId >= MaxPlayers || slot >= SkillSlots || !IsBrawlSpell(spell) || m_Phase == MatchPhase::Ended)
			return false;
		if (m_Brawl.Phase != BrawlPhase::Draft && m_Brawl.Phase != BrawlPhase::Shop)
			return false;
		BrawlSpells& spells = m_Brawlers[playerId].Loadout.Spells;
		const auto existing = std::find(spells.begin(), spells.end(), spell);
		if (existing != spells.end())
			std::swap(*existing, spells[slot]);
		else
			spells[slot] = spell;
		world.RefreshTraits(player);
		return true;
	}

	bool MatchDirector::ChooseBoon(World& world, Entity player, uint32_t playerId, uint32_t choice)
	{
		if (m_Mode != GameMode::Brawl || playerId >= MaxPlayers || choice >= BoonChoices)
			return false;
		BrawlLoadout& loadout = m_Brawlers[playerId].Loadout;
		if (!loadout.OfferPending || loadout.Offer[choice] == BoonId::Count)
			return false;
		uint8_t& stacks = loadout.Boons[size_t(loadout.Offer[choice])];
		if (stacks >= GetBoon(loadout.Offer[choice]).MaxPicks)
			return false;
		stacks++;
		loadout.OfferPending = false;
		world.RefreshTraits(player);
		return true;
	}

	bool MatchDirector::SetReady(uint32_t playerId, bool ready)
	{
		if (m_Mode != GameMode::Brawl || playerId >= MaxPlayers || !m_Present[playerId] || m_Phase == MatchPhase::Ended)
			return false;
		if (m_Brawl.Phase != BrawlPhase::Draft && m_Brawl.Phase != BrawlPhase::Shop)
			return false;
		m_Brawlers[playerId].Loadout.Ready = ready;
		return true;
	}

	float MatchDirector::GetKnockbackScale(uint32_t attacker, uint32_t victim, float healthFraction) const
	{
		if (m_Mode != GameMode::Brawl)
			return 1.0f;
		const float dealt = attacker < MaxPlayers ? 1.0f + 0.12f * float(m_Brawlers[attacker].Loadout.Boons[size_t(BoonId::HeavyHand)]) : 1.0f;
		const float taken = victim < MaxPlayers ? 1.0f - 0.12f * float(m_Brawlers[victim].Loadout.Boons[size_t(BoonId::Anchored)]) : 1.0f;
		return dealt * taken * (1.0f + MissingHealthKnockback * (1.0f - std::clamp(healthFraction, 0.0f, 1.0f)));
	}
}
