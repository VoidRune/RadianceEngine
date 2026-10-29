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
		constexpr float ChainRange = 5.5f;

		constexpr float BossChillScale = 0.5f;

		bool IsVulnerable(const EntityRegistry& entities, Entity entity, const Health& health)
		{
			if (health.IsDead())
				return false;
			if (const Boss* boss = entities.TryGet<Boss>(entity))
				return !boss->Burrowed;
			if (const Dummy* dummy = entities.TryGet<Dummy>(entity))
				return dummy->Fall <= 0.0f;
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
			if (ability.ImpactRadius > 0.0f)
				return strike.Radius / ability.ImpactRadius;
			return ability.Reach > 0.0f ? strike.Radius / ability.Reach : 1.0f;
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
			float Facing = 0.0f;
			bool Guarded = false;
			bool Warded = false;
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
					targets.push_back({ entity, transform->Position, collider->Radius, health.Side, controller ? controller->PlayerId : NoPlayer, transform->Facing, controller && IsGuarding(controller->State, controller->Traits), controller && IsWarded(controller->State) && !controller->State.IsFalling() });
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
			if (target.PlayerId != NoPlayer)
				rewind = std::min(rewind, MaxPlayerRewindTicks);
			return rewind <= 0.5f || world.GetPastPosition(target.Handle, double(world.GetTick()) - double(rewind), position);
		}

		HitInfo MakeHit(int32_t damage, const AbilityDef& ability, const AbilityContext& context)
		{
			HitInfo hit{ damage, context.Owner, context.Source, context.CritChance, context.CritMultiplier, context.LifeSteal, ability.Damages, ability.Chill, ability.Stun, context.PvpScale, uint8_t(ability.Id) };
			hit.Push = ability.Push * context.PushScale;
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
			hit.Slick = (projectile.Perks & PerkSlick) != 0;
			hit.Returned = projectile.Returned;
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
			world.SpawnEffect(ability.Id, position, AngleOf(projectile.Velocity), owner, tag, ability.ImpactRadius > 0.0f ? projectile.ImpactRadius / ability.ImpactRadius : 1.0f);
		}

		void SpearBurst(World& world, const Projectile& projectile, Vec2 position, uint32_t owner)
		{
			HitInfo hit = MakeHit(projectile, owner);
			hit.Damage = ScaleDamage(projectile.Damage, float(SpearBurstDamage) / float(SpearFullDamage));
			hit.Push = projectile.Push * (SpearBurstPush / SpearFullPush);
			hit.Chill = SpearBurstChill;
			hit.Mode = PushMode::Radial;
			hit.PushOrigin = position;
			hit.PushReach = SpearBurstRadius;
			StrikeCircle(world, position, SpearBurstRadius, hit, projectile.Side, projectile.Rewind, { projectile.Hits.data(), projectile.HitCount });
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
			Dummy* dummy = entities.TryGet<Dummy>(target);
			if (caster == target || !from || !to || !casterController || (!targetController && !dummy) || !casterController->State.Alive)
				return false;
			std::swap(from->Position, to->Position);
			if (targetController)
				world.NoteSwap(targetController->PlayerId, casterController->PlayerId, to->Position);
			for (PlayerController* controller : { casterController, targetController })
				if (controller)
				{
					controller->State.Push = {};
					controller->State.DashDistance = 0.0f;
				}
			if (dummy)
			{
				dummy->Push = {};
				dummy->Idle = 0.0f;
			}
			if (targetController && world.GetMatch().HasPlayerCard(casterController->PlayerId, CardId::HeartSwap))
				if (Health* mine = entities.TryGet<Health>(caster), *theirs = entities.TryGet<Health>(target); mine && theirs)
				{
					const float kept = float(mine->Current) / float(std::max(mine->Max, 1));
					const float taken = float(theirs->Current) / float(std::max(theirs->Max, 1));
					if (!world.IsDamageBrawl())
					{
						mine->Current = std::clamp(int32_t(std::lround(taken * float(mine->Max))), 1, mine->Max);
						theirs->Current = std::clamp(int32_t(std::lround(kept * float(theirs->Max))), 1, theirs->Max);
					}
					else if (theirs->Current > mine->Current)
					{
						const int32_t stolen = std::min(HeartSwapSteal, (theirs->Current - mine->Current) / 2);
						mine->Current = std::min(mine->Max, mine->Current + stolen);
						theirs->Current = std::max(1, theirs->Current - stolen);
					}
				}
			ResetHistory(world, caster, from->Position);
			ResetHistory(world, target, to->Position);
			world.SpawnEffect(AbilityId::Swap, from->Position, from->Facing, casterController->PlayerId, 0, 1.0f);
			world.SpawnEffect(AbilityId::Swap, to->Position, to->Facing, casterController->PlayerId, 0, 1.0f);
			return true;
		}

		Vec2 PushImpulse(const HitInfo& hit, Vec2 position)
		{
			const Vec2 offset = position - hit.PushOrigin;
			const float distance = Length(offset);
			const Vec2 radial = distance > 1e-4f ? offset / distance : Normalize(hit.PushDirection);
			switch (hit.Mode)
			{
			case PushMode::Pull:
				return radial * -(distance * PushFriction * (hit.Push > 0.0f ? hit.Push : 0.95f * std::max(-hit.Push, 1.0f)));
			case PushMode::Radial:
				return radial * (hit.Push * (hit.PushReach > 0.0f ? 1.0f - 0.4f * std::clamp(distance / hit.PushReach, 0.0f, 1.0f) : 1.0f));
			case PushMode::Cone:
				return Normalize(Normalize(hit.PushDirection) * 0.65f + radial * 0.35f) * hit.Push;
			default:
				return Normalize(hit.PushDirection) * hit.Push;
			}
		}

		bool ShoveDummy(World& world, Entity target, Dummy& dummy, const HitInfo& hit, bool record)
		{
			EntityRegistry& entities = world.GetEntities();
			const Transform* transform = entities.TryGet<Transform>(target);
			const Health* health = entities.TryGet<Health>(target);
			if (!transform || !health || dummy.Fall > 0.0f)
				return false;
			if (const Replicated* replicated = entities.TryGet<Replicated>(target); replicated && record)
				world.RecordHit({ replicated->NetId, transform->Position, 0, false, false, hit.Damages });
			if (dummy.Kind == DummyKind::Balloon)
			{
				world.PopBalloon(dummy, hit.Attacker);
				return true;
			}
			if (hit.Swap)
			{
				SwapPlaces(world, hit.Source, target);
				return true;
			}
			if (hit.Push == 0.0f)
				return true;
			const bool pull = hit.Mode == PushMode::Pull;
			const float fraction = float(std::max(health->Current, 0)) / float(std::max(health->Max, 1));
			const float fling = world.UsesBrawlDamage() ? (pull ? DamagePullScale : DamagePushScale) : pull ? 1.0f : 1.0f + MissingHealthKnockback * (1.0f - fraction);
			dummy.Push = ClampLength(dummy.Push + PushImpulse(hit, transform->Position) * fling, MaxPush);
			dummy.LastAttacker = hit.Attacker;
			dummy.Idle = 0.0f;
			world.OnDummyShoved(dummy, hit.Attacker);
			return true;
		}

		bool ApplyKnockback(World& world, Entity target, const HitInfo& hit, bool record)
		{
			EntityRegistry& entities = world.GetEntities();
			if (Dummy* dummy = entities.TryGet<Dummy>(target))
				return ShoveDummy(world, target, *dummy, hit, record);
			PlayerController* controller = entities.TryGet<PlayerController>(target);
			const Transform* transform = entities.TryGet<Transform>(target);
			const Health* health = entities.TryGet<Health>(target);
			if (!controller || !transform || !health || !controller->State.Alive || controller->State.Invulnerable > 0.0f || controller->State.IsFalling())
				return false;
			if (hit.Attacker != NoPlayer && hit.Attacker != controller->PlayerId)
			{
				controller->LastAttacker = hit.Attacker;
				controller->LastAttackAge = 0.0f;
				controller->LastAbility = uint8_t(CreditOf(AbilityId(hit.Ability)));
			}
			if (const Replicated* replicated = entities.TryGet<Replicated>(target); replicated && record)
				world.RecordHit({ replicated->NetId, transform->Position, 0, false, true, hit.Damages });
			if (hit.Swap)
			{
				SwapPlaces(world, hit.Source, target);
				return true;
			}
			if (hit.Push == 0.0f || IsWarded(controller->State))
				return true;
			const float fraction = float(std::max(health->Current, 0)) / float(std::max(health->Max, 1));
			Vec2 impulse = PushImpulse(hit, transform->Position);
			impulse *= world.GetMatch().GetKnockbackScale(hit.Attacker, controller->PlayerId, fraction, hit.Mode == PushMode::Pull);
			PlayerState& state = controller->State;
			state.Push = ClampLength(state.Push + impulse, MaxPush);
			if (hit.Mode != PushMode::Pull)
				world.NoteFling(controller->PlayerId, hit.Attacker, hit.Ability);
			if (Length(impulse) >= PushStagger)
			{
				state.CastSlot = -1;
				state.CastTimer = 0.0f;
			}
			return true;
		}

		enum class GuardOutcome : uint8_t
		{
			Open,
			Held,
			Broke,
		};

		GuardOutcome Guard(World& world, Entity target, const HitInfo& hit, int32_t& damage, HitInfo& through)
		{
			EntityRegistry& entities = world.GetEntities();
			PlayerController* controller = entities.TryGet<PlayerController>(target);
			const Transform* transform = entities.TryGet<Transform>(target);
			if (!controller || !transform || !IsGuarding(controller->State, controller->Traits) || controller->State.Invulnerable > 0.0f || controller->State.IsFalling())
				return GuardOutcome::Open;
			const AbilityDef& shield = GetAbility(AbilityId::HexShield);
			const Vec2 position = transform->Position;
			Vec2 point = hit.GuardPoint;
			if (!hit.AtGuard)
			{
				const Vec2 incoming = hit.Mode == PushMode::Along ? hit.PushDirection * -1.0f : hit.PushOrigin - position;
				const float distance = Length(incoming);
				if (distance < 1e-4f || (hit.Mode != PushMode::Along && distance < HexShieldOverhead) || std::abs(AngleBetween(transform->Facing, AngleOf(incoming))) > shield.HalfArc)
					return GuardOutcome::Open;
				point = position + incoming * (shield.Reach / distance);
			}
			PlayerState& state = controller->State;
			const PlayerTraits& traits = controller->Traits;
			const bool brawlDamage = world.UsesBrawlDamage();
			const float wear = brawlDamage ? std::max(float(damage), 1.0f) : std::max(Length(PushImpulse(hit, position)), HexShieldMinWear);
			const float left = GuardLeft(state, traits);
			const float absorbed = std::min(wear, left);
			state.GuardLost += absorbed;
			if (brawlDamage)
				state.Mana = std::max(state.Mana - absorbed * HexShieldManaPerPoint, 0.0f);
			if (const Replicated* replicated = entities.TryGet<Replicated>(target))
				world.RecordHit({ replicated->NetId, point, uint16_t(std::lround(absorbed)), false, true, hit.Damages, hit.Attacker, true });
			if (wear <= left)
			{
				if (hit.Push != 0.0f && hit.Mode != PushMode::Pull && !hit.Swap)
				{
					HitInfo brace = hit;
					brace.Push *= HexShieldBrace;
					ApplyKnockback(world, target, brace, false);
				}
				return GuardOutcome::Held;
			}
			BreakGuard(state, traits);
			const float share = (wear - left) / wear;
			through = hit;
			through.Push *= share;
			through.AtGuard = false;
			damage = damage > 0 ? std::max(1, int32_t(std::lround(float(damage) * share))) : 0;
			return GuardOutcome::Broke;
		}

		int32_t StruckDamage(World& world, Entity target, const HitInfo& struck, float crit)
		{
			const EntityRegistry& entities = world.GetEntities();
			const bool pvp = struck.Attacker != NoPlayer && world.IsPvp() && entities.Has<PlayerController>(target);
			const Dummy* dummy = entities.TryGet<Dummy>(target);
			const bool unharmed = dummy && (dummy->Kind == DummyKind::Duck || dummy->Kind == DummyKind::Balloon);
			return struck.Damage > 0 && !unharmed ? std::max(1, ScaleDamage(struck.Damage, crit * (pvp ? struck.PvpScale : 1.0f))) : 0;
		}

		float WardWear(const World& world, const HitInfo& hit, int32_t damage, Vec2 position)
		{
			return world.UsesBrawlDamage() ? std::max(float(damage), 1.0f) : std::max(Length(PushImpulse(hit, position)), DiamondWardMinWear);
		}

		Vec2 WardPoint(const HitInfo& hit, Vec2 position)
		{
			if (hit.AtWard)
				return hit.GuardPoint;
			const Vec2 incoming = hit.Mode == PushMode::Along ? hit.PushDirection * -1.0f : hit.PushOrigin - position;
			const float distance = Length(incoming);
			return distance > 1e-4f ? position + incoming * (DiamondWardReach / distance) : position;
		}

		GuardOutcome Soak(World& world, Entity target, PlayerController& controller, float wear, Vec2 point, Element damages, uint32_t attacker, float& share)
		{
			PlayerState& state = controller.State;
			const float left = state.WardLeft;
			if (const Replicated* replicated = world.GetEntities().TryGet<Replicated>(target))
				world.RecordHit({ .Target = replicated->NetId, .Position = point, .Damage = uint16_t(std::lround(std::min(wear, left))), .OnPlayer = true, .Damages = damages, .Attacker = attacker, .Warded = true });
			if (wear <= left)
			{
				state.WardLeft = left - wear;
				if (state.WardLeft <= 1e-4f)
					BreakWard(state, controller.Traits);
				return GuardOutcome::Held;
			}
			BreakWard(state, controller.Traits);
			share = (wear - left) / wear;
			return GuardOutcome::Broke;
		}

		GuardOutcome Ward(World& world, Entity target, const HitInfo& hit, int32_t& damage, HitInfo& through)
		{
			EntityRegistry& entities = world.GetEntities();
			PlayerController* controller = entities.TryGet<PlayerController>(target);
			const Transform* transform = entities.TryGet<Transform>(target);
			if (!controller || !transform || !IsWarded(controller->State) || controller->State.Invulnerable > 0.0f || controller->State.IsFalling())
				return GuardOutcome::Open;
			const Vec2 position = transform->Position;
			float share = 0.0f;
			if (Soak(world, target, *controller, WardWear(world, hit, damage, position), WardPoint(hit, position), hit.Damages, hit.Attacker, share) == GuardOutcome::Held)
				return GuardOutcome::Held;
			through = hit;
			through.Push *= share;
			through.AtWard = false;
			damage = damage > 0 ? std::max(1, int32_t(std::lround(float(damage) * share))) : 0;
			return GuardOutcome::Broke;
		}

		bool Reflect(World& world, Entity entity, Projectile& projectile, Transform& transform, Entity target, Vec2 point, const HitInfo& hit)
		{
			EntityRegistry& entities = world.GetEntities();
			PlayerController* controller = entities.TryGet<PlayerController>(target);
			const Transform* body = entities.TryGet<Transform>(target);
			if (!controller || !body || !IsWarded(controller->State) || controller->State.IsFalling() || projectile.Reflections >= DiamondWardReflects)
				return false;
			const float wear = WardWear(world, hit, StruckDamage(world, target, hit, 1.0f), body->Position);
			if (wear > controller->State.WardLeft)
				return false;
			float share = 0.0f;
			Soak(world, target, *controller, wear, point, projectile.Damages, hit.Attacker, share);
			const float speed = Length(projectile.Velocity);
			Vec2 heading = Normalize(projectile.Velocity) * -1.0f;
			if (const Transform* caster = entities.TryGet<Transform>(projectile.Source); caster && entities.Has<PlayerController>(projectile.Source) && projectile.Source != target)
				if (const Vec2 toward = caster->Position - point; LengthSquared(toward) > 0.25f)
					heading = Normalize(toward);
			CancelRecast(world, projectile.Source, AbilityId(projectile.Ability));
			if (const PlayerController* sender = entities.TryGet<PlayerController>(projectile.Source); sender && projectile.Source != target)
				projectile.Returned = sender->PlayerId;
			projectile.Velocity = heading * speed;
			projectile.Lifetime = projectile.Span > 0.0f ? projectile.Span : GetProjectileLifetime(GetAbility(AbilityId(projectile.Ability)), speed);
			projectile.Source = target;
			if (world.GetMatch().HasPlayerCard(controller->PlayerId, CardId::ThornedMirror))
			{
				projectile.Push *= ThornScale;
				if (world.IsDamageBrawl())
					projectile.Damage = ScaleDamage(projectile.Damage, ThornScale);
			}
			projectile.Rewind = 0.0f;
			projectile.Floor = LaunchFloor(world.GetLevel(), point, projectile.Floor);
			projectile.Hits = {};
			projectile.Hits[0] = target;
			projectile.HitCount = 1;
			projectile.Reflections++;
			transform.Position = point;
			transform.Facing = AngleOf(heading);
			if (Replicated* replicated = entities.TryGet<Replicated>(entity))
			{
				replicated->Owner = controller->PlayerId;
				replicated->Tag = 0;
			}
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
			if (projectile.Perks & PerkPinball)
			{
				next.Push = projectile.Push * PinballGrowth;
				if (world.IsDamageBrawl())
					next.Damage = ScaleDamage(projectile.Damage, PinballGrowth);
			}
			if (next.HitCount < next.Hits.size())
				next.Hits[next.HitCount++] = hit;
			const Vec2 direction = Normalize(best->Position - from);
			const float speed = Length(projectile.Velocity);
			next.Velocity = direction * speed;
			next.Lifetime = ChainRange / std::max(speed, 1e-3f);
			world.SpawnProjectileCopy(next, from + direction * 0.3f, owner);
			return true;
		}

		void StormJumps(World& world, const std::vector<Target>& targets, const PendingStrike& strike, Vec2 center, const HitInfo& base, std::vector<Entity>& struck)
		{
			const AbilityDef& jump = GetAbility(AbilityId::StormJump);
			const uint32_t jumps = (strike.Perks & PerkFork) ? ForkedJumps : 1u;
			const float scale = strike.Push / std::max(GetAbility(AbilityId::StormStrike).Push, 1e-3f);
			Vec2 from = center;
			for (uint32_t i = 0; i < jumps; i++)
			{
				const Target* best = nullptr;
				float bestDistance = jump.Range * jump.Range;
				for (const Target& target : targets)
				{
					const float distance = LengthSquared(target.Position - from);
					if (distance >= bestDistance || !IsHostile(world, strike.Side, strike.Owner, target) || std::find(struck.begin(), struck.end(), target.Handle) != struck.end() || !HasLineOfFire(world.GetLevel(), from, target.Position))
						continue;
					best = &target;
					bestDistance = distance;
				}
				if (!best)
					return;
				HitInfo hit = base;
				hit.Ability = uint8_t(AbilityId::StormJump);
				if (world.UsesBrawlDamage())
					hit.Damage = int32_t(std::lround(float(base.Damage) * float(jump.BrawlDamage) / float(std::max(GetAbility(AbilityId::StormStrike).BrawlDamage, 1))));
				hit.Push = jump.Push * scale;
				hit.Mode = PushMode::Along;
				hit.PushDirection = best->Position - from;
				DealDamage(world, best->Handle, hit);
				struck.push_back(best->Handle);
				world.SpawnEffect(AbilityId::StormJump, best->Position, AngleOf(best->Position - from), strike.Owner, 0, std::sqrt(bestDistance));
				from = best->Position;
			}
		}

		template <typename AlreadyHit>
		void Seek(const World& world, const std::vector<Target>& targets, Projectile& projectile, Vec2 position, uint32_t owner, const AlreadyHit& alreadyHit, float dt)
		{
			const float speed = Length(projectile.Velocity);
			if (speed <= 0.0f)
				return;
			const Vec2 heading = projectile.Velocity / speed;
			const Target* best = nullptr;
			float bestDistance = SeekRange * SeekRange;
			for (const Target& target : targets)
			{
				const Vec2 offset = target.Position - position;
				const float distance = LengthSquared(offset);
				if (distance >= bestDistance || Dot(offset, heading) <= 0.0f || !IsHostile(world, projectile.Side, owner, target) || alreadyHit(target.Handle))
					continue;
				best = &target;
				bestDistance = distance;
			}
			if (!best)
				return;
			const float current = AngleOf(heading);
			const float turn = std::clamp(AngleBetween(current, AngleOf(best->Position - position)), -SeekTurnRate * dt, SeekTurnRate * dt);
			projectile.Velocity = FromAngle(current + turn) * speed;
		}

		void Split(World& world, const Projectile& projectile, Vec2 position, uint32_t owner)
		{
			const float speed = Length(projectile.Velocity);
			if (speed <= 0.0f)
				return;
			const float heading = AngleOf(projectile.Velocity);
			for (uint32_t i = 0; i < SplitShards; i++)
			{
				Projectile shard = projectile;
				shard.Velocity = FromAngle(heading + SplitSpread * (float(i) - float(SplitShards - 1) * 0.5f)) * speed;
				shard.Lifetime = SplitRange / speed;
				shard.Push = projectile.Push * SplitPushScale;
				shard.Damage = ScaleDamage(projectile.Damage, SplitPushScale);
				shard.Radius = projectile.Radius * SplitRadiusScale;
				shard.Perks = 0;
				shard.Hits = {};
				shard.HitCount = 0;
				shard.Rewind = 0.0f;
				world.SpawnProjectileCopy(shard, position, owner, SplitRadiusScale);
			}
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
		if (controller && !damage.Unmitigated && IsWarded(controller->State) && !controller->State.IsFalling())
		{
			const Transform* body = entities.TryGet<Transform>(target);
			float share = 0.0f;
			if (Soak(world, target, *controller, float(amount), body ? body->Position : Vec2{}, damage.Damages, damage.Attacker, share) == GuardOutcome::Held)
				return true;
			amount = std::max(1, int32_t(std::lround(float(amount) * share)));
		}
		if (controller && !damage.Unmitigated)
			amount = std::max(1, int32_t(std::lround(float(amount) * DamageTakenScale(controller->Traits, damage.Damages, damage.Pvp ? PvpMaxArmorReduction : MaxArmorReduction))));
		if (controller && damage.Pvp)
			amount = std::min(amount, std::max(1, int32_t(float(controller->Traits.MaxHealth) * PvpMaxHitFraction)));
		if (controller && world.IsDamageBrawl())
			amount = world.ShapeBrawlDamage(controller->State, controller->PlayerId, damage.Attacker, damage.Ability, damage.Unmitigated, controller->State.Chill > 0.0f, amount);
		if (controller && damage.Attacker != NoPlayer && damage.Attacker != controller->PlayerId)
		{
			controller->LastAttacker = damage.Attacker;
			controller->LastAttackAge = 0.0f;
			controller->LastAbility = uint8_t(CreditOf(AbilityId(damage.Ability)));
		}
		const Transform* transform = entities.TryGet<Transform>(target);
		const Replicated* replicated = entities.TryGet<Replicated>(target);
		if (transform && replicated)
			world.RecordHit({ replicated->NetId, transform->Position, uint16_t(std::min(amount, 65535)), damage.Critical, controller != nullptr, damage.Damages, damage.Attacker });
		if (controller)
			world.RecordDamage(controller->PlayerId, damage.Attacker, std::min(amount, std::max(health->Current, 0)), damage.Ability);

		health->Current -= amount;
		if (controller && world.IsDamageBrawl())
			world.AfterBrawlDamage(target, controller->PlayerId, damage.Attacker, damage.Ability, amount);
		if (Dummy* dummy = entities.TryGet<Dummy>(target))
		{
			dummy->SinceHurt = 0.0f;
			dummy->LastAttacker = damage.Attacker;
			health->Current = std::max(health->Current, 0);
			return true;
		}
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

	bool DealDamage(World& world, Entity target, const HitInfo& struck)
	{
		EntityRegistry& entities = world.GetEntities();
		const bool pvp = struck.Attacker != NoPlayer && world.IsPvp() && entities.Has<PlayerController>(target);
		const bool critical = struck.CritChance > 0.0f && world.GetRandom().Float() < struck.CritChance;
		const float crit = critical ? (pvp && !world.IsDamageBrawl() ? 1.0f + (struck.CritMultiplier - 1.0f) * PvpCritScale : struck.CritMultiplier) : 1.0f;
		int32_t damage = StruckDamage(world, target, struck, crit);
		HitInfo hit = struck;
		if (Ward(world, target, struck, damage, hit) == GuardOutcome::Held)
			return true;
		const HitInfo warded = hit;
		if (Guard(world, target, warded, damage, hit) == GuardOutcome::Held)
			return true;
		if (const PlayerController* victim = entities.TryGet<PlayerController>(target); victim && victim->State.Alive && victim->State.Invulnerable <= 0.0f)
			world.NoteBlow(victim->PlayerId, hit.Attacker, hit.Ability, hit.Mode == PushMode::Pull, hit.Returned);
		if (damage <= 0)
		{
			if (!ApplyKnockback(world, target, hit, true))
				return false;
		}
		else if (!ApplyDamage(world, target, { .Amount = damage, .Attacker = hit.Attacker, .Damages = hit.Damages, .Ability = hit.Ability, .Critical = critical, .Pvp = pvp }))
			return false;
		else if (world.UsesBrawlDamage())
			ApplyKnockback(world, target, hit, false);
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
			if (hit.Slick)
				state.Slick = std::max(state.Slick, BlackIceSeconds);
			if (hit.Static)
			{
				state.DodgeCooldown = std::max(state.DodgeCooldown, StaticSeconds);
				state.Static = StaticSeconds;
			}
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

	void StrikeCircle(World& world, Vec2 center, float radius, const HitInfo& hit, Team side, float rewind, std::span<const Entity> spared)
	{
		for (const Target& target : CollectTargets(world.GetEntities()))
		{
			Vec2 position;
			if (!IsHostile(world, side, hit.Attacker, target) || std::find(spared.begin(), spared.end(), target.Handle) != spared.end() || !TargetPosition(world, target, rewind, position))
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

	void DeflectProjectiles(World& world, const AbilityContext& context, float reach, float halfArc)
	{
		EntityRegistry& entities = world.GetEntities();
		const float facing = AngleOf(context.Direction);
		entities.Each<Projectile>([&](Entity entity, Projectile& projectile)
		{
			Transform* transform = entities.TryGet<Transform>(entity);
			Replicated* replicated = entities.TryGet<Replicated>(entity);
			if (!transform || !replicated || replicated->Owner == context.Owner || projectile.Source == context.Source)
				return;
			const Vec2 offset = transform->Position - context.Origin;
			const float distance = Length(offset);
			if (distance > reach + projectile.Radius || (distance > 0.3f && std::abs(AngleBetween(facing, AngleOf(offset))) > halfArc))
				return;
			const float speed = Length(projectile.Velocity);
			Vec2 heading = Normalize(context.Direction);
			if (const Transform* caster = entities.TryGet<Transform>(projectile.Source); caster && entities.Has<PlayerController>(projectile.Source))
				if (const Vec2 toward = caster->Position - transform->Position; LengthSquared(toward) > 0.25f)
					heading = Normalize(toward);
			projectile.Velocity = heading * speed;
			projectile.Lifetime = projectile.Span > 0.0f ? projectile.Span : GetProjectileLifetime(GetAbility(AbilityId(projectile.Ability)), speed);
			CancelRecast(world, projectile.Source, AbilityId(projectile.Ability));
			projectile.Source = context.Source;
			projectile.Rewind = 0.0f;
			projectile.Floor = LaunchFloor(world.GetLevel(), transform->Position, projectile.Floor);
			projectile.Hits = {};
			projectile.HitCount = 0;
			transform->Facing = AngleOf(heading);
			replicated->Owner = context.Owner;
			replicated->Tag = 0;
		});
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
			bool Guarded = false;
			bool Warded = false;
		};
		std::vector<Contact> contacts;
		struct Burst
		{
			Projectile Source;
			Vec2 Position;
			uint32_t Owner = NoPlayer;
		};
		std::vector<Burst> bursts;
		struct Hooked
		{
			Vec2 Position;
			float Facing = 0.0f;
			uint32_t Owner = NoPlayer;
			uint16_t Tag = 0;
			Entity Source;
			bool Caught = false;
		};
		std::vector<Hooked> hooks;

		entities.Each<Projectile>([&](Entity entity, Projectile& projectile)
		{
			Transform* transform = entities.TryGet<Transform>(entity);
			if (!transform)
				return;
			const Replicated* replicated = entities.TryGet<Replicated>(entity);
			const uint32_t owner = replicated ? replicated->Owner : NoPlayer;
			const uint16_t tag = replicated ? replicated->Tag : 0;
			const auto alreadyHit = [&](Entity target)
			{
				return std::find(projectile.Hits.begin(), projectile.Hits.begin() + projectile.HitCount, target) != projectile.Hits.begin() + projectile.HitCount;
			};
			if (projectile.Perks & PerkSeek)
				Seek(world, targets, projectile, transform->Position, owner, alreadyHit, dt);
			const Vec2 start = transform->Position;
			const Vec2 end = start + projectile.Velocity * dt;
			const float travel = CastSegment(world.GetLevel(), start, end, Block::Projectiles, &projectile.Floor);
			const Vec2 reached = Lerp(start, end, travel);
			transform->Position = reached;
			transform->Facing = AngleOf(projectile.Velocity);
			const bool clear = travel >= 1.0f;
			projectile.Lifetime -= dt;
			const bool hook = projectile.Ability == uint8_t(AbilityId::ChainHook);

			contacts.clear();
			for (const Target& target : targets)
			{
				Vec2 position;
				float parameter = 0.0f;
				if (!IsHostile(world, projectile.Side, owner, target) || alreadyHit(target.Handle) || !TargetPosition(world, target, projectile.Rewind, position))
					continue;
				if (target.Warded && SegmentTouchesCircle(start, reached, position, std::max(target.Radius, DiamondWardReach) + projectile.Radius, parameter))
					contacts.push_back({ target.Handle, parameter, false, true });
				else if (target.Guarded && GuardContact(start, reached, position, target.Facing, projectile.Radius, parameter))
					contacts.push_back({ target.Handle, parameter, true });
				else if (SegmentTouchesCircle(start, reached, position, target.Radius + projectile.Radius, parameter))
					contacts.push_back({ target.Handle, parameter });
			}
			std::sort(contacts.begin(), contacts.end(), [](const Contact& a, const Contact& b) { return a.Parameter < b.Parameter; });

			HitInfo hit = MakeHit(projectile, owner);
			if (hook)
				if (const Transform* caster = entities.TryGet<Transform>(projectile.Source))
				{
					hit.Mode = PushMode::Pull;
					hit.PushOrigin = caster->Position;
				}
			bool spent = false;
			bool reflected = false;
			Vec2 last = reached;
			for (const Contact& contact : contacts)
			{
				const Vec2 point = Lerp(start, reached, contact.Parameter);
				HitInfo struck = hit;
				struck.AtGuard = contact.Guarded;
				struck.AtWard = contact.Warded;
				struck.GuardPoint = point;
				if (contact.Warded && Reflect(world, entity, projectile, *transform, contact.Handle, point, struck))
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
				if (!DealDamage(world, contact.Handle, struck))
					continue;
				if (projectile.Side == Team::Players)
					world.GetStats().PlayerProjectileHits++;
				if (contact.Guarded || contact.Warded)
				{
					const PlayerController* guard = entities.TryGet<PlayerController>(contact.Handle);
					if (hook)
						hooks.push_back({ point, transform->Facing, owner, tag, projectile.Source, guard && (contact.Warded ? !IsWarded(guard->State) : !IsGuarding(guard->State, guard->Traits)) });
					if (projectile.HitCount < projectile.Hits.size())
						projectile.Hits[projectile.HitCount++] = contact.Handle;
					spent = true;
					last = point;
					break;
				}
				if (hook)
					hooks.push_back({ point, transform->Facing, owner, tag, projectile.Source, true });
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
					last = point;
					break;
				}
				projectile.Pierce--;
			}
			if (reflected)
				return;
			if ((projectile.Perks & PerkBurst) && (spent || !clear || projectile.Lifetime <= 0.0f))
				SpearBurst(world, projectile, last, owner);
			if (!spent && projectile.ImpactRadius > 0.0f && (!clear || projectile.Lifetime <= 0.0f))
				Explode(world, projectile, reached, owner, tag);
			else if (!spent && clear && projectile.Lifetime <= 0.0f && (projectile.Perks & PerkSplit))
				bursts.push_back({ projectile, reached, owner });
			if (hook && !spent && (!clear || projectile.Lifetime <= 0.0f))
				hooks.push_back({ reached, transform->Facing, owner, tag, projectile.Source, false });
			if (spent || !clear || projectile.Lifetime <= 0.0f)
				world.QueueDestroy(entity);
		});
		for (const Burst& burst : bursts)
			Split(world, burst.Source, burst.Position, burst.Owner);
		for (const Hooked& hooked : hooks)
		{
			world.SpawnEffect(hooked.Caught ? AbilityId::HookPull : AbilityId::HookAnchor, hooked.Position, hooked.Facing, hooked.Owner, hooked.Tag, 1.0f);
			if (hooked.Caught)
				CancelRecast(world, hooked.Source, AbilityId::ChainHook);
		}
	}

	void ReelIn(World& world, Entity caster, uint32_t owner, Vec2 point)
	{
		EntityRegistry& entities = world.GetEntities();
		uint16_t tag = 0;
		std::vector<Entity> gone;
		entities.Each<Projectile>([&](Entity entity, Projectile& projectile)
		{
			if (projectile.Ability != uint8_t(AbilityId::ChainHook) || projectile.Source != caster)
				return;
			if (const Replicated* replicated = entities.TryGet<Replicated>(entity))
				tag = replicated->Tag;
			gone.push_back(entity);
		});
		entities.Each<Replicated>([&](Entity entity, Replicated& replicated)
		{
			if (replicated.Kind != EntityKind::Effect || replicated.Variant != uint8_t(AbilityId::HookAnchor) || replicated.Owner != owner)
				return;
			tag = replicated.Tag;
			gone.push_back(entity);
		});
		for (const Entity entity : gone)
			world.DestroyEntity(entity);
		const Transform* transform = entities.TryGet<Transform>(caster);
		world.SpawnEffect(AbilityId::HookYank, point, transform ? AngleOf(point - transform->Position) : 0.0f, owner, tag, 1.0f);
	}

	void CancelRecast(World& world, Entity caster, AbilityId ability)
	{
		PlayerController* controller = world.GetEntities().TryGet<PlayerController>(caster);
		if (!controller || controller->State.RecastSlot < 0 || controller->Traits.Skills[size_t(controller->State.RecastSlot)].Ability != ability)
			return;
		controller->State.RecastTimer = 0.0f;
		controller->State.RecastSlot = -1;
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
				hit.Mode = landed.Push < 0.0f ? PushMode::Pull : (landed.Perks & PerkAlong) ? PushMode::Along : PushMode::Radial;
				hit.PushOrigin = center;
				hit.PushDirection = FromAngle(facing);
				hit.PushReach = landed.Radius;
				hit.Static = (landed.Perks & PerkStatic) != 0;
				const std::vector<Target> targets = CollectTargets(entities);
				std::vector<Entity> struck;
				for (const Target& target : targets)
				{
					Vec2 from;
					if (IsHostile(world, landed.Side, landed.Owner, target) && StrikeTouches(landed, center, facing, target.Position, target.Radius, from) && HasLineOfFire(world.GetLevel(), from, target.Position))
						if (DealDamage(world, target.Handle, hit))
							struck.push_back(target.Handle);
				}
				if (landed.Ability == uint8_t(AbilityId::StormStrike))
					StormJumps(world, targets, landed, center, hit, struck);
				if (landed.Perks & PerkShatter)
					world.AddCrater(center, landed.Radius * CraterScale);
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
			if (controller.RespawnTimer <= 0.0f && world.ConsumeRevive(controller.PlayerId))
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
