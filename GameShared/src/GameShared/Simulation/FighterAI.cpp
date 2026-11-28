#include "FighterAI.h"
#include "GameShared/Simulation/Abilities.h"
#include "GameShared/Simulation/Combat.h"
#include "GameShared/Simulation/World.h"
#include "GameShared/World/Collision.h"
#include "GameShared/World/Shapes.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float RetargetSeconds = 0.5f;
		constexpr float DirectRange = 14.0f;
		constexpr float ChillPace = 0.6f;
		constexpr float VelocityTicks = 12.0f;
		constexpr float ThreatMargin = 0.15f;
		constexpr float LungeStandoff = 0.25f;
		constexpr float MinLunge = 0.3f;
		constexpr float StrafeWeight = 0.75f;
		constexpr float EliteArea = 1.15f;
		constexpr float FighterSpacing = 2.4f;
		constexpr float SpacingWeight = 1.6f;

		enum Reaction : uint8_t
		{
			Undecided,
			Guarded,
			Dodged,
			Ignored,
		};

		struct Threat
		{
			Vec2 Escape;
			Vec2 Incoming;
			float Eta = 1e9f;
			bool Area = false;
		};

		void Cool(float& timer, float dt)
		{
			timer = std::max(timer - dt, 0.0f);
		}

		const FighterTarget* FindTarget(const FighterScene& scene, Entity handle)
		{
			const auto it = std::find_if(scene.Targets.begin(), scene.Targets.end(), [&](const FighterTarget& target) { return target.Handle == handle; });
			return it != scene.Targets.end() ? &*it : nullptr;
		}

		const FighterTarget* Nearest(const FighterScene& scene, Vec2 position)
		{
			const FighterTarget* best = nullptr;
			float bestDistance = 0.0f;
			for (const FighterTarget& target : scene.Targets)
				if (const float distance = LengthSquared(target.Position - position); !best || distance < bestDistance)
				{
					best = &target;
					bestDistance = distance;
				}
			return best;
		}

		bool Covers(const PendingStrike& strike, Vec2 center, float facing, Vec2 position, float radius)
		{
			const Vec2 offset = position - center;
			switch (strike.Shape)
			{
			case StrikeShape::Line:
			{
				const Vec2 axis = FromAngle(facing);
				const float along = Dot(offset, axis);
				return along >= -radius && along <= strike.Length + radius && std::abs(Dot(offset, Perpendicular(axis))) <= strike.Width * 0.5f + radius;
			}
			case StrikeShape::Cone:
				return LengthSquared(offset) <= (strike.Length + radius) * (strike.Length + radius);
			default:
				return LengthSquared(offset) <= (strike.Radius + radius) * (strike.Radius + radius);
			}
		}

		Threat Assess(const FighterScene& scene, Vec2 position, float radius, bool mirrored)
		{
			Threat threat;
			for (const FighterThreat& seen : scene.Threats)
			{
				if (mirrored && !seen.Area)
					continue;
				if (seen.Area)
				{
					if (seen.Remaining > FighterThreatWindow || seen.Remaining >= threat.Eta || LengthSquared(position - seen.Position) > (seen.Radius + radius + ThreatMargin) * (seen.Radius + radius + ThreatMargin))
						continue;
					const Vec2 away = position - seen.Position;
					threat = { LengthSquared(away) > 1e-4f ? Normalize(away) : FromAngle(seen.Remaining * 7.0f), Normalize(away), seen.Remaining, true };
					continue;
				}
				const float speed = LengthSquared(seen.Velocity);
				if (speed < 1e-4f)
					continue;
				const Vec2 relative = position - seen.Position;
				const float eta = Dot(relative, seen.Velocity) / speed;
				if (eta < 0.0f || eta > FighterThreatWindow || eta >= threat.Eta)
					continue;
				const Vec2 miss = relative - seen.Velocity * eta;
				if (LengthSquared(miss) > (radius + seen.Radius + ThreatMargin) * (radius + seen.Radius + ThreatMargin))
					continue;
				const Vec2 heading = seen.Velocity / std::sqrt(speed);
				threat = { LengthSquared(miss) > 1e-4f ? Normalize(miss) : Perpendicular(heading), heading * -1.0f, eta, false };
			}
			return threat;
		}

		void CancelMove(Enemy& enemy)
		{
			enemy.Move = -1;
			enemy.Windup = 0.0f;
			enemy.DashLeft = 0.0f;
		}

		Vec2 Probe(const Level& level, Vec2 position, float radius, Vec2 direction, float distance)
		{
			return MoveCircle(level, position, radius, direction * distance);
		}

		void StartDash(Enemy& enemy, Vec2 direction, float distance, float speed)
		{
			enemy.DashDirection = direction;
			enemy.DashLeft = distance;
			enemy.DashSpeed = speed;
		}

		void Dodge(World& world, Transform& transform, Enemy& enemy, const FighterKit& kit, float radius, Vec2 preferred)
		{
			const Level& level = world.GetLevel();
			Vec2 direction = preferred;
			if (LengthSquared(Probe(level, transform.Position, radius, -preferred, kit.DodgeDistance) - transform.Position) > LengthSquared(Probe(level, transform.Position, radius, preferred, kit.DodgeDistance) - transform.Position) * 1.4f)
				direction = -preferred;
			CancelMove(enemy);
			enemy.GuardUp = false;
			StartDash(enemy, direction, kit.DodgeDistance, kit.DodgeDistance / kit.DodgeSeconds);
			enemy.Evade = kit.DodgeSeconds + 0.05f;
			enemy.DodgeCooldown = kit.DodgeCooldown;
		}

		void Blink(World& world, Transform& transform, Enemy& enemy, const FighterKit& kit, float radius, const FighterScene& scene)
		{
			const Level& level = world.GetLevel();
			const FlowField& field = world.GetNavigation(radius);
			const float turn = world.GetRandom().Float() * 2.0f * Pi;
			Vec2 best = transform.Position;
			float bestScore = -1e9f;
			for (uint32_t i = 0; i < 12; i++)
			{
				const Vec2 candidate = transform.Position + FromAngle(turn + float(i) * (2.0f * Pi / 12.0f)) * kit.BlinkDistance;
				const Vec2 spot = world.FindFreeSpotNear(candidate, radius);
				if (!field.IsReachable(spot) || LengthSquared(spot - candidate) > 1.5f)
					continue;
				float nearest = 1e9f;
				bool seen = false;
				for (const FighterTarget& target : scene.Targets)
				{
					nearest = std::min(nearest, Length(target.Position - spot));
					seen = seen || HasLineOfFire(level, target.Position, spot);
				}
				const float score = std::min(nearest, kit.Range + 1.0f) + (seen ? 1.0f : 0.0f);
				if (score > bestScore)
				{
					bestScore = score;
					best = spot;
				}
			}
			if (best == transform.Position)
				return;
			world.SpawnEffect(AbilityId::Warp, transform.Position, transform.Facing, NoPlayer, 0, 1.0f);
			transform.Position = best;
			world.SpawnEffect(AbilityId::Warp, best, transform.Facing, NoPlayer, 0, 1.0f);
			world.MarkTargetsMoved();
			enemy.Anchor = best;
			enemy.BlinkCooldown = kit.BlinkCooldown;
		}

		void Turn(Enemy& enemy, Vec2 desired, float rate, float dt)
		{
			if (LengthSquared(desired) < 1e-6f)
				return;
			if (LengthSquared(enemy.AimDirection) < 1e-6f)
			{
				enemy.AimDirection = Normalize(desired);
				return;
			}
			const float current = AngleOf(enemy.AimDirection);
			enemy.AimDirection = FromAngle(current + std::clamp(AngleBetween(current, AngleOf(desired)), -rate * dt, rate * dt));
		}

		void Release(World& world, Entity entity, Transform& transform, Enemy& enemy, const FighterKit& kit, const FighterTarget* target)
		{
			const int8_t index = enemy.Move;
			const FighterMove& move = kit.Moves[size_t(index)];
			AbilityContext context;
			context.Origin = transform.Position;
			context.Direction = LengthSquared(enemy.AimDirection) > 0.0f ? enemy.AimDirection : FromAngle(transform.Facing);
			if (GetAbility(move.Ability).Kind == AbilityKind::Projectile && kit.AimError > 0.0f)
				context.Direction = FromAngle(AngleOf(context.Direction) + (world.GetRandom().Float() * 2.0f - 1.0f) * kit.AimError);
			context.Target = move.AtSelf ? transform.Position : enemy.AimPoint;
			context.Side = Team::Enemies;
			context.Source = entity;
			context.DamageScale = enemy.DamageScale;
			context.BaseDamage = move.Damage > 0 ? move.Damage : -1;
			context.PushScale = move.PushScale;
			if (enemy.Elite)
			{
				context.AreaScale = EliteArea;
				context.ExtraProjectiles = GetAbility(move.Ability).ProjectileCount > 1 ? EliteExtraProjectiles : uint8_t(0);
			}
			if (move.Rank == RoarRank)
			{
				context.AreaScale = ChampionRoarArea;
				context.DamageScale *= 0.6f;
			}
			const uint32_t casts = std::max<uint32_t>(move.Repeat, 1u);
			const float facing = AngleOf(context.Direction);
			const float reach = Length(context.Target - context.Origin);
			for (uint32_t k = 0; k < casts; k++)
			{
				AbilityContext cast = context;
				if (move.Ring > 0.0f && k > 0)
					cast.Target = context.Target + FromAngle(facing + Pi * 0.5f + 2.0f * Pi * float(k - 1) / float(std::max<uint32_t>(casts - 1, 1u))) * move.Ring;
				else if (move.Spread != 0.0f)
				{
					cast.Direction = FromAngle(facing + (float(k) - 0.5f * float(casts - 1)) * move.Spread);
					cast.Target = context.Origin + cast.Direction * reach;
				}
				const float delay = move.Interval * float(k);
				if (delay <= 0.0f)
					ExecuteAbility(world, move.Ability, cast);
				else
					world.QueueCast(entity, move.Ability, cast, delay, move.Track, true);
			}
			enemy.MoveCooldowns[size_t(index)] = move.Cooldown * enemy.CooldownScale;
			enemy.Windup = 0.0f;
			if (move.BlinkAfter && kit.BlinkCooldown > 0.0f && enemy.BlinkCooldown <= 0.0f)
				enemy.Blink = FighterBlinkDelay;
			if (move.Next >= 0 && target)
			{
				const FighterMove& next = kit.Moves[size_t(move.Next)];
				if (Length(target->Position - transform.Position) <= next.MaxRange + target->Radius)
				{
					enemy.Move = move.Next;
					enemy.Windup = next.Windup;
					return;
				}
			}
			enemy.Move = -1;
			enemy.Recovery = move.Recovery;
			enemy.AttackCooldown = kit.Pause * enemy.CooldownScale;
		}

		void Finish(World& world, Entity entity, Transform& transform, Enemy& enemy, const FighterKit& kit, const FighterTarget* target, float radius)
		{
			const FighterMove& move = kit.Moves[size_t(enemy.Move)];
			if (move.Lunge <= 0.0f)
			{
				Release(world, entity, transform, enemy, kit, target);
				return;
			}
			float reach = move.Lunge;
			if (target)
				reach = std::clamp(Length(target->Position - transform.Position) - radius - target->Radius - LungeStandoff, MinLunge, move.Lunge);
			enemy.Windup = 0.0f;
			StartDash(enemy, enemy.AimDirection, reach, move.LungeSpeed);
		}

		Vec2 PathTo(World& world, Vec2 position, float radius, Vec2 goal)
		{
			const FlowField::Step step = world.GetNavigation(radius).GetStep(position, radius);
			if (step.Portal >= 0)
				if (const Portal* portal = world.GetLevel().FindPortal(uint32_t(step.Portal)))
					return Normalize(portal->Position - position);
			if (LengthSquared(step.Direction) > 0.0f)
				return step.Direction;
			return Normalize(goal - position);
		}

		bool ChooseMove(World& world, Transform& transform, Enemy& enemy, const FighterKit& kit, const FighterTarget& target, float distance, FighterScene& scene)
		{
			if (enemy.AttackCooldown > 0.0f || scene.Attackers >= scene.MaxAttackers || IsMirrored(enemy))
				return false;
			Random& random = world.GetRandom();
			uint32_t options = 0;
			int8_t chosen = -1;
			for (uint32_t i = 0; i < kit.MoveCount; i++)
			{
				const FighterMove& move = kit.Moves[i];
				if (!move.Opener || move.Rank > enemy.Rank || enemy.MoveCooldowns[i] > 0.0f || distance < move.MinRange || distance > move.MaxRange + target.Radius)
					continue;
				if (random.Below(++options) == 0)
					chosen = int8_t(i);
			}
			if (chosen < 0)
				return false;
			const FighterMove& move = kit.Moves[size_t(chosen)];
			enemy.Move = chosen;
			enemy.Windup = move.Windup;
			enemy.GuardUp = false;
			Vec2 aim = target.Position + target.Velocity * move.Lead - transform.Position;
			if (LengthSquared(aim) < 0.01f)
				aim = target.Position - transform.Position;
			enemy.AimDirection = Normalize(aim);
			enemy.AimPoint = move.AtSelf ? transform.Position : transform.Position + enemy.AimDirection * std::min(Length(aim), move.MaxRange + 1.0f);
			scene.Attackers++;
			return true;
		}
	}

	void GatherFighterScene(World& world, FighterScene& scene)
	{
		EntityRegistry& entities = world.GetEntities();
		scene.Targets.clear();
		scene.Threats.clear();
		scene.Attackers = 0;
		const double past = double(world.GetTick()) - double(VelocityTicks);
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			const Transform* transform = entities.TryGet<Transform>(entity);
			if (!controller.State.Alive || !transform)
				return;
			Vec2 before = transform->Position;
			world.GetPastPosition(entity, past, before);
			scene.Targets.push_back({ entity, transform->Position, (transform->Position - before) / (VelocityTicks * world.GetTickSeconds()), PlayerRadius });
		});
		scene.MaxAttackers = FighterAttackers + uint32_t(scene.Targets.size() > 1 ? scene.Targets.size() - 1 : 0);
		entities.Each<Projectile>([&](Entity entity, Projectile& projectile)
		{
			if (projectile.Side != Team::Players)
				return;
			if (const Transform* transform = entities.TryGet<Transform>(entity))
				scene.Threats.push_back({ transform->Position, projectile.Velocity, projectile.Radius, 0.0f, false });
		});
		entities.Each<PendingStrike>([&](Entity entity, PendingStrike& strike)
		{
			if (strike.Side != Team::Players || strike.Damage <= 0)
				return;
			if (const Transform* transform = entities.TryGet<Transform>(entity))
				scene.Threats.push_back({ transform->Position, FromAngle(transform->Facing), std::max(strike.Radius, strike.Length), strike.Remaining, true });
		});
		scene.Fighters.clear();
		entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			if (!IsFighterKind(enemy.Type))
				return;
			scene.Attackers += enemy.Move >= 0 ? 1u : 0u;
			if (const Transform* transform = entities.TryGet<Transform>(entity))
				scene.Fighters.push_back(transform->Position);
		});
	}

	void UpdateFighter(World& world, Entity entity, Transform& transform, Enemy& enemy, const EnemyDef& def, float radius, FighterScene& scene, Vec2 push, float dt)
	{
		const FighterKit* found = FindFighterKit(enemy.Type);
		if (!found)
			return;
		const FighterKit& kit = *found;
		const Level& level = world.GetLevel();
		const float pace = enemy.Chill > 0.0f ? ChillPace : 1.0f;
		const float haste = enemy.Empowered > 0.0f ? EmpowerHaste : 1.0f;
		Cool(enemy.Chill, dt);
		Cool(enemy.Evade, dt);
		Cool(enemy.DodgeCooldown, dt);
		Cool(enemy.BlinkCooldown, dt);
		Cool(enemy.GuardBroken, dt);
		Cool(enemy.AttackCooldown, dt * pace * haste);
		for (float& cooldown : enemy.MoveCooldowns)
			Cool(cooldown, dt * pace * haste);
		enemy.PoiseIdle += dt;
		if (enemy.PoiseIdle > FighterPoiseIdle)
			Cool(enemy.Poise, FighterPoiseDecay * dt);
		if (!enemy.GuardUp && enemy.GuardBroken <= 0.0f)
			enemy.GuardLeft = std::min(kit.Guard, enemy.GuardLeft + kit.GuardRegen * dt);

		if (enemy.Stun > 0.0f || enemy.Stagger > 0.0f)
		{
			Cool(enemy.Stun, dt);
			Cool(enemy.Stagger, dt);
			CancelMove(enemy);
			enemy.GuardUp = false;
			enemy.Blink = 0.0f;
			enemy.Recovery = 0.0f;
			return;
		}
		if (enemy.Blink > 0.0f && (enemy.Blink -= dt) <= 0.0f)
		{
			enemy.Blink = 0.0f;
			Blink(world, transform, enemy, kit, radius, scene);
		}

		enemy.RetargetTimer -= dt;
		const FighterTarget* target = FindTarget(scene, enemy.Target);
		if (!target || enemy.RetargetTimer <= 0.0f)
		{
			target = Nearest(scene, transform.Position);
			enemy.Target = target ? target->Handle : Entity{};
			enemy.RetargetTimer = RetargetSeconds;
		}

		if (enemy.DashLeft > 0.0f)
		{
			const float step = std::min(enemy.DashLeft, enemy.DashSpeed * dt);
			const Vec2 from = transform.Position;
			transform.Position = MoveCircle(level, from, radius, enemy.DashDirection * step);
			enemy.DashLeft -= step;
			if (LengthSquared(transform.Position - from) < step * step * 0.25f)
				enemy.DashLeft = 0.0f;
			if (target && enemy.Move >= 0 && LengthSquared(target->Position - transform.Position) <= (radius + target->Radius + LungeStandoff) * (radius + target->Radius + LungeStandoff))
				enemy.DashLeft = 0.0f;
			if (enemy.DashLeft <= 0.0f && enemy.Move >= 0)
				Release(world, entity, transform, enemy, kit, target);
			enemy.Anchor = transform.Position;
			return;
		}
		if (enemy.Move >= 0 && enemy.Windup <= 0.0f)
			CancelMove(enemy);
		if (enemy.Recovery > 0.0f)
		{
			Cool(enemy.Recovery, dt * pace);
			enemy.GuardUp = false;
			return;
		}
		if (!target)
		{
			CancelMove(enemy);
			enemy.GuardUp = false;
			return;
		}

		const Vec2 toTarget = target->Position - transform.Position;
		const float distance = Length(toTarget);
		if (enemy.Move >= 0)
		{
			const FighterMove& move = kit.Moves[size_t(enemy.Move)];
			if (move.Lead <= 0.0f && enemy.Windup > move.Lock)
			{
				Turn(enemy, toTarget, move.Turn, dt);
				if (!move.AtSelf)
					enemy.AimPoint = transform.Position + enemy.AimDirection * std::min(distance, move.MaxRange + 1.0f);
			}
			transform.Facing = AngleOf(enemy.AimDirection);
			enemy.Windup -= dt * pace;
			if (enemy.Windup <= 0.0f)
				Finish(world, entity, transform, enemy, kit, target, radius);
			enemy.Anchor = transform.Position;
			return;
		}

		const Threat threat = Assess(scene, transform.Position, radius, IsMirrored(enemy));
		const bool threatened = threat.Eta < 1e8f;
		if (!threatened)
		{
			enemy.Alarm = 0.0f;
			enemy.Reaction = Undecided;
		}
		else if ((enemy.Alarm += dt) >= kit.Reaction && enemy.Reaction == Undecided)
		{
			const bool frontal = std::abs(AngleBetween(transform.Facing, AngleOf(threat.Incoming))) <= GetAbility(AbilityId::HexShield).HalfArc;
			const float roll = world.GetRandom().Float();
			if (!threat.Area && frontal && kit.Guard > 0.0f && enemy.GuardBroken <= 0.0f && enemy.GuardLeft > 1.0f)
				enemy.Reaction = Guarded;
			else if (kit.DodgeChance > 0.0f && enemy.DodgeCooldown <= 0.0f && roll < kit.DodgeChance)
			{
				enemy.Reaction = Dodged;
				Dodge(world, transform, enemy, kit, radius, threat.Escape);
				return;
			}
			else if (kit.BlinkCooldown > 0.0f && enemy.BlinkCooldown <= 0.0f && threat.Area && roll < 0.7f)
			{
				enemy.Reaction = Dodged;
				Blink(world, transform, enemy, kit, radius, scene);
				return;
			}
			else
				enemy.Reaction = Ignored;
		}

		const bool visible = distance < DirectRange && HasLineOfFire(level, transform.Position, target->Position);
		enemy.GuardUp = kit.Guard > 0.0f && enemy.GuardBroken <= 0.0f && enemy.GuardLeft > 1.0f && (distance <= kit.GuardRange || (threatened && enemy.Reaction == Guarded));
		if (kit.Retreat > 0.0f && distance < kit.Retreat && enemy.DodgeCooldown <= 0.0f && visible)
		{
			const Vec2 away = distance > 1e-3f ? toTarget * (-1.0f / distance) : FromAngle(transform.Facing + Pi);
			Dodge(world, transform, enemy, kit, radius, Normalize(away + Perpendicular(away) * (enemy.Strafe * 0.5f)));
			return;
		}
		if (visible && ChooseMove(world, transform, enemy, kit, *target, distance, scene))
		{
			transform.Facing = AngleOf(enemy.AimDirection);
			return;
		}
		if (kit.Rooted)
		{
			if (visible)
				transform.Facing = AngleOf(toTarget);
			enemy.Anchor = transform.Position;
			return;
		}

		Random& random = world.GetRandom();
		if ((enemy.StrafeTimer -= dt) <= 0.0f)
		{
			enemy.StrafeTimer = 1.2f + 1.8f * random.Float();
			if (random.Below(2) == 0)
				enemy.Strafe = -enemy.Strafe;
		}
		Vec2 desired;
		if (!visible || distance > DirectRange)
			desired = PathTo(world, transform.Position, radius, target->Position);
		else
		{
			const Vec2 toward = distance > 1e-3f ? toTarget / distance : FromAngle(transform.Facing);
			float radial = 0.0f;
			if (distance > kit.Range + kit.RangeSlack)
				radial = 1.0f;
			else if (distance < kit.Range - kit.RangeSlack)
				radial = -1.0f;
			Vec2 spacing;
			for (const Vec2 other : scene.Fighters)
			{
				const Vec2 gap = transform.Position - other;
				const float span = LengthSquared(gap);
				if (span > 1e-4f && span < FighterSpacing * FighterSpacing)
				{
					const float apart = std::sqrt(span);
					spacing += gap * ((FighterSpacing - apart) / (apart * FighterSpacing));
				}
			}
			desired = toward * radial + Perpendicular(toward) * (enemy.Strafe * StrafeWeight) + spacing * SpacingWeight;
			if (radial > 0.0f && !IsPathClear(level, transform.Position, target->Position, radius))
				desired = PathTo(world, transform.Position, radius, target->Position);
			desired = Normalize(desired);
		}
		const float speed = def.Speed * enemy.SpeedScale * pace * (enemy.GuardUp ? kit.GuardSpeed : 1.0f) * (enemy.Empowered > 0.0f ? EmpowerSpeed : 1.0f);
		const Vec2 from = transform.Position;
		transform.Position = MoveCircle(level, from, radius, desired * (speed * dt) + push);
		if (LengthSquared(transform.Position - from) < 1e-6f && LengthSquared(desired) > 0.0f)
			enemy.Strafe = -enemy.Strafe;
		transform.Facing = visible ? AngleOf(toTarget) : LengthSquared(desired) > 0.0f ? AngleOf(desired) : transform.Facing;
		enemy.Anchor = transform.Position;
	}

	void StaggerFighter(Enemy& enemy, float seconds)
	{
		enemy.Stagger = std::max(enemy.Stagger, seconds);
		enemy.Poise = 0.0f;
		CancelMove(enemy);
		enemy.GuardUp = false;
		enemy.Blink = 0.0f;
		enemy.Recovery = 0.0f;
	}

	void BreakFighterGuard(Enemy& enemy)
	{
		const FighterKit* kit = FindFighterKit(enemy.Type);
		enemy.GuardUp = false;
		enemy.GuardLeft = 0.0f;
		enemy.GuardBroken = kit ? kit->GuardCooldown : 5.0f;
		StaggerFighter(enemy, FighterGuardBreakStagger);
	}

	void StrikeFighter(World& world, Entity entity, Enemy& enemy, const HitInfo& hit, int32_t damage)
	{
		const FighterKit* kit = FindFighterKit(enemy.Type);
		const Health* health = world.GetEntities().TryGet<Health>(entity);
		if (!kit || !health || health->IsDead())
			return;
		const bool control = hit.Stun > 0.0f || hit.Chill > 0.0f;
		if (control && enemy.Move >= 0 && enemy.Windup > 0.0f && kit->Moves[size_t(enemy.Move)].Channel)
		{
			CancelMove(enemy);
			enemy.Recovery = FighterInterruptRecovery;
		}
		if (kit->Poise > 0.0f && enemy.Stagger <= 0.0f)
		{
			enemy.Poise += float(damage) * (float(damage) >= FighterHeavyHit ? FighterHeavyPoise : 1.0f) + (hit.Chill > 0.0f ? FighterChillPoise : 0.0f);
			enemy.PoiseIdle = 0.0f;
			if (enemy.Poise >= kit->Poise * enemy.PoiseScale)
				StaggerFighter(enemy, FighterStaggerSeconds);
		}
		if (kit->BlinkCooldown > 0.0f && enemy.BlinkCooldown <= 0.0f && enemy.Blink <= 0.0f && enemy.Move < 0 && enemy.Stagger <= 0.0f)
			enemy.Blink = FighterBlinkDelay;
	}

	void UpdateEnemyTraits(Enemy& enemy, float dt)
	{
		Cool(enemy.Empowered, dt);
		Cool(enemy.Cracked, dt);
		if (enemy.WardMax > 0.0f && (enemy.WardIdle += dt) >= WardedIdle)
			enemy.Ward = std::min(enemy.WardMax, enemy.Ward + enemy.WardMax * WardedRefill * dt);
		const FighterKit* kit = FindFighterKit(enemy.Type);
		float open = kit ? kit->MirrorOpen : 0.0f;
		float closed = kit ? kit->MirrorClosed : 0.0f;
		if (open <= 0.0f && HasAffix(enemy.Affixes, EliteAffix::Reflective))
		{
			open = ReflectiveOpen;
			closed = ReflectiveClosed;
		}
		if (open <= 0.0f)
			return;
		if (enemy.Stun > 0.0f || enemy.Stagger > 0.0f)
		{
			if (enemy.Mirror > 0.0f)
				enemy.MirrorWait = closed;
			enemy.Mirror = 0.0f;
			return;
		}
		if (enemy.Mirror > 0.0f)
		{
			if ((enemy.Mirror -= dt) <= 0.0f)
			{
				enemy.Mirror = 0.0f;
				enemy.MirrorWait = closed;
			}
			return;
		}
		Cool(enemy.MirrorWait, dt);
		if (enemy.MirrorWait <= 0.0f && enemy.Move < 0 && enemy.DashLeft <= 0.0f && enemy.Windup <= 0.0f && enemy.Lunge <= 0.0f)
			enemy.Mirror = open;
	}

	void EmpowerAllies(World& world, float dt)
	{
		struct Source
		{
			Entity Handle;
			Vec2 Position;
			float Radius = 0.0f;
		};
		EntityRegistry& entities = world.GetEntities();
		thread_local std::vector<Source> sources;
		sources.clear();
		entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			const FighterKit* kit = FindFighterKit(enemy.Type);
			const Health* health = entities.TryGet<Health>(entity);
			const Transform* transform = entities.TryGet<Transform>(entity);
			if (kit && kit->Aura > 0.0f && enemy.Sprout <= 0.0f && enemy.Stun <= 0.0f && enemy.Stagger <= 0.0f && health && !health->IsDead() && transform)
				sources.push_back({ entity, transform->Position, kit->Aura });
		});
		if (sources.empty())
			return;
		entities.Each<Enemy>([&](Entity entity, Enemy& enemy)
		{
			Health* health = entities.TryGet<Health>(entity);
			const Transform* transform = entities.TryGet<Transform>(entity);
			if (enemy.Sprout > 0.0f || !health || health->IsDead() || !transform)
				return;
			for (const Source& source : sources)
			{
				if (source.Handle == entity || LengthSquared(transform->Position - source.Position) > source.Radius * source.Radius)
					continue;
				enemy.Empowered = EmpowerLinger;
				if (health->Current < health->Max)
				{
					enemy.Mend += float(health->Max) * EmpowerHeal * dt;
					const int32_t heal = int32_t(enemy.Mend);
					enemy.Mend -= float(heal);
					health->Current = std::min(health->Max, health->Current + heal);
				}
				break;
			}
		});
	}

	int32_t ShieldEnemy(World& world, Entity entity, Enemy& enemy, int32_t amount, Element damages, uint32_t attacker, uint8_t ability)
	{
		if (amount <= 0)
			return amount;
		if (const FighterKit* kit = FindFighterKit(enemy.Type); kit && kit->Shell > 0.0f && enemy.Cracked <= 0.0f)
		{
			if (float(amount) >= kit->Shell)
			{
				enemy.Cracked = ShellRegrow;
				StaggerFighter(enemy, ShellCrackStagger);
			}
			else
				amount = std::max(1, int32_t(std::lround(float(amount) * ShellArmor)));
		}
		if (enemy.WardMax <= 0.0f)
			return amount;
		enemy.WardIdle = 0.0f;
		if (enemy.Ward <= 0.0f)
			return amount;
		const float absorbed = std::min(enemy.Ward, float(amount));
		enemy.Ward -= absorbed;
		if (const Transform* transform = world.GetEntities().TryGet<Transform>(entity))
			if (const Replicated* replicated = world.GetEntities().TryGet<Replicated>(entity))
				world.RecordHit({ .Target = replicated->NetId, .Position = transform->Position, .Damage = uint16_t(std::lround(absorbed)), .Damages = damages, .Attacker = attacker, .Guarded = true, .Ability = ability });
		return amount - int32_t(std::lround(absorbed));
	}

	void FeedVampire(World& world, Entity source)
	{
		EntityRegistry& entities = world.GetEntities();
		const Enemy* enemy = entities.TryGet<Enemy>(source);
		Health* health = entities.TryGet<Health>(source);
		if (!enemy || !health || health->IsDead() || !HasAffix(enemy->Affixes, EliteAffix::Vampiric))
			return;
		health->Current = std::min(health->Max, health->Current + std::max(1, int32_t(std::lround(float(health->Max) * VampiricHeal))));
	}

	void ApplyAffixes(World& world, Entity entity, uint8_t affixes)
	{
		EntityRegistry& entities = world.GetEntities();
		Enemy* enemy = entities.TryGet<Enemy>(entity);
		const Health* health = entities.TryGet<Health>(entity);
		if (!enemy || !health)
			return;
		enemy->Affixes = affixes;
		if (HasAffix(affixes, EliteAffix::Hasted))
		{
			enemy->SpeedScale *= HastedSpeed;
			enemy->CooldownScale *= HastedCooldown;
		}
		if (HasAffix(affixes, EliteAffix::Warded))
		{
			enemy->WardMax = float(health->Max) * WardedShare;
			enemy->Ward = enemy->WardMax;
		}
		if (HasAffix(affixes, EliteAffix::Reflective))
			enemy->MirrorWait = ReflectiveClosed * 0.5f;
		if (Replicated* replicated = entities.TryGet<Replicated>(entity))
			replicated->Tag = affixes;
	}

	uint8_t RollAffixes(Random& random, EntityKind kind, uint32_t count, uint8_t banned)
	{
		uint8_t chosen = banned;
		for (uint32_t n = 0; n < count; n++)
		{
			uint32_t options = 0;
			int32_t pick = -1;
			for (uint32_t affix = 0; affix < uint32_t(EliteAffix::Count); affix++)
				if (!HasAffix(chosen, EliteAffix(affix)) && AffixFits(kind, EliteAffix(affix)) && random.Below(++options) == 0)
					pick = int32_t(affix);
			if (pick < 0)
				break;
			chosen |= AffixBit(EliteAffix(pick));
		}
		return uint8_t(chosen & ~banned);
	}
}
