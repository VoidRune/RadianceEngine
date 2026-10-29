#pragma once
#include "GameShared/Content/Definitions.h"
#include "GameShared/Entity/Components.h"
#include "GameShared/Simulation/Abilities.h"
#include <span>

namespace Game
{
	class World;

	enum class PushMode : uint8_t
	{
		Along,
		Radial,
		Cone,
		Pull,
	};

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
		float Push = 0.0f;
		PushMode Mode = PushMode::Along;
		Vec2 PushOrigin;
		Vec2 PushDirection;
		float PushReach = 0.0f;
		bool Swap = false;
		bool Slick = false;
		bool Static = false;
		uint32_t Returned = NoPlayer;
		Vec2 GuardPoint;
		bool AtGuard = false;
		bool AtWard = false;
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
	void StrikeCircle(World& world, Vec2 center, float radius, const HitInfo& hit, Team side, float rewind, std::span<const Entity> spared = {});
	void DeflectProjectiles(World& world, const AbilityContext& context, float reach, float halfArc);
	void ReelIn(World& world, Entity caster, uint32_t owner, Vec2 point);
	void CancelRecast(World& world, Entity caster, AbilityId ability);
	void DropLoot(World& world, const EnemyDef& def, const Enemy& enemy, Vec2 position);
	void UpdateProjectiles(World& world, float dt);
	void UpdateStrikes(World& world, float dt);
	void UpdateEffects(World& world, float dt);
	void UpdateLoot(World& world, float dt);
	void UpdateRespawns(World& world, float dt);
	void UpdateRegeneration(World& world, float dt);
}
