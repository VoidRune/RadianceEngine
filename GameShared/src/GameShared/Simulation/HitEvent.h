#pragma once
#include "GameShared/Core/Math.h"
#include "GameShared/Entity/Components.h"
#include <cstdint>

namespace Game
{
	struct HitEvent
	{
		uint32_t Target = 0;
		Vec2 Position;
		uint16_t Damage = 0;
		bool Critical = false;
		bool OnPlayer = false;
		Element Damages = Element::Physical;
		uint32_t Attacker = NoPlayer;
	};
}
