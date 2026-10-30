#include "LaunchOptions.h"
#include "App/GraphicsTier.h"
#include "Rendering/Lighting.h"
#include "Rendering/PostProcess.h"
#include <GameShared/Net/Protocol.h>
#include <RadianceEngine/Core/Log.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <iterator>
#include <string_view>

namespace Game
{
	namespace
	{
		template<typename T>
		bool ParseNumber(std::string_view text, T& value)
		{
			const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
			return error == std::errc() && end == text.data() + text.size();
		}

		bool SameText(std::string_view a, std::string_view b)
		{
			return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return std::tolower(uint8_t(x)) == std::tolower(uint8_t(y)); });
		}

		bool ParseMap(std::string_view text, MapKind& map)
		{
			if (SameText(text, "abbey") || SameText(text, GetMapDef(MapKind::Abbey).Name))
			{
				map = MapKind::Abbey;
				return true;
			}
			for (const MapDef& def : Maps)
				if (IsMissionMap(def.Id) && (SameText(def.Name, text) || (def.Id == MapKind::Town && SameText(text, "town"))))
				{
					map = def.Id;
					return true;
				}
			return false;
		}

		bool ParseModifiers(std::string_view text, ModifierMask& mask)
		{
			mask = 0;
			while (!text.empty())
			{
				const size_t comma = text.find(',');
				const std::string_view name = text.substr(0, comma);
				const auto it = std::find_if(std::begin(MissionModifiers), std::end(MissionModifiers), [&](const ModifierDef& def) { return SameText(def.Name, name); });
				if (it == std::end(MissionModifiers))
					return false;
				mask |= ModifierMask(1u << uint32_t(it->Id));
				text = comma == std::string_view::npos ? std::string_view() : text.substr(comma + 1);
			}
			return true;
		}

		bool ParseRoyaleMap(std::string_view text, MapKind& map)
		{
			if (SameText(text, "ashlands") || SameText(text, GetMapDef(MapKind::Ashlands).Name))
			{
				map = MapKind::Ashlands;
				return true;
			}
			return ParseMap(text, map);
		}

		bool ParseKit(std::string_view text, StarterKit& kit)
		{
			for (const KitDef& def : Kits)
			{
				if (SameText(def.Name, text))
				{
					kit = def.Id;
					return true;
				}
			}
			return false;
		}
	}

	LaunchOptions ParseLaunchOptions(int argc, char** argv)
	{
		constexpr std::string_view usage = "usage: Game [--host [port]] [--join address[:port]] [--name name] [--kit arcanist|knight|ranger|lancer] [--new-character] [--no-save] [--export-props] [--mission [town|caves]] [--horde [town|caves|abbey]] [--arena [ffa|teams]] [--royale [ashlands|town|caves]] [--brawl [rounds]] [--brawl-map caldera|lake|ruins|peak|random] [--brawl-mode damage|knockback] [--bots 0..11] [--bot-skill easy|normal|hard] [--teams 2..4] [--health 25..500] [--mana 25..500] [--damage 25..300] [--cast-speed 50..300] [--cooldown-speed 50..300] [--move-speed 50..200] [--mana-regen 0..300] [--knockback 25..300] [--random-spells] [--no-apex] [--apex-draft] [--no-crumble] [--no-winner-card] [--spectate] [--goal kills] [--minutes 3..20] [--tier 1..10] [--mods brutal,fortified,...] [--unlimited-materials] [--volume 0..1] [--music-volume 0..1] [--effects-volume 0..1] [--mute] [--peaceful] [--start-wave n] [--loot-rate multiplier] [--lag ms] [--loss percent] [--no-validation] [--capture-audio file.wav] [--log-audio] [--debug-view lit|albedo|normal|roughness|metallic|occlusion|emission|sun|ambient|shadow|irradiance|gisource|translucency] [--tonemap softclip|neutral|agx|aces|linear] [--shadows off|low|high] [--gi off|low|medium|high] [--vsync on|off] [--frame-cap 30..240] [--screen-shake 0..100] [--performance] [--exposure x] [--no-grading] [--lighting-test] [--profile-gpu] [--leviathan] [--no-distortion] [--no-trails] [--no-translucency] [--distortion-stage all|shockwaves] [--no-steam]";
		LaunchOptions options;
		for (int i = 1; i < argc; i++)
		{
			const std::string_view argument = argv[i];
			const bool hasValue = i + 1 < argc && std::string_view(argv[i + 1]).substr(0, 2) != "--";
			const std::string_view value = hasValue ? std::string_view(argv[i + 1]) : std::string_view();
			bool valid = true;
			if (argument == "--host")
			{
				uint16_t port = DefaultPort;
				if (hasValue && ParseNumber(value, port))
					i++;
				options.HostPort = port;
			}
			else if (argument == "--join" && hasValue)
			{
				options.JoinAddress = std::string(value);
				i++;
			}
			else if (argument == "--name" && hasValue)
			{
				options.Name = std::string(value);
				i++;
			}
			else if (argument == "--lag" && hasValue)
			{
				valid = ParseNumber(value, options.Simulation.LagMs);
				i++;
			}
			else if (argument == "--loss" && hasValue)
			{
				valid = ParseNumber(value, options.Simulation.LossPercent);
				i++;
			}
			else if ((argument == "--kit" || argument == "--class") && hasValue)
			{
				StarterKit kit = StarterKit::Arcanist;
				valid = ParseKit(value, kit);
				if (valid)
					options.Kit = kit;
				i++;
			}
			else if (argument == "--map" && hasValue)
			{
				MapKind map = MapKind::Town;
				valid = ParseMap(value, map);
				if (valid)
					options.Map = map;
				i++;
			}
			else if (argument == "--mission" || argument == "--horde")
			{
				options.StartMode = argument == "--mission" ? GameMode::Mission : GameMode::Horde;
				MapKind map = MapKind::Town;
				if (hasValue && ParseMap(value, map))
				{
					options.Map = map;
					i++;
				}
			}
			else if (argument == "--arena")
			{
				options.StartMode = GameMode::Arena;
				options.Map = MapKind::Crucible;
				if (hasValue && (value == "teams" || value == "ffa"))
				{
					options.Rules.Teams = value == "teams" ? 2 : 0;
					i++;
				}
			}
			else if (argument == "--royale")
			{
				options.StartMode = GameMode::Royale;
				MapKind map = MapKind::Ashlands;
				if (hasValue && ParseRoyaleMap(value, map))
					i++;
				options.Map = map;
			}
			else if (argument == "--brawl")
			{
				options.StartMode = GameMode::Brawl;
				if (!options.Map || (!IsBrawlMap(*options.Map) && *options.Map != RandomBrawlMap))
					options.Map = MapKind::Caldera;
				uint32_t rounds = 0;
				if (hasValue && ParseNumber(value, rounds))
				{
					valid = rounds >= MinBrawlRounds && rounds <= MaxBrawlRounds;
					if (valid)
						options.Rules.Rounds = uint8_t(rounds);
					i++;
				}
			}
			else if (argument == "--brawl-map" && hasValue)
			{
				valid = value == "caldera" || value == "lake" || value == "ruins" || value == "peak" || value == "random";
				if (valid)
					options.Map = value == "caldera" ? MapKind::Caldera : value == "lake" ? MapKind::FrozenLake : value == "ruins" ? MapKind::SkyRuins : value == "peak" ? MapKind::StormPeak : RandomBrawlMap;
				i++;
			}
			else if (argument == "--brawl-mode" && hasValue)
			{
				const auto mode = std::find(std::begin(BrawlModeNames), std::end(BrawlModeNames), value);
				valid = mode != std::end(BrawlModeNames);
				if (valid)
					options.Rules.Mode = BrawlMode(mode - std::begin(BrawlModeNames));
				i++;
			}
			else if (argument == "--bots" && hasValue)
			{
				uint32_t bots = 0;
				valid = ParseNumber(value, bots) && bots <= MaxBots;
				if (valid)
					options.Rules.Bots = uint8_t(bots);
				i++;
			}
			else if (argument == "--bot-skill" && hasValue)
			{
				constexpr std::string_view Levels[] = { "easy", "normal", "hard" };
				const auto level = std::find(std::begin(Levels), std::end(Levels), value);
				valid = level != std::end(Levels);
				if (valid)
					options.Rules.BotLevel = uint8_t(level - std::begin(Levels));
				i++;
			}
			else if (argument == "--teams" && hasValue)
			{
				uint32_t teams = 0;
				valid = ParseNumber(value, teams) && (teams == 0 || (teams >= 2 && teams <= MaxTeams));
				if (valid)
					options.Rules.Teams = uint8_t(teams);
				i++;
			}
			else if ((argument == "--health" || argument == "--mana") && hasValue)
			{
				uint32_t amount = 0;
				const bool health = argument == "--health";
				valid = ParseNumber(value, amount) && amount >= (health ? MinBrawlHealth : MinBrawlMana) && amount <= (health ? MaxBrawlHealth : MaxBrawlMana);
				if (valid)
					(health ? options.Rules.Health : options.Rules.Mana) = uint16_t(amount);
				i++;
			}
			else if (argument == "--damage" && hasValue)
			{
				uint32_t percent = 0;
				valid = ParseNumber(value, percent) && percent >= MinDamagePercent && percent <= MaxDamagePercent;
				if (valid)
					options.Rules.Damage = uint16_t(percent);
				i++;
			}
			else if ((argument == "--cast-speed" || argument == "--cooldown-speed" || argument == "--move-speed" || argument == "--mana-regen" || argument == "--knockback") && hasValue)
			{
				uint32_t percent = 0;
				uint16_t& field = argument == "--cast-speed" ? options.Rules.CastSpeed : argument == "--cooldown-speed" ? options.Rules.CooldownSpeed : argument == "--move-speed" ? options.Rules.MoveSpeed : argument == "--mana-regen" ? options.Rules.ManaRegen : options.Rules.Knockback;
				valid = ParseNumber(value, percent) && percent <= 1000;
				if (valid)
					field = uint16_t(percent);
				i++;
			}
			else if (argument == "--random-spells")
				options.Rules.RandomSpells = true;
			else if (argument == "--no-apex")
				options.Rules.Apex = false;
			else if (argument == "--apex-draft")
				options.Rules.ApexDraft = true;
			else if (argument == "--no-crumble")
				options.Rules.Crumble = false;
			else if (argument == "--no-winner-card")
				options.Rules.WinnerCard = false;
			else if (argument == "--spectate")
				options.Rules.Seats[0] = SpectatorSeat;
			else if (argument == "--goal" && hasValue)
			{
				uint32_t goal = 0;
				valid = ParseNumber(value, goal) && goal >= MinScoreGoal && goal <= MaxScoreGoal;
				if (valid)
					options.Rules.ScoreGoal = uint8_t(goal);
				i++;
			}
			else if (argument == "--minutes" && hasValue)
			{
				uint32_t minutes = 0;
				valid = ParseNumber(value, minutes) && minutes >= MinMatchMinutes && minutes <= MaxMatchMinutes;
				if (valid)
					options.Rules.Minutes = uint8_t(minutes);
				i++;
			}
			else if (argument == "--tier" && hasValue)
			{
				uint32_t tier = 1;
				valid = ParseNumber(value, tier) && tier >= 1 && tier <= MaxMissionTier;
				if (valid)
					options.Tier = uint8_t(tier);
				i++;
			}
			else if ((argument == "--mods" || argument == "--modifiers") && hasValue)
			{
				valid = ParseModifiers(value, options.Modifiers);
				i++;
			}
			else if (argument == "--unlimited-materials" || argument == "--unlimited")
				options.UnlimitedMaterials = true;
			else if (argument == "--volume" && hasValue)
			{
				float volume = 1.0f;
				valid = ParseNumber(value, volume) && volume >= 0.0f && volume <= 1.0f;
				if (valid)
					options.Volume = volume;
				i++;
			}
			else if ((argument == "--music-volume" || argument == "--effects-volume") && hasValue)
			{
				float volume = 1.0f;
				valid = ParseNumber(value, volume) && volume >= 0.0f && volume <= 1.0f;
				if (valid)
					(argument == "--music-volume" ? options.MusicVolume : options.EffectsVolume) = volume;
				i++;
			}
			else if (argument == "--start-wave" && hasValue)
			{
				valid = ParseNumber(value, options.FirstWave) && options.FirstWave >= 1 && options.FirstWave <= 99;
				i++;
			}
			else if (argument == "--loot-rate" && hasValue)
			{
				float rate = 1.0f;
				valid = ParseNumber(value, rate) && rate >= 0.0f && rate <= 100.0f;
				if (valid)
					options.LootRate = rate;
				i++;
			}
			else if (argument == "--mute")
				options.Muted = true;
			else if (argument == "--peaceful")
				options.Peaceful = true;
			else if (argument == "--no-save")
				options.SaveProfile = false;
			else if (argument == "--new-character")
				options.NewCharacter = true;
			else if (argument == "--export-props")
				options.ExportProps = true;
			else if (argument == "--no-validation")
				options.Validation = false;
			else if (argument == "--screenshot" && hasValue)
			{
				options.ScreenshotPath = std::string(value);
				i++;
			}
			else if (argument == "--capture-audio" && hasValue)
			{
				options.AudioCapturePath = std::string(value);
				i++;
			}
			else if (argument == "--log-audio")
				options.LogAudio = true;
			else if (argument == "--frames" && hasValue)
			{
				valid = ParseNumber(value, options.ScreenshotFrames);
				i++;
			}
			else if (argument == "--burst" && hasValue)
			{
				valid = ParseNumber(value, options.ScreenshotBurst) && options.ScreenshotBurst > 0;
				i++;
			}
			else if (argument == "--burst-interval" && hasValue)
			{
				float milliseconds = 0.0f;
				valid = ParseNumber(value, milliseconds) && milliseconds >= 0.0f;
				options.BurstInterval = double(milliseconds) / 1000.0;
				i++;
			}
			else if (argument == "--delay" && hasValue)
			{
				valid = ParseNumber(value, options.ScreenshotDelay) && options.ScreenshotDelay >= 0.0f;
				i++;
			}
			else if (argument == "--debug-view" && hasValue)
			{
				const auto it = std::find_if(std::begin(DebugViewNames), std::end(DebugViewNames), [&](const char* name) { return SameText(name, value); });
				valid = it != std::end(DebugViewNames);
				if (valid)
					options.DebugView = uint8_t(it - std::begin(DebugViewNames));
				i++;
			}
			else if (argument == "--tonemap" && hasValue)
			{
				const auto it = std::find_if(std::begin(TonemapperNames), std::end(TonemapperNames), [&](const char* name) { return SameText(name, value); });
				valid = it != std::end(TonemapperNames);
				if (valid)
					options.Tonemap = uint8_t(it - std::begin(TonemapperNames));
				i++;
			}
			else if (argument == "--shadows" && hasValue)
			{
				const auto it = std::find_if(std::begin(ShadowQualityNames), std::end(ShadowQualityNames), [&](const char* name) { return SameText(name, value); });
				valid = it != std::end(ShadowQualityNames);
				if (valid)
					options.Shadows = uint8_t(it - std::begin(ShadowQualityNames));
				i++;
			}
			else if (argument == "--gi" && hasValue)
			{
				const auto it = std::find_if(std::begin(GiQualityNames), std::end(GiQualityNames), [&](const char* name) { return SameText(name, value); });
				valid = it != std::end(GiQualityNames);
				if (valid)
					options.Gi = uint8_t(it - std::begin(GiQualityNames));
				i++;
			}
			else if (argument == "--vsync" && hasValue)
			{
				valid = SameText("on", value) || SameText("off", value);
				if (valid)
					options.Vsync = SameText("on", value);
				i++;
			}
			else if (argument == "--frame-cap" && hasValue)
			{
				uint32_t cap = 0;
				valid = ParseNumber(value, cap) && cap >= MinFrameCap && cap <= MaxFrameCap;
				if (valid)
					options.FrameCap = cap;
				i++;
			}
			else if (argument == "--screen-shake" && hasValue)
			{
				float shake = 100.0f;
				valid = ParseNumber(value, shake) && shake >= 0.0f && shake <= 100.0f;
				if (valid)
					options.ScreenShake = shake / 100.0f;
				i++;
			}
			else if (argument == "--performance")
				options.ShowPerformance = true;
			else if (argument == "--exposure" && hasValue)
			{
				float exposure = 1.0f;
				valid = ParseNumber(value, exposure) && exposure > 0.0f;
				if (valid)
					options.Exposure = exposure;
				i++;
			}
			else if (argument == "--no-grading")
				options.Grading = false;
			else if (argument == "--lighting-test")
				options.LightingTest = true;
			else if (argument == "--pbr-test" && hasValue)
			{
				options.PbrTest = std::string(value);
				i++;
			}
			else if (argument == "--no-distortion")
				options.Distortion = false;
			else if (argument == "--no-trails")
				options.Trails = false;
			else if (argument == "--no-translucency")
				options.Translucency = false;
			else if (argument == "--distortion-stage" && hasValue)
			{
				valid = SameText("all", value) || SameText("shockwaves", value);
				if (valid)
					options.DistortionStage = SameText("all", value) ? 1 : 2;
				i++;
			}
			else if (argument == "--leviathan")
				options.Leviathan = true;
			else if (argument == "--profile-gpu")
				options.ProfileGpu = true;
			else if (argument == "--no-steam")
				options.Steam = false;
			else if (uint64_t lobby = 0; argument == "+connect_lobby" && hasValue && ParseNumber(value, lobby))
			{
				options.ConnectLobby = lobby;
				i++;
			}
			else
				valid = false;
			if (!valid)
				RDN_LOG_ERROR("Ignoring the invalid argument {} ({})", argument, usage);
		}
		if (options.Rules.Teams >= 2 && options.StartMode == GameMode::Brawl)
		{
			std::array<uint32_t, MaxTeams> members{};
			members[0] = options.Rules.Seats[0] == SpectatorSeat ? 0u : 1u;
			for (uint32_t bot = 0; bot < options.Rules.Bots; bot++)
			{
				uint8_t smallest = 0;
				for (uint8_t team = 1; team < options.Rules.Teams; team++)
					if (members[team] < members[smallest])
						smallest = team;
				members[smallest]++;
				options.Rules.TeamBots[smallest]++;
			}
		}
		return options;
	}
}
