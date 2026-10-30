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
		std::optional<float> MusicVolume;
		std::optional<float> EffectsVolume;
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
		std::string AudioCapturePath;
		bool LogAudio = false;
		uint32_t ScreenshotFrames = 0;
		uint32_t ScreenshotBurst = 1;
		double BurstInterval = 0.0;
		float ScreenshotDelay = 0.0f;
		uint8_t DebugView = 0;
		std::optional<uint8_t> Tonemap;
		std::optional<uint8_t> Shadows;
		std::optional<uint8_t> Gi;
		std::optional<bool> Vsync;
		std::optional<uint32_t> FrameCap;
		std::optional<float> ScreenShake;
		bool ShowPerformance = false;
		std::optional<float> Exposure;
		bool Grading = true;
		bool LightingTest = false;
		std::string PbrTest;
		bool ProfileGpu = false;
		bool Leviathan = false;
		bool Distortion = true;
		bool Trails = true;
		uint8_t DistortionStage = 0;
		bool Steam = true;
		std::optional<uint64_t> ConnectLobby;
	};

	LaunchOptions ParseLaunchOptions(int argc, char** argv);
}
