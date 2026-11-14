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

	}

	LaunchOptions ParseLaunchOptions(int argc, char** argv)
	{
		constexpr std::string_view usage = "usage: Game [--host [port]] [--join address[:port]] [--name name] [--new-character] [--no-save] [--export-props] [--shardfall] [--run-minute 0..12] [--run-level 1..30] [--spawns 0..300] [--xp 25..500] [--only quartz|jade|sapphire|ruby|amethyst|prismatic] [--immortal] [--brawl [rounds]] [--brawl-map caldera|lake|ruins|peak|random] [--brawl-mode damage|knockback] [--bots 0..11] [--bot-skill easy|normal|hard] [--teams 2..4] [--health 25..500] [--mana 25..500] [--damage 25..300] [--cast-speed 50..300] [--cooldown-speed 50..300] [--move-speed 50..200] [--mana-regen 0..300] [--knockback 25..300] [--random-spells] [--no-apex] [--apex-draft] [--no-crumble] [--no-winner-card] [--spectate] [--volume 0..1] [--music-volume 0..1] [--effects-volume 0..1] [--mute] [--lag ms] [--loss percent] [--no-validation] [--capture-audio file.wav] [--log-audio] [--debug-view lit|albedo|normal|roughness|metallic|occlusion|emission|sun|ambient|shadow|irradiance|gisource|translucency] [--tonemap softclip|neutral|agx|aces|linear] [--shadows off|low|high] [--gi off|low|medium|high] [--vsync on|off] [--frame-cap 30..240] [--render-scale 50..100] [--aa off|smaa|ssaa1.5|ssaa2] [--screen-shake 0..100] [--performance] [--exposure x] [--no-grading] [--lighting-test] [--profile-gpu] [--menu-page 0..5] [--leviathan] [--no-distortion] [--no-trails] [--no-translucency] [--no-point-lights] [--no-refraction] [--distortion-stage all|shockwaves] [--no-steam]";
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
			else if (argument == "--shardfall")
			{
				options.StartMode = GameMode::Shardfall;
				options.Map = MapKind::ProvingHollow;
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
			else if ((argument == "--run-minute" || argument == "--run-level") && hasValue)
			{
				uint32_t number = 0;
				const bool minute = argument == "--run-minute";
				valid = ParseNumber(value, number) && (minute ? number <= MaxStartMinute : number >= 1 && number <= MaxStartLevel);
				if (valid)
					(minute ? options.Run.StartMinute : options.Run.StartLevel) = uint8_t(number);
				i++;
			}
			else if ((argument == "--spawns" || argument == "--xp") && hasValue)
			{
				uint32_t percent = 0;
				const bool spawns = argument == "--spawns";
				valid = ParseNumber(value, percent) && (spawns ? percent <= MaxSpawnRate : percent >= MinXpRate && percent <= MaxXpRate);
				if (valid)
					(spawns ? options.Run.Spawns : options.Run.Xp) = uint16_t(percent);
				i++;
			}
			else if (argument == "--only" && hasValue)
			{
				constexpr std::string_view Kinds[] = { "quartz", "jade", "sapphire", "ruby", "amethyst", "prismatic" };
				const auto kind = std::find(std::begin(Kinds), std::end(Kinds), value);
				valid = kind != std::end(Kinds);
				if (valid)
					options.Run.Only = uint8_t(kind - std::begin(Kinds) + 1);
				i++;
			}
			else if (argument == "--immortal")
				options.Run.Immortal = true;
			else if (argument == "--mute")
				options.Muted = true;
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
			else if (argument == "--menu-page" && hasValue)
			{
				valid = ParseNumber(value, options.MenuPage) && options.MenuPage <= 5;
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
			else if (argument == "--render-scale" && hasValue)
			{
				uint32_t scale = 0;
				valid = ParseNumber(value, scale) && scale >= MinRenderScale && scale <= MaxRenderScale;
				if (valid)
					options.RenderScale = scale;
				i++;
			}
			else if (argument == "--aa" && hasValue)
			{
				const auto it = std::find_if(std::begin(AntiAliasingNames), std::end(AntiAliasingNames), [&](const char* name) { return SameText(name, value); });
				valid = it != std::end(AntiAliasingNames);
				if (valid)
					options.Aa = uint8_t(it - std::begin(AntiAliasingNames));
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
			else if (argument == "--no-distortion")
				options.Distortion = false;
			else if (argument == "--no-trails")
				options.Trails = false;
			else if (argument == "--no-translucency")
				options.Translucency = false;
			else if (argument == "--no-point-lights")
				options.PointLights = false;
			else if (argument == "--no-refraction")
				options.Refraction = false;
			else if (argument == "--distortion-stage" && hasValue)
			{
				valid = SameText("all", value) || SameText("shockwaves", value);
				if (valid)
					options.DistortionStage = SameText("all", value) ? 1 : 2;
				i++;
			}
			else if (argument == "--leviathan")
				options.Leviathan = true;
			else if (argument == "--outfit" && hasValue)
			{
				valid = false;
				for (const OutfitDef& def : Outfits)
					if (SameText(def.Key, value))
					{
						options.WornOutfit = def.Id;
						valid = true;
					}
				i++;
			}
			else if (argument == "--focus" && hasValue)
			{
				valid = false;
				for (const FocusDef& def : Foci)
					if (SameText(def.Key, value))
					{
						options.CastFocus = def.Id;
						valid = true;
					}
				i++;
			}
			else if (argument == "--aura" && hasValue)
			{
				valid = false;
				for (const AuraDef& def : Auras)
					if (SameText(def.Key, value))
					{
						options.WornAura = def.Id;
						valid = true;
					}
				i++;
			}
			else if (argument == "--headpiece" && hasValue)
			{
				valid = false;
				for (const HeadpieceDef& def : Headpieces)
					if (SameText(def.Key, value))
					{
						options.WornHeadpiece = def.Id;
						valid = true;
					}
				i++;
			}
			else if (argument == "--shot-camera" && hasValue)
			{
				std::array<float, 6> camera{};
				std::string_view rest = value;
				for (size_t k = 0; k < camera.size() && valid; k++)
				{
					const size_t comma = rest.find(',');
					valid = ParseNumber(rest.substr(0, comma), camera[k]) && (comma != std::string_view::npos) == (k + 1 < camera.size());
					rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 1);
				}
				options.ShotCamera = camera;
				i++;
			}
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
