#include "EnemyAI.h"
#include "GameShared/Content/Definitions.h"
#include "GameShared/Simulation/Abilities.h"
#include "GameShared/Simulation/World.h"
#include "GameShared/World/Collision.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr float RetargetInterval = 0.5f;
		constexpr float DirectChaseRange = 12.0f;
		constexpr float EnemyPortalCooldown = 1.5f;
		constexpr float StuckDistance = 1.0f;
		constexpr float StuckSeconds = 5.0f;
		constexpr float ChillPace = 0.6f;
		constexpr float WakeRange = 11.0f;
		constexpr float WanderRadius = 2.0f;
		constexpr float WanderPace = 0.35f;
		constexpr float MaxBodyPush = 0.25f;
		constexpr float QueueSlack = 3.0f;
		constexpr float LungeSpeed = 16.0f;
		constexpr float KeepSlack = 1.0f;
		constexpr float FleeRange = 9.0f;
		constexpr float HatchSprout = 0.4f;
		constexpr float EggSprout = 0.3f;
		constexpr float EggSpacing = 0.9f;

		struct Body
		{
			Entity Handle;
			Vec2 Position;
			float Radius = 0.0f;
		};

		const Body* FindBody(const std::vector<Body>& bodies, Entity entity)
		{
			const auto it = std::find_if(bodies.begin(), bodies.end(), [&](const Body& body) { return body.Handle == entity; });
			return it != bodies.end() ? &*it : nullptr;
		}

		const Body* FindNearest(const std::vector<Body>& bodies, Vec2 position)
		{
			const Body* nearest = nullptr;
			float nearestDistance = 0.0f;
			for (const Body& body : bodies)
			{
				const float distance = LengthSquared(body.Position - position);
				if (!nearest || distance < nearestDistance)
				{
					nearest = &body;
					nearestDistance = distance;
				}
			}
			return nearest;
		}

		Vec2 Separation(const std::vector<Body>& crowd, const TargetGrid& grid, std::vector<uint32_t>& nearby, Entity self, Vec2 position, float radius)
		{
			const Vec2 reach(radius + 0.05f, radius + 0.05f);
			grid.Gather(position - reach, position + reach, nearby);
			Vec2 push;
			for (const uint32_t index : nearby)
			{
				const Body& other = crowd[index];
				if (other.Handle == self)
					continue;
				const Vec2 offset = position - other.Position;
				const float distance = Length(offset);
				const float minimum = radius + other.Radius + 0.05f;
				if (distance >= minimum)
					continue;
				push += distance > 1e-4f ? offset * ((minimum - distance) * 0.5f / distance) : Vec2(0.01f * float(self.Index % 7), 0.01f);
			}
			return ClampLength(push, 0.1f);
		}

		bool TracksDuringWindup(const EnemyDef& def)
		{
			return GetAbility(def.Attack).Kind == AbilityKind::Ground;
		}

		void ReleaseAttack(World& world, const Transform& transform, const EnemyDef& def, const Enemy& enemy)
		{
			AbilityContext context;
			context.Origin = transform.Position;
			context.Direction = LengthSquared(enemy.AimDirection) > 0.0f ? enemy.AimDirection : FromAngle(transform.Facing);
			context.Target = enemy.AimPoint;
			context.Side = Team::Enemies;
			context.DamageScale = enemy.DamageScale;
			context.ExtraProjectiles = uint8_t(enemy.ExtraProjectiles + def.Volley);
			if (enemy.Elite)
			{
				context.ExtraProjectiles += EliteExtraProjectiles;
				context.AreaScale = EliteAreaScale;
			}
			ExecuteAbility(world, def.Attack, context);
		}

		bool UpdateDormant(World& world, Transform& transform, Enemy& enemy, const EnemyDef& def, float radius, const std::vector<Body>& players, float dt)
		{
			const Level& level = world.GetLevel();
			for (const Body& player : players)
				if (LengthSquared(player.Position - transform.Position) < WakeRange * WakeRange && HasLineOfFire(level, transform.Position, player.Position))
				{
					enemy.Awake = true;
					world.WakePack(enemy.Pack);
					return false;
				}
			enemy.Wander -= dt;
			if (enemy.Wander <= 0.0f)
			{
				Random& random = world.GetRandom();
				enemy.AimPoint = enemy.Home + FromAngle(random.Float() * 2.0f * Pi) * (WanderRadius * random.Float());
				enemy.Wander = 2.5f + 3.0f * random.Float();
			}
			const Vec2 offset = enemy.AimPoint - transform.Position;
			if (LengthSquared(offset) > 0.04f && enemy.Wander < 2.0f)
			{
				const Vec2 direction = Normalize(offset);
				transform.Position = MoveCircle(level, transform.Position, radius, direction * (def.Speed * WanderPace * dt));
				transform.Facing = AngleOf(direction);
			}
			enemy.Anchor = transform.Position;
			enemy.Stuck = 0.0f;
			return true;
		}

		EnemyScaling Inherit(const Enemy& enemy, const Health& health, const EnemyDef& def)
		{
			return { .Health = float(health.Max) / float(std::max(def.Health, 1)), .Damage = enemy.DamageScale, .Speed = enemy.SpeedScale, .Cooldown = enemy.CooldownScale, .Level = enemy.Level };
		}

		struct Birth
		{
			EntityKind Kind = EntityKind::QuartzCrawler;
			Vec2 Position;
			EnemyScaling Scaling;
			float Sprout = 0.0f;
		};

		void Hatch(World& world, Entity entity, const Transform& transform, const Enemy& enemy, const Health& health, const EnemyDef& def, std::vector<Birth>& births)
		{
			births.push_back({ EntityKind::QuartzCrawler, transform.Position, Inherit(enemy, health, def), HatchSprout });
			world.QueueDestroy(entity);
		}

		void LayEggs(World& world, const Transform& transform, const Enemy& enemy, const Health& health, const EnemyDef& def, float radius, std::vector<Birth>& births)
		{
			const Vec2 back = FromAngle(transform.Facing) * -1.0f;
			const Vec2 side = { -back.y, back.x };
			for (const float lean : { -1.0f, 1.0f })
			{
				const Vec2 spot = world.FindFreeSpotNear(transform.Position + back * (radius + 0.45f) + side * (lean * EggSpacing * 0.5f), FindEnemyDef(EntityKind::CrawlerEgg)->Radius);
				births.push_back({ EntityKind::CrawlerEgg, spot, Inherit(enemy, health, def), EggSprout });
			}
		}

		void Flee(World& world, Transform& transform, Enemy& enemy, const EnemyDef& def, float radius, const std::vector<Body>& players, Vec2 push, float dt)
		{
			const Body* threat = FindNearest(players, transform.Position);
			Vec2 desired;
			if (threat && LengthSquared(threat->Position - transform.Position) < FleeRange * FleeRange)
			{
				const FlowField::Step step = world.GetNavigation(radius).GetStep(transform.Position, radius);
				const Vec2 away = Normalize(transform.Position - threat->Position);
				desired = step.Portal < 0 && LengthSquared(step.Direction) > 0.0f ? Normalize(away * 1.4f - step.Direction) : away;
			}
			transform.Position = MoveCircle(world.GetLevel(), transform.Position, radius, desired * (def.Speed * enemy.SpeedScale * dt) + push);
			if (LengthSquared(desired) > 0.0f)
				transform.Facing = AngleOf(desired);
			enemy.Anchor = transform.Position;
			enemy.Stuck = 0.0f;
		}

		void ShieldBroods(World& world)
		{
			EntityRegistry& entities = world.GetEntities();
			std::vector<Vec2> broods;
			entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
			{
				const EnemyDef* def = FindEnemyDef(enemy.Type);
				const Health* health = entities.TryGet<Health>(entity);
				if (def && def->Behavior == EnemyBehavior::Brood && enemy.Sprout <= 0.0f && health && !health->IsDead())
					broods.push_back(entities.TryGet<Transform>(entity)->Position);
			});
			entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
			{
				enemy.Shielded = false;
				const EnemyDef* def = FindEnemyDef(enemy.Type);
				if (broods.empty() || !def || def->Tier == 0 || def->Behavior == EnemyBehavior::Brood || def->Behavior == EnemyBehavior::Treasure || enemy.Sprout > 0.0f)
					return;
				const Vec2 position = entities.TryGet<Transform>(entity)->Position;
				for (const Vec2 brood : broods)
					enemy.Shielded = enemy.Shielded || LengthSquared(brood - position) <= BroodReach * BroodReach;
			});
		}

		bool UsePortal(World& world, Entity entity, Transform& transform, Enemy& enemy, float radius, int32_t index)
		{
			const Portal* portal = world.GetLevel().FindPortal(uint32_t(index));
			if (!portal || enemy.PortalCooldown > 0.0f || LengthSquared(portal->Position - transform.Position) > portal->Radius * portal->Radius)
				return false;
			const Vec2 spread = FromAngle(float(entity.Index % 16) * 2.39996f) * 0.7f;
			transform.Position = world.FindFreeSpotNear(portal->Destination + spread, radius);
			enemy.PortalCooldown = EnemyPortalCooldown;
			enemy.Anchor = transform.Position;
			enemy.Stuck = 0.0f;
			return true;
		}
	}

	void UpdateEnemies(World& world, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		const Level& level = world.GetLevel();

		std::vector<Body> players;
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (controller.State.Alive)
				if (const Transform* transform = entities.TryGet<Transform>(entity))
					players.push_back({ entity, transform->Position, PlayerRadius });
		});
		ShieldBroods(world);
		std::vector<Body> crowd;
		thread_local TargetGrid crowdGrid;
		thread_local std::vector<uint32_t> nearby;
		crowdGrid.Begin(level.Map.GetWidth(), level.Map.GetHeight());
		entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			const Transform* transform = entities.TryGet<Transform>(entity);
			const Collider* collider = entities.TryGet<Collider>(entity);
			if (!transform || !collider || enemy.Sprout > 0.0f)
				return;
			crowd.push_back({ entity, transform->Position, collider->Radius });
			crowdGrid.Add(transform->Position - Vec2(collider->Radius, collider->Radius), transform->Position + Vec2(collider->Radius, collider->Radius));
		});
		crowdGrid.Finish();
		std::vector<Birth> births;

		entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			world.MarkTargetsMoved();
			Transform* transform = entities.TryGet<Transform>(entity);
			const Health* health = entities.TryGet<Health>(entity);
			const EnemyDef* def = FindEnemyDef(enemy.Type);
			if (!transform || !health || !def || health->IsDead())
				return;
			const Collider* collider = entities.TryGet<Collider>(entity);
			const float radius = collider ? collider->Radius : def->Radius;
			if (enemy.Sprout > 0.0f)
			{
				enemy.Sprout = std::max(enemy.Sprout - dt, 0.0f);
				enemy.Anchor = transform->Position;
				return;
			}
			enemy.Age += dt;
			if (def->Behavior == EnemyBehavior::Egg)
			{
				if (enemy.Age >= EggHatchSeconds)
					Hatch(world, entity, *transform, enemy, *health, *def, births);
				return;
			}
			if (def->Behavior == EnemyBehavior::Treasure)
			{
				if (enemy.Age >= BurrowSeconds)
					world.QueueDestroy(entity);
				else
					Flee(world, *transform, enemy, *def, radius, players, Separation(crowd, crowdGrid, nearby, entity, transform->Position, radius), dt);
				return;
			}
			if (def->Behavior == EnemyBehavior::Brood && (enemy.Brood += dt) >= BroodInterval)
			{
				enemy.Brood = 0.0f;
				LayEggs(world, *transform, enemy, *health, *def, radius, births);
			}
			if (!enemy.Awake && UpdateDormant(world, *transform, enemy, *def, radius, players, dt))
				return;
			if (const Boss* boss = entities.TryGet<Boss>(entity))
			{
				Transform body = *transform;
				Enemy mind = enemy;
				Boss state = *boss;
				UpdateBoss(world, entity, body, mind, state, *def, radius, dt);
				*entities.TryGet<Transform>(entity) = body;
				*entities.TryGet<Enemy>(entity) = mind;
				*entities.TryGet<Boss>(entity) = state;
				return;
			}

			enemy.PortalCooldown = std::max(enemy.PortalCooldown - dt, 0.0f);
			enemy.Chill = std::max(enemy.Chill - dt, 0.0f);
			if (enemy.Stun > 0.0f)
			{
				enemy.Stun = std::max(enemy.Stun - dt, 0.0f);
				enemy.Windup = 0.0f;
				enemy.Lunge = 0.0f;
				return;
			}
			if (enemy.Lunge > 0.0f)
			{
				const float step = std::min(enemy.Lunge, LungeSpeed * dt);
				const Vec2 from = transform->Position;
				transform->Position = MoveCircle(level, from, radius, enemy.AimDirection * step);
				enemy.Lunge -= step;
				const Body* prey = FindBody(players, enemy.Target);
				const bool pounce = def->Behavior == EnemyBehavior::Pouncer;
				const bool touching = prey && LengthSquared(prey->Position - transform->Position) <= (radius + prey->Radius + 0.05f) * (radius + prey->Radius + 0.05f);
				if (LengthSquared(transform->Position - from) < step * step * 0.25f || (touching && !pounce))
					enemy.Lunge = 0.0f;
				enemy.Anchor = transform->Position;
				enemy.Stuck = 0.0f;
				if (enemy.Lunge <= 0.0f && !pounce)
					ReleaseAttack(world, *transform, *def, enemy);
				return;
			}
			const float pace = enemy.Chill > 0.0f ? ChillPace : 1.0f;
			enemy.AttackCooldown = std::max(enemy.AttackCooldown - dt * pace, 0.0f);
			enemy.RetargetTimer -= dt;
			const Body* target = FindBody(players, enemy.Target);
			if (!target || enemy.RetargetTimer <= 0.0f)
			{
				target = FindNearest(players, transform->Position);
				enemy.Target = target ? target->Handle : Entity{};
				enemy.RetargetTimer = RetargetInterval;
			}
			if (!target)
			{
				enemy.Windup = 0.0f;
				return;
			}

			const Vec2 toTarget = target->Position - transform->Position;
			const float distance = Length(toTarget);
			if (enemy.Windup > 0.0f)
			{
				if (TracksDuringWindup(*def))
				{
					enemy.AimPoint = target->Position;
					enemy.AimDirection = Normalize(toTarget);
				}
				if (LengthSquared(enemy.AimDirection) > 0.0f)
					transform->Facing = AngleOf(enemy.AimDirection);
				enemy.Windup -= dt * pace;
				if (enemy.Windup <= 0.0f)
				{
					enemy.Windup = 0.0f;
					enemy.AttackCooldown = GetAbility(def->Attack).Cooldown * enemy.CooldownScale;
					if (def->Lunge <= 0.0f || def->Behavior == EnemyBehavior::Pouncer)
						ReleaseAttack(world, *transform, *def, enemy);
					enemy.Lunge = def->Lunge;
				}
				return;
			}

			const bool visible = distance < 20.0f && HasLineOfFire(level, transform->Position, target->Position);
			const bool direct = visible && distance < DirectChaseRange && IsPathClear(level, transform->Position, target->Position, radius);
			Vec2 approach;
			bool arrived = false;
			bool lost = false;
			if (direct)
				approach = Normalize(toTarget);
			else
			{
				const FlowField* field = &world.GetNavigation(radius);
				FlowField::Step step = field->GetStep(transform->Position, radius);
				if (step.Portal < 0 && LengthSquared(step.Direction) == 0.0f && radius > NarrowBodyRadius && !field->IsReachable(transform->Position))
				{
					field = &world.GetNavigation(0.0f);
					step = field->GetStep(transform->Position, radius);
				}
				if (step.Portal >= 0)
				{
					if (UsePortal(world, entity, *transform, enemy, radius, step.Portal))
						return;
					if (const Portal* portal = level.FindPortal(uint32_t(step.Portal)))
						approach = Normalize(portal->Position - transform->Position);
				}
				else if (LengthSquared(step.Direction) > 0.0f)
					approach = step.Direction;
				else if (field->IsReachable(transform->Position))
				{
					arrived = true;
					approach = Normalize(toTarget);
				}
				else
					lost = true;
			}

			const float reach = def->AttackRange + target->Radius;
			if (visible && distance <= reach && enemy.AttackCooldown <= 0.0f)
			{
				enemy.Windup = def->Windup;
				enemy.AimDirection = distance > 1e-4f ? toTarget / distance : FromAngle(transform->Facing);
				enemy.AimPoint = target->Position;
				transform->Facing = AngleOf(enemy.AimDirection);
				return;
			}
			const AbilityKind kind = GetAbility(def->Attack).Kind;
			const bool ranged = kind == AbilityKind::Projectile || kind == AbilityKind::Ground;
			const bool keeps = def->Keep > 0.0f;
			const bool chasing = !visible || distance > (keeps ? def->Keep + KeepSlack : ranged ? reach : reach * 0.8f);
			Vec2 desired;
			if (chasing)
				desired = approach;
			else if (keeps ? distance < def->Keep : ranged && distance < reach * 0.55f)
				desired = -Normalize(toTarget);

			const Vec2 push = Separation(crowd, crowdGrid, nearby, entity, transform->Position, radius);
			transform->Position = MoveCircle(level, transform->Position, radius, desired * (def->Speed * enemy.SpeedScale * pace * dt) + push);
			if (visible)
				transform->Facing = AngleOf(toTarget);
			else if (LengthSquared(desired) > 0.0f)
				transform->Facing = AngleOf(desired);

			const bool queued = world.IsShardfall() && !lost && distance <= reach + QueueSlack;
			if (LengthSquared(transform->Position - enemy.Anchor) > StuckDistance * StuckDistance || (LengthSquared(desired) == 0.0f && !(chasing && lost)) || direct || arrived || queued)
			{
				enemy.Anchor = transform->Position;
				enemy.Stuck = 0.0f;
			}
			else if ((enemy.Stuck += dt) > StuckSeconds)
			{
				if (const std::optional<Vec2> spot = world.FindEnemySpawn(8.0f, radius))
					transform->Position = *spot;
				enemy.Anchor = transform->Position;
				enemy.Stuck = 0.0f;
			}
		});
		for (const Birth& birth : births)
			if (Enemy* born = entities.TryGet<Enemy>(world.SpawnEnemy(birth.Kind, birth.Position, birth.Scaling)))
				born->Sprout = birth.Sprout;

		if (players.empty())
			return;
		entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			Transform* transform = entities.TryGet<Transform>(entity);
			const Collider* collider = entities.TryGet<Collider>(entity);
			const Health* health = entities.TryGet<Health>(entity);
			const Boss* boss = entities.TryGet<Boss>(entity);
			if (!transform || !collider || !health || health->IsDead() || (boss && boss->Burrowed) || enemy.Sprout > 0.0f)
				return;
			Vec2 push;
			for (const Body& player : players)
			{
				const Vec2 offset = transform->Position - player.Position;
				const float minimum = collider->Radius + player.Radius;
				const float distanceSquared = LengthSquared(offset);
				if (distanceSquared >= minimum * minimum)
					continue;
				const float distance = std::sqrt(distanceSquared);
				push += (distance > 1e-4f ? offset / distance : FromAngle(float(entity.Index % 16) * 2.39996f)) * (minimum - distance);
			}
			if (LengthSquared(push) > 0.0f)
				transform->Position = MoveCircle(level, transform->Position, collider->Radius, ClampLength(push, MaxBodyPush));
		});
	}
}
