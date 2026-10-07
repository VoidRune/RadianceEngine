#pragma once
#include "GameShared/Content/Traits.h"
#include "GameShared/Entity/Components.h"

namespace Game
{
	class World;

	struct AbilityContext
	{
		Vec2 Origin;
		Vec2 Direction;
		Vec2 Target;
		Team Side = Team::Players;
		uint32_t Owner = NoPlayer;
		Entity Source;
		uint16_t Tag = 0;
		float DamageScale = 1.0f;
		float Rewind = 0.0f;
		uint8_t ExtraProjectiles = 0;
		uint8_t ExtraPierce = 0;
		float AreaScale = 1.0f;
		float ProjectileSpeedScale = 1.0f;
		float CritChance = 0.0f;
		float CritMultiplier = 1.5f;
		float LifeSteal = 0.0f;
		float PvpScale = 1.0f;
	};

	void ExecuteAbility(World& world, AbilityId id, const AbilityContext& context);
}
