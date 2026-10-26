#pragma once
#include "GameShared/Content/Definitions.h"
#include "GameShared/Content/Items.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace Game
{
	inline constexpr float PvpDamageScale = 0.25f;
	inline constexpr float PvpPowerExponent = 0.7f;
	inline constexpr float PvpCritScale = 0.5f;
	inline constexpr float PvpLifeStealScale = 0.5f;
	inline constexpr float PvpMaxHitFraction = 0.5f;
	inline constexpr float PvpMaxArmorReduction = 0.5f;
	inline constexpr float PvpStunScale = 0.35f;
	inline constexpr float PvpMaxStun = 0.5f;
	inline constexpr float PvpStunGuard = 2.0f;
	inline constexpr float PvpChillScale = 0.5f;
	inline constexpr float PvpMaxChill = 1.5f;
	inline constexpr float ChilledMoveScale = 0.7f;
	inline constexpr float SpawnProtection = 2.0f;
	inline constexpr float ArenaRespawnDelay = 3.0f;
	inline constexpr float ArenaCountdown = 5.0f;
	inline constexpr float RoyaleCountdown = 10.0f;
	inline constexpr float MatchEndSeconds = 12.0f;
	inline constexpr float KillCreditSeconds = 10.0f;
	inline constexpr float RewardMinimumSeconds = 45.0f;
	inline constexpr float RoyaleRelevance = 40.0f;

	inline float GetPvpScale(const AbilityDef& ability, float weaponPower)
	{
		return PvpDamageScale * ability.PvpScale / std::pow(std::max(weaponPower, 1.0f), PvpPowerExponent);
	}

	inline float PvpStunDuration(float stun)
	{
		return std::min(stun * PvpStunScale, PvpMaxStun);
	}

	inline float PvpChillDuration(float chill)
	{
		return std::min(chill * PvpChillScale, PvpMaxChill);
	}

	struct KillRecord
	{
		uint32_t Killer = NoPlayer;
		uint32_t Victim = NoPlayer;
		uint8_t Ability = uint8_t(AbilityId::Count);
	};

	enum class MatchPhase : uint8_t
	{
		Countdown,
		Fight,
		Ended,
	};

	enum class BrawlMode : uint8_t
	{
		Damage,
		Knockback,
		Count,
	};

	inline constexpr const char* BrawlModeNames[] = { "damage", "knockback" };
	static_assert(std::size(BrawlModeNames) == size_t(BrawlMode::Count));

	inline constexpr uint8_t MaxTeams = 4;
	inline constexpr uint8_t SpectatorSeat = 254;
	using MatchSeats = std::array<uint8_t, MaxPlayers>;

	inline constexpr MatchSeats AutoSeats = []
	{
		MatchSeats seats{};
		seats.fill(NoTeam);
		return seats;
	}();

	struct MatchRules
	{
		uint8_t Teams = 0;
		uint8_t ScoreGoal = 15;
		uint8_t Minutes = 8;
		uint8_t Rounds = 5;
		uint8_t Bots = 0;
		uint8_t BotLevel = 1;
		BrawlMode Mode = BrawlMode::Damage;
		uint16_t Health = 100;
		uint16_t Mana = 100;
		uint16_t Damage = 100;
		uint16_t CastSpeed = 100;
		uint16_t CooldownSpeed = 100;
		uint16_t MoveSpeed = 100;
		uint16_t ManaRegen = 100;
		uint16_t Knockback = 100;
		bool Apex = true;
		bool ApexDraft = false;
		bool Crumble = true;
		bool WinnerCard = true;
		bool RandomSpells = false;
		std::array<uint8_t, MaxTeams> TeamBots{};
		MatchSeats Seats = AutoSeats;
	};

	inline constexpr uint8_t MinScoreGoal = 5;
	inline constexpr uint8_t MaxScoreGoal = 50;
	inline constexpr uint8_t MinMatchMinutes = 3;
	inline constexpr uint8_t MaxMatchMinutes = 20;
	inline constexpr uint8_t MinBrawlRounds = 3;
	inline constexpr uint8_t MaxBrawlRounds = 15;
	inline constexpr uint8_t MaxBots = 11;
	inline constexpr uint8_t BotLevels = 3;
	inline constexpr uint16_t MinBrawlHealth = 25;
	inline constexpr uint16_t MaxBrawlHealth = 500;
	inline constexpr uint16_t MinBrawlMana = 25;
	inline constexpr uint16_t MaxBrawlMana = 500;
	inline constexpr uint16_t BrawlPoolStep = 25;
	inline constexpr uint16_t MinDamagePercent = 25;
	inline constexpr uint16_t MaxDamagePercent = 300;
	inline constexpr uint16_t DamagePercentStep = 25;
	inline constexpr uint16_t MinSpeedPercent = 50;
	inline constexpr uint16_t MaxSpeedPercent = 300;
	inline constexpr uint16_t MaxMovePercent = 200;
	inline constexpr uint16_t MaxRegenPercent = 300;
	inline constexpr uint16_t MinKnockbackPercent = 25;
	inline constexpr uint16_t MaxKnockbackPercent = 300;
	inline constexpr uint16_t TuningStep = 25;

	inline MatchRules SanitizeRules(MatchRules rules)
	{
		rules.Teams = rules.Teams >= 2 ? std::min(rules.Teams, MaxTeams) : uint8_t(0);
		rules.ScoreGoal = std::clamp(rules.ScoreGoal, MinScoreGoal, MaxScoreGoal);
		rules.Minutes = std::clamp(rules.Minutes, MinMatchMinutes, MaxMatchMinutes);
		rules.Rounds = std::clamp(rules.Rounds, MinBrawlRounds, MaxBrawlRounds);
		rules.Bots = std::min(rules.Bots, MaxBots);
		rules.BotLevel = std::min<uint8_t>(rules.BotLevel, BotLevels - 1);
		if (rules.Mode >= BrawlMode::Count)
			rules.Mode = BrawlMode::Damage;
		rules.Health = std::clamp(rules.Health, MinBrawlHealth, MaxBrawlHealth);
		rules.Mana = std::clamp(rules.Mana, MinBrawlMana, MaxBrawlMana);
		rules.Damage = std::clamp(rules.Damage, MinDamagePercent, MaxDamagePercent);
		rules.CastSpeed = std::clamp(rules.CastSpeed, MinSpeedPercent, MaxSpeedPercent);
		rules.CooldownSpeed = std::clamp(rules.CooldownSpeed, MinSpeedPercent, MaxSpeedPercent);
		rules.MoveSpeed = std::clamp(rules.MoveSpeed, MinSpeedPercent, MaxMovePercent);
		rules.ManaRegen = std::min(rules.ManaRegen, MaxRegenPercent);
		rules.Knockback = std::clamp(rules.Knockback, MinKnockbackPercent, MaxKnockbackPercent);
		rules.ApexDraft = rules.ApexDraft && rules.Apex;
		uint32_t bots = 0;
		for (uint8_t team = 0; team < MaxTeams; team++)
		{
			rules.TeamBots[team] = team < rules.Teams ? uint8_t(std::min<uint32_t>(rules.TeamBots[team], MaxBots - bots)) : uint8_t(0);
			bots += rules.TeamBots[team];
		}
		for (uint8_t& seat : rules.Seats)
			if (seat != SpectatorSeat && seat >= rules.Teams)
				seat = NoTeam;
		return rules;
	}

	inline uint32_t CountTeamBots(const MatchRules& rules)
	{
		uint32_t bots = 0;
		for (uint8_t team = 0; team < rules.Teams; team++)
			bots += rules.TeamBots[team];
		return bots;
	}

	inline uint32_t CountRuleBots(const MatchRules& rules)
	{
		return rules.Teams >= 2 ? CountTeamBots(rules) : rules.Bots;
	}

	inline const char* GetTeamName(uint8_t team)
	{
		constexpr const char* Names[] = { "Azure", "Crimson", "Verdant", "Amber" };
		static_assert(std::size(Names) == MaxTeams);
		return team < std::size(Names) ? Names[team] : "No team";
	}

	struct StormStage
	{
		float Wait = 0.0f;
		float Shrink = 0.0f;
		float Scale = 0.5f;
		float Damage = 0.02f;
	};

	inline constexpr StormStage StormStages[] = {
		{ 55.0f, 40.0f, 0.62f, 0.02f },
		{ 45.0f, 35.0f, 0.58f, 0.03f },
		{ 40.0f, 30.0f, 0.52f, 0.05f },
		{ 32.0f, 25.0f, 0.48f, 0.07f },
		{ 25.0f, 22.0f, 0.4f, 0.1f },
		{ 20.0f, 25.0f, 0.0f, 0.15f },
	};
	inline constexpr uint8_t StormStageCount = uint8_t(std::size(StormStages));
	inline constexpr float StormFinalDamage = 0.25f;

	struct StormState
	{
		Vec2 Center;
		float Radius = 0.0f;
		Vec2 Target;
		float TargetRadius = 0.0f;
		uint8_t Stage = 0;
		float Timer = 0.0f;
		bool Shrinking = false;
		float Damage = 0.0f;

		bool Contains(Vec2 position) const { return LengthSquared(position - Center) <= Radius * Radius; }
	};

	struct ChestDef
	{
		ChestKind Kind = ChestKind::Common;
		const char* Name = "";
		uint8_t Items = 2;
		Rarity Minimum = Rarity::Uncommon;
		float RarityBonus = 0.0f;
		uint8_t LevelBonus = 0;
		float GemChance = 0.0f;
		float OrbChance = 0.0f;
	};

	inline constexpr ChestDef ChestDefs[] = {
		{ ChestKind::Boss, "Boss Chest", 2, Rarity::Rare, 0.5f, 0, 0.0f, 0.0f },
		{ ChestKind::Common, "Chest", 2, Rarity::Uncommon, 0.0f, 0, 0.35f, 0.5f },
		{ ChestKind::Vault, "Vault", 3, Rarity::Rare, 0.4f, 3, 1.0f, 1.0f },
		{ ChestKind::Cache, "Ember Cache", 4, Rarity::Legendary, 0.8f, 6, 1.0f, 1.0f },
	};
	static_assert(std::size(ChestDefs) == size_t(ChestKind::Count));

	inline const ChestDef& GetChestDef(ChestKind kind)
	{
		return ChestDefs[size_t(kind) < std::size(ChestDefs) ? size_t(kind) : 0];
	}

	inline uint32_t RoyaleItemLevel(uint8_t stormStage)
	{
		return std::min<uint32_t>(4u + 3u * stormStage, MaxItemLevel);
	}

	struct MatchReward
	{
		uint32_t Gold = 0;
		uint32_t Shards = 0;
		uint32_t Dust = 0;
		uint32_t Starsteel = 0;
	};

	inline MatchReward GetRoyaleReward(uint32_t placement, uint32_t entrants, uint32_t kills)
	{
		MatchReward reward;
		if (entrants < 2)
			return reward;
		reward.Gold = 120 + 60 * kills + (placement == 1 ? 600 : placement == 2 ? 300 : placement == 3 ? 150 : 0);
		reward.Shards = placement <= 3 ? 6 - placement : 1;
		reward.Dust = placement <= 3 ? 5 - placement : 1;
		reward.Starsteel = placement == 1 ? 1 : 0;
		return reward;
	}

	inline MatchReward GetArenaReward(bool won, uint32_t players, uint32_t kills)
	{
		MatchReward reward;
		if (players < 2)
			return reward;
		reward.Gold = 60 + 20 * std::min(kills, 30u) + (won ? 200 : 0);
		reward.Shards = won ? 3 : 1;
		return reward;
	}

	inline MatchReward GetBrawlReward(bool won, uint32_t players, uint32_t points)
	{
		MatchReward reward;
		if (players < 2)
			return reward;
		reward.Gold = 60 + 15 * std::min(points, 40u) + (won ? 250 : 0);
		reward.Shards = won ? 3 : 1;
		reward.Dust = won ? 2 : 0;
		return reward;
	}
}
