#include "Abilities.h"
#include "GameShared/Simulation/Combat.h"
#include "GameShared/Simulation/World.h"
#include "GameShared/World/Collision.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Game
{
	namespace
	{
		void Resonate(World& world, const AbilityDef& ability, int32_t damage, const AbilityContext& context)
		{
			Vec2 center = ability.Kind == AbilityKind::Nova ? context.Origin : context.Target;
			float radius = ability.ImpactRadius * context.AreaScale;
			if (ability.Shape == StrikeShape::Line)
			{
				center = context.Target + Normalize(context.Direction) * (ability.Reach * context.AreaScale * 0.5f);
				radius = ability.Reach * context.AreaScale * 0.6f;
			}
			else if (ability.Kind == AbilityKind::Nova)
				radius = ability.Reach * context.AreaScale;
			const int32_t pulse = std::max(1, int32_t(std::lround(float(damage) * ResonanceScale)));
			world.SpawnResonance(ability, center, ability.ImpactDelay + ResonanceDelay, radius, pulse, ability.Push * ResonanceScale * context.PushScale, context);
		}
	}

	void ExecuteAbility(World& world, AbilityId id, const AbilityContext& context)
	{
		const AbilityDef& ability = GetAbility(id);
		const bool brawlDamage = world.UsesBrawlDamage() && context.Side == Team::Players;
		const int32_t damage = ScaleDamage(brawlDamage ? ability.BrawlDamage : ability.Damage, context.DamageScale);
		if (HasCard(context.Cards, CardId::ResonantGround) && ability.Spell && IsAreaSpell(id) && ability.Kind != AbilityKind::Projectile && id != AbilityId::GlacialNova)
			Resonate(world, ability, damage, context);
		if (id == AbilityId::GlacialEruption && HasCard(context.Cards, CardId::ResonantGround))
			Resonate(world, ability, damage, context);
		switch (ability.Kind)
		{
		case AbilityKind::Projectile:
		{
			const ProjectilePattern pattern = ShotPattern(ability, GetProjectilePattern(ability, context.ExtraProjectiles, context.ExtraPierce, context.ProjectileSpeedScale), context.Charge, context.ProjectileSpeedScale);
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
			if (id == AbilityId::GlacialEruption && HasCard(context.Cards, CardId::IceShrapnel))
			{
				AbilityContext shards = context;
				shards.DamageScale /= std::max(context.PushScale, 1e-3f);
				world.SpawnCardStrike(AbilityId::GlacialEruption, context.Origin, AngleOf(axis), ShrapnelDelay, ability.Reach * ShrapnelReach * context.AreaScale, ability.Push * ShrapnelScale * context.PushScale, 0, false, shards);
			}
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
				world.SpawnCardStrike(AbilityId::Comet, context.Target, AngleOf(context.Direction), ability.ImpactDelay + TwinDelay, ability.ImpactRadius * context.AreaScale * (brawlDamage ? 1.0f : TwinScale), ability.Push * TwinScale * context.PushScale, 0, true, twin);
			}
			if (id == AbilityId::Comet && HasCard(context.Cards, CardId::Aftershock))
			{
				const AbilityDef& shock = GetAbility(AbilityId::Aftershock);
				world.SpawnCardStrike(AbilityId::Aftershock, context.Target, AngleOf(context.Direction), ability.ImpactDelay + AftershockDelay, ability.ImpactRadius * AftershockRadius * context.AreaScale, shock.Push * context.PushScale, 0, true, context);
			}
			if (id == AbilityId::StormStrike && HasCard(context.Cards, CardId::ForkedLightning))
				world.SpawnCardStrike(AbilityId::StormStrike, context.Target, AngleOf(context.Direction), ability.ImpactDelay + ForkedDelay, ability.ImpactRadius * context.AreaScale, ability.Push * ForkedScale * context.PushScale, HasCard(context.Cards, CardId::StaticCharge) ? PerkStatic : 0, true, context);
			if (id == AbilityId::GravityWell && HasCard(context.Cards, CardId::Implosion))
			{
				AbilityContext burst = context;
				burst.DamageScale *= brawlDamage ? ImplosionDamage : 1.0f;
				world.SpawnCardStrike(AbilityId::WellBurst, context.Target, AngleOf(context.Direction), ability.ImpactDelay + ImplosionDelay, ability.ImpactRadius * context.AreaScale, ImplosionPush * context.PushScale, 0, false, burst);
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
				world.SpawnCardStrike(AbilityId::WarpWake, context.Departure, AngleOf(context.Direction), AfterimageDelay, AfterimageRadius, AfterimagePush * context.PushScale, 0, false, wake);
			}
			break;
		case AbilityKind::Shield:
			break;
		}
	}

	void EruptFrostNova(World& world, Vec2 fallback, const AbilityContext& context)
	{
		EntityRegistry& entities = world.GetEntities();
		std::vector<Entity> fields;
		entities.Each<Replicated>([&](Entity entity, Replicated& replicated)
		{
			if (replicated.Kind == EntityKind::Effect && replicated.Variant == uint8_t(AbilityId::GlacialNova) && replicated.Owner == context.Owner && entities.Has<Transform>(entity))
				fields.push_back(entity);
		});
		if (fields.empty())
		{
			AbilityContext eruption = context;
			eruption.Origin = fallback;
			ExecuteAbility(world, AbilityId::GlacialEruption, eruption);
			world.AddCrust(eruption.Origin, GetAbility(AbilityId::GlacialEruption).ImpactRadius * eruption.AreaScale * NovaCrustShare);
			return;
		}
		for (const Entity field : fields)
		{
			AbilityContext eruption = context;
			eruption.Origin = entities.TryGet<Transform>(field)->Position;
			eruption.Tag = entities.TryGet<Replicated>(field)->Tag;
			world.DestroyEntity(field);
			ExecuteAbility(world, AbilityId::GlacialEruption, eruption);
			world.AddCrust(eruption.Origin, GetAbility(AbilityId::GlacialEruption).ImpactRadius * eruption.AreaScale * NovaCrustShare);
		}
	}
}
