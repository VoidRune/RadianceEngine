#pragma once
#include "GameShared/Content/Definitions.h"
#include <array>
#include <cstdint>

namespace Game
{
	struct FighterMove
	{
		AbilityId Ability = AbilityId::Count;
		float MinRange = 0.0f;
		float MaxRange = 0.0f;
		float Windup = 0.5f;
		float Recovery = 0.3f;
		float Cooldown = 0.0f;
		float Lunge = 0.0f;
		float LungeSpeed = 14.0f;
		int32_t Damage = 0;
		float Lead = 0.0f;
		float Turn = 3.0f;
		float Lock = 0.25f;
		float PushScale = 0.5f;
		int8_t Next = -1;
		bool Channel = false;
		bool Opener = true;
		bool AtSelf = false;
		bool BlinkAfter = false;
		uint8_t Rank = 0;
		uint8_t Repeat = 0;
		float Spread = 0.0f;
		float Ring = 0.0f;
		float Interval = 0.0f;
		bool Track = false;
	};

	inline constexpr uint8_t RoarRank = 9;
	inline constexpr FighterMove ChampionRoar = { .Ability = AbilityId::ObeliskPulse, .MaxRange = 99.0f, .Windup = 1.3f, .Recovery = 0.7f, .Turn = 0.0f, .PushScale = 1.6f, .Opener = false, .AtSelf = true, .Rank = RoarRank };
	inline constexpr uint32_t FighterMoveSlots = 8;

	struct FighterKit
	{
		EntityKind Kind = EntityKind::CrystalDuelist;
		std::array<FighterMove, FighterMoveSlots> Moves{};
		uint8_t MoveCount = 0;
		float Range = 2.0f;
		float RangeSlack = 1.0f;
		float Retreat = 0.0f;
		float GuardSpeed = 1.0f;
		float Reaction = 0.25f;
		float AimError = 0.05f;
		float DodgeChance = 0.0f;
		float DodgeCooldown = 3.0f;
		float DodgeDistance = 3.0f;
		float DodgeSeconds = 0.25f;
		float Guard = 0.0f;
		float GuardRange = 7.0f;
		float GuardRegen = 25.0f;
		float GuardCooldown = 5.0f;
		float Poise = 60.0f;
		float BlinkCooldown = 0.0f;
		float BlinkDistance = 5.5f;
		float Pause = 0.6f;
		float Shell = 0.0f;
		float MirrorOpen = 0.0f;
		float MirrorClosed = 0.0f;
		float Aura = 0.0f;
		bool Rooted = false;
	};

	inline constexpr FighterKit FighterKits[] = {
		{
			.Kind = EntityKind::CrystalDuelist,
			.Moves = { {
				{ .Ability = AbilityId::DuelistSlash, .MinRange = 0.0f, .MaxRange = 2.5f, .Windup = 0.6f, .Recovery = 0.12f, .Cooldown = 3.2f, .Lunge = 0.9f, .Turn = 2.6f, .Next = 1 },
				{ .Ability = AbilityId::DuelistCleave, .MaxRange = 3.2f, .Windup = 0.42f, .Recovery = 0.12f, .Lunge = 0.8f, .Turn = 2.2f, .Next = 2, .Opener = false },
				{ .Ability = AbilityId::DuelistThrust, .MaxRange = 3.6f, .Windup = 0.62f, .Recovery = 1.05f, .Lunge = 1.5f, .Turn = 1.8f, .Lock = 0.35f, .Opener = false },
				{ .Ability = AbilityId::DuelistLunge, .MinRange = 3.8f, .MaxRange = 6.5f, .Windup = 0.8f, .Recovery = 0.9f, .Cooldown = 6.0f, .Lunge = 4.0f, .LungeSpeed = 14.0f, .Turn = 1.6f, .Lock = 0.4f },
				{ .Ability = AbilityId::DuelistLunge, .MinRange = 3.8f, .MaxRange = 7.5f, .Windup = 0.7f, .Recovery = 0.15f, .Cooldown = 9.0f, .Lunge = 4.5f, .LungeSpeed = 15.0f, .Turn = 1.8f, .Lock = 0.4f, .Next = 5, .Rank = 1 },
				{ .Ability = AbilityId::DuelistLunge, .MaxRange = 7.5f, .Windup = 0.45f, .Recovery = 0.9f, .Lunge = 4.5f, .LungeSpeed = 15.0f, .Turn = 2.6f, .Lock = 0.4f, .Opener = false, .Rank = 1 },
				{ .Ability = AbilityId::DuelistCleave, .MaxRange = 3.4f, .Windup = 0.75f, .Recovery = 0.8f, .Cooldown = 7.0f, .Turn = 1.5f, .PushScale = 0.8f, .Rank = 2, .Repeat = 3, .Spread = 2.094f, .Interval = 0.12f },
				ChampionRoar,
			} },
			.MoveCount = 8,
			.Range = 2.0f,
			.RangeSlack = 0.6f,
			.GuardSpeed = 0.7f,
			.Reaction = 0.22f,
			.DodgeChance = 0.35f,
			.DodgeCooldown = 3.5f,
			.DodgeDistance = 2.8f,
			.Guard = 100.0f,
			.Poise = 150.0f,
			.Pause = 0.5f,
		},
		{
			.Kind = EntityKind::ShardArcher,
			.Moves = { {
				{ .Ability = AbilityId::ArcherShot, .MinRange = 2.5f, .MaxRange = 12.5f, .Windup = 0.7f, .Recovery = 0.25f, .Cooldown = 1.5f, .Lead = 1.1f, .Turn = 4.0f },
				{ .Ability = AbilityId::ArcherVolley, .MinRange = 3.5f, .MaxRange = 10.0f, .Windup = 0.9f, .Recovery = 0.45f, .Cooldown = 5.5f, .Turn = 2.5f },
				{ .Ability = AbilityId::ArcherVolley, .MinRange = 3.5f, .MaxRange = 10.0f, .Windup = 0.9f, .Recovery = 0.6f, .Cooldown = 8.0f, .Turn = 2.5f, .Rank = 1, .Repeat = 2, .Interval = 0.6f, .Track = true },
				{ .Ability = AbilityId::ArcherShot, .MinRange = 2.5f, .MaxRange = 12.5f, .Windup = 0.9f, .Recovery = 0.4f, .Cooldown = 9.0f, .Lead = 0.8f, .Turn = 3.0f, .Rank = 2, .Repeat = 5, .Interval = 0.22f, .Track = true },
				{ .Ability = AbilityId::ArcherVolley, .MaxRange = 9.0f, .Windup = 1.0f, .Recovery = 0.6f, .Cooldown = 11.0f, .Turn = 2.0f, .Rank = 2, .Repeat = 6, .Spread = 1.047f, .Interval = 0.06f },
				ChampionRoar,
			} },
			.MoveCount = 6,
			.Range = 9.0f,
			.RangeSlack = 1.5f,
			.Retreat = 4.5f,
			.Reaction = 0.25f,
			.AimError = 0.06f,
			.DodgeChance = 0.3f,
			.DodgeCooldown = 3.0f,
			.DodgeDistance = 3.4f,
			.Poise = 90.0f,
			.Pause = 0.7f,
		},
		{
			.Kind = EntityKind::PrismMage,
			.Moves = { {
				{ .Ability = AbilityId::Comet, .MinRange = 3.0f, .MaxRange = 11.0f, .Windup = 0.8f, .Recovery = 0.35f, .Cooldown = 4.5f, .Damage = 18, .Lead = 1.2f, .Turn = 3.0f, .PushScale = 0.5f, .Channel = true },
				{ .Ability = AbilityId::ArcLightning, .MinRange = 1.5f, .MaxRange = 7.5f, .Windup = 0.75f, .Recovery = 0.3f, .Cooldown = 3.0f, .Damage = 10, .Turn = 3.5f, .Lock = 0.35f, .PushScale = 0.4f, .Channel = true },
				{ .Ability = AbilityId::GlacialEruption, .MinRange = 0.0f, .MaxRange = 3.0f, .Windup = 0.6f, .Recovery = 0.2f, .Cooldown = 6.0f, .Damage = 12, .PushScale = 0.7f, .Channel = true, .AtSelf = true, .BlinkAfter = true },
				{ .Ability = AbilityId::Comet, .MinRange = 3.0f, .MaxRange = 11.0f, .Windup = 0.85f, .Recovery = 0.4f, .Cooldown = 9.0f, .Damage = 15, .Lead = 0.8f, .Turn = 3.0f, .PushScale = 0.5f, .Channel = true, .Rank = 1, .Repeat = 3, .Ring = 2.4f, .Interval = 0.25f },
				{ .Ability = AbilityId::Comet, .MinRange = 2.0f, .MaxRange = 11.0f, .Windup = 1.0f, .Recovery = 0.5f, .Cooldown = 12.0f, .Damage = 13, .Lead = 0.4f, .Turn = 3.0f, .PushScale = 0.5f, .Channel = true, .Rank = 2, .Repeat = 7, .Ring = 3.4f, .Interval = 0.1f },
				{ .Ability = AbilityId::ArcLightning, .MinRange = 1.5f, .MaxRange = 8.0f, .Windup = 0.8f, .Recovery = 0.35f, .Cooldown = 6.0f, .Damage = 10, .Turn = 3.5f, .Lock = 0.35f, .PushScale = 0.4f, .Channel = true, .Rank = 2, .Repeat = 3, .Spread = 0.4f },
				ChampionRoar,
			} },
			.MoveCount = 7,
			.Range = 7.0f,
			.RangeSlack = 1.5f,
			.Reaction = 0.3f,
			.AimError = 0.04f,
			.Poise = 100.0f,
			.BlinkCooldown = 5.0f,
			.Pause = 0.8f,
		},
		{
			.Kind = EntityKind::GeodeBrute,
			.Moves = { {
				{ .Ability = AbilityId::BruteSlam, .MaxRange = 2.8f, .Windup = 1.1f, .Recovery = 1.2f, .Cooldown = 3.5f, .Turn = 0.0f, .PushScale = 1.0f, .AtSelf = true },
				{ .Ability = AbilityId::BruteQuake, .MinRange = 2.6f, .MaxRange = 7.0f, .Windup = 1.05f, .Recovery = 1.0f, .Cooldown = 6.0f, .Turn = 1.2f, .Lock = 0.5f, .PushScale = 1.0f },
				{ .Ability = AbilityId::GeodeToss, .MinRange = 5.0f, .MaxRange = 11.0f, .Windup = 0.7f, .Recovery = 0.6f, .Cooldown = 6.5f, .Lead = 0.9f, .Turn = 2.0f, .PushScale = 1.0f },
				{ .Ability = AbilityId::BruteQuake, .MinRange = 2.0f, .MaxRange = 7.0f, .Windup = 1.1f, .Recovery = 1.1f, .Cooldown = 9.0f, .Turn = 1.0f, .Lock = 0.5f, .PushScale = 1.0f, .Rank = 1, .Repeat = 3, .Spread = 0.45f },
				{ .Ability = AbilityId::GeodeToss, .MinRange = 3.0f, .MaxRange = 11.0f, .Windup = 0.8f, .Recovery = 0.7f, .Cooldown = 10.0f, .Lead = 0.6f, .Turn = 2.0f, .PushScale = 1.0f, .Rank = 2, .Repeat = 4, .Ring = 2.8f, .Interval = 0.25f },
				ChampionRoar,
			} },
			.MoveCount = 6,
			.Range = 1.8f,
			.RangeSlack = 0.6f,
			.Reaction = 0.4f,
			.Poise = 300.0f,
			.Pause = 0.8f,
			.Shell = 50.0f,
		},
		{
			.Kind = EntityKind::MirrorMoth,
			.Moves = { {
				{ .Ability = AbilityId::MothDust, .MinRange = 2.0f, .MaxRange = 9.0f, .Windup = 0.7f, .Recovery = 0.3f, .Cooldown = 2.6f, .Lead = 0.6f, .Turn = 3.5f },
				{ .Ability = AbilityId::MothDive, .MinRange = 2.5f, .MaxRange = 6.0f, .Windup = 0.7f, .Recovery = 0.8f, .Cooldown = 5.0f, .Lunge = 4.0f, .LungeSpeed = 13.0f, .Turn = 2.0f, .Lock = 0.4f },
				{ .Ability = AbilityId::MothDust, .MinRange = 2.0f, .MaxRange = 9.0f, .Windup = 0.75f, .Recovery = 0.4f, .Cooldown = 6.0f, .Lead = 0.5f, .Turn = 3.5f, .Rank = 1, .Repeat = 3, .Spread = 0.5f, .Interval = 0.12f },
				{ .Ability = AbilityId::MothDust, .MaxRange = 8.0f, .Windup = 0.85f, .Recovery = 0.5f, .Cooldown = 9.0f, .Turn = 3.0f, .Rank = 2, .Repeat = 8, .Spread = 0.785f, .Interval = 0.07f },
				ChampionRoar,
			} },
			.MoveCount = 5,
			.Range = 6.0f,
			.RangeSlack = 2.0f,
			.Reaction = 0.25f,
			.AimError = 0.05f,
			.DodgeChance = 0.3f,
			.DodgeCooldown = 3.0f,
			.DodgeDistance = 3.0f,
			.Poise = 80.0f,
			.Pause = 0.5f,
			.MirrorOpen = 2.8f,
			.MirrorClosed = 3.4f,
		},
		{
			.Kind = EntityKind::LensWisp,
			.Moves = { {
				{ .Ability = AbilityId::LensBeam, .MinRange = 3.0f, .MaxRange = 12.5f, .Windup = 1.3f, .Recovery = 0.6f, .Cooldown = 4.5f, .Turn = 1.1f, .Lock = 0.45f, .Next = 2, .Channel = true },
				{ .Ability = AbilityId::LensFlare, .MaxRange = 2.6f, .Windup = 0.6f, .Recovery = 0.2f, .Cooldown = 7.0f, .PushScale = 1.0f, .Channel = true, .AtSelf = true, .BlinkAfter = true },
				{ .Ability = AbilityId::LensSweep, .MaxRange = 13.0f, .Windup = 0.45f, .Recovery = 0.7f, .Turn = 0.5f, .Lock = 0.0f, .Channel = true, .Opener = false },
				{ .Ability = AbilityId::LensBeam, .MinRange = 3.0f, .MaxRange = 12.5f, .Windup = 1.3f, .Recovery = 0.7f, .Cooldown = 9.0f, .Turn = 1.0f, .Lock = 0.45f, .Channel = true, .Rank = 2, .Repeat = 3, .Spread = 0.55f },
				ChampionRoar,
			} },
			.MoveCount = 5,
			.Range = 8.0f,
			.RangeSlack = 1.5f,
			.Reaction = 0.3f,
			.DodgeChance = 0.2f,
			.DodgeCooldown = 4.0f,
			.Poise = 65.0f,
			.BlinkCooldown = 6.0f,
			.Pause = 0.9f,
		},
		{
			.Kind = EntityKind::ShardObelisk,
			.Moves = { {
				{ .Ability = AbilityId::ObeliskPulse, .MaxRange = 3.2f, .Windup = 1.0f, .Recovery = 0.5f, .Cooldown = 4.0f, .Turn = 0.0f, .PushScale = 1.0f, .AtSelf = true },
			} },
			.MoveCount = 1,
			.Range = 0.0f,
			.Reaction = 0.3f,
			.Poise = 0.0f,
			.Pause = 0.6f,
			.Aura = 7.0f,
			.Rooted = true,
		},
	};

	struct ChampionDef
	{
		EntityKind Kind;
		const char* Name;
		uint8_t MinAct;
	};

	inline constexpr ChampionDef Champions[] = {
		{ EntityKind::CrystalDuelist, "Crystal Blademaster", 1 },
		{ EntityKind::ShardArcher, "Stormfletcher", 1 },
		{ EntityKind::PrismMage, "Prism Archon", 1 },
		{ EntityKind::GeodeBrute, "Geode Colossus", 1 },
		{ EntityKind::MirrorMoth, "Mirror Queen", 2 },
		{ EntityKind::LensWisp, "The Great Lens", 2 },
	};

	inline const ChampionDef* FindChampion(EntityKind kind)
	{
		for (const ChampionDef& champion : Champions)
			if (champion.Kind == kind)
				return &champion;
		return nullptr;
	}

	inline constexpr float ChampionHealth = 5.5f;
	inline constexpr float ChampionDamage = 1.15f;
	inline constexpr float ChampionSize = 1.45f;
	inline constexpr float ChampionPoise = 3.0f;
	inline constexpr float ChampionPhases[] = { 0.66f, 0.33f };
	inline constexpr float ChampionRoarArea = 1.8f;
	inline constexpr float ChampionEnrageSpeed = 1.15f;
	inline constexpr float ChampionEnrageCooldown = 0.75f;
	inline constexpr uint8_t ChampionRank = 2;
	inline constexpr float VeteranCooldown = 0.85f;
	inline constexpr float RoarGrace = 0.3f;
	inline constexpr uint32_t RoarCrawlers = 3;

	inline constexpr float EnemyPushScale = 0.55f;
	inline constexpr float EnemyPushFriction = 4.0f;
	inline constexpr float EnemyMaxPush = 24.0f;
	inline constexpr float EnemyPushStop = 0.3f;
	inline constexpr float EnemyPushInterrupt = 7.0f;
	inline constexpr float EnemyFlungSpeed = 5.0f;
	inline constexpr float EliteWeight = 1.4f;
	inline constexpr float ChampionWeight = 6.0f;

	inline float EnemyWeight(EntityKind kind)
	{
		switch (kind)
		{
		case EntityKind::QuartzCrawler:
		case EntityKind::PrismaticCrawler:
			return 1.0f;
		case EntityKind::JadeCrawler:
			return 1.2f;
		case EntityKind::SapphireCrawler:
			return 1.5f;
		case EntityKind::RubyCrawler:
			return 1.8f;
		case EntityKind::AmethystCrawler:
			return 4.0f;
		case EntityKind::CrystalDuelist:
			return 1.6f;
		case EntityKind::ShardArcher:
			return 1.2f;
		case EntityKind::PrismMage:
			return 1.3f;
		case EntityKind::GeodeBrute:
			return 3.5f;
		case EntityKind::MirrorMoth:
		case EntityKind::LensWisp:
			return 1.0f;
		default:
			return 0.0f;
		}
	}

	inline int32_t FindRoarMove(EntityKind kind);

	enum class EliteAffix : uint8_t
	{
		Hasted,
		Warded,
		Splitting,
		Volatile,
		Vampiric,
		Reflective,
		Count,
	};

	inline constexpr const char* EliteAffixNames[] = { "Hasted", "Warded", "Splitting", "Volatile", "Vampiric", "Reflective" };
	static_assert(std::size(EliteAffixNames) == size_t(EliteAffix::Count));

	constexpr uint8_t AffixBit(EliteAffix affix)
	{
		return uint8_t(1u << uint32_t(affix));
	}

	inline bool HasAffix(uint8_t affixes, EliteAffix affix)
	{
		return (affixes & AffixBit(affix)) != 0;
	}

	inline bool AffixFits(EntityKind kind, EliteAffix affix)
	{
		const bool rooted = kind == EntityKind::ShardObelisk;
		if (rooted && (affix == EliteAffix::Hasted || affix == EliteAffix::Splitting))
			return false;
		return !(kind == EntityKind::MirrorMoth && affix == EliteAffix::Reflective);
	}

	inline constexpr float ShellArmor = 0.3f;
	inline constexpr float ShellRegrow = 6.0f;
	inline constexpr float ShellCrackStagger = 1.2f;
	inline constexpr float MirrorDamage = 0.2f;
	inline constexpr int32_t MirrorMaxDamage = 14;
	inline constexpr uint8_t MirrorReflects = 2;
	inline constexpr float EmpowerSpeed = 1.25f;
	inline constexpr float EmpowerHaste = 1.35f;
	inline constexpr float EmpowerHeal = 0.02f;
	inline constexpr float EmpowerLinger = 0.3f;
	inline constexpr float HastedSpeed = 1.3f;
	inline constexpr float HastedCooldown = 0.7f;
	inline constexpr float WardedShare = 0.35f;
	inline constexpr float WardedIdle = 5.0f;
	inline constexpr float WardedRefill = 0.25f;
	inline constexpr uint32_t SplitCount = 2;
	inline constexpr float SplitHealth = 0.6f;
	inline constexpr float VampiricHeal = 0.05f;
	inline constexpr float ReflectiveOpen = 2.0f;
	inline constexpr float ReflectiveClosed = 5.0f;

	inline constexpr float FighterStaggerSeconds = 1.5f;
	inline constexpr float FighterGuardBreakStagger = 0.9f;
	inline constexpr float FighterStaggerDamage = 1.25f;
	inline constexpr float FighterPoiseDecay = 55.0f;
	inline constexpr float FighterPoiseIdle = 2.0f;
	inline constexpr float FighterHeavyHit = 60.0f;
	inline constexpr float FighterHeavyPoise = 1.5f;
	inline constexpr float FighterChillPoise = 25.0f;
	inline constexpr float FighterInterruptRecovery = 0.7f;
	inline constexpr float FighterBlinkDelay = 0.15f;
	inline constexpr float FighterThreatWindow = 0.6f;
	inline constexpr uint32_t FighterAttackers = 2;

	inline const FighterKit* FindFighterKit(EntityKind kind)
	{
		for (const FighterKit& kit : FighterKits)
			if (kit.Kind == kind)
				return &kit;
		return nullptr;
	}

	inline bool IsFighterKind(EntityKind kind)
	{
		return FindFighterKit(kind) != nullptr;
	}

	inline const FighterMove* FindFighterMove(EntityKind kind, AbilityId ability)
	{
		if (const FighterKit* kit = FindFighterKit(kind))
			for (uint32_t i = 0; i < kit->MoveCount; i++)
				if (kit->Moves[i].Ability == ability)
					return &kit->Moves[i];
		return nullptr;
	}
	inline int32_t FindRoarMove(EntityKind kind)
	{
		if (const FighterKit* kit = FindFighterKit(kind))
			for (uint32_t i = 0; i < kit->MoveCount; i++)
				if (kit->Moves[i].Rank == RoarRank)
					return int32_t(i);
		return -1;
	}
}
