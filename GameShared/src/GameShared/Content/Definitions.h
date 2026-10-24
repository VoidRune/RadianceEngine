#pragma once
#include "GameShared/Entity/Components.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace Game
{
	enum class AbilityKind : uint8_t
	{
		Projectile,
		Melee,
		Nova,
		Dash,
		Ground,
		Heal,
		Ward,
	};

	enum class Requirement : uint8_t
	{
		None,
		Caster,
		Melee,
		Bow,
	};

	namespace Affinity
	{
		constexpr uint8_t Caster = 1 << 0;
		constexpr uint8_t Melee = 1 << 1;
		constexpr uint8_t Bow = 1 << 2;
	}

	enum class AbilityId : uint8_t
	{
		Bolt,
		Orb,
		Slash,
		Chop,
		Stab,
		Thrust,
		Arrow,
		Quarrel,
		Fireball,
		Meteor,
		FrostNova,
		IceSpear,
		ChainLightning,
		Blink,
		Cyclone,
		GroundSlam,
		Volley,
		RainOfArrows,
		PiercingShot,
		Rejuvenate,
		Roll,
		Bite,
		Spit,
		Slam,
		Firestrike,
		TomeVolley,
		PageStorm,
		ShelfCrush,
		ArcaneRing,
		ArcaneBurst,
		InkSpiral,
		CrystalSpike,
		TailSweep,
		Eruption,
		ShardBurst,
		CrystalRain,
		EmberBolt,
		Gust,
		Comet,
		Warp,
		Shockwave,
		GravityWell,
		ArcLightning,
		Swap,
		MirrorWard,
		Frostbite,
		ChainHook,
		Firebomb,
		SplittingBolt,
		StormStrike,
		FirebombSkip,
		FirebombSkip2,
		BoltShard,
		StormJump,
		HookAnchor,
		HookPull,
		HookYank,
		FirebombFlame,
		Ignite,
		ShatterBurst,
		Count,
	};

	struct AbilityDef
	{
		AbilityId Id = AbilityId::Bolt;
		const char* Name = "";
		const char* Description = "";
		AbilityKind Kind = AbilityKind::Projectile;
		Element Damages = Element::Physical;
		Requirement Needs = Requirement::None;
		bool Spell = false;
		bool Gem = false;
		float CastTime = 0.0f;
		float CastMove = 1.0f;
		float ManaCost = 0.0f;
		float Cooldown = 0.5f;
		int32_t Damage = 0;
		float ProjectileSpeed = 0.0f;
		float ProjectileRadius = 0.0f;
		float ProjectileRange = 0.0f;
		uint8_t ProjectileCount = 1;
		float ProjectileSpread = 0.0f;
		uint8_t Pierce = 0;
		uint8_t Chain = 0;
		float SpawnOffset = 0.45f;
		float Reach = 0.0f;
		float HalfArc = 0.0f;
		float Lunge = 0.0f;
		float DashDistance = 0.0f;
		float DashDuration = 0.0f;
		float Invulnerability = 0.0f;
		float EffectDuration = 0.15f;
		float ImpactRadius = 0.0f;
		float ImpactDelay = 0.0f;
		float Range = 0.0f;
		float Chill = 0.0f;
		float Stun = 0.0f;
		float Heal = 0.0f;
		StrikeShape Shape = StrikeShape::Circle;
		float Width = 0.0f;
		float InnerRadius = 0.0f;
		float PvpScale = 1.0f;
		float Push = 0.0f;
		float Ward = 0.0f;
		bool Swap = false;
		AbilityId Parent = AbilityId::Count;
		int32_t BrawlDamage = 0;
		float BrawlMana = 0.0f;
		float BrawlCooldown = 0.0f;
	};

	inline constexpr AbilityDef Abilities[] = {
		{ .Id = AbilityId::Bolt, .Name = "Spark Bolt", .Description = "Fires a crackling bolt.", .Damages = Element::Lightning, .CastTime = 0.2f, .CastMove = 0.65f, .Cooldown = 0.28f, .Damage = 22, .ProjectileSpeed = 17.0f, .ProjectileRadius = 0.14f, .ProjectileRange = 9.0f },
		{ .Id = AbilityId::Orb, .Name = "Frost Orb", .Description = "Lobs a slow orb of frost that passes through one enemy and chills.", .Damages = Element::Cold, .CastTime = 0.35f, .CastMove = 0.5f, .Cooldown = 0.35f, .Damage = 40, .ProjectileSpeed = 11.0f, .ProjectileRadius = 0.24f, .ProjectileRange = 8.5f, .Pierce = 1, .Chill = 0.8f },
		{ .Id = AbilityId::Slash, .Name = "Slash", .Description = "Steps into a sweep of the blade in a wide arc.", .Kind = AbilityKind::Melee, .CastTime = 0.18f, .CastMove = 0.55f, .Cooldown = 0.3f, .Damage = 30, .Reach = 2.8f, .HalfArc = 1.1f, .Lunge = 0.7f },
		{ .Id = AbilityId::Chop, .Name = "Chop", .Description = "A heavy overhead chop that staggers.", .Kind = AbilityKind::Melee, .CastTime = 0.35f, .CastMove = 0.4f, .Cooldown = 0.4f, .Damage = 50, .Reach = 2.8f, .HalfArc = 0.9f, .Lunge = 0.6f, .EffectDuration = 0.18f, .Stun = 0.3f, .PvpScale = 0.9f },
		{ .Id = AbilityId::Stab, .Name = "Stab", .Description = "A quick thrust.", .Kind = AbilityKind::Melee, .CastTime = 0.08f, .CastMove = 0.75f, .Cooldown = 0.22f, .Damage = 15, .Reach = 2.4f, .HalfArc = 0.5f, .Lunge = 0.5f, .EffectDuration = 0.1f },
		{ .Id = AbilityId::Thrust, .Name = "Thrust", .Description = "Lunges forward and drives the spear through everything in a long line.", .Kind = AbilityKind::Melee, .CastTime = 0.22f, .CastMove = 0.5f, .Cooldown = 0.32f, .Damage = 30, .Reach = 3.6f, .Lunge = 1.0f, .EffectDuration = 0.14f, .Shape = StrikeShape::Line, .Width = 0.9f },
		{ .Id = AbilityId::Arrow, .Name = "Arrow", .Description = "Looses an arrow that pierces one enemy.", .CastTime = 0.3f, .CastMove = 0.5f, .Cooldown = 0.15f, .Damage = 26, .ProjectileSpeed = 22.0f, .ProjectileRadius = 0.12f, .ProjectileRange = 10.0f, .Pierce = 1 },
		{ .Id = AbilityId::Quarrel, .Name = "Quarrel", .Description = "Fires a heavy bolt that pierces two enemies.", .CastTime = 0.5f, .CastMove = 0.35f, .Cooldown = 0.25f, .Damage = 55, .ProjectileSpeed = 28.0f, .ProjectileRadius = 0.1f, .ProjectileRange = 11.0f, .Pierce = 2, .PvpScale = 0.85f },
		{ .Id = AbilityId::Fireball, .Name = "Fireball", .Description = "Hurls a fireball that explodes on impact. Roots you while casting.", .Damages = Element::Fire, .Needs = Requirement::Caster, .Spell = true, .Gem = true, .CastTime = 0.5f, .CastMove = 0.0f, .ManaCost = 16.0f, .Cooldown = 0.3f, .Damage = 55, .ProjectileSpeed = 13.0f, .ProjectileRadius = 0.22f, .ProjectileRange = 9.0f, .EffectDuration = 0.3f, .ImpactRadius = 1.8f, .PvpScale = 0.8f },
		{ .Id = AbilityId::Meteor, .Name = "Meteor", .Description = "Calls a meteor down on the target area after a short delay. Stuns. Roots you while casting.", .Kind = AbilityKind::Ground, .Damages = Element::Fire, .Needs = Requirement::Caster, .Spell = true, .Gem = true, .CastTime = 0.8f, .CastMove = 0.0f, .ManaCost = 35.0f, .Cooldown = 5.0f, .Damage = 150, .EffectDuration = 0.4f, .ImpactRadius = 2.4f, .ImpactDelay = 0.7f, .Range = 9.0f, .Stun = 0.5f, .PvpScale = 0.5f },
		{ .Id = AbilityId::FrostNova, .Name = "Frost Nova", .Description = "Blasts frost around you, chilling enemies. Works with any weapon.", .Kind = AbilityKind::Nova, .Damages = Element::Cold, .Spell = true, .Gem = true, .CastTime = 0.25f, .CastMove = 0.3f, .ManaCost = 22.0f, .Cooldown = 5.0f, .Damage = 30, .Reach = 4.2f, .EffectDuration = 0.35f, .Chill = 2.5f },
		{ .Id = AbilityId::IceSpear, .Name = "Ice Spear", .Description = "A fast shard of ice that pierces three enemies and chills them.", .Damages = Element::Cold, .Needs = Requirement::Caster, .Spell = true, .Gem = true, .CastTime = 0.35f, .CastMove = 0.4f, .ManaCost = 10.0f, .Cooldown = 0.0f, .Damage = 38, .ProjectileSpeed = 20.0f, .ProjectileRadius = 0.15f, .ProjectileRange = 10.0f, .Pierce = 3, .Chill = 1.2f },
		{ .Id = AbilityId::ChainLightning, .Name = "Chain Lightning", .Description = "A bolt of lightning that jumps to 3 more enemies.", .Damages = Element::Lightning, .Needs = Requirement::Caster, .Spell = true, .Gem = true, .CastTime = 0.4f, .CastMove = 0.5f, .ManaCost = 14.0f, .Cooldown = 0.5f, .Damage = 30, .ProjectileSpeed = 24.0f, .ProjectileRadius = 0.15f, .ProjectileRange = 9.0f, .Chain = 3, .PvpScale = 0.85f },
		{ .Id = AbilityId::Blink, .Name = "Blink", .Description = "Teleports you up to 6 units toward the cursor. Works with any weapon.", .Kind = AbilityKind::Dash, .Spell = true, .Gem = true, .CastTime = 0.1f, .CastMove = 0.0f, .ManaCost = 15.0f, .Cooldown = 2.5f, .DashDistance = 6.0f, .Invulnerability = 0.2f, .Range = 6.0f },
		{ .Id = AbilityId::Cyclone, .Name = "Cyclone", .Description = "Spins with your weapon, hitting everything around you. You keep most of your speed.", .Kind = AbilityKind::Nova, .Needs = Requirement::Melee, .Gem = true, .CastTime = 0.2f, .CastMove = 0.8f, .ManaCost = 10.0f, .Cooldown = 0.9f, .Damage = 34, .Reach = 2.8f, .EffectDuration = 0.25f },
		{ .Id = AbilityId::GroundSlam, .Name = "Ground Slam", .Description = "Slams the ground in a long cone after a heavy windup. Stuns. Roots you while casting.", .Kind = AbilityKind::Melee, .Needs = Requirement::Melee, .Gem = true, .CastTime = 0.6f, .CastMove = 0.0f, .ManaCost = 20.0f, .Cooldown = 3.0f, .Damage = 110, .Reach = 4.0f, .HalfArc = 0.7f, .EffectDuration = 0.3f, .Stun = 1.2f, .PvpScale = 0.55f },
		{ .Id = AbilityId::Volley, .Name = "Volley", .Description = "Fires five arrows in a spread.", .Needs = Requirement::Bow, .Gem = true, .CastTime = 0.35f, .CastMove = 0.5f, .ManaCost = 12.0f, .Cooldown = 1.0f, .Damage = 24, .ProjectileSpeed = 21.0f, .ProjectileRadius = 0.12f, .ProjectileRange = 8.0f, .ProjectileCount = 5, .ProjectileSpread = 0.6f, .PvpScale = 0.75f },
		{ .Id = AbilityId::RainOfArrows, .Name = "Rain of Arrows", .Description = "Arrows rain on the target area after a short delay.", .Kind = AbilityKind::Ground, .Needs = Requirement::Bow, .Gem = true, .CastTime = 0.45f, .CastMove = 0.3f, .ManaCost = 20.0f, .Cooldown = 4.0f, .Damage = 70, .EffectDuration = 0.35f, .ImpactRadius = 2.3f, .ImpactDelay = 0.45f, .Range = 9.5f, .PvpScale = 0.75f },
		{ .Id = AbilityId::PiercingShot, .Name = "Piercing Shot", .Description = "A fully drawn shot that pierces six enemies and staggers them. Roots you while drawing.", .Needs = Requirement::Bow, .Gem = true, .CastTime = 0.75f, .CastMove = 0.0f, .ManaCost = 16.0f, .Cooldown = 2.0f, .Damage = 130, .ProjectileSpeed = 32.0f, .ProjectileRadius = 0.14f, .ProjectileRange = 11.5f, .Pierce = 6, .Stun = 0.4f, .PvpScale = 0.6f },
		{ .Id = AbilityId::Rejuvenate, .Name = "Rejuvenate", .Description = "Restores 35% of your health. Works with any weapon. Roots you while casting.", .Kind = AbilityKind::Heal, .Spell = true, .Gem = true, .CastTime = 0.6f, .CastMove = 0.0f, .ManaCost = 30.0f, .Cooldown = 12.0f, .EffectDuration = 0.5f, .Heal = 0.35f },
		{ .Id = AbilityId::Roll, .Name = "Dodge", .Description = "Dodge roll, briefly invulnerable.", .Kind = AbilityKind::Dash, .Cooldown = 1.1f, .DashDistance = 3.2f, .DashDuration = 0.22f, .Invulnerability = 0.28f },
		{ .Id = AbilityId::Bite, .Name = "Bite", .Kind = AbilityKind::Melee, .Cooldown = 1.4f, .Damage = 14, .Reach = 2.0f, .HalfArc = 0.9f, .EffectDuration = 0.15f },
		{ .Id = AbilityId::Spit, .Name = "Spit", .Damages = Element::Poison, .Cooldown = 2.2f, .Damage = 13, .ProjectileSpeed = 8.0f, .ProjectileRadius = 0.2f, .ProjectileRange = 8.5f, .SpawnOffset = 0.4f },
		{ .Id = AbilityId::Slam, .Name = "Slam", .Kind = AbilityKind::Nova, .Cooldown = 2.8f, .Damage = 34, .Reach = 2.3f, .EffectDuration = 0.3f },
		{ .Id = AbilityId::Firestrike, .Name = "Firestrike", .Kind = AbilityKind::Ground, .Damages = Element::Fire, .Cooldown = 3.2f, .Damage = 26, .EffectDuration = 0.35f, .ImpactRadius = 1.7f, .ImpactDelay = 0.85f, .Range = 9.0f },
		{ .Id = AbilityId::TomeVolley, .Name = "Tome Volley", .Damages = Element::Lightning, .Cooldown = 0.0f, .Damage = 24, .ProjectileSpeed = 9.0f, .ProjectileRadius = 0.32f, .ProjectileRange = 12.5f, .SpawnOffset = 1.0f },
		{ .Id = AbilityId::PageStorm, .Name = "Page Storm", .Kind = AbilityKind::Ground, .Damages = Element::Lightning, .Cooldown = 0.0f, .Damage = 40, .EffectDuration = 0.35f, .ImpactRadius = 2.0f, .ImpactDelay = 1.25f },
		{ .Id = AbilityId::ShelfCrush, .Name = "Shelf Crush", .Kind = AbilityKind::Ground, .Cooldown = 0.0f, .Damage = 55, .Reach = 12.0f, .EffectDuration = 0.4f, .ImpactDelay = 1.1f, .Shape = StrikeShape::Line, .Width = 2.4f },
		{ .Id = AbilityId::ArcaneRing, .Name = "Arcane Ring", .Kind = AbilityKind::Ground, .Damages = Element::Lightning, .Cooldown = 0.0f, .Damage = 50, .EffectDuration = 0.45f, .ImpactRadius = 10.0f, .ImpactDelay = 1.5f, .Shape = StrikeShape::Ring, .InnerRadius = 3.5f },
		{ .Id = AbilityId::ArcaneBurst, .Name = "Arcane Burst", .Kind = AbilityKind::Ground, .Damages = Element::Lightning, .Cooldown = 0.0f, .Damage = 55, .EffectDuration = 0.45f, .ImpactRadius = 4.5f, .ImpactDelay = 1.5f },
		{ .Id = AbilityId::InkSpiral, .Name = "Ink Spiral", .Damages = Element::Poison, .Cooldown = 0.0f, .Damage = 20, .ProjectileSpeed = 7.0f, .ProjectileRadius = 0.28f, .ProjectileRange = 13.0f, .SpawnOffset = 1.0f },
		{ .Id = AbilityId::CrystalSpike, .Name = "Crystal Spike", .Kind = AbilityKind::Ground, .Damages = Element::Cold, .Cooldown = 0.0f, .Damage = 45, .Reach = 11.0f, .EffectDuration = 0.4f, .ImpactDelay = 0.95f, .Shape = StrikeShape::Line, .Width = 1.7f },
		{ .Id = AbilityId::TailSweep, .Name = "Tail Sweep", .Kind = AbilityKind::Ground, .Cooldown = 0.0f, .Damage = 60, .Reach = 5.5f, .HalfArc = 1.2f, .EffectDuration = 0.35f, .ImpactDelay = 0.8f, .Shape = StrikeShape::Cone },
		{ .Id = AbilityId::Eruption, .Name = "Eruption", .Kind = AbilityKind::Ground, .Cooldown = 0.0f, .Damage = 70, .EffectDuration = 0.45f, .ImpactRadius = 3.2f, .ImpactDelay = 1.0f },
		{ .Id = AbilityId::ShardBurst, .Name = "Shard Burst", .Damages = Element::Cold, .Cooldown = 0.0f, .Damage = 22, .ProjectileSpeed = 10.0f, .ProjectileRadius = 0.25f, .ProjectileRange = 13.0f, .SpawnOffset = 1.1f },
		{ .Id = AbilityId::CrystalRain, .Name = "Crystal Rain", .Kind = AbilityKind::Ground, .Damages = Element::Cold, .Cooldown = 0.0f, .Damage = 40, .EffectDuration = 0.35f, .ImpactRadius = 1.9f, .ImpactDelay = 1.35f },
		{ .Id = AbilityId::EmberBolt, .Name = "Ember Bolt", .Description = "A quick bolt of fire that shoves whoever it hits.", .Damages = Element::Fire, .Spell = true, .CastTime = 0.18f, .CastMove = 0.6f, .Cooldown = 0.9f, .ProjectileSpeed = 14.5f, .ProjectileRadius = 0.24f, .ProjectileRange = 10.0f, .EffectDuration = 0.2f, .Push = 11.5f, .BrawlDamage = 6, .BrawlMana = 8.0f },
		{ .Id = AbilityId::Gust, .Name = "Gust", .Description = "A blast of wind in front of you that hurls rivals away.", .Kind = AbilityKind::Melee, .Spell = true, .CastTime = 0.18f, .CastMove = 0.5f, .Cooldown = 7.5f, .Reach = 3.3f, .HalfArc = 0.7f, .EffectDuration = 0.3f, .Push = 13.0f, .BrawlDamage = 10, .BrawlMana = 25.0f },
		{ .Id = AbilityId::Comet, .Name = "Comet", .Description = "Calls a comet down on the target area. The impact flings everyone near it.", .Kind = AbilityKind::Ground, .Damages = Element::Fire, .Spell = true, .CastTime = 0.35f, .CastMove = 0.3f, .Cooldown = 5.0f, .EffectDuration = 0.4f, .ImpactRadius = 2.8f, .ImpactDelay = 0.7f, .Range = 10.0f, .Push = 22.0f, .BrawlDamage = 20, .BrawlMana = 30.0f },
		{ .Id = AbilityId::Warp, .Name = "Warp", .Description = "Teleports you up to 5 units toward the cursor. Your way back out of the lava.", .Kind = AbilityKind::Dash, .Spell = true, .CastTime = 0.08f, .CastMove = 0.0f, .Cooldown = 15.0f, .DashDistance = 5.0f, .Invulnerability = 0.15f, .Range = 5.0f, .BrawlDamage = 0, .BrawlMana = 20.0f },
		{ .Id = AbilityId::Shockwave, .Name = "Shockwave", .Description = "Blasts everyone around you away.", .Kind = AbilityKind::Nova, .Damages = Element::Lightning, .Spell = true, .CastTime = 0.2f, .CastMove = 0.3f, .Cooldown = 8.0f, .Reach = 3.3f, .EffectDuration = 0.35f, .Push = 13.5f, .BrawlDamage = 11, .BrawlMana = 30.0f },
		{ .Id = AbilityId::GravityWell, .Name = "Gravity Well", .Description = "Opens a well at the target that slings everyone inside through its center.", .Kind = AbilityKind::Ground, .Spell = true, .CastTime = 0.3f, .CastMove = 0.3f, .Cooldown = 4.5f, .EffectDuration = 0.45f, .ImpactRadius = 4.0f, .ImpactDelay = 0.5f, .Range = 9.0f, .Push = -1.7f, .BrawlDamage = 8, .BrawlMana = 30.0f },
		{ .Id = AbilityId::ArcLightning, .Name = "Arc Lightning", .Description = "A bolt that jumps between up to three rivals and jolts each of them.", .Damages = Element::Lightning, .Spell = true, .CastTime = 0.25f, .CastMove = 0.6f, .Cooldown = 4.5f, .ProjectileSpeed = 22.0f, .ProjectileRadius = 0.16f, .ProjectileRange = 9.5f, .Chain = 2, .Push = 9.0f, .BrawlDamage = 8, .BrawlMana = 25.0f },
		{ .Id = AbilityId::Swap, .Name = "Swap", .Description = "A bolt that trades places with the first rival it touches.", .Spell = true, .CastTime = 0.18f, .CastMove = 0.5f, .Cooldown = 4.0f, .ProjectileSpeed = 22.0f, .ProjectileRadius = 0.22f, .ProjectileRange = 11.0f, .EffectDuration = 0.4f, .Swap = true, .BrawlDamage = 14, .BrawlMana = 15.0f },
		{ .Id = AbilityId::MirrorWard, .Name = "Mirror Ward", .Description = "For a second nothing can shove you, and spells that hit you fly back at whoever cast them.", .Kind = AbilityKind::Ward, .Spell = true, .CastTime = 0.0f, .CastMove = 1.0f, .Cooldown = 18.0f, .EffectDuration = 0.3f, .Ward = 1.0f, .BrawlDamage = 0, .BrawlMana = 50.0f, .BrawlCooldown = 25.0f },
		{ .Id = AbilityId::Frostbite, .Name = "Frostbite", .Description = "An icy shard that nudges its target and slows it for a second.", .Damages = Element::Cold, .Spell = true, .CastTime = 0.2f, .CastMove = 0.6f, .Cooldown = 3.5f, .ProjectileSpeed = 16.0f, .ProjectileRadius = 0.2f, .ProjectileRange = 10.0f, .Chill = 2.0f, .Push = 7.0f, .BrawlDamage = 8, .BrawlMana = 15.0f },
		{ .Id = AbilityId::ChainHook, .Name = "Chain Hook", .Description = "Throws a hook that drags the first rival it catches to you, and past you. Press again to yank yourself to the hook instead.", .Spell = true, .CastTime = 0.2f, .CastMove = 0.5f, .Cooldown = 4.5f, .ProjectileSpeed = 15.0f, .ProjectileRadius = 0.26f, .ProjectileRange = 8.0f, .EffectDuration = 0.3f, .BrawlDamage = 14, .BrawlMana = 15.0f },
		{ .Id = AbilityId::Firebomb, .Name = "Firebomb", .Description = "Lobs a bomb that bursts where it lands, skips on and bursts again.", .Kind = AbilityKind::Ground, .Damages = Element::Fire, .Spell = true, .CastTime = 0.25f, .CastMove = 0.5f, .Cooldown = 4.0f, .EffectDuration = 0.35f, .ImpactRadius = 2.0f, .ImpactDelay = 0.5f, .Range = 8.5f, .Push = 19.0f, .BrawlDamage = 10, .BrawlMana = 25.0f },
		{ .Id = AbilityId::SplittingBolt, .Name = "Splitting Bolt", .Description = "A slow, heavy bolt. Press again to split it into three fast shards.", .Spell = true, .CastTime = 0.22f, .CastMove = 0.55f, .Cooldown = 5.0f, .ProjectileSpeed = 8.0f, .ProjectileRadius = 0.34f, .ProjectileRange = 9.0f, .EffectDuration = 0.25f, .Push = 14.0f, .BrawlDamage = 12, .BrawlMana = 25.0f },
		{ .Id = AbilityId::StormStrike, .Name = "Storm Strike", .Description = "Lightning hits the target a moment after you cast, then jumps once to the nearest rival.", .Kind = AbilityKind::Ground, .Damages = Element::Lightning, .Spell = true, .CastTime = 0.15f, .CastMove = 0.55f, .Cooldown = 5.0f, .EffectDuration = 0.35f, .ImpactRadius = 1.3f, .ImpactDelay = 0.3f, .Range = 8.5f, .Push = 13.0f, .BrawlDamage = 6, .BrawlMana = 45.0f },
		{ .Id = AbilityId::FirebombSkip, .Name = "Firebomb", .Kind = AbilityKind::Ground, .Damages = Element::Fire, .EffectDuration = 0.35f, .ImpactRadius = 2.0f, .ImpactDelay = 0.95f, .Push = 19.0f, .Parent = AbilityId::Firebomb, .BrawlDamage = 9 },
		{ .Id = AbilityId::FirebombSkip2, .Name = "Firebomb", .Kind = AbilityKind::Ground, .Damages = Element::Fire, .EffectDuration = 0.35f, .ImpactRadius = 2.4f, .ImpactDelay = 1.4f, .Push = 22.0f, .Parent = AbilityId::Firebomb, .BrawlDamage = 16 },
		{ .Id = AbilityId::BoltShard, .Name = "Splitting Bolt", .ProjectileSpeed = 15.0f, .ProjectileRadius = 0.2f, .ProjectileRange = 4.5f, .EffectDuration = 0.2f, .Push = 7.5f, .Parent = AbilityId::SplittingBolt, .BrawlDamage = 5 },
		{ .Id = AbilityId::StormJump, .Name = "Storm Strike", .Damages = Element::Lightning, .EffectDuration = 0.3f, .Range = 4.0f, .Push = 10.0f, .Parent = AbilityId::StormStrike, .BrawlDamage = 3 },
		{ .Id = AbilityId::HookAnchor, .Name = "Chain Hook", .EffectDuration = 1.0f, .Parent = AbilityId::ChainHook },
		{ .Id = AbilityId::HookPull, .Name = "Chain Hook", .EffectDuration = 0.3f, .Parent = AbilityId::ChainHook },
		{ .Id = AbilityId::HookYank, .Name = "Chain Hook", .EffectDuration = 0.35f, .Parent = AbilityId::ChainHook },
		{ .Id = AbilityId::FirebombFlame, .Name = "Firebomb", .Kind = AbilityKind::Ground, .Damages = Element::Fire, .EffectDuration = 0.3f, .ImpactRadius = 1.3f, .Parent = AbilityId::Firebomb, .BrawlDamage = 2 },
		{ .Id = AbilityId::Ignite, .Name = "Kindling", .Damages = Element::Fire },
		{ .Id = AbilityId::ShatterBurst, .Name = "Shatter", .Kind = AbilityKind::Nova, .Damages = Element::Cold, .Reach = 2.5f, .EffectDuration = 0.35f, .ImpactRadius = 2.5f },
	};

	constexpr bool AbilitiesMatchIds()
	{
		for (size_t i = 0; i < std::size(Abilities); i++)
			if (size_t(Abilities[i].Id) != i)
				return false;
		return std::size(Abilities) == size_t(AbilityId::Count);
	}
	static_assert(AbilitiesMatchIds());

	inline const AbilityDef& GetAbility(AbilityId id)
	{
		return Abilities[size_t(id) < std::size(Abilities) ? size_t(id) : 0];
	}

	inline AbilityId CreditOf(AbilityId id)
	{
		const AbilityId parent = GetAbility(id).Parent;
		return parent != AbilityId::Count ? parent : id;
	}

	inline float GetProjectileLifetime(const AbilityDef& ability, float speed)
	{
		return speed > 0.0f ? ability.ProjectileRange / speed : 0.0f;
	}

	inline bool MeetsRequirement(Requirement needs, uint8_t affinity)
	{
		switch (needs)
		{
		case Requirement::Caster:
			return (affinity & Affinity::Caster) != 0;
		case Requirement::Melee:
			return (affinity & Affinity::Melee) != 0;
		case Requirement::Bow:
			return (affinity & Affinity::Bow) != 0;
		default:
			return true;
		}
	}

	inline const char* DescribeRequirement(Requirement needs)
	{
		switch (needs)
		{
		case Requirement::Caster:
			return "a wand, staff or dagger";
		case Requirement::Melee:
			return "a sword, axe, spear or dagger";
		case Requirement::Bow:
			return "a bow or crossbow";
		default:
			return "any weapon";
		}
	}

	inline const char* GetElementName(Element element)
	{
		constexpr const char* Names[] = { "physical", "fire", "cold", "lightning", "poison" };
		return Names[size_t(element) < std::size(Names) ? size_t(element) : 0];
	}

	struct EnemyDef
	{
		EntityKind Kind = EntityKind::Chaser;
		const char* Name = "";
		int32_t Health = 60;
		float Radius = 0.38f;
		float Speed = 3.4f;
		AbilityId Attack = AbilityId::Bite;
		float AttackRange = 1.4f;
		float Windup = 0.5f;
		float LootWeight = 1.0f;
		float OrbChance = 0.12f;
		float BuffChance = 0.015f;
		bool Boss = false;
		const char* Title = "";
	};

	inline constexpr EnemyDef EnemyDefs[] = {
		{ .Kind = EntityKind::Chaser, .Name = "Ghoul" },
		{ .Kind = EntityKind::Shooter, .Name = "Spitter", .Health = 45, .Radius = 0.34f, .Speed = 2.7f, .Attack = AbilityId::Spit, .AttackRange = 6.5f, .Windup = 0.65f, .LootWeight = 1.2f },
		{ .Kind = EntityKind::Brute, .Name = "Ogre", .Health = 220, .Radius = 0.6f, .Speed = 2.2f, .Attack = AbilityId::Slam, .AttackRange = 1.6f, .Windup = 1.0f, .LootWeight = 3.0f, .OrbChance = 0.45f, .BuffChance = 0.12f },
		{ .Kind = EntityKind::Caster, .Name = "Pyromancer", .Health = 55, .Radius = 0.36f, .Speed = 2.5f, .Attack = AbilityId::Firestrike, .AttackRange = 7.0f, .Windup = 0.8f, .LootWeight = 1.5f, .OrbChance = 0.2f },
		{ .Kind = EntityKind::Archivist, .Name = "The Archivist", .Health = 6000, .Radius = 0.85f, .Speed = 2.6f, .Attack = AbilityId::TomeVolley, .AttackRange = 10.0f, .Windup = 0.7f, .LootWeight = 0.0f, .OrbChance = 0.0f, .BuffChance = 0.0f, .Boss = true, .Title = "Keeper of the Burned Library" },
		{ .Kind = EntityKind::Wyrm, .Name = "Crystal Wyrm", .Health = 7500, .Radius = 1.0f, .Speed = 3.0f, .Attack = AbilityId::CrystalSpike, .AttackRange = 9.0f, .Windup = 0.8f, .LootWeight = 0.0f, .OrbChance = 0.0f, .BuffChance = 0.0f, .Boss = true, .Title = "Devourer of the Deep Veins" },
	};

	inline const EnemyDef* FindEnemyDef(EntityKind kind)
	{
		for (const EnemyDef& def : EnemyDefs)
			if (def.Kind == kind)
				return &def;
		return nullptr;
	}

	inline bool IsEnemyKind(EntityKind kind)
	{
		return FindEnemyDef(kind) != nullptr;
	}

	inline bool IsBossKind(EntityKind kind)
	{
		const EnemyDef* def = FindEnemyDef(kind);
		return def && def->Boss;
	}

	struct EnemyScaling
	{
		float Health = 1.0f;
		float Damage = 1.0f;
		float Speed = 1.0f;
		float Cooldown = 1.0f;
		bool Elite = false;
		uint32_t Level = 1;
		uint8_t ExtraProjectiles = 0;
	};

	inline constexpr float EliteSize = 1.2f;
	inline constexpr float EliteAreaScale = 1.3f;
	inline constexpr uint8_t EliteExtraProjectiles = 2;

	inline EnemyScaling GetEnemyScaling(uint32_t wave, uint32_t players, bool elite)
	{
		const float steps = float(wave > 0 ? wave - 1 : 0);
		const float crowd = float(players > 0 ? players - 1 : 0);
		EnemyScaling scaling;
		scaling.Health = (1.0f + 0.08f * steps + 0.012f * steps * steps) * (1.0f + 0.5f * crowd) * (elite ? 2.5f : 1.0f);
		scaling.Damage = (1.0f + 0.06f * steps + 0.004f * steps * steps) * (elite ? 1.4f : 1.0f);
		scaling.Speed = std::min(1.0f + 0.02f * steps, 1.3f) * (elite ? 1.1f : 1.0f);
		scaling.Cooldown = std::max(1.0f - 0.02f * steps, 0.6f);
		scaling.Elite = elite;
		scaling.Level = std::clamp(wave, 1u, 30u);
		return scaling;
	}

	struct LootDef
	{
		LootKind Kind = LootKind::Health;
		const char* Name = "";
		const char* Effect = "";
		uint8_t MaxStacks = 0;
	};

	inline constexpr LootDef LootDefs[] = {
		{ LootKind::Health, "Health orb", "heals 35", 0 },
		{ LootKind::Might, "Might", "+8% damage", 10 },
		{ LootKind::Haste, "Haste", "+6% attack and cast speed", 10 },
		{ LootKind::Swiftness, "Swiftness", "+4% move speed", 8 },
		{ LootKind::Vitality, "Vitality", "+15 max health", 10 },
		{ LootKind::Mana, "Mana orb", "restores 40 mana", 0 },
		{ LootKind::Item, "Item", "", 0 },
	};
	static_assert(std::size(LootDefs) == size_t(LootKind::Count));

	inline constexpr float PlayerRadius = 0.35f;
	inline constexpr float RespawnDelay = 5.0f;
	inline constexpr int32_t BaseHealth = 120;
	inline constexpr float BaseMana = 100.0f;
	inline constexpr float BaseManaRegen = 5.0f;
	inline constexpr float BaseMoveSpeed = 4.5f;
	inline constexpr int32_t HealthOrbHeal = 35;
	inline constexpr float ManaOrbRestore = 40.0f;
	inline constexpr int32_t VitalityHealth = 15;
	inline constexpr float MightPerStack = 0.08f;
	inline constexpr float HastePerStack = 0.06f;
	inline constexpr float SwiftnessPerStack = 0.04f;
	inline constexpr float LootLifetime = 30.0f;
	inline constexpr float ItemLootLifetime = 300.0f;
	inline constexpr float LootPickupRadius = 0.8f;

	inline int32_t ScaleDamage(int32_t damage, float scale)
	{
		return int32_t(std::lround(float(damage) * scale));
	}
}
