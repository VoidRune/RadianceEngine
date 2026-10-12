#pragma once
#include "GameShared/Content/Definitions.h"
#include "GameShared/Content/Traits.h"
#include "GameShared/World/Level.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace Game
{
	enum class BrawlPhase : uint8_t
	{
		Draft,
		Countdown,
		Fight,
		Over,
		Shop,
	};

	enum class BoonId : uint8_t
	{
		FleetFoot,
		HeavyHand,
		Anchored,
		Firewalker,
		Vigor,
		Quickening,
		SwiftSpells,
		Nimble,
		SecondWind,
		Count,
	};

	struct BoonDef
	{
		BoonId Id = BoonId::FleetFoot;
		const char* Name = "";
		const char* Description = "";
		uint8_t MaxPicks = 3;
	};

	inline constexpr BoonDef Boons[] = {
		{ BoonId::FleetFoot, "Fleet Foot", "+7% move speed", 3 },
		{ BoonId::HeavyHand, "Heavy Hand", "Your spells shove 12% harder", 4 },
		{ BoonId::Anchored, "Anchored", "You get shoved 12% less", 3 },
		{ BoonId::Firewalker, "Firewalker", "Lava burns you 20% less", 3 },
		{ BoonId::Vigor, "Vigor", "+20 max health", 3 },
		{ BoonId::Quickening, "Quickening", "Your spells recover 10% faster", 3 },
		{ BoonId::SwiftSpells, "Swift Spells", "Your bolts fly 15% faster", 3 },
		{ BoonId::Nimble, "Nimble", "Your dodge roll recovers 25% faster", 2 },
		{ BoonId::SecondWind, "Second Wind", "Regenerate 1.5 health per second", 2 },
	};
	static_assert(std::size(Boons) == size_t(BoonId::Count));

	inline const BoonDef& GetBoon(BoonId id)
	{
		return Boons[size_t(id) < std::size(Boons) ? size_t(id) : 0];
	}

	inline constexpr size_t BoonChoices = 3;
	using BoonStacks = std::array<uint8_t, size_t(BoonId::Count)>;
	using BoonOffer = std::array<BoonId, BoonChoices>;
	using BrawlSpells = std::array<AbilityId, SkillSlots>;

	inline constexpr AbilityId BrawlBasic = AbilityId::EmberBolt;
	inline constexpr AbilityId BrawlSpellPool[] = {
		AbilityId::Gust,
		AbilityId::Comet,
		AbilityId::Warp,
		AbilityId::Shockwave,
		AbilityId::GravityWell,
		AbilityId::ArcLightning,
		AbilityId::Swap,
		AbilityId::MirrorWard,
		AbilityId::Frostbite,
	};
	inline constexpr BrawlSpells DefaultBrawlSpells = { AbilityId::Gust, AbilityId::Warp, AbilityId::Comet };

	inline bool IsBrawlSpell(AbilityId id)
	{
		return std::find(std::begin(BrawlSpellPool), std::end(BrawlSpellPool), id) != std::end(BrawlSpellPool);
	}

	struct BrawlLoadout
	{
		BrawlSpells Spells = DefaultBrawlSpells;
		BoonStacks Boons{};
		BoonOffer Offer{ BoonId::Count, BoonId::Count, BoonId::Count };
		bool OfferPending = false;
		bool Ready = false;
	};

	struct CollapsePlan
	{
		Vec2 Center;
		float StartRadius = 12.0f;
		uint32_t Seed = 1;
	};

	struct BrawlStatus
	{
		BrawlPhase Phase = BrawlPhase::Draft;
		uint8_t Round = 0;
		uint8_t Rounds = 5;
		float Timer = 0.0f;
		float Clock = -1e5f;
		CollapsePlan Plan;
		uint32_t RoundWinner = NoPlayer;
		uint8_t Ready = 0;
		uint8_t Players = 0;
	};

	inline constexpr int32_t BrawlHealth = 100;
	inline constexpr float BrawlMoveSpeed = 5.0f;
	inline constexpr float BrawlDodgeCooldown = 2.4f;
	inline constexpr float PushFriction = 3.2f;
	inline constexpr float PushControlSpeed = 9.0f;
	inline constexpr float PushControlLoss = 0.85f;
	inline constexpr float PushStop = 0.25f;
	inline constexpr float MaxPush = 42.0f;
	inline constexpr float PushStagger = 12.0f;
	inline constexpr float MissingHealthKnockback = 0.6f;
	inline constexpr float LavaDamagePerSecond = 14.0f;
	inline constexpr float AfterburnSeconds = 0.8f;
	inline constexpr float AfterburnDamagePerSecond = 8.0f;
	inline constexpr float BrawlDraftSeconds = 40.0f;
	inline constexpr float BrawlShopSeconds = 25.0f;
	inline constexpr float BrawlCountdownSeconds = 3.0f;
	inline constexpr float BrawlOverSeconds = 4.5f;
	inline constexpr float CollapseGrace = 7.0f;
	inline constexpr float CollapseRate = 0.16f;
	inline constexpr float CollapseMinRadius = 2.8f;
	inline constexpr float CollapseJitter = 1.6f;
	inline constexpr float FinalCollapseStart = 78.0f;
	inline constexpr float FinalCollapseRate = 0.35f;
	inline constexpr float CollapseWarning = 2.5f;
	inline constexpr float BrawlSpawnRing = 0.62f;
	inline constexpr uint16_t RoundWinPoints = 3;
	inline constexpr uint16_t KnockoutPoints = 1;

	inline float BrawlStartRadius(uint32_t players)
	{
		return std::clamp(6.5f + 1.1f * float(players), 8.5f, 12.0f);
	}

	inline float CollapseNoise(int32_t x, int32_t y, uint32_t seed)
	{
		uint32_t h = uint32_t(x) * 0x8DA6B343u ^ uint32_t(y) * 0xD8163841u ^ seed * 0xCB1AB31Fu;
		h ^= h >> 15;
		h *= 0x2C1B3C6Du;
		h ^= h >> 12;
		h *= 0x297A2D39u;
		h ^= h >> 15;
		return float(h & 0xFFFFu) / 65535.0f;
	}

	inline float TileCollapseTime(const CollapsePlan& plan, int32_t x, int32_t y)
	{
		const Vec2 center = { float(x) + 0.5f, float(y) + 0.5f };
		const float rank = Length(center - plan.Center) + (CollapseNoise(x, y, plan.Seed) - 0.5f) * CollapseJitter;
		if (rank >= plan.StartRadius)
			return -1e6f;
		if (rank >= CollapseMinRadius)
			return CollapseGrace + (plan.StartRadius - rank) / CollapseRate;
		const float settled = CollapseGrace + (plan.StartRadius - CollapseMinRadius) / CollapseRate;
		return std::max(settled, FinalCollapseStart) + (CollapseMinRadius - rank) / FinalCollapseRate;
	}

	inline bool IsBrawlLava(const Level& level, const BrawlStatus& brawl, Vec2 position)
	{
		const int32_t x = int32_t(std::floor(position.x));
		const int32_t y = int32_t(std::floor(position.y));
		switch (level.Map.GetTerrain(x, y))
		{
		case TerrainId::Lava:
			return true;
		case TerrainId::Basalt:
			return TileCollapseTime(brawl.Plan, x, y) <= brawl.Clock;
		default:
			return false;
		}
	}

	inline SkillTraits MakeBrawlSkill(AbilityId id, float cooldownScale)
	{
		const AbilityDef& def = GetAbility(id);
		SkillTraits skill;
		skill.Ability = id;
		skill.Usable = true;
		skill.CastTime = def.CastTime;
		skill.Cooldown = def.Cooldown * cooldownScale;
		skill.MoveScale = def.CastMove;
		return skill;
	}

	inline PlayerTraits MakeBrawlTraits(const BrawlLoadout& loadout)
	{
		const auto stacks = [&](BoonId id) { return float(loadout.Boons[size_t(id)]); };
		PlayerTraits traits;
		traits.MoveSpeed = BrawlMoveSpeed * (1.0f + 0.07f * stacks(BoonId::FleetFoot));
		traits.MaxHealth = BrawlHealth + 20 * int32_t(loadout.Boons[size_t(BoonId::Vigor)]);
		traits.MaxMana = 0.0f;
		traits.ManaRegen = 0.0f;
		traits.Regeneration = 1.5f * stacks(BoonId::SecondWind);
		traits.ProjectileSpeedScale = 1.0f + 0.15f * stacks(BoonId::SwiftSpells);
		const float cooldown = 1.0f - 0.1f * stacks(BoonId::Quickening);
		traits.Skills[0] = MakeBrawlSkill(BrawlBasic, cooldown);
		for (uint32_t i = 0; i < SkillSlots; i++)
			traits.Skills[i + 1] = IsBrawlSpell(loadout.Spells[i]) ? MakeBrawlSkill(loadout.Spells[i], cooldown) : SkillTraits{};
		traits.Dodge = MakeBrawlSkill(AbilityId::Roll, 1.0f);
		traits.Dodge.Cooldown = BrawlDodgeCooldown * (1.0f - 0.25f * stacks(BoonId::Nimble));
		return traits;
	}
}
