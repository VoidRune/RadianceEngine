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

		constexpr uint32_t ProfileVersion = 3;
		constexpr const char* AbilityKeys[] = { "Roll", "Bite", "Spit", "Slam", "Firestrike", "TomeVolley", "PageStorm", "ShelfCrush", "ArcaneRing", "ArcaneBurst", "InkSpiral", "CrystalSpike", "TailSweep", "Eruption", "ShardBurst", "CrystalRain", "EmberBolt", "Gust", "Comet", "Warp", "GlacialNova", "GravityWell", "ArcLightning", "DiamondWard", "FrostSpear", "ChainHook", "Firebomb", "ObsidianBlades", "StormStrike", "FirebombSkip", "FirebombSkip2", "StormJump", "HookAnchor", "HookPull", "HookYank", "FirebombFlame", "Ignite", "ShatterBurst", "EarthSpike", "PurpleFireball", "HexShield", "GlacialEruption", "WellBurst", "WarpWake", "CrawlerBite", "Pounce", "ShardSpit", "RubyBurst", "RubyShard", "Aftershock", "Resonance" };
		static_assert(std::size(AbilityKeys) == size_t(AbilityId::Count));

		std::optional<AbilityId> AbilityFromKey(const std::string& name)
		{
			if (name == "Frostbite")
				return AbilityId::FrostSpear;
			if (name == "MirrorWard")
				return AbilityId::DiamondWard;
			if (name == "Shockwave")
				return AbilityId::GlacialNova;
			if (name == "SplittingBolt" || name == "ObsidianShards")
				return AbilityId::ObsidianBlades;
			const auto key = std::find(std::begin(AbilityKeys), std::end(AbilityKeys), name);
			return key != std::end(AbilityKeys) ? std::optional<AbilityId>(AbilityId(key - std::begin(AbilityKeys))) : std::nullopt;
		}

		std::string SafeFileName(const std::string& name)
		{
			std::string safe;
			for (const char c : name)
				safe += std::isalnum(uint8_t(c)) || c == '-' || c == '_' ? c : '_';
			return safe.empty() ? "Player" : safe.substr(0, 40);
		}
	}

	CharacterProfile MakeNewProfile(std::string name)
	{
		CharacterProfile profile;
		profile.Name = std::move(name);
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
				const std::string focus = text("focus");
				for (const OutfitDef& def : Outfits)
					if (outfit == def.Key)
						profile.Style.Body = def.Id;
				for (const FocusDef& def : Foci)
					if (focus == def.Key)
						profile.Style.Cast = def.Id;
				const std::string aura = text("aura");
				const std::string headpiece = text("headpiece");
				for (const AuraDef& def : Auras)
					if (aura == def.Key)
						profile.Style.Glow = def.Id;
				for (const HeadpieceDef& def : Headpieces)
					if (headpiece == def.Key)
						profile.Style.Crown = def.Id;
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
		Json spells = Json::array();
		for (const AbilityId spell : profile.Spells)
			spells.push_back(AbilityKeys[size_t(spell) < std::size(AbilityKeys) ? size_t(spell) : 0]);
		const Json look = { { "outfit", GetOutfit(profile.Style.Body).Key }, { "focus", GetFocus(profile.Style.Cast).Key }, { "aura", GetAura(profile.Style.Glow).Key }, { "headpiece", GetHeadpiece(profile.Style.Crown).Key } };
		Json root = { { "version", ProfileVersion }, { "name", profile.Name }, { "look", look }, { "spells", spells } };
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
		std::string text;
		for (const AbilityId spell : profile.Spells)
			text += std::string(text.empty() ? "" : ", ") + GetAbility(spell).Name;
		return text;
	}
}
