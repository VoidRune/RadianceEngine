#include "World.h"
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
		if (m_Settings.Mode == GameMode::Mission)
			m_Mission.Begin(*this);
		else if (IsPvp())
			m_Match.Begin(*this);
	}

	void World::Step()
	{
		const float dt = GetTickSeconds();
		UpdateNavigation(dt);
		if (m_Settings.Mode != GameMode::Horde || m_Director.GetStatus().Phase != GamePhase::Defeat)
			UpdateRespawns(*this, dt);
		UpdateRegeneration(*this, dt);
		UpdateEnemies(*this, dt);
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
		return status;
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
		const PlayerActions actions = SimulatePlayer(m_Level, *transform, controller->State, traits, input, GetTickSeconds(), m_Bodies);
		if (actions.Teleported())
			Emit({ WorldEventType::Teleported, controller->PlayerId, uint8_t(actions.Portal) });
		if (actions.Released == 0)
			return actions;

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
		if (m_Settings.LagCompensation)
			context.Rewind = float(std::clamp(double(m_Tick) - input.GetViewTime(), 0.0, double(MaxRewindTicks)));
		for (uint32_t slot = 0; slot < AbilitySlots; slot++)
		{
			if (!actions.HasReleased(slot))
				continue;
			const SkillTraits& skill = traits.Skills[slot];
			const AbilityDef& ability = GetAbility(skill.Ability);
			context.DamageScale = skill.DamageScale;
			context.AreaScale = traits.AreaScale * skill.AreaScale;
			context.ExtraProjectiles = uint8_t(traits.ExtraProjectiles + skill.ExtraProjectiles);
			context.Target = GetGroundTarget(m_Level, context.Origin, context.Direction, actions.AimDistance, ability.Range);
			context.PvpScale = skill.PvpScale;
			ExecuteAbility(*this, skill.Ability, context);
		}
		return actions;
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

		PlayerTraits traits = m_Settings.Mode == GameMode::Brawl ? m_Match.GetBrawlTraits(playerId) : ComputeTraits(inventory.Gear, inventory.Gems, {}, inventory.Upgrades);
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
		if (killer == NoPlayer && controller->LastAttacker != NoPlayer && controller->LastAttackAge <= KillCreditSeconds)
		{
			killer = controller->LastAttacker;
			ability = controller->LastAbility;
		}
		const uint32_t victim = controller->PlayerId;
		if (killer == victim || (IsPvp() && !m_Match.IsPresent(killer)))
			killer = NoPlayer;
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
		m_Kills.push_back({ killer, victim, ability });
		m_Match.OnPlayerKilled(*this, player, victim, killer);
	}

	void World::RemovePlayer(Entity player)
	{
		if (const PlayerController* controller = m_Entities.TryGet<PlayerController>(player); controller && IsPvp())
			m_Match.OnPlayerRemoved(*this, controller->PlayerId);
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

	bool World::PickBrawlSpell(Entity player, uint32_t slot, AbilityId spell)
	{
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		return controller && m_Settings.Mode == GameMode::Brawl && m_Match.PickSpell(*this, player, controller->PlayerId, slot, spell);
	}

	bool World::ChooseBoon(Entity player, uint32_t choice)
	{
		const PlayerController* controller = m_Entities.TryGet<PlayerController>(player);
		return controller && m_Settings.Mode == GameMode::Brawl && m_Match.ChooseBoon(*this, player, controller->PlayerId, choice);
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
		controller->Traits = m_Settings.Mode == GameMode::Brawl ? m_Match.GetBrawlTraits(controller->PlayerId) : ComputeTraits(inventory->Gear, inventory->Gems, controller->Buffs, inventory->Upgrades);
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
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, position, AngleOf(direction));
		m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = EntityKind::Projectile, .Owner = context.Owner, .Tag = context.Tag, .Variant = uint8_t(ability.Id) });
		m_Entities.Add<Projectile>(entity, Projectile{
			.Velocity = direction * pattern.Speed,
			.Lifetime = GetProjectileLifetime(ability, pattern.Speed),
			.Radius = ability.ProjectileRadius,
			.Damage = damage,
			.Side = context.Side,
			.Pierce = pattern.Pierce,
			.Chain = ability.Chain,
			.Rewind = context.Rewind,
			.CritChance = context.CritChance,
			.CritMultiplier = context.CritMultiplier,
			.LifeSteal = context.LifeSteal,
			.ImpactRadius = ability.ImpactRadius * context.AreaScale,
			.Chill = ability.Chill,
			.Stun = ability.Stun,
			.PvpScale = context.PvpScale,
			.Push = ability.Push,
			.Swap = ability.Swap,
			.Damages = ability.Damages,
			.Ability = uint8_t(ability.Id),
			.Source = context.Source,
		});
		if (context.Side == Team::Players)
		{
			m_Stats.PlayerProjectiles++;
			m_Stats.RewindTicks += context.Rewind;
		}
		return entity;
	}

	Entity World::SpawnProjectileCopy(const Projectile& projectile, Vec2 position, uint32_t owner)
	{
		const Entity entity = m_Entities.Create();
		m_Entities.Add<Transform>(entity, position, AngleOf(projectile.Velocity));
		m_Entities.Add<Replicated>(entity, Replicated{ .NetId = AllocateNetId(), .Kind = EntityKind::Projectile, .Owner = owner, .Variant = projectile.Ability });
		m_Entities.Add<Projectile>(entity, projectile);
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
			.Push = ability.Push,
			.Ability = uint8_t(ability.Id),
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

	bool World::ConsumeRevive()
	{
		if (IsPvp())
			return m_Match.CanRespawn();
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
