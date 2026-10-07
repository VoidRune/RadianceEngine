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

	struct PlayerActions
	{
		uint8_t Released = 0;
		Vec2 Direction;
		float AimDistance = 0.0f;
		int32_t Portal = -1;
		bool Dodged = false;

		bool HasReleased(uint32_t slot) const { return (Released & (1u << slot)) != 0; }
		bool Teleported() const { return Portal >= 0; }
	};

	bool IsInPortalRange(const Level& level, Vec2 position, uint32_t portal);
	bool CanCast(const PlayerState& state, const PlayerTraits& traits, uint32_t slot);
	Vec2 GetGroundTarget(const Level& level, Vec2 origin, Vec2 direction, float aimDistance, float range);
	PlayerActions SimulatePlayer(const Level& level, Transform& transform, PlayerState& state, const PlayerTraits& traits, const PlayerInput& input, float dt, std::span<const BodyCircle> bodies = {});
}
