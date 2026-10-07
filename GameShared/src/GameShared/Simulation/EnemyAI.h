#pragma once
#include "GameShared/Entity/EntityRegistry.h"

namespace Game
{
	class World;
	struct Enemy;
	struct Boss;
	struct EnemyDef;
	struct Transform;

	void UpdateEnemies(World& world, float dt);
	void UpdateBoss(World& world, Entity entity, Transform& transform, Enemy& enemy, Boss& boss, const EnemyDef& def, float radius, float dt);
}
