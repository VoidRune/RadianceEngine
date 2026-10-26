#pragma once
#include "GameShared/Content/Definitions.h"
#include "GameShared/Content/Items.h"
#include "GameShared/Content/Stats.h"
#include "GameShared/Content/Upgrades.h"
#include <array>
#include <cstdint>

namespace Game
{
	struct SkillTraits
	{
		AbilityId Ability = AbilityId::Count;
		bool Usable = false;
		uint8_t Level = 1;
		float CastTime = 0.0f;
		float ChargeTime = 0.0f;
		float Cooldown = 0.0f;
		float ManaCost = 0.0f;
		float MoveScale = 1.0f;
		float DamageScale = 1.0f;
		float AreaScale = 1.0f;
		uint8_t ExtraProjectiles = 0;
		float PvpScale = 1.0f;
		float RangeScale = 1.0f;

		bool IsEmpty() const { return Ability == AbilityId::Count; }
		bool operator==(const SkillTraits&) const = default;
	};

	struct PlayerTraits
	{
		std::array<SkillTraits, AbilitySlots> Skills{};
		SkillTraits Dodge;
		float MoveSpeed = BaseMoveSpeed;
		float AreaScale = 1.0f;
		float ProjectileSpeedScale = 1.0f;
		uint8_t ExtraProjectiles = 0;
		uint8_t ExtraPierce = 0;
		int32_t MaxHealth = BaseHealth;
		float MaxMana = BaseMana;
		float ManaRegen = BaseManaRegen;
		float Regeneration = 0.0f;
		float CritChance = 0.0f;
		float CritMultiplier = 1.5f;
		float LifeSteal = 0.0f;
		float Armor = 0.0f;
		std::array<float, size_t(Element::Count)> Resist{};
		float BloodRate = 0.0f;
		uint8_t OverloadEvery = 0;

		bool operator==(const PlayerTraits&) const = default;
	};

	struct ProjectilePattern
	{
		uint32_t Count = 1;
		float Spread = 0.0f;
		float Speed = 0.0f;
		uint8_t Pierce = 0;
	};

	inline constexpr float MaxResist = 0.75f;
	inline constexpr float ArmorConstant = 120.0f;
	inline constexpr float MaxArmorReduction = 0.7f;

	StatBlock CollectStats(const Equipment& gear, const BuffStacks& buffs, const UpgradeStacks& upgrades);
	PlayerTraits ComputeTraits(const Equipment& gear, const GemSlots& gems, const BuffStacks& buffs, const UpgradeStacks& upgrades);
	float DamageTakenScale(const PlayerTraits& traits, Element element, float maxArmor = MaxArmorReduction);
	ProjectilePattern GetProjectilePattern(const AbilityDef& ability, uint8_t extraProjectiles, uint8_t extraPierce, float speedScale);
	Vec2 ProjectileDirection(const ProjectilePattern& pattern, Vec2 aim, uint32_t index);
}
