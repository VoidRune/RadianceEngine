#include "MatchDirector.h"
#include "GameShared/Simulation/Combat.h"
#include "GameShared/Simulation/World.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <functional>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr float LavaTickSeconds = 0.25f;
		constexpr float ArenaWander = 1.5f;
		constexpr float SkySpawnMargin = 1.2f;
		constexpr float SkyCatchSearch = 6.0f;
		constexpr float SkyCatchGrace = 0.6f;
		constexpr float BoltHunt = 0.55f;
		constexpr float BoltScatter = 0.5f;
		constexpr float BoltSpacing = 2.6f;
		constexpr float StillReach = 1.0f;
		constexpr float StillSeconds = 2.0f;
		constexpr float StillHunt = 6.0f;
		constexpr float HeightHunt = 0.25f;

		bool SafeOnSky(const Level& level, const CollapsePlan& plan, float clock, Vec2 point, float margin)
		{
			if (IsBrawlLava(level, plan, clock, point))
				return false;
			for (int32_t i = 0; i < 8; i++)
				if (IsBrawlLava(level, plan, clock, point + FromAngle(float(i) * (Pi * 0.25f)) * margin))
					return false;
			return true;
		}
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
		m_Brawl.Plan.Shape = ShapeOf(world.GetLevel().Kind);
		const Vec2 wander = FromAngle(random.Float() * 2.0f * Pi) * (ArenaWander * std::sqrt(random.Float()));
		m_Brawl.Plan.Center = world.GetLevel().PlayerSpawn + (m_Brawl.Plan.Shape == BrawlShape::Peak ? Vec2{} : wander);
		m_Brawl.Plan.Seed = random.Next() | 1u;
		m_Brawl.Plan.CraterCount = 0;
		m_Brawl.Plan.BoltCount = 0;
		m_Brawl.Plan.Steady = !m_Rules.Crumble;
		m_Brawl.Clock = -1e5f;
		FitBrawlArena();
	}

	void MatchDirector::FitBrawlArena()
	{
		if (m_Brawl.Phase == BrawlPhase::Draft || m_Brawl.Phase == BrawlPhase::Pick)
			m_Brawl.Plan.StartRadius = BrawlStartRadius(CountPresent(), m_Brawl.Plan.Shape);
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
		if (IsTeamBrawl())
			std::stable_sort(players.begin(), players.end(), [&](Entity a, Entity b)
			{
				const PlayerController* first = entities.TryGet<PlayerController>(a);
				const PlayerController* second = entities.TryGet<PlayerController>(b);
				return (first ? GetTeam(first->PlayerId) : NoTeam) < (second ? GetTeam(second->PlayerId) : NoTeam);
			});
		const float turn = random.Float() * 2.0f * Pi;
		const float radius = SpawnRank(m_Brawl.Plan);
		const Level& level = world.GetLevel();
		const auto clearOfHoles = [&](Vec2 point)
		{
			for (uint32_t hole = 0; hole < HoleCount(m_Brawl.Plan); hole++)
				if (LengthSquared(point - HoleCenter(m_Brawl.Plan, hole)) < (HoleStartRadius + HoleSpawnClearance) * (HoleStartRadius + HoleSpawnClearance))
					return false;
			return !level.Skyborne || SafeOnSky(level, m_Brawl.Plan, 0.0f, point, SkySpawnMargin);
		};
		const std::vector<Vec2>& spawns = level.SpawnPoints;
		const bool peak = m_Brawl.Plan.Shape == BrawlShape::Peak && !spawns.empty();
		const size_t first = peak ? size_t(random.Below(uint32_t(spawns.size()))) : 0;
		for (size_t i = 0; i < players.size(); i++)
		{
			const Entity player = players[i];
			world.RespawnPlayer(player);
			Transform* transform = entities.TryGet<Transform>(player);
			PlayerController* controller = entities.TryGet<PlayerController>(player);
			if (!transform || !controller)
				continue;
			const float angle = turn + 2.0f * Pi * float(i) / float(players.size());
			Vec2 wanted = peak ? spawns[(first + i * spawns.size() / players.size()) % spawns.size()] : ArenaPoint(m_Brawl.Plan, angle, radius);
			for (int32_t step = 1; step <= 8 && !peak && !clearOfHoles(wanted); step++)
				wanted = ArenaPoint(m_Brawl.Plan, angle + float(step % 2 == 0 ? step / 2 : -(step + 1) / 2) * 0.16f, radius * (step >= 5 ? 0.8f : 1.0f));
			const Vec2 spot = world.FindFreeSpotNear(wanted, PlayerRadius);
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
		case BrawlPhase::Pick:
			if (m_Brawl.Phase == BrawlPhase::Pick || CountPresent() >= 2)
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
			UpdateCards(world, dt);
			const uint32_t alive = CountAlive();
			const bool decided = IsTeamBrawl() ? CountTeams(false) >= 2 && CountTeams(true) <= 1 : m_Entrants >= 2 && alive <= 1;
			if (alive == 0 || decided)
				EndBrawlRound(world);
			break;
		}
		case BrawlPhase::Over:
			m_Brawl.Clock += dt;
			m_Brawl.Timer -= dt;
			if (m_Brawl.Timer > 0.0f)
				break;
			if (m_Brawl.Round >= m_Brawl.Rounds || TeamClinched())
				FinishBrawl(world);
			else
				OpenBrawlPick(world);
			break;
		}
	}

	void MatchDirector::StartBrawlCountdown(World& world)
	{
		for (uint32_t id = 0; id < MaxPlayers; id++)
		{
			BrawlLoadout& loadout = m_Brawlers[id].Loadout;
			if (m_Present[id] && loadout.OfferPending)
				for (uint32_t i = 0; i < CardChoices; i++)
					if (loadout.Offer[i] != CardId::Count && ChooseCard(world, world.FindPlayer(id), id, i))
						break;
			loadout.Ready = false;
			loadout.OfferPending = false;
			m_Brawlers[id].Shield = IsDamageMode() && HasCard(loadout.Cards, CardId::Barrier) ? BarrierShield : 0.0f;
			if (m_Present[id] && m_Rules.RandomSpells)
			{
				loadout.Spells = DealSpells(world.GetRandom());
				world.RefreshTraits(world.FindPlayer(id));
				world.Notify(id, std::string("Your spells this round: ") + GetAbility(loadout.Spells[0]).Name + ", " + GetAbility(loadout.Spells[1]).Name + " and " + GetAbility(loadout.Spells[2]).Name, true);
			}
			if (m_Present[id])
				RollArcana(world, id, -1);
		}
		m_Brawl.Phase = BrawlPhase::Countdown;
		m_Brawl.Timer = BrawlCountdownSeconds;
		m_Brawl.Clock = -BrawlCountdownSeconds;
		m_Brawl.RoundWinner = NoPlayer;
		m_Brawl.RoundTeam = NoTeam;
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
		m_Ledgers.fill({});
		m_LavaTick = 0.0f;
		m_NextBolt = BoltFirst;
		m_Brawl.Plan.BoltCount = 0;
		world.GetEntities().Each<PlayerController>([&](Entity, PlayerController& controller)
		{
			if (controller.PlayerId >= MaxPlayers)
				return;
			MatchScore& score = m_Scores[controller.PlayerId];
			score.Entered = m_Present[controller.PlayerId] && controller.State.Alive;
			score.Out = false;
			m_Entrants += score.Entered ? 1u : 0u;
			if (score.Entered)
				m_Brawlers[controller.PlayerId].EnteredRounds |= uint16_t(1u << (m_Brawl.Round - 1u));
		});
		m_Flings.clear();
		for (Brawler& brawler : m_Brawlers)
		{
			brawler.RoundKnockouts = 0;
			brawler.RoundCasts = 0;
			brawler.Hooked = {};
			brawler.Swapped = {};
			brawler.Returned = {};
			brawler.Afterburn = 0.0f;
			brawler.LavaBank = 0.0f;
			brawler.Burning = false;
			brawler.FeatherUsed = false;
			brawler.FeatherGrace = 0.0f;
			brawler.EchoArmed = false;
			brawler.IgniteLeft = 0.0f;
			brawler.IgniteBank = 0.0f;
			brawler.IgniteBy = NoPlayer;
			brawler.MarkTime = 0.0f;
			brawler.MarkedBy = NoPlayer;
			brawler.SecondWindUsed = false;
		}
		m_Bursts.clear();
		m_IgniteTick = 0.0f;
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
		if (IsTeamBrawl())
		{
			uint8_t team = winner != NoPlayer ? m_Teams[winner] : !m_Fallen.empty() ? m_Teams[m_Fallen.back()] : NoTeam;
			if (CountTeams(false) < 2 || team >= MaxTeams)
				team = NoTeam;
			if (team != NoTeam)
			{
				m_TeamScores[team]++;
				for (uint32_t id = 0; id < MaxPlayers; id++)
					if (m_Present[id] && m_Teams[id] == team && m_Scores[id].Entered)
					{
						m_Brawlers[id].Points += RoundWinPoints;
						m_Brawlers[id].RoundWins++;
					}
			}
			winner = NoPlayer;
			m_Brawl.RoundTeam = team;
		}
		else
		{
			if (alive != 1)
				winner = alive == 0 && !m_Fallen.empty() ? m_Fallen.back() : NoPlayer;
			if (m_Entrants < 2)
				winner = NoPlayer;
			if (winner != NoPlayer)
			{
				if (IsDamageMode())
					AwardPlacements(alive);
				else
					m_Brawlers[winner].Points += RoundWinPoints;
				m_Brawlers[winner].RoundWins++;
			}
		}
		m_Brawl.RoundWinner = winner;
		for (uint32_t id = 0; id < MaxPlayers; id++)
			if (m_Present[id] && m_Scores[id].Entered && WonRound(id))
				NoteRoundWin(world, id);
		m_Brawl.Phase = BrawlPhase::Over;
		m_Brawl.Timer = BrawlOverSeconds;
		for (Brawler& brawler : m_Brawlers)
		{
			brawler.Burning = false;
			brawler.Afterburn = 0.0f;
			brawler.IgniteLeft = 0.0f;
			brawler.IgniteBank = 0.0f;
			brawler.MarkTime = 0.0f;
		}
		world.Emit({ WorldEventType::RoundEnded, winner, m_Brawl.Round });
	}

	void MatchDirector::OpenBrawlPick(World& world)
	{
		m_Brawl.Phase = BrawlPhase::Pick;
		m_Brawl.Timer = BrawlPickSeconds;
		PlanBrawlRound(world);
		for (uint32_t id = 0; id < MaxPlayers; id++)
		{
			m_Brawlers[id].Loadout.Ready = false;
			m_Brawlers[id].Loadout.OfferPending = false;
			if (m_Present[id] && (m_Rules.WinnerCard || !WonRound(id)))
				OfferCards(world, id);
		}
		m_CardRevision++;
		PlaceBrawlers(world);
	}

	bool MatchDirector::WonRound(uint32_t playerId) const
	{
		if (playerId >= MaxPlayers)
			return false;
		return IsTeamBrawl() ? m_Brawl.RoundTeam != NoTeam && m_Teams[playerId] == m_Brawl.RoundTeam : playerId == m_Brawl.RoundWinner;
	}

	bool MatchDirector::TeamClinched() const
	{
		if (!IsTeamBrawl() || m_Brawl.Round >= m_Brawl.Rounds)
			return false;
		std::array<uint16_t, MaxTeams> wins = m_TeamScores;
		std::sort(wins.begin(), wins.end(), std::greater<>());
		return wins[0] > wins[1] + uint32_t(m_Brawl.Rounds - m_Brawl.Round);
	}

	void MatchDirector::FinishBrawl(World& world)
	{
		if (IsTeamBrawl())
		{
			std::array<std::tuple<uint16_t, uint32_t, uint32_t>, MaxTeams> totals{};
			std::array<bool, MaxTeams> fielded{};
			for (uint32_t id = 0; id < MaxPlayers; id++)
				if (m_Present[id] && m_Teams[id] < MaxTeams)
				{
					fielded[m_Teams[id]] = true;
					std::get<1>(totals[m_Teams[id]]) += m_Scores[id].Kills;
					std::get<2>(totals[m_Teams[id]]) += m_Scores[id].Damage;
				}
			uint8_t best = NoTeam;
			bool tied = false;
			for (uint8_t team = 0; team < m_Rules.Teams; team++)
			{
				std::get<0>(totals[team]) = m_TeamScores[team];
				if (!fielded[team] && m_TeamScores[team] == 0)
					continue;
				if (best == NoTeam || totals[team] > totals[best])
				{
					best = team;
					tied = false;
				}
				else if (totals[team] == totals[best])
					tied = true;
			}
			if (!tied && best != NoTeam)
				for (uint32_t id = 0; id < MaxPlayers; id++)
					if (m_Present[id] && m_Teams[id] == best)
						NoteMatchWin(world, id);
			EndMatch(world, NoPlayer, tied ? NoTeam : best);
			return;
		}
		uint32_t best = NoPlayer;
		bool tied = false;
		const auto rank = [&](uint32_t id)
		{
			const Brawler& brawler = m_Brawlers[id];
			const MatchScore& score = m_Scores[id];
			return IsDamageMode() ? std::tuple(brawler.Points, score.Assists, uint32_t(brawler.RoundWins), score.Damage) : std::tuple(brawler.Points, uint16_t(brawler.RoundWins), uint32_t(score.Kills), 0u);
		};
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
		if (!tied && best != NoPlayer)
			NoteMatchWin(world, best);
		EndMatch(world, tied ? NoPlayer : best, NoTeam);
	}

	void MatchDirector::UpdateFalls(World& world, float dt)
	{
		std::vector<Entity> fallen;
		world.GetEntities().Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (controller.PlayerId >= MaxPlayers || !controller.State.Alive || !controller.State.IsFalling())
				return;
			Brawler& brawler = m_Brawlers[controller.PlayerId];
			if (!brawler.FeatherUsed && controller.State.Fall >= SkyCatchSeconds && HasCard(brawler.Loadout.Cards, CardId::Featherfall) && Catch(world, entity, controller))
			{
				brawler.FeatherUsed = true;
				return;
			}
			if (controller.State.Fall >= SkyFallSeconds)
				fallen.push_back(entity);
		});
		for (const Entity entity : fallen)
			world.KillPlayer(entity, NoPlayer, uint8_t(AbilityId::Count));
		if (m_Brawl.Plan.Shape == BrawlShape::Peak)
			UpdateBolts(world, dt);
	}

	void MatchDirector::UpdateBolts(World& world, float dt)
	{
		CollapsePlan& plan = m_Brawl.Plan;
		const float clock = m_Brawl.Clock;
		EntityRegistry& entities = world.GetEntities();
		std::vector<Entity> struck;
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			const Transform* transform = entities.TryGet<Transform>(entity);
			if (controller.PlayerId >= MaxPlayers || !controller.State.Alive || controller.State.IsFalling() || !transform)
				return;
			Brawler& brawler = m_Brawlers[controller.PlayerId];
			if (LengthSquared(transform->Position - brawler.Anchor) > StillReach * StillReach)
			{
				brawler.Anchor = transform->Position;
				brawler.Still = 0.0f;
			}
			else
				brawler.Still += dt;
			if (controller.State.Invulnerable <= 0.0f && BoltStriking(plan, transform->Position, clock - dt, clock))
				struck.push_back(entity);
		});
		for (const Entity entity : struck)
			ApplyDamage(world, entity, { .Amount = BoltDamage, .Damages = Element::Lightning, .Unmitigated = true });
		uint8_t kept = 0;
		for (const LightningBolt& bolt : BoltsOf(plan))
			if (bolt.Time > clock - BoltLinger)
				plan.Bolts[kept++] = bolt;
		plan.BoltCount = kept;
		if (clock < m_NextBolt || plan.BoltCount >= MaxBolts)
			return;
		Random& random = world.GetRandom();
		m_NextBolt = clock + BoltInterval(clock) * (0.75f + 0.5f * random.Float());
		if (const std::optional<Vec2> spot = AimBolt(world))
			plan.Bolts[plan.BoltCount++] = { *spot, clock + BoltWarning + 0.5f * dt };
	}

	std::optional<Vec2> MatchDirector::AimBolt(World& world)
	{
		Random& random = world.GetRandom();
		const Level& level = world.GetLevel();
		const CollapsePlan& plan = m_Brawl.Plan;
		const auto open = [&](Vec2 spot)
		{
			if (level.Relief.RimDistance(spot) < 0.3f)
				return false;
			for (const LightningBolt& bolt : BoltsOf(plan))
				if (bolt.Time > m_Brawl.Clock - 0.2f && LengthSquared(bolt.Center - spot) < BoltSpacing * BoltSpacing)
					return false;
			return true;
		};
		if (random.Float() < BoltHunt)
		{
			struct Mark
			{
				Vec2 Position;
				float Weight = 0.0f;
			};
			std::vector<Mark> marks;
			float total = 0.0f;
			world.GetEntities().Each<PlayerController>([&](Entity entity, PlayerController& controller)
			{
				const Transform* transform = world.GetEntities().TryGet<Transform>(entity);
				if (controller.PlayerId >= MaxPlayers || !controller.State.Alive || controller.State.IsFalling() || !transform)
					return;
				const float weight = 1.0f + StillHunt * std::min(m_Brawlers[controller.PlayerId].Still / StillSeconds, 1.0f) + HeightHunt * FloorHeight(level, transform->Position);
				marks.push_back({ transform->Position, weight });
				total += weight;
			});
			float pick = random.Float() * total;
			for (const Mark& mark : marks)
			{
				pick -= mark.Weight;
				if (pick > 0.0f)
					continue;
				const Vec2 spot = mark.Position + FromAngle(random.Float() * 2.0f * Pi) * (BoltScatter * random.Float());
				if (open(spot))
				{
					return spot;
				}
				break;
			}
		}
		const float reach = RingOuter;
		for (uint32_t attempt = 0; attempt < 32; attempt++)
		{
			const Vec2 spot = plan.Center + FromAngle(random.Float() * 2.0f * Pi) * (reach * std::sqrt(random.Float()));
			if (open(spot) && random.Float() < 0.3f + 0.7f * std::clamp(FloorHeight(level, spot) / (3.0f * TierHeight), 0.0f, 1.0f))
				return spot;
		}
		return std::nullopt;
	}

	bool MatchDirector::Catch(World& world, Entity entity, PlayerController& controller)
	{
		Transform* transform = world.GetEntities().TryGet<Transform>(entity);
		if (!transform)
			return false;
		const Level& level = world.GetLevel();
		const float footing = m_Brawl.Clock + FeatherfallFooting;
		const auto safe = [&](Vec2 spot) { return SafeOnSky(level, m_Brawl.Plan, m_Brawl.Clock, spot, 0.5f) && SafeOnSky(level, m_Brawl.Plan, footing, spot, 0.5f); };
		std::optional<Vec2> best;
		for (const Vec2 origin : { controller.State.Ledge, transform->Position })
			for (float radius = 0.0f; radius <= SkyCatchSearch && !best; radius += 0.5f)
				for (uint32_t step = 0; step < (radius > 0.0f ? 24u : 1u) && !best; step++)
					if (const Vec2 spot = origin + FromAngle(2.0f * Pi * float(step) / 24.0f) * radius; safe(spot))
						best = spot;
		if (!best)
			return false;
		transform->Position = *best;
		controller.State.Fall = 0.0f;
		controller.State.Push = {};
		controller.State.Ledge = *best;
		controller.State.Invulnerable = std::max(controller.State.Invulnerable, SkyCatchGrace);
		if (MotionHistory* history = world.GetEntities().TryGet<MotionHistory>(entity))
		{
			history->Positions.fill(*best);
			history->FirstTick = world.GetTick() + 1;
		}
		world.SpawnEffect(AbilityId::Gust, *best, 0.0f, controller.PlayerId, 0, 0.7f);
		world.Notify(controller.PlayerId, "Featherfall caught you", true);
		return true;
	}

	void MatchDirector::UpdateLava(World& world, float dt)
	{
		const bool peak = m_Brawl.Plan.Shape == BrawlShape::Peak;
		if (world.GetLevel().Skyborne)
		{
			UpdateFalls(world, dt);
			if (!peak)
				return;
		}
		EntityRegistry& entities = world.GetEntities();
		const Level& level = world.GetLevel();
		const StormRing ring = RingAt(m_Brawl.Plan, m_Brawl.Clock);
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
			if (brawler.FeatherGrace > 0.0f)
			{
				brawler.FeatherGrace = std::max(brawler.FeatherGrace - dt, 0.0f);
				brawler.Burning = false;
				brawler.Afterburn = 0.0f;
				return;
			}
			const bool lava = peak ? !controller.State.IsFalling() && ring.Outside(transform->Position) : controller.State.DashDistance <= 0.0f && IsBrawlLava(level, m_Brawl, transform->Position);
			if (lava && !peak && !brawler.FeatherUsed && m_Brawl.Phase == BrawlPhase::Fight && HasCard(brawler.Loadout.Cards, CardId::Featherfall) && Featherfall(world, controller, transform->Position))
			{
				brawler.FeatherUsed = true;
				brawler.FeatherGrace = FeatherfallGrace;
				return;
			}
			float rate = 0.0f;
			if (lava)
			{
				rate = peak ? ring.Damage : level.Kind == MapKind::FrozenLake ? WaterDamagePerSecond : LavaDamagePerSecond;
				brawler.Afterburn = peak ? 0.0f : AfterburnSeconds;
			}
			else if (brawler.Afterburn > 0.0f)
			{
				brawler.Afterburn = std::max(brawler.Afterburn - dt, 0.0f);
				rate = AfterburnDamagePerSecond;
			}
			brawler.Burning = rate > 0.0f;
			brawler.LavaBank += rate * dt;
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
		const Element element = peak ? Element::Lightning : level.Kind == MapKind::FrozenLake ? Element::Cold : Element::Fire;
		for (const Victim& victim : victims)
			ApplyDamage(world, victim.Handle, { .Amount = victim.Damage, .Damages = element, .Unmitigated = true });
	}

	void MatchDirector::OfferApexPick(World& world, uint32_t playerId)
	{
		BrawlLoadout& loadout = m_Brawlers[playerId].Loadout;
		std::vector<CardId> apexes;
		for (const CardDef& card : Cards)
			if (card.Rarity == CardRarity::Apex && CardFitsLoadout(loadout, card.Id, m_Rules.Mode))
				apexes.push_back(card.Id);
		Random& random = world.GetRandom();
		loadout.Offer.fill(CardId::Count);
		for (size_t slot = 0; slot < CardChoices && !apexes.empty(); slot++)
		{
			const size_t chosen = random.Below(uint32_t(apexes.size()));
			loadout.Offer[slot] = apexes[chosen];
			apexes.erase(apexes.begin() + ptrdiff_t(chosen));
		}
		loadout.OfferPending = loadout.Offer[0] != CardId::Count;
	}

	void MatchDirector::OfferCards(World& world, uint32_t playerId)
	{
		BrawlLoadout& loadout = m_Brawlers[playerId].Loadout;
		if (IsApexDraft() && (loadout.Cards & ApexMask) == 0)
		{
			OfferApexPick(world, playerId);
			return;
		}
		uint16_t leader = 0;
		for (uint32_t id = 0; id < MaxPlayers; id++)
			if (m_Present[id])
				leader = std::max(leader, m_Brawlers[id].Points);
		const float behind = std::clamp(float(leader - m_Brawlers[playerId].Points) / float(std::max<uint16_t>(leader, 3)), 0.0f, 1.0f);
		const bool tagged = m_Rules.Mode == BrawlMode::Damage;
		const std::array<float, 3> classes = tagged ? OfferClassWeights(m_Brawl.Round, behind) : std::array<float, 3>{ 60.0f - 35.0f * behind, 30.0f + 15.0f * behind, 10.0f + 20.0f * behind };
		struct Candidate
		{
			CardId Id;
			size_t Rarity = 0;
			float Affinity = 1.0f;
		};
		const auto affinityOf = [&](const CardDef& card)
		{
			uint32_t kin = 0;
			for (const CardDef& owned : Cards)
				kin += HasCard(loadout.Cards, owned.Id) && (owned.Tags & card.Tags) != 0 ? 1u : 0u;
			return 1.0f + TagAffinity * float(kin);
		};
		std::vector<Candidate> candidates;
		for (const CardDef& card : Cards)
			if (card.Rarity != CardRarity::Apex && CardFitsLoadout(loadout, card.Id, m_Rules.Mode) && (!m_Rules.RandomSpells || !NeedsChosenSpell(card)))
				candidates.push_back({ card.Id, size_t(card.Rarity), affinityOf(card) });
		Random& random = world.GetRandom();
		loadout.Offer.fill(CardId::Count);
		for (size_t slot = 0; slot < CardChoices && !candidates.empty(); slot++)
		{
			float affinity[3] = {};
			for (const Candidate& candidate : candidates)
				affinity[candidate.Rarity] += candidate.Affinity;
			const auto weight = [&](const Candidate& candidate) { return tagged ? classes[candidate.Rarity] * candidate.Affinity / affinity[candidate.Rarity] : classes[candidate.Rarity]; };
			float total = 0.0f;
			for (const Candidate& candidate : candidates)
				total += weight(candidate);
			float roll = random.Float() * total;
			size_t chosen = candidates.size() - 1;
			for (size_t i = 0; i < candidates.size(); i++)
			{
				roll -= weight(candidates[i]);
				if (roll < 0.0f)
				{
					chosen = i;
					break;
				}
			}
			loadout.Offer[slot] = candidates[chosen].Id;
			candidates.erase(candidates.begin() + ptrdiff_t(chosen));
		}
		if (tagged && m_Rules.Apex && uint32_t(m_Brawl.Round) + 1 >= ApexFromRound && (loadout.Cards & ApexMask) == 0 && random.Float() < ApexChance + ApexCatchUp * behind)
		{
			std::vector<std::pair<CardId, float>> apexes;
			float total = 0.0f;
			for (const CardDef& card : Cards)
				if (card.Rarity == CardRarity::Apex && CardFitsLoadout(loadout, card.Id, m_Rules.Mode))
				{
					apexes.push_back({ card.Id, affinityOf(card) });
					total += apexes.back().second;
				}
			float roll = random.Float() * total;
			for (const auto& [card, weight] : apexes)
				if ((roll -= weight) < 0.0f || card == apexes.back().first)
				{
					loadout.Offer[loadout.Offer[1] != CardId::Count ? 1 : 0] = card;
					break;
				}
		}
		loadout.OfferPending = loadout.Offer[0] != CardId::Count;
	}

	PlayerTraits MatchDirector::GetBrawlTraits(uint32_t playerId) const
	{
		const Brawler& brawler = m_Brawlers[playerId < MaxPlayers ? playerId : 0];
		if (!IsDamageMode() || !HasCard(brawler.Loadout.Cards, CardId::UnstableArcana) || brawler.Arcana[0] == AbilityId::Count)
			return MakeBrawlTraits(brawler.Loadout, m_Rules);
		BrawlLoadout rolled = brawler.Loadout;
		rolled.Spells = brawler.Arcana;
		return MakeBrawlTraits(rolled, m_Rules);
	}

	void MatchDirector::RollArcana(World& world, uint32_t playerId, int32_t slot)
	{
		if (!IsDamageMode() || playerId >= MaxPlayers || !HasCard(m_Brawlers[playerId].Loadout.Cards, CardId::UnstableArcana))
			return;
		BrawlSpells& arcana = m_Brawlers[playerId].Arcana;
		if (slot < 0)
			arcana.fill(AbilityId::Count);
		Random& random = world.GetRandom();
		for (uint32_t i = 0; i < SkillSlots; i++)
		{
			if (slot >= 0 && uint32_t(slot) != i)
				continue;
			std::array<AbilityId, std::size(BrawlSpellPool)> options{};
			uint32_t count = 0;
			for (const AbilityId spell : BrawlSpellPool)
				if (GetAbility(spell).BrawlDamage > 0 && std::none_of(arcana.begin(), arcana.end(), [&](AbilityId held) { return held == spell && &held != &arcana[i]; }))
					options[count++] = spell;
			arcana[i] = options[random.Below(count)];
		}
		world.RefreshTraits(world.FindPlayer(playerId));
	}

	bool MatchDirector::Featherfall(World& world, PlayerController& controller, Vec2 position)
	{
		const Level& level = world.GetLevel();
		const auto safe = [&](Vec2 spot)
		{
			const CellCoord cell = CellAt(spot);
			return level.Map.IsWalkableAt(spot) && !IsBrawlLava(level, m_Brawl, spot) && TileCollapseTime(m_Brawl.Plan, cell.X, cell.Y) > m_Brawl.Clock + FeatherfallFooting;
		};
		for (float radius = 0.5f; radius <= FeatherfallSearch; radius += 0.5f)
		{
			std::optional<Vec2> best;
			for (uint32_t step = 0; step < 24; step++)
			{
				const Vec2 spot = position + FromAngle(2.0f * Pi * float(step) / 24.0f) * radius;
				if (safe(spot) && (!best || LengthSquared(spot - m_Brawl.Plan.Center) < LengthSquared(*best - m_Brawl.Plan.Center)))
					best = spot;
			}
			if (!best)
				continue;
			const Vec2 offset = *best - position;
			controller.State.Push = ClampLength(Normalize(offset) * ((Length(offset) + FeatherfallMargin) * PushFriction), MaxPush);
			controller.State.DashDistance = 0.0f;
			m_Brawlers[controller.PlayerId].Afterburn = 0.0f;
			world.SpawnEffect(AbilityId::Gust, position, AngleOf(offset), controller.PlayerId, 0, 0.7f);
			world.Notify(controller.PlayerId, "Featherfall caught you", true);
			return true;
		}
		return false;
	}

	void MatchDirector::AddCrater(Vec2 center, float radius)
	{
		if (m_Mode != GameMode::Brawl || m_Brawl.Phase != BrawlPhase::Fight || m_Brawl.Plan.CraterCount >= MaxCraters)
			return;
		m_Brawl.Plan.Craters[m_Brawl.Plan.CraterCount++] = { center, radius * (IsDamageMode() ? DamageCraterScale : 1.0f), m_Brawl.Clock + (IsDamageMode() ? DamageCraterDelay : CraterDelay) };
	}

	CardMask MatchDirector::GetCards(uint32_t playerId) const
	{
		return m_Mode == GameMode::Brawl && playerId < MaxPlayers ? m_Brawlers[playerId].Loadout.Cards : 0;
	}

	void MatchDirector::ArmEcho(uint32_t playerId)
	{
		if (HasCard(GetCards(playerId), CardId::Echo))
			m_Brawlers[playerId].EchoArmed = true;
	}

	bool MatchDirector::ConsumeEcho(uint32_t playerId)
	{
		return playerId < MaxPlayers && std::exchange(m_Brawlers[playerId].EchoArmed, false);
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
		if (IsDamageMode())
		{
			AwardAssists(victim, killer);
			m_Ledgers[victim] = {};
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
		if (m_Mode != GameMode::Brawl || m_Rules.RandomSpells || playerId >= MaxPlayers || slot >= SkillSlots || !IsBrawlSpell(spell) || m_Phase == MatchPhase::Ended)
			return false;
		if (m_Brawl.Phase != BrawlPhase::Draft && m_Brawl.Phase != BrawlPhase::Pick)
			return false;
		if (!AssignSpell(m_Brawlers[playerId].Loadout.Spells, slot, spell))
			return false;
		world.RefreshTraits(player);
		return true;
	}

	bool MatchDirector::ChooseCard(World& world, Entity player, uint32_t playerId, uint32_t choice)
	{
		if (m_Mode != GameMode::Brawl || playerId >= MaxPlayers || choice >= CardChoices)
			return false;
		BrawlLoadout& loadout = m_Brawlers[playerId].Loadout;
		if (!loadout.OfferPending || loadout.Offer[choice] == CardId::Count || HasCard(loadout.Cards, loadout.Offer[choice]))
			return false;
		const CardId taken = loadout.Offer[choice];
		loadout.Cards |= CardBit(taken);
		loadout.OfferPending = false;
		m_CardRevision++;
		world.RefreshTraits(player);
		NoteDeck(world, playerId, taken);
		return true;
	}

	bool MatchDirector::GrantCard(World& world, Entity player, uint32_t playerId, CardId card)
	{
		if (m_Mode != GameMode::Brawl || playerId >= MaxPlayers || card >= CardId::Count)
			return false;
		m_Brawlers[playerId].Loadout.Cards |= CardBit(card);
		m_CardRevision++;
		world.RefreshTraits(player);
		NoteDeck(world, playerId, card);
		return true;
	}

	bool MatchDirector::PresentCards(uint32_t playerId, const CardOffer& offer)
	{
		if (m_Mode != GameMode::Brawl || playerId >= MaxPlayers || !m_Present[playerId] || m_Brawl.Phase != BrawlPhase::Pick || offer[0] == CardId::Count)
			return false;
		BrawlLoadout& loadout = m_Brawlers[playerId].Loadout;
		loadout.Offer = offer;
		loadout.OfferPending = true;
		m_CardRevision++;
		return true;
	}

	bool MatchDirector::SetReady(uint32_t playerId, bool ready)
	{
		if (m_Mode != GameMode::Brawl || playerId >= MaxPlayers || !m_Present[playerId] || m_Phase == MatchPhase::Ended)
			return false;
		if (m_Brawl.Phase != BrawlPhase::Draft && m_Brawl.Phase != BrawlPhase::Pick)
			return false;
		m_Brawlers[playerId].Loadout.Ready = ready;
		return true;
	}

	float MatchDirector::GetKnockbackScale(uint32_t attacker, uint32_t victim, float healthFraction, bool pull) const
	{
		if (m_Mode != GameMode::Brawl)
			return 1.0f;
		const CardMask dealer = GetCards(attacker);
		const CardMask target = GetCards(victim);
		const float custom = float(m_Rules.Knockback) / 100.0f;
		if (m_Rules.Mode == BrawlMode::Damage)
			return (pull ? DamagePullScale : DamagePushScale) * custom;
		const float dealt = HasCard(dealer, CardId::GlassCannon) ? GlassCannonDealt : 1.0f;
		const float taken = (HasCard(target, CardId::GlassCannon) ? GlassCannonTaken : 1.0f) * (HasCard(target, CardId::HeavyRobes) ? HeavyRobesTaken : 1.0f);
		return custom * dealt * taken * (pull ? 1.0f : 1.0f + MissingHealthKnockback * (1.0f - std::clamp(healthFraction, 0.0f, 1.0f)));
	}

	void MatchDirector::RecordDamage(uint32_t victim, uint32_t attacker, int32_t amount, uint8_t ability)
	{
		if (!IsDamageMode() || m_Brawl.Phase != BrawlPhase::Fight || victim >= MaxPlayers || attacker >= MaxPlayers || attacker == victim || amount <= 0 || !m_Present[attacker])
			return;
		m_Scores[attacker].Damage += uint32_t(amount);
		DamageLedger& ledger = m_Ledgers[victim];
		ledger.Entries[ledger.Next] = { attacker, float(amount), m_Brawl.Clock, uint8_t(CreditOf(AbilityId(ability))) };
		ledger.Next = (ledger.Next + 1) % uint32_t(LedgerSize);
	}

	uint32_t MatchDirector::CreditKill(uint32_t victim, uint8_t& ability) const
	{
		if (victim >= MaxPlayers)
			return NoPlayer;
		std::array<float, MaxPlayers> dealt{};
		std::array<float, MaxPlayers> latest{};
		latest.fill(-1e9f);
		std::array<uint8_t, MaxPlayers> last{};
		for (const DamageEntry& entry : m_Ledgers[victim].Entries)
		{
			if (entry.Attacker >= MaxPlayers || entry.Amount <= 0.0f || entry.Time < m_Brawl.Clock - DamageCreditSeconds || !m_Present[entry.Attacker])
				continue;
			dealt[entry.Attacker] += entry.Amount;
			if (entry.Time >= latest[entry.Attacker])
			{
				latest[entry.Attacker] = entry.Time;
				last[entry.Attacker] = entry.Ability;
			}
		}
		uint32_t best = NoPlayer;
		for (uint32_t id = 0; id < MaxPlayers; id++)
			if (dealt[id] > 0.0f && (best == NoPlayer || dealt[id] > dealt[best] || (dealt[id] == dealt[best] && latest[id] > latest[best])))
				best = id;
		if (best != NoPlayer)
			ability = last[best];
		return best;
	}

	void MatchDirector::AwardAssists(uint32_t victim, uint32_t killer)
	{
		std::array<float, MaxPlayers> dealt{};
		float total = 0.0f;
		for (const DamageEntry& entry : m_Ledgers[victim].Entries)
		{
			if (entry.Attacker >= MaxPlayers || entry.Amount <= 0.0f || entry.Time < m_Brawl.Clock - DamageCreditSeconds || !m_Present[entry.Attacker])
				continue;
			dealt[entry.Attacker] += entry.Amount;
			total += entry.Amount;
		}
		if (total <= 0.0f)
			return;
		for (uint32_t id = 0; id < MaxPlayers; id++)
			if (id != killer && id != victim && dealt[id] >= AssistShare * total)
				m_Scores[id].Assists++;
	}

	void MatchDirector::AwardPlacements(uint32_t alive)
	{
		std::vector<uint32_t> order;
		if (alive == 1)
			for (uint32_t id = 0; id < MaxPlayers; id++)
				if (m_Scores[id].Entered && !m_Scores[id].Out)
					order.push_back(id);
		order.insert(order.end(), m_Fallen.rbegin(), m_Fallen.rend());
		for (size_t rank = 0; rank < order.size() && rank < std::size(PlacementPoints) && rank + 1 < m_Entrants; rank++)
			if (m_Present[order[rank]])
				m_Brawlers[order[rank]].Points += PlacementPoints[rank];
	}

	float MatchDirector::CastDamageScale(uint32_t playerId, float healthFraction, float manaBefore, float maxMana, bool overloaded, float still) const
	{
		if (!IsDamageMode() || playerId >= MaxPlayers)
			return 1.0f;
		const CardMask cards = m_Brawlers[playerId].Loadout.Cards;
		float scale = overloaded ? OverloadScale : 1.0f;
		if (HasCard(cards, CardId::Brimming) && maxMana > 0.0f && manaBefore >= BrimmingShare * maxMana)
			scale *= BrimmingScale;
		if (HasCard(cards, CardId::LastStand) && healthFraction < LastStandHealth)
			scale *= LastStandScale;
		if (HasCard(cards, CardId::ManaSingularity))
			scale *= 1.0f + ManaSingularityDamage * manaBefore;
		if (HasCard(cards, CardId::BloodMagic))
			scale *= BloodMagicDamage;
		if (HasCard(cards, CardId::Leyline) && still >= LeylineSeconds)
			scale *= LeylineDamage;
		return scale;
	}

	int32_t MatchDirector::ShapeDamage(PlayerState& state, uint32_t victim, uint32_t attacker, uint8_t ability, bool unmitigated, bool chilled, int32_t amount)
	{
		if (!IsDamageMode() || victim >= MaxPlayers || amount <= 0)
			return amount;
		Brawler& target = m_Brawlers[victim];
		float scaled = float(amount);
		if (!unmitigated)
		{
			const CardMask taken = target.Loadout.Cards;
			scaled *= (HasCard(taken, CardId::GlassCannon) ? GlassCannonDamageTaken : 1.0f) * (HasCard(taken, CardId::HeavyRobes) ? HeavyRobesDamageTaken : 1.0f);
			if (attacker < MaxPlayers && attacker != victim)
			{
				scaled *= float(m_Rules.Damage) / 100.0f;
				const CardMask dealt = m_Brawlers[attacker].Loadout.Cards;
				if (HasCard(dealt, CardId::GlassCannon))
					scaled *= GlassCannonDamageDealt;
				if (HasCard(dealt, CardId::Brittle) && chilled)
					scaled *= BrittleScale;
				if (HasCard(dealt, CardId::Conductive) && CreditOf(AbilityId(ability)) == AbilityId::EmberBolt && target.MarkTime > 0.0f && target.MarkedBy == attacker)
				{
					scaled *= ConductiveScale;
					target.MarkTime = 0.0f;
					target.MarkedBy = NoPlayer;
				}
			}
		}
		int32_t result = std::max(1, int32_t(std::lround(scaled)));
		if (target.Shield > 0.0f)
		{
			const int32_t absorbed = std::min(result, int32_t(std::ceil(target.Shield)));
			target.Shield = std::max(target.Shield - float(absorbed), 0.0f);
			result -= absorbed;
		}
		if (HasCard(target.Loadout.Cards, CardId::MindOverMatter))
		{
			state.ManaStall = MindOverMatterStall;
			const int32_t absorbed = std::min(result, int32_t(std::floor(state.Mana / MindOverMatterRatio)));
			state.Mana = std::max(state.Mana - float(absorbed) * MindOverMatterRatio, 0.0f);
			result -= absorbed;
		}
		return result;
	}

	void MatchDirector::AfterDamage(World& world, Entity target, uint32_t victim, uint32_t attacker, uint8_t ability, int32_t dealt)
	{
		if (!IsDamageMode() || m_Brawl.Phase != BrawlPhase::Fight || victim >= MaxPlayers)
			return;
		EntityRegistry& entities = world.GetEntities();
		const AbilityId source = CreditOf(AbilityId(ability));
		if (attacker < MaxPlayers && attacker != victim && dealt > 0 && source != AbilityId::Ignite && source != AbilityId::Count)
		{
			const CardMask cards = m_Brawlers[attacker].Loadout.Cards;
			const Entity caster = world.FindPlayer(attacker);
			PlayerController* controller = entities.TryGet<PlayerController>(caster);
			Health* health = entities.TryGet<Health>(caster);
			if (controller && health && controller->State.Alive)
			{
				if (HasCard(cards, CardId::Siphon))
					controller->State.Mana = std::min(controller->Traits.MaxMana, controller->State.Mana + SiphonMana);
				if (HasCard(cards, CardId::VampiricEmbers) && source == AbilityId::EmberBolt)
					health->Current = std::min(health->Max, health->Current + VampiricHeal);
				if (HasCard(cards, CardId::BloodMagic))
					health->Current = std::min(health->Max, health->Current + int32_t(std::lround(float(dealt) * BloodMagicHeal)));
			}
			if (HasCard(cards, CardId::Kindling) && source != AbilityId::EmberBolt)
				Ignite(victim, attacker, KindlingDamage, KindlingSeconds);
			if (HasCard(cards, CardId::Conductive) && (source == AbilityId::ArcLightning || source == AbilityId::StormStrike))
			{
				m_Brawlers[victim].MarkedBy = attacker;
				m_Brawlers[victim].MarkTime = ConductiveSeconds;
			}
		}
		Brawler& brawler = m_Brawlers[victim];
		if (PlayerController* hurt = entities.TryGet<PlayerController>(target); hurt && dealt > 0 && HasCard(brawler.Loadout.Cards, CardId::ManaSingularity))
			hurt->State.Mana = std::max(hurt->State.Mana - float(dealt) * ManaSingularityDrain, 0.0f);
		Health* life = entities.TryGet<Health>(target);
		if (life && life->Current > 0 && !brawler.SecondWindUsed && HasCard(brawler.Loadout.Cards, CardId::SecondWind) && float(life->Current) < SecondWindHealth * float(life->Max))
		{
			brawler.SecondWindUsed = true;
			life->Current = std::min(life->Max, life->Current + SecondWindHeal);
			world.Notify(victim, "Second Wind", true);
		}
	}

	void MatchDirector::Ignite(uint32_t victim, uint32_t attacker, float damage, float seconds)
	{
		if (victim >= MaxPlayers || attacker == victim)
			return;
		Brawler& brawler = m_Brawlers[victim];
		brawler.IgniteLeft = std::max(brawler.IgniteLeft, damage);
		brawler.IgniteRate = damage / std::max(seconds, 0.1f);
		brawler.IgniteBy = attacker;
	}

	void MatchDirector::NoteDeath(uint32_t victim, uint32_t killer, Vec2 position, bool chilled)
	{
		if (!IsDamageMode() || victim >= MaxPlayers || m_Brawl.Phase != BrawlPhase::Fight)
			return;
		Brawler& brawler = m_Brawlers[victim];
		if (killer < MaxPlayers && killer != victim)
		{
			const CardMask cards = m_Brawlers[killer].Loadout.Cards;
			if (HasCard(cards, CardId::Wildfire) && (brawler.IgniteLeft > 0.0f || brawler.Burning))
				m_Bursts.push_back({ position, killer, CardId::Wildfire });
			if (HasCard(cards, CardId::Shatter) && chilled)
				m_Bursts.push_back({ position, killer, CardId::Shatter });
		}
		brawler.IgniteLeft = 0.0f;
		brawler.IgniteBank = 0.0f;
		brawler.IgniteBy = NoPlayer;
		brawler.MarkTime = 0.0f;
		brawler.MarkedBy = NoPlayer;
		brawler.Shield = 0.0f;
	}

	void MatchDirector::UpdateCards(World& world, float dt)
	{
		if (!IsDamageMode())
			return;
		EntityRegistry& entities = world.GetEntities();
		for (Brawler& brawler : m_Brawlers)
		{
			brawler.MarkTime = std::max(brawler.MarkTime - dt, 0.0f);
			if (brawler.IgniteLeft <= 0.0f)
				continue;
			const float burn = std::min(brawler.IgniteLeft, brawler.IgniteRate * dt);
			brawler.IgniteLeft -= burn;
			brawler.IgniteBank += burn;
		}
		m_IgniteTick += dt;
		if (m_IgniteTick >= LavaTickSeconds)
		{
			m_IgniteTick -= LavaTickSeconds;
			struct Tick
			{
				Entity Handle;
				uint32_t By = NoPlayer;
				int32_t Damage = 0;
			};
			std::vector<Tick> ticks;
			entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
			{
				if (controller.PlayerId >= MaxPlayers || !controller.State.Alive)
					return;
				Brawler& brawler = m_Brawlers[controller.PlayerId];
				const int32_t damage = int32_t(brawler.IgniteBank);
				if (damage <= 0)
					return;
				brawler.IgniteBank -= float(damage);
				ticks.push_back({ entity, brawler.IgniteBy, damage });
			});
			for (const Tick& tick : ticks)
				ApplyDamage(world, tick.Handle, { .Amount = tick.Damage, .Attacker = tick.By, .Damages = Element::Fire, .Ability = uint8_t(AbilityId::Ignite) });
		}
		const std::vector<Burst> bursts = std::exchange(m_Bursts, {});
		for (const Burst& burst : bursts)
		{
			if (burst.Card == CardId::Wildfire)
			{
				const AbilityDef& flame = GetAbility(AbilityId::FirebombSkip2);
				world.SpawnEffect(AbilityId::FirebombSkip2, burst.Center, 0.0f, burst.Owner, 0, WildfireRadius / flame.ImpactRadius);
				entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
				{
					const Transform* transform = entities.TryGet<Transform>(entity);
					if (controller.PlayerId < MaxPlayers && controller.PlayerId != burst.Owner && controller.State.Alive && transform && LengthSquared(transform->Position - burst.Center) <= WildfireRadius * WildfireRadius)
						Ignite(controller.PlayerId, burst.Owner, WildfireDamage, WildfireSeconds);
				});
			}
			else if (burst.Card == CardId::Shatter)
			{
				const AbilityDef& shatter = GetAbility(AbilityId::ShatterBurst);
				const HitInfo hit{ .Damage = ShatterDamage, .Attacker = burst.Owner, .Damages = Element::Cold, .Ability = uint8_t(AbilityId::ShatterBurst), .Push = 8.0f, .Mode = PushMode::Radial, .PushOrigin = burst.Center, .PushReach = shatter.ImpactRadius };
				StrikeCircle(world, burst.Center, shatter.ImpactRadius, hit, Team::Players, 0.0f);
				world.SpawnEffect(AbilityId::ShatterBurst, burst.Center, 0.0f, burst.Owner, 0, 1.0f);
			}
		}
	}

	void MatchDirector::NoteCast(uint32_t playerId)
	{
		if (TracksFeats() && playerId < MaxPlayers)
			m_Brawlers[playerId].RoundCasts++;
	}

	void MatchDirector::NoteBlow(uint32_t victim, uint32_t attacker, uint8_t ability, bool pull, uint32_t returned)
	{
		if (!TracksFeats() || victim >= MaxPlayers || attacker >= MaxPlayers || attacker == victim)
			return;
		Brawler& target = m_Brawlers[victim];
		if (pull && CreditOf(AbilityId(ability)) == AbilityId::ChainHook)
			target.Hooked = { attacker, m_Brawl.Clock };
		if (returned == victim)
			target.Returned = { attacker, m_Brawl.Clock };
	}

	void MatchDirector::NoteFling(World& world, uint32_t victim, uint32_t attacker, uint8_t ability)
	{
		if (!TracksFeats() || victim >= MaxPlayers || attacker >= MaxPlayers || attacker == victim)
			return;
		if (m_FlingTick != world.GetTick())
		{
			m_FlingTick = world.GetTick();
			m_Flings.clear();
		}
		const uint8_t source = uint8_t(CreditOf(AbilityId(ability)));
		auto fling = std::find_if(m_Flings.begin(), m_Flings.end(), [&](const Fling& entry) { return entry.Attacker == attacker && entry.Ability == source; });
		if (fling == m_Flings.end())
			fling = m_Flings.insert(m_Flings.end(), { attacker, source, 0u });
		const uint32_t before = fling->Victims;
		fling->Victims |= 1u << victim;
		if (fling->Victims != before && uint32_t(std::popcount(fling->Victims)) == BowlingFlings)
			world.Achieve(attacker, Achievement::BowlingNight);
	}

	void MatchDirector::NoteSwap(World& world, uint32_t victim, uint32_t attacker, Vec2 landing)
	{
		if (TracksFeats() && victim < MaxPlayers && attacker < MaxPlayers && attacker != victim && IsBrawlLava(world.GetLevel(), m_Brawl, landing))
			m_Brawlers[victim].Swapped = { attacker, m_Brawl.Clock };
	}

	void MatchDirector::NoteKnockout(World& world, uint32_t victim, uint32_t killer, bool hazard)
	{
		if (!TracksFeats() || victim >= MaxPlayers)
			return;
		Brawler& fallen = m_Brawlers[victim];
		const auto recent = [&](const FeatMark& mark) { return mark.By < MaxPlayers && mark.By != victim && m_Brawl.Clock - mark.At <= KillCreditSeconds; };
		if (hazard && recent(fallen.Hooked))
			world.Achieve(fallen.Hooked.By, Achievement::Hooked);
		if (hazard && recent(fallen.Swapped))
			world.Achieve(fallen.Swapped.By, Achievement::SwapMeet);
		if (killer < MaxPlayers && killer != victim)
		{
			if (hazard)
				world.Achieve(killer, Achievement::FirstFling);
			if (recent(fallen.Returned) && fallen.Returned.By == killer)
				world.Achieve(killer, Achievement::ReturnToSender);
			if (++m_Brawlers[killer].RoundKnockouts == HatTrickKnockouts)
				world.Achieve(killer, Achievement::HatTrick);
		}
		fallen.Hooked = {};
		fallen.Swapped = {};
		fallen.Returned = {};
	}

	void MatchDirector::NoteRoundWin(World& world, uint32_t playerId)
	{
		Brawler& brawler = m_Brawlers[playerId];
		brawler.WonRounds |= uint16_t(1u << (m_Brawl.Round - 1u));
		world.Achieve(playerId, Achievement::Scholar, SpellPoolBits(brawler.Loadout.Spells));
		if (m_Scores[playerId].Out)
			return;
		if (brawler.RoundCasts == 0)
			world.Achieve(playerId, Achievement::Pacifist);
		const Health* health = world.GetEntities().TryGet<Health>(world.FindPlayer(playerId));
		if (health && health->Current > 0 && health->Current < ThreadHealth)
			world.Achieve(playerId, Achievement::ByAThread);
	}

	void MatchDirector::NoteMatchWin(World& world, uint32_t playerId)
	{
		constexpr uint16_t Opening = 0b11;
		const Brawler& brawler = m_Brawlers[playerId];
		if (m_Brawl.Round >= RoundsForComeback && (brawler.EnteredRounds & Opening) == Opening && (brawler.WonRounds & Opening) == 0)
			world.Achieve(playerId, Achievement::ComebackMage);
	}

	void MatchDirector::NoteDeck(World& world, uint32_t playerId, CardId card)
	{
		const CardDef& taken = GetCard(card);
		if (taken.Kind != CardKind::Upgrade || taken.Spell == AbilityId::Count)
			return;
		const CardMask cards = m_Brawlers[playerId].Loadout.Cards;
		uint32_t stacked = 0;
		for (const CardDef& owned : Cards)
			stacked += HasCard(cards, owned.Id) && owned.Kind == CardKind::Upgrade && owned.Spell == taken.Spell ? 1u : 0u;
		if (stacked >= 2)
			world.Achieve(playerId, Achievement::DeckBuilder);
	}
}
