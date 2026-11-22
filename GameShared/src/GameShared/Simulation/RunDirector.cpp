#include "RunDirector.h"
#include "GameShared/Content/Fighters.h"
#include "GameShared/Simulation/Combat.h"
#include "GameShared/Simulation/FighterAI.h"
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
		constexpr float MemberStagger = 0.09f;
		constexpr float PackSpread = 1.6f;
		constexpr float RecycleDistance = 30.0f;
		constexpr float RecycleSeconds = 10.0f;
		constexpr float TreasureInterval = 180.0f;
		constexpr float TreasureFirst = 150.0f;
		constexpr float HeadStartBudget = 6.0f;
		constexpr uint32_t OnlyGroupCost = 6;
		constexpr float SightClearance = 9.0f;
		constexpr float SpellOfferWeight = 1.0f;
		constexpr float SpellOfferPerSlot = 0.5f;
		constexpr float RankOfferWeight = 1.5f;
		constexpr float CardOfferWeight = 2.0f;
		constexpr float RuneOfferWeight = 1.5f;
		constexpr int32_t CrumbleDamage = 1000000;

		constexpr std::pair<EntityKind, uint32_t> Caps[] = { { EntityKind::RubyCrawler, 8 }, { EntityKind::AmethystCrawler, 3 }, { EntityKind::PrismaticCrawler, 1 } };

		struct GuardianSlot
		{
			EntityKind Kind;
			float Health;
		};

		constexpr GuardianSlot Guardians[] = {
			{ EntityKind::AmethystCrawler, GuardianHealth },
			{ EntityKind::RubyCrawler, EscortHealth },
			{ EntityKind::JadeCrawler, EscortHealth },
		};
		static_assert(std::size(Guardians) == GuardianCount);

		void GainMana(PlayerController& controller, float amount)
		{
			if (controller.State.Alive)
				controller.State.Mana = std::min(controller.Traits.MaxMana, controller.State.Mana + amount * controller.Traits.ManaRegen / RunManaRegen);
		}

		uint64_t SpellBit(AbilityId spell)
		{
			return uint64_t(1) << (uint32_t(spell) & 63u);
		}

		std::string ActName(uint8_t act)
		{
			return "Act " + std::string(act == 1 ? "I" : act == 2 ? "II" : "III");
		}

		std::string ClockText(float seconds)
		{
			const uint32_t whole = uint32_t(std::max(seconds, 0.0f));
			return std::to_string(whole / 60u) + (whole % 60u < 10u ? ":0" : ":") + std::to_string(whole % 60u);
		}

		std::vector<Vec2> PackSockets(const Level& level, Random& random, size_t count)
		{
			std::vector<Vec2> pool = level.Sockets;
			if (pool.empty())
				pool.push_back(level.PlayerSpawn + Vec2(0.0f, 10.0f));
			std::vector<Vec2> chosen;
			while (chosen.size() < count)
			{
				float best = -1.0f;
				size_t pick = 0;
				for (size_t i = 0; i < pool.size(); i++)
				{
					float score = Length(pool[i] - level.PlayerSpawn) * (chosen.empty() ? 1.0f : 0.25f);
					for (const Vec2 other : chosen)
						score = std::min(score, Length(pool[i] - other));
					score += 3.0f * random.Float();
					if (score > best)
					{
						best = score;
						pick = i;
					}
				}
				chosen.push_back(pool[pick]);
			}
			return chosen;
		}

		std::string ChampionText(uint8_t affixes)
		{
			std::string names;
			for (uint32_t affix = 0; affix < uint32_t(EliteAffix::Count); affix++)
				if (HasAffix(affixes, EliteAffix(affix)))
					names += std::string(names.empty() ? "" : ", ") + EliteAffixNames[affix];
			return ", led by a" + std::string(names.empty() ? "" : " ") + names + " champion";
		}
	}

	void RunDirector::Begin(const RunRules& rules)
	{
		m_Rules = rules;
		m_Status.ActClock = float(rules.StartMinute) * 60.0f;
		m_Status.RunClock = m_Status.ActClock;
		m_Treasure = std::max(TreasureFirst - m_Status.ActClock, 5.0f);
		m_Budget = std::min(m_Budget + float(rules.StartMinute) * HeadStartBudget, BudgetReserve);
		m_Spawned = uint8_t(std::min<uint32_t>(rules.StartMinute, ActEncounters - 1u));
		m_Status.Wave = m_Spawned;
		m_Status.Waves = UsesEncounters() ? ActEncounters : 0;
	}

	void RunDirector::OnPlayerSpawned(uint32_t playerId, const BrawlSpells& chosen)
	{
		if (playerId >= MaxPlayers)
			return;
		RunSeat& seat = m_Seats[playerId];
		seat = RunSeat{};
		seat.Present = true;
		seat.Rng.Seed(MixSeed(m_Rules.Seed, 0x0FFEu, playerId));
		seat.Loadout = MakeRunLoadout(chosen);
		seat.Loadout.Revives = SoloRevives;
		if (m_Rules.AllRunes)
			seat.Loadout.Runes = (RuneMask(1) << uint32_t(RuneId::Count)) - 1;
		seat.Loadout.Offers = uint8_t(m_Started ? std::min<uint32_t>(m_Rules.StartLevel - 1u + m_Status.Rooms, MaxBankedOffers) : 0u);
		m_Revision++;
	}

	void RunDirector::OnPlayerRemoved(uint32_t playerId)
	{
		if (playerId >= MaxPlayers)
			return;
		m_Seats[playerId].Present = false;
		m_Seats[playerId].Loadout.Offers = 0;
		m_Seats[playerId].Loadout.Offer = {};
		m_Seats[playerId].Focused.clear();
		m_Revision++;
	}

	void RunDirector::OnPlayerDowned(uint32_t playerId)
	{
		if (playerId >= MaxPlayers)
			return;
		m_Seats[playerId].Downs++;
		m_Seats[playerId].Revive = 0.0f;
		m_Revision++;
	}

	void RunDirector::NoteDamage(uint32_t attacker, uint8_t ability, int32_t amount)
	{
		if (attacker >= MaxPlayers || amount <= 0 || ability >= uint8_t(AbilityId::Count))
			return;
		m_Seats[attacker].Damage[size_t(CreditOf(AbilityId(ability)))] += uint32_t(amount);
	}

	void RunDirector::Update(World& world, float dt)
	{
		const uint32_t players = world.CountPlayers();
		if (players == 0)
			return;
		if (!m_Started)
		{
			m_Started = true;
			if (m_Rules.StartLevel > 1)
				GrantOffers(world, m_Rules.StartLevel - 1u);
			if (RoomsOn())
				StartRooms(world);
		}
		for (RunSeat& seat : m_Seats)
			if (seat.Present && seat.Loadout.Offers > 0 && seat.Loadout.Offer[0].IsEmpty())
				Deal(world, seat);
		if (IsRunOver(m_Status.Phase))
		{
			m_Status.PhaseClock += dt;
			return;
		}
		m_Status.RunClock += dt;
		m_Status.PhaseClock += dt;
		switch (m_Status.Phase)
		{
		case RunPhase::Act:
			m_Status.ActClock += dt;
			if (!UsesEncounters() && !RoomsOn() && m_Status.ActClock >= ActSeconds)
				OpenGate(world);
			break;
		case RunPhase::Gate:
			UpdateGate(world);
			break;
		case RunPhase::Boss:
			UpdateBoss(world);
			break;
		case RunPhase::Respite:
			UpdateRespite(world, dt);
			break;
		default:
			break;
		}
		if (IsRunOver(m_Status.Phase))
			return;
		GainShards(world, m_Orbs.Update(world, dt));
		if (m_Status.Phase == RunPhase::Act && RoomsOn())
			UpdateRoom(world, dt);
		else if (m_Status.Phase == RunPhase::Act && UsesEncounters())
			UpdateEncounters(world, dt, players);
		else if (m_Status.Phase == RunPhase::Act)
			SpendOnly(world, dt, players);
		else
		{
			uint32_t alive = 0;
			world.GetEntities().Each<Enemy>([&](Entity entity, Enemy&)
			{
				const Health* health = world.GetEntities().TryGet<Health>(entity);
				alive += health && !health->IsDead() ? 1u : 0u;
			});
			m_Status.Alive = uint16_t(alive);
			m_Status.Goal = 0;
		}
		Arrive(world, dt, players);
		Recycle(world, dt);
		UpdateDowned(world, dt);
		UpdateCards(world, dt);
		UpdateSigils(world, dt);
		UpdateRunes(world, dt);
		m_Status.Fighting = !CanArrange(world);
	}

	void RunDirector::OnKilled(World& world, const EnemyDef& def, Entity entity, const Enemy& enemy, Vec2 position, uint64_t seed, uint32_t attacker)
	{
		m_Orbs.Drop(position, def.Worth * (enemy.Elite ? EliteWorth : 1u), seed);
		m_Budget = std::min(m_Budget + float(def.Cost) * KillRefund, BudgetReserve);
		if (std::find(m_Members.begin(), m_Members.end(), entity) != m_Members.end())
			m_LastFall = position;
		if (attacker < MaxPlayers)
			m_Seats[attacker].Kills++;
		if (!m_Crumbling && attacker < MaxPlayers)
			if (PlayerController* killer = world.GetEntities().TryGet<PlayerController>(world.FindPlayer(attacker)))
				GainMana(*killer, RunKillMana);
		if (attacker < MaxPlayers && !m_Crumbling && Has(attacker, CardId::Vampirism) && m_Seats[attacker].Sips < RunVampirismKills)
			if (Health* health = world.GetEntities().TryGet<Health>(world.FindPlayer(attacker)); health && !health->IsDead() && health->Current < health->Max)
			{
				m_Seats[attacker].Sips++;
				health->Current++;
			}
		if (const Health* health = world.GetEntities().TryGet<Health>(entity); health && enemy.Chill > 0.0f && Has(attacker, CardId::Shatter))
			ShatterAt(world, position, std::max(1, int32_t(std::lround(float(health->Max) * ShatterShare))), attacker);
		for (RunSeat& seat : m_Seats)
			if (seat.Grudge == entity)
				seat.Grudge = {};
		if (const float pyre = enemy.BurnBy < MaxPlayers ? SigilPower(m_Seats[enemy.BurnBy].Loadout, SigilId::EmberPyre) : 0.0f; pyre > 0.0f && enemy.Burn > 0.0f && !m_Crumbling)
			if (const Health* health = world.GetEntities().TryGet<Health>(entity))
				PyreAt(world, position, std::max(1, int32_t(std::lround(float(health->Max) * EmberPyreShare * pyre))), enemy.BurnBy);
		if (const float hungry = attacker < MaxPlayers ? SigilPower(m_Seats[attacker].Loadout, SigilId::HungryComet) : 0.0f; hungry > 0.0f)
			if (PlayerController* controller = world.GetEntities().TryGet<PlayerController>(world.FindPlayer(attacker)))
				if (const int32_t slot = FindRunSlot(m_Seats[attacker].Loadout, AbilityId::Comet); slot >= 0)
					controller->State.Cooldowns[size_t(slot)] = std::max(controller->State.Cooldowns[size_t(slot)] - HungryCometRefund * hungry, 0.0f);
		if (!m_Crumbling)
			DropPickups(world, def, enemy, std::find(m_Guardians.begin(), m_Guardians.end(), entity) != m_Guardians.end(), position);
		if (m_Crumbling)
			return;
		if (HasAffix(enemy.Affixes, EliteAffix::Volatile))
		{
			AbilityContext context;
			context.Side = Team::Enemies;
			context.Origin = position;
			world.SpawnPendingStrike(GetAbility(AbilityId::VolatileBlast), position, ScaleDamage(GetAbility(AbilityId::VolatileBlast).Damage, enemy.DamageScale), context);
		}
		if (HasAffix(enemy.Affixes, EliteAffix::Splitting))
		{
			const bool member = std::find(m_Members.begin(), m_Members.end(), entity) != m_Members.end();
			for (uint32_t i = 0; i < SplitCount; i++)
				m_Arrivals.push_back({ enemy.Type, position + FromAngle(Pi * float(i) + world.GetRandom().Float()) * 0.8f, 0.0f, SplitHealth, false, member });
		}
	}

	uint8_t RunDirector::Stack(uint32_t playerId, CardId card) const
	{
		if (playerId >= MaxPlayers)
			return 0;
		const RunLoadout& loadout = m_Seats[playerId].Loadout;
		if (!HasCard(RunCardsOf(loadout), card))
			return 0;
		return std::max<uint8_t>(StackOf(loadout.Cards, loadout.Second, loadout.Third, card), 1);
	}

	int32_t RunDirector::ShapeDealt(Entity target, Enemy& enemy, uint32_t attacker, uint8_t ability, int32_t amount)
	{
		if (attacker >= MaxPlayers || amount <= 0)
			return amount;
		const RunSeat& seat = m_Seats[attacker];
		float scaled = float(amount);
		if (const float grudge = SigilPower(seat.Loadout, SigilId::Grudge); grudge > 0.0f && seat.Grudge == target)
			scaled *= 1.0f + grudge;
		if (const float engine = SigilPower(seat.Loadout, SigilId::MomentumEngine); engine > 0.0f)
			scaled *= 1.0f + MomentumEngineDamage * engine * m_Status.Momentum;
		if (HasCurse(seat.Loadout, CurseId::GlassPact))
			scaled *= GlassPactDealt;
		if (Has(attacker, CardId::GlassCannon))
			scaled *= GlassCannonDamageDealt;
		if (Has(attacker, CardId::Brittle) && enemy.Chill > 0.0f)
			scaled *= BrittleScale;
		if (Has(attacker, CardId::Conductive) && CreditOf(AbilityId(ability)) == AbilityId::EmberBolt && enemy.Mark > 0.0f && enemy.MarkBy == attacker)
		{
			scaled *= ConductiveScale;
			enemy.Mark = 0.0f;
			enemy.MarkBy = NoPlayer;
		}
		if (enemy.Shock > 0.0f)
			scaled *= 1.0f + enemy.ShockBonus;
		if (enemy.Stagger > 0.0f)
			scaled *= FighterStaggerDamage;
		return std::max(1, int32_t(std::lround(scaled)));
	}

	int32_t RunDirector::ShapeTaken(World& world, Entity target, PlayerController& controller, bool unmitigated, int32_t amount)
	{
		const uint32_t id = controller.PlayerId;
		if (id >= MaxPlayers || amount <= 0)
			return amount;
		RunSeat& seat = m_Seats[id];
		if (!unmitigated && seat.WardUp)
		{
			seat.WardUp = false;
			seat.WardSpent = true;
			const Transform* transform = world.GetEntities().TryGet<Transform>(target);
			const Replicated* replicated = world.GetEntities().TryGet<Replicated>(target);
			if (transform && replicated)
				world.RecordHit({ .Target = replicated->NetId, .Position = transform->Position, .Damage = 0, .OnPlayer = true, .Guarded = true });
			return 0;
		}
		float scaled = float(amount);
		if (!unmitigated)
		{
			scaled *= (Has(id, CardId::GlassCannon) ? GlassCannonDamageTaken : 1.0f) * (Has(id, CardId::HeavyRobes) ? HeavyRobesDamageTaken : 1.0f);
			scaled *= (HasCurse(seat.Loadout, CurseId::GlassPact) ? GlassPactTaken : 1.0f) * (HasCurse(seat.Loadout, CurseId::BrittleBones) ? BrittleTaken : 1.0f);
		}
		int32_t result = std::max(1, int32_t(std::lround(scaled)));
		const Health* health = world.GetEntities().TryGet<Health>(target);
		if (const float wind = SigilPower(seat.Loadout, SigilId::SecondWind); health && controller.State.Alive && result >= health->Current && float(seat.Winds) < wind)
		{
			seat.Winds++;
			result = std::max(health->Current - 1, 0);
			controller.State.Invulnerable = std::max(controller.State.Invulnerable, SecondWindSeconds);
			if (const Transform* transform = world.GetEntities().TryGet<Transform>(target))
				world.SpawnEffect(AbilityId::Ascension, transform->Position, 0.0f, id, 0, 1.0f);
			world.Notify(id, "Second Wind: you refuse to fall", true);
			m_Revision++;
		}
		if (Has(id, CardId::BloodPact))
			GainMana(controller, float(result) * BloodPactMana);
		return result;
	}

	void RunDirector::AfterDealt(World& world, Entity target, uint32_t attacker, uint8_t ability, int32_t dealt, RuneMask runes, uint8_t depth, bool killed)
	{
		if (attacker >= MaxPlayers || dealt <= 0)
			return;
		if (killed && HasRune(runes, RuneId::Combustion) && !m_Crumbling)
		{
			const Transform* place = world.GetEntities().TryGet<Transform>(target);
			const Health* health = world.GetEntities().TryGet<Health>(target);
			if (place && health && world.GetEntities().Has<Enemy>(target))
				m_Blasts.push_back({ place->Position, std::max(CombustionFloor, int32_t(std::lround(float(health->Max) * CombustionShare))), attacker, CombustionDelay });
		}
		if (HasRune(runes, RuneId::Link) && depth < MaxCastDepth)
			TriggerLink(world, m_Seats[attacker], attacker, target, depth);
		const AbilityId source = CreditOf(AbilityId(ability));
		if (source == AbilityId::Ignite || source == AbilityId::Count || source == AbilityId::Ascension || source == AbilityId::ShardBomb || source == AbilityId::ShatterBurst || source == AbilityId::RuneBlast || source == AbilityId::RuneZone)
			return;
		TriggerDuos(world, target, attacker, AbilityId(ability));
		EntityRegistry& entities = world.GetEntities();
		RunSeat& seat = m_Seats[attacker];
		if (Enemy* enemy = entities.TryGet<Enemy>(target))
		{
			if (const uint8_t kindling = Stack(attacker, CardId::Kindling); kindling > 0 && source != AbilityId::EmberBolt)
			{
				enemy->Burn = std::max(enemy->Burn, float(dealt) * RunKindlingShare * float(kindling));
				enemy->BurnRate = enemy->Burn / RunKindlingSeconds;
				enemy->BurnBy = attacker;
			}
			if (Has(attacker, CardId::Conductive) && (source == AbilityId::ArcLightning || source == AbilityId::StormStrike))
			{
				enemy->Mark = ConductiveMarkSeconds;
				enemy->MarkBy = attacker;
			}
			if (const uint8_t shock = Stack(attacker, CardId::Shock); shock > 0 && GetAbility(AbilityId(ability)).Damages == Element::Lightning)
			{
				enemy->Shock = ShockSeconds;
				enemy->ShockBonus = std::max(enemy->ShockBonus, ShockScale * float(shock));
			}
		}
		const Entity caster = world.FindPlayer(attacker);
		PlayerController* controller = entities.TryGet<PlayerController>(caster);
		if (!controller || !controller->State.Alive)
			return;
		GainMana(*controller, (source == AbilityId::EmberBolt ? RunBasicMana : 0.0f) + (Has(attacker, CardId::Siphon) ? RunSiphonMana : 0.0f));
		if (Has(attacker, CardId::Flow) && m_Status.RunClock - seat.FlowAt >= FlowSpacing)
		{
			seat.FlowAt = m_Status.RunClock;
			for (uint32_t slot = 0; slot < AbilitySlots; slot++)
				if (!controller->Traits.Skills[slot].IsEmpty() && CreditOf(controller->Traits.Skills[slot].Ability) != source)
					controller->State.Cooldowns[slot] = std::max(controller->State.Cooldowns[slot] - RunFlowRefund, 0.0f);
		}
	}

	void RunDirector::ArmEcho(uint32_t playerId)
	{
		if (Has(playerId, CardId::Echo))
			m_Seats[playerId].EchoArmed = true;
	}

	bool RunDirector::ConsumeEcho(uint32_t playerId)
	{
		return playerId < MaxPlayers && std::exchange(m_Seats[playerId].EchoArmed, false);
	}

	void RunDirector::NoteDodge(World& world, uint32_t playerId, Vec2 origin)
	{
		if (playerId >= MaxPlayers)
			return;
		RunSeat& seat = m_Seats[playerId];
		seat.DodgeAt = m_Status.RunClock;
		seat.Perfected = false;
		if (SigilPower(seat.Loadout, SigilId::RiftStep) > 0.0f)
		{
			seat.RiftStep = RiftStepSeconds;
			seat.RiftHits.clear();
		}
		const float copies = SigilPower(seat.Loadout, SigilId::Afterimage);
		const Entity player = world.FindPlayer(playerId);
		if (copies <= 0.0f || seat.LastSpell == AbilityId::Count || !player.IsValid())
			return;
		for (uint32_t copy = 0; copy < uint32_t(copies); copy++)
		{
			AbilityContext context = seat.LastContext;
			const Vec2 shift = origin - context.Origin;
			context.Origin = origin;
			context.Departure = origin;
			context.Target += shift;
			context.Rewind = 0.0f;
			context.Tag = 0;
			context.DamageScale *= AfterimageShare;
			context.PushScale *= AfterimageShare;
			world.QueueCast(player, seat.LastSpell, context, 0.05f + 0.2f * float(copy));
		}
	}

	void RunDirector::PerfectDodge(World& world, Entity player, uint32_t playerId)
	{
		if (playerId >= MaxPlayers)
			return;
		RunSeat& seat = m_Seats[playerId];
		if (seat.Perfected || m_Status.RunClock - seat.DodgeAt > PerfectDodgeWindow)
			return;
		seat.Perfected = true;
		seat.Focus = true;
		seat.PerfectDodges++;
		EntityRegistry& entities = world.GetEntities();
		if (PlayerController* controller = entities.TryGet<PlayerController>(player))
		{
			controller->State.DodgeCooldown = std::max(controller->State.DodgeCooldown - controller->Traits.Dodge.Cooldown, 0.0f);
			GainMana(*controller, RunPerfectMana);
		}
		const Transform* transform = entities.TryGet<Transform>(player);
		const Replicated* replicated = entities.TryGet<Replicated>(player);
		if (transform && replicated)
			world.RecordHit({ .Target = replicated->NetId, .Position = transform->Position, .OnPlayer = true, .Attacker = playerId, .Ability = uint8_t(AbilityId::PerfectDodge) });
	}

	bool RunDirector::ConsumeFocus(uint32_t playerId)
	{
		return playerId < MaxPlayers && std::exchange(m_Seats[playerId].Focus, false);
	}

	void RunDirector::ShatterAt(World& world, Vec2 position, int32_t damage, uint32_t attacker)
	{
		world.SpawnEffect(AbilityId::ShatterBurst, position, 0.0f, attacker, 0, ShatterRadius / std::max(GetAbility(AbilityId::ShatterBurst).Reach, 0.1f));
		std::vector<Entity> struck;
		world.GetEntities().Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			const Transform* transform = world.GetEntities().TryGet<Transform>(entity);
			const Health* health = world.GetEntities().TryGet<Health>(entity);
			if (transform && health && !health->IsDead() && enemy.Sprout <= 0.0f && LengthSquared(transform->Position - position) <= ShatterRadius * ShatterRadius)
				struck.push_back(entity);
		});
		for (const Entity entity : struck)
			ApplyDamage(world, entity, { .Amount = damage, .Attacker = attacker, .Damages = Element::Cold, .Ability = uint8_t(AbilityId::ShatterBurst) });
	}

	void RunDirector::UpdateCards(World& world, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (controller.PlayerId >= MaxPlayers)
				return;
			RunSeat& seat = m_Seats[controller.PlayerId];
			if (!Has(controller.PlayerId, CardId::CrystalWard))
				seat.WardUp = false;
			else if (!seat.WardUp && !seat.WardSpent && controller.State.Alive)
				seat.WardUp = true;
		});
		m_BurnTick += dt;
		const bool tick = m_BurnTick >= BurnTickSeconds;
		if (tick)
			m_BurnTick -= BurnTickSeconds;
		struct Burning
		{
			Entity Handle;
			int32_t Damage = 0;
			uint32_t By = NoPlayer;
		};
		std::vector<Burning> burning;
		entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			enemy.Mark = std::max(enemy.Mark - dt, 0.0f);
			enemy.Shock = std::max(enemy.Shock - dt, 0.0f);
			if (enemy.Shock <= 0.0f)
				enemy.ShockBonus = 0.0f;
			if (enemy.Burn <= 0.0f)
				return;
			const float burn = std::min(enemy.Burn, enemy.BurnRate * dt);
			enemy.Burn -= burn;
			enemy.BurnBank += burn;
			if (!tick)
				return;
			const int32_t damage = int32_t(enemy.BurnBank);
			if (damage <= 0)
				return;
			enemy.BurnBank -= float(damage);
			burning.push_back({ entity, damage, enemy.BurnBy });
		});
		for (const Burning& burn : burning)
			ApplyDamage(world, burn.Handle, { .Amount = burn.Damage, .Attacker = burn.By, .Damages = Element::Fire, .Ability = uint8_t(AbilityId::Ignite), .Unmitigated = true });
	}

	void RunDirector::DropPickups(World& world, const EnemyDef& def, const Enemy& enemy, bool guardian, Vec2 position)
	{
		Random& random = world.GetRandom();
		std::array<uint32_t, size_t(LootKind::Count)> lying{};
		world.GetEntities().Each<Loot>([&](Entity, Loot& loot) { lying[size_t(loot.Kind)]++; });
		std::vector<LootKind> drops;
		if (def.Behavior == EnemyBehavior::Treasure)
			drops = { LootKind::Chest, LootKind::Magnet };
		else if (enemy.Elite && !guardian && !RoomsOn())
			drops = { LootKind::Chest };
		else if (!guardian)
		{
			const float roll = random.Float();
			if (roll < MagnetChance)
				drops.push_back(LootKind::Magnet);
			else if (roll < MagnetChance + BombChance)
				drops.push_back(LootKind::Bomb);
			else if (roll < MagnetChance + BombChance + FrostChance)
				drops.push_back(LootKind::Frost);
		}
		for (const LootKind kind : drops)
		{
			if (kind != LootKind::Chest && lying[size_t(kind)] > 0)
				continue;
			const Vec2 spot = world.PlaceDrop(position, position + FromAngle(random.Float() * 2.0f * Pi) * 0.6f);
			if (Loot* loot = world.GetEntities().TryGet<Loot>(world.SpawnLoot(kind, spot)))
				loot->Lifetime = kind == LootKind::Chest ? ChestLifetime : PickupLifetime;
		}
	}

	void RunDirector::Shockwave(World& world, Vec2 center, float radius, AbilityId visual, uint32_t owner, bool clear, float eliteShare, float stun)
	{
		EntityRegistry& entities = world.GetEntities();
		world.SpawnEffect(visual, center, 0.0f, owner, 0, radius / std::max(GetAbility(visual).Reach, 0.1f));
		struct Struck
		{
			Entity Handle;
			int32_t Damage = 0;
		};
		std::vector<Struck> struck;
		entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			const Transform* transform = entities.TryGet<Transform>(entity);
			const Health* health = entities.TryGet<Health>(entity);
			if (!transform || !health || health->IsDead() || enemy.Sprout > 0.0f || LengthSquared(transform->Position - center) > radius * radius)
				return;
			enemy.Stun = std::max(enemy.Stun, stun);
			enemy.Windup = 0.0f;
			const bool champion = enemy.Elite || entities.Has<Boss>(entity);
			if (clear && !champion)
				struck.push_back({ entity, health->Current });
			else if (eliteShare > 0.0f)
				struck.push_back({ entity, std::max(1, int32_t(std::lround(float(health->Max) * eliteShare))) });
		});
		for (const Struck& hit : struck)
			ApplyDamage(world, hit.Handle, { .Amount = hit.Damage, .Attacker = owner, .Damages = GetAbility(visual).Damages, .Ability = uint8_t(visual), .Unmitigated = true });
	}

	bool RunDirector::ApplyPickup(World& world, uint32_t playerId, Vec2 position, LootKind kind)
	{
		switch (kind)
		{
		case LootKind::Magnet:
			m_Orbs.Attract();
			world.SpawnEffect(AbilityId::Ascension, position, 0.0f, playerId, 0, 1.0f);
			Announce(world, "Shard magnet: every crystal shard flies to you", true);
			return true;
		case LootKind::Bomb:
			Shockwave(world, position, PickupReach, AbilityId::ShardBomb, playerId, true, BombEliteShare, 0.6f);
			return true;
		case LootKind::Frost:
		{
			world.SpawnEffect(AbilityId::FrostClock, position, 0.0f, playerId, 0, 1.0f);
			EntityRegistry& entities = world.GetEntities();
			entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
			{
				const Transform* transform = entities.TryGet<Transform>(entity);
				if (!transform || LengthSquared(transform->Position - position) > PickupReach * PickupReach)
					return;
				enemy.Stun = std::max(enemy.Stun, FrostSeconds);
				enemy.Chill = std::max(enemy.Chill, FrostSeconds + 2.0f);
				enemy.Windup = 0.0f;
			});
			return true;
		}
		case LootKind::Chest:
			world.SpawnEffect(AbilityId::Ascension, position, 0.0f, playerId, 0, 1.0f);
			if (m_Reward != RoomReward::None)
			{
				GiveReward(world, std::exchange(m_Reward, RoomReward::None));
				return true;
			}
			GrantOffers(world, 1);
			Announce(world, "A crystal chest cracks open: a card for everyone", true);
			return true;
		default:
			return false;
		}
	}

	EnemyScaling RunDirector::Scale(uint32_t players, EntityKind kind) const
	{
		const float minutes = RoomsOn() ? RoomMinutes(m_Status.Act, m_Status.Room) + float(m_Rules.StartMinute) : RunMinutes(m_Status.Act, m_Status.ActClock);
		return { .Health = IsFighterKind(kind) ? FighterHealth(minutes, players) : IsFodder(kind) ? 1.0f : TrashHealth(minutes, players), .Damage = 1.0f + RunDamagePerMinute * minutes, .Level = 1 + uint32_t(minutes) };
	}

	void RunDirector::GainShards(World& world, uint32_t worth)
	{
		m_HeartBank += worth;
		while (m_HeartBank >= ShardHeartWorth)
		{
			m_HeartBank -= ShardHeartWorth;
			for (uint32_t id = 0; id < MaxPlayers; id++)
				if (const float heart = m_Seats[id].Present ? SigilPower(m_Seats[id].Loadout, SigilId::ShardHeart) : 0.0f; heart > 0.0f)
					if (const Transform* self = world.GetEntities().TryGet<Transform>(world.FindPlayer(id)))
						if (const std::optional<Entity> prey = NearestEnemy(world, self->Position, 12.0f))
							for (uint32_t bolt = 0; bolt < uint32_t(heart); bolt++)
								CastFor(world, id, AbilityId::EmberBolt, self->Position, world.GetEntities().TryGet<Transform>(*prey)->Position, 0.9f, 0.05f + 0.1f * float(bolt));
		}
		m_ShardCarry += float(worth) * ShardsPerWorth * float(m_Rules.ShardRate) / 100.0f * (RoomsOn() ? 1.0f + MomentumShards * m_Status.Momentum : 1.0f);
		const uint32_t whole = uint32_t(m_ShardCarry);
		m_ShardCarry -= float(whole);
		if (whole > 0)
			GrantShards(whole);
	}

	void RunDirector::Announce(World& world, const std::string& text, bool success)
	{
		world.GetEntities().Each<PlayerController>([&](Entity, PlayerController& controller) { world.Notify(controller.PlayerId, text, success); });
	}

	void RunDirector::OpenGate(World& world)
	{
		m_Status.Phase = RunPhase::Gate;
		m_Status.PhaseClock = 0.0f;
		Crumble(world);
		if (world.GetLevel().BossArenaRadius <= 0.0f)
		{
			StartBoss(world);
			return;
		}
		Announce(world, "The waves are spent and the crystals shatter. Step onto the guardian stone in the cobbled ring to face the guardian", true);
	}

	void RunDirector::UpdateGate(World& world)
	{
		const Level& level = world.GetLevel();
		bool entered = false;
		world.GetEntities().Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			const Transform* transform = world.GetEntities().TryGet<Transform>(entity);
			if (controller.State.Alive && transform && LengthSquared(transform->Position - level.BossArena) <= GateRadius * GateRadius)
				entered = true;
		});
		if (entered)
			StartBoss(world);
	}

	void RunDirector::StartBoss(World& world)
	{
		m_Status.Phase = RunPhase::Boss;
		m_Status.PhaseClock = 0.0f;
		Crumble(world);
		const Level& level = world.GetLevel();
		const Vec2 center = level.BossArenaRadius > 0.0f ? level.BossArena : ChooseSocket(world, 0.6f).value_or(world.FindSpawnPosition(0));
		const uint32_t players = std::max(world.CountPlayers(), 1u);
		const float act = float(m_Status.Act);
		for (size_t i = 0; i < std::size(Guardians); i++)
		{
			const GuardianSlot& slot = Guardians[i];
			const EnemyDef* def = FindEnemyDef(slot.Kind);
			EnemyScaling scaling = Scale(players);
			scaling.Health *= slot.Health * act;
			scaling.Elite = true;
			const Vec2 spot = i == 0 ? center : center + FromAngle(2.0f * Pi * float(i - 1) / float(std::size(Guardians) - 1)) * GuardianSpread;
			const Entity entity = world.SpawnEnemy(slot.Kind, world.FindFreeSpotNear(spot, def->Radius * EliteSize), scaling);
			if (Enemy* enemy = world.GetEntities().TryGet<Enemy>(entity))
			{
				enemy->Sprout = SproutSeconds;
				enemy->OffspringHealth = Scale(players).Health;
			}
			if (const uint32_t count = GuardianAffixes(m_Status.Act); count > 0)
				ApplyAffixes(world, entity, RollAffixes(world.GetRandom(), slot.Kind, count, AffixBit(EliteAffix::Splitting)));
			m_Guardians.push_back(entity);
		}
		m_Status.Guardians = uint8_t(m_Guardians.size());
		Announce(world, ActName(m_Status.Act) + " guardian: the brood rises from the stone", false);
	}

	void RunDirector::UpdateBoss(World& world)
	{
		uint32_t alive = 0;
		for (const Entity entity : m_Guardians)
			if (const Health* health = world.GetEntities().TryGet<Health>(entity); health && !health->IsDead())
				alive++;
		m_Status.Guardians = uint8_t(alive);
		if (alive == 0)
			EndBoss(world);
	}

	void RunDirector::Crumble(World& world)
	{
		std::vector<Entity> doomed;
		world.GetEntities().Each<Enemy>([&](Entity entity, Enemy&)
		{
			if (const Health* health = world.GetEntities().TryGet<Health>(entity); health && !health->IsDead())
				doomed.push_back(entity);
		});
		m_Crumbling = true;
		for (const Entity entity : doomed)
			ApplyDamage(world, entity, { .Amount = CrumbleDamage, .Unmitigated = true });
		m_Crumbling = false;
		m_Arrivals.clear();
		m_Guardians.clear();
		m_Members.clear();
		m_Open = 0;
	}

	void RunDirector::EndBoss(World& world)
	{
		Crumble(world);
		m_Status.Guardians = 0;
		if (m_Status.Act >= RunActs)
		{
			EndRun(world, true);
			return;
		}
		m_Status.Phase = RunPhase::Respite;
		m_Status.PhaseClock = 0.0f;
		m_RiftHold = 0.0f;
		EntityRegistry& entities = world.GetEntities();
		std::vector<Entity> downed;
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (!controller.State.Alive)
				downed.push_back(entity);
			else if (Health* health = entities.TryGet<Health>(entity))
				health->Current = std::min(health->Max, health->Current + std::max(1, int32_t(std::lround(float(health->Max) * BossHeal * HealScale(controller.PlayerId)))));
		});
		for (const Entity entity : downed)
			world.RevivePlayer(entity, BossHeal);
		for (RunSeat& seat : m_Seats)
			seat.Revive = 0.0f;
		for (RunSeat& seat : m_Seats)
			GrantFocused(world, seat, RunCardKind::Card, CardRarity::Legendary);
		Announce(world, ActName(m_Status.Act) + " cleared. Take the guardian's card, then step into the rift", true);
	}

	uint32_t RunDirector::CountInRift(World& world, uint32_t& living) const
	{
		const Level& level = world.GetLevel();
		const Vec2 center = level.BossArenaRadius > 0.0f ? level.BossArena : world.FindSpawnPosition(0);
		uint32_t inside = 0;
		living = 0;
		world.GetEntities().Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			const Transform* transform = world.GetEntities().TryGet<Transform>(entity);
			if (!controller.State.Alive || !transform)
				return;
			living++;
			inside += LengthSquared(transform->Position - center) <= GateRadius * GateRadius ? 1u : 0u;
		});
		return inside;
	}

	void RunDirector::UpdateRespite(World& world, float dt)
	{
		uint32_t living = 0;
		const uint32_t inside = CountInRift(world, living);
		m_Status.Rift = uint8_t(inside);
		m_RiftHold = living > 0 && inside == living ? m_RiftHold + dt : 0.0f;
		if (m_RiftHold >= RiftHoldSeconds || m_Status.PhaseClock >= RespiteSeconds)
			StartAct(world);
	}

	void RunDirector::StartAct(World& world)
	{
		m_Status.Act++;
		m_Status.Phase = RunPhase::Act;
		m_Status.ActClock = 0.0f;
		m_Status.PhaseClock = 0.0f;
		m_Status.Rift = 0;
		m_RiftHold = 0.0f;
		m_Budget = 4.0f;
		m_Treasure = TreasureFirst;
		m_Lull = FirstLull;
		m_Spawned = 0;
		m_Open = 0;
		m_Members.clear();
		m_Status.Wave = 0;
		if (RoomsOn())
		{
			m_Map = MakeActMap(m_Rules.Seed, m_Status.Act);
			m_Status.Route = 0;
			TravelTo(world, RoomIndex(0, 0));
			Announce(world, ActName(m_Status.Act) + ": the rift opens onto the " + GetMapDef(RoomMapFor(m_Status.Act, RoomKind::Guardian)).Name, false);
			return;
		}
		const MapKind arena = ArenaForAct(m_Status.Act);
		if (arena != world.GetLevel().Kind)
		{
			m_PendingArena = ArenaChange{ arena, world.GetSettings().Seed };
			Announce(world, ActName(m_Status.Act) + ": the rift opens onto the " + GetMapDef(arena).Name, false);
			return;
		}
		Announce(world, ActName(m_Status.Act) + ": the crystals stir again, stronger", false);
	}

	void RunDirector::OnArenaChanged(World& world)
	{
		uint32_t banked = 0;
		for (const Orb& orb : m_Orbs.GetOrbs())
			banked += orb.Value;
		m_Orbs.Clear();
		if (banked > 0)
			GainShards(world, banked);
		m_Arrivals.clear();
		m_Members.clear();
		m_Guardians.clear();
		m_LastFall = world.FindSpawnPosition(0);
		m_RiftHold = 0.0f;
		m_Revision++;
		if (RoomsOn())
			EnterRoom(world);
	}

	void RunDirector::EndRun(World& world, bool victory)
	{
		m_Status.Phase = victory ? RunPhase::Victory : RunPhase::Defeat;
		m_Status.PhaseClock = 0.0f;
		m_Arrivals.clear();
		for (RunSeat& seat : m_Seats)
		{
			seat.Paused = false;
			seat.Loadout.Offers = 0;
			seat.Loadout.Offer = {};
			seat.Loadout.Focus = RunCardKind::None;
			seat.Focused.clear();
		}
		m_Revision++;
		Announce(world, victory ? "The hollow falls silent. The run is won" : "The party has fallen. The run is over", victory);
	}

	void RunDirector::UpdateDowned(World& world, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		struct Living
		{
			uint32_t PlayerId;
			Vec2 Position;
		};
		std::vector<Living> living;
		uint32_t present = 0;
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			present++;
			if (const Transform* transform = entities.TryGet<Transform>(entity); transform && controller.State.Alive)
				living.push_back({ controller.PlayerId, transform->Position });
		});
		const bool solo = present == 1;
		struct Rising
		{
			Entity Handle;
			uint32_t PlayerId;
			uint32_t Helper;
		};
		std::vector<Rising> rising;
		bool hope = !living.empty();
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (controller.State.Alive || controller.PlayerId >= MaxPlayers)
				return;
			RunSeat& seat = m_Seats[controller.PlayerId];
			const Transform* transform = entities.TryGet<Transform>(entity);
			uint32_t helper = NoPlayer;
			if (solo)
			{
				if (seat.Loadout.Revives == 0)
					return;
				hope = true;
				helper = controller.PlayerId;
			}
			else if (transform)
				for (const Living& ally : living)
					if (LengthSquared(ally.Position - transform->Position) <= ReviveRange * ReviveRange)
						helper = ally.PlayerId;
			if (helper != NoPlayer)
				seat.Revive += dt / ReviveSeconds;
			else
				seat.Revive = std::max(seat.Revive - ReviveDecay * dt / ReviveSeconds, 0.0f);
			if (seat.Revive >= 1.0f)
				rising.push_back({ entity, controller.PlayerId, helper });
		});
		for (const Rising& rise : rising)
		{
			RunSeat& seat = m_Seats[rise.PlayerId];
			seat.Revive = 0.0f;
			if (rise.Helper == rise.PlayerId)
				seat.Loadout.Revives--;
			else if (rise.Helper < MaxPlayers)
				m_Seats[rise.Helper].Revived++;
			world.RevivePlayer(rise.Handle, ReviveHealth);
			if (const Transform* transform = world.GetEntities().TryGet<Transform>(rise.Handle))
				Shockwave(world, transform->Position, ReviveBurstRadius, AbilityId::Ascension, rise.PlayerId, true, 0.0f, ReviveBurstStun);
			m_Revision++;
		}
		m_Wipe = hope || !rising.empty() ? 0.0f : m_Wipe + dt;
		if (m_Wipe >= WipeSeconds)
			EndRun(world, false);
	}

	void RunDirector::GrantOffers(World& world, uint32_t count)
	{
		for (RunSeat& seat : m_Seats)
		{
			if (!seat.Present)
				continue;
			seat.Loadout.Offers = uint8_t(std::min<uint32_t>(uint32_t(seat.Loadout.Offers) + count, MaxBankedOffers));
			if (seat.Loadout.Offer[0].IsEmpty())
				Deal(world, seat);
		}
		m_Revision++;
	}

	void RunDirector::GrantFocused(World& world, RunSeat& seat, RunCardKind focus, CardRarity floor)
	{
		if (!seat.Present)
			return;
		seat.Loadout.Offers = uint8_t(std::min<uint32_t>(uint32_t(seat.Loadout.Offers) + 1u, MaxBankedOffers));
		if (focus != RunCardKind::None && seat.Focused.size() < seat.Loadout.Offers)
			seat.Focused.push_back({ focus, floor });
		if (seat.Loadout.Offer[0].IsEmpty())
			Deal(world, seat);
		m_Revision++;
	}

	void RunDirector::ConsumeOffer(World& world, RunSeat& seat)
	{
		if (seat.Loadout.Focus != RunCardKind::None && !seat.Focused.empty())
			seat.Focused.erase(seat.Focused.begin());
		seat.Loadout.Offers--;
		seat.Loadout.Offer = {};
		seat.Loadout.Focus = RunCardKind::None;
		if (seat.Loadout.Offers > 0)
			Deal(world, seat);
		else
			seat.Focused.clear();
	}

	void RunDirector::Deal(World& world, RunSeat& seat)
	{
		seat.Loadout.Offer = {};
		seat.Loadout.Focus = seat.Focused.empty() ? RunCardKind::None : seat.Focused.front().Kind;
		seat.Floor = seat.Focused.empty() ? CardRarity::Common : seat.Focused.front().Floor;
		for (uint32_t slot = 0; slot < CardChoices; slot++)
			DealSlot(world, seat, slot);
		const TagMask owned = RunTagsOf(seat.Loadout);
		const bool pivot = std::any_of(seat.Loadout.Offer.begin(), seat.Loadout.Offer.end(), [&](const RunCard& card) { return IsOffTag(card, owned); });
		if (!pivot && owned != 0 && seat.Loadout.Focus == RunCardKind::None)
		{
			const RunCard kept = seat.Loadout.Offer[CardChoices - 1];
			DealSlot(world, seat, CardChoices - 1, true);
			if (seat.Loadout.Offer[CardChoices - 1].IsEmpty())
				seat.Loadout.Offer[CardChoices - 1] = kept;
		}
		std::stable_partition(seat.Loadout.Offer.begin(), seat.Loadout.Offer.end(), [](const RunCard& card) { return !card.IsEmpty(); });
		if (seat.Loadout.Offer[0].IsEmpty())
		{
			seat.Loadout.Offers = 0;
			seat.Loadout.Focus = RunCardKind::None;
			seat.Focused.clear();
		}
	}

	void RunDirector::DealSlot(World& world, RunSeat& seat, uint32_t slot, bool offTag)
	{
		RunLoadout& loadout = seat.Loadout;
		const TagMask owned = RunTagsOf(loadout);
		loadout.Offer[slot] = {};
		const auto offered = [&](const RunCard& card)
		{
			for (uint32_t i = 0; i < CardChoices; i++)
				if (i != slot && loadout.Offer[i] == card)
					return true;
			return false;
		};
		struct Candidate
		{
			RunCard Card;
			float Weight = 0.0f;
			CardRarity Rarity = CardRarity::Common;
		};
		std::vector<Candidate> spells;
		std::vector<Candidate> ranks;
		std::vector<Candidate> cards;
		const uint32_t free = FreeRunSlots(loadout);
		if (free > 0)
			for (const AbilityId spell : BrawlSpellPool)
			{
				const RunCard card{ RunCardKind::Spell, uint8_t(spell) };
				if (FindRunSlot(loadout, spell) < 0 && (seat.BannedSpells & SpellBit(spell)) == 0 && !offered(card) && (!offTag || (SpellTags(spell) & owned) == 0))
					spells.push_back({ card, 1.0f });
			}
		for (uint32_t i = 0; i < AbilitySlots; i++)
		{
			const AbilityId spell = i == 0 ? BrawlBasic : loadout.Spells[i - 1];
			const RunCard card{ RunCardKind::Rank, uint8_t(spell) };
			if (spell != AbilityId::Count && loadout.Ranks[i] < MaxSpellRank && (seat.BannedRanks & SpellBit(spell)) == 0 && !offered(card) && !offTag)
				ranks.push_back({ card, float(MaxSpellRank + 1 - loadout.Ranks[i]) });
		}
		const std::array<float, 3> classes = OfferClassWeights(m_Status.Act, 0.0f);
		for (const CardId id : RunCardPool)
		{
			const RunCard card{ RunCardKind::Card, uint8_t(id) };
			if (!CardFitsRun(loadout, id) || HasCard(seat.BannedCards, id) || offered(card))
				continue;
			const CardDef& def = GetCard(id);
			const bool onTag = (def.Tags & owned) != 0;
			if (offTag && (onTag || def.Tags == 0))
				continue;
			cards.push_back({ card, classes[std::min<size_t>(size_t(def.Rarity), 2)] * (onTag ? RunTagWeight : 1.0f), def.Rarity });
		}
		for (CardRarity floor = seat.Floor; floor > CardRarity::Common; floor = CardRarity(uint8_t(floor) - 1))
			if (std::any_of(cards.begin(), cards.end(), [floor](const Candidate& candidate) { return candidate.Rarity >= floor; }))
			{
				std::erase_if(cards, [floor](const Candidate& candidate) { return candidate.Rarity < floor; });
				break;
			}
		float pool = 1.0f;
		for (const Candidate& candidate : cards)
			pool += candidate.Weight;
		for (const DuoDef& duo : Duos)
		{
			const RunCard card{ RunCardKind::Duo, uint8_t(duo.Id) };
			if (!offTag && !HasDuo(loadout, duo.Id) && !offered(card) && RankOf(loadout, duo.First) >= DuoRank && RankOf(loadout, duo.Second) >= DuoRank)
				cards.push_back({ card, pool * DuoOfferShare });
		}
		std::vector<Candidate> sigils;
		if (loadout.Focus == RunCardKind::Sigil && FreeSigilSlots(loadout) > 0)
			for (uint32_t id = 0; id < uint32_t(SigilId::Count); id++)
				if (const RunCard card{ RunCardKind::Sigil, uint8_t(id) }; !HasSigil(loadout, SigilId(id)) && !offered(card))
					sigils.push_back({ card, 1.0f });
		const auto total = [](const std::vector<Candidate>& list)
		{
			float sum = 0.0f;
			for (const Candidate& candidate : list)
				sum += candidate.Weight;
			return sum;
		};
		std::vector<Candidate> runes;
		if (!offTag)
			for (const RuneDef& rune : Runes)
			{
				const RunCard card{ RunCardKind::Rune, uint8_t(rune.Id) };
				if (HasRune(loadout.Runes, rune.Id) || HasRune(seat.BannedRunes, rune.Id) || offered(card) || !RuneFitsBuild(loadout, rune.Id))
					continue;
				runes.push_back({ card, classes[std::min<size_t>(size_t(rune.Rarity), 2)], rune.Rarity });
			}
		if (loadout.Focus == RunCardKind::Rune)
			for (CardRarity floor = seat.Floor; floor > CardRarity::Common; floor = CardRarity(uint8_t(floor) - 1))
				if (std::any_of(runes.begin(), runes.end(), [floor](const Candidate& candidate) { return candidate.Rarity >= floor; }))
				{
					std::erase_if(runes, [floor](const Candidate& candidate) { return candidate.Rarity < floor; });
					break;
				}
		std::array<std::pair<const std::vector<Candidate>*, float>, 5> kinds = { {
			{ &spells, spells.empty() ? 0.0f : SpellOfferWeight + SpellOfferPerSlot * float(free) },
			{ &ranks, ranks.empty() ? 0.0f : RankOfferWeight },
			{ &cards, cards.empty() ? 0.0f : CardOfferWeight },
			{ &sigils, sigils.empty() ? 0.0f : 1.0f },
			{ &runes, runes.empty() ? 0.0f : RuneOfferWeight },
		} };
		if (loadout.Focus != RunCardKind::None)
		{
			const size_t wanted = loadout.Focus == RunCardKind::Rune ? 4 : size_t(loadout.Focus) - size_t(RunCardKind::Spell);
			const size_t fallback = loadout.Focus == RunCardKind::Rank || loadout.Focus == RunCardKind::Sigil || loadout.Focus == RunCardKind::Rune ? 2 : 1;
			const size_t keep = wanted < kinds.size() && kinds[wanted].second > 0.0f ? wanted : fallback;
			for (size_t i = 0; i < kinds.size(); i++)
				if (i != keep)
					kinds[i].second = 0.0f;
		}
		float kindTotal = 0.0f;
		for (const auto& kind : kinds)
			kindTotal += kind.second;
		if (kindTotal <= 0.0f)
			return;
		Random& random = seat.Rng;
		float roll = random.Float() * kindTotal;
		const std::vector<Candidate>* chosen = nullptr;
		for (const auto& [list, weight] : kinds)
		{
			if (weight <= 0.0f)
				continue;
			chosen = list;
			if ((roll -= weight) < 0.0f)
				break;
		}
		float pick = random.Float() * total(*chosen);
		for (const Candidate& candidate : *chosen)
		{
			loadout.Offer[slot] = candidate.Card;
			if ((pick -= candidate.Weight) < 0.0f)
				break;
		}
		if (chosen == &cards && RoomsOn() && (m_Status.Act > 1 || RoomLayer(m_Status.Room) >= 2) && CurseCount(loadout) < MaxCurses && random.Float() < CurseOfferChance)
		{
			std::vector<RunCard> curses;
			for (const CurseDef& curse : Curses)
				if (const RunCard card{ RunCardKind::Curse, uint8_t(curse.Id) }; !HasCurse(loadout, curse.Id) && !offered(card))
					curses.push_back(card);
			if (!curses.empty())
				loadout.Offer[slot] = curses[random.Below(uint32_t(curses.size()))];
		}
	}

	bool RunDirector::Pick(World& world, uint32_t playerId, uint32_t choice)
	{
		if (playerId >= MaxPlayers || choice >= CardChoices || IsRunOver(m_Status.Phase))
			return false;
		RunSeat& seat = m_Seats[playerId];
		RunLoadout& loadout = seat.Loadout;
		if (!seat.Present || !loadout.HasOffer() || loadout.Offer[choice].IsEmpty())
			return false;
		const RunCard card = loadout.Offer[choice];
		switch (card.Kind)
		{
		case RunCardKind::Spell:
		{
			const auto free = std::find(loadout.Spells.begin(), loadout.Spells.end(), AbilityId::Count);
			if (free == loadout.Spells.end() || FindRunSlot(loadout, card.Spell()) >= 0)
				return false;
			*free = card.Spell();
			loadout.Ranks[size_t(free - loadout.Spells.begin()) + 1] = 1;
			break;
		}
		case RunCardKind::Rank:
		{
			const int32_t slot = FindRunSlot(loadout, card.Spell());
			if (slot < 0 || loadout.Ranks[size_t(slot)] >= MaxSpellRank)
				return false;
			loadout.Ranks[size_t(slot)]++;
			break;
		}
		case RunCardKind::Card:
			if (!CardFitsRun(loadout, card.Upgrade()))
				return false;
			if (!HasCard(loadout.Cards, card.Upgrade()))
				loadout.Cards |= CardBit(card.Upgrade());
			else if (!HasCard(loadout.Second, card.Upgrade()))
				loadout.Second |= CardBit(card.Upgrade());
			else
				loadout.Third |= CardBit(card.Upgrade());
			break;
		case RunCardKind::Sigil:
		{
			if (HasSigil(loadout, SigilId(card.Id)))
				return false;
			const auto free = std::find(loadout.Sigils.begin(), loadout.Sigils.end(), SigilId::None);
			*(free != loadout.Sigils.end() ? free : loadout.Sigils.end() - 1) = SigilId(card.Id);
			break;
		}
		case RunCardKind::Duo:
			loadout.Duos |= uint8_t(1u << card.Id);
			break;
		case RunCardKind::Curse:
			loadout.Curses |= uint8_t(1u << card.Id);
			break;
		case RunCardKind::Rune:
			if (HasRune(loadout.Runes, card.Rune()))
				return false;
			loadout.Runes |= RuneBit(card.Rune());
			break;
		default:
			return false;
		}
		seat.Picks.push_back({ card, RunStage(m_Status.Act, m_Status.Room) });
		ConsumeOffer(world, seat);
		world.RefreshTraits(world.FindPlayer(playerId));
		m_Revision++;
		return true;
	}

	bool RunDirector::Build(World& world, uint32_t playerId, const RunLoadout& loadout)
	{
		if (playerId >= MaxPlayers || !m_Seats[playerId].Present)
			return false;
		m_Seats[playerId].Loadout = loadout;
		world.RefreshTraits(world.FindPlayer(playerId));
		m_Revision++;
		return true;
	}

	bool RunDirector::Reroll(World& world, uint32_t playerId)
	{
		if (playerId >= MaxPlayers)
			return false;
		RunSeat& seat = m_Seats[playerId];
		if (!seat.Present || !seat.Loadout.HasOffer() || seat.Loadout.Rerolls == 0)
			return false;
		seat.Loadout.Rerolls--;
		const uint8_t offers = seat.Loadout.Offers;
		Deal(world, seat);
		seat.Loadout.Offers = seat.Loadout.Offer[0].IsEmpty() ? uint8_t(0) : offers;
		m_Revision++;
		return true;
	}

	bool RunDirector::Banish(World& world, uint32_t playerId, uint32_t choice)
	{
		if (playerId >= MaxPlayers || choice >= CardChoices)
			return false;
		RunSeat& seat = m_Seats[playerId];
		RunLoadout& loadout = seat.Loadout;
		if (!seat.Present || !loadout.HasOffer() || loadout.Banishes == 0 || loadout.Offer[choice].IsEmpty())
			return false;
		const RunCard card = loadout.Offer[choice];
		if (card.Kind == RunCardKind::Spell)
			seat.BannedSpells |= SpellBit(card.Spell());
		else if (card.Kind == RunCardKind::Rank)
			seat.BannedRanks |= SpellBit(card.Spell());
		else if (card.Kind == RunCardKind::Rune)
			seat.BannedRunes |= RuneBit(card.Rune());
		else
			seat.BannedCards |= CardBit(card.Upgrade());
		loadout.Banishes--;
		DealSlot(world, seat, choice);
		std::stable_partition(loadout.Offer.begin(), loadout.Offer.end(), [](const RunCard& offered) { return !offered.IsEmpty(); });
		if (loadout.Offer[0].IsEmpty())
			ConsumeOffer(world, seat);
		m_Revision++;
		return true;
	}

	bool RunDirector::Skip(World& world, uint32_t playerId)
	{
		if (playerId >= MaxPlayers)
			return false;
		RunSeat& seat = m_Seats[playerId];
		if (!seat.Present || !seat.Loadout.HasOffer())
			return false;
		seat.Picks.push_back({ RunCard{}, RunStage(m_Status.Act, m_Status.Room) });
		ConsumeOffer(world, seat);
		m_Revision++;
		return true;
	}

	bool RunDirector::SetPaused(const World&, uint32_t playerId, bool paused)
	{
		if (playerId >= MaxPlayers || !m_Seats[playerId].Present)
			return false;
		m_Seats[playerId].Paused = paused;
		return true;
	}

	bool RunDirector::IsPaused(const World&) const
	{
		if (IsRunOver(m_Status.Phase))
			return false;
		const RunSeat* only = nullptr;
		for (const RunSeat& seat : m_Seats)
		{
			if (!seat.Present)
				continue;
			if (only)
				return false;
			only = &seat;
		}
		return only && only->Paused;
	}

	void RunDirector::UpdateEncounters(World& world, float dt, uint32_t players)
	{
		EntityRegistry& entities = world.GetEntities();
		uint32_t alive = 0;
		entities.Each<Enemy>([&](Entity entity, Enemy&)
		{
			const Health* health = entities.TryGet<Health>(entity);
			alive += health && !health->IsDead() ? 1u : 0u;
		});
		m_Status.Alive = uint16_t(alive);
		m_Status.Goal = 0;
		std::erase_if(m_Members, [&](Entity entity)
		{
			const Health* health = entities.TryGet<Health>(entity);
			return !health || health->IsDead();
		});
		if ((m_Treasure -= dt) <= 0.0f)
		{
			m_Treasure = TreasureInterval;
			if (const std::optional<Vec2> socket = ChooseSocket(world, FindEnemyDef(EntityKind::PrismaticCrawler)->Radius))
			{
				m_Arrivals.push_back({ EntityKind::PrismaticCrawler, *socket, 0.0f });
				Announce(world, "A prismatic crawler surfaces. Catch it before it burrows away", true);
			}
		}
		if (m_Open > 0)
		{
			m_EncounterClock += dt;
			uint32_t fighters = 0;
			uint32_t crawlers = 0;
			for (const Entity entity : m_Members)
				if (const Enemy* enemy = entities.TryGet<Enemy>(entity))
					(IsFighterKind(enemy->Type) ? fighters : crawlers)++;
			const bool arriving = std::any_of(m_Arrivals.begin(), m_Arrivals.end(), [](const Arrival& arrival) { return arrival.Member; });
			if (!arriving && fighters == 0 && crawlers <= ClearStragglers)
			{
				ClearEncounter(world);
				return;
			}
			const uint32_t crowd = EncounterCrowd + EncounterCrowdPerPlayer * (players > 1 ? players - 1 : 0);
			const uint32_t present = alive + uint32_t(m_Arrivals.size());
			if (fighters > 0 && m_Reinforcements < ReinforceWaves && (m_Reinforce -= dt) <= 0.0f)
			{
				m_Reinforce = ReinforceSeconds;
				m_Reinforcements++;
				const uint32_t count = std::min(ReinforceCrawlers + m_OpenTier - 1u, crowd > present ? crowd - present : 0u);
				if (const std::optional<Vec2> socket = count > 0 ? ChooseSocket(world, FindEnemyDef(EntityKind::JadeCrawler)->Radius) : std::nullopt)
				{
					Random& random = world.GetRandom();
					for (uint32_t i = 0; i < count; i++)
						m_Arrivals.push_back({ i == 0 && m_OpenTier >= 2 ? EntityKind::JadeCrawler : EntityKind::QuartzCrawler, *socket + FromAngle(random.Float() * 2.0f * Pi) * (PackSpread * std::sqrt(random.Float())), MemberStagger * float(i) });
				}
			}
			if (m_EncounterClock >= EncounterTimeout && m_Spawned < ActEncounters && present < crowd)
				SpawnEncounter(world, players);
			m_Status.Lull = 0.0f;
			return;
		}
		m_Lull -= dt;
		m_Status.Lull = std::max(m_Lull, 0.0f);
		if (m_Lull <= 0.0f && m_Spawned < ActEncounters)
			SpawnEncounter(world, players);
	}

	const EncounterDef* RunDirector::ChooseEncounter(uint8_t tier, Random& random) const
	{
		uint32_t options = 0;
		const EncounterDef* chosen = nullptr;
		for (const EncounterDef& encounter : Encounters)
			if (encounter.Tier == tier && random.Below(++options) == 0)
				chosen = &encounter;
		return chosen;
	}

	std::vector<RunDirector::Recruit> RunDirector::Muster(const EncounterDef& encounter, uint32_t players, uint32_t affixes, Random& random) const
	{
		const uint32_t extra = players > 1 ? players - 1 : 0;
		std::vector<Recruit> roster;
		std::vector<EntityKind> mobile;
		for (const EncounterMember& member : encounter.Members)
		{
			const FighterKit* kit = FindFighterKit(member.Kind);
			const uint32_t count = kit ? member.Count : uint32_t(std::lround(float(member.Count) * (1.0f + 0.5f * float(extra))));
			for (uint32_t i = 0; i < count; i++)
				roster.push_back({ member.Kind });
			if (kit && !kit->Rooted && member.Count > 0)
				mobile.push_back(member.Kind);
		}
		for (uint32_t i = 0; i < extra && !mobile.empty(); i++)
			roster.push_back({ mobile[i % mobile.size()] });
		if (affixes > 0 && !mobile.empty())
			for (Recruit& recruit : roster)
				if (recruit.Kind == mobile.front())
				{
					recruit.Elite = true;
					recruit.Affixes = RollAffixes(random, recruit.Kind, affixes);
					break;
				}
		return roster;
	}

	void RunDirector::SpawnEncounter(World& world, uint32_t players)
	{
		Random& random = world.GetRandom();
		const uint32_t index = m_Spawned++;
		const uint8_t tier = EncounterTier(m_Status.Act, index);
		const EncounterDef* chosen = ChooseEncounter(tier, random);
		if (!chosen)
			return;
		float radius = 0.0f;
		for (const EncounterMember& member : chosen->Members)
			if (member.Count > 0)
				radius = std::max(radius, FindEnemyDef(member.Kind)->Radius);
		const std::optional<Vec2> socket = ChooseSocket(world, radius);
		if (!socket)
			return;
		const std::vector<Recruit> roster = Muster(*chosen, players, EncounterAffixes(m_Status.Act, index), random);
		float delay = 0.0f;
		for (const Recruit& recruit : roster)
		{
			m_Arrivals.push_back({ recruit.Kind, *socket + FromAngle(random.Float() * 2.0f * Pi) * (EncounterSpread * std::sqrt(random.Float())), delay, recruit.Elite ? EliteFighterHealth : 1.0f, recruit.Elite, true, recruit.Affixes });
			delay += MemberStagger;
		}
		m_OpenTier = m_Open == 0 ? tier : std::max(m_OpenTier, tier);
		m_EncounterClock = 0.0f;
		m_Reinforce = ReinforceSeconds;
		m_Reinforcements = 0;
		m_Open++;
		uint8_t leader = 0;
		for (const Recruit& recruit : roster)
			leader = recruit.Elite && leader == 0 ? uint8_t(recruit.Affixes | 0x80u) : leader;
		Announce(world, "Encounter " + std::to_string(index + 1) + " of " + std::to_string(ActEncounters) + ": " + chosen->Name + (leader ? ChampionText(uint8_t(leader & 0x7Fu)) : std::string()), false);
	}

	void RunDirector::ClearEncounter(World& world)
	{
		const uint8_t cleared = m_Open;
		m_Open = 0;
		m_Status.Wave = uint8_t(std::min<uint32_t>(m_Status.Wave + cleared, ActEncounters));
		m_Lull = EncounterLull;
		for (RunSeat& seat : m_Seats)
		{
			seat.WardSpent = false;
			seat.Sips = 0;
		}
		const Vec2 spot = world.PlaceDrop(m_LastFall, m_LastFall);
		m_Orbs.Drop(spot, (ClearWorth + ClearWorthPerTier * m_OpenTier) * cleared, uint64_t(world.GetTick()) * 2654435761ull);
		if (m_Status.Wave % ChestEvery == 0 && m_Status.Wave < ActEncounters)
			if (Loot* loot = world.GetEntities().TryGet<Loot>(world.SpawnLoot(LootKind::Chest, world.PlaceDrop(spot, spot - Vec2(0.7f, 0.0f)))))
				loot->Lifetime = ChestLifetime;
		if (m_Status.Wave >= ActEncounters)
		{
			OpenGate(world);
			return;
		}
		Announce(world, "Encounter cleared. Catch your breath", true);
	}

	void RunDirector::SpendOnly(World& world, float dt, uint32_t players)
	{
		EntityRegistry& entities = world.GetEntities();
		const EntityKind kind = RunOnlyKinds[std::max<uint8_t>(m_Rules.Only, 1) - 1];
		const EnemyDef* def = FindEnemyDef(kind);
		uint32_t alive = 0;
		uint32_t count = 0;
		entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			const Health* health = entities.TryGet<Health>(entity);
			if (!health || health->IsDead())
				return;
			alive++;
			count += enemy.Type == kind ? 1u : 0u;
		});
		for (const Arrival& arrival : m_Arrivals)
			count += arrival.Kind == kind ? 1u : 0u;
		const uint32_t pending = uint32_t(m_Arrivals.size());
		const float rate = float(m_Rules.Spawns) / 100.0f;
		const bool fighter = IsFighterKind(kind);
		m_Status.Alive = uint16_t(alive);
		m_Status.Goal = uint16_t(std::min<uint32_t>(uint32_t(std::lround(float(fighter ? FighterOnlyGoal : GetShardfallPopulation(m_Status.ActClock, players, m_Status.Act)) * (fighter ? PartyScale(players) : rate))), ShardfallEnemyCap));
		const float minutes = RunMinutes(m_Status.Act, m_Status.ActClock);
		m_Budget = std::min(m_Budget + BudgetRate * (1.0f + 0.1f * minutes) * PartyScale(players) * rate * dt, BudgetReserve);
		if (rate <= 0.0f || (kind != EntityKind::PrismaticCrawler && alive + pending >= m_Status.Goal))
			return;
		uint32_t cap = ShardfallEnemyCap;
		for (const auto& [capped, limit] : Caps)
			cap = capped == kind ? limit : cap;
		const uint32_t cost = std::max<uint32_t>(def->Cost, 1u);
		const uint32_t room = m_Status.Goal > alive + pending ? m_Status.Goal - alive - pending : 1u;
		const uint32_t group = std::min({ std::max(OnlyGroupCost / cost, 1u), cap > count ? cap - count : 0u, fighter ? room : ShardfallEnemyCap, ShardfallEnemyCap > alive + pending ? ShardfallEnemyCap - alive - pending : 0u });
		if (group == 0)
			return;
		const bool afford = float(cost * group) <= m_Budget;
		if (kind != EntityKind::PrismaticCrawler && !fighter && !afford && alive + pending >= uint32_t(m_Status.Goal) / 3u)
			return;
		const std::optional<Vec2> socket = ChooseSocket(world, def->Radius);
		if (!socket)
			return;
		if (afford)
			m_Budget -= float(cost * group);
		Random& random = world.GetRandom();
		for (uint32_t i = 0; i < group; i++)
		{
			uint8_t affixes = 0;
			if (m_Rules.Affix == RandomAffix)
				affixes = RollAffixes(random, kind, 1);
			else if (m_Rules.Affix > 0 && AffixFits(kind, EliteAffix(m_Rules.Affix - 1)))
				affixes = AffixBit(EliteAffix(m_Rules.Affix - 1));
			const bool elite = m_Rules.Affix > 0;
			m_Arrivals.push_back({ kind, *socket + FromAngle(random.Float() * 2.0f * Pi) * (PackSpread * std::sqrt(random.Float())), float(i) * MemberStagger, elite && fighter ? EliteFighterHealth : 1.0f, elite, false, affixes });
		}
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
			EnemyScaling scaling = Scale(players, arrival.Kind);
			scaling.Health *= arrival.Health;
			scaling.Elite = arrival.Elite;
			if (arrival.Elite && IsFighterKind(arrival.Kind))
				scaling.Damage *= arrival.Champion ? ChampionDamage : EliteFighterDamage;
			const float size = arrival.Champion ? ChampionSize : arrival.Elite ? EliteSize : 1.0f;
			const Entity entity = world.SpawnEnemy(arrival.Kind, world.FindFreeSpotNear(arrival.Position, def->Radius * size), scaling);
			if (Enemy* enemy = world.GetEntities().TryGet<Enemy>(entity))
			{
				enemy->Sprout = SproutSeconds;
				enemy->Rank = arrival.Rank;
				if (IsFighterKind(arrival.Kind))
					enemy->CooldownScale *= FighterPace;
				if (arrival.Rank > 0)
					enemy->CooldownScale *= VeteranCooldown;
				if (arrival.Champion)
				{
					enemy->Champion = true;
					enemy->PoiseScale = ChampionPoise;
					m_Champion = entity;
					if (Replicated* replicated = world.GetEntities().TryGet<Replicated>(entity))
						replicated->Scale = uint8_t(std::lround(ChampionSize * 16.0f));
					if (Collider* collider = world.GetEntities().TryGet<Collider>(entity))
						collider->Radius = def->Radius * ChampionSize;
				}
				enemy->Strafe = world.GetRandom().Below(2) == 0 ? 1.0f : -1.0f;
				if (const FighterKit* kit = FindFighterKit(arrival.Kind))
					enemy->GuardLeft = kit->Guard;
				if (arrival.Pack != 0)
				{
					enemy->Awake = false;
					enemy->Pack = arrival.Pack;
					if (const Transform* transform = world.GetEntities().TryGet<Transform>(entity))
						enemy->Home = transform->Position;
					enemy->Wander = 1.0f + 2.0f * world.GetRandom().Float();
				}
			}
			if (arrival.Affixes != 0)
				ApplyAffixes(world, entity, arrival.Affixes);
			if (arrival.Member && entity.IsValid())
				m_Members.push_back(entity);
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
			if (!def || !transform || def->Behavior == EnemyBehavior::Treasure || def->Behavior == EnemyBehavior::Egg || enemy.Sprout > 0.0f || !enemy.Awake)
				return;
			if (std::find(m_Guardians.begin(), m_Guardians.end(), entity) != m_Guardians.end())
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

	void RunDirector::StartRooms(World& world)
	{
		m_Map = MakeActMap(m_Rules.Seed, m_Status.Act);
		const uint8_t entry = RoomIndex(0, 0);
		const MapKind map = RoomMapFor(m_Status.Act, m_Map.At(entry).Kind);
		if (world.GetSettings().Map != map || world.GetSettings().Seed != RoomLevelSeed(m_Rules.Seed, m_Status.Act, m_Map, entry))
		{
			TravelTo(world, entry);
			return;
		}
		m_Status.Room = entry;
		m_Status.Route = RoomBit(entry);
		EnterRoom(world);
	}

	void RunDirector::TravelTo(World& world, uint8_t room)
	{
		if (m_Reward != RoomReward::None)
			GiveReward(world, std::exchange(m_Reward, RoomReward::None));
		m_Status.Room = room;
		m_Status.Route |= RoomBit(room);
		m_Status.Cleared = false;
		m_Status.Votes = {};
		m_DoorHold = 0.0f;
		m_DoorPick = NoRoom;
		m_PendingArena = ArenaChange{ RoomMapFor(m_Status.Act, m_Map.At(room).Kind), RoomLevelSeed(m_Rules.Seed, m_Status.Act, m_Map, room) };
		m_Revision++;
	}

	void RunDirector::EnterRoom(World& world)
	{
		const MapRoom& node = m_Map.At(m_Status.Room);
		m_Status.Cleared = false;
		m_Status.RoomClock = 0.0f;
		m_Status.Par = 0.0f;
		m_Status.Medal = RoomMedal::None;
		m_Status.Votes = {};
		m_Status.PhaseClock = 0.0f;
		m_DoorHold = 0.0f;
		m_DoorPick = NoRoom;
		m_SinceClear = 0.0f;
		m_Reward = RoomReward::None;
		m_Champion = {};
		m_Status.Champion = 0;
		m_Status.ChampionPhase = 0;
		EntityRegistry& entities = world.GetEntities();
		std::vector<Entity> downed;
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (!controller.State.Alive)
				downed.push_back(entity);
		});
		for (const Entity entity : downed)
			world.RevivePlayer(entity, RoomReviveHealth);
		for (RunSeat& seat : m_Seats)
		{
			seat.Revive = 0.0f;
			seat.Loadout.Rested = 0;
			seat.Winds = 0;
			seat.WardSpent = false;
			seat.Sips = 0;
			if (seat.Present && node.Kind == RoomKind::Shop)
				FillShop(seat);
		}
		switch (node.Kind)
		{
		case RoomKind::Guardian:
			m_Status.Phase = RunPhase::Gate;
			Announce(world, ActName(m_Status.Act) + " guardian: step onto the guardian stone in the cobbled ring when you are ready", false);
			break;
		case RoomKind::Rest:
			m_Status.Phase = RunPhase::Act;
			m_Status.Cleared = true;
			Announce(world, "A quiet place. Rest at the left altar to heal, or train at the right one to rank up a spell, then pick a door", true);
			break;
		case RoomKind::Shop:
			m_Status.Phase = RunPhase::Act;
			m_Status.Cleared = true;
			Announce(world, "A crystal merchant. Spend your shards, sell or reorder sigils, then pick a door", true);
			break;
		case RoomKind::Shrine:
			m_Status.Phase = RunPhase::Act;
			m_Status.Cleared = true;
			Announce(world, "An old shrine. Each of you may ask one boon of it, then pick a door", true);
			break;
		default:
			m_Status.Phase = RunPhase::Act;
			SpawnRoom(world, std::max(world.CountPlayers(), 1u));
			break;
		}
		m_Revision++;
	}

	void RunDirector::SpawnRoom(World& world, uint32_t players)
	{
		const MapRoom& node = m_Map.At(m_Status.Room);
		const bool elite = node.Kind == RoomKind::Elite;
		Random random(MixSeed(m_Rules.Seed, 0xF16u + m_Status.Act, m_Status.Room));
		m_RoomTier = RoomTier(m_Status.Act, m_Status.Room, node.Kind);
		const uint8_t rank = m_Status.Act >= 2 || m_RoomTier >= 3 ? 1 : 0;
		const uint32_t extra = players > 1 ? players - 1 : 0;
		std::vector<std::vector<Recruit>> packs;
		std::string title;
		if (elite)
		{
			std::vector<const ChampionDef*> options;
			for (const ChampionDef& champion : Champions)
				if (champion.MinAct <= m_Status.Act)
					options.push_back(&champion);
			const ChampionDef& champion = *options[random.Below(uint32_t(options.size()))];
			const uint32_t affixes = m_Status.Act >= 2 ? m_Status.Act - 1u : 0u;
			packs.push_back({ Recruit{ champion.Kind, true, affixes > 0 ? RollAffixes(random, champion.Kind, affixes, AffixBit(EliteAffix::Splitting)) : uint8_t(0), ChampionRank, true } });
			std::vector<Recruit> brood;
			const uint32_t crawlers = uint32_t(std::lround(float(3u + m_Status.Act) * (1.0f + 0.5f * float(extra))));
			for (uint32_t i = 0; i < crawlers; i++)
				brood.push_back({ i < m_Status.Act - 1u ? EntityKind::JadeCrawler : EntityKind::QuartzCrawler });
			packs.push_back(std::move(brood));
			if (m_Status.Act >= 2)
			{
				constexpr EntityKind Guards[] = { EntityKind::CrystalDuelist, EntityKind::ShardArcher, EntityKind::PrismMage };
				EntityKind guard = Guards[random.Below(uint32_t(std::size(Guards)))];
				if (guard == champion.Kind)
					guard = champion.Kind == EntityKind::ShardArcher ? EntityKind::CrystalDuelist : EntityKind::ShardArcher;
				packs.push_back({ Recruit{ guard, false, 0, 1 } });
			}
			title = std::string("Champion: ") + champion.Name;
		}
		else
		{
			const uint32_t encounters = RoomEncounters(m_Status.Act, m_Status.Room);
			for (uint32_t e = 0; e < encounters; e++)
			{
				const EncounterDef* chosen = ChooseEncounter(uint8_t(std::max<int32_t>(int32_t(m_RoomTier) - int32_t(e), 1)), random);
				if (!chosen)
					continue;
				title += (title.empty() ? "" : ", and ") + std::string(chosen->Name);
				std::vector<Recruit> roster = Muster(*chosen, players, 0, random);
				std::vector<Recruit> lead;
				std::vector<Recruit> crawlers;
				for (Recruit& recruit : roster)
				{
					recruit.Rank = IsFighterKind(recruit.Kind) ? rank : 0;
					(IsFighterKind(recruit.Kind) ? lead : crawlers).push_back(recruit);
				}
				const size_t half = crawlers.size() / 2;
				lead.insert(lead.end(), crawlers.begin(), crawlers.begin() + ptrdiff_t(half));
				packs.push_back(std::move(lead));
				if (crawlers.size() > half)
					packs.emplace_back(crawlers.begin() + ptrdiff_t(half), crawlers.end());
			}
			const uint32_t count = uint32_t(std::lround(float(ReinforceCrawlers + m_RoomTier) * (1.0f + 0.5f * float(extra))));
			std::vector<Recruit> swarm;
			for (uint32_t i = 0; i < count; i++)
				swarm.push_back({ i == 0 && m_RoomTier >= 2 ? EntityKind::JadeCrawler : i == 1 && m_RoomTier >= 3 ? EntityKind::SapphireCrawler : EntityKind::QuartzCrawler });
			packs.push_back(std::move(swarm));
		}
		const std::vector<Vec2> sockets = PackSockets(world.GetLevel(), random, packs.size());
		uint32_t fighters = 0;
		uint32_t others = 0;
		uint8_t leader = 0;
		for (size_t p = 0; p < packs.size(); p++)
		{
			if (m_NextPack == 0)
				m_NextPack = 1;
			const uint16_t pack = m_NextPack++;
			float delay = 0.0f;
			for (const Recruit& recruit : packs[p])
			{
				const float health = recruit.Champion ? ChampionHealth : recruit.Elite ? EliteFighterHealth : 1.0f;
				m_Arrivals.push_back({ recruit.Kind, sockets[p] + FromAngle(random.Float() * 2.0f * Pi) * (recruit.Champion ? 0.0f : EncounterSpread * std::sqrt(random.Float())), delay, health, recruit.Elite, true, recruit.Affixes, pack, recruit.Rank, recruit.Champion });
				delay += MemberStagger;
				(IsFighterKind(recruit.Kind) ? fighters : others) += recruit.Champion ? 8u : recruit.Elite ? 2u : 1u;
				leader = recruit.Elite && leader == 0 ? uint8_t(recruit.Affixes | 0x80u) : leader;
			}
		}
		m_Status.Par = RoomPar(fighters, others);
		Announce(world, title + (leader && (leader & 0x7Fu) ? ChampionText(uint8_t(leader & 0x7Fu)) : std::string()), false);
	}

	void RunDirector::UpdateRoom(World& world, float dt)
	{
		if (m_PendingArena)
			return;
		EntityRegistry& entities = world.GetEntities();
		uint32_t alive = 0;
		entities.Each<Enemy>([&](Entity entity, Enemy&)
		{
			const Health* health = entities.TryGet<Health>(entity);
			alive += health && !health->IsDead() ? 1u : 0u;
		});
		m_Status.Alive = uint16_t(alive);
		m_Status.Goal = 0;
		std::erase_if(m_Members, [&](Entity entity)
		{
			const Health* health = entities.TryGet<Health>(entity);
			return !health || health->IsDead();
		});
		if (!m_Status.Cleared)
		{
			m_Status.RoomClock += dt;
			UpdateChampion(world);
			uint32_t fighters = 0;
			uint32_t crawlers = 0;
			for (const Entity entity : m_Members)
				if (const Enemy* enemy = entities.TryGet<Enemy>(entity))
					(IsFighterKind(enemy->Type) ? fighters : crawlers)++;
			const bool arriving = std::any_of(m_Arrivals.begin(), m_Arrivals.end(), [](const Arrival& arrival) { return arrival.Member; });
			if (!arriving && fighters == 0 && crawlers <= ClearStragglers)
				ClearRoom(world);
			return;
		}
		m_SinceClear += dt;
		if (m_SinceClear > MomentumGrace)
			m_Status.Momentum = std::max(m_Status.Momentum - MomentumDecay * dt, 0.0f);
		if (!world.GetLevel().Altars.empty() && m_Map.At(m_Status.Room).Kind == RoomKind::Rest)
			UpdateRest(world);
		UpdateDoors(world, dt);
	}

	void RunDirector::ClearRoom(World& world)
	{
		const MapRoom& node = m_Map.At(m_Status.Room);
		const RoomMedal medal = MedalFor(m_Status.RoomClock, m_Status.Par);
		m_Status.Cleared = true;
		m_Status.Medal = medal;
		m_Status.Momentum = std::min(m_Status.Momentum + MomentumGain[size_t(medal)], 1.0f);
		m_Status.Golds = uint16_t(m_Status.Golds + (medal == RoomMedal::Gold ? 1 : 0));
		m_Status.Champion = 0;
		GrantShards(uint32_t(ShardsPerRoom + ShardsPerTier * m_RoomTier + MedalShards[size_t(medal)] + (node.Kind == RoomKind::Elite ? EliteShards : 0)));
		m_Status.Rooms++;
		m_SinceClear = 0.0f;
		Crumble(world);
		const Vec2 spot = world.PlaceDrop(m_LastFall, m_LastFall);
		const float share = node.Kind == RoomKind::Elite ? EliteClearWorth : 1.0f;
		m_Orbs.Drop(spot, uint32_t(std::lround(float(ClearWorth + ClearWorthPerTier * m_RoomTier) * share)), uint64_t(world.GetTick()) * 2654435761ull);
		if (node.Reward == RoomReward::Cache)
			m_Orbs.Drop(world.PlaceDrop(spot, spot + Vec2(0.0f, 0.8f)), uint32_t(std::lround(float(CacheShards) / ShardsPerWorth)), uint64_t(world.GetTick()) * 40503ull + 7ull);
		else if (node.Reward != RoomReward::None)
		{
			m_Reward = node.Reward;
			if (Loot* loot = world.GetEntities().TryGet<Loot>(world.SpawnLoot(LootKind::Chest, world.PlaceDrop(spot, spot - Vec2(0.7f, 0.0f)))))
				loot->Lifetime = 1e9f;
		}
		std::string text = "Room cleared in " + ClockText(m_Status.RoomClock);
		if (medal != RoomMedal::None)
			text += ", " + std::string(RoomMedalNames[size_t(medal)]) + " medal";
		Announce(world, text + ". Pick a door when you are ready", true);
		m_Revision++;
	}

	void RunDirector::UpdateRest(World& world)
	{
		const std::vector<Vec2>& altars = world.GetLevel().Altars;
		EntityRegistry& entities = world.GetEntities();
		struct Visit
		{
			Entity Handle;
			uint32_t PlayerId = 0;
			uint8_t Altar = 0;
		};
		std::vector<Visit> visits;
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			const Transform* transform = entities.TryGet<Transform>(entity);
			if (!transform || !controller.State.Alive || controller.PlayerId >= MaxPlayers || m_Seats[controller.PlayerId].Loadout.Rested != 0)
				return;
			for (size_t i = 0; i < altars.size() && i < 2; i++)
				if (LengthSquared(transform->Position - altars[i]) <= AltarRadius * AltarRadius)
				{
					visits.push_back({ entity, controller.PlayerId, uint8_t(i + 1) });
					break;
				}
		});
		for (const Visit& visit : visits)
		{
			RunSeat& seat = m_Seats[visit.PlayerId];
			seat.Loadout.Rested = visit.Altar;
			world.SpawnEffect(AbilityId::Ascension, altars[visit.Altar - 1u], 0.0f, visit.PlayerId, 0, 1.0f);
			if (visit.Altar == 1)
			{
				if (Health* health = entities.TryGet<Health>(visit.Handle))
					health->Current = std::min(health->Max, health->Current + std::max(1, int32_t(std::lround(float(health->Max) * RestHeal * HealScale(visit.PlayerId)))));
				world.Notify(visit.PlayerId, "You rest by the altar and recover", true);
			}
			else
			{
				GrantFocused(world, seat, RunCardKind::Rank);
				world.Notify(visit.PlayerId, "You train at the altar: choose a spell to rank up", true);
			}
			m_Revision++;
		}
	}

	void RunDirector::UpdateDoors(World& world, float dt)
	{
		const Level& level = world.GetLevel();
		const uint8_t mask = DoorMaskOf(m_Map, m_Status.Room);
		std::array<uint8_t, DoorSlots> votes{};
		uint32_t living = 0;
		world.GetEntities().Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			const Transform* transform = world.GetEntities().TryGet<Transform>(entity);
			if (!transform || !controller.State.Alive)
				return;
			living++;
			for (uint8_t slot = 0; slot < DoorSlots && slot < level.Doors.size(); slot++)
				if ((mask & (1u << slot)) && LengthSquared(transform->Position - level.Doors[slot]) <= DoorRadius * DoorRadius)
					votes[slot]++;
		});
		m_Status.Votes = votes;
		uint8_t pick = NoRoom;
		for (uint8_t slot = 0; slot < DoorSlots; slot++)
			if (living > 0 && votes[slot] == living)
				pick = slot;
		if (pick != NoRoom && pick == m_DoorPick)
			m_DoorHold += dt;
		else
		{
			m_DoorPick = pick;
			m_DoorHold = 0.0f;
		}
		if (pick == NoRoom || m_DoorHold < DoorHoldSeconds)
			return;
		if (const uint8_t target = DoorTarget(m_Map, m_Status.Room, pick); target != NoRoom)
			TravelTo(world, target);
	}

	void RunDirector::GiveReward(World& world, RoomReward reward)
	{
		const auto grant = [&](RunCardKind focus, CardRarity floor = CardRarity::Common)
		{
			for (RunSeat& seat : m_Seats)
				GrantFocused(world, seat, focus, floor);
		};
		switch (reward)
		{
		case RoomReward::Spell:
			grant(RunCardKind::Spell);
			break;
		case RoomReward::Rank:
			grant(RunCardKind::Rank);
			break;
		case RoomReward::Card:
			grant(RunCardKind::Card);
			break;
		case RoomReward::Treasure:
			grant(RunCardKind::Sigil);
			grant(RunCardKind::Rune, CardRarity::Rare);
			break;
		case RoomReward::Rune:
			grant(RunCardKind::Rune);
			break;
		default:
			return;
		}
		Announce(world, std::string("The chest opens: ") + RoomRewardNames[size_t(reward)] + " for everyone", true);
	}

	void RunDirector::ShapeCast(World& world, PlayerController& controller, uint32_t slot, AbilityId ability, AbilityContext& context)
	{
		if (controller.PlayerId < MaxPlayers)
		{
			context.Runes = SlotRunes(m_Seats[controller.PlayerId].Loadout, slot);
			context.Depth = 0;
		}
		if (controller.PlayerId >= MaxPlayers || slot == 0 || IsGuardSpell(ability) || GetAbility(ability).Kind == AbilityKind::Dash)
			return;
		RunSeat& seat = m_Seats[controller.PlayerId];
		if (const float odd = SigilPower(seat.Loadout, SigilId::OddHours); odd > 0.0f)
		{
			seat.Casts++;
			context.DamageScale *= seat.Casts % 2 == 0 ? 1.0f + odd : OddHoursWeak;
		}
	}

	void RunDirector::AfterCast(World& world, Entity player, PlayerController& controller, uint32_t slot, AbilityId ability, const AbilityContext& context)
	{
		if (controller.PlayerId >= MaxPlayers)
			return;
		RunSeat& seat = m_Seats[controller.PlayerId];
		if (slot < AbilitySlots && HasRune(context.Runes, RuneId::Echo) && ++seat.SlotCasts[slot] % EchoEvery == 0)
		{
			AbilityContext again = context;
			again.Runes &= ~RuneBit(RuneId::Echo);
			again.Depth = 1;
			again.Tag = 0;
			again.Rewind = 0.0f;
			world.QueueCast(player, ability, again, EchoRuneDelay);
		}
		if (slot == 0)
		{
			const float echo = SigilPower(seat.Loadout, SigilId::GlassEcho);
			if (echo <= 0.0f || seat.LastSpell == AbilityId::Count || m_Status.RunClock - seat.EchoAt < GlassEchoSpacing)
				return;
			seat.EchoAt = m_Status.RunClock;
			AbilityContext recast = seat.LastContext;
			const float reach = Length(recast.Target - recast.Origin);
			recast.Origin = context.Origin;
			recast.Departure = context.Origin;
			recast.Direction = context.Direction;
			recast.Target = context.Origin + context.Direction * reach;
			recast.Rewind = 0.0f;
			recast.Tag = 0;
			recast.DamageScale *= GlassEchoShare * echo;
			world.QueueCast(player, seat.LastSpell, recast, 0.08f);
			return;
		}
		if (IsGuardSpell(ability) || GetAbility(ability).Kind == AbilityKind::Dash)
			return;
		seat.LastSpell = ability;
		seat.LastContext = context;
		if (ability == AbilityId::Comet && SigilPower(seat.Loadout, SigilId::HungryComet) > 0.0f)
			if (Health* health = world.GetEntities().TryGet<Health>(player); health && health->Current > 1)
				health->Current = std::max(1, health->Current - std::max(1, int32_t(std::lround(float(health->Max) * HungryCometCost))));
	}

	void RunDirector::OnSlammed(World& world, Entity entity, float speed)
	{
		EntityRegistry& entities = world.GetEntities();
		Enemy* enemy = entities.TryGet<Enemy>(entity);
		const Transform* transform = entities.TryGet<Transform>(entity);
		if (!enemy || !transform || enemy->PushBy >= MaxPlayers)
			return;
		const float slam = SigilPower(m_Seats[enemy->PushBy].Loadout, SigilId::WallSlam);
		if (slam <= 0.0f)
			return;
		const uint32_t by = enemy->PushBy;
		const int32_t damage = std::max(1, int32_t(std::lround((WallSlamBase + WallSlamPerSpeed * speed) * slam * (1.0f + SigilActDamage * float(m_Status.Act - 1)))));
		enemy->Stun = std::max(enemy->Stun, 0.4f);
		world.SpawnEffect(AbilityId::ShatterBurst, transform->Position, 0.0f, by, 0, 0.5f);
		ApplyDamage(world, entity, { .Amount = damage, .Attacker = by, .Damages = Element::Physical, .Ability = uint8_t(AbilityId::Resonance), .Unmitigated = true });
	}

	void RunDirector::OnFlung(World& world, Entity entity)
	{
		EntityRegistry& entities = world.GetEntities();
		Enemy* enemy = entities.TryGet<Enemy>(entity);
		const Transform* transform = entities.TryGet<Transform>(entity);
		const Collider* collider = entities.TryGet<Collider>(entity);
		if (!enemy || !transform || !collider || enemy->PushBy >= MaxPlayers || enemy->Crashed > 0.0f)
			return;
		const float domino = SigilPower(m_Seats[enemy->PushBy].Loadout, SigilId::Domino);
		if (domino <= 0.0f)
			return;
		const uint32_t by = enemy->PushBy;
		const Vec2 push = enemy->Push;
		const float speed = Length(push);
		std::vector<Entity> struck;
		entities.Each<Enemy>([&](Entity other, Enemy& victim)
		{
			const Transform* place = entities.TryGet<Transform>(other);
			const Collider* body = entities.TryGet<Collider>(other);
			const Health* health = entities.TryGet<Health>(other);
			if (other == entity || !place || !body || !health || health->IsDead() || victim.Crashed > 0.0f || victim.Sprout > 0.0f)
				return;
			const float reach = collider->Radius + body->Radius + 0.15f;
			if (LengthSquared(place->Position - transform->Position) > reach * reach)
				return;
			const float weight = std::max(EnemyWeight(victim.Type) * (victim.Champion ? ChampionWeight : victim.Elite ? EliteWeight : 1.0f), 0.0f);
			if (weight > 0.0f)
				victim.Push = ClampLength(victim.Push + push * (DominoShare / weight), EnemyMaxPush);
			victim.PushBy = by;
			victim.Crashed = 0.5f;
			struck.push_back(other);
		});
		if (struck.empty())
			return;
		enemy->Crashed = 0.5f;
		struck.push_back(entity);
		const int32_t damage = std::max(1, int32_t(std::lround((WallSlamBase * 0.5f + 1.5f * speed) * domino * (1.0f + SigilActDamage * float(m_Status.Act - 1)))));
		world.SpawnEffect(AbilityId::ShatterBurst, transform->Position, 0.0f, by, 0, 0.4f);
		for (const Entity hit : struck)
			ApplyDamage(world, hit, { .Amount = damage, .Attacker = by, .Damages = Element::Physical, .Ability = uint8_t(AbilityId::Resonance), .Unmitigated = true });
	}

	void RunDirector::NoteHurt(uint32_t playerId, Entity source)
	{
		if (playerId < MaxPlayers && source.IsValid())
			m_Seats[playerId].Grudge = source;
	}

	void RunDirector::PyreAt(World& world, Vec2 position, int32_t damage, uint32_t attacker)
	{
		world.SpawnEffect(AbilityId::RubyBurst, position, 0.0f, attacker, 0, EmberPyreRadius / std::max(GetAbility(AbilityId::RubyBurst).Reach, 0.1f));
		std::vector<Entity> struck;
		world.GetEntities().Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			const Transform* transform = world.GetEntities().TryGet<Transform>(entity);
			const Health* health = world.GetEntities().TryGet<Health>(entity);
			if (transform && health && !health->IsDead() && enemy.Sprout <= 0.0f && LengthSquared(transform->Position - position) <= EmberPyreRadius * EmberPyreRadius)
				struck.push_back(entity);
		});
		for (const Entity entity : struck)
			ApplyDamage(world, entity, { .Amount = damage, .Attacker = attacker, .Damages = Element::Fire, .Ability = uint8_t(AbilityId::Ignite), .Unmitigated = true });
	}

	std::optional<Entity> RunDirector::NearestEnemy(World& world, Vec2 position, float range, Entity except) const
	{
		std::optional<Entity> best;
		float bestDistance = range * range;
		world.GetEntities().Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			const Transform* transform = world.GetEntities().TryGet<Transform>(entity);
			const Health* health = world.GetEntities().TryGet<Health>(entity);
			if (entity == except || !transform || !health || health->IsDead() || enemy.Sprout > 0.0f || !enemy.Awake)
				return;
			if (const float distance = LengthSquared(transform->Position - position); distance < bestDistance)
			{
				bestDistance = distance;
				best = entity;
			}
		});
		return best;
	}

	void RunDirector::CastFor(World& world, uint32_t playerId, AbilityId ability, Vec2 origin, Vec2 target, float share, float delay, uint8_t depth, float charge)
	{
		const Entity player = world.FindPlayer(playerId);
		const PlayerController* controller = world.GetEntities().TryGet<PlayerController>(player);
		if (!controller || playerId >= MaxPlayers)
			return;
		const RunLoadout& loadout = m_Seats[playerId].Loadout;
		const int32_t slot = FindRunSlot(loadout, CreditOf(ability));
		const SkillTraits skill = slot >= 0 ? controller->Traits.Skills[size_t(slot)] : MakeRunSkill(ability, 1, loadout.Cards);
		AbilityContext context;
		context.Origin = origin;
		context.Departure = origin;
		context.Direction = LengthSquared(target - origin) > 1e-4f ? Normalize(target - origin) : Vec2(1.0f, 0.0f);
		context.Target = target;
		context.Side = Team::Players;
		context.Owner = playerId;
		context.Source = player;
		context.DamageScale = skill.DamageScale * share;
		context.AreaScale = controller->Traits.AreaScale * skill.AreaScale;
		context.PushScale = skill.PushScale * share;
		context.CritChance = controller->Traits.CritChance;
		context.CritMultiplier = controller->Traits.CritMultiplier;
		context.Cards = GetCards(playerId);
		context.ExtraProjectiles = uint8_t(controller->Traits.ExtraProjectiles + skill.ExtraProjectiles);
		context.ExtraPierce = uint8_t(controller->Traits.ExtraPierce + skill.ExtraPierce);
		context.ExtraChain = skill.ExtraChain;
		context.RangeScale = skill.RangeScale;
		context.Charge = charge;
		context.Runes = slot >= 0 ? SlotRunes(loadout, uint32_t(slot)) & ~RuneBit(RuneId::Echo) : 0;
		context.Depth = depth;
		world.QueueCast(player, ability, context, delay);
	}

	void RunDirector::TriggerLink(World& world, RunSeat& seat, uint32_t playerId, Entity target, uint8_t depth)
	{
		if (m_Status.RunClock < seat.LinkReady)
			return;
		const RunePlace place = FindRune(seat.Loadout, RuneId::Link);
		const AbilityId linked = place.IsSet() ? LinkedSpell(seat.Loadout, uint32_t(place.Slot)) : AbilityId::Count;
		EntityRegistry& entities = world.GetEntities();
		const Transform* struck = entities.TryGet<Transform>(target);
		const Transform* caster = entities.TryGet<Transform>(world.FindPlayer(playerId));
		if (linked == AbilityId::Count || !struck || !caster)
			return;
		seat.LinkReady = m_Status.RunClock + LinkCooldown;
		const Vec2 at = struck->Position;
		Vec2 from = at;
		Vec2 to = at;
		if (GetAbility(linked).Kind == AbilityKind::Projectile)
		{
			if (const std::optional<Entity> next = NearestEnemy(world, at, LinkReach, target))
				to = entities.TryGet<Transform>(*next)->Position;
			else
				from = at - (LengthSquared(at - caster->Position) > 1e-4f ? Normalize(at - caster->Position) : Vec2(1.0f, 0.0f)) * 0.9f;
		}
		const AbilityId cast = linked == AbilityId::GlacialNova ? AbilityId::GlacialEruption : linked;
		CastFor(world, playerId, cast, from, to, LinkShare, 0.05f, uint8_t(depth + 1), IsChargedShot(linked) ? 0.6f : 0.0f);
	}

	bool RunDirector::CanArrange(World& world) const
	{
		bool fighting = false;
		world.GetEntities().Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			const Health* health = world.GetEntities().TryGet<Health>(entity);
			fighting = fighting || (enemy.Awake && health && !health->IsDead());
		});
		return (!fighting || m_Rules.AllRunes) && !IsRunOver(m_Status.Phase);
	}

	bool RunDirector::Socket(World& world, uint32_t playerId, RuneId rune, uint32_t slot, uint32_t socket)
	{
		if (playerId >= MaxPlayers || !m_Seats[playerId].Present || !CanArrange(world))
			return false;
		RunLoadout& loadout = m_Seats[playerId].Loadout;
		if (!CanSocket(loadout, rune, slot, socket))
			return false;
		const RunePlace old = FindRune(loadout, rune);
		const RuneId displaced = loadout.Sockets[slot][socket];
		if (displaced == rune)
			return false;
		if (old.IsSet())
			loadout.Sockets[size_t(old.Slot)][size_t(old.Socket)] = RuneId::None;
		loadout.Sockets[slot][socket] = rune;
		if (old.IsSet() && CanSocket(loadout, displaced, uint32_t(old.Slot), uint32_t(old.Socket)))
			loadout.Sockets[size_t(old.Slot)][size_t(old.Socket)] = displaced;
		world.RefreshTraits(world.FindPlayer(playerId));
		m_Revision++;
		return true;
	}

	bool RunDirector::Unsocket(World& world, uint32_t playerId, RuneId rune)
	{
		if (playerId >= MaxPlayers || !m_Seats[playerId].Present || !CanArrange(world))
			return false;
		RunLoadout& loadout = m_Seats[playerId].Loadout;
		const RunePlace place = FindRune(loadout, rune);
		if (!place.IsSet())
			return false;
		loadout.Sockets[size_t(place.Slot)][size_t(place.Socket)] = RuneId::None;
		world.RefreshTraits(world.FindPlayer(playerId));
		m_Revision++;
		return true;
	}

	void RunDirector::UpdateRunes(World& world, float dt)
	{
		std::vector<Blast> ready;
		for (size_t i = 0; i < m_Blasts.size();)
		{
			if ((m_Blasts[i].Delay -= dt) > 0.0f)
			{
				i++;
				continue;
			}
			ready.push_back(m_Blasts[i]);
			m_Blasts.erase(m_Blasts.begin() + ptrdiff_t(i));
		}
		EntityRegistry& entities = world.GetEntities();
		for (const Blast& blast : ready)
		{
			world.SpawnEffect(AbilityId::RuneBlast, blast.Position, 0.0f, blast.By, 0, CombustionRadius / std::max(GetAbility(AbilityId::RuneBlast).Reach, 0.1f));
			std::vector<Entity> struck;
			entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
			{
				const Transform* transform = entities.TryGet<Transform>(entity);
				const Collider* body = entities.TryGet<Collider>(entity);
				const Health* health = entities.TryGet<Health>(entity);
				const float reach = CombustionRadius + (body ? body->Radius : 0.0f);
				if (transform && health && !health->IsDead() && enemy.Sprout <= 0.0f && LengthSquared(transform->Position - blast.Position) <= reach * reach)
					struck.push_back(entity);
			});
			for (const Entity entity : struck)
				ApplyDamage(world, entity, { .Amount = blast.Damage, .Attacker = blast.By, .Damages = Element::Fire, .Ability = uint8_t(AbilityId::RuneBlast), .Runes = RuneBit(RuneId::Combustion) });
		}
	}

	void RunDirector::TriggerDuos(World& world, Entity target, uint32_t attacker, AbilityId ability)
	{
		RunSeat& seat = m_Seats[attacker];
		if (seat.Loadout.Duos == 0)
			return;
		EntityRegistry& entities = world.GetEntities();
		const Transform* transform = entities.TryGet<Transform>(target);
		Enemy* enemy = entities.TryGet<Enemy>(target);
		if (!transform || !enemy)
			return;
		const AbilityId source = CreditOf(ability);
		const Vec2 at = transform->Position;
		const auto ready = [&](DuoId duo)
		{
			if (!HasDuo(seat.Loadout, duo) || m_Status.RunClock < seat.DuoReady[size_t(duo)])
				return false;
			seat.DuoReady[size_t(duo)] = m_Status.RunClock + DuoSpacing;
			return true;
		};
		if (source == AbilityId::Comet && HasDuo(seat.Loadout, DuoId::FrozenComet))
		{
			if (enemy->Chill > 0.0f && !enemy->Champion)
				enemy->Stun = std::max(enemy->Stun, 1.0f);
			enemy->Chill = std::max(enemy->Chill, 2.5f);
		}
		if (source == AbilityId::ChainHook && ready(DuoId::AbyssalHook))
			CastFor(world, attacker, AbilityId::GravityWell, at, at, DuoShare, 0.05f);
		if (source == AbilityId::Firebomb && ready(DuoId::StormSkip))
			if (const std::optional<Entity> next = NearestEnemy(world, at, 7.0f, target))
				CastFor(world, attacker, AbilityId::ArcLightning, at, entities.TryGet<Transform>(*next)->Position, DuoShare, 0.05f);
		if (source == AbilityId::FrostSpear && enemy->Chill > 0.0f && ready(DuoId::ThunderSpear))
			CastFor(world, attacker, AbilityId::StormStrike, at, at, DuoShare, 0.05f);
		if (source == AbilityId::EarthSpike && ready(DuoId::ObsidianQuake))
			for (const float turn : { 0.0f, Pi })
				CastFor(world, attacker, AbilityId::ObsidianBlades, at, at + FromAngle(turn + float(world.GetTick() % 7)) * 4.0f, DuoShare, 0.05f);
		if (source == AbilityId::PurpleFireball && ready(DuoId::VioletSingularity))
			CastFor(world, attacker, AbilityId::GravityWell, at, at, DuoShare, 0.05f);
	}

	void RunDirector::UpdateSigils(World& world, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		bool hunting = false;
		entities.Each<Enemy>([&](Entity, Enemy& enemy) { hunting = hunting || (enemy.Awake && enemy.Sprout <= 0.0f); });
		struct Cut
		{
			Entity Target;
			uint32_t By = 0;
			int32_t Damage = 0;
			float Stun = 0.0f;
		};
		std::vector<Cut> cuts;
		entities.Each<PlayerController>([&](Entity player, PlayerController& controller)
		{
			if (controller.PlayerId >= MaxPlayers || !controller.State.Alive)
				return;
			RunSeat& seat = m_Seats[controller.PlayerId];
			Health* health = entities.TryGet<Health>(player);
			const Transform* self = entities.TryGet<Transform>(player);
			if (health && hunting && HasCurse(seat.Loadout, CurseId::Fever))
			{
				seat.FeverBank += FeverDrain * dt;
				while (seat.FeverBank >= 1.0f)
				{
					seat.FeverBank -= 1.0f;
					health->Current = std::max(1, health->Current - 1);
				}
			}
			if (seat.RiftStep <= 0.0f || !self)
				return;
			seat.RiftStep = std::max(seat.RiftStep - dt, 0.0f);
			const float power = SigilPower(seat.Loadout, SigilId::RiftStep);
			entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
			{
				const Transform* place = entities.TryGet<Transform>(entity);
				const Collider* body = entities.TryGet<Collider>(entity);
				if (!place || !body || enemy.Sprout > 0.0f || std::find(seat.RiftHits.begin(), seat.RiftHits.end(), entity) != seat.RiftHits.end())
					return;
				const float reach = RiftStepRadius + body->Radius;
				if (LengthSquared(place->Position - self->Position) > reach * reach)
					return;
				seat.RiftHits.push_back(entity);
				cuts.push_back({ entity, controller.PlayerId, std::max(1, int32_t(std::lround(float(RiftStepDamage) * power * (1.0f + SigilActDamage * float(m_Status.Act - 1))))), RiftStepStun * power });
			});
		});
		for (const Cut& cut : cuts)
		{
			if (Enemy* enemy = entities.TryGet<Enemy>(cut.Target); enemy && !enemy->Champion)
				enemy->Stun = std::max(enemy->Stun, cut.Stun);
			ApplyDamage(world, cut.Target, { .Amount = cut.Damage, .Attacker = cut.By, .Damages = Element::Physical, .Ability = uint8_t(AbilityId::WarpWake), .Unmitigated = true });
		}
	}

	void RunDirector::UpdateChampion(World& world)
	{
		EntityRegistry& entities = world.GetEntities();
		Enemy* enemy = entities.TryGet<Enemy>(m_Champion);
		const Health* health = entities.TryGet<Health>(m_Champion);
		const Transform* transform = entities.TryGet<Transform>(m_Champion);
		if (!enemy || !health || health->IsDead() || !transform)
		{
			m_Status.Champion = 0;
			return;
		}
		if (const Replicated* replicated = entities.TryGet<Replicated>(m_Champion))
			m_Status.Champion = replicated->NetId;
		m_Status.ChampionPhase = enemy->Phase;
		if (enemy->Phase >= std::size(ChampionPhases) || enemy->Sprout > 0.0f)
			return;
		if (float(health->Current) > ChampionPhases[enemy->Phase] * float(health->Max))
			return;
		enemy->Phase++;
		m_Status.ChampionPhase = enemy->Phase;
		if (const int32_t roar = FindRoarMove(enemy->Type); roar >= 0)
		{
			const FighterMove& move = FindFighterKit(enemy->Type)->Moves[size_t(roar)];
			enemy->Move = int8_t(roar);
			enemy->Windup = move.Windup;
			enemy->DashLeft = 0.0f;
			enemy->Recovery = 0.0f;
			enemy->Stun = 0.0f;
			enemy->Stagger = 0.0f;
			enemy->GuardUp = false;
			enemy->Push = {};
			enemy->AimPoint = transform->Position;
			enemy->Evade = move.Windup + RoarGrace;
		}
		const ChampionDef* def = FindChampion(enemy->Type);
		const std::string name = def ? def->Name : "The champion";
		const bool enraged = enemy->Phase >= std::size(ChampionPhases);
		if (enraged)
		{
			enemy->SpeedScale *= ChampionEnrageSpeed;
			enemy->CooldownScale *= ChampionEnrageCooldown;
		}
		const uint32_t crawlers = RoarCrawlers + m_Status.Act - 1u;
		Random& random = world.GetRandom();
		for (uint32_t i = 0; i < crawlers; i++)
			m_Arrivals.push_back({ i == 0 && m_Status.Act >= 2 ? EntityKind::JadeCrawler : EntityKind::QuartzCrawler, transform->Position + FromAngle(2.0f * Pi * float(i) / float(crawlers) + random.Float()) * 3.8f, 0.1f * float(i), 1.0f, false, true });
		if (enraged && m_Status.Act >= 2)
		{
			constexpr EntityKind Guards[] = { EntityKind::CrystalDuelist, EntityKind::ShardArcher, EntityKind::PrismMage };
			EntityKind guard = Guards[random.Below(uint32_t(std::size(Guards)))];
			if (guard == enemy->Type)
				guard = EntityKind::ShardArcher == enemy->Type ? EntityKind::CrystalDuelist : EntityKind::ShardArcher;
			m_Arrivals.push_back({ .Kind = guard, .Position = transform->Position + FromAngle(random.Float() * 2.0f * Pi) * 4.5f, .Delay = 0.4f, .Health = 1.0f, .Member = true, .Rank = 1 });
		}
		Announce(world, enraged ? name + " is enraged and calls its guard" : name + " shatters its armour and summons the brood", false);
		m_Revision++;
	}

	void RunDirector::GrantShards(uint32_t amount)
	{
		for (RunSeat& seat : m_Seats)
			if (seat.Present)
				seat.Loadout.Shards = uint16_t(std::min<uint32_t>(seat.Loadout.Shards + amount * (HasCurse(seat.Loadout, CurseId::Greed) ? 2u : 1u), MaxShards));
		m_Revision++;
	}

	void RunDirector::FillShop(RunSeat& seat)
	{
		RunLoadout& loadout = seat.Loadout;
		loadout.Stock = {};
		loadout.Sold = 0;
		std::vector<CardId> cards;
		for (const CardId id : RunCardPool)
			if (CardFitsRun(loadout, id) && !HasCard(seat.BannedCards, id))
				cards.push_back(id);
		std::vector<RuneId> runes;
		for (const RuneDef& rune : Runes)
			if (!HasRune(loadout.Runes, rune.Id) && RuneFitsBuild(loadout, rune.Id))
				runes.push_back(rune.Id);
		for (uint32_t i = 0; i < 2 && !cards.empty(); i++)
		{
			const size_t pick = seat.Rng.Below(uint32_t(cards.size()));
			loadout.Stock[i] = { RunCardKind::Card, uint8_t(cards[pick]) };
			cards.erase(cards.begin() + ptrdiff_t(pick));
		}
		if (!runes.empty())
			loadout.Stock[1] = { RunCardKind::Rune, uint8_t(runes[seat.Rng.Below(uint32_t(runes.size()))]) };
		std::vector<SigilId> sigils;
		for (uint32_t id = 0; id < uint32_t(SigilId::Count); id++)
			if (!HasSigil(loadout, SigilId(id)))
				sigils.push_back(SigilId(id));
		if (!sigils.empty())
			loadout.Stock[2] = { RunCardKind::Sigil, uint8_t(sigils[seat.Rng.Below(uint32_t(sigils.size()))]) };
		loadout.Stock[3] = { RunCardKind::Service, uint8_t(ServiceId::Heal) };
		loadout.Stock[4] = { RunCardKind::Service, uint8_t(ServiceId::Reroll) };
		loadout.Stock[5] = { RunCardKind::Service, uint8_t(ServiceId::Banish) };
	}

	bool RunDirector::Buy(World& world, uint32_t playerId, uint32_t slot)
	{
		if (playerId >= MaxPlayers || slot >= ShopSlots || !RoomsOn() || m_Map.At(m_Status.Room).Kind != RoomKind::Shop)
			return false;
		RunSeat& seat = m_Seats[playerId];
		RunLoadout& loadout = seat.Loadout;
		const RunCard item = loadout.Stock[slot];
		const uint16_t price = ItemPrice(item, loadout.Draughts);
		if (!seat.Present || item.IsEmpty() || (loadout.Sold & (1u << slot)) || loadout.Shards < price)
			return false;
		const Entity player = world.FindPlayer(playerId);
		switch (item.Kind)
		{
		case RunCardKind::Card:
			if (!CardFitsRun(loadout, item.Upgrade()))
				return false;
			if (!HasCard(loadout.Cards, item.Upgrade()))
				loadout.Cards |= CardBit(item.Upgrade());
			else if (!HasCard(loadout.Second, item.Upgrade()))
				loadout.Second |= CardBit(item.Upgrade());
			else
				loadout.Third |= CardBit(item.Upgrade());
			seat.Picks.push_back({ item, RunStage(m_Status.Act, m_Status.Room) });
			break;
		case RunCardKind::Sigil:
		{
			const auto free = std::find(loadout.Sigils.begin(), loadout.Sigils.end(), SigilId::None);
			if (free == loadout.Sigils.end() || HasSigil(loadout, SigilId(item.Id)))
				return false;
			*free = SigilId(item.Id);
			seat.Picks.push_back({ item, RunStage(m_Status.Act, m_Status.Room) });
			break;
		}
		case RunCardKind::Rune:
			if (HasRune(loadout.Runes, item.Rune()))
				return false;
			loadout.Runes |= RuneBit(item.Rune());
			seat.Picks.push_back({ item, RunStage(m_Status.Act, m_Status.Room) });
			break;
		case RunCardKind::Service:
			if (ServiceId(item.Id) == ServiceId::Heal)
			{
				Health* health = world.GetEntities().TryGet<Health>(player);
				if (!health)
					return false;
				health->Current = std::min(health->Max, health->Current + std::max(1, int32_t(std::lround(float(health->Max) * ShopHeal * HealScale(playerId)))));
				loadout.Draughts++;
			}
			else if (ServiceId(item.Id) == ServiceId::Reroll)
				loadout.Rerolls++;
			else
				loadout.Banishes++;
			break;
		default:
			return false;
		}
		loadout.Shards = uint16_t(loadout.Shards - price);
		loadout.Sold |= uint8_t(1u << slot);
		world.RefreshTraits(player);
		m_Revision++;
		return true;
	}

	bool RunDirector::Sell(World& world, uint32_t playerId, uint32_t slot)
	{
		if (playerId >= MaxPlayers || slot >= SigilSlots || !RoomsOn() || m_Map.At(m_Status.Room).Kind != RoomKind::Shop)
			return false;
		RunLoadout& loadout = m_Seats[playerId].Loadout;
		const SigilId sigil = loadout.Sigils[slot];
		if (sigil == SigilId::None)
			return false;
		loadout.Sigils[slot] = SigilId::None;
		loadout.Shards = uint16_t(std::min<uint32_t>(loadout.Shards + uint32_t(float(GetSigil(sigil).Price) * SigilSellShare), MaxShards));
		world.RefreshTraits(world.FindPlayer(playerId));
		m_Revision++;
		return true;
	}

	bool RunDirector::SellCard(World& world, uint32_t playerId, CardId card)
	{
		if (playerId >= MaxPlayers || card >= CardId::Count || !RoomsOn() || m_Map.At(m_Status.Room).Kind != RoomKind::Shop)
			return false;
		RunLoadout& loadout = m_Seats[playerId].Loadout;
		if (!HasCard(loadout.Cards, card))
			return false;
		const CardMask bit = CardBit(card);
		if (HasCard(loadout.Third, card))
			loadout.Third &= ~bit;
		else if (HasCard(loadout.Second, card))
			loadout.Second &= ~bit;
		else
		{
			for (const CardDef& other : Cards)
				if (other.NeedsCard == card && HasCard(loadout.Cards, other.Id))
					return false;
			loadout.Cards &= ~bit;
		}
		loadout.Shards = uint16_t(std::min<uint32_t>(loadout.Shards + CardSellPrice(card), MaxShards));
		world.RefreshTraits(world.FindPlayer(playerId));
		m_Revision++;
		return true;
	}

	bool RunDirector::SwapSigils(World& world, uint32_t playerId, uint32_t from, uint32_t to)
	{
		const RoomKind kind = m_Map.At(m_Status.Room).Kind;
		if (playerId >= MaxPlayers || from >= SigilSlots || to >= SigilSlots || from == to || !RoomsOn() || (kind != RoomKind::Shop && kind != RoomKind::Rest))
			return false;
		RunLoadout& loadout = m_Seats[playerId].Loadout;
		std::swap(loadout.Sigils[from], loadout.Sigils[to]);
		world.RefreshTraits(world.FindPlayer(playerId));
		m_Revision++;
		return true;
	}

	bool RunDirector::UseShrine(World& world, uint32_t playerId, uint32_t choice)
	{
		if (playerId >= MaxPlayers || choice == 0 || choice >= uint32_t(ShrineChoice::Count) || !RoomsOn() || m_Map.At(m_Status.Room).Kind != RoomKind::Shrine)
			return false;
		RunSeat& seat = m_Seats[playerId];
		RunLoadout& loadout = seat.Loadout;
		if (!seat.Present || loadout.Rested != 0)
			return false;
		const Entity player = world.FindPlayer(playerId);
		switch (ShrineChoice(choice))
		{
		case ShrineChoice::Cleanse:
			if (loadout.Curses != 0)
				loadout.Curses = 0;
			else
				loadout.Shards = uint16_t(std::min<uint32_t>(loadout.Shards + CleanseShards, MaxShards));
			break;
		case ShrineChoice::Transform:
		{
			std::vector<CardId> owned;
			for (const CardId id : RunCardPool)
				if (HasCard(loadout.Cards, id) && !HasCard(loadout.Second, id))
					owned.push_back(id);
			if (owned.empty())
				return false;
			const CardId gone = owned[seat.Rng.Below(uint32_t(owned.size()))];
			loadout.Cards &= ~CardBit(gone);
			std::vector<CardId> fresh;
			for (const CardId id : RunCardPool)
				if (id != gone && CardFitsRun(loadout, id) && !HasCard(loadout.Cards, id) && GetCard(id).Rarity == GetCard(gone).Rarity)
					fresh.push_back(id);
			if (fresh.empty())
				for (const CardId id : RunCardPool)
					if (id != gone && CardFitsRun(loadout, id) && !HasCard(loadout.Cards, id))
						fresh.push_back(id);
			if (fresh.empty())
			{
				loadout.Cards |= CardBit(gone);
				return false;
			}
			const CardId gained = fresh[seat.Rng.Below(uint32_t(fresh.size()))];
			loadout.Cards |= CardBit(gained);
			seat.Picks.push_back({ RunCard{ RunCardKind::Card, uint8_t(gained) }, RunStage(m_Status.Act, m_Status.Room) });
			world.Notify(playerId, std::string("The shrine turns ") + GetCard(gone).Name + " into " + GetCard(gained).Name, true);
			break;
		}
		case ShrineChoice::BloodRank:
		{
			const PlayerController* controller = world.GetEntities().TryGet<PlayerController>(player);
			if (!controller)
				return false;
			loadout.Wounds = uint8_t(std::min<uint32_t>(loadout.Wounds + uint32_t(std::lround(float(controller->Traits.MaxHealth) * BloodRankShare)), 200u));
			GrantFocused(world, seat, RunCardKind::Rank);
			break;
		}
		default:
			return false;
		}
		loadout.Rested = uint8_t(choice);
		if (const Transform* transform = world.GetEntities().TryGet<Transform>(player))
			world.SpawnEffect(AbilityId::Ascension, transform->Position, 0.0f, playerId, 0, 1.0f);
		world.RefreshTraits(player);
		if (Health* health = world.GetEntities().TryGet<Health>(player))
			health->Current = std::min(health->Current, health->Max);
		m_Revision++;
		return true;
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
