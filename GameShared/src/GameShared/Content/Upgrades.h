#pragma once
#include "GameShared/Content/Stats.h"
#include <array>
#include <cstdint>
#include <iterator>

namespace Game
{
	enum class UpgradeId : uint8_t
	{
		Sharpened,
		QuickHands,
		Incantation,
		FleetFoot,
		IronSkin,
		DeepWell,
		Flow,
		Vampirism,
		Precision,
		Brutality,
		SplitShot,
		Penetration,
		Expanse,
		SecondWind,
		Focus,
		Bulwark,
		Count,
	};

	struct UpgradeDef
	{
		UpgradeId Id = UpgradeId::Sharpened;
		const char* Name = "";
		const char* Description = "";
		Stat Kind = Stat::Damage;
		float Value = 0.0f;
		uint8_t MaxPicks = 5;
	};

	inline constexpr UpgradeDef Upgrades[] = {
		{ UpgradeId::Sharpened, "Sharpened", "+15% damage with everything", Stat::Damage, 0.15f },
		{ UpgradeId::QuickHands, "Quick Hands", "+12% attack speed", Stat::AttackSpeed, 0.12f },
		{ UpgradeId::Incantation, "Incantation", "+12% cast speed", Stat::CastSpeed, 0.12f },
		{ UpgradeId::FleetFoot, "Fleet Foot", "+8% move speed", Stat::MoveSpeed, 0.08f, 4 },
		{ UpgradeId::IronSkin, "Iron Skin", "+40 max health", Stat::MaxHealth, 40.0f },
		{ UpgradeId::DeepWell, "Deep Well", "+30 max mana", Stat::MaxMana, 30.0f },
		{ UpgradeId::Flow, "Flow", "Regenerate 2 more mana per second", Stat::ManaRegen, 2.0f },
		{ UpgradeId::Vampirism, "Vampirism", "Heal for 3% of the damage you deal", Stat::LifeSteal, 0.03f },
		{ UpgradeId::Precision, "Precision", "+8% critical chance", Stat::CritChance, 0.08f },
		{ UpgradeId::Brutality, "Brutality", "+35% critical damage", Stat::CritDamage, 0.35f },
		{ UpgradeId::SplitShot, "Split Shot", "Projectile skills fire 1 more projectile", Stat::ExtraProjectiles, 1.0f, 2 },
		{ UpgradeId::Penetration, "Penetration", "Projectiles pierce 1 more enemy", Stat::Pierce, 1.0f, 3 },
		{ UpgradeId::Expanse, "Expanse", "+20% area of melee, nova and ground skills", Stat::AreaSize, 0.2f, 4 },
		{ UpgradeId::SecondWind, "Second Wind", "Regenerate 2 health per second", Stat::Regeneration, 2.0f },
		{ UpgradeId::Focus, "Focus", "+20% cooldown recovery for skills", Stat::CooldownRecovery, 0.2f },
		{ UpgradeId::Bulwark, "Bulwark", "+25 armor", Stat::Armor, 25.0f },
	};

	constexpr bool UpgradesMatchIds()
	{
		for (size_t i = 0; i < std::size(Upgrades); i++)
			if (size_t(Upgrades[i].Id) != i)
				return false;
		return std::size(Upgrades) == size_t(UpgradeId::Count);
	}
	static_assert(UpgradesMatchIds());

	constexpr size_t UpgradeChoices = 3;
	using UpgradeStacks = std::array<uint8_t, size_t(UpgradeId::Count)>;
	using UpgradeOffer = std::array<UpgradeId, UpgradeChoices>;

	inline const UpgradeDef& GetUpgrade(UpgradeId id)
	{
		return Upgrades[size_t(id) < std::size(Upgrades) ? size_t(id) : 0];
	}
}
