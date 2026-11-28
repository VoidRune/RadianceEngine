#include "Combat.h"
#include "GameShared/Content/Brawl.h"
#include "GameShared/Content/Pvp.h"
#include "GameShared/Simulation/FighterAI.h"
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
			if (const Enemy* enemy = entities.TryGet<Enemy>(entity))
				return enemy->Sprout <= 0.0f && enemy->Evade <= 0.0f;
			const PlayerController* controller = entities.TryGet<PlayerController>(entity);
			return !controller || (controller->State.Alive && controller->State.Invulnerable <= 0.0f);
		}

		bool IsEvading(const EntityRegistry& entities, Entity entity, const Health& health)
		{
			const PlayerController* controller = entities.TryGet<PlayerController>(entity);
			return controller && controller->State.Alive && controller->State.Invulnerable > 0.0f && !health.IsDead();
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
			bool Mirrored = false;
		};

		bool MakeTarget(EntityRegistry& entities, Entity entity, const Health& health, Target& target, bool evasion)
		{
			const Transform* transform = entities.TryGet<Transform>(entity);
			const Collider* collider = entities.TryGet<Collider>(entity);
			const PlayerController* controller = entities.TryGet<PlayerController>(entity);
			if (!transform || !collider || !(IsVulnerable(entities, entity, health) || (evasion && IsEvading(entities, entity, health))))
				return false;
			const Enemy* enemy = controller ? nullptr : entities.TryGet<Enemy>(entity);
			target = { entity, transform->Position, collider->Radius, health.Side, controller ? controller->PlayerId : NoPlayer, transform->Facing, controller ? IsGuarding(controller->State, controller->Traits) : enemy && enemy->GuardUp, controller && IsWarded(controller->State) && !controller->State.IsFalling(), enemy && IsMirrored(*enemy) };
			return true;
		}

		std::vector<Target> CollectTargets(EntityRegistry& entities, bool evasion)
		{
			std::vector<Target> targets;
			entities.Each<Health>([&](Entity entity, Health& health)
			{
				if (Target target; MakeTarget(entities, entity, health, target, evasion))
					targets.push_back(target);
			});
			return targets;
		}

		std::vector<Target> CollectTargets(World& world, Vec2 center, float reach)
		{
			thread_local std::vector<Entity> nearby;
			EntityRegistry& entities = world.GetEntities();
			world.GatherTargets(center - Vec2(reach, reach), center + Vec2(reach, reach), nearby);
			std::vector<Target> targets;
			for (const Entity entity : nearby)
				if (const Health* health = entities.TryGet<Health>(entity))
					if (Target target; MakeTarget(entities, entity, *health, target, world.IsShardfall()))
						targets.push_back(target);
			return targets;
		}

		class TargetSweep
		{
		public:
			void Build(const World& world, const std::vector<Target>& targets)
			{
				const float guard = GetAbility(AbilityId::HexShield).Reach;
				m_Grid.Begin(world.GetMap().GetWidth(), world.GetMap().GetHeight());
				for (const Target& target : targets)
				{
					Vec2 low;
					Vec2 high;
					world.GetTargetBounds(target.Handle, target.Position, std::max(target.Radius, target.Warded ? DiamondWardReach : target.Guarded ? guard : 0.0f), low, high);
					m_Grid.Add(low, high);
				}
				m_Grid.Finish();
			}

			const std::vector<uint32_t>& Near(Vec2 low, Vec2 high)
			{
				m_Grid.Gather(low, high, m_Found);
				return m_Found;
			}

			const std::vector<Target>& Pick(const std::vector<Target>& targets, Vec2 center, float reach)
			{
				m_Grid.Gather(center - Vec2(reach, reach), center + Vec2(reach, reach), m_Picked);
				m_Subset.clear();
				for (const uint32_t index : m_Picked)
					m_Subset.push_back(targets[index]);
				return m_Subset;
			}

		private:
			TargetGrid m_Grid;
			std::vector<uint32_t> m_Found;
			std::vector<uint32_t> m_Picked;
			std::vector<Target> m_Subset;
		};

		void BurstRuby(World& world, Vec2 center, float facing, const PendingStrike& strike)
		{
			const AbilityDef& shard = GetAbility(AbilityId::RubyShard);
			const ProjectilePattern pattern = GetProjectilePattern(shard, 0, 0, 1.0f);
			AbilityContext context;
			context.Side = Team::Enemies;
			context.Origin = center;
			for (uint32_t i = 0; i < RubyShards; i++)
			{
				context.Direction = FromAngle(facing + 2.0f * Pi * float(i) / float(RubyShards));
				world.SpawnProjectile(shard, pattern, center + context.Direction * 0.25f, context.Direction, strike.Damage, context);
			}
		}

		float StrikeExtent(const PendingStrike& strike)
		{
			float extent = std::max(strike.Radius, strike.Length + strike.Width * 0.5f);
			if (strike.Ability == uint8_t(AbilityId::StormStrike))
				extent += GetAbility(AbilityId::StormJump).Range;
			return extent;
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
			hit.Runes = context.Runes;
			hit.Depth = context.Depth;
			return hit;
		}

		HitInfo MakeHit(const Projectile& projectile, uint32_t owner)
		{
			HitInfo hit{ projectile.Damage, owner, projectile.Source, projectile.CritChance, projectile.CritMultiplier, projectile.LifeSteal, projectile.Damages, projectile.Chill, projectile.Stun, projectile.PvpScale, projectile.Ability };
			hit.Push = projectile.Push;
			hit.PushDirection = projectile.Velocity;
			hit.Slick = (projectile.Perks & PerkSlick) != 0;
			hit.Returned = projectile.Returned;
			hit.Runes = projectile.Runes;
			hit.Depth = projectile.Depth;
			return hit;
		}

		void Explode(World& world, const Projectile& projectile, Vec2 position, uint32_t owner, uint16_t tag)
		{
			HitInfo hit = MakeHit(projectile, owner);
			hit.Mode = PushMode::Radial;
			hit.PushOrigin = position;
			hit.PushReach = projectile.ImpactRadius;
			StrikeCircle(world, position, projectile.ImpactRadius, hit, projectile.Side, projectile.Rewind);
			const AbilityDef& ability = GetAbility(AbilityId(projectile.Ability));
			world.SpawnEffect(ability.Id, position, AngleOf(projectile.Velocity), owner, tag, ability.ImpactRadius > 0.0f ? projectile.ImpactRadius / ability.ImpactRadius : 1.0f, projectile.Charge);
			if (HasRune(projectile.Runes, RuneId::Linger))
				world.SpawnLinger(position, 0.0f, projectile.ImpactRadius * LingerRadius, projectile.Damage, projectile.Damages, owner, projectile.Source, projectile.Side, projectile.Runes, projectile.Depth);
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
				world.RecordHit({ .Target = replicated->NetId, .Position = transform->Position, .Damages = hit.Damages, .Ability = hit.Ability });
			if (dummy.Kind == DummyKind::Balloon)
			{
				world.PopBalloon(dummy, hit.Attacker);
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

		bool KnockEnemy(World& world, Entity target, Enemy& enemy, const HitInfo& hit)
		{
			if (!world.IsShardfall() || hit.Push == 0.0f || hit.Attacker >= MaxPlayers)
				return true;
			const Transform* transform = world.GetEntities().TryGet<Transform>(target);
			const float weight = EnemyWeight(enemy.Type) * (enemy.Champion ? ChampionWeight : enemy.Elite ? EliteWeight : 1.0f);
			if (!transform || weight <= 0.0f || enemy.Sprout > 0.0f)
				return true;
			Vec2 impulse = PushImpulse(hit, transform->Position) * (EnemyPushScale / weight);
			if (enemy.Stagger > 0.0f || enemy.Stun > 0.0f)
				impulse *= 1.4f;
			enemy.Push = ClampLength(enemy.Push + impulse, EnemyMaxPush);
			enemy.PushBy = hit.Attacker;
			if (Length(impulse) >= EnemyPushInterrupt && !enemy.Champion && enemy.Move >= 0)
			{
				enemy.Move = -1;
				enemy.Windup = 0.0f;
				enemy.DashLeft = 0.0f;
			}
			return true;
		}

		bool ApplyKnockback(World& world, Entity target, const HitInfo& hit, bool record)
		{
			EntityRegistry& entities = world.GetEntities();
			if (Dummy* dummy = entities.TryGet<Dummy>(target))
				return ShoveDummy(world, target, *dummy, hit, record);
			if (Enemy* enemy = entities.TryGet<Enemy>(target))
				return KnockEnemy(world, target, *enemy, hit);
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
				world.RecordHit({ .Target = replicated->NetId, .Position = transform->Position, .OnPlayer = true, .Damages = hit.Damages, .Ability = hit.Ability });
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
			if (Length(impulse) >= SwordBreakPush && InSwordCombo(state, controller->Traits))
				BreakSwordCombo(state, controller->Traits);
			return true;
		}

		enum class GuardOutcome : uint8_t
		{
			Open,
			Held,
			Broke,
		};

		bool FacesGuard(const HitInfo& hit, Vec2 position, float facing, Vec2& point)
		{
			const AbilityDef& shield = GetAbility(AbilityId::HexShield);
			point = hit.GuardPoint;
			if (hit.AtGuard)
				return true;
			const Vec2 incoming = hit.Mode == PushMode::Along ? hit.PushDirection * -1.0f : hit.PushOrigin - position;
			const float distance = Length(incoming);
			if (distance < 1e-4f || (hit.Mode != PushMode::Along && distance < HexShieldOverhead) || std::abs(AngleBetween(facing, AngleOf(incoming))) > shield.HalfArc)
				return false;
			point = position + incoming * (shield.Reach / distance);
			return true;
		}

		GuardOutcome GuardEnemy(World& world, Entity target, Enemy& enemy, const Transform& transform, const HitInfo& hit, int32_t& damage, HitInfo& through)
		{
			Vec2 point;
			if (!FacesGuard(hit, transform.Position, transform.Facing, point))
				return GuardOutcome::Open;
			const float wear = std::max(float(damage), 1.0f);
			const float left = enemy.GuardLeft;
			if (const Replicated* replicated = world.GetEntities().TryGet<Replicated>(target))
				world.RecordHit({ .Target = replicated->NetId, .Position = point, .Damage = uint16_t(std::lround(std::min(wear, left))), .Damages = hit.Damages, .Attacker = hit.Attacker, .Guarded = true, .Ability = hit.Ability });
			if (wear < left)
			{
				enemy.GuardLeft = left - wear;
				return GuardOutcome::Held;
			}
			BreakFighterGuard(enemy);
			through = hit;
			through.AtGuard = false;
			damage = damage > 0 ? std::max(1, int32_t(std::lround(float(damage) * (wear - left) / wear))) : 0;
			return GuardOutcome::Broke;
		}

		GuardOutcome Guard(World& world, Entity target, const HitInfo& hit, int32_t& damage, HitInfo& through)
		{
			EntityRegistry& entities = world.GetEntities();
			PlayerController* controller = entities.TryGet<PlayerController>(target);
			const Transform* transform = entities.TryGet<Transform>(target);
			if (Enemy* enemy = controller ? nullptr : entities.TryGet<Enemy>(target); enemy && enemy->GuardUp && transform)
				return GuardEnemy(world, target, *enemy, *transform, hit, damage, through);
			if (!controller || !transform || !IsGuarding(controller->State, controller->Traits) || controller->State.Invulnerable > 0.0f || controller->State.IsFalling())
				return GuardOutcome::Open;
			const Vec2 position = transform->Position;
			Vec2 point;
			if (!FacesGuard(hit, position, transform->Facing, point))
				return GuardOutcome::Open;
			PlayerState& state = controller->State;
			const PlayerTraits& traits = controller->Traits;
			const bool brawlDamage = world.UsesBrawlDamage();
			const float wear = brawlDamage ? std::max(float(damage), 1.0f) : std::max(Length(PushImpulse(hit, position)), HexShieldMinWear);
			const float left = GuardLeft(state, traits);
			const float absorbed = std::min(wear, left);
			state.GuardLost += absorbed;
			if (brawlDamage && !world.IsShardfall())
				state.Mana = std::max(state.Mana - absorbed * HexShieldManaPerPoint, 0.0f);
			if (const Replicated* replicated = entities.TryGet<Replicated>(target))
				world.RecordHit({ replicated->NetId, point, uint16_t(std::lround(absorbed)), false, true, hit.Damages, hit.Attacker, true, false, hit.Ability });
			if (wear <= left)
			{
				if (hit.Push != 0.0f && hit.Mode != PushMode::Pull)
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
			if (HasCard(world.GetCards(controller->PlayerId), CardId::ThornedMirror))
			{
				projectile.Push *= ThornScale;
				if (world.UsesBrawlDamage())
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

		bool MirrorProjectile(World& world, Entity entity, Projectile& projectile, Transform& transform, Entity target, Vec2 point)
		{
			EntityRegistry& entities = world.GetEntities();
			if (projectile.Reflections >= MirrorReflects)
				return false;
			const float speed = Length(projectile.Velocity);
			Vec2 heading = Normalize(projectile.Velocity) * -1.0f;
			if (const Transform* caster = entities.TryGet<Transform>(projectile.Source); caster && entities.Has<PlayerController>(projectile.Source))
				if (const Vec2 toward = caster->Position - point; LengthSquared(toward) > 0.25f)
					heading = Normalize(toward);
			CancelRecast(world, projectile.Source, AbilityId(projectile.Ability));
			if (const Replicated* mirror = entities.TryGet<Replicated>(target))
				world.RecordHit({ .Target = mirror->NetId, .Position = point, .Damages = projectile.Damages, .Guarded = true, .Ability = projectile.Ability });
			projectile.Side = Team::Enemies;
			const Enemy* mirror = entities.TryGet<Enemy>(target);
			projectile.Damage = std::clamp(ScaleDamage(projectile.Damage, MirrorDamage), 1, std::max(1, ScaleDamage(MirrorMaxDamage, mirror ? mirror->DamageScale : 1.0f)));
			projectile.Velocity = heading * speed;
			projectile.Lifetime = projectile.Span > 0.0f ? projectile.Span : GetProjectileLifetime(GetAbility(AbilityId(projectile.Ability)), speed);
			projectile.Source = target;
			projectile.CritChance = 0.0f;
			projectile.LifeSteal = 0.0f;
			projectile.Chain = 0;
			projectile.Pierce = 0;
			projectile.Perks = 0;
			projectile.Returned = NoPlayer;
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
				replicated->Owner = NoPlayer;
				replicated->Tag = 0;
			}
			return true;
		}

		float ChainReach(const Projectile& projectile)
		{
			return ChainRange * ((projectile.Perks & PerkReach) ? OverchargeReach : 1.0f);
		}

		bool Chain(World& world, const std::vector<Target>& targets, const Projectile& projectile, Entity hit, Vec2 from, uint32_t owner)
		{
			const Target* best = nullptr;
			float bestDistance = ChainReach(projectile) * ChainReach(projectile);
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
				if (world.UsesBrawlDamage())
					next.Damage = ScaleDamage(projectile.Damage, PinballGrowth);
			}
			if (next.HitCount < next.Hits.size())
				next.Hits[next.HitCount++] = hit;
			const Vec2 direction = Normalize(best->Position - from);
			const float speed = Length(projectile.Velocity);
			next.Velocity = direction * speed;
			next.Lifetime = ChainReach(projectile) / std::max(speed, 1e-3f);
			world.SpawnProjectileCopy(next, from + direction * 0.3f, owner);
			return true;
		}

		void StormJumps(World& world, const std::vector<Target>& targets, const PendingStrike& strike, Vec2 center, const HitInfo& base, std::vector<Entity>& struck)
		{
			const AbilityDef& jump = GetAbility(AbilityId::StormJump);
			const float scale = strike.Push / std::max(GetAbility(AbilityId::StormStrike).Push, 1e-3f);
			const Target* best = nullptr;
			float bestDistance = jump.Range * jump.Range;
			for (const Target& target : targets)
			{
				const float distance = LengthSquared(target.Position - center);
				if (distance >= bestDistance || !IsHostile(world, strike.Side, strike.Owner, target) || std::find(struck.begin(), struck.end(), target.Handle) != struck.end() || !HasLineOfFire(world.GetLevel(), center, target.Position))
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
			hit.PushDirection = best->Position - center;
			DealDamage(world, best->Handle, hit);
			struck.push_back(best->Handle);
			world.SpawnEffect(AbilityId::StormJump, best->Position, AngleOf(best->Position - center), strike.Owner, 0, std::sqrt(bestDistance));
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

		void Shatter(World& world, const Projectile& projectile, Vec2 position, uint32_t owner, Entity struck, bool blocked)
		{
			const float speed = Length(projectile.Velocity);
			if (speed <= 0.0f)
				return;
			const float heading = AngleOf(projectile.Velocity) + (blocked ? Pi : 0.0f);
			const Vec2 origin = blocked ? position - projectile.Velocity * (0.25f / speed) : position;
			for (uint32_t i = 0; i < SplitShards; i++)
			{
				Projectile shard = projectile;
				shard.Velocity = FromAngle(heading + RuneSplitSpread * (float(i) - float(SplitShards - 1) * 0.5f)) * speed;
				shard.Lifetime = RuneSplitRange / speed;
				shard.Span = shard.Lifetime;
				shard.Push = projectile.Push * RuneSplitShare;
				shard.Damage = std::max(1, ScaleDamage(projectile.Damage, RuneSplitShare));
				shard.Radius = projectile.Radius * SplitRadiusScale;
				shard.ImpactRadius = projectile.ImpactRadius * 0.6f;
				shard.Perks = uint8_t(projectile.Perks & PerkSeek);
				shard.Runes = projectile.Runes & (TriggerRunes | RuneBit(RuneId::Homing));
				shard.Pierce = 0;
				shard.Chain = 0;
				shard.Bounces = 0;
				shard.Hits = {};
				shard.HitCount = 0;
				if (struck.IsValid())
					shard.Hits[shard.HitCount++] = struck;
				shard.Rewind = 0.0f;
				world.SpawnProjectileCopy(shard, origin, owner, SplitRadiusScale);
			}
		}

		bool Rebound(const Level& level, Projectile& projectile, Vec2 start, Vec2 end, float travel)
		{
			const Vec2 step = end - start;
			const Vec2 from = Lerp(start, end, std::max(travel - 0.1f, 0.0f));
			float floor = projectile.Floor;
			const bool wallX = CastSegment(level, from, from + Vec2(step.x, 0.0f), Block::Projectiles, &floor) < 1.0f;
			floor = projectile.Floor;
			const bool wallY = CastSegment(level, from, from + Vec2(0.0f, step.y), Block::Projectiles, &floor) < 1.0f;
			if (wallX || !wallY)
				projectile.Velocity.x = -projectile.Velocity.x;
			if (wallY || !wallX)
				projectile.Velocity.y = -projectile.Velocity.y;
			projectile.Bounces--;
			projectile.Lifetime = std::max(projectile.Lifetime, projectile.Span * 0.6f);
			projectile.HitCount = 0;
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
		{
			amount = world.ShapeBrawlDamage(controller->State, controller->PlayerId, damage.Attacker, damage.Ability, damage.Unmitigated, controller->State.Chill > 0.0f, amount);
			if (amount <= 0)
			{
				const Transform* body = entities.TryGet<Transform>(target);
				const Replicated* replicated = entities.TryGet<Replicated>(target);
				if (body && replicated)
					world.RecordHit({ .Target = replicated->NetId, .Position = body->Position, .Damage = 0, .OnPlayer = true, .Guarded = true });
				return true;
			}
		}
		if (world.IsShardfall())
		{
			if (controller)
			{
				amount = world.ShapeRunTaken(target, *controller, damage.Unmitigated, amount);
				if (amount <= 0)
					return true;
			}
			else if (Enemy* struck = entities.TryGet<Enemy>(target); struck && damage.Attacker < MaxPlayers)
			{
				amount = ShieldEnemy(world, target, *struck, world.ShapeRunDealt(target, *struck, damage.Attacker, damage.Ability, amount), damage.Damages, damage.Attacker, damage.Ability);
				if (amount <= 0)
					return true;
			}
		}
		if (const Enemy* shielded = entities.TryGet<Enemy>(target); shielded && shielded->Shielded)
			amount = std::max(1, int32_t(std::lround(float(amount) * ShieldScale)));
		if (controller && damage.Attacker != NoPlayer && damage.Attacker != controller->PlayerId)
		{
			controller->LastAttacker = damage.Attacker;
			controller->LastAttackAge = 0.0f;
			controller->LastAbility = uint8_t(CreditOf(AbilityId(damage.Ability)));
		}
		const Transform* transform = entities.TryGet<Transform>(target);
		const Replicated* replicated = entities.TryGet<Replicated>(target);
		if (transform && replicated)
			world.RecordHit({ .Target = replicated->NetId, .Position = transform->Position, .Damage = uint16_t(std::min(amount, 65535)), .Critical = damage.Critical, .OnPlayer = controller != nullptr, .Damages = damage.Damages, .Attacker = damage.Attacker, .Ability = damage.Ability, .Killed = entities.Has<Enemy>(target) && amount >= health->Current });
		if (controller)
			world.RecordDamage(controller->PlayerId, damage.Attacker, std::min(amount, std::max(health->Current, 0)), damage.Ability);

		const int32_t dealt = std::min(amount, std::max(health->Current, 0));
		if (!controller)
			world.NoteRunDamage(damage.Attacker, damage.Ability, dealt);
		health->Current -= amount;
		if (!controller && damage.Attacker < MaxPlayers)
			world.AfterRunDamage(target, damage, dealt, health->Current <= 0);
		if (controller && world.IsShardfall() && world.GetSettings().Run.Immortal)
			health->Current = std::max(health->Current, 1);
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
			world.OnEnemyKilled(target, enemy, position, attacker);
		}
		world.QueueDestroy(target);
		return true;
	}

	bool DealDamage(World& world, Entity target, const HitInfo& struck)
	{
		EntityRegistry& entities = world.GetEntities();
		if (const PlayerController* dodger = entities.TryGet<PlayerController>(target); dodger && dodger->State.Invulnerable > 0.0f && world.IsShardfall())
		{
			world.NotePerfectDodge(target, dodger->PlayerId);
			return false;
		}
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
		if (const PlayerController* crystal = entities.TryGet<PlayerController>(target); crystal && crystal->State.Alive && crystal->State.Invulnerable <= 0.0f && hit.Attacker != NoPlayer && hit.Attacker != crystal->PlayerId && world.BreakCrystalWard(target, crystal->PlayerId))
			return true;
		if (const PlayerController* victim = entities.TryGet<PlayerController>(target); victim && victim->State.Alive && victim->State.Invulnerable <= 0.0f)
			world.NoteBlow(victim->PlayerId, hit.Attacker, hit.Ability, hit.Returned);
		if (damage <= 0)
		{
			if (!ApplyKnockback(world, target, hit, true))
				return false;
		}
		else if (!ApplyDamage(world, target, { .Amount = damage, .Attacker = hit.Attacker, .Damages = hit.Damages, .Ability = hit.Ability, .Critical = critical, .Pvp = pvp, .Runes = hit.Runes, .Depth = hit.Depth }))
			return false;
		else if (world.UsesBrawlDamage())
			ApplyKnockback(world, target, hit, false);
		if (Enemy* enemy = entities.TryGet<Enemy>(target))
		{
			const bool boss = entities.Has<Boss>(target);
			enemy->Chill = std::max(enemy->Chill, hit.Chill * (boss ? BossChillScale : 1.0f) * world.RunChillScale(hit.Attacker));
			if (hit.Stun > 0.0f && !boss)
			{
				enemy->Stun = std::max(enemy->Stun, hit.Stun);
				enemy->Windup = 0.0f;
			}
			StrikeFighter(world, target, *enemy, hit, damage);
		}
		else if (PlayerController* controller = entities.TryGet<PlayerController>(target); controller && controller->State.Alive)
		{
			PlayerState& state = controller->State;
			if (hit.Chill > 0.0f)
				state.Chill = std::max(state.Chill, PvpChillDuration(hit.Chill) * (world.IsDamageBrawl() && world.GetMatch().HasPlayerCard(hit.Attacker, CardId::Brittle) ? BrittleChill : 1.0f));
			if (hit.Attacker != NoPlayer && hit.Attacker != controller->PlayerId)
				world.NoteSpellHit(hit.Attacker, hit.Ability);
			if (damage > 0 && world.IsShardfall())
			{
				FeedVampire(world, hit.Source);
				world.NoteRunHurt(controller->PlayerId, hit.Source);
			}
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
		for (const Target& target : CollectTargets(world, center, radius))
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
		const bool sword = CreditOf(ability.Id) == AbilityId::CrystalSword;
		bool blocked = false;
		for (const Target& target : CollectTargets(world, context.Origin, reach))
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
			if (!HasLineOfFire(world.GetLevel(), context.Origin, position))
				continue;
			if (const PlayerController* victim = sword ? world.GetEntities().TryGet<PlayerController>(target.Handle) : nullptr)
				blocked = blocked || IsWarded(victim->State) || (IsGuarding(victim->State, victim->Traits) && std::abs(AngleBetween(target.Facing, AngleOf(context.Origin - position))) <= GetAbility(AbilityId::HexShield).HalfArc);
			DealDamage(world, target.Handle, hit);
		}
		if (blocked)
			if (PlayerController* caster = world.GetEntities().TryGet<PlayerController>(context.Source))
				BreakSwordCombo(caster->State, caster->Traits);
	}

	namespace
	{
		void ShatterThrow(World& world, const Projectile& projectile, Vec2 position, uint32_t owner, uint16_t tag)
		{
			const AbilityDef& shatter = GetAbility(AbilityId::SwordShatter);
			if ((projectile.Perks & PerkShatter) == 0)
			{
				world.SpawnEffect(AbilityId::SwordShatter, position, AngleOf(projectile.Velocity), owner, tag, 1.0f);
				return;
			}
			AbilityContext shards;
			shards.Side = projectile.Side;
			shards.Owner = owner;
			shards.Source = projectile.Source;
			shards.PvpScale = projectile.PvpScale;
			world.SpawnCardStrike(AbilityId::SwordShatter, position, AngleOf(projectile.Velocity), 0.0f, shatter.ImpactRadius, shatter.Push, 0, false, shards);
		}
	}

	void UpdateProjectiles(World& world, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		const std::vector<Target> targets = CollectTargets(entities, world.IsShardfall());
		thread_local TargetSweep sweep;
		sweep.Build(world, targets);
		struct Contact
		{
			Entity Handle;
			float Parameter = 0.0f;
			bool Guarded = false;
			bool Warded = false;
			bool Mirrored = false;
		};
		std::vector<Contact> contacts;
		struct Burst
		{
			Projectile Source;
			Vec2 Position;
			uint32_t Owner = NoPlayer;
			bool Rune = false;
			Entity Struck;
			bool Blocked = false;
		};
		std::vector<Burst> bursts;

		struct Parry
		{
			Vec2 Center;
			float Facing = 0.0f;
			float Reach = 0.0f;
			uint32_t Owner = NoPlayer;
		};
		std::vector<Parry> parries;
		entities.Each<PlayerController>([&](Entity player, PlayerController& controller)
		{
			const Transform* place = entities.TryGet<Transform>(player);
			const PlayerState& state = controller.State;
			if (!place || !state.Alive || state.CastSlot < 0 || state.CastTimer > RiposteWindow || controller.Traits.Skills[size_t(state.CastSlot)].Ability != AbilityId::CrystalSword || !HasCard(world.GetCards(controller.PlayerId), CardId::Riposte))
				return;
			parries.push_back({ place->Position, place->Facing, RiposteReach * controller.Traits.Skills[size_t(state.CastSlot)].AreaScale, controller.PlayerId });
		});
		std::vector<BodyCircle> walls;
		world.CollectWalls(walls);
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
				Seek(world, sweep.Pick(targets, transform->Position, SeekRange), projectile, transform->Position, owner, alreadyHit, dt);
			const Vec2 start = transform->Position;
			const Vec2 end = start + projectile.Velocity * dt;
			float travel = CastSegment(world.GetLevel(), start, end, Block::Projectiles, &projectile.Floor);
			const float barrier = walls.empty() ? 1.0f : CastBodies(start, end, projectile.Radius, walls);
			const bool barred = barrier < travel;
			travel = std::min(travel, barrier);
			const Vec2 reached = Lerp(start, end, travel);
			transform->Position = reached;
			transform->Facing = AngleOf(projectile.Velocity);
			const bool walled = travel < 1.0f;
			const bool clear = !walled || (!barred && projectile.Bounces > 0 && Rebound(world.GetLevel(), projectile, start, end, travel));
			projectile.Lifetime -= dt;
			bool parried = false;
			for (const Parry& parry : parries)
			{
				float parameter = 0.0f;
				if (parry.Owner == owner || (projectile.Side == Team::Players && !world.AreRivals(owner, parry.Owner)) || !SegmentTouchesCircle(start, reached, parry.Center, parry.Reach + projectile.Radius, parameter))
					continue;
				const Vec2 point = Lerp(start, reached, parameter);
				if (std::abs(AngleBetween(parry.Facing, AngleOf(point - parry.Center))) > RiposteArc)
					continue;
				world.SpawnEffect(AbilityId::SwordShatter, point, AngleOf(projectile.Velocity), parry.Owner, 0, 0.3f);
				world.QueueDestroy(entity);
				parried = true;
				break;
			}
			if (parried)
				return;

			contacts.clear();
			const Vec2 low = { std::min(start.x, reached.x) - projectile.Radius, std::min(start.y, reached.y) - projectile.Radius };
			const Vec2 high = { std::max(start.x, reached.x) + projectile.Radius, std::max(start.y, reached.y) + projectile.Radius };
			for (const uint32_t index : sweep.Near(low, high))
			{
				const Target& target = targets[index];
				Vec2 position;
				float parameter = 0.0f;
				if (!IsHostile(world, projectile.Side, owner, target) || alreadyHit(target.Handle) || !TargetPosition(world, target, projectile.Rewind, position))
					continue;
				if (target.Mirrored && SegmentTouchesCircle(start, reached, position, target.Radius + projectile.Radius, parameter))
					contacts.push_back({ target.Handle, parameter, false, false, true });
				else if (target.Warded && SegmentTouchesCircle(start, reached, position, std::max(target.Radius, DiamondWardReach) + projectile.Radius, parameter))
					contacts.push_back({ target.Handle, parameter, false, true });
				else if (target.Guarded && GuardContact(start, reached, position, target.Facing, projectile.Radius, parameter))
					contacts.push_back({ target.Handle, parameter, true });
				else if (SegmentTouchesCircle(start, reached, position, target.Radius + projectile.Radius, parameter))
					contacts.push_back({ target.Handle, parameter });
			}
			std::sort(contacts.begin(), contacts.end(), [](const Contact& a, const Contact& b) { return a.Parameter < b.Parameter; });

			const HitInfo hit = MakeHit(projectile, owner);
			bool spent = false;
			bool reflected = false;
			Vec2 last = reached;
			Entity victim;
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
				if (contact.Mirrored && MirrorProjectile(world, entity, projectile, *transform, contact.Handle, point))
				{
					reflected = true;
					break;
				}
				if (projectile.ImpactRadius > 0.0f)
				{
					Explode(world, projectile, point, owner, tag);
					spent = true;
					last = point;
					victim = contact.Handle;
					break;
				}
				if (!DealDamage(world, contact.Handle, struck))
					continue;
				if (projectile.Side == Team::Players)
					world.GetStats().PlayerProjectileHits++;
				if (contact.Guarded || contact.Warded)
				{
					if (projectile.HitCount < projectile.Hits.size())
						projectile.Hits[projectile.HitCount++] = contact.Handle;
					spent = true;
					last = point;
					break;
				}
				if (projectile.Chain > 0 && Chain(world, sweep.Pick(targets, point, ChainReach(projectile)), projectile, contact.Handle, point, owner))
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
					victim = contact.Handle;
					break;
				}
				projectile.Pierce--;
			}
			if (reflected)
				return;
			if (!spent && (!clear || projectile.Lifetime <= 0.0f) && projectile.Side == Team::Players && projectile.ImpactRadius <= 0.0f && (projectile.Perks & (PerkDebt | PerkBurst | PerkSplit)) == 0)
				if (const float debt = world.RunSigilPower(owner, SigilId::MirrorDebt); debt > 0.0f)
				{
					projectile.Perks |= PerkDebt;
					projectile.Velocity = projectile.Velocity * -1.0f;
					projectile.Lifetime = std::max(projectile.Span, 0.5f);
					projectile.HitCount = 0;
					if (debt > 1.0f)
						projectile.Damage *= 2;
					return;
				}
			if ((projectile.Perks & PerkBurst) && (spent || !clear || projectile.Lifetime <= 0.0f))
				SpearBurst(world, projectile, last, owner);
			if (!spent && projectile.ImpactRadius > 0.0f && (!clear || projectile.Lifetime <= 0.0f))
				Explode(world, projectile, reached, owner, tag);
			else if (!spent && clear && projectile.Lifetime <= 0.0f && (projectile.Perks & PerkSplit))
				bursts.push_back({ projectile, reached, owner });
			if (HasRune(projectile.Runes, RuneId::Split) && (spent || !clear || projectile.Lifetime <= 0.0f))
				bursts.push_back({ projectile, spent ? last : reached, owner, true, victim, !spent && !clear });
			if (projectile.Ability == uint8_t(AbilityId::SwordThrow) && (spent || !clear || projectile.Lifetime <= 0.0f))
				ShatterThrow(world, projectile, spent ? last : reached, owner, tag);
			if (projectile.Ability == uint8_t(AbilityId::FrostSpear) && (spent || !clear || projectile.Lifetime <= 0.0f))
				world.AddCrust(last, projectile.Pierce > 0 || projectile.Radius > GetAbility(AbilityId::FrostSpear).ProjectileRadius + 0.05f ? SpearCrustCharged : SpearCrustRadius);
			if (spent || !clear || projectile.Lifetime <= 0.0f)
				world.QueueDestroy(entity);
		});
		for (const Burst& burst : bursts)
			if (burst.Rune)
				Shatter(world, burst.Source, burst.Position, burst.Owner, burst.Struck, burst.Blocked);
			else
				Split(world, burst.Source, burst.Position, burst.Owner);
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
				if (landed.Ability == uint8_t(AbilityId::RubyBurst))
				{
					BurstRuby(world, center, facing, landed);
					world.SpawnEffect(AbilityId::RubyBurst, center, facing, NoPlayer, 0, 1.0f);
					world.QueueDestroy(entity);
					return;
				}
				HitInfo hit{ landed.Damage, landed.Owner, landed.Source, landed.CritChance, landed.CritMultiplier, landed.LifeSteal, landed.Damages, landed.Chill, landed.Stun, landed.PvpScale, landed.Ability };
				hit.Push = landed.Push;
				hit.Mode = landed.Push < 0.0f ? PushMode::Pull : (landed.Perks & PerkAlong) ? PushMode::Along : PushMode::Radial;
				hit.PushOrigin = center;
				hit.PushDirection = FromAngle(facing);
				hit.PushReach = landed.Radius;
				hit.Static = (landed.Perks & PerkStatic) != 0;
				hit.Runes = landed.Runes;
				hit.Depth = landed.Depth;
				const std::vector<Target> targets = CollectTargets(world, center, StrikeExtent(landed));
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
			const Transform* transform = entities.TryGet<Transform>(entity);
			if (loot.Lifetime <= 0.0f || !transform)
			{
				world.QueueDestroy(entity);
				return;
			}
			for (const Picker& player : players)
			{
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
