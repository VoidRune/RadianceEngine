#include "PlayerSimulation.h"
#include "GameShared/Content/Pvp.h"
#include "GameShared/World/Collision.h"
#include <algorithm>

namespace Game
{
	namespace
	{
		constexpr float ReadyEpsilon = 1e-4f;
		constexpr uint32_t CastPriority[] = { 1, 2, 3, 0 };

		void Cool(float& timer, float dt)
		{
			timer = std::max(timer - dt, 0.0f);
		}

		void StartDash(PlayerState& state, const AbilityDef& ability, Vec2 direction, float distance)
		{
			state.DashDirection = direction;
			state.DashDistance = distance;
			state.DashSpeed = ability.DashDuration > 0.0f ? ability.DashDistance / ability.DashDuration : 0.0f;
			state.Invulnerable = std::max(state.Invulnerable, ability.Invulnerability);
		}

		void Release(PlayerState& state, PlayerActions& actions, const PlayerTraits& traits, uint32_t slot, Vec2 aim, float aimDistance)
		{
			const SkillTraits& skill = traits.Skills[slot];
			state.CastSlot = -1;
			state.CastTimer = 0.0f;
			if (skill.IsEmpty() || !skill.Usable || state.Mana + ReadyEpsilon < skill.ManaCost)
				return;
			state.Mana = std::max(state.Mana - skill.ManaCost, 0.0f);
			state.Cooldowns[slot] = skill.Cooldown;
			actions.Released |= uint8_t(1u << slot);
			const AbilityDef& ability = GetAbility(skill.Ability);
			if (ability.Kind == AbilityKind::Dash)
				StartDash(state, ability, aim, ability.Range > 0.0f ? std::clamp(aimDistance, 1.0f, ability.Range) : ability.DashDistance);
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
		return !skill.IsEmpty() && skill.Usable && state.Cooldowns[slot] <= ReadyEpsilon && state.Mana + ReadyEpsilon >= skill.ManaCost;
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
		Cool(state.StunGuard, dt);
		if (!state.Alive)
		{
			state.DashDistance = 0.0f;
			state.CastSlot = -1;
			state.CastTimer = 0.0f;
			return actions;
		}
		state.Mana = std::min(traits.MaxMana, state.Mana + traits.ManaRegen * dt);

		const Vec2 aim = FromAngle(input.Aim);
		const Vec2 move = ClampLength(input.Move, 1.0f);
		actions.Direction = aim;
		actions.AimDistance = input.AimDistance;
		if (state.IsStunned())
		{
			state.CastSlot = -1;
			state.CastTimer = 0.0f;
			if (state.DashDistance > 0.0f)
			{
				const float step = state.DashSpeed > 0.0f ? std::min(state.DashDistance, state.DashSpeed * dt) : state.DashDistance;
				state.DashDistance = state.DashDistance - step > 1e-5f ? state.DashDistance - step : 0.0f;
				transform.Position = MoveCircle(level, transform.Position, PlayerRadius, state.DashDirection * step);
			}
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
			actions.Dodged = true;
		}

		int32_t castingSlot = -1;
		if (state.DashDistance <= 0.0f)
		{
			if (state.CastSlot >= 0)
			{
				castingSlot = state.CastSlot;
				state.CastTimer -= dt;
				if (state.CastTimer <= ReadyEpsilon)
					Release(state, actions, traits, uint32_t(state.CastSlot), aim, input.AimDistance);
			}
			else
			{
				for (const uint32_t slot : CastPriority)
				{
					if (!input.IsDown(SlotButton(slot)) || !CanCast(state, traits, slot))
						continue;
					if (traits.Skills[slot].CastTime <= ReadyEpsilon)
						Release(state, actions, traits, slot, aim, input.AimDistance);
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
		Vec2 delta = move * (speed * dt);
		const bool dashing = state.DashDistance > 0.0f;
		if (dashing)
		{
			const float step = state.DashSpeed > 0.0f ? std::min(state.DashDistance, state.DashSpeed * dt) : state.DashDistance;
			delta = state.DashDirection * step;
			state.DashDistance = state.DashDistance - step > 1e-5f ? state.DashDistance - step : 0.0f;
		}
		transform.Position = MoveCircle(level, transform.Position, PlayerRadius, delta, dashing ? std::span<const BodyCircle>() : bodies);
		return actions;
	}
}
