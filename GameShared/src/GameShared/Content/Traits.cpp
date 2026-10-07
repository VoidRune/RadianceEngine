#include "Traits.h"
#include "GameShared/Content/Pvp.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		float ElementBonus(const StatBlock& stats, Element element)
		{
			switch (element)
			{
			case Element::Physical:
				return stats[size_t(Stat::PhysicalDamage)];
			case Element::Fire:
				return stats[size_t(Stat::FireDamage)];
			case Element::Cold:
				return stats[size_t(Stat::ColdDamage)];
			case Element::Lightning:
				return stats[size_t(Stat::LightningDamage)];
			default:
				return 0.0f;
			}
		}

		SkillTraits MakeSkill(AbilityId id, uint32_t level, bool usable, bool weaponAttack, const Item& weapon, const StatBlock& stats, const Item* gem = nullptr)
		{
			const AbilityDef& def = GetAbility(id);
			const auto stat = [&](Stat kind) { return stats[size_t(kind)]; };
			const GemMod mod = gem ? gem->Mod : GemMod::None;
			SkillTraits skill;
			skill.Ability = id;
			skill.Usable = usable;
			skill.Level = uint8_t(std::clamp(level, 1u, 30u));
			const float speed = std::min(def.Spell ? stat(Stat::CastSpeed) : stat(Stat::AttackSpeed), 2.0f);
			const float speedScale = 1.0f / (1.0f + std::max(speed, -0.5f));
			skill.CastTime = def.CastTime * speedScale * (mod == GemMod::Quickened ? 0.8f : 1.0f);
			skill.Cooldown = (weaponAttack ? def.Cooldown * speedScale : def.Cooldown / (1.0f + std::clamp(stat(Stat::CooldownRecovery), 0.0f, 1.5f))) * (mod == GemMod::Restless ? 0.75f : 1.0f);
			skill.ManaCost = def.ManaCost * (1.0f + 0.03f * float(skill.Level - 1)) * (mod == GemMod::Frugal ? 0.75f : 1.0f);
			skill.MoveScale = def.CastMove;
			const float quality = gem ? 1.0f + 0.01f * float(gem->GemQuality) : 1.0f;
			skill.DamageScale = weapon.Power * (1.0f + stat(Stat::Damage) + ElementBonus(stats, def.Damages)) * (1.0f + 0.06f * float(skill.Level - 1)) * quality * (mod == GemMod::Ruthless ? 1.25f : 1.0f);
			skill.AreaScale = mod == GemMod::Vast ? 1.25f : 1.0f;
			skill.ExtraProjectiles = mod == GemMod::Split ? 1 : 0;
			skill.PvpScale = GetPvpScale(def, weapon.Power);
			return skill;
		}
	}

	StatBlock CollectStats(const Equipment& gear, const BuffStacks& buffs, const UpgradeStacks& upgrades)
	{
		StatBlock stats{};
		for (const Item& item : gear)
		{
			if (!item.IsValid() || item.IsGem())
				continue;
			const ItemBaseDef& base = GetItemBase(item.Base);
			for (uint32_t i = 0; i < base.ImplicitCount; i++)
				stats[size_t(base.Implicits[i].Kind)] += GetImplicitValue(item, i);
			for (uint8_t i = 0; i < item.AffixCount; i++)
				stats[size_t(item.Affixes[i].Kind)] += item.Affixes[i].Value;
		}
		stats[size_t(Stat::Damage)] += MightPerStack * float(buffs[size_t(LootKind::Might)]);
		stats[size_t(Stat::AttackSpeed)] += HastePerStack * float(buffs[size_t(LootKind::Haste)]);
		stats[size_t(Stat::CastSpeed)] += HastePerStack * float(buffs[size_t(LootKind::Haste)]);
		stats[size_t(Stat::MoveSpeed)] += SwiftnessPerStack * float(buffs[size_t(LootKind::Swiftness)]);
		stats[size_t(Stat::MaxHealth)] += float(VitalityHealth * buffs[size_t(LootKind::Vitality)]);
		for (size_t i = 0; i < upgrades.size(); i++)
			stats[size_t(Upgrades[i].Kind)] += Upgrades[i].Value * float(upgrades[i]);
		return stats;
	}

	PlayerTraits ComputeTraits(const Equipment& gear, const GemSlots& gems, const BuffStacks& buffs, const UpgradeStacks& upgrades)
	{
		const StatBlock stats = CollectStats(gear, buffs, upgrades);
		const auto stat = [&](Stat kind) { return stats[size_t(kind)]; };
		const Item& equipped = gear[size_t(GearSlot::Weapon)];
		const Item weapon = equipped.IsValid() && IsWeapon(equipped.Base) ? equipped : MakeItem(ItemBase::Wand, 0);
		const ItemBaseDef& weaponDef = GetItemBase(weapon.Base);

		PlayerTraits traits;
		traits.MoveSpeed = BaseMoveSpeed * (1.0f + std::clamp(stat(Stat::MoveSpeed), -0.5f, 0.8f));
		traits.AreaScale = 1.0f + std::clamp(stat(Stat::AreaSize), 0.0f, 1.5f);
		traits.ProjectileSpeedScale = 1.0f + std::clamp(stat(Stat::ProjectileSpeed), 0.0f, 1.5f);
		traits.ExtraProjectiles = uint8_t(std::clamp(std::lround(stat(Stat::ExtraProjectiles)), 0l, 4l));
		traits.ExtraPierce = uint8_t(std::clamp(std::lround(stat(Stat::Pierce)), 0l, 4l));
		traits.MaxHealth = std::max(1, BaseHealth + int32_t(std::lround(stat(Stat::MaxHealth))));
		traits.MaxMana = std::max(0.0f, BaseMana + stat(Stat::MaxMana));
		traits.ManaRegen = std::max(0.0f, BaseManaRegen + stat(Stat::ManaRegen));
		traits.Regeneration = std::max(0.0f, stat(Stat::Regeneration));
		traits.CritChance = std::clamp(stat(Stat::CritChance), 0.0f, 0.75f);
		traits.CritMultiplier = 1.5f + std::max(stat(Stat::CritDamage), 0.0f);
		traits.LifeSteal = std::clamp(stat(Stat::LifeSteal), 0.0f, 0.25f);
		traits.Armor = std::max(stat(Stat::Armor), 0.0f);
		traits.Resist[size_t(Element::Fire)] = std::clamp(stat(Stat::FireResist), 0.0f, MaxResist);
		traits.Resist[size_t(Element::Cold)] = std::clamp(stat(Stat::ColdResist), 0.0f, MaxResist);
		traits.Resist[size_t(Element::Lightning)] = std::clamp(stat(Stat::LightningResist), 0.0f, MaxResist);
		traits.Resist[size_t(Element::Poison)] = std::clamp(stat(Stat::PoisonResist), 0.0f, MaxResist);

		const int32_t gemBonus = int32_t(std::lround(std::max(stat(Stat::GemLevel), 0.0f)));
		traits.Skills[0] = MakeSkill(weaponDef.Attack, 1, true, true, weapon, stats);
		for (uint32_t i = 0; i < SkillSlots; i++)
		{
			const Item& gem = gems[i];
			if (!gem.IsGem() || !GetAbility(gem.Skill).Gem)
				continue;
			const AbilityDef& ability = GetAbility(gem.Skill);
			traits.Skills[i + 1] = MakeSkill(gem.Skill, uint32_t(gem.Level) + uint32_t(gemBonus), MeetsRequirement(ability.Needs, weaponDef.Affinity), false, weapon, stats, &gem);
		}
		traits.Dodge = MakeSkill(AbilityId::Roll, 1, true, false, weapon, StatBlock{});
		return traits;
	}

	float DamageTakenScale(const PlayerTraits& traits, Element element, float maxArmor)
	{
		if (element == Element::Physical)
			return 1.0f - std::min(traits.Armor / (traits.Armor + ArmorConstant), maxArmor);
		return 1.0f - std::clamp(traits.Resist[size_t(element)], -0.5f, MaxResist);
	}

	ProjectilePattern GetProjectilePattern(const AbilityDef& ability, uint8_t extraProjectiles, uint8_t extraPierce, float speedScale)
	{
		ProjectilePattern pattern;
		pattern.Count = uint32_t(ability.ProjectileCount) + extraProjectiles;
		pattern.Spread = std::max(ability.ProjectileSpread, 0.13f * float(pattern.Count - 1));
		pattern.Speed = ability.ProjectileSpeed * speedScale;
		pattern.Pierce = uint8_t(std::min(ability.Pierce + extraPierce, 8));
		return pattern;
	}

	Vec2 ProjectileDirection(const ProjectilePattern& pattern, Vec2 aim, uint32_t index)
	{
		if (pattern.Count <= 1)
			return aim;
		const float angle = AngleOf(aim) + pattern.Spread * (float(index) / float(pattern.Count - 1) - 0.5f);
		return FromAngle(angle);
	}
}
