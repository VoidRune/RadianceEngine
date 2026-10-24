#include "PlayerSimulation.h"
#include "GameShared/Content/Brawl.h"
#include "GameShared/Content/Pvp.h"
#include "GameShared/World/Collision.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float ReadyEpsilon = 1e-4f;
		constexpr float LungeStandoff = 0.75f;
		constexpr uint32_t CastPriority[] = { 1, 2, 3, 0 };
		constexpr float ShoveOverVoid = 1.5f;
		constexpr float LedgeClearance = 0.8f;
		constexpr float FallDrag = 1.1f;
		constexpr float DashCarry = 0.35f;
		constexpr float FootingStep = 0.75f;
		constexpr float LiftRate = 7.5f;
		constexpr float LandingGrace = 0.4f;

		void Cool(float& timer, float dt)
		{
			timer = std::max(timer - dt, 0.0f);
		}

		float PushControl(const PlayerState& state)
		{
			return 1.0f - std::min(Length(state.Push) / PushControlSpeed, PushControlLoss);
		}

		void DecayPush(const Level& level, Vec2 position, PlayerState& state, float dt)
		{
			state.Push *= std::exp(-PushFriction * SurfaceFriction(level, position) * (state.Slick > 0.0f ? BlackIceFriction : 1.0f) * dt);
			if (LengthSquared(state.Push) < PushStop * PushStop)
				state.Push = {};
		}

		Vec2 MoveAndBrake(const Level& level, PlayerState& state, Vec2 position, Vec2 delta, std::span<const BodyCircle> bodies)
		{
			const bool shoved = level.Floating && LengthSquared(state.Push) > ShoveOverVoid * ShoveOverVoid;
			const Vec2 moved = MoveCircle(level, position, PlayerRadius, delta, bodies, shoved);
			const float wanted = LengthSquared(delta);
			if (wanted <= 1e-10f || LengthSquared(state.Push) <= 0.0f)
				return moved;
			const Vec2 blocked = position + delta - moved;
			if (level.Bouncy && LengthSquared(blocked) > 1e-8f && LengthSquared(state.Push) > BounceSpeed * BounceSpeed)
			{
				const Vec2 normal = Normalize(blocked);
				if (const float into = Dot(state.Push, normal); into > 0.0f)
				{
					state.Push -= normal * (into * (1.0f + IceBounce));
					return moved;
				}
			}
			state.Push *= std::clamp(Dot(moved - position, delta) / wanted, 0.0f, 1.0f);
			return moved;
		}

		void DashStep(const Level& level, Transform& transform, PlayerState& state, Vec2 delta)
		{
			const Vec2 from = transform.Position;
			transform.Position = MoveCircle(level, from, PlayerRadius, delta, {}, level.Floating);
			if (CrossesVoid(level, from, transform.Position))
				state.Airborne = true;
		}

		void Topple(PlayerState& state, PlayerActions& actions, bool dashed)
		{
			if (dashed)
				state.Push += state.DashDirection * (state.DashSpeed * DashCarry);
			state.Fall = ReadyEpsilon;
			state.DashDistance = 0.0f;
			state.Airborne = false;
			state.CastSlot = -1;
			state.CastTimer = 0.0f;
			actions.Fell = true;
		}

		void Footing(const Level& level, const Transform& transform, PlayerState& state, PlayerActions& actions, bool dashed)
		{
			if (!level.Floating)
				return;
			if (IsOverVoid(level, transform.Position))
			{
				if (state.DashDistance > 0.0f)
					state.Airborne = true;
				else
					Topple(state, actions, dashed);
				return;
			}
			const float height = level.Map.GetHeight(transform.Position);
			if (state.Airborne && std::abs(height - state.Footing) > FootingStep)
			{
				Topple(state, actions, true);
				return;
			}
			state.Airborne = false;
			state.Footing = height;
			if (state.DashDistance <= 0.0f && GetVoidClearance(level, transform.Position, LedgeClearance) >= LedgeClearance)
				state.Ledge = transform.Position;
		}

		void Plunge(const Level& level, Transform& transform, PlayerState& state, PlayerActions& actions, float dt)
		{
			state.Fall += dt;
			state.CastSlot = -1;
			state.CastTimer = 0.0f;
			state.DashDistance = 0.0f;
			if (state.Fall < FallSeconds)
			{
				const Vec2 drifted = transform.Position + state.Push * dt;
				transform.Position = { std::clamp(drifted.x, 0.5f, float(level.Map.GetWidth()) - 0.5f), std::clamp(drifted.y, 0.5f, float(level.Map.GetHeight()) - 0.5f) };
				state.Push *= std::exp(-FallDrag * dt);
				return;
			}
			state.Push = {};
			const Vec2 ledge = state.Ledge;
			const Vec2 home = level.Map.IsWalkableAt(ledge) ? ledge : level.PlayerSpawn;
			transform.Position += (home - transform.Position) * (1.0f - std::exp(-LiftRate * dt));
			if (state.Fall < FallSeconds + LiftSeconds)
				return;
			transform.Position = home;
			state.Ledge = home;
			state.Fall = 0.0f;
			state.Invulnerable = std::max(state.Invulnerable, LandingGrace);
			actions.Landed = true;
		}

		void StartDash(PlayerState& state, const AbilityDef& ability, Vec2 direction, float distance)
		{
			state.DashDirection = direction;
			state.DashDistance = distance;
			state.DashSpeed = ability.DashDuration > 0.0f ? ability.DashDistance / ability.DashDuration : 0.0f;
			state.Invulnerable = std::max(state.Invulnerable, ability.Invulnerability);
		}

		Vec2 LungeStep(const SkillTraits& skill, Vec2 aim, float aimDistance, float before, float after)
		{
			const float lunge = GetAbility(skill.Ability).Lunge;
			if (lunge <= 0.0f || skill.CastTime <= ReadyEpsilon)
				return {};
			const float elapsed = std::clamp(before, 0.0f, skill.CastTime) - std::clamp(after, 0.0f, skill.CastTime);
			return aim * std::min(lunge * elapsed / skill.CastTime, std::max(aimDistance - LungeStandoff, 0.0f));
		}

		bool CaughtByStars(const Level& level, Vec2 origin, AbilityId spell, Vec2 aim, float aimDistance)
		{
			const Landmark* observatory = level.FindLandmark(LandmarkKind::Observatory);
			if (!observatory || LengthSquared(origin - observatory->Position) > observatory->Radius * observatory->Radius)
				return false;
			const Vec2 target = origin + aim * aimDistance;
			return std::any_of(level.Constellations.begin(), level.Constellations.end(), [&](const Constellation& constellation) { return constellation.Spell == spell && LengthSquared(target - constellation.Position) <= constellation.Radius * constellation.Radius; });
		}

		void Release(const Level& level, Vec2 origin, PlayerState& state, PlayerActions& actions, const PlayerTraits& traits, uint32_t slot, Vec2 aim, float aimDistance)
		{
			const SkillTraits& skill = traits.Skills[slot];
			state.CastSlot = -1;
			state.CastTimer = 0.0f;
			if (skill.IsEmpty() || !skill.Usable)
				return;
			const bool overloaded = IsOverloaded(state, traits, slot);
			const float cost = overloaded ? 0.0f : skill.ManaCost;
			if (state.Mana + ReadyEpsilon < cost && traits.BloodRate <= 0.0f)
				return;
			actions.ManaBefore = state.Mana;
			if (state.Mana + ReadyEpsilon < cost)
				actions.BloodCost += (cost - state.Mana) * traits.BloodRate;
			state.Mana = std::max(state.Mana - cost, 0.0f);
			if (traits.OverloadEvery > 0)
				state.Overload = overloaded ? 0 : uint8_t(state.Overload + 1);
			if (overloaded)
				actions.Overloaded |= uint8_t(1u << slot);
			state.Cooldowns[slot] = skill.Cooldown;
			actions.Released |= uint8_t(1u << slot);
			const AbilityDef& ability = GetAbility(skill.Ability);
			if (IsRecastable(skill.Ability) && ability.ProjectileSpeed > 0.0f)
			{
				const float range = ability.ProjectileRange * skill.RangeScale;
				state.RecastSlot = int8_t(slot);
				state.RecastAbility = uint8_t(skill.Ability);
				state.RecastAge = 0.0f;
				state.RecastOrigin = origin + aim * ability.SpawnOffset;
				state.RecastAim = aim;
				state.RecastReach = range * std::clamp(CastSegment(level, state.RecastOrigin, state.RecastOrigin + aim * range, Block::Projectiles), 0.0f, 1.0f);
				state.RecastTimer = range / ability.ProjectileSpeed + (skill.Ability == AbilityId::ChainHook ? HookAnchorSeconds : 0.0f);
			}
			if (ability.Kind == AbilityKind::Dash)
			{
				if (!CaughtByStars(level, origin, skill.Ability, aim, aimDistance))
				{
					StartDash(state, ability, aim, ability.Range > 0.0f ? std::clamp(aimDistance, 1.0f, ability.Range) : ability.DashDistance);
					state.Push = {};
				}
			}
			else if (ability.Kind == AbilityKind::Ward)
				state.Ward = ability.Ward;
		}
	}

	bool IsInPortalRange(const Level& level, Vec2 position, uint32_t portal)
	{
		const Portal* target = level.FindPortal(portal);
		return target && LengthSquared(position - target->Position) <= target->Radius * target->Radius;
	}

	bool CanCast(const PlayerState& state, const PlayerTraits& traits, uint32_t slot)
	{
		const SkillTraits& skill = traits.Skills[slot];
		return !skill.IsEmpty() && skill.Usable && state.Cooldowns[slot] <= ReadyEpsilon && (state.Mana + ReadyEpsilon >= skill.ManaCost || traits.BloodRate > 0.0f || IsOverloaded(state, traits, slot));
	}

	bool IsOverloaded(const PlayerState& state, const PlayerTraits& traits, uint32_t slot)
	{
		return traits.OverloadEvery > 0 && slot < AbilitySlots && traits.Skills[slot].ManaCost > 0.0f && state.Overload + 1u >= traits.OverloadEvery;
	}

	bool IsRecastable(AbilityId ability)
	{
		return ability == AbilityId::ChainHook || ability == AbilityId::SplittingBolt;
	}

	Vec2 GetHookPoint(const PlayerState& state)
	{
		return state.RecastOrigin + state.RecastAim * std::min(state.RecastAge * GetAbility(AbilityId::ChainHook).ProjectileSpeed, state.RecastReach);
	}

	Vec2 GetGroundTarget(const Level& level, Vec2 origin, Vec2 direction, float aimDistance, float range)
	{
		const Vec2 target = origin + direction * std::clamp(aimDistance, 0.0f, range);
		const float travel = CastSegment(level, origin, target, Block::Projectiles);
		return travel >= 1.0f ? target : Lerp(origin, target, std::max(travel - 0.05f, 0.0f));
	}

	PlayerActions SimulatePlayer(const Level& level, Transform& transform, PlayerState& state, const PlayerTraits& traits, const PlayerInput& input, float dt, std::span<const BodyCircle> bodies)
	{
		PlayerActions actions;
		const uint8_t pressed = input.Buttons & ~state.HeldButtons;
		state.HeldButtons = input.Buttons;
		for (float& cooldown : state.Cooldowns)
			Cool(cooldown, dt);
		Cool(state.DodgeCooldown, dt);
		Cool(state.Invulnerable, dt);
		Cool(state.PortalCooldown, dt);
		Cool(state.Stun, dt);
		Cool(state.Chill, dt);
		Cool(state.Slick, dt);
		Cool(state.StunGuard, dt);
		Cool(state.Ward, dt);
		Cool(state.Static, dt);
		Cool(state.ManaStall, dt);
		if (state.RecastTimer > 0.0f)
		{
			state.RecastAge += dt;
			Cool(state.RecastTimer, dt);
		}
		if (state.RecastTimer <= 0.0f)
		{
			state.RecastSlot = -1;
			state.RecastAbility = UINT8_MAX;
		}
		if (!state.Alive)
		{
			state.DashDistance = 0.0f;
			state.CastSlot = -1;
			state.CastTimer = 0.0f;
			state.Push = {};
			state.Fall = 0.0f;
			state.Still = 0.0f;
			return actions;
		}
		if (state.IsFalling())
		{
			state.Still = 0.0f;
			Plunge(level, transform, state, actions, dt);
			return actions;
		}
		if (state.ManaStall <= 0.0f)
			state.Mana = std::min(traits.MaxMana, state.Mana + traits.ManaRegen * dt);

		const Vec2 aim = FromAngle(input.Aim);
		const Vec2 move = ClampLength(input.Move, 1.0f);
		actions.Direction = aim;
		actions.AimDistance = input.AimDistance;
		const bool dashed = state.DashDistance > 0.0f;
		if (state.IsStunned())
		{
			state.CastSlot = -1;
			state.CastTimer = 0.0f;
			if (state.DashDistance > 0.0f)
			{
				const float step = state.DashSpeed > 0.0f ? std::min(state.DashDistance, state.DashSpeed * dt) : state.DashDistance;
				state.DashDistance = state.DashDistance - step > 1e-5f ? state.DashDistance - step : 0.0f;
				DashStep(level, transform, state, state.DashDirection * step);
			}
			else if (LengthSquared(state.Push) > 0.0f)
				transform.Position = MoveAndBrake(level, state, transform.Position, state.Push * dt, {});
			DecayPush(level, transform.Position, state, dt);
			Footing(level, transform, state, actions, dashed);
			return actions;
		}
		transform.Facing = input.Aim;

		if (input.Interact != 0 && state.PortalCooldown <= ReadyEpsilon && state.DashDistance <= 0.0f && IsInPortalRange(level, transform.Position, input.Interact - 1u))
		{
			const Portal& portal = *level.FindPortal(input.Interact - 1u);
			transform.Position = portal.Destination;
			state.PortalCooldown = PortalCooldownSeconds;
			state.CastSlot = -1;
			state.CastTimer = 0.0f;
			actions.Portal = int32_t(input.Interact - 1u);
			return actions;
		}

		if ((pressed & uint8_t(InputButton::Dodge)) && state.DodgeCooldown <= ReadyEpsilon && state.DashDistance <= 0.0f)
		{
			const AbilityDef& roll = GetAbility(traits.Dodge.Ability);
			state.CastSlot = -1;
			state.CastTimer = 0.0f;
			StartDash(state, roll, LengthSquared(move) > 0.01f ? Normalize(move) : aim, roll.DashDistance);
			state.DodgeCooldown = traits.Dodge.Cooldown;
			state.Push *= 0.5f;
			actions.Dodged = true;
		}

		if (state.RecastSlot >= 0 && state.DashDistance <= 0.0f && (pressed & uint8_t(SlotButton(uint32_t(state.RecastSlot)))) != 0)
		{
			const uint32_t slot = uint32_t(state.RecastSlot);
			actions.Recast |= uint8_t(1u << slot);
			actions.RecastPoint = GetHookPoint(state);
			actions.RecastAbility = AbilityId(state.RecastAbility);
			if (actions.RecastAbility == AbilityId::ChainHook)
			{
				const Vec2 offset = actions.RecastPoint - transform.Position;
				const float distance = Length(offset);
				if (distance > HookYankStop + 0.05f)
				{
					state.CastSlot = -1;
					state.CastTimer = 0.0f;
					state.DashDirection = offset / distance;
					state.DashDistance = distance - HookYankStop;
					state.DashSpeed = HookYankSpeed;
					state.Push = {};
				}
			}
			state.RecastTimer = 0.0f;
			state.RecastSlot = -1;
			state.RecastAbility = UINT8_MAX;
		}

		int32_t castingSlot = -1;
		Vec2 lunge;
		if (state.DashDistance <= 0.0f)
		{
			if (state.CastSlot >= 0)
			{
				castingSlot = state.CastSlot;
				const float before = state.CastTimer;
				state.CastTimer -= dt;
				lunge = LungeStep(traits.Skills[size_t(castingSlot)], aim, input.AimDistance, before, state.CastTimer);
				if (state.CastTimer <= ReadyEpsilon)
					Release(level, transform.Position, state, actions, traits, uint32_t(state.CastSlot), aim, input.AimDistance);
			}
			else
			{
				for (const uint32_t slot : CastPriority)
				{
					if (!input.IsDown(SlotButton(slot)) || !CanCast(state, traits, slot))
						continue;
					if (traits.Skills[slot].CastTime <= ReadyEpsilon)
						Release(level, transform.Position, state, actions, traits, slot, aim, input.AimDistance);
					else
					{
						state.CastSlot = int8_t(slot);
						state.CastTimer = traits.Skills[slot].CastTime;
						castingSlot = int32_t(slot);
					}
					break;
				}
			}
		}

		const float speed = traits.MoveSpeed * (castingSlot >= 0 ? traits.Skills[size_t(castingSlot)].MoveScale : 1.0f) * (state.Chill > 0.0f ? ChilledMoveScale : 1.0f);
		Vec2 delta = move * (speed * dt * PushControl(state)) + lunge + state.Push * dt;
		const bool dashing = state.DashDistance > 0.0f;
		if (dashing)
		{
			const float step = state.DashSpeed > 0.0f ? std::min(state.DashDistance, state.DashSpeed * dt) : state.DashDistance;
			delta = state.DashDirection * step;
			state.DashDistance = state.DashDistance - step > 1e-5f ? state.DashDistance - step : 0.0f;
			DashStep(level, transform, state, delta);
		}
		else
			transform.Position = MoveAndBrake(level, state, transform.Position, delta, bodies);
		DecayPush(level, transform.Position, state, dt);
		Footing(level, transform, state, actions, dashed || dashing);
		if (LengthSquared(move) <= 1e-4f && !dashed && !dashing)
			state.Still = std::min(state.Still + dt, LeylineSeconds + LeylineLinger);
		else
			state.Still = state.Still > LeylineSeconds ? state.Still - dt : 0.0f;
		return actions;
	}
}
