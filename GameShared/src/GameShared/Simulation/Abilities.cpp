#include "Abilities.h"
#include "GameShared/Simulation/Combat.h"
#include "GameShared/Simulation/World.h"
#include "GameShared/World/Collision.h"
#include <algorithm>

namespace Game
{
	void ExecuteAbility(World& world, AbilityId id, const AbilityContext& context)
	{
		const AbilityDef& ability = GetAbility(id);
		const int32_t damage = ScaleDamage(ability.Damage, context.DamageScale);
		switch (ability.Kind)
		{
		case AbilityKind::Projectile:
		{
			const ProjectilePattern pattern = GetProjectilePattern(ability, context.ExtraProjectiles, context.ExtraPierce, context.ProjectileSpeedScale);
			for (uint32_t i = 0; i < pattern.Count; i++)
			{
				const Vec2 direction = ProjectileDirection(pattern, context.Direction, i);
				const Vec2 start = context.Origin + direction * ability.SpawnOffset;
				if (CastSegment(world.GetLevel(), context.Origin, start, Block::Projectiles) >= 1.0f)
					world.SpawnProjectile(ability, pattern, start, direction, damage, context);
			}
			break;
		}
		case AbilityKind::Melee:
		case AbilityKind::Nova:
			StrikeArea(world, ability, damage, context);
			world.SpawnEffect(ability, context, context.Origin, context.Tag);
			break;
		case AbilityKind::Ground:
			world.SpawnPendingStrike(ability, context.Target, damage, context);
			break;
		case AbilityKind::Heal:
			if (Health* health = world.GetEntities().TryGet<Health>(context.Source); health && !health->IsDead())
				health->Current = std::min(health->Max, health->Current + std::max(1, int32_t(float(health->Max) * ability.Heal)));
			world.SpawnEffect(ability, context, context.Origin, context.Tag);
			break;
		case AbilityKind::Dash:
		case AbilityKind::Ward:
			world.SpawnEffect(ability, context, context.Origin, context.Tag);
			break;
		}
	}
}
