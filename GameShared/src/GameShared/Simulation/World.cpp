#include "World.h"
#include "GameShared/Content/Hub.h"
#include "GameShared/Content/Pvp.h"
#include "GameShared/Simulation/Combat.h"
#include "GameShared/Simulation/EnemyAI.h"
#include "GameShared/World/Collision.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float NavigationInterval = 0.25f;
		constexpr float DroppedItemBlockSeconds = 2.0f;
		constexpr float SpotSearchStep = 0.5f;
		constexpr int32_t SpotSearchRings = 40;

		constexpr float StationSlack = 2.5f;
		constexpr float BossHealthPerPlayer = 0.7f;
		constexpr float BodyCollectRange = 4.0f;

		constexpr float DummyShoveOverVoid = 1.5f;
		constexpr float DummyFallSeconds = 1.6f;
		constexpr float DummyFallDrag = 1.1f;
		constexpr float BalloonSpeed = 1.3f;
		constexpr float BalloonFallSeconds = 2.6f;
		constexpr float BalloonFirstDelay = 75.0f;
		constexpr float BalloonInterval = 210.0f;
		constexpr uint32_t BalloonJitter = 150;
		constexpr uint32_t VoidGiftOdds = 12;

		constexpr const char* LandingLines[] = {
			"A gust catches you and sets you back on the island.",
			"The ember winds carry you home. They sound amused.",
			"An updraft lifts you back to the ledge. Emberhold keeps its own.",
			"The void spits you back out.",
			"Wind howls up from below and drops you on solid stone.",
		};

		constexpr const char* Fortunes[] = {
			"The coin tumbles into the clouds. A distant voice says: \"Your next fireball will fly true.\"",
			"The void whispers: \"The lava remembers everyone it has met.\"",
			"Far below, something catches the coin. It says thank you.",
			"The void whispers: \"Duck.\" You are not sure if it means the bird.",
			"The coin falls for a very long time. You stop listening.",
			"The void whispers: \"Stand still and the island will forget you. Keep moving.\"",
			"A warm wind rises: luck favours the one who dodges last.",
			"The void whispers: \"The cat knows a way.\"",
			"Your coin joins a great many others in the dark. Somewhere, a dragon smiles.",
			"The void whispers: \"Every edge is a door, if you are brave or careless.\"",
			"The coin glints once, then is gone. You feel slightly heavier with destiny.",
			"The void whispers: \"Count the stars above the observatory.\"",
			"A faint chime answers from the deep. Good omens for your next round.",
			"The void whispers: \"Not every wall is a wall. Not every floor is a floor.\"",
			"The coin vanishes into the mist. The mist seems pleased.",
			"The void whispers: \"Seek the fallen champion beneath the plaza.\"",
		};

		uint8_t ScaleByte(float scale)
		{
			return uint8_t(std::clamp(std::lround(scale * 16.0f), 1l, 255l));
		}

		void ApplyPenalties(PlayerTraits& traits, ModifierMask mask)
		{
			const PlayerPenalties penalties = GetPlayerPenalties(mask);
			traits.Regeneration *= penalties.Regeneration;
			traits.LifeSteal *= penalties.Regeneration;
			traits.ManaRegen *= penalties.ManaRegen;
			traits.Armor *= penalties.Armor;
			for (float& resist : traits.Resist)
				resist -= penalties.Resist;
		}
	}

	World::World(const WorldSettings& settings)
		: m_Settings(settings),
		  m_Level(BuildLevel(settings.Map, settings.Seed)),
		  m_Director(settings.IntermissionSeconds, settings.FirstWave),
		  m_Random(uint64_t(settings.Seed) * 0x9E3779B97F4A7C15ULL + 1)
	{
		m_Settings.Tier = uint8_t(std::clamp<uint32_t>(m_Settings.Tier, 1u, MaxMissionTier));
		m_Settings.Modifiers &= AllModifiers;
		m_Settings.Rules = SanitizeRules(m_Settings.Rules);
		m_NavGrid.Build(m_Level);
		m_HubSpells.fill(DefaultBrawlSpells);
		if (m_Settings.Mode == GameMode::Mission)
			m_Mission.Begin(*this);
		else if (IsPvp())
			m_Match.Begin(*this);
		else if (IsHub())
		{
			for (uint32_t spot = 0; spot < m_Level.Dummies.size(); spot++)
			{
				if (!m_Level.Dummies[spot].Duck)
					SpawnDummy(spot, m_Random.Below(GoldenDummyOdds) == 0 ? DummyKind::Golden : DummyKind::Straw);
				else if (m_Random.Below(2) == 0)
					SpawnDummy(spot, DummyKind::Duck);
			}
			m_BalloonTimer = BalloonFirstDelay + float(m_Random.Below(BalloonJitter));
			m_BalloonLane = m_Level.DriftLanes.empty() ? 0u : m_Random.Below(uint32_t(m_Level.DriftLanes.size()));
		}
	}

	void World::Step()
	{
		const float dt = GetTickSeconds();
		UpdateNavigation(dt);
		if (m_Settings.Mode != GameMode::Horde || m_Director.GetStatus().Phase != GamePhase::Defeat)
			UpdateRespawns(*this, dt);
		UpdateRegeneration(*this, dt);
		UpdateEnemies(*this, dt);
		UpdateEchoes(dt);
		UpdateProjectiles(*this, dt);
		UpdateStrikes(*this, dt);
		UpdateEffects(*this, dt);
		UpdateLoot(*this, dt);
		if (m_Settings.Mode == GameMode::Horde)
			m_Director.Update(*this, dt);
		else if (m_Settings.Mode == GameMode::Mission)
			m_Mission.Update(*this, dt);
		else if (IsPvp())
			m_Match.Update(*this, dt);
		else if (IsHub())
			UpdateHub(dt);
		if (const ArenaState arena = GetArena(); arena.Plan)
			SinkFallenProps(m_Level, *arena.Plan, arena.Clock);
		FlushDestroyed();
		m_Tick++;
		RecordHistory();
	}

	GameStatus World::GetStatus() const
	{
		GameStatus status = m_Settings.Mode == GameMode::Horde ? m_Director.GetStatus() : GameStatus{};
		status.Mode = m_Settings.Mode;
		status.Map = m_Settings.Map;
		status.Tier = m_Settings.Tier;
		status.Modifiers = m_Settings.Modifiers;
		if (m_Settings.Mode == GameMode::Mission)
			m_Mission.Describe(*this, status);
		else if (IsPvp())
			m_Match.Describe(*this, status);
		else if (IsHub())
			status.HubFalls = uint16_t(std::min<uint32_t>(m_HubFalls, UINT16_MAX));
		return status;
	}

	ArenaState World::GetArena() const
	{
		if (!m_Level.Skyborne || m_Settings.Mode != GameMode::Brawl)
			return {};
		const BrawlStatus& brawl = m_Match.GetBrawl();
		return { &brawl.Plan, brawl.Clock, brawl.Phase == BrawlPhase::Fight };
	}

	bool World::AreRivals(uint32_t a, uint32_t b) const
	{
		if (!IsPvp() || a == b || !m_Match.IsFighting() || !m_Match.IsPresent(a) || !m_Match.IsPresent(b))
			return false;
		const uint8_t team = m_Match.GetTeam(a);
		return team == NoTeam || team != m_Match.GetTeam(b);
	}

	MissionRewards World::GetRewards() const
	{
		MissionRewards rewards = m_Settings.Mode == GameMode::Mission ? GetMissionRewards(m_Settings.Tier, m_Settings.Modifiers) : MissionRewards{};
		rewards.Quantity *= m_Settings.LootRate;
		return rewards;
	}

	PlayerActions World::ApplyPlayerInput(Entity player, const PlayerInput& input)
	{
		Transform* transform = m_Entities.TryGet<Transform>(player);
		PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		if (!transform || !controller)
			return {};
		const PlayerTraits& traits = controller->Traits;
		CollectBodies(transform->Position, m_Bodies);
		const Vec2 origin = transform->Position;
		const PlayerActions actions = SimulatePlayer(m_Level, *transform, controller->State, traits, input, GetTickSeconds(), m_Bodies, GetArena());
		if (actions.Teleported())
			Emit({ WorldEventType::Teleported, controller->PlayerId, uint8_t(actions.Portal) });
		if (actions.Fell && IsHub())
			m_HubFalls++;
		if (actions.Landed && IsHub())
			Notify(controller->PlayerId, LandingLines[m_HubFalls % std::size(LandingLines)], true);
		if (actions.Dodged)
			m_Match.ArmEcho(controller->PlayerId);
		for (uint32_t slot = 0; slot < AbilitySlots && actions.Recast != 0; slot++)
		{
			if (!actions.HasRecast(slot))
				continue;
			if (actions.RecastAbility == AbilityId::SplittingBolt)
				SplitBolts(*this, player, controller->PlayerId);
			else if (actions.RecastAbility == AbilityId::ChainHook)
				ReelIn(*this, player, controller->PlayerId, actions.RecastPoint);
		}
		if (actions.Released == 0)
			return actions;
		m_Match.NoteCast(controller->PlayerId);
		if (IsHub())
			StargazeAt(*controller, origin, actions);
		Health* life = m_Entities.TryGet<Health>(player);
		if (actions.BloodCost > 0.0f && life && life->Current > 1)
		{
			const int32_t price = std::min(life->Current - 1, std::max(1, int32_t(std::lround(actions.BloodCost))));
			life->Current -= price;
			if (const Replicated* replicated = m_Entities.TryGet<Replicated>(player))
				RecordHit({ replicated->NetId, transform->Position, uint16_t(price), false, true, Element::Physical, NoPlayer });
		}
		const float healthFraction = life ? float(std::max(life->Current, 0)) / float(std::max(life->Max, 1)) : 1.0f;

		AbilityContext context;
		context.Origin = transform->Position;
		context.Direction = actions.Direction;
		context.Side = Team::Players;
		context.Owner = controller->PlayerId;
		context.Source = player;
		context.Tag = uint16_t(input.Sequence);
		context.ExtraProjectiles = traits.ExtraProjectiles;
		context.ExtraPierce = traits.ExtraPierce;
		context.AreaScale = traits.AreaScale;
		context.ProjectileSpeedScale = traits.ProjectileSpeedScale;
		context.CritChance = traits.CritChance;
		context.CritMultiplier = traits.CritMultiplier;
		context.LifeSteal = traits.LifeSteal;
		context.Cards = m_Match.GetCards(controller->PlayerId);
		context.Departure = origin;
		if (m_Settings.LagCompensation)
			context.Rewind = float(std::clamp(double(m_Tick) - input.GetViewTime(), 0.0, double(MaxRewindTicks)));
		const bool chamber = HasCard(context.Cards, CardId::EchoChamber) && IsDamageBrawl();
		uint8_t rolled = 0;
		for (uint32_t slot = 0; slot < AbilitySlots; slot++)
		{
			if (!actions.HasReleased(slot))
				continue;
			const SkillTraits& skill = traits.Skills[slot];
			const AbilityDef& ability = GetAbility(skill.Ability);
			context.DamageScale = skill.DamageScale * m_Match.CastDamageScale(controller->PlayerId, healthFraction, actions.ManaBefore, traits.MaxMana, (actions.Overloaded & (1u << slot)) != 0, controller->State.Still);
			if (skill.Ability == AbilityId::ChainHook && IsDamageBrawl())
				context.DamageScale *= (HasCard(context.Cards, CardId::Whiplash) ? WhiplashDamage : 1.0f) * (HasCard(context.Cards, CardId::LongChain) ? LongChainDamage : 1.0f);
			context.AreaScale = traits.AreaScale * skill.AreaScale;
			context.ExtraProjectiles = uint8_t(traits.ExtraProjectiles + skill.ExtraProjectiles);
			context.Target = GetGroundTarget(m_Level, context.Origin, context.Direction, actions.AimDistance, ability.Range);
			context.PvpScale = skill.PvpScale;
			context.RangeScale = skill.RangeScale;
			context.Charge = IsCharged(skill) ? actions.Charge : 0.0f;
			ExecuteAbility(*this, skill.Ability, context);
			if (slot > 0 && ability.Kind != AbilityKind::Dash && ability.Kind != AbilityKind::Ward && m_Match.ConsumeEcho(controller->PlayerId))
				m_Echoes.push_back({ player, skill.Ability, context, EchoDelay });
			if (chamber && EchoesInChamber(skill.Ability))
				m_Echoes.push_back({ player, skill.Ability, context, EchoChamberDelay, 1.0f, true });
			if (slot > 0)
				rolled |= uint8_t(1u << slot);
		}
		for (uint32_t slot = 1; slot < AbilitySlots && rolled != 0; slot++)
			if (rolled & (1u << slot))
				m_Match.RollArcana(*this, controller->PlayerId, int32_t(slot - 1));
		return actions;
	}

	void World::UpdateEchoes(float dt)
	{
		for (size_t i = 0; i < m_Echoes.size();)
		{
			m_Echoes[i].Remaining -= dt;
			if (m_Echoes[i].Remaining > 0.0f)
			{
				i++;
				continue;
			}
			const EchoCast echo = m_Echoes[i];
			m_Echoes.erase(m_Echoes.begin() + ptrdiff_t(i));
			const PlayerController* controller = m_Entities.TryGet<PlayerController>(echo.Caster);
			const Transform* transform = m_Entities.TryGet<Transform>(echo.Caster);
			if (!controller || !transform || !controller->State.Alive)
				continue;
			AbilityContext context = echo.Context;
			context.Origin = transform->Position;
			context.Departure = transform->Position;
			if (echo.Reaim)
				context.Direction = FromAngle(transform->Facing);
			context.Rewind = 0.0f;
			context.Tag = 0;
			context.PushScale *= echo.Strength;
			context.DamageScale *= echo.Strength;
			ExecuteAbility(*this, echo.Ability, context);
		}
	}

	void World::CollectBodies(Vec2 around, std::vector<BodyCircle>& bodies)
	{
		bodies.clear();
		m_Entities.Each<Enemy>([&](Entity entity, Enemy&)
		{
			const Transform* transform = m_Entities.TryGet<Transform>(entity);
			const Collider* collider = m_Entities.TryGet<Collider>(entity);
			const Health* health = m_Entities.TryGet<Health>(entity);
			const Boss* boss = m_Entities.TryGet<Boss>(entity);
			if (!transform || !collider || !health || health->IsDead() || (boss && boss->Burrowed))
				return;
			if (LengthSquared(transform->Position - around) < BodyCollectRange * BodyCollectRange)
				bodies.push_back({ SnapToNetGrid(transform->Position), collider->Radius });
		});
	}

	Entity World::SpawnPlayer(uint32_t playerId, const GearSet& gear)
	{
		Inventory inventory;
		inventory.Gear = gear.Gear;
		inventory.Gems = gear.Gems;
		inventory.Pack = gear.Pack;
		inventory.Stash = gear.Stash;
		inventory.Materials = gear.Materials;
		return SpawnPlayer(playerId, inventory);
	}

	Entity World::SpawnPlayer(uint32_t playerId, const Inventory& source)
	{
		Inventory inventory = source;
		const auto adopt = [&](Item& item)
		{
			if (item.IsValid())
				item.Id = AllocateItemId();
		};
		for (Item& item : inventory.Gear)
			adopt(item);
		for (Item& item : inventory.Gems)
			adopt(item);
		for (Item& item : inventory.Pack)
			adopt(item);
		for (Item& item : inventory.Stash)
			adopt(item);
		if (!inventory.Gear[size_t(GearSlot::Weapon)].IsValid())
			inventory.Gear[size_t(GearSlot::Weapon)] = MakeItem(ItemBase::Wand, AllocateItemId());

		PlayerTraits traits = m_Settings.Mode == GameMode::Brawl ? m_Match.GetBrawlTraits(playerId) : IsHub() ? HubTraits(playerId) : ComputeTraits(inventory.Gear, inventory.Gems, {}, inventory.Upgrades);
		if (m_Settings.Mode == GameMode::Mission)
			ApplyPenalties(traits, m_Settings.Modifiers);
		PlayerController controller{ .PlayerId = playerId, .Traits = traits };
		controller.State.Mana = traits.MaxMana;
		const Vec2 position = IsPvp() ? m_Match.FindSpawn(*this, playerId) : FindSpawnPosition(playerId);
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, position, 0.0f);
		m_Entities.Add<Collider>(entity, PlayerRadius);
		m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = EntityKind::Player, .Owner = playerId });
		m_Entities.Add<Health>(entity, traits.MaxHealth, traits.MaxHealth, Team::Players);
		m_Entities.Add<PlayerController>(entity, controller);
		m_Entities.Add<Inventory>(entity, inventory);
		MotionHistory history;
		history.Positions.fill(position);
		history.FirstTick = m_Tick + 1;
		m_Entities.Add<MotionHistory>(entity, history);
		if (IsPvp())
			m_Match.OnPlayerSpawned(*this, entity, playerId);
		return entity;
	}

	Entity World::SpawnPlayer(uint32_t playerId, StarterKit kit)
	{
		return SpawnPlayer(playerId, MakeStarterGear(kit));
	}

	void World::RespawnPlayer(Entity player)
	{
		Transform* transform = m_Entities.TryGet<Transform>(player);
		PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		Health* health = m_Entities.TryGet<Health>(player);
		if (!transform || !controller || !health)
			return;

		Vec2 position;
		if (IsPvp())
		{
			position = m_Match.FindSpawn(*this, controller->PlayerId);
			if (std::any_of(controller->Buffs.begin(), controller->Buffs.end(), [](uint8_t stacks) { return stacks > 0; }))
			{
				controller->Buffs = {};
				RefreshTraits(player);
			}
		}
		else
		{
			std::optional<Vec2> teammate;
			m_Entities.Each<PlayerController>([&](Entity entity, PlayerController& other)
			{
				if (entity != player && other.State.Alive && !teammate)
					if (const Transform* otherTransform = m_Entities.TryGet<Transform>(entity))
						teammate = otherTransform->Position;
			});
			position = teammate ? FindFreeSpotNear(*teammate, PlayerRadius) : FindSpawnPosition(controller->PlayerId);
		}
		transform->Position = position;
		controller->State = PlayerState{};
		controller->State.Mana = controller->Traits.MaxMana;
		if (IsPvp())
			controller->State.Invulnerable = SpawnProtection;
		controller->RespawnTimer = 0.0f;
		controller->LastAttacker = NoPlayer;
		controller->KilledBy = NoPlayer;
		health->Max = controller->Traits.MaxHealth;
		health->Current = health->Max;
		if (MotionHistory* history = m_Entities.TryGet<MotionHistory>(player))
		{
			history->Positions.fill(position);
			history->FirstTick = m_Tick + 1;
		}
	}

	void World::KillPlayer(Entity player, uint32_t killer, uint8_t ability)
	{
		PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		if (!controller || !controller->State.Alive)
			return;
		const bool hazard = killer == NoPlayer;
		if (IsDamageBrawl())
		{
			uint8_t credited = ability;
			killer = m_Match.CreditKill(controller->PlayerId, credited);
			if (killer != NoPlayer)
				ability = credited;
		}
		else if (killer == NoPlayer && controller->LastAttacker != NoPlayer && controller->LastAttackAge <= KillCreditSeconds)
		{
			killer = controller->LastAttacker;
			ability = controller->LastAbility;
		}
		const uint32_t victim = controller->PlayerId;
		if (killer == victim || (IsPvp() && !m_Match.IsPresent(killer)))
			killer = NoPlayer;
		if (IsDamageBrawl())
			if (const Transform* transform = m_Entities.TryGet<Transform>(player))
				m_Match.NoteDeath(victim, killer, transform->Position, controller->State.Chill > 0.0f);
		controller->State.Alive = false;
		controller->State.DashDistance = 0.0f;
		controller->State.CastSlot = -1;
		controller->State.Stun = 0.0f;
		controller->State.Chill = 0.0f;
		controller->RespawnTimer = IsPvp() ? ArenaRespawnDelay : RespawnDelay;
		controller->LastAttacker = NoPlayer;
		controller->KilledBy = killer;
		if (Health* health = m_Entities.TryGet<Health>(player))
			health->Current = 0;
		Emit({ WorldEventType::PlayerDied, victim, uint8_t(std::min<uint32_t>(killer, 255u)) });
		if (!IsPvp())
			return;
		m_Match.NoteKnockout(*this, victim, killer, hazard);
		m_Kills.push_back({ killer, victim, ability });
		m_Match.OnPlayerKilled(*this, player, victim, killer);
	}

	void World::RemovePlayer(Entity player)
	{
		if (const PlayerController* controller = m_Entities.TryGet<PlayerController>(player); controller && IsPvp())
			m_Match.OnPlayerRemoved(*this, controller->PlayerId);
		if (const PlayerController* controller = m_Entities.TryGet<PlayerController>(player))
			LeaveChess(controller->PlayerId);
		m_Entities.Destroy(player);
	}

	Entity World::FindPlayer(uint32_t playerId) const
	{
		Entity found;
		const_cast<EntityRegistry&>(m_Entities).Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (controller.PlayerId == playerId)
				found = entity;
		});
		return found;
	}

	void World::Spectate(Entity player, int32_t direction)
	{
		if (IsPvp())
			m_Match.Watch(*this, player, direction);
	}

	bool World::JoinMatch(Entity player)
	{
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		return controller && m_Settings.Mode == GameMode::Brawl && m_Match.JoinMatch(*this, player, controller->PlayerId);
	}

	bool World::PickBrawlSpell(Entity player, uint32_t slot, AbilityId spell)
	{
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		if (!controller)
			return false;
		if (IsHub())
		{
			if (controller->PlayerId >= MaxPlayers || !AssignSpell(m_HubSpells[controller->PlayerId], slot, spell))
				return false;
			RefreshTraits(player);
			return true;
		}
		return m_Settings.Mode == GameMode::Brawl && m_Match.PickSpell(*this, player, controller->PlayerId, slot, spell);
	}

	PlayerTraits World::HubTraits(uint32_t playerId) const
	{
		return MakeBrawlTraits({ .Spells = playerId < MaxPlayers ? m_HubSpells[playerId] : DefaultBrawlSpells }, BrawlMode::Damage);
	}

	void World::SpawnDummy(uint32_t spot, DummyKind kind)
	{
		if (spot >= m_Level.Dummies.size())
			return;
		const DummySpot& place = m_Level.Dummies[spot];
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, place.Position, -0.5f * Pi);
		m_Entities.Add<Collider>(entity, DummyRadius);
		m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = EntityKind::Dummy, .Variant = uint8_t(kind) });
		m_Entities.Add<Health>(entity, DummyHealth, DummyHealth, Team::Enemies);
		m_Entities.Add<Dummy>(entity, Dummy{ .Spot = spot, .Kind = kind, .Home = place.Position, .Patrol = place.Patrol });
		MotionHistory history;
		history.Positions.fill(place.Position);
		history.FirstTick = m_Tick + 1;
		m_Entities.Add<MotionHistory>(entity, history);
	}

	void World::OnDummyShoved(const Dummy& dummy, uint32_t attacker)
	{
		if (dummy.Kind != DummyKind::Duck || attacker >= MaxPlayers || (m_DuckFans & (1u << attacker)) != 0)
			return;
		m_DuckFans |= 1u << attacker;
		Notify(attacker, "Quack.", true);
	}

	void World::PopBalloon(Dummy& dummy, uint32_t attacker)
	{
		if (dummy.Fall > 0.0f)
			return;
		dummy.Fall = 1e-4f;
		dummy.LastAttacker = attacker;
		dummy.Push = Normalize(dummy.Patrol - dummy.Home) * BalloonSpeed;
		m_Entities.Each<PlayerController>([&](Entity, PlayerController& controller)
		{
			Notify(controller.PlayerId, controller.PlayerId == attacker ? "Pop! The balloon duck plummets into the clouds, quacking indignantly." : "Somebody popped the balloon duck. A long, fading quack echoes from below.", true);
		});
	}

	void World::LaunchBalloon(uint32_t lane)
	{
		if (m_Level.DriftLanes.empty())
			return;
		const uint32_t lanes = uint32_t(m_Level.DriftLanes.size());
		const DriftLane& drift = m_Level.DriftLanes[lane % lanes];
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, drift.From, AngleOf(drift.To - drift.From));
		m_Entities.Add<Collider>(entity, DummyRadius);
		m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = EntityKind::Dummy, .Variant = uint8_t(DummyKind::Balloon) });
		m_Entities.Add<Health>(entity, DummyHealth, DummyHealth, Team::Enemies);
		m_Entities.Add<Dummy>(entity, Dummy{ .Spot = UINT32_MAX, .Kind = DummyKind::Balloon, .Home = drift.From, .Patrol = drift.To });
		MotionHistory history;
		history.Positions.fill(drift.From);
		history.FirstTick = m_Tick + 1;
		m_Entities.Add<MotionHistory>(entity, history);
		m_BalloonLane = lanes > 1 ? (lane % lanes + 1 + m_Random.Below(lanes - 1)) % lanes : 0u;
		m_BalloonTimer = BalloonInterval + float(m_Random.Below(BalloonJitter));
	}

	void World::UpdateBalloon(float dt)
	{
		if (m_Level.DriftLanes.empty())
			return;
		bool aloft = false;
		std::vector<Entity> gone;
		m_Entities.Each<Dummy>([&](Entity entity, Dummy& dummy)
		{
			Transform* transform = m_Entities.TryGet<Transform>(entity);
			if (dummy.Kind != DummyKind::Balloon || !transform)
				return;
			aloft = true;
			if (dummy.Fall > 0.0f)
			{
				dummy.Fall += dt;
				transform->Position += dummy.Push * dt;
				dummy.Push *= std::exp(-DummyFallDrag * dt);
				if (dummy.Fall >= BalloonFallSeconds)
					gone.push_back(entity);
				return;
			}
			const float length = std::max(Length(dummy.Patrol - dummy.Home), 1.0f);
			dummy.Phase += dt * BalloonSpeed / length;
			if (dummy.Phase >= 1.0f)
			{
				gone.push_back(entity);
				return;
			}
			const Vec2 heading = (dummy.Patrol - dummy.Home) / length;
			const Vec2 side = { -heading.y, heading.x };
			transform->Position = Lerp(dummy.Home, dummy.Patrol, dummy.Phase) + side * (0.6f * std::sin(dummy.Phase * length * 0.35f));
			transform->Facing = AngleOf(heading) + 0.35f * std::sin(dummy.Phase * length * 0.5f);
		});
		for (const Entity entity : gone)
			QueueDestroy(entity);
		if (aloft || (m_BalloonTimer -= dt) > 0.0f)
			return;
		LaunchBalloon(m_BalloonLane);
	}

	bool World::MakeWish(Entity player)
	{
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		if (!controller || !inventory || controller->PlayerId >= MaxPlayers || !controller->State.Alive || controller->State.IsFalling() || !IsNearStation(player, StationKind::WishingVoid))
			return false;
		float& cooldown = m_WishCooldowns[controller->PlayerId];
		if (cooldown > 0.0f)
			return false;
		MaterialStock cost{};
		cost[size_t(Material::Gold)] = WishCost;
		if (!PayFor(*inventory, cost))
		{
			Notify(controller->PlayerId, "You have no gold to offer. The void waits.", false);
			return false;
		}
		cooldown = WishCooldownSeconds;
		Emit({ WorldEventType::MaterialsChanged, controller->PlayerId });
		Notify(controller->PlayerId, Fortunes[m_Random.Below(uint32_t(std::size(Fortunes)))], true);
		if (m_Random.Below(VoidGiftOdds) == 0)
		{
			MaterialStock gift{};
			gift[size_t(Material::Shard) + m_Random.Below(uint32_t(Material::Count) - uint32_t(Material::Shard))] = 1;
			GrantMaterials(player, gift);
			Notify(controller->PlayerId, "Something comes flying back up out of the void and lands at your feet.", true);
		}
		return true;
	}

	bool World::PlayChess(Entity player, Chess::Command command, uint32_t argument, uint32_t revision)
	{
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		if (!IsHub() || !controller || controller->PlayerId >= MaxPlayers)
			return false;
		Chess::Match& match = m_Chess;
		const uint8_t id = uint8_t(controller->PlayerId);
		const bool near = IsNearStation(player, StationKind::Chess);
		const bool playing = match.Result == Chess::Verdict::Playing;
		const int32_t side = match.Seats[size_t(match.Board.ToMove)] == id ? int32_t(match.Board.ToMove) : match.Seats[0] == id ? 0 : match.Seats[1] == id ? 1 : -1;
		switch (command)
		{
		case Chess::Command::Sit:
			if (!near || match.Seats[argument & 1u] != Chess::NoSeat)
				return false;
			match.Seats[argument & 1u] = id;
			if (!playing)
				Chess::ResetBoard(match);
			break;
		case Chess::Command::Stand:
			return LeaveChess(controller->PlayerId);
		case Chess::Command::Move:
		{
			const size_t mover = size_t(match.Board.ToMove);
			if (!near || !playing || revision != match.Revision || match.Seats[mover] != id || match.Seats[1 - mover] == Chess::NoSeat)
				return false;
			if (!Chess::PlayMove(match, uint8_t(argument & 63u), uint8_t((argument >> 6) & 63u), Chess::Piece((argument >> 12) & 7u)))
				return false;
			break;
		}
		case Chess::Command::Resign:
			if (side < 0 || !playing)
				return false;
			match.Result = Chess::Verdict::Resigned;
			match.Winner = uint8_t(1 - side);
			match.DrawOffer = 0;
			break;
		case Chess::Command::Draw:
			if (side < 0 || !playing || match.Seats[size_t(1 - side)] == Chess::NoSeat || (match.DrawOffer & (1u << side)) != 0)
				return false;
			if ((match.DrawOffer & (1u << (1 - side))) != 0 && revision == match.Revision)
			{
				match.Result = Chess::Verdict::Agreed;
				match.DrawOffer = 0;
			}
			else
				match.DrawOffer = uint8_t(match.DrawOffer | (1u << side));
			break;
		case Chess::Command::Rematch:
			if (side < 0 || (playing && !match.Moves.empty() && match.Seats[0] != match.Seats[1]))
				return false;
			std::swap(match.Seats[0], match.Seats[1]);
			Chess::ResetBoard(match);
			break;
		default:
			return false;
		}
		match.Revision++;
		Emit({ WorldEventType::ChessChanged, controller->PlayerId });
		return true;
	}

	bool World::LeaveChess(uint32_t playerId)
	{
		Chess::Match& match = m_Chess;
		if (playerId >= MaxPlayers || !Chess::IsSeated(match, uint8_t(playerId)))
			return false;
		const uint8_t id = uint8_t(playerId);
		const bool alone = match.Seats[0] == id && match.Seats[1] == id;
		if (match.Result == Chess::Verdict::Playing && !match.Moves.empty() && !alone)
		{
			match.Result = Chess::Verdict::Abandoned;
			match.Winner = match.Seats[0] == id ? 1 : 0;
			match.DrawOffer = 0;
		}
		for (uint8_t& seat : match.Seats)
			if (seat == id)
				seat = Chess::NoSeat;
		if (match.Result == Chess::Verdict::Playing && match.Seats[0] == Chess::NoSeat && match.Seats[1] == Chess::NoSeat)
			Chess::ResetBoard(match);
		match.Revision++;
		Emit({ WorldEventType::ChessChanged, playerId });
		return true;
	}

	void World::StargazeAt(const PlayerController& controller, Vec2 origin, const PlayerActions& actions)
	{
		const Landmark* observatory = m_Level.FindLandmark(LandmarkKind::Observatory);
		if (!observatory || controller.PlayerId >= MaxPlayers || LengthSquared(origin - observatory->Position) > observatory->Radius * observatory->Radius)
			return;
		const Vec2 aim = origin + actions.Direction * actions.AimDistance;
		for (uint32_t slot = 0; slot < AbilitySlots; slot++)
		{
			if (!actions.HasReleased(slot))
				continue;
			const AbilityId spell = controller.Traits.Skills[slot].Ability;
			for (size_t i = 0; i < m_Level.Constellations.size() && i < 16; i++)
			{
				const Constellation& constellation = m_Level.Constellations[i];
				const uint16_t bit = uint16_t(1u << i);
				if (constellation.Spell != spell || (m_Starlit[controller.PlayerId] & bit) != 0 || LengthSquared(aim - constellation.Position) > constellation.Radius * constellation.Radius)
					continue;
				m_Starlit[controller.PlayerId] |= bit;
				Notify(controller.PlayerId, std::string("The ") + GetAbility(spell).Name + " constellation flares to life.", true);
			}
		}
	}

	void World::UpdateHub(float dt)
	{
		for (auto& [spot, timer] : m_DummyRespawns)
			if ((timer -= dt) <= 0.0f)
				SpawnDummy(spot, spot < m_Level.Dummies.size() && m_Level.Dummies[spot].Duck ? DummyKind::Duck : m_Random.Below(GoldenDummyOdds) == 0 ? DummyKind::Golden : DummyKind::Straw);
		std::erase_if(m_DummyRespawns, [](const auto& respawn) { return respawn.second <= 0.0f; });
		for (float& cooldown : m_WishCooldowns)
			cooldown = std::max(cooldown - dt, 0.0f);
		UpdateBalloon(dt);

		struct Dunk
		{
			Entity Handle;
			uint32_t Spot = 0;
			uint32_t Attacker = NoPlayer;
			bool Golden = false;
			bool Void = false;
			bool Slain = false;
		};
		std::vector<Dunk> dunks;
		m_Entities.Each<Dummy>([&](Entity entity, Dummy& dummy)
		{
			Transform* transform = m_Entities.TryGet<Transform>(entity);
			Health* health = m_Entities.TryGet<Health>(entity);
			if (!transform || !health || dummy.Kind == DummyKind::Balloon)
				return;
			if (dummy.Fall > 0.0f)
			{
				dummy.Fall += dt;
				const Vec2 drifted = transform->Position + dummy.Push * dt;
				transform->Position = { std::clamp(drifted.x, 0.5f, float(m_Level.Map.GetWidth()) - 0.5f), std::clamp(drifted.y, 0.5f, float(m_Level.Map.GetHeight()) - 0.5f) };
				dummy.Push *= std::exp(-DummyFallDrag * dt);
				if (dummy.Fall >= DummyFallSeconds)
					dunks.push_back({ entity, dummy.Spot, dummy.LastAttacker, dummy.Kind == DummyKind::Golden, true });
				return;
			}
			if (health->Current <= 0)
			{
				dunks.push_back({ entity, dummy.Spot, dummy.LastAttacker, dummy.Kind == DummyKind::Golden, false, !dummy.Burning });
				return;
			}
			dummy.SinceHurt += dt;
			if (LengthSquared(dummy.Push) > 0.0f)
			{
				const bool shoved = m_Level.Floating && LengthSquared(dummy.Push) > DummyShoveOverVoid * DummyShoveOverVoid;
				transform->Position = MoveCircle(m_Level, transform->Position, DummyRadius, dummy.Push * dt, {}, shoved);
				dummy.Push *= std::exp(-PushFriction * dt);
				if (LengthSquared(dummy.Push) < PushStop * PushStop)
					dummy.Push = {};
				dummy.Idle = 0.0f;
				if (IsOverVoid(m_Level, transform->Position))
				{
					dummy.Fall = dt;
					dummy.Burning = false;
					m_HubFalls++;
					return;
				}
			}
			else
				dummy.Idle += dt;
			const bool lava = IsLavaTerrain(m_Level, transform->Position);
			const bool patrols = LengthSquared(dummy.Patrol - dummy.Home) > 0.01f;
			if (dummy.Idle >= DummyRestSeconds && patrols && !lava)
				dummy.Phase += dt * DummyPatrolSpeed / Length(dummy.Patrol - dummy.Home);
			const Vec2 post = patrols ? Lerp(dummy.Home, dummy.Patrol, 0.5f - 0.5f * std::cos(dummy.Phase * Pi)) : dummy.Home;
			const Vec2 toPost = post - transform->Position;
			dummy.Away = LengthSquared(toPost) > 1.0f && dummy.Idle > 0.0f ? dummy.Away + dt : 0.0f;
			if (dummy.Away > DummyStraySeconds)
			{
				transform->Position = post;
				dummy.Away = 0.0f;
				if (MotionHistory* history = m_Entities.TryGet<MotionHistory>(entity))
				{
					history->Positions.fill(post);
					history->FirstTick = m_Tick + 1;
				}
			}
			else if (dummy.Idle >= (dummy.Kind == DummyKind::Duck ? 0.6f : DummyRestSeconds) && LengthSquared(toPost) > 0.0004f && (dummy.Kind == DummyKind::Duck || !lava))
			{
				const float speed = patrols && dummy.Idle >= DummyRestSeconds ? DummyPatrolSpeed : DummyWalkSpeed;
				transform->Position = MoveCircle(m_Level, transform->Position, DummyRadius, ClampLength(toPost, speed * dt));
				transform->Facing = AngleOf(toPost);
			}
			dummy.Burning = lava && dummy.Kind != DummyKind::Duck;
			if (!dummy.Burning)
			{
				dummy.LavaBank = 0.0f;
				if (dummy.SinceHurt < DummyMendDelay)
					return;
				dummy.MendBank += DummyRegenPerSecond * dt;
				const int32_t mend = int32_t(dummy.MendBank);
				dummy.MendBank -= float(mend);
				health->Current = std::min(health->Max, health->Current + mend);
				return;
			}
			dummy.LavaBank += DummyBurnPerSecond * dt;
			const int32_t burn = int32_t(dummy.LavaBank);
			dummy.LavaBank -= float(burn);
			health->Current -= burn;
			if (health->Current <= 0)
				dunks.push_back({ entity, dummy.Spot, dummy.LastAttacker, dummy.Kind == DummyKind::Golden });
		});
		for (const Dunk& dunk : dunks)
		{
			if (dunk.Golden)
				m_Entities.Each<PlayerController>([&](Entity, PlayerController& controller)
				{
					if (dunk.Void)
						Notify(controller.PlayerId, controller.PlayerId == dunk.Attacker ? "You knocked the golden dummy off the island! It only turns up now and then." : "Someone knocked the golden dummy off the island.", true);
					else if (dunk.Slain)
						Notify(controller.PlayerId, controller.PlayerId == dunk.Attacker ? "You brought down the golden dummy! It only turns up now and then." : "Someone brought down the golden dummy.", true);
					else
						Notify(controller.PlayerId, controller.PlayerId == dunk.Attacker ? "You dunked the golden dummy! It only turns up now and then." : "Someone dunked the golden dummy.", true);
				});
			m_DummyRespawns.emplace_back(dunk.Spot, DummyRespawnSeconds);
			QueueDestroy(dunk.Handle);
		}

		const auto shrine = std::find_if(m_Level.Regions.begin(), m_Level.Regions.end(), [](const Region& region) { return region.Name == "Ember Shrine"; });
		if (shrine == m_Level.Regions.end())
			return;
		m_Entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			const Transform* transform = m_Entities.TryGet<Transform>(entity);
			if (!transform || controller.PlayerId >= MaxPlayers || (m_ShrineVisitors & (1u << controller.PlayerId)) != 0 || !shrine->Contains(transform->Position) || controller.State.IsFalling() || controller.State.Airborne || !m_Level.Map.IsWalkable(int32_t(std::floor(transform->Position.x)), int32_t(std::floor(transform->Position.y))))
				return;
			m_ShrineVisitors |= 1u << controller.PlayerId;
			Notify(controller.PlayerId, "You found the Ember Shrine. The first champion of the Caldera watches over it, and very few mages ever find their way in.", true);
		});
	}

	bool World::ChooseCard(Entity player, uint32_t choice)
	{
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		return controller && m_Settings.Mode == GameMode::Brawl && m_Match.ChooseCard(*this, player, controller->PlayerId, choice);
	}

	bool World::GrantCard(Entity player, CardId card)
	{
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		return controller && m_Settings.Mode == GameMode::Brawl && m_Match.GrantCard(*this, player, controller->PlayerId, card);
	}

	bool World::PresentCards(Entity player, const CardOffer& offer)
	{
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		return controller && m_Settings.Mode == GameMode::Brawl && m_Match.PresentCards(controller->PlayerId, offer);
	}

	bool World::SetBrawlReady(Entity player, bool ready)
	{
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		return controller && m_Settings.Mode == GameMode::Brawl && m_Match.SetReady(controller->PlayerId, ready);
	}

	void World::RefreshTraits(Entity player)
	{
		PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		Health* health = m_Entities.TryGet<Health>(player);
		if (!controller || !inventory || !health)
			return;
		const int32_t previousMax = controller->Traits.MaxHealth;
		controller->Traits = m_Settings.Mode == GameMode::Brawl ? m_Match.GetBrawlTraits(controller->PlayerId) : IsHub() ? HubTraits(controller->PlayerId) : ComputeTraits(inventory->Gear, inventory->Gems, controller->Buffs, inventory->Upgrades);
		if (m_Settings.Mode == GameMode::Mission)
			ApplyPenalties(controller->Traits, m_Settings.Modifiers);
		controller->State.Mana = std::min(controller->State.Mana, controller->Traits.MaxMana);
		health->Max = controller->Traits.MaxHealth;
		if (controller->State.Alive)
			health->Current = std::clamp(health->Current + std::max(health->Max - previousMax, 0), 1, health->Max);
	}

	Entity World::SpawnEnemy(EntityKind kind, Vec2 position, const EnemyScaling& scaling)
	{
		const EnemyDef* def = FindEnemyDef(kind);
		if (!def)
			return {};
		const int32_t health = std::max(1, int32_t(std::lround(float(def->Health) * scaling.Health)));
		const float size = scaling.Elite ? EliteSize : 1.0f;
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, position, 0.0f);
		m_Entities.Add<Collider>(entity, def->Radius * size);
		m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = kind, .Scale = ScaleByte(size) });
		m_Entities.Add<Health>(entity, health, health, Team::Enemies);
		m_Entities.Add<Enemy>(entity, Enemy{ .Type = kind, .AttackCooldown = 1.0f, .DamageScale = scaling.Damage, .SpeedScale = scaling.Speed, .CooldownScale = scaling.Cooldown, .Elite = scaling.Elite, .Level = scaling.Level, .ExtraProjectiles = scaling.ExtraProjectiles });
		MotionHistory history;
		history.Positions.fill(position);
		history.FirstTick = m_Tick + 1;
		m_Entities.Add<MotionHistory>(entity, history);
		return entity;
	}

	Entity World::SpawnBoss(EntityKind kind, Vec2 position, uint32_t level)
	{
		const uint32_t players = std::max(CountPlayers(), 1u);
		EnemyScaling scaling = GetMonsterScaling(kind, level, players, false, m_Settings.Modifiers);
		scaling.Health = LevelScale(scaling.Level) * MonsterToughness(scaling.Level) * (1.0f + BossHealthPerPlayer * float(players - 1)) * (HasModifier(m_Settings.Modifiers, MissionModifier::Fortified) ? 1.6f : 1.0f);
		scaling.Speed = HasModifier(m_Settings.Modifiers, MissionModifier::Frenzied) ? 1.1f : 1.0f;
		const Entity entity = SpawnEnemy(kind, position, scaling);
		if (!entity.IsValid())
			return entity;
		Boss boss;
		boss.Arena = m_Level.BossArenaRadius > 0.0f ? m_Level.BossArena : position;
		boss.ArenaRadius = m_Level.BossArenaRadius > 0.0f ? m_Level.BossArenaRadius : 10.0f;
		boss.Level = scaling.Level;
		boss.Goal = position;
		m_Entities.Add<Boss>(entity, boss);
		if (Enemy* enemy = m_Entities.TryGet<Enemy>(entity))
			enemy->AttackCooldown = 0.0f;
		return entity;
	}

	Entity World::SpawnProjectile(const AbilityDef& ability, const ProjectilePattern& pattern, Vec2 position, Vec2 direction, int32_t damage, const AbilityContext& context)
	{
		uint8_t perks = 0;
		if (ability.Id == AbilityId::EmberBolt && HasCard(context.Cards, CardId::SplittingEmber))
			perks |= PerkSplit;
		if (ability.Id == AbilityId::ArcLightning && HasCard(context.Cards, CardId::Pinball))
			perks |= PerkPinball;
		if (ability.Id == AbilityId::FrostSpear && HasCard(context.Cards, CardId::BlackIce))
			perks |= PerkSlick;
		if (ability.Id == AbilityId::FrostSpear && context.Charge >= 0.999f)
			perks |= PerkBurst;
		const bool overcharged = ability.Id == AbilityId::ArcLightning && HasCard(context.Cards, CardId::Overcharge);
		const bool spear = ability.Id == AbilityId::FrostSpear;
		const ChargedShot shot = spear ? SpearShot(context.Charge) : ChargedShot{ .Radius = ability.ProjectileRadius, .Push = ability.Push, .Chill = ability.Chill };
		const float push = ability.Id == AbilityId::ChainHook ? (HasCard(context.Cards, CardId::Whiplash) ? WhiplashPull : HookPull) : shot.Push;
		const float lifetime = (pattern.Speed > 0.0f ? ChargedRange(ability, context.Charge) / pattern.Speed : 0.0f) * context.RangeScale;
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, position, AngleOf(direction));
		m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = EntityKind::Projectile, .Owner = context.Owner, .Tag = context.Tag, .Variant = uint8_t(ability.Id), .Scale = ScaleByte(shot.Size) });
		m_Entities.Add<Projectile>(entity, Projectile{
			.Velocity = direction * pattern.Speed,
			.Lifetime = lifetime,
			.Span = lifetime,
			.Radius = shot.Radius,
			.Damage = spear ? ScaleDamage(damage, shot.DamageScale) : damage,
			.Side = context.Side,
			.Pierce = pattern.Pierce,
			.Chain = uint8_t(ability.Chain + (overcharged ? 1 : 0)),
			.Rewind = context.Rewind,
			.CritChance = context.CritChance,
			.CritMultiplier = context.CritMultiplier,
			.LifeSteal = context.LifeSteal,
			.ImpactRadius = ability.ImpactRadius * context.AreaScale,
			.Chill = shot.Chill + ((perks & PerkSlick) != 0 && IsDamageBrawl() ? BlackIceChill : 0.0f),
			.Stun = ability.Stun,
			.PvpScale = context.PvpScale,
			.Push = push * context.PushScale,
			.Swap = ability.Swap,
			.Damages = ability.Damages,
			.Ability = uint8_t(ability.Id),
			.Source = context.Source,
			.Floor = LaunchFloor(m_Level, context.Origin),
			.Perks = perks,
		});
		if (context.Side == Team::Players)
		{
			m_Stats.PlayerProjectiles++;
			m_Stats.RewindTicks += context.Rewind;
		}
		return entity;
	}

	Entity World::SpawnProjectileCopy(const Projectile& projectile, Vec2 position, uint32_t owner, float scale, uint16_t tag)
	{
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, position, AngleOf(projectile.Velocity));
		m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = EntityKind::Projectile, .Owner = owner, .Tag = tag, .Variant = projectile.Ability, .Scale = ScaleByte(scale) });
		Projectile copy = projectile;
		copy.Floor = m_Level.Relief.IsEmpty() ? FloorAt(m_Level.Map, position, projectile.Floor) : projectile.Floor;
		m_Entities.Add<Projectile>(entity, copy);
		return entity;
	}

	Entity World::SpawnEffect(const AbilityDef& ability, const AbilityContext& context, Vec2 position, uint16_t tag)
	{
		return SpawnEffect(ability.Id, position, AngleOf(context.Direction), context.Owner, tag, context.AreaScale);
	}

	Entity World::SpawnEffect(AbilityId ability, Vec2 position, float facing, uint32_t owner, uint16_t tag, float scale)
	{
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, position, facing);
		m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = EntityKind::Effect, .Owner = owner, .Tag = tag, .Variant = uint8_t(ability), .Scale = ScaleByte(scale) });
		m_Entities.Add<Effect>(entity, GetAbility(ability).EffectDuration);
		return entity;
	}

	Entity World::SpawnPendingStrike(const AbilityDef& ability, Vec2 position, int32_t damage, const AbilityContext& context)
	{
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, position, AngleOf(context.Direction));
		m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = EntityKind::Effect, .Owner = context.Owner, .Tag = context.Tag, .Variant = uint8_t(ability.Id), .Scale = ScaleByte(context.AreaScale) });
		m_Entities.Add<PendingStrike>(entity, PendingStrike{
			.Remaining = ability.ImpactDelay,
			.Radius = ability.ImpactRadius * context.AreaScale,
			.Shape = ability.Shape,
			.Length = ability.Reach * context.AreaScale,
			.Width = ability.Width * context.AreaScale,
			.Inner = ability.InnerRadius * context.AreaScale,
			.HalfArc = ability.HalfArc,
			.Damage = damage,
			.Side = context.Side,
			.Owner = context.Owner,
			.Source = context.Source,
			.Damages = ability.Damages,
			.CritChance = context.CritChance,
			.CritMultiplier = context.CritMultiplier,
			.LifeSteal = context.LifeSteal,
			.Chill = ability.Chill,
			.Stun = ability.Stun,
			.PvpScale = context.PvpScale,
			.Push = ability.Push * context.PushScale,
			.Ability = uint8_t(ability.Id),
			.Perks = uint8_t((ability.Id == AbilityId::Comet && HasCard(context.Cards, CardId::ShatteringImpact) ? PerkShatter : 0)
				| (ability.Id == AbilityId::StormStrike && HasCard(context.Cards, CardId::ForkedLightning) ? PerkFork : 0)
				| (ability.Id == AbilityId::StormStrike && HasCard(context.Cards, CardId::StaticCharge) ? PerkStatic : 0)),
		});
		return entity;
	}

	Entity World::SpawnCardStrike(AbilityId visual, Vec2 position, float facing, float delay, float radius, float push, uint8_t perks, bool visible, const AbilityContext& context)
	{
		const AbilityDef& ability = GetAbility(visual);
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, position, facing);
		if (visible)
		{
			const float base = ability.ImpactRadius > 0.0f ? ability.ImpactRadius : std::max(ability.Reach, 0.1f);
			m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = EntityKind::Effect, .Owner = context.Owner, .Variant = uint8_t(visual), .Scale = ScaleByte(radius / base) });
		}
		const float share = std::abs(ability.Push) > 1e-3f ? std::abs(push) / std::abs(ability.Push) : context.PushScale;
		m_Entities.Add<PendingStrike>(entity, PendingStrike{
			.Remaining = delay,
			.Radius = radius,
			.Shape = StrikeShape::Circle,
			.Damage = IsDamageBrawl() ? int32_t(std::lround(float(ability.BrawlDamage) * share * context.DamageScale)) : 0,
			.Side = context.Side,
			.Owner = context.Owner,
			.Source = context.Source,
			.Damages = ability.Damages,
			.PvpScale = context.PvpScale,
			.Push = push,
			.Ability = uint8_t(visual),
			.Perks = perks,
		});
		return entity;
	}

	Entity World::SpawnLoot(LootKind kind, Vec2 position)
	{
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, position, m_Random.Float() * 2.0f * Pi);
		m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = EntityKind::Loot, .Variant = uint8_t(kind) });
		m_Entities.Add<Loot>(entity, Loot{ .Kind = kind, .Lifetime = LootLifetime });
		return entity;
	}

	Entity World::SpawnItem(const Item& item, Vec2 position, uint32_t blockedPlayer, uint32_t forPlayer)
	{
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, position, m_Random.Float() * 2.0f * Pi);
		m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = EntityKind::Loot, .Owner = blockedPlayer, .Tag = EncodeItemTag(item), .Variant = uint8_t(LootKind::Item), .Visibility = forPlayer });
		m_Entities.Add<Loot>(entity, Loot{
			.Kind = LootKind::Item,
			.Lifetime = ItemLootLifetime,
			.Drop = item,
			.BlockedPlayer = blockedPlayer,
			.BlockTimer = blockedPlayer != NoPlayer ? DroppedItemBlockSeconds : 0.0f,
			.ForPlayer = forPlayer,
		});
		return entity;
	}

	Entity World::SpawnChest(Vec2 position, const Chest& chest)
	{
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, position, m_Random.Float() * 2.0f * Pi);
		m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = EntityKind::Chest, .Tag = uint16_t(chest.Kind) });
		m_Entities.Add<Chest>(entity, chest);
		return entity;
	}

	Entity World::SpawnGateway(Vec2 position)
	{
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, position, 0.0f);
		m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = EntityKind::Gateway });
		m_Entities.Add<Gateway>(entity, Gateway{});
		return entity;
	}

	Item* World::FindFreePackSlot(Inventory& inventory)
	{
		const auto slot = std::find_if(inventory.Pack.begin(), inventory.Pack.end(), [](const Item& item) { return !item.IsValid(); });
		return slot != inventory.Pack.end() ? &*slot : nullptr;
	}

	void World::InventoryChanged(Entity player)
	{
		RefreshTraits(player);
		if (const PlayerController* controller = m_Entities.TryGet<PlayerController>(player))
			Emit({ WorldEventType::InventoryChanged, controller->PlayerId });
	}

	bool World::ApplyLoot(Entity player, const Loot& loot)
	{
		PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		Health* health = m_Entities.TryGet<Health>(player);
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		if (!controller || !health || !inventory || !controller->State.Alive)
			return false;
		switch (loot.Kind)
		{
		case LootKind::Health:
			if (health->Current >= health->Max)
				return false;
			health->Current = std::min(health->Max, health->Current + (IsPvp() ? std::max(HealthOrbHeal, health->Max / 4) : HealthOrbHeal));
			break;
		case LootKind::Mana:
			if (controller->State.Mana >= controller->Traits.MaxMana - 0.5f)
				return false;
			controller->State.Mana = std::min(controller->Traits.MaxMana, controller->State.Mana + ManaOrbRestore);
			break;
		case LootKind::Item:
		{
			Item* slot = FindFreePackSlot(*inventory);
			if (!slot)
				return false;
			*slot = loot.Drop;
			Emit({ WorldEventType::InventoryChanged, controller->PlayerId });
			if (m_Settings.Mode == GameMode::Royale)
				AutoEquip(player, uint32_t(slot - inventory->Pack.data()));
			break;
		}
		default:
		{
			uint8_t& stacks = controller->Buffs[size_t(loot.Kind)];
			if (stacks >= LootDefs[size_t(loot.Kind)].MaxStacks)
				return false;
			stacks++;
			RefreshTraits(player);
			break;
		}
		}
		Emit({ WorldEventType::LootPicked, controller->PlayerId, uint8_t(loot.Kind) });
		return true;
	}

	void World::AutoEquip(Entity player, uint32_t packSlot)
	{
		const Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		if (!inventory || packSlot >= BackpackSize)
			return;
		const Item& item = inventory->Pack[packSlot];
		if (item.IsGem())
		{
			const Item& weapon = inventory->Gear[size_t(GearSlot::Weapon)];
			if (!MeetsRequirement(GetAbility(item.Skill).Needs, weapon.IsValid() ? GetItemBase(weapon.Base).Affinity : 0))
				return;
			for (uint32_t gem = 0; gem < SkillSlots; gem++)
				if (!inventory->Gems[gem].IsValid())
				{
					SlotGem(player, packSlot, gem);
					return;
				}
			return;
		}
		const std::optional<GearSlot> slot = PreferredGearSlot(item, inventory->Gear);
		if (slot && !inventory->Gear[size_t(*slot)].IsValid())
			EquipItemTo(player, packSlot, *slot);
	}

	bool World::EquipItem(Entity player, uint32_t packSlot)
	{
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		if (!inventory || packSlot >= BackpackSize)
			return false;
		const Item& item = inventory->Pack[packSlot];
		if (item.IsGem())
		{
			for (uint32_t gem = 0; gem < SkillSlots; gem++)
				if (!inventory->Gems[gem].IsValid())
					return SlotGem(player, packSlot, gem);
			return SlotGem(player, packSlot, SkillSlots - 1);
		}
		const std::optional<GearSlot> slot = PreferredGearSlot(item, inventory->Gear);
		return slot && EquipItemTo(player, packSlot, *slot);
	}

	bool World::EquipItemTo(Entity player, uint32_t packSlot, GearSlot slot)
	{
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		if (!inventory || packSlot >= BackpackSize || slot >= GearSlot::Count || !CanEquip(inventory->Pack[packSlot], slot))
			return false;
		std::swap(inventory->Gear[size_t(slot)], inventory->Pack[packSlot]);
		InventoryChanged(player);
		return true;
	}

	bool World::UnequipItem(Entity player, GearSlot slot)
	{
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		if (!inventory || slot >= GearSlot::Count || slot == GearSlot::Weapon || !inventory->Gear[size_t(slot)].IsValid())
			return false;
		Item* free = FindFreePackSlot(*inventory);
		if (!free)
			return false;
		std::swap(*free, inventory->Gear[size_t(slot)]);
		InventoryChanged(player);
		return true;
	}

	bool World::DropItem(Entity player, uint32_t packSlot)
	{
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		const Transform* transform = m_Entities.TryGet<Transform>(player);
		if (!inventory || !controller || !transform || packSlot >= BackpackSize || !inventory->Pack[packSlot].IsValid())
			return false;
		SpawnItem(inventory->Pack[packSlot], PlaceDrop(transform->Position, transform->Position + FromAngle(transform->Facing) * 0.7f), controller->PlayerId);
		inventory->Pack[packSlot] = {};
		Emit({ WorldEventType::InventoryChanged, controller->PlayerId });
		return true;
	}

	bool World::SlotGem(Entity player, uint32_t packSlot, uint32_t gemSlot)
	{
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		if (!inventory || !controller || packSlot >= BackpackSize || gemSlot >= SkillSlots || !inventory->Pack[packSlot].IsGem())
			return false;
		std::swap(inventory->Gems[gemSlot], inventory->Pack[packSlot]);
		controller->State.Cooldowns[gemSlot + 1] = 0.0f;
		if (controller->State.CastSlot == int8_t(gemSlot + 1))
			controller->State.CastSlot = -1;
		InventoryChanged(player);
		return true;
	}

	bool World::UnslotGem(Entity player, uint32_t gemSlot)
	{
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		if (!inventory || !controller || gemSlot >= SkillSlots || !inventory->Gems[gemSlot].IsValid())
			return false;
		Item* free = FindFreePackSlot(*inventory);
		if (!free)
			return false;
		std::swap(*free, inventory->Gems[gemSlot]);
		if (controller->State.CastSlot == int8_t(gemSlot + 1))
			controller->State.CastSlot = -1;
		InventoryChanged(player);
		return true;
	}

	Item* World::ResolveItem(Inventory& inventory, ItemRef ref)
	{
		switch (ref.Location)
		{
		case ItemLocation::Pack:
			return ref.Index < BackpackSize ? &inventory.Pack[ref.Index] : nullptr;
		case ItemLocation::Gear:
			return ref.Index < GearSlotCount ? &inventory.Gear[ref.Index] : nullptr;
		case ItemLocation::Gem:
			return ref.Index < SkillSlots ? &inventory.Gems[ref.Index] : nullptr;
		case ItemLocation::Stash:
			return ref.Index < StashSize ? &inventory.Stash[ref.Index] : nullptr;
		default:
			return nullptr;
		}
	}

	bool World::PayFor(Inventory& inventory, const MaterialStock& cost)
	{
		if (m_Settings.UnlimitedMaterials)
			return true;
		if (!CanAfford(inventory.Materials, cost))
			return false;
		Spend(inventory.Materials, cost);
		return true;
	}

	bool World::IsNearStation(Entity player, StationKind kind) const
	{
		const Station* station = m_Level.FindStation(kind);
		const Transform* transform = m_Entities.TryGet<Transform>(player);
		return IsHub() && station && transform && Length(transform->Position - station->Position) <= station->Radius + StationSlack;
	}

	bool World::StashItem(Entity player, uint32_t packSlot, uint32_t stashSlot)
	{
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		if (!inventory || !controller || !IsNearStation(player, StationKind::Stash) || packSlot >= BackpackSize)
			return false;
		if (stashSlot >= StashSize)
		{
			const auto free = std::find_if(inventory->Stash.begin(), inventory->Stash.end(), [](const Item& item) { return !item.IsValid(); });
			if (free == inventory->Stash.end())
				return false;
			stashSlot = uint32_t(free - inventory->Stash.begin());
		}
		if (!inventory->Pack[packSlot].IsValid() && !inventory->Stash[stashSlot].IsValid())
			return false;
		std::swap(inventory->Pack[packSlot], inventory->Stash[stashSlot]);
		Emit({ WorldEventType::StashChanged, controller->PlayerId });
		Emit({ WorldEventType::InventoryChanged, controller->PlayerId });
		return true;
	}

	bool World::TakeFromStash(Entity player, uint32_t stashSlot, uint32_t packSlot)
	{
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		if (!inventory || stashSlot >= StashSize)
			return false;
		if (packSlot >= BackpackSize)
		{
			Item* free = FindFreePackSlot(*inventory);
			if (!free)
				return false;
			packSlot = uint32_t(free - inventory->Pack.data());
		}
		return StashItem(player, packSlot, stashSlot);
	}

	bool World::SalvageItem(Entity player, ItemRef ref)
	{
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		if (!inventory || !controller || m_Settings.Mode == GameMode::Royale)
			return false;
		if (ref.Location != ItemLocation::Pack && !(ref.Location == ItemLocation::Stash && IsNearStation(player, StationKind::Stash)))
			return false;
		Item* item = ResolveItem(*inventory, ref);
		if (!item || !item->IsValid())
			return false;
		const MaterialStock value = GetSalvageValue(*item);
		Notify(controller->PlayerId, "Salvaged " + GetItemName(*item), true);
		*item = {};
		Emit({ ref.Location == ItemLocation::Stash ? WorldEventType::StashChanged : WorldEventType::InventoryChanged, controller->PlayerId });
		GrantMaterials(player, value);
		return true;
	}

	CraftResult World::ForgeItem(Entity player, ItemRef ref, ForgeAction action, uint32_t affix, StatCategory category)
	{
		CraftResult result;
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		if (!inventory || !controller || action >= ForgeAction::Count)
			return result;
		Item* item = ResolveItem(*inventory, ref);
		if (!IsNearStation(player, StationKind::Forge))
			result.Message = "Stand at the forge";
		else if (!item || !item->IsValid() || item->IsGem())
			result.Message = "Place a weapon or armor piece on the anvil";
		else if (const std::optional<std::string> reason = CheckForge(*item, action, affix, category))
			result.Message = *reason;
		else if (!PayFor(*inventory, GetForgeCost(*item, action, affix)))
			result.Message = "Not enough materials";
		else
		{
			result = ApplyForge(m_Random, *item, action, affix, category);
			Emit({ WorldEventType::MaterialsChanged, controller->PlayerId });
			if (ref.Location == ItemLocation::Stash)
				Emit({ WorldEventType::StashChanged, controller->PlayerId });
			else if (ref.Location == ItemLocation::Gear)
				InventoryChanged(player);
			else
				Emit({ WorldEventType::InventoryChanged, controller->PlayerId });
		}
		Notify(controller->PlayerId, result.Message, result.Applied && result.Success);
		return result;
	}

	CraftResult World::CraftGem(Entity player, ItemRef ref, GemAction action)
	{
		CraftResult result;
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		if (!inventory || !controller || action >= GemAction::Count)
			return result;
		Item* gem = ResolveItem(*inventory, ref);
		if (!IsNearStation(player, StationKind::Lapidary))
			result.Message = "Stand at the lapidary";
		else if (!gem || !gem->IsGem())
			result.Message = "Place a skill gem on the altar";
		else if (const std::optional<std::string> reason = CheckGemCraft(*gem, action))
			result.Message = *reason;
		else if (!PayFor(*inventory, GetGemCost(*gem, action)))
			result.Message = "Not enough materials";
		else
		{
			result = ApplyGemCraft(m_Random, *gem, action);
			Emit({ WorldEventType::MaterialsChanged, controller->PlayerId });
			if (ref.Location == ItemLocation::Stash)
				Emit({ WorldEventType::StashChanged, controller->PlayerId });
			else if (ref.Location == ItemLocation::Gem)
			{
				controller->State.Cooldowns[ref.Index + 1] = 0.0f;
				InventoryChanged(player);
			}
			else
				Emit({ WorldEventType::InventoryChanged, controller->PlayerId });
		}
		Notify(controller->PlayerId, result.Message, result.Applied && result.Success);
		return result;
	}

	void World::GrantMaterials(Entity player, const MaterialStock& materials)
	{
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		if (!inventory || !controller || std::all_of(materials.begin(), materials.end(), [](uint32_t amount) { return amount == 0; }))
			return;
		Gain(inventory->Materials, materials);
		Emit({ WorldEventType::MaterialsChanged, controller->PlayerId });
	}

	bool World::OpenChest(Entity player)
	{
		const Transform* transform = m_Entities.TryGet<Transform>(player);
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		if (!transform || !controller || !controller->State.Alive)
			return false;
		const uint16_t bit = uint16_t(1u << (controller->PlayerId % 16));
		Entity found;
		float best = InteractRange * InteractRange;
		m_Entities.Each<Chest>([&](Entity entity, Chest& chest)
		{
			const Transform* chestTransform = m_Entities.TryGet<Transform>(entity);
			if (!chestTransform || (chest.Opened & bit))
				return;
			const float distance = LengthSquared(chestTransform->Position - transform->Position);
			if (distance <= best)
			{
				best = distance;
				found = entity;
			}
		});
		Chest* chest = m_Entities.TryGet<Chest>(found);
		if (!chest)
			return false;
		const Vec2 origin = m_Entities.TryGet<Transform>(found)->Position;
		if (chest->Kind != ChestKind::Boss)
		{
			chest->Opened = 0xFFFF;
			OpenMatchChest(player, *chest, origin);
			return true;
		}
		chest->Opened |= bit;
		for (uint32_t i = 0; i < chest->Items; i++)
		{
			const Vec2 spot = PlaceDrop(origin, origin + FromAngle(m_Random.Float() * 2.0f * Pi) * (1.0f + 0.8f * m_Random.Float()));
			SpawnItem(GenerateItem(m_Random, AllocateItemId(), chest->Level, chest->RarityBonus, Rarity::Rare), spot, NoPlayer, controller->PlayerId);
		}
		MaterialStock materials{};
		materials[size_t(Material::Gold)] = uint32_t(std::lround(150.0f * (1.0f + 0.15f * float(chest->Level - 1)) * chest->Quantity));
		materials[size_t(Material::Shard)] = 2 + m_Random.Below(3);
		materials[size_t(Material::ArcaneDust)] = 1 + m_Random.Below(3);
		materials[size_t(Material::Starsteel)] = m_Random.Below(2);
		GrantMaterials(player, materials);
		Emit({ WorldEventType::ChestOpened, controller->PlayerId });
		return true;
	}

	void World::OpenMatchChest(Entity player, const Chest& chest, Vec2 origin)
	{
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		const Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		if (!controller || !inventory)
			return;
		const uint32_t playerId = controller->PlayerId;
		const Item& weapon = inventory->Gear[size_t(GearSlot::Weapon)];
		const uint8_t affinity = weapon.IsValid() ? GetItemBase(weapon.Base).Affinity : 0;
		const ChestDef& def = GetChestDef(chest.Kind);
		const uint32_t level = std::min(RoyaleItemLevel(m_Match.GetStorm().Stage) + def.LevelBonus + m_Random.Below(3), MaxItemLevel);
		const auto scatter = [&](float reach) { return PlaceDrop(origin, origin + FromAngle(m_Random.Float() * 2.0f * Pi) * (reach + 0.6f * m_Random.Float())); };
		for (uint32_t i = 0; i < def.Items; i++)
			SpawnItem(GenerateItem(m_Random, AllocateItemId(), level, def.RarityBonus, def.Minimum), scatter(1.0f), NoPlayer);
		if (m_Random.Float() < def.GemChance)
		{
			std::vector<AbilityId> gems;
			const bool fitting = m_Random.Float() < 0.7f;
			for (const AbilityDef& ability : Abilities)
				if (ability.Gem && (!fitting || MeetsRequirement(ability.Needs, affinity)))
					gems.push_back(ability.Id);
			const uint32_t gemLevel = std::min(1u + level / 7u + (m_Random.Below(100) < 30 ? 1u : 0u), uint32_t(MaxGemLevel));
			SpawnItem(MakeGem(gems[m_Random.Below(uint32_t(gems.size()))], AllocateItemId(), gemLevel), scatter(1.2f), NoPlayer);
		}
		if (m_Random.Float() < def.OrbChance)
			SpawnLoot(m_Random.Below(4) == 0 ? LootKind(1 + m_Random.Below(uint32_t(BuffKinds) - 1)) : LootKind::Health, scatter(0.8f));
		Emit({ WorldEventType::ChestOpened, playerId, uint8_t(chest.Kind) });
	}

	bool World::IsNearGateway(Entity player) const
	{
		const Transform* transform = m_Entities.TryGet<Transform>(player);
		if (!transform)
			return false;
		bool near = false;
		const_cast<EntityRegistry&>(m_Entities).Each<Gateway>([&](Entity entity, Gateway& gateway)
		{
			if (const Transform* gate = m_Entities.TryGet<Transform>(entity))
				near = near || Length(gate->Position - transform->Position) <= gateway.Radius + InteractRange * 0.5f;
		});
		return near;
	}

	void World::WakePack(uint16_t pack)
	{
		if (pack == 0)
			return;
		m_Entities.Each<Enemy>([&](Entity, Enemy& enemy)
		{
			if (enemy.Pack == pack)
				enemy.Awake = true;
		});
	}

	void World::OnEnemyKilled(Entity, const Enemy& enemy, Vec2 position, uint32_t)
	{
		if (m_Settings.Mode == GameMode::Mission)
			m_Mission.OnKilled(*this, position, IsBossKind(enemy.Type));
	}

	bool World::ConsumeRevive(uint32_t playerId)
	{
		if (IsPvp())
			return m_Match.CanRespawn(playerId);
		return m_Settings.Mode != GameMode::Mission || m_Mission.ConsumeRevive();
	}

	void World::OfferUpgrades()
	{
		m_Entities.Each<Inventory>([&](Entity entity, Inventory& inventory)
		{
			const PlayerController* controller = m_Entities.TryGet<PlayerController>(entity);
			if (!controller)
				return;
			std::vector<UpgradeId> candidates;
			for (const UpgradeDef& upgrade : Upgrades)
				if (inventory.Upgrades[size_t(upgrade.Id)] < upgrade.MaxPicks)
					candidates.push_back(upgrade.Id);
			if (candidates.empty())
				return;
			for (size_t i = candidates.size(); i > 1; i--)
				std::swap(candidates[i - 1], candidates[m_Random.Below(uint32_t(i))]);
			for (size_t i = 0; i < UpgradeChoices; i++)
				inventory.Offer[i] = i < candidates.size() ? candidates[i] : UpgradeId::Count;
			inventory.OfferPending = true;
			Emit({ WorldEventType::UpgradesOffered, controller->PlayerId });
		});
	}

	bool World::ChooseUpgrade(Entity player, uint32_t choice)
	{
		Inventory* inventory = m_Entities.TryGet<Inventory>(player);
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		if (!inventory || !controller || !inventory->OfferPending || choice >= UpgradeChoices || inventory->Offer[choice] == UpgradeId::Count)
			return false;
		const UpgradeDef& upgrade = GetUpgrade(inventory->Offer[choice]);
		uint8_t& stacks = inventory->Upgrades[size_t(upgrade.Id)];
		if (stacks >= upgrade.MaxPicks)
			return false;
		stacks++;
		inventory->OfferPending = false;
		RefreshTraits(player);
		Emit({ WorldEventType::InventoryChanged, controller->PlayerId, uint8_t(upgrade.Id) });
		return true;
	}

	uint32_t World::CountPendingUpgrades() const
	{
		uint32_t pending = 0;
		const_cast<EntityRegistry&>(m_Entities).Each<Inventory>([&](Entity, Inventory& inventory) { pending += inventory.OfferPending ? 1 : 0; });
		return pending;
	}

	uint32_t World::CountPlayers() const
	{
		uint32_t players = 0;
		const_cast<EntityRegistry&>(m_Entities).Each<PlayerController>([&](Entity, PlayerController&) { players++; });
		return players;
	}

	void World::DestroyEntity(Entity entity)
	{
		m_Entities.Destroy(entity);
	}

	void World::ResetRun()
	{
		m_Entities.Each<Enemy>([&](Entity entity, Enemy&) { QueueDestroy(entity); });
		m_Entities.Each<Projectile>([&](Entity entity, Projectile&) { QueueDestroy(entity); });
		m_Entities.Each<Effect>([&](Entity entity, Effect&) { QueueDestroy(entity); });
		m_Entities.Each<PendingStrike>([&](Entity entity, PendingStrike&) { QueueDestroy(entity); });
		m_Entities.Each<Loot>([&](Entity entity, Loot&) { QueueDestroy(entity); });
		m_Entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (Inventory* inventory = m_Entities.TryGet<Inventory>(entity))
			{
				inventory->Upgrades = {};
				inventory->Offer = {};
				inventory->OfferPending = false;
			}
			controller.Buffs = {};
			RefreshTraits(entity);
			Emit({ WorldEventType::InventoryChanged, controller.PlayerId });
			controller.State.Alive = false;
		});
		m_Entities.Each<PlayerController>([&](Entity entity, PlayerController&) { RespawnPlayer(entity); });
	}

	bool World::GetPastPosition(Entity entity, double tick, Vec2& position) const
	{
		const MotionHistory* history = m_Entities.TryGet<MotionHistory>(entity);
		if (!history)
		{
			const Transform* transform = m_Entities.TryGet<Transform>(entity);
			if (!transform)
				return false;
			position = transform->Position;
			return true;
		}
		if (tick < double(history->FirstTick) - 0.5)
			return false;
		const uint32_t oldest = std::max(history->FirstTick, m_Tick >= HistoryLength - 1 ? m_Tick - (HistoryLength - 1) : 0u);
		tick = std::clamp(tick, double(oldest), double(m_Tick));
		const uint32_t base = uint32_t(std::floor(tick));
		const Vec2 from = history->Positions[base % HistoryLength];
		const Vec2 to = base < m_Tick ? history->Positions[(base + 1) % HistoryLength] : from;
		position = Lerp(from, to, float(tick - double(base)));
		return true;
	}

	Vec2 World::FindSpawnPosition(uint32_t playerId) const
	{
		const Vec2 offsets[4] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f } };
		const Vec2 origin = m_Settings.Mode == GameMode::Mission ? m_Level.MissionStart : m_Level.PlayerSpawn;
		return FindFreeSpotNear(origin + offsets[playerId % 4], PlayerRadius);
	}

	Vec2 World::FindFreeSpotNear(Vec2 origin, float radius) const
	{
		std::optional<Vec2> fallback;
		for (int32_t ring = 0; ring <= SpotSearchRings; ring++)
			for (int32_t dy = -ring; dy <= ring; dy++)
				for (int32_t dx = -ring; dx <= ring; dx++)
				{
					if (std::max(std::abs(dx), std::abs(dy)) != ring)
						continue;
					const Vec2 candidate = origin + Vec2(float(dx), float(dy)) * SpotSearchStep;
					if (!IsFree(m_Level, candidate, radius + 0.02f))
						continue;
					if (CastSegment(m_Level, origin, candidate, Block::Movement) >= 1.0f)
						return candidate;
					if (!fallback)
						fallback = candidate;
				}
		return fallback ? *fallback : m_Level.PlayerSpawn;
	}

	Vec2 World::PlaceDrop(Vec2 origin, Vec2 desired) const
	{
		return IsFree(m_Level, desired, 0.2f) && CastSegment(m_Level, origin, desired, Block::Movement) >= 1.0f ? desired : origin;
	}

	std::optional<Vec2> World::FindEnemySpawn(float minDistance, float bodyRadius)
	{
		float totalArea = 0.0f;
		for (const Region& region : m_Level.Regions)
			if (region.EnemySpawns)
				totalArea += region.GetArea();
		if (totalArea <= 0.0f)
			return std::nullopt;

		const FlowField* fields[] = { &GetNavigation(bodyRadius), &m_Navigation };
		for (const FlowField* navigation : fields)
		{
			std::optional<Vec2> farthest;
			float farthestDistance = -1.0f;
			for (int32_t attempt = 0; attempt < 96; attempt++)
			{
				float pick = m_Random.Float() * totalArea;
				const Region* chosen = nullptr;
				for (const Region& region : m_Level.Regions)
				{
					if (!region.EnemySpawns)
						continue;
					chosen = &region;
					pick -= region.GetArea();
					if (pick <= 0.0f)
						break;
				}
				const Vec2 position = { chosen->Min.x + m_Random.Float() * (chosen->Max.x - chosen->Min.x), chosen->Min.y + m_Random.Float() * (chosen->Max.y - chosen->Min.y) };
				if (!IsFree(m_Level, position, bodyRadius + 0.05f))
					continue;
				const float distance = navigation->GetTravelDistance(position);
				if (!std::isfinite(distance))
					continue;
				if (distance >= minDistance)
					return position;
				if (distance > farthestDistance)
				{
					farthestDistance = distance;
					farthest = position;
				}
			}
			if (farthest || fields[0] == fields[1])
				return farthest;
		}
		return std::nullopt;
	}

	void World::UpdateNavigation(float dt)
	{
		m_NavigationTimer -= dt;
		if (m_NavigationTimer > 0.0f)
			return;
		if (IsPvp())
		{
			bool hunting = false;
			m_Entities.Each<Enemy>([&](Entity, Enemy& enemy) { hunting = hunting || enemy.Awake; });
			if (!hunting)
				return;
		}
		m_NavigationTimer = NavigationInterval;
		std::vector<Vec2> targets;
		m_Entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (controller.State.Alive)
				if (const Transform* transform = m_Entities.TryGet<Transform>(entity))
					targets.push_back(transform->Position);
		});
		m_Navigation.Build(m_Level, m_NavGrid, NarrowBodyRadius, targets);
		m_WideNavigation.Build(m_Level, m_NavGrid, WideBodyRadius, targets);
	}

	void World::RecordHistory()
	{
		m_Entities.Each<MotionHistory>([&](Entity entity, MotionHistory& history)
		{
			if (const Transform* transform = m_Entities.TryGet<Transform>(entity))
				history.Positions[m_Tick % HistoryLength] = transform->Position;
		});
	}

	void World::FlushDestroyed()
	{
		for (Entity entity : m_Destroyed)
			m_Entities.Destroy(entity);
		m_Destroyed.clear();
	}
}
