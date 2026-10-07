#pragma once
#include <GameShared/Content/Items.h>
#include <GameShared/Content/Missions.h>
#include <GameShared/Content/Pvp.h>
#include <GameShared/Net/Transport.h>
#include <GameShared/World/Level.h>
#include <cstdint>
#include <optional>
#include <string>

namespace Game
{
	struct LaunchOptions
	{
		std::string Name;
		std::optional<uint16_t> HostPort;
		std::optional<std::string> JoinAddress;
		std::optional<StarterKit> Kit;
		std::optional<MapKind> Map;
		std::optional<float> Volume;
		GameMode StartMode = GameMode::Hub;
		uint8_t Tier = 1;
		ModifierMask Modifiers = 0;
		MatchRules Rules;
		uint32_t FirstWave = 1;
		float LootRate = 1.0f;
		bool Muted = false;
		bool Peaceful = false;
		bool UnlimitedMaterials = false;
		bool SaveProfile = true;
		bool NewCharacter = false;
		bool ExportProps = false;
		NetworkSimulation Simulation;
		bool Validation = true;
		std::string ScreenshotPath;
		uint32_t ScreenshotFrames = 0;
		float ScreenshotDelay = 0.0f;
	};

	LaunchOptions ParseLaunchOptions(int argc, char** argv);
}
