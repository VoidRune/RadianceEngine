#include "Items.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr float TierWeights[AffixTierCount] = { 40.0f, 30.0f, 18.0f, 9.0f, 3.0f };
		constexpr float RollMin = 0.95f;
		constexpr float RollMax = 1.05f;
		constexpr uint8_t MaxPity = 10;

		Rarity RollRarity(Random& random, uint32_t level, float rarityBonus, Rarity minimum)
		{
			const float shift = 0.035f * float(level > 0 ? level - 1 : 0) + rarityBonus;
			float weights[size_t(Rarity::Count)] = {};
			float total = 0.0f;
			for (size_t i = size_t(minimum); i < size_t(Rarity::Count); i++)
			{
				weights[i] = std::max(Rarities[i].Weight, i == size_t(minimum) ? 1.0f : 0.0f) * (1.0f + shift * float(i));
				total += weights[i];
			}
			float roll = random.Float() * total;
			for (size_t i = size_t(minimum); i < size_t(Rarity::Count); i++)
			{
				if (roll < weights[i])
					return Rarity(i);
				roll -= weights[i];
			}
			return Rarity(size_t(Rarity::Count) - 1);
		}

		ItemBase RollBase(Random& random)
		{
			float total = 0.0f;
			for (const ItemBaseDef& def : ItemBases)
				total += def.DropWeight;
			float roll = random.Float() * total;
			for (const ItemBaseDef& def : ItemBases)
			{
				if (def.DropWeight <= 0.0f)
					continue;
				if (roll < def.DropWeight)
					return def.Id;
				roll -= def.DropWeight;
			}
			return ItemBase::Ring;
		}

		uint8_t InferTier(Stat stat, float value, uint8_t maxTier)
		{
			uint8_t tier = 1;
			for (uint8_t candidate = 1; candidate <= maxTier; candidate++)
				if (value >= GetAffixRange(stat, candidate).Min * 0.999f)
					tier = candidate;
			return tier;
		}
	}

	bool IsAffixAllowed(Stat stat, ItemBase base, Rarity rarity)
	{
		const StatDef& def = GetStat(stat);
		const EquipSlot slot = GetEquipSlot(base);
		if ((def.Slots & SlotBit(slot)) == 0)
			return false;
		if (stat == Stat::GemLevel && rarity < Rarity::Rare)
			return false;
		if (def.Scope == StatScope::Any)
			return true;
		return slot == EquipSlot::Weapon && (def.Scope == StatScope::Ranged) == IsRangedWeapon(base);
	}

	bool HasAffix(const Item& item, Stat stat)
	{
		for (uint8_t i = 0; i < item.AffixCount; i++)
			if (item.Affixes[i].Kind == stat)
				return true;
		return false;
	}

	uint8_t RollAffixTier(Random& random, uint8_t maxTier)
	{
		maxTier = uint8_t(std::clamp<uint32_t>(maxTier, 1u, AffixTierCount));
		float total = 0.0f;
		for (uint8_t i = 0; i < maxTier; i++)
			total += TierWeights[i];
		float roll = random.Float() * total;
		for (uint8_t i = 0; i < maxTier; i++)
		{
			if (roll < TierWeights[i])
				return uint8_t(i + 1);
			roll -= TierWeights[i];
		}
		return maxTier;
	}

	float RollAffixValue(Random& random, Stat stat, uint8_t tier)
	{
		const AffixRange range = GetAffixRange(stat, tier);
		return range.Min + (range.Max - range.Min) * random.Float();
	}

	Item MakeItem(ItemBase base, uint32_t id, uint32_t level)
	{
		Item item;
		item.Id = id;
		item.Base = base;
		item.Level = uint8_t(std::clamp(level, 1u, MaxItemLevel));
		item.Power = LevelScale(item.Level);
		return item;
	}

	Item MakeGem(AbilityId skill, uint32_t id, uint32_t level)
	{
		Item item;
		item.Id = id;
		item.Base = ItemBase::Gem;
		item.Skill = skill;
		item.Level = uint8_t(std::clamp<uint32_t>(level, 1u, MaxGemLevel));
		return item;
	}

	Item GenerateItem(Random& random, uint32_t id, uint32_t level, float rarityBonus, Rarity minimum, std::optional<ItemBase> base)
	{
		Item item = MakeItem(base && *base != ItemBase::Gem ? *base : RollBase(random), id, level);
		item.Quality = RollRarity(random, item.Level, rarityBonus, std::max(minimum, Rarity::Uncommon));
		item.Power = LevelScale(item.Level) * (RollMin + (RollMax - RollMin) * random.Float());

		std::vector<Stat> candidates;
		for (const StatDef& stat : StatDefs)
			if (IsAffixAllowed(stat.Id, item.Base, item.Quality))
				candidates.push_back(stat.Id);
		for (size_t i = candidates.size(); i > 1; i--)
			std::swap(candidates[i - 1], candidates[random.Below(uint32_t(i))]);

		const uint8_t slots = GetAffixSlots(item.Quality);
		const uint32_t count = std::max<uint32_t>(1, slots - random.Below(2));
		item.AffixCount = uint8_t(std::min<size_t>({ count, candidates.size(), MaxAffixes }));
		const uint8_t maxTier = MaxAffixTier(item.Level);
		for (uint8_t i = 0; i < item.AffixCount; i++)
		{
			Affix& affix = item.Affixes[i];
			affix.Kind = candidates[i];
			affix.Tier = RollAffixTier(random, maxTier);
			affix.Value = RollAffixValue(random, affix.Kind, affix.Tier);
		}
		return item;
	}

	Item GenerateGem(Random& random, uint32_t id, uint32_t level)
	{
		std::vector<AbilityId> gems;
		for (const AbilityDef& ability : Abilities)
			if (ability.Gem)
				gems.push_back(ability.Id);
		const AbilityId skill = gems[random.Below(uint32_t(gems.size()))];
		const uint32_t gemLevel = 1 + (std::max(level, 1u) - 1) / 8 + (random.Below(100) < 25 ? 1 : 0);
		return MakeGem(skill, id, std::min(gemLevel, 6u));
	}

	GearSet MakeStarterGear(StarterKit kit)
	{
		const KitDef& def = GetKit(kit);
		GearSet set;
		uint32_t id = 1;
		set.Gear[size_t(GearSlot::Weapon)] = MakeItem(def.Weapon, id++);
		set.Gear[size_t(GearSlot::Head)] = MakeItem(def.Armor[0], id++);
		set.Gear[size_t(GearSlot::Body)] = MakeItem(def.Armor[1], id++);
		set.Gear[size_t(GearSlot::Feet)] = MakeItem(def.Armor[2], id++);
		for (size_t i = 0; i < def.Gems.size(); i++)
			set.Gems[i] = MakeGem(def.Gems[i], id++);
		return set;
	}

	std::optional<Item> SanitizeItem(const Item& item)
	{
		if (!item.IsValid() || item.Base >= ItemBase::Count || item.Quality >= Rarity::Count)
			return std::nullopt;
		Item clean = item;
		if (clean.Base == ItemBase::Gem)
		{
			if (clean.Skill >= AbilityId::Count || !GetAbility(clean.Skill).Gem)
				return std::nullopt;
			clean = MakeGem(item.Skill, item.Id, 1);
			clean.Corrupted = item.Corrupted;
			clean.Level = uint8_t(std::clamp<uint32_t>(item.Level, 1u, item.Corrupted ? CorruptedGemLevel : MaxGemLevel));
			clean.GemQuality = std::min(item.GemQuality, MaxGemQuality);
			clean.Mod = item.Corrupted && item.Mod < GemMod::Count ? item.Mod : GemMod::None;
			return clean;
		}
		clean.Skill = AbilityId::Count;
		clean.GemQuality = 0;
		clean.Mod = GemMod::None;
		clean.Corrupted = false;
		clean.Pity = std::min(item.Pity, MaxPity);
		clean.Level = uint8_t(std::clamp<uint32_t>(clean.Level, 1u, MaxItemLevel));
		const float scale = LevelScale(clean.Level);
		clean.Power = std::isfinite(item.Power) ? std::clamp(item.Power, scale * RollMin, scale * RollMax) : scale;
		const uint8_t maxTier = MaxAffixTier(clean.Level);
		uint8_t kept = 0;
		for (uint8_t i = 0; i < std::min<uint8_t>(item.AffixCount, uint8_t(MaxAffixes)) && kept < GetAffixSlots(clean.Quality); i++)
		{
			Affix affix = item.Affixes[i];
			if (affix.Kind >= Stat::Count || !std::isfinite(affix.Value) || affix.Value <= 0.0f || !IsAffixAllowed(affix.Kind, clean.Base, clean.Quality))
				continue;
			bool duplicate = false;
			for (uint8_t j = 0; j < kept; j++)
				duplicate = duplicate || clean.Affixes[j].Kind == affix.Kind;
			if (duplicate)
				continue;
			affix.Tier = affix.Tier == 0 ? InferTier(affix.Kind, affix.Value, maxTier) : std::clamp(affix.Tier, uint8_t(1), maxTier);
			const AffixRange range = GetAffixRange(affix.Kind, affix.Tier);
			affix.Value = std::clamp(affix.Value, range.Min, range.Max);
			clean.Affixes[kept++] = affix;
		}
		clean.AffixCount = kept;
		for (size_t i = kept; i < clean.Affixes.size(); i++)
			clean.Affixes[i] = {};
		return clean;
	}

	GearSet SanitizeGear(const GearSet& gear)
	{
		GearSet clean;
		std::vector<Item> spare;
		uint32_t nextId = 1;
		const auto take = [&](const Item& item) -> std::optional<Item>
		{
			std::optional<Item> result = SanitizeItem(item);
			if (result)
				result->Id = nextId++;
			return result;
		};
		for (size_t slot = 0; slot < GearSlotCount; slot++)
			if (const std::optional<Item> item = take(gear.Gear[slot]))
			{
				if (CanEquip(*item, GearSlot(slot)))
					clean.Gear[slot] = *item;
				else
					spare.push_back(*item);
			}
		for (size_t slot = 0; slot < SkillSlots; slot++)
			if (const std::optional<Item> item = take(gear.Gems[slot]))
			{
				if (item->IsGem())
					clean.Gems[slot] = *item;
				else
					spare.push_back(*item);
			}
		for (size_t slot = 0; slot < BackpackSize; slot++)
			if (const std::optional<Item> item = take(gear.Pack[slot]))
				clean.Pack[slot] = *item;
		for (size_t slot = 0; slot < StashSize; slot++)
			if (const std::optional<Item> item = take(gear.Stash[slot]))
				clean.Stash[slot] = *item;
		for (const Item& item : spare)
		{
			auto free = std::find_if(clean.Pack.begin(), clean.Pack.end(), [](const Item& held) { return !held.IsValid(); });
			if (free != clean.Pack.end())
			{
				*free = item;
				continue;
			}
			auto stashed = std::find_if(clean.Stash.begin(), clean.Stash.end(), [](const Item& held) { return !held.IsValid(); });
			if (stashed != clean.Stash.end())
				*stashed = item;
		}
		if (!clean.Gear[size_t(GearSlot::Weapon)].IsValid())
			clean.Gear[size_t(GearSlot::Weapon)] = MakeItem(ItemBase::Wand, nextId++);
		for (size_t i = 0; i < clean.Materials.size(); i++)
			clean.Materials[i] = std::min(gear.Materials[i], MaxMaterialStack);
		return clean;
	}

	float GetImplicitValue(const Item& item, uint32_t index)
	{
		const ItemBaseDef& def = GetItemBase(item.Base);
		if (index >= def.ImplicitCount)
			return 0.0f;
		const Affix& implicit = def.Implicits[index];
		const StatDef& stat = GetStat(implicit.Kind);
		const float value = implicit.Value * (stat.Percent ? GetItemRoll(item) : item.Power);
		return stat.Integer ? std::round(value) : value;
	}

	bool CanEquip(const Item& item, GearSlot slot)
	{
		return item.IsValid() && !item.IsGem() && GetEquipSlot(item.Base) == GetGearSlotKind(slot);
	}

	std::optional<GearSlot> PreferredGearSlot(const Item& item, const Equipment& gear)
	{
		if (!item.IsValid() || item.IsGem())
			return std::nullopt;
		switch (GetEquipSlot(item.Base))
		{
		case EquipSlot::Weapon:
			return GearSlot::Weapon;
		case EquipSlot::Head:
			return GearSlot::Head;
		case EquipSlot::Body:
			return GearSlot::Body;
		case EquipSlot::Feet:
			return GearSlot::Feet;
		case EquipSlot::Amulet:
			return GearSlot::Amulet;
		case EquipSlot::Ring:
			if (!gear[size_t(GearSlot::Ring1)].IsValid())
				return GearSlot::Ring1;
			if (!gear[size_t(GearSlot::Ring2)].IsValid())
				return GearSlot::Ring2;
			return GearSlot::Ring1;
		default:
			return std::nullopt;
		}
	}

	std::string GetItemName(const Item& item)
	{
		if (item.IsGem())
		{
			const std::string name = std::string(GetAbility(item.Skill).Name) + " Gem";
			return item.Mod != GemMod::None ? std::string(GetGemMod(item.Mod).Name) + " " + name : name;
		}
		std::string name = GetRarity(item.Quality).Name;
		if (item.AffixCount > 0)
		{
			const Affix* strongest = &item.Affixes[0];
			for (uint8_t i = 1; i < item.AffixCount; i++)
			{
				const Affix& affix = item.Affixes[i];
				if (affix.Tier > strongest->Tier || (affix.Tier == strongest->Tier && affix.Value / GetAffixRange(affix.Kind, affix.Tier).Max > strongest->Value / GetAffixRange(strongest->Kind, strongest->Tier).Max))
					strongest = &affix;
			}
			name += std::string(" ") + GetStat(strongest->Kind).Prefix;
		}
		return name + " " + GetItemBase(item.Base).Name;
	}

	std::string DescribeAffix(const Affix& affix)
	{
		const StatDef& stat = GetStat(affix.Kind);
		char text[64];
		if (stat.Integer)
			std::snprintf(text, sizeof(text), "+%d %s", int(std::lround(affix.Value)), stat.Label);
		else if (stat.Percent)
			std::snprintf(text, sizeof(text), "+%.0f%% %s", double(affix.Value) * 100.0, stat.Label);
		else
			std::snprintf(text, sizeof(text), "+%.*f %s", affix.Value < 5.0f ? 1 : 0, double(affix.Value), stat.Label);
		return text;
	}
}
