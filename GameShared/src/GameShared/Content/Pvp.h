#pragma once
#include "GameShared/Content/Definitions.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace Game
{
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
	inline constexpr float PvpRespawnDelay = 3.0f;
	inline constexpr float MatchEndSeconds = 12.0f;
	inline constexpr float KillCreditSeconds = 10.0f;

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
}
