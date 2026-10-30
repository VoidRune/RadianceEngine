#pragma once
#include "App/AppState.h"
#include "App/GraphicsTier.h"
#include "App/LaunchOptions.h"
#include "Rendering/Lighting.h"
#include "UI/PerformanceWindow.h"
#include "UI/UiTypes.h"
#include <GameShared/Content/Definitions.h>
#include <GameShared/Net/Protocol.h>
#include <GameShared/Save/CharacterProfile.h>
#include <RadianceEngine/Core/FrameLimiter.h>
#include <RadianceEngine/Core/Timer.h>
#include <RadianceEngine/Graphics/Device.h>
#include <RadianceEngine/Graphics/PresentQueue.h>
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <RadianceEngine/Window/Window.h>
#include <TimbreAudio/AudioEngine.h>
#include <memory>
#include <optional>
#include <vector>
#include <string>

namespace Game
{
	class GameAudio;
	class ImGuiLayer;
	class MenuBackdrop;
	class SteamPlatform;
	struct LobbyJoin;
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
		bool FriendsCanJoin = true;
		bool SaveProfiles = true;
		float Volume = 0.8f;
		float MusicVolume = 0.8f;
		float EffectsVolume = 1.0f;
		bool Muted = false;
		ShadowQuality Shadows = ShadowQuality::High;
		GiQuality Gi = GiQuality::Medium;
		bool Fullscreen = false;
		bool Vsync = true;
		uint32_t FrameCap = GraphicsPreset{}.FrameCap;
		float ScreenShake = 1.0f;
		bool ShowControls = true;
		bool ShowPerformance = false;
		MapKind Map = MapKind::Town;
		GameMode StartMode = GameMode::Hub;
		uint8_t Tier = 1;
		ModifierMask Modifiers = 0;
		MatchRules Rules;
		bool UnlimitedMaterials = false;
		bool Peaceful = false;
		bool Leviathan = false;
		bool Distortion = true;
		bool Trails = true;
		uint8_t DistortionStage = 0;
		uint32_t FirstWave = 1;
		float LootRate = 1.0f;
		NetworkSimulation Simulation;
	};

	enum class SettingsGroup : uint8_t
	{
		Audio,
		Visuals,
		Identity,
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
		MenuBackdrop& GetMenuBackdrop() { return *m_MenuBackdrop; }
		Timbre::AudioEngine& GetAudio() { return *m_Audio; }
		GameAudio& GetGameAudio() { return *m_GameAudio; }
		SteamPlatform& GetSteam() { return *m_Steam; }
		Settings& GetSettings() { return m_Settings; }
		void ApplyAudioSettings();
		void ApplyVisualSettings();
		void RememberSettings(SettingsGroup group);
		const Rdn::DeviceProperties& GetDeviceProperties() const { return m_Device->GetProperties(); }
		Rdn::PresentMode GetPresentMode() const { return m_PresentQueue->GetPresentMode(); }
		Rdn::Extent2D GetExtent() const { return m_PresentQueue->GetExtent(); }
		Rdn::MemoryUsage GetMemoryUsage() const { return m_Allocator->GetDeviceLocalUsage(); }
		GraphicsTier GetRecommendedGraphics() const { return m_RecommendedGraphics; }
		void ApplyRecommendedGraphics();
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

		void LoadPreferences();
		void SavePreferences();
		void AdoptGraphicsTier();
		void JoinFriend(const LobbyJoin& join);

		Settings m_Settings;
		Settings m_Stored;
		bool m_NamedInSettings = false;
		std::unique_ptr<SteamPlatform> m_Steam;
		bool m_PreferencesDirty = false;
		float m_PreferencesDelay = 0.0f;
		GraphicsTier m_RecommendedGraphics = GraphicsTier::High;
		std::string m_GraphicsGpu;
		std::optional<GraphicsTier> m_GraphicsTier;
		Rdn::Timer m_Clock;
		Rdn::FrameLimiter m_Limiter;
		FrameTiming m_FrameTiming;
		std::unique_ptr<PerformanceWindow> m_Performance;
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
		std::unique_ptr<MenuBackdrop> m_MenuBackdrop;
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
