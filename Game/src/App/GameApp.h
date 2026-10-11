#pragma once
#include "App/AppState.h"
#include "App/LaunchOptions.h"
#include "Rendering/Lighting.h"
#include "UI/UiTypes.h"
#include <GameShared/Content/Definitions.h>
#include <GameShared/Net/Protocol.h>
#include <GameShared/Save/CharacterProfile.h>
#include <RadianceEngine/Graphics/Device.h>
#include <RadianceEngine/Graphics/PresentQueue.h>
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <RadianceEngine/Window/Window.h>
#include <TimbreAudio/AudioEngine.h>
#include <memory>
#include <vector>
#include <string>

namespace Game
{
	class GameAudio;
	class ImGuiLayer;
	class Ui;
	class UiAssets;
	class UiRenderer;
	class WorldRenderer;

	struct Settings
	{
		std::string PlayerName = "Player";
		std::string JoinAddress = "127.0.0.1";
		uint16_t Port = DefaultPort;
		StarterKit Kit = StarterKit::Arcanist;
		bool SaveProfiles = true;
		float Volume = 0.8f;
		float MusicVolume = 0.8f;
		float EffectsVolume = 1.0f;
		bool Muted = false;
		ShadowQuality Shadows = ShadowQuality::High;
		GiQuality Gi = GiQuality::Medium;
		MapKind Map = MapKind::Town;
		GameMode StartMode = GameMode::Hub;
		uint8_t Tier = 1;
		ModifierMask Modifiers = 0;
		MatchRules Rules;
		bool UnlimitedMaterials = false;
		bool Peaceful = false;
		uint32_t FirstWave = 1;
		float LootRate = 1.0f;
		NetworkSimulation Simulation;
	};

	class GameApp
	{
	public:
		explicit GameApp(const LaunchOptions& options);
		~GameApp();
		GameApp(const GameApp&) = delete;
		GameApp& operator=(const GameApp&) = delete;

		int Run();
		void ChangeState(std::unique_ptr<AppState> state);
		void Quit() { m_Running = false; }

		Rdn::Window& GetWindow() { return *m_Window; }
		Ui& GetUi() { return *m_Ui; }
		ImGuiLayer& GetDebugUi() { return *m_DebugUi; }
		bool UiWantsMouse() const;
		bool UiWantsKeyboard() const;
		uint32_t GetUiPrimitiveCount() const { return uint32_t(m_UiPrimitives.size()); }
		WorldRenderer& GetWorldRenderer() { return *m_WorldRenderer; }
		Timbre::AudioEngine& GetAudio() { return *m_Audio; }
		GameAudio& GetGameAudio() { return *m_GameAudio; }
		Settings& GetSettings() { return m_Settings; }
		void ApplyAudioSettings();
		CharacterProfile LoadCharacter(const std::string& name, StarterKit kit, bool fresh) const;
		void SaveCharacter(const CharacterProfile& profile) const;

	private:
		void Frame(float dt, bool minimized);
		void RenderFrame();
		void CaptureFrame(Rdn::RGImage backbuffer, Rdn::Extent2D extent);
		void SaveCapture();
		void CaptureAudio(float dt);
		void SaveAudioCapture() const;
		void ProfileGpu();

		Settings m_Settings;
		std::unique_ptr<Rdn::Window> m_Window;
		std::unique_ptr<Rdn::Device> m_Device;
		std::unique_ptr<Rdn::PresentQueue> m_PresentQueue;
		std::unique_ptr<Rdn::ResourceAllocator> m_Allocator;
		std::unique_ptr<Rdn::RenderGraph> m_Graph;
		std::unique_ptr<Timbre::AudioEngine> m_Audio;
		std::unique_ptr<GameAudio> m_GameAudio;
		std::unique_ptr<ImGuiLayer> m_DebugUi;
		std::unique_ptr<UiRenderer> m_UiRenderer;
		std::unique_ptr<UiAssets> m_UiAssets;
		std::unique_ptr<Ui> m_Ui;
		std::vector<UiPrimitive> m_UiPrimitives;
		std::unique_ptr<WorldRenderer> m_WorldRenderer;
		std::unique_ptr<AppState> m_State;
		std::unique_ptr<AppState> m_NextState;
		bool m_Running = true;
		std::string m_ScreenshotPath;
		uint32_t m_ScreenshotFrames = 0;
		uint32_t m_ScreenshotBurst = 1;
		double m_BurstInterval = 0.0;
		float m_ScreenshotDelay = 0.0f;
		uint32_t m_RenderedFrames = 0;
		double m_Uptime = 0.0;
		std::vector<std::unique_ptr<Rdn::GpuImage>> m_Captures;
		std::string m_AudioCapturePath;
		std::vector<float> m_AudioCapture;
		double m_AudioClock = 0.0;
		uint64_t m_AudioCaptured = 0;
		bool m_ProfileGpu = false;
		double m_ProfileStart = 0.0;
		uint32_t m_ProfileFrames = 0;
		std::vector<std::pair<std::string, double>> m_ProfileTotals;
	};
}
