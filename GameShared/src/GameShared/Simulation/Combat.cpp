#include "Combat.h"
#include "GameShared/Content/Brawl.h"
#include "GameShared/Content/Items.h"
#include "GameShared/Content/Pvp.h"
#include "GameShared/Simulation/World.h"
#include "GameShared/World/Collision.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr float ChainRange = 7.0f;

		constexpr float BossChillScale = 0.5f;

		bool IsVulnerable(const EntityRegistry& entities, Entity entity, const Health& health)
		{
			if (health.IsDead())
				return false;
			if (const Boss* boss = entities.TryGet<Boss>(entity))
				return !boss->Burrowed;
			const PlayerController* controller = entities.TryGet<PlayerController>(entity);
			return !controller || (controller->State.Alive && controller->State.Invulnerable <= 0.0f);
		}

		bool StrikeTouches(const PendingStrike& strike, Vec2 center, float facing, Vec2 position, float radius, Vec2& from)
		{
			const Vec2 offset = position - center;
			from = center;
			switch (strike.Shape)
			{
			case StrikeShape::Line:
			{
				const Vec2 axis = FromAngle(facing);
				const float along = Dot(offset, axis);
				from = center + axis * std::clamp(along, 0.0f, strike.Length);
				return along >= -radius && along <= strike.Length + radius && std::abs(Dot(offset, Perpendicular(axis))) <= strike.Width * 0.5f + radius;
			}
			case StrikeShape::Ring:
			{
				const float distance = Length(offset);
				return distance <= strike.Radius + radius && distance >= strike.Inner - radius;
			}
			case StrikeShape::Cone:
			{
				const float distance = Length(offset);
				return distance <= strike.Length + radius && (distance <= radius + 0.2f || std::abs(AngleBetween(facing, AngleOf(offset))) <= strike.HalfArc);
			}
			default:
				return LengthSquared(offset) <= (strike.Radius + radius) * (strike.Radius + radius);
			}
		}

		float StrikeScale(const AbilityDef& ability, const PendingStrike& strike)
		{
			if (strike.Shape == StrikeShape::Line || strike.Shape == StrikeShape::Cone)
				return ability.Reach > 0.0f ? strike.Length / ability.Reach : 1.0f;
			return ability.ImpactRadius > 0.0f ? strike.Radius / ability.ImpactRadius : 1.0f;
		}

		uint32_t RollAmount(Random& random, float chance)
		{
			uint32_t amount = 0;
			for (; chance >= 1.0f; chance -= 1.0f)
				amount++;
			return amount + (random.Float() < chance ? 1 : 0);
		}

		struct Target
		{
			Entity Handle;
			Vec2 Position;
			float Radius = 0.0f;
			Team Side = Team::Players;
			uint32_t PlayerId = NoPlayer;
		};

		std::vector<Target> CollectTargets(EntityRegistry& entities)
		{
			std::vector<Target> targets;
			entities.Each<Health>([&](Entity entity, Health& health)
			{
				const Transform* transform = entities.TryGet<Transform>(entity);
				const Collider* collider = entities.TryGet<Collider>(entity);
				const PlayerController* controller = entities.TryGet<PlayerController>(entity);
				if (transform && collider && IsVulnerable(entities, entity, health))
					targets.push_back({ entity, transform->Position, collider->Radius, health.Side, controller ? controller->PlayerId : NoPlayer });
			});
			return targets;
		}

		bool IsHostile(const World& world, Team side, uint32_t owner, const Target& target)
		{
			return target.Side != side || world.AreRivals(owner, target.PlayerId);
		}

		bool TargetPosition(const World& world, const Target& target, float rewind, Vec2& position)
		{
			position = target.Position;
			return rewind <= 0.5f || world.GetPastPosition(target.Handle, double(world.GetTick()) - double(rewind), position);
		}

		HitInfo MakeHit(int32_t damage, const AbilityDef& ability, const AbilityContext& context)
		{
			HitInfo hit{ damage, context.Owner, context.Source, context.CritChance, context.CritMultiplier, context.LifeSteal, ability.Damages, ability.Chill, ability.Stun, context.PvpScale, uint8_t(ability.Id) };
			hit.Push = ability.Push;
			hit.PushOrigin = context.Origin;
			hit.PushDirection = context.Direction;
			hit.PushReach = ability.Reach * context.AreaScale;
			hit.Mode = ability.Kind == AbilityKind::Nova ? PushMode::Radial : ability.Shape == StrikeShape::Line ? PushMode::Along : PushMode::Cone;
			return hit;
		}

		HitInfo MakeHit(const Projectile& projectile, uint32_t owner)
		{
			HitInfo hit{ projectile.Damage, owner, projectile.Source, projectile.CritChance, projectile.CritMultiplier, projectile.LifeSteal, projectile.Damages, projectile.Chill, projectile.Stun, projectile.PvpScale, projectile.Ability };
			hit.Push = projectile.Push;
			hit.PushDirection = projectile.Velocity;
			hit.Swap = projectile.Swap;
			return hit;
		}

		Vec2 Scatter(Random& random, Vec2 position)
		{
			return position + FromAngle(random.Float() * 2.0f * Pi) * (0.25f + 0.3f * random.Float());
		}

		void Explode(World& world, const Projectile& projectile, Vec2 position, uint32_t owner, uint16_t tag)
		{
			HitInfo hit = MakeHit(projectile, owner);
			hit.Mode = PushMode::Radial;
			hit.PushOrigin = position;
			hit.PushReach = projectile.ImpactRadius;
			StrikeCircle(world, position, projectile.ImpactRadius, hit, projectile.Side, projectile.Rewind);
			const AbilityDef& ability = GetAbility(AbilityId(projectile.Ability));
			world.SpawnEffect(ability.Id, position, 0.0f, owner, tag, ability.ImpactRadius > 0.0f ? projectile.ImpactRadius / ability.ImpactRadius : 1.0f);
		}

		void ResetHistory(World& world, Entity entity, Vec2 position)
		{
			if (MotionHistory* history = world.GetEntities().TryGet<MotionHistory>(entity))
			{
				history->Positions.fill(position);
				history->FirstTick = world.GetTick() + 1;
			}
		}

		bool SwapPlaces(World& world, Entity caster, Entity target)
		{
			EntityRegistry& entities = world.GetEntities();
			Transform* from = entities.TryGet<Transform>(caster);
			Transform* to = entities.TryGet<Transform>(target);
			PlayerController* casterController = entities.TryGet<PlayerController>(caster);
			PlayerController* targetController = entities.TryGet<PlayerController>(target);
			if (caster == target || !from || !to || !casterController || !targetController || !casterController->State.Alive)
				return false;
			std::swap(from->Position, to->Position);
			for (PlayerController* controller : { casterController, targetController })
			{
				controller->State.Push = {};
				controller->State.DashDistance = 0.0f;
			}
			ResetHistory(world, caster, from->Position);
			ResetHistory(world, target, to->Position);
			world.SpawnEffect(AbilityId::Swap, from->Position, from->Facing, casterController->PlayerId, 0, 1.0f);
			world.SpawnEffect(AbilityId::Swap, to->Position, to->Facing, casterController->PlayerId, 0, 1.0f);
			return true;
		}

		bool ApplyKnockback(World& world, Entity target, const HitInfo& hit)
		{
			EntityRegistry& entities = world.GetEntities();
			PlayerController* controller = entities.TryGet<PlayerController>(target);
			const Transform* transform = entities.TryGet<Transform>(target);
			const Health* health = entities.TryGet<Health>(target);
			if (!controller || !transform || !health || !controller->State.Alive || controller->State.Invulnerable > 0.0f)
				return false;
			if (hit.Attacker != NoPlayer && hit.Attacker != controller->PlayerId)
			{
				controller->LastAttacker = hit.Attacker;
				controller->LastAttackAge = 0.0f;
				controller->LastAbility = hit.Ability;
			}
			if (const Replicated* replicated = entities.TryGet<Replicated>(target))
				world.RecordHit({ replicated->NetId, transform->Position, 0, false, true, hit.Damages });
			if (hit.Swap)
			{
				SwapPlaces(world, hit.Source, target);
				return true;
			}
			if (hit.Push == 0.0f || controller->State.Ward > 0.0f)
				return true;
			const Vec2 offset = transform->Position - hit.PushOrigin;
			const float distance = Length(offset);
			const Vec2 radial = distance > 1e-4f ? offset / distance : Normalize(hit.PushDirection);
			const float fraction = float(std::max(health->Current, 0)) / float(std::max(health->Max, 1));
			Vec2 impulse;
			switch (hit.Mode)
			{
			case PushMode::Pull:
				impulse = radial * -(distance * PushFriction * 0.95f);
				break;
			case PushMode::Radial:
				impulse = radial * (hit.Push * (hit.PushReach > 0.0f ? 1.0f - 0.4f * std::clamp(distance / hit.PushReach, 0.0f, 1.0f) : 1.0f));
				break;
			case PushMode::Cone:
				impulse = Normalize(Normalize(hit.PushDirection) * 0.65f + radial * 0.35f) * hit.Push;
				break;
			default:
				impulse = Normalize(hit.PushDirection) * hit.Push;
				break;
			}
			impulse *= world.GetMatch().GetKnockbackScale(hit.Attacker, controller->PlayerId, hit.Mode == PushMode::Pull ? 1.0f : fraction);
			PlayerState& state = controller->State;
			state.Push = ClampLength(state.Push + impulse, MaxPush);
			if (Length(impulse) >= PushStagger)
			{
				state.CastSlot = -1;
				state.CastTimer = 0.0f;
			}
			return true;
		}

		bool Reflect(World& world, Entity entity, Projectile& projectile, Transform& transform, Entity target, Vec2 point)
		{
			EntityRegistry& entities = world.GetEntities();
			const PlayerController* controller = entities.TryGet<PlayerController>(target);
			if (!controller || !controller->State.Alive || controller->State.Ward <= 0.0f)
				return false;
			const float speed = Length(projectile.Velocity);
			Vec2 heading = Normalize(projectile.Velocity) * -1.0f;
			if (const Transform* caster = entities.TryGet<Transform>(projectile.Source); caster && entities.Has<PlayerController>(projectile.Source) && projectile.Source != target)
				if (const Vec2 toward = caster->Position - point; LengthSquared(toward) > 0.25f)
					heading = Normalize(toward);
			projectile.Velocity = heading * speed;
			projectile.Lifetime = GetProjectileLifetime(GetAbility(AbilityId(projectile.Ability)), speed);
			projectile.Source = target;
			projectile.Rewind = 0.0f;
			projectile.Hits = {};
			projectile.Hits[0] = target;
			projectile.HitCount = 1;
			transform.Position = point;
			transform.Facing = AngleOf(heading);
			if (Replicated* replicated = entities.TryGet<Replicated>(entity))
			{
				replicated->Owner = controller->PlayerId;
				replicated->Tag = 0;
			}
			if (const Replicated* warded = entities.TryGet<Replicated>(target))
				world.RecordHit({ warded->NetId, point, 0, false, true, projectile.Damages });
			return true;
		}

		bool Chain(World& world, const std::vector<Target>& targets, const Projectile& projectile, Entity hit, Vec2 from, uint32_t owner)
		{
			const Target* best = nullptr;
			float bestDistance = ChainRange * ChainRange;
			const auto already = [&](Entity entity) { return entity == hit || std::find(projectile.Hits.begin(), projectile.Hits.begin() + projectile.HitCount, entity) != projectile.Hits.begin() + projectile.HitCount; };
			for (const Target& target : targets)
			{
				const float distance = LengthSquared(target.Position - from);
				if (!IsHostile(world, projectile.Side, owner, target) || already(target.Handle) || distance >= bestDistance || !HasLineOfFire(world.GetLevel(), from, target.Position))
					continue;
				best = &target;
				bestDistance = distance;
			}
			if (!best)
				return false;
			Projectile next = projectile;
			next.Chain--;
			next.Pierce = 0;
			if (next.HitCount < next.Hits.size())
				next.Hits[next.HitCount++] = hit;
			const Vec2 direction = Normalize(best->Position - from);
			next.Velocity = direction * Length(projectile.Velocity);
			world.SpawnProjectileCopy(next, from + direction * 0.3f, owner);
			return true;
		}
	}

	bool ApplyDamage(World& world, Entity target, const DamageEvent& damage)
	{
		EntityRegistry& entities = world.GetEntities();
		Health* health = entities.TryGet<Health>(target);
		PlayerController* controller = entities.TryGet<PlayerController>(target);
		if (!health || damage.Amount <= 0)
			return false;
		if (damage.Unmitigated && controller ? !controller->State.Alive || health->IsDead() : !IsVulnerable(entities, target, *health))
			return false;
		int32_t amount = damage.Amount;
		if (controller && !damage.Unmitigated)
			amount = std::max(1, int32_t(std::lround(float(amount) * DamageTakenScale(controller->Traits, damage.Damages, damage.Pvp ? PvpMaxArmorReduction : MaxArmorReduction))));
		if (controller && damage.Pvp)
			amount = std::min(amount, std::max(1, int32_t(float(controller->Traits.MaxHealth) * PvpMaxHitFraction)));
		if (controller && damage.Attacker != NoPlayer && damage.Attacker != controller->PlayerId)
		{
			controller->LastAttacker = damage.Attacker;
			controller->LastAttackAge = 0.0f;
			controller->LastAbility = damage.Ability;
		}
		const Transform* transform = entities.TryGet<Transform>(target);
		const Replicated* replicated = entities.TryGet<Replicated>(target);
		if (transform && replicated)
			world.RecordHit({ replicated->NetId, transform->Position, uint16_t(std::min(amount, 65535)), damage.Critical, controller != nullptr, damage.Damages });

		health->Current -= amount;
		if (Enemy* enemy = entities.TryGet<Enemy>(target); enemy && !enemy->Awake)
		{
			enemy->Awake = true;
			world.WakePack(enemy->Pack);
		}
		if (health->Current > 0)
			return true;
		health->Current = 0;
		if (controller)
		{
			world.KillPlayer(target, damage.Attacker, damage.Ability);
			return true;
		}
		const uint32_t attacker = damage.Attacker;
		const Enemy* found = entities.TryGet<Enemy>(target);
		world.Emit({ WorldEventType::EnemyKilled, attacker, uint8_t(found ? found->Type : EntityKind::Chaser) });
		if (const EnemyDef* def = found ? FindEnemyDef(found->Type) : nullptr; def && transform)
		{
			const Enemy enemy = *found;
			const Vec2 position = transform->Position;
			DropLoot(world, *def, enemy, position);
			world.OnEnemyKilled(target, enemy, position, attacker);
		}
		world.QueueDestroy(target);
		return true;
	}

	bool DealDamage(World& world, Entity target, const HitInfo& hit)
	{
		EntityRegistry& entities = world.GetEntities();
		const bool pvp = hit.Attacker != NoPlayer && world.IsPvp() && entities.Has<PlayerController>(target);
		const bool critical = hit.CritChance > 0.0f && world.GetRandom().Float() < hit.CritChance;
		const float crit = critical ? (pvp ? 1.0f + (hit.CritMultiplier - 1.0f) * PvpCritScale : hit.CritMultiplier) : 1.0f;
		const int32_t damage = hit.Damage > 0 ? std::max(1, ScaleDamage(hit.Damage, crit * (pvp ? hit.PvpScale : 1.0f))) : 0;
		if (damage <= 0)
		{
			if (!ApplyKnockback(world, target, hit))
				return false;
		}
		else if (!ApplyDamage(world, target, { .Amount = damage, .Attacker = hit.Attacker, .Damages = hit.Damages, .Ability = hit.Ability, .Critical = critical, .Pvp = pvp }))
			return false;
		if (Enemy* enemy = entities.TryGet<Enemy>(target))
		{
			const bool boss = entities.Has<Boss>(target);
			enemy->Chill = std::max(enemy->Chill, hit.Chill * (boss ? BossChillScale : 1.0f));
			if (hit.Stun > 0.0f && !boss)
			{
				enemy->Stun = std::max(enemy->Stun, hit.Stun);
				enemy->Windup = 0.0f;
			}
		}
		else if (PlayerController* controller = entities.TryGet<PlayerController>(target); controller && controller->State.Alive)
		{
			PlayerState& state = controller->State;
			if (hit.Chill > 0.0f)
				state.Chill = std::max(state.Chill, PvpChillDuration(hit.Chill));
			if (hit.Stun > 0.0f && state.StunGuard <= 0.0f)
			{
				state.Stun = PvpStunDuration(hit.Stun);
				state.StunGuard = state.Stun + PvpStunGuard;
				state.CastSlot = -1;
				state.CastTimer = 0.0f;
			}
		}
		if (hit.LifeSteal > 0.0f)
		{
			Health* health = entities.TryGet<Health>(hit.Source);
			const PlayerController* controller = entities.TryGet<PlayerController>(hit.Source);
			if (health && controller && controller->State.Alive)
				health->Current = std::min(health->Max, health->Current + std::max(1, ScaleDamage(damage, hit.LifeSteal * (pvp ? PvpLifeStealScale : 1.0f))));
		}
		return true;
	}

	void StrikeCircle(World& world, Vec2 center, float radius, const HitInfo& hit, Team side, float rewind)
	{
		for (const Target& target : CollectTargets(world.GetEntities()))
		{
			Vec2 position;
			if (!IsHostile(world, side, hit.Attacker, target) || !TargetPosition(world, target, rewind, position))
				continue;
			if (LengthSquared(position - center) > (radius + target.Radius) * (radius + target.Radius))
				continue;
			if (HasLineOfFire(world.GetLevel(), center, position))
				DealDamage(world, target.Handle, hit);
		}
	}

	void StrikeArea(World& world, const AbilityDef& ability, int32_t damage, const AbilityContext& context)
	{
		const bool nova = ability.Kind == AbilityKind::Nova;
		const bool line = !nova && ability.Shape == StrikeShape::Line;
		const float facing = AngleOf(context.Direction);
		const Vec2 axis = FromAngle(facing);
		const float reach = ability.Reach * context.AreaScale;
		const float halfWidth = ability.Width * context.AreaScale * 0.5f;
		const HitInfo hit = MakeHit(damage, ability, context);
		for (const Target& target : CollectTargets(world.GetEntities()))
		{
			Vec2 position;
			if (!IsHostile(world, context.Side, context.Owner, target) || !TargetPosition(world, target, context.Rewind, position))
				continue;
			const Vec2 offset = position - context.Origin;
			const float distance = Length(offset);
			if (distance > reach + target.Radius)
				continue;
			if (line)
			{
				const float along = Dot(offset, axis);
				if (along < -target.Radius || std::abs(Dot(offset, Perpendicular(axis))) > halfWidth + target.Radius)
					continue;
			}
			else if (!nova && distance > target.Radius + 0.2f && std::abs(AngleBetween(facing, AngleOf(offset))) > ability.HalfArc)
				continue;
			if (HasLineOfFire(world.GetLevel(), context.Origin, position))
				DealDamage(world, target.Handle, hit);
		}
	}

	void DropLoot(World& world, const EnemyDef& def, const Enemy& enemy, Vec2 position)
	{
		if (world.IsHub())
			return;
		Random& random = world.GetRandom();
		const uint32_t level = std::clamp<uint32_t>(enemy.Level, 1u, MaxItemLevel);
		const float elite = enemy.Elite ? 1.0f : 0.0f;
		if (world.IsPvp())
		{
			const float weight = def.LootWeight * (enemy.Elite ? 4.0f : 1.0f);
			if (random.Float() < std::min(1.0f, 0.1f * weight))
				world.SpawnItem(GenerateItem(random, world.AllocateItemId(), level, 0.3f * elite), world.PlaceDrop(position, Scatter(random, position)), NoPlayer);
			if (random.Float() < std::min(1.0f, 0.05f * weight))
				world.SpawnItem(GenerateGem(random, world.AllocateItemId(), level), world.PlaceDrop(position, Scatter(random, position)), NoPlayer);
			if (random.Float() < std::min(1.0f, 0.3f + def.OrbChance))
				world.SpawnLoot(random.Below(3) == 0 ? LootKind::Mana : LootKind::Health, world.PlaceDrop(position, Scatter(random, position)));
			return;
		}
		const MissionRewards rewards = world.GetRewards();
		const float quantity = rewards.Quantity;
		const float goldScale = 1.0f + 0.15f * float(level - 1);
		const float itemChance = 0.012f * def.LootWeight * (enemy.Elite ? 10.0f : 1.0f) * quantity;
		const float gemChance = 0.004f * def.LootWeight * (enemy.Elite ? 12.0f : 1.0f) * quantity;
		world.GetEntities().Each<PlayerController>([&](Entity player, PlayerController& controller)
		{
			MaterialStock materials{};
			if (def.LootWeight > 0.0f)
			{
				materials[size_t(Material::Gold)] = uint32_t(std::lround(float(2 + random.Below(4)) * goldScale * (enemy.Elite ? 6.0f : 1.0f) * def.LootWeight * quantity));
				materials[size_t(Material::Shard)] = RollAmount(random, std::lerp(0.06f, 0.5f, elite) * quantity);
				materials[size_t(Material::Starsteel)] = RollAmount(random, std::lerp(0.002f, 0.03f, elite) * quantity);
				materials[size_t(Material::ArcaneDust)] = RollAmount(random, std::lerp(0.04f, 0.35f, elite) * quantity);
				materials[size_t(Material::VoidShard)] = RollAmount(random, std::lerp(0.0005f, 0.01f, elite) * quantity);
			}
			world.GrantMaterials(player, materials);
			for (uint32_t i = RollAmount(random, itemChance); i > 0; i--)
				world.SpawnItem(GenerateItem(random, world.AllocateItemId(), level, rewards.Rarity + 0.5f * elite), world.PlaceDrop(position, Scatter(random, position)), NoPlayer, controller.PlayerId);
			for (uint32_t i = RollAmount(random, gemChance); i > 0; i--)
				world.SpawnItem(GenerateGem(random, world.AllocateItemId(), level), world.PlaceDrop(position, Scatter(random, position)), NoPlayer, controller.PlayerId);
		});
		const float rate = world.GetSettings().LootRate;
		if (random.Float() < std::min(1.0f, def.BuffChance * (enemy.Elite ? 2.0f : 1.0f) * rate))
			world.SpawnLoot(LootKind(1 + random.Below(uint32_t(BuffKinds) - 1)), world.PlaceDrop(position, Scatter(random, position)));
		else if (random.Float() < std::min(1.0f, def.OrbChance * rate))
			world.SpawnLoot(random.Below(2) == 0 ? LootKind::Health : LootKind::Mana, world.PlaceDrop(position, Scatter(random, position)));
	}

	void UpdateProjectiles(World& world, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		const std::vector<Target> targets = CollectTargets(entities);
		struct Contact
		{
			Entity Handle;
			float Parameter = 0.0f;
		};
		std::vector<Contact> contacts;

		entities.Each<Projectile>([&](Entity entity, Projectile& projectile)
		{
			Transform* transform = entities.TryGet<Transform>(entity);
			if (!transform)
				return;
			const Vec2 start = transform->Position;
			const Vec2 end = start + projectile.Velocity * dt;
			const float travel = CastSegment(world.GetLevel(), start, end, Block::Projectiles);
			const Vec2 reached = Lerp(start, end, travel);
			transform->Position = reached;
			const bool clear = travel >= 1.0f;
			projectile.Lifetime -= dt;

			const Replicated* replicated = entities.TryGet<Replicated>(entity);
			const uint32_t owner = replicated ? replicated->Owner : NoPlayer;
			const uint16_t tag = replicated ? replicated->Tag : 0;
			contacts.clear();
			const auto alreadyHit = [&](Entity target)
			{
				return std::find(projectile.Hits.begin(), projectile.Hits.begin() + projectile.HitCount, target) != projectile.Hits.begin() + projectile.HitCount;
			};
			for (const Target& target : targets)
			{
				Vec2 position;
				float parameter = 0.0f;
				if (!IsHostile(world, projectile.Side, owner, target) || alreadyHit(target.Handle) || !TargetPosition(world, target, projectile.Rewind, position))
					continue;
				if (SegmentTouchesCircle(start, reached, position, target.Radius + projectile.Radius, parameter))
					contacts.push_back({ target.Handle, parameter });
			}
			std::sort(contacts.begin(), contacts.end(), [](const Contact& a, const Contact& b) { return a.Parameter < b.Parameter; });

			const HitInfo hit = MakeHit(projectile, owner);
			bool spent = false;
			bool reflected = false;
			for (const Contact& contact : contacts)
			{
				const Vec2 point = Lerp(start, reached, contact.Parameter);
				if (Reflect(world, entity, projectile, *transform, contact.Handle, point))
				{
					reflected = true;
					break;
				}
				if (projectile.ImpactRadius > 0.0f)
				{
					Explode(world, projectile, point, owner, tag);
					spent = true;
					break;
				}
				if (!DealDamage(world, contact.Handle, hit))
					continue;
				if (projectile.Side == Team::Players)
					world.GetStats().PlayerProjectileHits++;
				if (projectile.Chain > 0 && Chain(world, targets, projectile, contact.Handle, point, owner))
				{
					spent = true;
					break;
				}
				if (projectile.HitCount < projectile.Hits.size())
					projectile.Hits[projectile.HitCount++] = contact.Handle;
				if (projectile.Pierce == 0)
				{
					spent = true;
					break;
				}
				projectile.Pierce--;
			}
			if (reflected)
				return;
			if (!spent && projectile.ImpactRadius > 0.0f && (!clear || projectile.Lifetime <= 0.0f))
				Explode(world, projectile, reached, owner, tag);
			if (spent || !clear || projectile.Lifetime <= 0.0f)
				world.QueueDestroy(entity);
		});
	}

	void UpdateStrikes(World& world, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		entities.Each<PendingStrike>([&](Entity entity, PendingStrike& strike)
		{
			strike.Remaining -= dt;
			if (strike.Remaining > 0.0f)
				return;
			if (const Transform* transform = entities.TryGet<Transform>(entity))
			{
				const Vec2 center = transform->Position;
				const float facing = transform->Facing;
				const PendingStrike landed = strike;
				HitInfo hit{ landed.Damage, landed.Owner, landed.Source, landed.CritChance, landed.CritMultiplier, landed.LifeSteal, landed.Damages, landed.Chill, landed.Stun, landed.PvpScale, landed.Ability };
				hit.Push = landed.Push;
				hit.Mode = landed.Push < 0.0f ? PushMode::Pull : PushMode::Radial;
				hit.PushOrigin = center;
				hit.PushDirection = FromAngle(facing);
				hit.PushReach = landed.Radius;
				for (const Target& target : CollectTargets(entities))
				{
					Vec2 from;
					if (IsHostile(world, landed.Side, landed.Owner, target) && StrikeTouches(landed, center, facing, target.Position, target.Radius, from) && HasLineOfFire(world.GetLevel(), from, target.Position))
						DealDamage(world, target.Handle, hit);
				}
				const AbilityDef& ability = GetAbility(AbilityId(landed.Ability));
				const Replicated* replicated = entities.TryGet<Replicated>(entity);
				world.SpawnEffect(ability.Id, center, facing, landed.Owner, replicated ? replicated->Tag : 0, StrikeScale(ability, landed));
			}
			world.QueueDestroy(entity);
		});
	}

	void UpdateEffects(World& world, float dt)
	{
		world.GetEntities().Each<Effect>([&](Entity entity, Effect& effect)
		{
			effect.Remaining -= dt;
			if (effect.Remaining <= 0.0f)
				world.QueueDestroy(entity);
		});
	}

	void UpdateLoot(World& world, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		struct Picker
		{
			Entity Handle;
			Vec2 Position;
			uint32_t PlayerId = 0;
		};
		std::vector<Picker> players;
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (controller.State.Alive)
				if (const Transform* transform = entities.TryGet<Transform>(entity))
					players.push_back({ entity, transform->Position, controller.PlayerId });
		});
		entities.Each<Loot>([&](Entity entity, Loot& loot)
		{
			loot.Lifetime -= dt;
			loot.BlockTimer = std::max(loot.BlockTimer - dt, 0.0f);
			const Transform* transform = entities.TryGet<Transform>(entity);
			if (loot.Lifetime <= 0.0f || !transform)
			{
				world.QueueDestroy(entity);
				return;
			}
			for (const Picker& player : players)
			{
				if (loot.BlockTimer > 0.0f && loot.BlockedPlayer == player.PlayerId)
					continue;
				if (loot.ForPlayer != NoPlayer && loot.ForPlayer != player.PlayerId)
					continue;
				if (LengthSquared(player.Position - transform->Position) > LootPickupRadius * LootPickupRadius || !world.ApplyLoot(player.Handle, loot))
					continue;
				loot.Lifetime = 0.0f;
				world.QueueDestroy(entity);
				return;
			}
		});
	}

	void UpdateRespawns(World& world, float dt)
	{
		world.GetEntities().Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (controller.State.Alive)
				return;
			controller.RespawnTimer = std::max(controller.RespawnTimer - dt, 0.0f);
			if (controller.RespawnTimer <= 0.0f && world.ConsumeRevive())
				world.RespawnPlayer(entity);
		});
	}

	void UpdateRegeneration(World& world, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			controller.LastAttackAge += dt;
			Health* health = entities.TryGet<Health>(entity);
			if (!health || !controller.State.Alive || controller.Traits.Regeneration <= 0.0f || health->Current >= health->Max)
			{
				controller.RegenBank = 0.0f;
				return;
			}
			controller.RegenBank += controller.Traits.Regeneration * dt;
			const int32_t heal = int32_t(controller.RegenBank);
			if (heal <= 0)
				return;
			controller.RegenBank -= float(heal);
			health->Current = std::min(health->Max, health->Current + heal);
		});
	}
}
