#pragma once
#include "GameShared/Content/Progression.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace Game
{
	enum class EquipSlot : uint8_t
	{
		Weapon,
		Head,
		Body,
		Feet,
		Amulet,
		Ring,
		Gem,
		Count,
	};

	constexpr uint8_t SlotBit(EquipSlot slot)
	{
		return uint8_t(1u << uint32_t(slot));
	}

	enum class Stat : uint8_t
	{
		Damage,
		PhysicalDamage,
		FireDamage,
		ColdDamage,
		LightningDamage,
		AttackSpeed,
		CastSpeed,
		CritChance,
		CritDamage,
		LifeSteal,
		AreaSize,
		ExtraProjectiles,
		Pierce,
		ProjectileSpeed,
		CooldownRecovery,
		GemLevel,
		MaxHealth,
		MaxMana,
		ManaRegen,
		Regeneration,
		Armor,
		FireResist,
		ColdResist,
		LightningResist,
		PoisonResist,
		MoveSpeed,
		Count,
	};

	enum class StatScope : uint8_t
	{
		Any,
		Ranged,
		Melee,
	};

	enum class StatCategory : uint8_t
	{
		Offense,
		Defense,
		Utility,
		Count,
	};

	struct StatDef
	{
		Stat Id = Stat::Damage;
		const char* Label = "";
		const char* Prefix = "";
		bool Percent = true;
		bool Integer = false;
		float AffixMin = 0.0f;
		float AffixMax = 0.0f;
		uint8_t Slots = 0;
		StatScope Scope = StatScope::Any;
		StatCategory Category = StatCategory::Offense;
	};

	namespace StatSlots
	{
		constexpr uint8_t Weapon = SlotBit(EquipSlot::Weapon);
		constexpr uint8_t Head = SlotBit(EquipSlot::Head);
		constexpr uint8_t Body = SlotBit(EquipSlot::Body);
		constexpr uint8_t Feet = SlotBit(EquipSlot::Feet);
		constexpr uint8_t Amulet = SlotBit(EquipSlot::Amulet);
		constexpr uint8_t Ring = SlotBit(EquipSlot::Ring);
		constexpr uint8_t Armor = Head | Body | Feet;
		constexpr uint8_t Jewelry = Amulet | Ring;
	}

	inline constexpr StatDef StatDefs[] = {
		{ Stat::Damage, "damage", "Brutal", true, false, 0.06f, 0.12f, StatSlots::Weapon | StatSlots::Jewelry },
		{ Stat::PhysicalDamage, "physical damage", "Honed", true, false, 0.08f, 0.16f, StatSlots::Weapon | StatSlots::Ring },
		{ Stat::FireDamage, "fire damage", "Searing", true, false, 0.08f, 0.18f, StatSlots::Weapon | StatSlots::Jewelry },
		{ Stat::ColdDamage, "cold damage", "Frigid", true, false, 0.08f, 0.18f, StatSlots::Weapon | StatSlots::Jewelry },
		{ Stat::LightningDamage, "lightning damage", "Crackling", true, false, 0.08f, 0.18f, StatSlots::Weapon | StatSlots::Jewelry },
		{ Stat::AttackSpeed, "attack speed", "Swift", true, false, 0.05f, 0.1f, StatSlots::Weapon | StatSlots::Ring },
		{ Stat::CastSpeed, "cast speed", "Chanting", true, false, 0.05f, 0.1f, StatSlots::Weapon | StatSlots::Amulet | StatSlots::Head },
		{ Stat::CritChance, "critical chance", "Keen", true, false, 0.03f, 0.07f, StatSlots::Weapon | StatSlots::Jewelry },
		{ Stat::CritDamage, "critical damage", "Savage", true, false, 0.12f, 0.3f, StatSlots::Weapon | StatSlots::Amulet },
		{ Stat::LifeSteal, "life steal", "Vampiric", true, false, 0.01f, 0.025f, StatSlots::Weapon | StatSlots::Ring },
		{ Stat::AreaSize, "area", "Sweeping", true, false, 0.08f, 0.16f, StatSlots::Weapon | StatSlots::Amulet },
		{ Stat::ExtraProjectiles, "projectile", "Splitting", false, true, 1.0f, 1.0f, StatSlots::Weapon, StatScope::Ranged },
		{ Stat::Pierce, "pierce", "Piercing", false, true, 1.0f, 1.0f, StatSlots::Weapon, StatScope::Ranged },
		{ Stat::ProjectileSpeed, "projectile speed", "Hasty", true, false, 0.1f, 0.25f, StatSlots::Weapon, StatScope::Ranged },
		{ Stat::CooldownRecovery, "cooldown recovery", "Focused", true, false, 0.06f, 0.12f, StatSlots::Weapon | StatSlots::Head | StatSlots::Amulet, StatScope::Any, StatCategory::Utility },
		{ Stat::GemLevel, "to skill gem levels", "Resonant", false, true, 1.0f, 1.0f, StatSlots::Weapon, StatScope::Any, StatCategory::Utility },
		{ Stat::MaxHealth, "max health", "Sturdy", false, false, 10.0f, 25.0f, StatSlots::Armor | StatSlots::Jewelry, StatScope::Any, StatCategory::Defense },
		{ Stat::MaxMana, "max mana", "Sage's", false, false, 10.0f, 25.0f, StatSlots::Head | StatSlots::Body | StatSlots::Jewelry, StatScope::Any, StatCategory::Utility },
		{ Stat::ManaRegen, "mana per second", "Flowing", false, false, 0.6f, 1.5f, StatSlots::Head | StatSlots::Body | StatSlots::Jewelry, StatScope::Any, StatCategory::Utility },
		{ Stat::Regeneration, "health per second", "Living", false, false, 0.4f, 1.2f, StatSlots::Body | StatSlots::Jewelry, StatScope::Any, StatCategory::Defense },
		{ Stat::Armor, "armor", "Plated", false, false, 8.0f, 20.0f, StatSlots::Armor, StatScope::Any, StatCategory::Defense },
		{ Stat::FireResist, "fire resistance", "Fireproof", true, false, 0.06f, 0.14f, StatSlots::Armor | StatSlots::Jewelry, StatScope::Any, StatCategory::Defense },
		{ Stat::ColdResist, "cold resistance", "Insulated", true, false, 0.06f, 0.14f, StatSlots::Armor | StatSlots::Jewelry, StatScope::Any, StatCategory::Defense },
		{ Stat::LightningResist, "lightning resistance", "Grounded", true, false, 0.06f, 0.14f, StatSlots::Armor | StatSlots::Jewelry, StatScope::Any, StatCategory::Defense },
		{ Stat::PoisonResist, "poison resistance", "Antidotal", true, false, 0.06f, 0.14f, StatSlots::Armor | StatSlots::Jewelry, StatScope::Any, StatCategory::Defense },
		{ Stat::MoveSpeed, "move speed", "Fleet", true, false, 0.04f, 0.08f, StatSlots::Feet, StatScope::Any, StatCategory::Utility },
	};

	constexpr bool StatsMatchIds()
	{
		for (size_t i = 0; i < std::size(StatDefs); i++)
			if (size_t(StatDefs[i].Id) != i)
				return false;
		return std::size(StatDefs) == size_t(Stat::Count);
	}
	static_assert(StatsMatchIds());

	using StatBlock = std::array<float, size_t(Stat::Count)>;

	inline const StatDef& GetStat(Stat stat)
	{
		return StatDefs[size_t(stat) < std::size(StatDefs) ? size_t(stat) : 0];
	}

	inline const char* GetStatCategoryName(StatCategory category)
	{
		constexpr const char* Names[] = { "Offense", "Defense", "Utility" };
		return Names[size_t(category) < std::size(Names) ? size_t(category) : 0];
	}

	struct AffixRange
	{
		float Min = 0.0f;
		float Max = 0.0f;
	};

	inline AffixRange GetAffixRange(Stat stat, uint8_t tier)
	{
		const StatDef& def = GetStat(stat);
		tier = uint8_t(std::clamp<uint32_t>(tier, 1u, AffixTierCount));
		if (def.Integer)
		{
			const float value = tier >= AffixTierCount ? 2.0f : 1.0f;
			return { value, value };
		}
		const float top = def.AffixMax * (def.Percent ? 2.2f : LevelScale(AffixTierLevels[AffixTierCount - 1]));
		const float ratio = top / def.AffixMin;
		const auto boundary = [&](uint32_t step) { return def.AffixMin * std::pow(ratio, float(step) / float(AffixTierCount)); };
		return { boundary(tier - 1u), boundary(tier) };
	}
}
