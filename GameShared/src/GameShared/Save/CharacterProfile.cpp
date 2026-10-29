#include "CharacterProfile.h"
#include "GameShared/Content/Hub.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <optional>
#include <stdexcept>

namespace Game
{
	namespace
	{
		using Json = nlohmann::json;

		constexpr uint32_t ProfileVersion = 2;
		constexpr const char* GearKeys[] = { "weapon", "head", "body", "feet", "amulet", "ring1", "ring2" };
		static_assert(std::size(GearKeys) == GearSlotCount);
		constexpr const char* BaseKeys[] = { "Wand", "Staff", "Sword", "Axe", "Dagger", "Spear", "Bow", "Crossbow", "WizardHat", "Hood", "Helm", "Robe", "Tunic", "Plate", "Sandals", "Boots", "Greaves", "Amulet", "Ring", "Gem" };
		static_assert(std::size(BaseKeys) == size_t(ItemBase::Count));
		constexpr const char* StatKeys[] = { "Damage", "PhysicalDamage", "FireDamage", "ColdDamage", "LightningDamage", "AttackSpeed", "CastSpeed", "CritChance", "CritDamage", "LifeSteal", "AreaSize", "ExtraProjectiles", "Pierce", "ProjectileSpeed", "CooldownRecovery", "GemLevel", "MaxHealth", "MaxMana", "ManaRegen", "Regeneration", "Armor", "FireResist", "ColdResist", "LightningResist", "PoisonResist", "MoveSpeed" };
		static_assert(std::size(StatKeys) == size_t(Stat::Count));
		constexpr const char* AbilityKeys[] = { "Bolt", "Orb", "Slash", "Chop", "Stab", "Thrust", "Arrow", "Quarrel", "Fireball", "Meteor", "FrostNova", "IceSpear", "ChainLightning", "Blink", "Cyclone", "GroundSlam", "Volley", "RainOfArrows", "PiercingShot", "Rejuvenate", "Roll", "Bite", "Spit", "Slam", "Firestrike", "TomeVolley", "PageStorm", "ShelfCrush", "ArcaneRing", "ArcaneBurst", "InkSpiral", "CrystalSpike", "TailSweep", "Eruption", "ShardBurst", "CrystalRain", "EmberBolt", "Gust", "Comet", "Warp", "Shockwave", "GravityWell", "ArcLightning", "Swap", "DiamondWard", "FrostSpear", "ChainHook", "Firebomb", "ObsidianShards", "StormStrike", "FirebombSkip", "FirebombSkip2", "StormJump", "HookAnchor", "HookPull", "HookYank", "FirebombFlame", "Ignite", "ShatterBurst", "EarthSpike", "PurpleFireball", "HexShield" };
		static_assert(std::size(AbilityKeys) == size_t(AbilityId::Count));

		std::optional<AbilityId> AbilityFromKey(const std::string& name)
		{
			if (name == "Frostbite")
				return AbilityId::FrostSpear;
			if (name == "MirrorWard")
				return AbilityId::DiamondWard;
			if (name == "SplittingBolt")
				return AbilityId::ObsidianShards;
			const auto key = std::find(std::begin(AbilityKeys), std::end(AbilityKeys), name);
			return key != std::end(AbilityKeys) ? std::optional<AbilityId>(AbilityId(key - std::begin(AbilityKeys))) : std::nullopt;
		}
		constexpr const char* RarityKeys[] = { "Common", "Uncommon", "Rare", "Legendary", "Mythic" };
		static_assert(std::size(RarityKeys) == size_t(Rarity::Count));
		constexpr const char* KitKeys[] = { "Arcanist", "Knight", "Ranger", "Lancer" };
		constexpr const char* GemModKeys[] = { "None", "Ruthless", "Frugal", "Quickened", "Restless", "Vast", "Split" };
		static_assert(std::size(GemModKeys) == size_t(GemMod::Count));
		constexpr const char* MaterialKeys[] = { "gold", "shards", "starsteel", "arcaneDust", "voidShards" };
		static_assert(std::size(MaterialKeys) == size_t(Material::Count));
		static_assert(std::size(KitKeys) == size_t(StarterKit::Count));

		template<size_t Size>
		size_t FindKey(const char* const (&keys)[Size], const std::string& text, const char* what)
		{
			for (size_t i = 0; i < Size; i++)
				if (text == keys[i])
					return i;
			throw std::runtime_error(std::string("unknown ") + what + " \"" + text + "\"");
		}

		double Round(float value)
		{
			return std::round(double(value) * 100000.0) / 100000.0;
		}

		Json ItemToJson(const Item& item)
		{
			if (!item.IsValid())
				return nullptr;
			Json json = { { "base", BaseKeys[size_t(item.Base)] }, { "level", item.Level } };
			if (item.IsGem())
			{
				json["skill"] = AbilityKeys[size_t(item.Skill)];
				if (item.GemQuality > 0)
					json["quality"] = item.GemQuality;
				if (item.Mod != GemMod::None)
					json["mod"] = GemModKeys[size_t(item.Mod)];
				if (item.Corrupted)
					json["corrupted"] = true;
				return json;
			}
			json["rarity"] = RarityKeys[size_t(item.Quality)];
			json["power"] = Round(item.Power);
			if (item.Pity > 0)
				json["pity"] = item.Pity;
			Json affixes = Json::array();
			for (uint8_t i = 0; i < item.AffixCount; i++)
				affixes.push_back({ { "stat", StatKeys[size_t(item.Affixes[i].Kind)] }, { "value", Round(item.Affixes[i].Value) }, { "tier", item.Affixes[i].Tier } });
			json["affixes"] = affixes;
			return json;
		}

		Item ItemFromJson(const Json& json, uint32_t& nextId)
		{
			if (json.is_null())
				return {};
			if (!json.is_object())
				throw std::runtime_error("items must be objects or null");
			Item item;
			item.Id = nextId++;
			item.Base = ItemBase(FindKey(BaseKeys, json.at("base").get<std::string>(), "item base"));
			item.Level = uint8_t(std::clamp(json.value("level", 1), 1, 99));
			if (item.Base == ItemBase::Gem)
			{
				item.Skill = AbilityId(FindKey(AbilityKeys, json.at("skill").get<std::string>(), "skill"));
				item.GemQuality = uint8_t(std::clamp(json.value("quality", 0), 0, 255));
				item.Mod = GemMod(FindKey(GemModKeys, json.value("mod", std::string("None")), "gem mod"));
				item.Corrupted = json.value("corrupted", false);
				return item;
			}
			item.Quality = Rarity(FindKey(RarityKeys, json.value("rarity", std::string("Common")), "rarity"));
			item.Power = json.value("power", 1.0f);
			item.Pity = uint8_t(std::clamp(json.value("pity", 0), 0, 255));
			if (json.contains("affixes"))
				for (const Json& affix : json["affixes"])
				{
					if (item.AffixCount >= MaxAffixes)
						break;
					item.Affixes[item.AffixCount++] = { Stat(FindKey(StatKeys, affix.at("stat").get<std::string>(), "stat")), affix.at("value").get<float>(), uint8_t(std::clamp(affix.value("tier", 0), 0, 255)) };
				}
			return item;
		}

		std::string SafeFileName(const std::string& name)
		{
			std::string safe;
			for (const char c : name)
				safe += std::isalnum(uint8_t(c)) || c == '-' || c == '_' ? c : '_';
			return safe.empty() ? "Player" : safe.substr(0, 40);
		}
	}

	CharacterProfile MakeNewProfile(std::string name, StarterKit kit)
	{
		CharacterProfile profile;
		profile.Name = std::move(name);
		profile.Kit = kit;
		profile.Gear = MakeStarterGear(kit);
		return profile;
	}

	std::filesystem::path GetProfilePath(const std::string& name)
	{
		return std::filesystem::path(ProfileDirectory) / (SafeFileName(name) + ".json");
	}

	std::optional<CharacterProfile> LoadProfile(const std::filesystem::path& path, std::string& error)
	{
		std::ifstream file(path);
		if (!file)
		{
			error = "no saved character at " + path.string();
			return std::nullopt;
		}
		try
		{
			const Json root = Json::parse(file);
			if (!root.is_object() || root.value("version", 0u) > ProfileVersion)
				throw std::runtime_error("unsupported save version");
			CharacterProfile profile;
			profile.Name = root.value("name", std::string());
			profile.Kit = StarterKit(FindKey(KitKeys, root.value("kit", std::string("Arcanist")), "kit"));
			uint32_t nextId = 1;
			GearSet gear;
			if (root.contains("gear"))
				for (size_t slot = 0; slot < GearSlotCount; slot++)
					if (root["gear"].contains(GearKeys[slot]))
						gear.Gear[slot] = ItemFromJson(root["gear"][GearKeys[slot]], nextId);
			if (root.contains("gems"))
				for (size_t i = 0; i < SkillSlots && i < root["gems"].size(); i++)
					gear.Gems[i] = ItemFromJson(root["gems"][i], nextId);
			if (root.contains("backpack"))
				for (size_t i = 0; i < BackpackSize && i < root["backpack"].size(); i++)
					gear.Pack[i] = ItemFromJson(root["backpack"][i], nextId);
			if (root.contains("stash"))
				for (size_t i = 0; i < StashSize && i < root["stash"].size(); i++)
					gear.Stash[i] = ItemFromJson(root["stash"][i], nextId);
			if (root.contains("materials"))
				for (size_t i = 0; i < size_t(Material::Count); i++)
					gear.Materials[i] = uint32_t(std::clamp<int64_t>(root["materials"].value(MaterialKeys[i], int64_t(0)), 0, MaxMaterialStack));
			profile.Gear = SanitizeGear(gear);
			if (root.contains("spells") && root["spells"].is_array())
				for (size_t slot = 0; slot < profile.Spells.size() && slot < root["spells"].size(); slot++)
				{
					const std::string name = root["spells"][slot].is_string() ? root["spells"][slot].get<std::string>() : std::string();
					if (const std::optional<AbilityId> spell = AbilityFromKey(name))
						AssignSpell(profile.Spells, uint32_t(slot), *spell);
				}
			if (root.contains("falls") && root["falls"].is_number_unsigned())
				profile.Falls = uint32_t(std::min<uint64_t>(root["falls"].get<uint64_t>(), UINT32_MAX));
			if (root.contains("starlit") && root["starlit"].is_array())
				for (const Json& entry : root["starlit"])
				{
					if (const std::optional<AbilityId> spell = entry.is_string() ? AbilityFromKey(entry.get<std::string>()) : std::nullopt)
						profile.Starlit |= uint64_t(1) << size_t(*spell);
				}
			if (root.contains("honors") && root["honors"].is_array())
				for (const Json& entry : root["honors"])
					for (const HonorDef& honor : Honors)
						if (entry.is_string() && entry.get<std::string>() == honor.Key)
							profile.Honors |= HonorBit(honor.Id);
			if (root.contains("achievements") && root["achievements"].is_array())
				for (const Json& entry : root["achievements"])
					for (const AchievementDef& achievement : Achievements)
						if (entry.is_string() && entry.get<std::string>() == achievement.Key)
							profile.Achievements |= AchievementBit(achievement.Id);
			if (root.contains("scholar") && root["scholar"].is_array())
				for (const Json& entry : root["scholar"])
					for (size_t i = 0; i < std::size(BrawlSpellPool); i++)
						if (entry.is_string() && AbilityFromKey(entry.get<std::string>()) == BrawlSpellPool[i])
							profile.Scholar |= 1u << uint32_t(i);
			profile.Achievements |= AchievementsFromHonors(profile.Honors);
			if (root.contains("look") && root["look"].is_object())
			{
				const Json& look = root["look"];
				const auto text = [&](const char* key) { return look.contains(key) && look[key].is_string() ? look[key].get<std::string>() : std::string(); };
				const std::string outfit = text("outfit");
				const std::string headwear = text("headwear");
				for (const OutfitDef& def : Outfits)
					if (outfit == def.Key)
						profile.Style.Body = def.Id;
				for (const HeadwearDef& def : Headwears)
					if (headwear == def.Key)
						profile.Style.Head = def.Id;
			}
			return profile;
		}
		catch (const std::exception& exception)
		{
			error = path.string() + ": " + exception.what();
			return std::nullopt;
		}
	}

	bool SaveProfile(const std::filesystem::path& path, const CharacterProfile& profile, std::string& error)
	{
		Json gear = Json::object();
		for (size_t slot = 0; slot < GearSlotCount; slot++)
			gear[GearKeys[slot]] = ItemToJson(profile.Gear.Gear[slot]);
		Json gems = Json::array();
		for (const Item& gem : profile.Gear.Gems)
			gems.push_back(ItemToJson(gem));
		Json backpack = Json::array();
		for (const Item& item : profile.Gear.Pack)
			backpack.push_back(ItemToJson(item));
		Json stash = Json::array();
		for (const Item& item : profile.Gear.Stash)
			stash.push_back(ItemToJson(item));
		Json materials = Json::object();
		for (size_t i = 0; i < size_t(Material::Count); i++)
			materials[MaterialKeys[i]] = profile.Gear.Materials[i];
		Json spells = Json::array();
		for (const AbilityId spell : profile.Spells)
			spells.push_back(AbilityKeys[size_t(spell) < std::size(AbilityKeys) ? size_t(spell) : 0]);
		const Json look = { { "outfit", GetOutfit(profile.Style.Body).Key }, { "headwear", GetHeadwear(profile.Style.Head).Key } };
		Json root = { { "version", ProfileVersion }, { "name", profile.Name }, { "kit", KitKeys[size_t(profile.Kit)] }, { "look", look }, { "gear", gear }, { "gems", gems }, { "backpack", backpack }, { "stash", stash }, { "materials", materials }, { "spells", spells } };
		if (profile.Falls > 0)
			root["falls"] = profile.Falls;
		if (profile.Starlit != 0)
		{
			Json starlit = Json::array();
			for (size_t i = 0; i < std::size(AbilityKeys); i++)
				if ((profile.Starlit >> i) & 1u)
					starlit.push_back(AbilityKeys[i]);
			root["starlit"] = starlit;
		}
		if (profile.Honors != 0)
		{
			Json honors = Json::array();
			for (const HonorDef& honor : Honors)
				if ((profile.Honors & HonorBit(honor.Id)) != 0)
					honors.push_back(honor.Key);
			root["honors"] = honors;
		}
		if (profile.Achievements != 0)
		{
			Json achievements = Json::array();
			for (const AchievementDef& achievement : Achievements)
				if ((profile.Achievements & AchievementBit(achievement.Id)) != 0)
					achievements.push_back(achievement.Key);
			root["achievements"] = achievements;
		}
		if (profile.Scholar != 0)
		{
			Json scholar = Json::array();
			for (size_t i = 0; i < std::size(BrawlSpellPool); i++)
				if ((profile.Scholar >> i) & 1u)
					scholar.push_back(AbilityKeys[size_t(BrawlSpellPool[i])]);
			root["scholar"] = scholar;
		}

		std::error_code code;
		if (path.has_parent_path())
			std::filesystem::create_directories(path.parent_path(), code);
		const std::filesystem::path temporary = path.string() + ".tmp";
		{
			std::ofstream file(temporary, std::ios::trunc);
			if (!file)
			{
				error = "can't write " + temporary.string();
				return false;
			}
			file << root.dump(1, '\t') << '\n';
			if (!file)
			{
				error = "can't write " + temporary.string();
				return false;
			}
		}
		std::filesystem::rename(temporary, path, code);
		if (code)
		{
			error = "can't replace " + path.string() + ": " + code.message();
			return false;
		}
		return true;
	}

	std::string DescribeProfile(const CharacterProfile& profile)
	{
		const Item& weapon = profile.Gear.Gear[size_t(GearSlot::Weapon)];
		std::string text = weapon.IsValid() ? GetItemName(weapon) : "no weapon";
		std::string gems;
		for (const Item& gem : profile.Gear.Gems)
			if (gem.IsGem())
				gems += std::string(gems.empty() ? "" : ", ") + GetAbility(gem.Skill).Name;
		const size_t carried = size_t(std::count_if(profile.Gear.Pack.begin(), profile.Gear.Pack.end(), [](const Item& item) { return item.IsValid(); }));
		text += gems.empty() ? ", no skill gems" : ", " + gems;
		const size_t stored = size_t(std::count_if(profile.Gear.Stash.begin(), profile.Gear.Stash.end(), [](const Item& item) { return item.IsValid(); }));
		text += ", " + std::to_string(carried) + " items in the backpack, " + std::to_string(stored) + " in the stash, ";
		text += std::to_string(profile.Gear.Materials[size_t(Material::Gold)]) + " gold";
		return text;
	}
}
