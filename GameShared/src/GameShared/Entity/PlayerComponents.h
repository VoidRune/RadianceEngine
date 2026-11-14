#pragma once
#include "GameShared/Content/Traits.h"
#include "GameShared/Entity/Components.h"

namespace Game
{
	struct PlayerController
	{
		uint32_t PlayerId = 0;
		PlayerState State;
		PlayerTraits Traits;
		float RespawnTimer = 0.0f;
		float RegenBank = 0.0f;
		uint32_t LastAttacker = NoPlayer;
		float LastAttackAge = 1e9f;
		uint8_t LastAbility = uint8_t(AbilityId::Count);
		uint32_t Watching = NoPlayer;
		uint32_t KilledBy = NoPlayer;
	};

	struct Loot
	{
		LootKind Kind = LootKind::Health;
		float Lifetime = 0.0f;
	};
}
