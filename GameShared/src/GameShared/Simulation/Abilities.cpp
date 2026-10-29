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
		const bool brawlDamage = world.UsesBrawlDamage();
		const int32_t damage = ScaleDamage(brawlDamage ? ability.BrawlDamage : ability.Damage, context.DamageScale);
		switch (ability.Kind)
		{
		case AbilityKind::Projectile:
		{
			const ProjectilePattern pattern = ChargedPattern(ability, GetProjectilePattern(ability, context.ExtraProjectiles, context.ExtraPierce, context.ProjectileSpeedScale), context.Charge, context.ProjectileSpeedScale);
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
		{
			StrikeArea(world, ability, damage, context);
			world.SpawnEffect(ability, context, context.Origin, context.Tag);
			const Vec2 axis = Normalize(context.Direction);
			if (id == AbilityId::Gust && HasCard(context.Cards, CardId::LingeringGale))
			{
				const float calm = brawlDamage ? GaleDamagePush : 1.0f;
				AbilityContext gale = context;
				gale.DamageScale /= calm;
				for (uint32_t pulse = 0; pulse < GalePulses; pulse++)
					world.SpawnCardStrike(AbilityId::Gust, context.Origin + axis * ((GaleOffset + GaleDrift * float(pulse)) * context.AreaScale), AngleOf(axis), GaleFirstDelay + GaleInterval * float(pulse), GaleRadius * context.AreaScale, GalePush * calm * context.PushScale, PerkAlong, false, gale);
			}
			if (id == AbilityId::Gust && HasCard(context.Cards, CardId::DeflectingGale))
				DeflectProjectiles(world, context, ability.Reach * context.AreaScale, ability.HalfArc);
			if (id == AbilityId::Shockwave && HasCard(context.Cards, CardId::Aftershock))
				world.SpawnCardStrike(AbilityId::Shockwave, context.Origin, AngleOf(axis), AftershockDelay, ability.Reach * context.AreaScale, ability.Push * AftershockScale * context.PushScale, 0, false, context);
			break;
		}
		case AbilityKind::Ground:
			if (id == AbilityId::EarthSpike)
			{
				AbilityContext thrust = context;
				thrust.Origin = context.Target;
				StrikeArea(world, ability, damage, thrust);
				world.SpawnEffect(ability, thrust, context.Target, context.Tag);
				break;
			}
			world.SpawnPendingStrike(ability, context.Target, damage, context);
			if (id == AbilityId::Firebomb)
			{
				const Vec2 axis = Normalize(context.Direction);
				const uint32_t skips = HasCard(context.Cards, CardId::ClusterBomb) ? ClusterSkips : 1u;
				Vec2 landing = context.Target;
				for (uint32_t skip = 0; skip <= skips; skip++)
				{
					const AbilityDef& bounce = GetAbility(skip == 0 ? AbilityId::Firebomb : skip == 1 ? AbilityId::FirebombSkip : AbilityId::FirebombSkip2);
					if (skip > 0)
					{
						const Vec2 next = context.Target + axis * (FirebombSkipDistance * float(skip));
						landing = Lerp(landing, next, std::max(CastSegment(world.GetLevel(), landing, next, Block::Projectiles) - 0.02f, 0.0f));
						world.SpawnPendingStrike(bounce, landing, brawlDamage ? ScaleDamage(bounce.BrawlDamage, context.DamageScale) : damage, context);
					}
					if (HasCard(context.Cards, CardId::StickyFire))
						for (uint32_t pulse = 0; pulse < StickyPulses; pulse++)
							world.SpawnCardStrike(AbilityId::FirebombFlame, landing, AngleOf(axis), bounce.ImpactDelay + StickyFirstDelay + StickyInterval * float(pulse), StickyRadius * context.AreaScale, StickyPush * context.PushScale, 0, false, context);
				}
			}
			if (id == AbilityId::Comet && HasCard(context.Cards, CardId::TwinComets))
			{
				AbilityContext twin = context;
				twin.DamageScale *= brawlDamage ? TwinDamage / TwinScale : 1.0f;
				world.SpawnCardStrike(AbilityId::Comet, context.Target, AngleOf(context.Direction), ability.ImpactDelay + TwinDelay, ability.ImpactRadius * context.AreaScale * (brawlDamage ? 1.0f : TwinScale), ability.Push * TwinScale * context.PushScale, HasCard(context.Cards, CardId::ShatteringImpact) ? PerkShatter : 0, true, twin);
			}
			if (id == AbilityId::GravityWell && HasCard(context.Cards, CardId::Implosion))
			{
				AbilityContext burst = context;
				burst.DamageScale *= brawlDamage ? ImplosionDamage : 1.0f;
				world.SpawnCardStrike(AbilityId::Shockwave, context.Target, AngleOf(context.Direction), ability.ImpactDelay + ImplosionDelay, ability.ImpactRadius * context.AreaScale, ImplosionPush * context.PushScale, 0, false, burst);
			}
			break;
		case AbilityKind::Heal:
			if (Health* health = world.GetEntities().TryGet<Health>(context.Source); health && !health->IsDead())
				health->Current = std::min(health->Max, health->Current + std::max(1, int32_t(float(health->Max) * ability.Heal)));
			world.SpawnEffect(ability, context, context.Origin, context.Tag);
			break;
		case AbilityKind::Dash:
		case AbilityKind::Ward:
			world.SpawnEffect(ability, context, context.Origin, context.Tag);
			if (id == AbilityId::Warp && HasCard(context.Cards, CardId::Afterimage))
			{
				AbilityContext wake = context;
				wake.DamageScale *= brawlDamage ? AfterimageDamage : 1.0f;
				world.SpawnCardStrike(AbilityId::Shockwave, context.Departure, AngleOf(context.Direction), AfterimageDelay, AfterimageRadius, AfterimagePush * context.PushScale, 0, false, wake);
			}
			break;
		case AbilityKind::Shield:
			break;
		}
	}
}
