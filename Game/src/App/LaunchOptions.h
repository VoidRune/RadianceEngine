#pragma once
#include <GameShared/Content/Cosmetics.h>
#include <GameShared/Content/GameMode.h>
#include <GameShared/Content/Pvp.h>
#include <GameShared/Content/Shardfall.h>
#include <GameShared/Net/Transport.h>
#include <GameShared/World/Level.h>
#include <array>
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
		std::optional<MapKind> Map;
		std::optional<float> Volume;
		std::optional<float> MusicVolume;
		std::optional<float> EffectsVolume;
		GameMode StartMode = GameMode::Hub;
		MatchRules Rules;
		RunRules Run;
		bool Muted = false;
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
		uint8_t MenuPage = 0;
		std::optional<uint8_t> Tonemap;
		std::optional<uint8_t> Shadows;
		std::optional<uint8_t> Gi;
		std::optional<bool> Vsync;
		std::optional<uint32_t> FrameCap;
		std::optional<uint32_t> RenderScale;
		std::optional<uint8_t> Aa;
		std::optional<float> ScreenShake;
		bool ShowPerformance = false;
		std::optional<float> Exposure;
		bool Grading = true;
		bool LightingTest = false;
		bool ProfileGpu = false;
		bool Leviathan = false;
		std::optional<Outfit> WornOutfit;
		std::optional<Focus> CastFocus;
		std::optional<Aura> WornAura;
		std::optional<Headpiece> WornHeadpiece;
		std::optional<std::array<float, 6>> ShotCamera;
		bool Distortion = true;
		bool Trails = true;
		bool Translucency = true;
		bool PointLights = true;
		bool Refraction = true;
		uint8_t DistortionStage = 0;
		bool Steam = true;
		std::optional<uint64_t> ConnectLobby;
	};

	LaunchOptions ParseLaunchOptions(int argc, char** argv);
}
