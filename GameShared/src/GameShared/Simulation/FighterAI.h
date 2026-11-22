#pragma once
#include "GameShared/Content/Fighters.h"
#include "GameShared/Core/Random.h"
#include "GameShared/Entity/Components.h"
#include <span>
#include <vector>

namespace Game
{
	class World;
	struct HitInfo;

	struct FighterTarget
	{
		Entity Handle;
		Vec2 Position;
		Vec2 Velocity;
		float Radius = 0.0f;
	};

	struct FighterThreat
	{
		Vec2 Position;
		Vec2 Velocity;
		float Radius = 0.0f;
		float Remaining = 0.0f;
		bool Area = false;
	};

	struct FighterScene
	{
		std::vector<FighterTarget> Targets;
		std::vector<FighterThreat> Threats;
		std::vector<Vec2> Fighters;
		uint32_t Attackers = 0;
		uint32_t MaxAttackers = FighterAttackers;
	};

	void GatherFighterScene(World& world, FighterScene& scene);
	void UpdateFighter(World& world, Entity entity, Transform& transform, Enemy& enemy, const EnemyDef& def, float radius, FighterScene& scene, Vec2 push, float dt);
	void StrikeFighter(World& world, Entity entity, Enemy& enemy, const HitInfo& hit, int32_t damage);
	void BreakFighterGuard(Enemy& enemy);
	void StaggerFighter(Enemy& enemy, float seconds);
	void UpdateEnemyTraits(Enemy& enemy, float dt);
	void EmpowerAllies(World& world, float dt);
	int32_t ShieldEnemy(World& world, Entity entity, Enemy& enemy, int32_t amount, Element damages, uint32_t attacker, uint8_t ability);
	void FeedVampire(World& world, Entity source);
	void ApplyAffixes(World& world, Entity entity, uint8_t affixes);
	uint8_t RollAffixes(Random& random, EntityKind kind, uint32_t count, uint8_t banned = 0);

	inline bool IsMirrored(const Enemy& enemy)
	{
		return enemy.Mirror > 0.0f;
	}
}
