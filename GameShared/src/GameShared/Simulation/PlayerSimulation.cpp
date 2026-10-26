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

		constexpr uint32_t FullSupport = 9;
		constexpr float LeadReach = 0.32f;

		void Cool(float& timer, float dt)
		{
			timer = std::max(timer - dt, 0.0f);
		}

		const CollapsePlan& PlanOf(const ArenaState& arena)
		{
			static const CollapsePlan Still{ .StartRadius = 1e9f, .Steady = true };
			return arena.Plan ? *arena.Plan : Still;
		}

		uint32_t SupportDepth(const Level& level, const ArenaState& arena, Vec2 position, float margin)
		{
			if (!IsSupported(level, arena, position))
				return 0;
			uint32_t depth = 1;
			for (int32_t i = 0; i < 8; i++)
				depth += IsSupported(level, arena, position + FromAngle(float(i) * (Pi * 0.25f)) * margin) ? 1u : 0u;
			return depth;
		}

		Vec2 Carry(const Level& level, const ArenaState& arena, Vec2 position, float dt)
		{
			if (level.Islands.empty() || !arena.Plan)
				return position;
			const int32_t island = IslandAt(level, *arena.Plan, arena.Clock, position);
			if (island < 0)
				return position;
			const DriftIsland& drift = level.Islands[size_t(island)];
			return position + IslandOffset(drift, arena.Clock + dt) - IslandOffset(drift, arena.Clock);
		}

		Vec2 WindOf(const Level& level, const ArenaState& arena)
		{
			return level.Skyborne && arena.Plan ? GustAt(*arena.Plan, arena.Clock).Velocity : Vec2{};
		}

		void Strike(const ArenaState& arena, float dt, const Transform& transform, PlayerState& state)
		{
			if (!arena.Deadly || !arena.Plan || arena.Plan->Shape != BrawlShape::Peak || !state.Alive || state.IsFalling() || state.Invulnerable > 0.0f)
				return;
			const LightningBolt* bolt = BoltStriking(*arena.Plan, transform.Position, arena.Clock, arena.Clock + dt);
			if (!bolt)
				return;
			Vec2 away = transform.Position - bolt->Center;
			if (LengthSquared(away) < 0.04f)
				away = FromAngle(2.0f * Pi * CollapseNoise(int32_t(bolt->Time * 64.0f), 0x7A7B, arena.Plan->Seed));
			state.Push += Normalize(away) * BoltPush;
			state.Stun = std::max(state.Stun, BoltStun);
			state.CastSlot = -1;
			state.CastTimer = 0.0f;
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

		std::optional<Vec2> TooHigh(const Level& level, float floor, Vec2 from, Vec2 point, float limit)
		{
			const Vec2 travel = point - from;
			const float length = Length(travel);
			const Vec2 lead = length > 1e-5f ? point + travel * (LeadReach / length) : point;
			for (const Vec2 sample : { point, lead })
				if (FloorHeight(level, sample) - floor > limit)
					return sample;
			return std::nullopt;
		}

		Vec2 Uphill(const Elevation& relief, Vec2 from, Vec2 wall)
		{
			for (int32_t cliff = 0; cliff < ElevationCliffs; cliff++)
				if (relief.EdgeDistance(cliff, from) < 0.0f && relief.EdgeDistance(cliff, wall) >= 0.0f)
					if (const Vec2 rise = relief.Gradient(relief.Edges[size_t(cliff)], wall); LengthSquared(rise) > 1e-8f)
						return Normalize(rise);
			const float h = relief.Spacing;
			const Vec2 rise(relief.Height(wall + Vec2(h, 0.0f)) - relief.Height(wall - Vec2(h, 0.0f)), relief.Height(wall + Vec2(0.0f, h)) - relief.Height(wall - Vec2(0.0f, h)));
			return LengthSquared(rise) > 1e-10f ? Normalize(rise) : Normalize(wall - from);
		}

		Vec2 Climb(const Level& level, Vec2 from, Vec2 to, std::span<const BodyCircle> bodies, float limit)
		{
			const float floor = FloorHeight(level, from);
			const std::optional<Vec2> wall = TooHigh(level, floor, from, to, limit);
			if (!wall)
				return to;
			const Vec2 normal = Uphill(level.Relief, from, *wall);
			Vec2 slide = to - from;
			slide -= normal * std::max(Dot(slide, normal), 0.0f);
			if (LengthSquared(slide) < 1e-10f)
				return from;
			const Vec2 slid = MoveCircle(level, from, PlayerRadius, slide, bodies, true);
			return TooHigh(level, floor, from, slid, limit) ? from : slid;
		}

		Vec2 MoveOnSky(const Level& level, const ArenaState& next, PlayerState& state, Vec2 position, Vec2 delta, std::span<const BodyCircle> bodies)
		{
			Vec2 moved = MoveCircle(level, position, PlayerRadius, delta, bodies, true);
			if (!level.Relief.IsEmpty())
				moved = Climb(level, position, moved, bodies, ClimbHeight);
			if (LengthSquared(state.Push) <= ShoveOverVoid * ShoveOverVoid)
			{
				const uint32_t before = SupportDepth(level, next, position, VoidLedge);
				const auto keeps = [&](Vec2 point)
				{
					const uint32_t after = SupportDepth(level, next, point, VoidLedge);
					return after >= FullSupport || (after > 0 && after >= before);
				};
				if (before > 0 && !keeps(moved))
				{
					const Vec2 alongX = MoveCircle(level, position, PlayerRadius, Vec2(delta.x, 0.0f), bodies, true);
					const Vec2 alongY = MoveCircle(level, position, PlayerRadius, Vec2(0.0f, delta.y), bodies, true);
					const bool x = keeps(alongX);
					const bool y = keeps(alongY);
					if (x && (!y || LengthSquared(alongX - position) >= LengthSquared(alongY - position)))
						moved = alongX;
					else if (y)
						moved = alongY;
					else
						moved = position;
				}
			}
			const float wanted = LengthSquared(delta);
			if (wanted > 1e-10f && LengthSquared(state.Push) > 0.0f)
				state.Push *= std::clamp(Dot(moved - position, delta) / wanted, 0.0f, 1.0f);
			return moved;
		}

		Vec2 Move(const Level& level, const ArenaState& next, PlayerState& state, Vec2 position, Vec2 delta, std::span<const BodyCircle> bodies)
		{
			return level.Skyborne ? MoveOnSky(level, next, state, position, delta, bodies) : MoveAndBrake(level, state, position, delta, bodies);
		}

		void DashStep(const Level& level, Transform& transform, PlayerState& state, Vec2 delta)
		{
			const Vec2 from = transform.Position;
			transform.Position = MoveCircle(level, from, PlayerRadius, delta, {}, level.Floating);
			if (!level.Relief.IsEmpty())
				if (const Vec2 climbed = Climb(level, from, transform.Position, {}, DashClimb); climbed != transform.Position)
				{
					transform.Position = climbed;
					state.DashDistance = 0.0f;
				}
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

		void Footing(const Level& level, const ArenaState& next, const Transform& transform, PlayerState& state, PlayerActions& actions, bool dashed)
		{
			if (level.Skyborne)
			{
				if (state.DashDistance > 0.0f)
					return;
				if (!IsSupported(level, next, transform.Position))
					Topple(state, actions, dashed);
				else if (SupportDepth(level, next, transform.Position, LedgeClearance) >= FullSupport)
					state.Ledge = transform.Position;
				return;
			}
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

		void Plunge(const Level& level, const ArenaState& arena, Transform& transform, PlayerState& state, PlayerActions& actions, float dt)
		{
			state.Fall += dt;
			state.CastSlot = -1;
			state.CastTimer = 0.0f;
			state.DashDistance = 0.0f;
			if (state.Fall < FallSeconds || (level.Skyborne && arena.Deadly))
			{
				const Vec2 drifted = transform.Position + state.Push * dt;
				transform.Position = { std::clamp(drifted.x, 0.5f, float(level.Map.GetWidth()) - 0.5f), std::clamp(drifted.y, 0.5f, float(level.Map.GetHeight()) - 0.5f) };
				state.Push *= std::exp(-FallDrag * dt);
				return;
			}
			state.Push = {};
			const Vec2 ledge = state.Ledge;
			const bool standing = level.Skyborne ? IsSupported(level, arena, ledge) : level.Map.IsWalkableAt(ledge);
			const Vec2 home = standing ? ledge : level.PlayerSpawn;
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

	bool IsSupported(const Level& level, const ArenaState& arena, Vec2 position)
	{
		return !IsBrawlLava(level, PlanOf(arena), arena.Plan ? arena.Clock : -1e5f, position);
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

	bool IsCharged(const SkillTraits& skill)
	{
		return skill.ChargeTime > 0.0f;
	}

	float ChargePower(const PlayerState& state, const SkillTraits& skill)
	{
		const float span = skill.ChargeTime - skill.CastTime;
		return span > ReadyEpsilon ? std::clamp((state.Charge - skill.CastTime) / span, 0.0f, 1.0f) : 1.0f;
	}

	float ChargeOver(const PlayerState& state, const SkillTraits& skill)
	{
		return std::clamp((state.Charge - skill.ChargeTime) / ChargeCrackSeconds, 0.0f, 1.0f);
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

	PlayerActions SimulatePlayer(const Level& level, Transform& transform, PlayerState& state, const PlayerTraits& traits, const PlayerInput& input, float dt, std::span<const BodyCircle> bodies, const ArenaState& arena)
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
			Plunge(level, arena, transform, state, actions, dt);
			return actions;
		}
		if (state.ManaStall <= 0.0f)
			state.Mana = std::min(traits.MaxMana, state.Mana + traits.ManaRegen * dt);

		ArenaState next = arena;
		next.Clock += dt;
		const Vec2 wind = WindOf(level, arena);
		if (level.Skyborne && state.DashDistance <= 0.0f)
			transform.Position = Carry(level, arena, transform.Position, dt);
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
			else
			{
				if (LengthSquared(state.Push) > 0.0f)
					transform.Position = Move(level, next, state, transform.Position, state.Push * dt, {});
				if (LengthSquared(wind) > 0.0f)
					transform.Position = MoveCircle(level, transform.Position, PlayerRadius, wind * dt, {}, true);
			}
			DecayPush(level, transform.Position, state, dt);
			Footing(level, next, transform, state, actions, dashed);
			Strike(arena, dt, transform, state);
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
				const SkillTraits& skill = traits.Skills[size_t(castingSlot)];
				const float before = state.CastTimer;
				if (IsCharged(skill))
				{
					state.CastTimer = std::max(state.CastTimer - dt, 0.0f);
					state.Charge = std::min(state.Charge + dt, skill.ChargeTime + ChargeCrackSeconds);
					const bool cracked = state.Charge >= skill.ChargeTime + ChargeCrackSeconds - ReadyEpsilon;
					if (cracked || (state.CastTimer <= ReadyEpsilon && !input.IsDown(SlotButton(uint32_t(castingSlot)))))
					{
						actions.Charge = cracked ? ChargeCrackPower : ChargePower(state, skill);
						actions.Cracked = cracked;
						Release(level, transform.Position, state, actions, traits, uint32_t(castingSlot), aim, input.AimDistance);
					}
				}
				else
				{
					state.CastTimer -= dt;
					lunge = LungeStep(skill, aim, input.AimDistance, before, state.CastTimer);
					if (state.CastTimer <= ReadyEpsilon)
						Release(level, transform.Position, state, actions, traits, uint32_t(castingSlot), aim, input.AimDistance);
				}
			}
			else
			{
				for (const uint32_t slot : CastPriority)
				{
					if (!input.IsDown(SlotButton(slot)) || !CanCast(state, traits, slot))
						continue;
					if (traits.Skills[slot].CastTime <= ReadyEpsilon && !IsCharged(traits.Skills[slot]))
						Release(level, transform.Position, state, actions, traits, slot, aim, input.AimDistance);
					else
					{
						state.CastSlot = int8_t(slot);
						state.CastTimer = traits.Skills[slot].CastTime;
						state.Charge = 0.0f;
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
		{
			transform.Position = Move(level, next, state, transform.Position, delta, bodies);
			if (LengthSquared(wind) > 0.0f)
				transform.Position = MoveCircle(level, transform.Position, PlayerRadius, wind * dt, bodies, true);
		}
		DecayPush(level, transform.Position, state, dt);
		Footing(level, next, transform, state, actions, dashed || dashing);
		if (LengthSquared(move) <= 1e-4f && !dashed && !dashing)
			state.Still = std::min(state.Still + dt, LeylineSeconds + LeylineLinger);
		else
			state.Still = state.Still > LeylineSeconds ? state.Still - dt : 0.0f;
		Strike(arena, dt, transform, state);
		return actions;
	}
}
