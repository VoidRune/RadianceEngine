#pragma once
#include "GameShared/Content/Traits.h"
#include "GameShared/Entity/Components.h"
#include "GameShared/Simulation/PlayerInput.h"
#include "GameShared/World/Collision.h"
#include "GameShared/World/Level.h"
#include <span>

namespace Game
{
	inline constexpr float PortalCooldownSeconds = 0.75f;
	inline constexpr float FallSeconds = 1.2f;
	inline constexpr float LiftSeconds = 0.8f;

	struct CollapsePlan;

	struct ArenaState
	{
		const CollapsePlan* Plan = nullptr;
		float Clock = -1e5f;
		bool Deadly = false;
	};

	struct PlayerActions
	{
		uint8_t Released = 0;
		uint8_t Recast = 0;
		uint8_t Overloaded = 0;
		float BloodCost = 0.0f;
		float ManaBefore = 0.0f;
		float Charge = 0.0f;
		bool Cracked = false;
		AbilityId RecastAbility = AbilityId::Count;
		Vec2 RecastPoint;
		Vec2 Direction;
		float AimDistance = 0.0f;
		int32_t Portal = -1;
		bool Dodged = false;
		bool Fell = false;
		bool Landed = false;

		bool HasReleased(uint32_t slot) const { return (Released & (1u << slot)) != 0; }
		bool HasRecast(uint32_t slot) const { return (Recast & (1u << slot)) != 0; }
		bool Teleported() const { return Portal >= 0; }
	};

	bool IsInPortalRange(const Level& level, Vec2 position, uint32_t portal);
	bool CanCast(const PlayerState& state, const PlayerTraits& traits, uint32_t slot);
	bool IsOverloaded(const PlayerState& state, const PlayerTraits& traits, uint32_t slot);
	bool IsRecastable(AbilityId ability);
	bool IsCharged(const SkillTraits& skill);
	float ChargePower(const PlayerState& state, const SkillTraits& skill);
	float ChargeOver(const PlayerState& state, const SkillTraits& skill);
	Vec2 GetHookPoint(const PlayerState& state);
	Vec2 GetGroundTarget(const Level& level, Vec2 origin, Vec2 direction, float aimDistance, float range);
	bool IsSupported(const Level& level, const ArenaState& arena, Vec2 position);
	PlayerActions SimulatePlayer(const Level& level, Transform& transform, PlayerState& state, const PlayerTraits& traits, const PlayerInput& input, float dt, std::span<const BodyCircle> bodies = {}, const ArenaState& arena = {});
}
