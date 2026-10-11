#include "EnemyAI.h"
#include "GameShared/Content/Definitions.h"
#include "GameShared/Content/Missions.h"
#include "GameShared/Simulation/Abilities.h"
#include "GameShared/Simulation/World.h"
#include "GameShared/World/Collision.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <span>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr float ChillPace = 0.6f;
		constexpr float LeashSeconds = 6.0f;
		constexpr float LeashMargin = 10.0f;
		constexpr float BurrowSpeed = 8.0f;
		constexpr float BurrowLimit = 2.5f;
		constexpr uint32_t MaxMinions = 10;
		constexpr float PhaseThresholds[] = { 0.66f, 0.33f };

		enum class Move : uint8_t
		{
			Volley,
			PageStorm,
			ShelfCrush,
			ArcaneRing,
			Summon,
			InkSpiral,
			Spikes,
			TailSweep,
			Burrow,
			ShardBurst,
			CrystalRain,
			Brood,
		};

		struct MoveDef
		{
			Move Id = Move::Volley;
			float Cooldown = 3.0f;
			uint8_t MinPhase = 0;
			uint32_t Weight = 10;
			float MaxRange = 99.0f;
		};

		constexpr MoveDef ArchivistMoves[] = {
			{ Move::Volley, 3.0f, 0, 30, 13.0f },
			{ Move::PageStorm, 7.0f, 0, 22, 14.0f },
			{ Move::ShelfCrush, 6.0f, 0, 22, 12.0f },
			{ Move::ArcaneRing, 11.0f, 1, 20, 12.0f },
			{ Move::Summon, 20.0f, 1, 12 },
			{ Move::InkSpiral, 14.0f, 2, 18 },
		};

		constexpr MoveDef WyrmMoves[] = {
			{ Move::Spikes, 3.5f, 0, 30, 11.0f },
			{ Move::TailSweep, 4.0f, 0, 26, 6.5f },
			{ Move::Burrow, 9.0f, 0, 18 },
			{ Move::ShardBurst, 7.0f, 1, 20 },
			{ Move::CrystalRain, 9.0f, 1, 18 },
			{ Move::Brood, 22.0f, 2, 10 },
		};

		struct Context
		{
			World& Sim;
			Entity Self;
			Transform& Body;
			Enemy& Mind;
			Boss& State;
			const EnemyDef& Def;
			float Radius = 0.0f;
			std::vector<Vec2> Players;
			Vec2 Target;
			bool Repeat = false;
		};

		std::span<const MoveDef> GetMoves(EntityKind kind)
		{
			if (kind == EntityKind::Wyrm)
				return WyrmMoves;
			return ArchivistMoves;
		}

		AbilityContext MakeContext(const Context& context, Vec2 direction)
		{
			AbilityContext ability;
			ability.Origin = context.Body.Position;
			ability.Direction = direction;
			ability.Side = Team::Enemies;
			ability.Source = context.Self;
			ability.DamageScale = context.Mind.DamageScale;
			return ability;
		}

		void Strike(Context& context, AbilityId id, Vec2 position, Vec2 direction = { 1.0f, 0.0f })
		{
			const AbilityDef& ability = GetAbility(id);
			context.Sim.SpawnPendingStrike(ability, position, ScaleDamage(ability.Damage, context.Mind.DamageScale), MakeContext(context, direction));
		}

		void Fire(Context& context, AbilityId id, Vec2 direction)
		{
			const AbilityDef& ability = GetAbility(id);
			const Vec2 start = context.Body.Position + direction * ability.SpawnOffset;
			if (CastSegment(context.Sim.GetLevel(), context.Body.Position, start, Block::Projectiles) < 1.0f)
				return;
			const ProjectilePattern pattern{ .Count = 1, .Speed = ability.ProjectileSpeed };
			context.Sim.SpawnProjectile(ability, pattern, start, direction, ScaleDamage(ability.Damage, context.Mind.DamageScale), MakeContext(context, direction));
		}

		void Fan(Context& context, AbilityId id, float aim, uint32_t count, float spacing, float offset = 0.0f)
		{
			for (uint32_t i = 0; i < count; i++)
				Fire(context, id, FromAngle(aim + (float(i) - 0.5f * float(count - 1) + offset) * spacing));
		}

		Vec2 Aim(const Context& context)
		{
			const Vec2 direction = Normalize(context.Target - context.Body.Position);
			return LengthSquared(direction) > 0.0f ? direction : FromAngle(context.Body.Facing);
		}

		Vec2 ArenaSpot(Context& context, float minimum, float maximum)
		{
			Random& random = context.Sim.GetRandom();
			const Vec2 candidate = context.State.Arena + FromAngle(random.Float() * 2.0f * Pi) * std::lerp(minimum, maximum, random.Float()) * context.State.ArenaRadius;
			return IsFree(context.Sim.GetLevel(), candidate, 0.3f) ? candidate : context.Target;
		}

		Vec2 Near(Context& context, Vec2 center, float spread)
		{
			Random& random = context.Sim.GetRandom();
			const Vec2 candidate = center + FromAngle(random.Float() * 2.0f * Pi) * (spread * std::sqrt(random.Float()));
			return IsFree(context.Sim.GetLevel(), candidate, 0.3f) ? candidate : center;
		}

		void SetBurrowed(Context& context, bool burrowed)
		{
			context.State.Burrowed = burrowed;
			if (Replicated* replicated = context.Sim.GetEntities().TryGet<Replicated>(context.Self))
				replicated->Variant = burrowed ? 1 : 0;
		}

		void SummonMinions(Context& context, EntityKind kind, uint32_t count)
		{
			EntityRegistry& entities = context.Sim.GetEntities();
			uint32_t alive = 0;
			entities.Each<Enemy>([&](Entity entity, Enemy&) { alive += entities.Has<Boss>(entity) ? 0 : 1; });
			const WorldSettings& settings = context.Sim.GetSettings();
			const EnemyScaling scaling = GetMonsterScaling(kind, context.State.Level, std::max(context.Sim.CountPlayers(), 1u), false, settings.Modifiers);
			const EnemyDef* def = FindEnemyDef(kind);
			for (uint32_t i = 0; i < count && alive < MaxMinions; i++, alive++)
			{
				const Vec2 spot = context.Sim.FindFreeSpotNear(ArenaSpot(context, 0.55f, 0.85f), def ? def->Radius : 0.4f);
				context.Sim.SpawnEnemy(kind, spot, scaling);
				context.Sim.SpawnEffect(AbilityId::Eruption, spot, 0.0f, NoPlayer, 0, 0.35f);
			}
		}

		float RunArchivist(Context& context, Move move, uint8_t step)
		{
			const uint8_t phase = context.State.Phase;
			switch (move)
			{
			case Move::Volley:
				if (step == 0)
					return 0.5f;
				Fan(context, AbilityId::TomeVolley, AngleOf(Aim(context)), 5 + 2 * phase, 0.2f, step % 2 == 0 ? 0.5f : 0.0f);
				return step < 3 + phase ? 0.45f : -0.6f;
			case Move::PageStorm:
				if (step == 0)
					return 0.4f;
				for (Vec2 player : context.Players)
					Strike(context, AbilityId::PageStorm, step == 1 ? player : Near(context, player, 2.0f));
				for (uint32_t i = 0; i < 2u + phase; i++)
					Strike(context, AbilityId::PageStorm, Near(context, context.Target, 6.0f));
				return step < 3 ? 0.55f : -0.9f;
			case Move::ShelfCrush:
			{
				const float aim = AngleOf(Aim(context));
				const int32_t spread = phase >= 1 ? 2 : 1;
				const float shift = step == 0 ? 0.0f : 0.275f;
				for (int32_t i = -spread; i <= spread; i++)
					Strike(context, AbilityId::ShelfCrush, context.Body.Position, FromAngle(aim + float(i) * 0.55f + shift));
				if (step == 0 && phase >= 2)
					return 0.8f;
				return -1.3f;
			}
			case Move::ArcaneRing:
				if (step == 0)
				{
					Strike(context, AbilityId::ArcaneRing, context.Body.Position);
					return 1.6f;
				}
				Strike(context, AbilityId::ArcaneBurst, context.Body.Position);
				return -1.6f;
			case Move::Summon:
				SummonMinions(context, EntityKind::Chaser, 2 + phase);
				SummonMinions(context, EntityKind::Shooter, 1);
				return -1.0f;
			case Move::InkSpiral:
			{
				const uint32_t arms = 3 + (phase >= 2 ? 1 : 0);
				for (uint32_t arm = 0; arm < arms; arm++)
					Fire(context, AbilityId::InkSpiral, FromAngle(float(step) * 0.32f + float(arm) * 2.0f * Pi / float(arms)));
				return step < 24 ? 0.13f : -1.0f;
			}
			default:
				return -0.5f;
			}
		}

		float RunWyrm(Context& context, Move move, uint8_t step, float dt)
		{
			const uint8_t phase = context.State.Phase;
			switch (move)
			{
			case Move::Spikes:
				if (phase >= 2 && step == 0)
				{
					const float aim = AngleOf(Aim(context));
					for (int32_t i = -2; i <= 2; i++)
						Strike(context, AbilityId::CrystalSpike, context.Body.Position, FromAngle(aim + float(i) * 0.4f));
					return 0.7f;
				}
				Strike(context, AbilityId::CrystalSpike, context.Body.Position, Aim(context));
				return step < 2 ? 0.55f : -1.0f;
			case Move::TailSweep:
				if (step == 0)
				{
					Strike(context, AbilityId::TailSweep, context.Body.Position, Aim(context));
					return phase >= 1 ? 0.9f : -1.0f;
				}
				Strike(context, AbilityId::TailSweep, context.Body.Position, -Aim(context));
				return -1.0f;
			case Move::Burrow:
				switch (step)
				{
				case 0:
					SetBurrowed(context, true);
					context.State.Goal = context.Target;
					if (Length(context.State.Goal - context.State.Arena) > context.State.ArenaRadius - context.Radius)
						context.State.Goal = context.State.Arena + Normalize(context.State.Goal - context.State.Arena) * (context.State.ArenaRadius - context.Radius);
					context.State.StageTimer = 0.0f;
					return 0.3f;
				case 1:
				{
					context.State.StageTimer += dt;
					const Vec2 offset = context.State.Goal - context.Body.Position;
					if (Length(offset) > 0.3f && context.State.StageTimer < BurrowLimit)
					{
						context.Body.Position = MoveCircle(context.Sim.GetLevel(), context.Body.Position, context.Radius, ClampLength(offset, BurrowSpeed * dt));
						context.Repeat = true;
						return 0.0f;
					}
					Strike(context, AbilityId::Eruption, context.Body.Position);
					if (phase >= 2)
						for (uint32_t i = 0; i < 6; i++)
							Strike(context, AbilityId::CrystalRain, context.Body.Position + FromAngle(float(i) * Pi / 3.0f) * 4.6f);
					return 1.0f;
				}
				default:
					SetBurrowed(context, false);
					return -0.7f;
				}
			case Move::ShardBurst:
			{
				const uint32_t count = 16 + 4 * phase;
				for (uint32_t i = 0; i < count; i++)
					Fire(context, AbilityId::ShardBurst, FromAngle((float(i) + (step % 2 == 0 ? 0.0f : 0.5f)) * 2.0f * Pi / float(count)));
				return step < 1 + phase ? 0.6f : -0.9f;
			}
			case Move::CrystalRain:
				for (Vec2 player : context.Players)
					Strike(context, AbilityId::CrystalRain, step == 0 ? player : Near(context, player, 1.5f));
				for (uint32_t i = 0; i < 4u + 2u * phase; i++)
					Strike(context, AbilityId::CrystalRain, ArenaSpot(context, 0.0f, 1.0f));
				return step < 2 ? 0.5f : -1.0f;
			case Move::Brood:
				SummonMinions(context, EntityKind::Chaser, 3 + phase);
				return -1.0f;
			default:
				return -0.5f;
			}
		}

		bool PickMove(Context& context, std::span<const MoveDef> moves)
		{
			const float distance = Length(context.Target - context.Body.Position);
			uint32_t total = 0;
			for (size_t i = 0; i < moves.size(); i++)
				if (context.State.Phase >= moves[i].MinPhase && context.State.Cooldowns[i] <= 0.0f && distance <= moves[i].MaxRange)
					total += moves[i].Weight;
			if (total == 0)
				return false;
			uint32_t roll = context.Sim.GetRandom().Below(total);
			for (size_t i = 0; i < moves.size(); i++)
			{
				if (context.State.Phase < moves[i].MinPhase || context.State.Cooldowns[i] > 0.0f || distance > moves[i].MaxRange)
					continue;
				if (roll < moves[i].Weight)
				{
					context.State.Move = int8_t(i);
					context.State.Step = 0;
					context.State.StepTimer = 0.0f;
					return true;
				}
				roll -= moves[i].Weight;
			}
			return false;
		}

		void Reposition(Context& context, float pace, float dt)
		{
			const Level& level = context.Sim.GetLevel();
			const Vec2 toTarget = context.Target - context.Body.Position;
			const float distance = Length(toTarget);
			const float preferred = context.Def.Kind == EntityKind::Wyrm ? 3.2f : 7.5f;
			const Vec2 forward = distance > 1e-3f ? toTarget / distance : Vec2();
			const float orbit = (int32_t(context.Sim.GetTick() / 240 + context.Self.Index) % 2 == 0) ? 1.0f : -1.0f;
			Vec2 desired = forward * std::clamp((distance - preferred) * 0.6f, -1.0f, 1.0f) + Perpendicular(forward) * (0.45f * orbit);
			const Vec2 fromArena = context.Body.Position - context.State.Arena;
			if (Length(fromArena) > context.State.ArenaRadius - context.Radius)
				desired = -Normalize(fromArena);
			desired = ClampLength(desired, 1.0f);
			context.Body.Position = MoveCircle(level, context.Body.Position, context.Radius, desired * (context.Def.Speed * context.Mind.SpeedScale * pace * dt));
		}
	}

	void UpdateBoss(World& world, Entity entity, Transform& transform, Enemy& enemy, Boss& boss, const EnemyDef& def, float radius, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		Context context{ world, entity, transform, enemy, boss, def, radius };
		Vec2 nearest;
		float nearestDistance = -1.0f;
		entities.Each<PlayerController>([&](Entity player, PlayerController& controller)
		{
			const Transform* other = entities.TryGet<Transform>(player);
			if (!controller.State.Alive || !other)
				return;
			context.Players.push_back(other->Position);
			const float distance = LengthSquared(other->Position - transform.Position);
			if (nearestDistance < 0.0f || distance < nearestDistance)
			{
				nearestDistance = distance;
				nearest = other->Position;
			}
		});

		Health* health = entities.TryGet<Health>(entity);
		const bool engaged = nearestDistance >= 0.0f && Length(nearest - boss.Arena) < boss.ArenaRadius + LeashMargin;
		boss.Leash = engaged || boss.Burrowed ? 0.0f : boss.Leash + dt;
		if (boss.Leash > LeashSeconds && health)
		{
			health->Current = health->Max;
			boss = Boss{ .Arena = boss.Arena, .ArenaRadius = boss.ArenaRadius, .Level = boss.Level };
			transform.Position = MoveCircle(world.GetLevel(), transform.Position, radius, ClampLength(boss.Arena - transform.Position, def.Speed * dt));
			enemy.Windup = 0.0f;
			return;
		}
		if (nearestDistance < 0.0f && !boss.Burrowed)
		{
			enemy.Windup = 0.0f;
			return;
		}
		context.Target = nearestDistance >= 0.0f ? nearest : boss.Goal;

		enemy.Stun = 0.0f;
		enemy.Chill = std::max(enemy.Chill - dt, 0.0f);
		const float pace = enemy.Chill > 0.0f ? ChillPace : 1.0f;
		const std::span<const MoveDef> moves = GetMoves(def.Kind);
		for (size_t i = 0; i < moves.size(); i++)
			boss.Cooldowns[i] = std::max(boss.Cooldowns[i] - dt * pace, 0.0f);

		if (health && !boss.Burrowed)
		{
			const float fraction = float(health->Current) / float(std::max(health->Max, 1));
			uint8_t phase = 0;
			for (float threshold : PhaseThresholds)
				phase += fraction <= threshold ? 1 : 0;
			if (phase > boss.Phase)
			{
				boss.Phase = phase;
				const Move signature = def.Kind == EntityKind::Wyrm ? Move::Burrow : Move::ArcaneRing;
				for (size_t i = 0; i < moves.size(); i++)
					if (moves[i].Id == signature)
					{
						boss.Move = int8_t(i);
						boss.Step = 0;
						boss.StepTimer = 0.4f;
					}
			}
		}

		if (boss.Move < 0 && !engaged)
		{
			transform.Position = MoveCircle(world.GetLevel(), transform.Position, radius, ClampLength(boss.Arena - transform.Position, def.Speed * pace * dt));
			enemy.Windup = 0.0f;
			return;
		}
		if (boss.Move < 0)
		{
			Reposition(context, pace, dt);
			transform.Facing = AngleOf(Aim(context));
			enemy.Windup = 0.0f;
			boss.Idle -= dt * pace;
			if (boss.Idle > 0.0f || !PickMove(context, moves))
				return;
		}

		const MoveDef& move = moves[size_t(boss.Move)];
		if (!boss.Burrowed)
			transform.Facing = LerpAngle(transform.Facing, AngleOf(Aim(context)), std::min(1.0f, 6.0f * dt));
		enemy.Windup = boss.Step <= 1 ? std::max(boss.StepTimer, 0.05f) : 0.0f;
		boss.StepTimer -= dt * pace;
		if (boss.StepTimer > 0.0f)
			return;
		context.Repeat = false;
		const float next = def.Kind == EntityKind::Wyrm ? RunWyrm(context, move.Id, boss.Step, dt) : RunArchivist(context, move.Id, boss.Step);
		if (context.Repeat)
		{
			boss.StepTimer = 0.0f;
			return;
		}
		if (next >= 0.0f)
		{
			boss.Step++;
			boss.StepTimer = next;
			return;
		}
		boss.Cooldowns[size_t(boss.Move)] = move.Cooldown;
		boss.Move = -1;
		boss.Step = 0;
		boss.Idle = -next * (boss.Phase >= 2 ? 0.7f : boss.Phase == 1 ? 0.85f : 1.0f);
		enemy.Windup = 0.0f;
	}
}
