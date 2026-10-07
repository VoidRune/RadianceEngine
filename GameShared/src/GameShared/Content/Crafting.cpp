#include "Crafting.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr float HoneChances[AffixTierCount + 1] = { 0.0f, 0.0f, 0.85f, 0.65f, 0.45f, 0.3f };
		constexpr float PityStep = 0.1f;
		constexpr float MaxHoneChance = 0.95f;
		constexpr uint8_t MaxPity = 10;
		constexpr uint8_t PolishStep = 2;

		float LevelFactor(const Item& item)
		{
			return 1.0f + 0.1f * float(std::max<uint32_t>(item.Level, 1u) - 1u);
		}

		uint32_t Amount(float value)
		{
			return uint32_t(std::lround(std::max(value, 0.0f)));
		}

		MaterialStock Cost(float gold, float shards, float starsteel = 0.0f, float dust = 0.0f, float voidShards = 0.0f)
		{
			MaterialStock cost{};
			cost[size_t(Material::Gold)] = Amount(gold);
			cost[size_t(Material::Shard)] = Amount(shards);
			cost[size_t(Material::Starsteel)] = Amount(starsteel);
			cost[size_t(Material::ArcaneDust)] = Amount(dust);
			cost[size_t(Material::VoidShard)] = Amount(voidShards);
			return cost;
		}

		std::vector<Stat> OpenStats(const Item& item, std::optional<StatCategory> category, std::optional<Stat> except)
		{
			std::vector<Stat> stats;
			for (const StatDef& stat : StatDefs)
				if ((!category || stat.Category == *category) && stat.Id != except && !HasAffix(item, stat.Id) && IsAffixAllowed(stat.Id, item.Base, item.Quality))
					stats.push_back(stat.Id);
			return stats;
		}

		bool ValidAffix(const Item& item, uint32_t affix)
		{
			return affix < item.AffixCount;
		}
	}

	const char* GetForgeActionName(ForgeAction action)
	{
		constexpr const char* Names[] = { "Temper", "Hone", "Imbue", "Reforge", "Ascend" };
		return Names[size_t(action) < std::size(Names) ? size_t(action) : 0];
	}

	const char* DescribeForgeAction(ForgeAction action)
	{
		constexpr const char* Descriptions[] = {
			"Rerolls the values of every affix within its tier, and the item's base roll.",
			"Raises one affix by a tier. The chance is shown first, and every failure raises it for the next try.",
			"Adds an affix from the category you pick, at tier 1 or 2.",
			"Replaces one affix with a different random stat at the same tier.",
			"Raises the item's rarity by one, which opens another affix slot.",
		};
		return Descriptions[size_t(action) < std::size(Descriptions) ? size_t(action) : 0];
	}

	const char* GetGemActionName(GemAction action)
	{
		constexpr const char* Names[] = { "Empower", "Polish", "Corrupt" };
		return Names[size_t(action) < std::size(Names) ? size_t(action) : 0];
	}

	const char* DescribeGemAction(GemAction action)
	{
		constexpr const char* Descriptions[] = {
			"Raises the gem's level by one, up to 20.",
			"Adds 2% quality, up to 20%. Each quality point adds 1% damage.",
			"A one-way gamble: +1 level past the cap, -1 level, a random corrupted bonus, or nothing. Corrupted gems can't be changed again.",
		};
		return Descriptions[size_t(action) < std::size(Descriptions) ? size_t(action) : 0];
	}

	MaterialStock GetForgeCost(const Item& item, ForgeAction action, uint32_t affix)
	{
		const float factor = LevelFactor(item);
		const float count = float(item.AffixCount);
		const float tier = ValidAffix(item, affix) ? float(item.Affixes[affix].Tier) : 1.0f;
		const float rarity = float(size_t(item.Quality));
		switch (action)
		{
		case ForgeAction::Temper:
			return Cost(40.0f * factor * std::pow(1.5f, count), 1.0f + count);
		case ForgeAction::Hone:
			return Cost(120.0f * factor * std::pow(2.0f, tier - 1.0f) * std::pow(1.2f, count), tier, tier);
		case ForgeAction::Imbue:
			return Cost(120.0f * factor * std::pow(2.0f, count), 3.0f * std::pow(2.0f, count));
		case ForgeAction::Reforge:
			return Cost(70.0f * factor * std::pow(1.7f, count) * (1.0f + 0.25f * (tier - 1.0f)), 2.0f + count + (tier - 1.0f));
		case ForgeAction::Ascend:
			return Cost(600.0f * factor * std::pow(3.0f, rarity), 5.0f * (rarity + 1.0f), std::pow(2.0f, rarity + 1.0f));
		default:
			return {};
		}
	}

	std::optional<std::string> CheckForge(const Item& item, ForgeAction action, uint32_t affix, StatCategory category)
	{
		if (!item.IsValid() || item.IsGem())
			return "Pick a piece of gear";
		switch (action)
		{
		case ForgeAction::Temper:
			return std::nullopt;
		case ForgeAction::Hone:
		{
			if (!ValidAffix(item, affix))
				return "Pick an affix to hone";
			const uint8_t tier = item.Affixes[affix].Tier;
			if (tier >= AffixTierCount)
				return "This affix is already at the highest tier";
			if (tier >= MaxAffixTier(item.Level))
				return "Item level " + std::to_string(item.Level) + " caps affixes at tier " + std::to_string(MaxAffixTier(item.Level));
			return std::nullopt;
		}
		case ForgeAction::Imbue:
			if (item.AffixCount >= GetAffixSlots(item.Quality) || item.AffixCount >= MaxAffixes)
				return "No free affix slot, ascend the item first";
			if (OpenStats(item, category, std::nullopt).empty())
				return std::string("No ") + GetStatCategoryName(category) + " affix can be added to this item";
			return std::nullopt;
		case ForgeAction::Reforge:
			if (!ValidAffix(item, affix))
				return "Pick an affix to reforge";
			if (OpenStats(item, std::nullopt, item.Affixes[affix].Kind).empty())
				return "No other stat fits this item";
			return std::nullopt;
		case ForgeAction::Ascend:
			if (item.Quality >= Rarity::Mythic)
				return "Mythic is the highest rarity";
			return std::nullopt;
		default:
			return "Unknown action";
		}
	}

	float GetHoneChance(const Item& item, uint32_t affix)
	{
		if (!ValidAffix(item, affix))
			return 0.0f;
		const uint8_t target = uint8_t(std::min<uint32_t>(item.Affixes[affix].Tier + 1u, AffixTierCount));
		return std::min(HoneChances[target] + PityStep * float(item.Pity), MaxHoneChance);
	}

	CraftResult ApplyForge(Random& random, Item& item, ForgeAction action, uint32_t affix, StatCategory category)
	{
		CraftResult result;
		if (const std::optional<std::string> reason = CheckForge(item, action, affix, category))
		{
			result.Message = *reason;
			return result;
		}
		result.Applied = true;
		result.Success = true;
		switch (action)
		{
		case ForgeAction::Temper:
			item.Power = LevelScale(item.Level) * (0.95f + 0.1f * random.Float());
			for (uint8_t i = 0; i < item.AffixCount; i++)
				item.Affixes[i].Value = RollAffixValue(random, item.Affixes[i].Kind, item.Affixes[i].Tier);
			result.Message = "Tempered " + GetItemName(item);
			break;
		case ForgeAction::Hone:
		{
			const float chance = GetHoneChance(item, affix);
			Affix& target = item.Affixes[affix];
			if (random.Float() < chance)
			{
				target.Tier++;
				target.Value = RollAffixValue(random, target.Kind, target.Tier);
				item.Pity = 0;
				result.Message = "Honed " + DescribeAffix(target) + " to tier " + std::to_string(target.Tier);
			}
			else
			{
				item.Pity = uint8_t(std::min<uint32_t>(item.Pity + 1u, MaxPity));
				result.Success = false;
				result.Message = "The hone failed. The next try has a " + std::to_string(int(std::lround(GetHoneChance(item, affix) * 100.0f))) + "% chance";
			}
			break;
		}
		case ForgeAction::Imbue:
		{
			const std::vector<Stat> options = OpenStats(item, category, std::nullopt);
			Affix& added = item.Affixes[item.AffixCount++];
			added.Kind = options[random.Below(uint32_t(options.size()))];
			added.Tier = RollAffixTier(random, std::min<uint8_t>(MaxAffixTier(item.Level), 2));
			added.Value = RollAffixValue(random, added.Kind, added.Tier);
			result.Message = "Imbued " + DescribeAffix(added);
			break;
		}
		case ForgeAction::Reforge:
		{
			Affix& target = item.Affixes[affix];
			const std::vector<Stat> options = OpenStats(item, std::nullopt, target.Kind);
			const std::string previous = DescribeAffix(target);
			target.Kind = options[random.Below(uint32_t(options.size()))];
			target.Tier = std::min(target.Tier, MaxAffixTier(item.Level));
			target.Value = RollAffixValue(random, target.Kind, target.Tier);
			result.Message = "Reforged " + previous + " into " + DescribeAffix(target);
			break;
		}
		case ForgeAction::Ascend:
			item.Quality = Rarity(size_t(item.Quality) + 1);
			result.Message = std::string("Ascended to ") + GetRarity(item.Quality).Name;
			break;
		default:
			break;
		}
		return result;
	}

	MaterialStock GetGemCost(const Item& gem, GemAction action)
	{
		const float level = float(gem.Level);
		const float quality = float(gem.GemQuality);
		switch (action)
		{
		case GemAction::Empower:
			return Cost(60.0f * level, 0.0f, 0.0f, std::ceil(2.0f * std::pow(1.2f, level - 1.0f)));
		case GemAction::Polish:
			return Cost(80.0f + 20.0f * quality, 0.0f, 0.0f, 3.0f + quality * 0.5f);
		case GemAction::Corrupt:
			return Cost(500.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		default:
			return {};
		}
	}

	std::optional<std::string> CheckGemCraft(const Item& gem, GemAction action)
	{
		if (!gem.IsGem())
			return "Pick a skill gem";
		if (gem.Corrupted)
			return "Corrupted gems can't be changed";
		switch (action)
		{
		case GemAction::Empower:
			if (gem.Level >= MaxGemLevel)
				return "Level 20 is the limit, only corruption goes further";
			return std::nullopt;
		case GemAction::Polish:
			if (gem.GemQuality >= MaxGemQuality)
				return "This gem is already at 20% quality";
			return std::nullopt;
		case GemAction::Corrupt:
			return std::nullopt;
		default:
			return "Unknown action";
		}
	}

	CraftResult ApplyGemCraft(Random& random, Item& gem, GemAction action)
	{
		CraftResult result;
		if (const std::optional<std::string> reason = CheckGemCraft(gem, action))
		{
			result.Message = *reason;
			return result;
		}
		result.Applied = true;
		result.Success = true;
		switch (action)
		{
		case GemAction::Empower:
			gem.Level++;
			result.Message = GetItemName(gem) + " is now level " + std::to_string(gem.Level);
			break;
		case GemAction::Polish:
			gem.GemQuality = uint8_t(std::min<uint32_t>(gem.GemQuality + PolishStep, MaxGemQuality));
			result.Message = GetItemName(gem) + " now has " + std::to_string(gem.GemQuality) + "% quality";
			break;
		case GemAction::Corrupt:
		{
			gem.Corrupted = true;
			const uint32_t roll = random.Below(100);
			if (roll < 30)
			{
				gem.Level = uint8_t(std::min<uint32_t>(gem.Level + 1u, CorruptedGemLevel));
				result.Message = "Corruption raised the gem to level " + std::to_string(gem.Level);
			}
			else if (roll < 50)
			{
				gem.Level = uint8_t(std::max<uint32_t>(gem.Level, 2u) - 1u);
				result.Success = false;
				result.Message = "Corruption lowered the gem to level " + std::to_string(gem.Level);
			}
			else if (roll < 80)
			{
				gem.Mod = GemMod(1 + random.Below(uint32_t(GemMod::Count) - 1));
				result.Message = std::string("Corruption added ") + GetGemMod(gem.Mod).Name + ": " + GetGemMod(gem.Mod).Description;
			}
			else
			{
				result.Success = false;
				result.Message = "Corruption changed nothing, but the gem is now sealed";
			}
			break;
		}
		default:
			break;
		}
		return result;
	}

	MaterialStock GetSalvageValue(const Item& item)
	{
		if (!item.IsValid())
			return {};
		if (item.IsGem())
			return Cost(20.0f * float(item.Level), 0.0f, 0.0f, 1.0f + float(item.Level) * 0.5f + float(item.GemQuality) * 0.25f);
		const float rarity = float(size_t(item.Quality));
		return Cost(15.0f * LevelFactor(item) * (1.0f + rarity), 1.0f + rarity + float(item.AffixCount), rarity >= 3.0f ? rarity - 2.0f : 0.0f);
	}

	bool CanAfford(const MaterialStock& stock, const MaterialStock& cost)
	{
		for (size_t i = 0; i < stock.size(); i++)
			if (stock[i] < cost[i])
				return false;
		return true;
	}

	void Spend(MaterialStock& stock, const MaterialStock& cost)
	{
		for (size_t i = 0; i < stock.size(); i++)
			stock[i] -= std::min(stock[i], cost[i]);
	}

	void Gain(MaterialStock& stock, const MaterialStock& gain)
	{
		for (size_t i = 0; i < stock.size(); i++)
			stock[i] = std::min(MaxMaterialStack, stock[i] + std::min(gain[i], MaxMaterialStack));
	}
}
