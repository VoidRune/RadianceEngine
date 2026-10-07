#pragma once
#include "GameShared/Content/Definitions.h"
#include "GameShared/Entity/Components.h"
#include "GameShared/Simulation/Abilities.h"

namespace Game
{
	class World;

	struct HitInfo
	{
		int32_t Damage = 0;
		uint32_t Attacker = NoPlayer;
		Entity Source;
		float CritChance = 0.0f;
		float CritMultiplier = 1.5f;
		float LifeSteal = 0.0f;
		Element Damages = Element::Physical;
		float Chill = 0.0f;
		float Stun = 0.0f;
		float PvpScale = 1.0f;
		uint8_t Ability = uint8_t(AbilityId::Count);
	};

	struct DamageEvent
	{
		int32_t Amount = 0;
		uint32_t Attacker = NoPlayer;
		Element Damages = Element::Physical;
		uint8_t Ability = uint8_t(AbilityId::Count);
		bool Critical = false;
		bool Pvp = false;
		bool Unmitigated = false;
	};

	bool ApplyDamage(World& world, Entity target, const DamageEvent& damage);
	bool DealDamage(World& world, Entity target, const HitInfo& hit);
	void StrikeArea(World& world, const AbilityDef& ability, int32_t damage, const AbilityContext& context);
	void StrikeCircle(World& world, Vec2 center, float radius, const HitInfo& hit, Team side, float rewind);
	void DropLoot(World& world, const EnemyDef& def, const Enemy& enemy, Vec2 position);
	void UpdateProjectiles(World& world, float dt);
	void UpdateStrikes(World& world, float dt);
	void UpdateEffects(World& world, float dt);
	void UpdateLoot(World& world, float dt);
	void UpdateRespawns(World& world, float dt);
	void UpdateRegeneration(World& world, float dt);
}
