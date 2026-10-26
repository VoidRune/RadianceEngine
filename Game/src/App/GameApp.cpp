#include "GameApp.h"
#include "Audio/GameAudio.h"
#include "Online/SteamPlatform.h"
#include "Rendering/MenuBackdrop.h"
#include "Rendering/ModelSources.h"
#include "Rendering/SpellVisuals.h"
#include "Rendering/WorldRenderer.h"
#include "States/LightingTestState.h"
#include "States/MainMenuState.h"
#include "States/SessionState.h"
#include "UI/ImGuiLayer.h"
#include "UI/Ui.h"
#include "UI/UiAssets.h"
#include "UI/UiRenderer.h"
#include <GameShared/Content/PropLibrary.h>
#include <GameShared/Net/Transport.h>
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Window/Input.h>
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <thread>

#pragma warning(push, 0)
#pragma warning(disable : 4996)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#pragma warning(pop)

namespace Game
{
	namespace
	{
		constexpr uint32_t AudioCaptureRate = 48000;
		constexpr const char* PreferencesPath = "Settings.ini";

		Rdn::PresentMode PresentModeFor(bool vsync)
		{
			return vsync ? Rdn::PresentMode::Fifo : Rdn::PresentMode::Mailbox;
		}

		template<typename T>
		bool ParseNumber(std::string_view text, T& value)
		{
			const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
			return error == std::errc() && end == text.data() + text.size();
		}

		template<typename T, size_t N>
		bool ParseName(std::string_view text, const char* const (&names)[N], T& value)
		{
			for (size_t i = 0; i < N; i++)
				if (text == names[i])
				{
					value = T(i);
					return true;
				}
			return false;
		}

		void ReadVolume(std::string_view text, float& value)
		{
			float parsed = 0.0f;
			if (ParseNumber(text, parsed))
				value = std::clamp(parsed, 0.0f, 1.0f);
		}

		std::string NameFromPersona(std::string_view persona)
		{
			constexpr size_t MaxName = 24;
			std::string name;
			for (const char c : persona)
				if (uint8_t(c) >= 32 && std::string_view("/\\:*?\"<>|").find(c) == std::string_view::npos)
					name += c;
			if (name.size() > MaxName)
			{
				size_t end = MaxName;
				while (end > 0 && (uint8_t(name[end]) & 0xC0u) == 0x80u)
					end--;
				name.resize(end);
			}
			return name;
		}
	}

	void GameApp::LoadPreferences()
	{
		std::ifstream file(PreferencesPath);
		std::string line;
		while (std::getline(file, line))
		{
			const size_t equals = line.find('=');
			if (equals == std::string::npos)
				continue;
			const std::string_view key = std::string_view(line).substr(0, equals);
			const std::string_view value = std::string_view(line).substr(equals + 1);
			if (key == "PlayerName" && !value.empty())
			{
				m_Settings.PlayerName = value;
				m_NamedInSettings = true;
			}
			else if (key == "JoinAddress")
				m_Settings.JoinAddress = value;
			else if (key == "FriendsCanJoin")
				m_Settings.FriendsCanJoin = value == "1";
			else if (uint16_t port = 0; key == "Port" && ParseNumber(value, port) && port >= 1024)
				m_Settings.Port = port;
			else if (key == "Kit")
			{
				for (const KitDef& kit : Kits)
					if (value == kit.Name)
						m_Settings.Kit = kit.Id;
			}
			else if (key == "Volume")
				ReadVolume(value, m_Settings.Volume);
			else if (key == "MusicVolume")
				ReadVolume(value, m_Settings.MusicVolume);
			else if (key == "EffectsVolume")
				ReadVolume(value, m_Settings.EffectsVolume);
			else if (key == "Muted")
				m_Settings.Muted = value == "1";
			else if (key == "Shadows")
				ParseName(value, ShadowQualityNames, m_Settings.Shadows);
			else if (key == "Gi")
				ParseName(value, GiQualityNames, m_Settings.Gi);
			else if (key == "Fullscreen")
				m_Settings.Fullscreen = value == "1";
			else if (key == "Vsync")
				m_Settings.Vsync = value == "1";
			else if (uint32_t cap = 0; key == "FrameCap" && ParseNumber(value, cap))
				m_Settings.FrameCap = std::clamp(cap, MinFrameCap, MaxFrameCap);
			else if (key == "ShowControls")
				m_Settings.ShowControls = value == "1";
			else if (key == "Gpu")
				m_GraphicsGpu = value;
			else if (GraphicsTier tier{}; key == "GraphicsTier" && ParseName(value, GraphicsTierNames, tier))
				m_GraphicsTier = tier;
		}
	}

	void GameApp::RememberSettings(SettingsGroup group)
	{
		switch (group)
		{
		case SettingsGroup::Audio:
			m_Stored.Volume = m_Settings.Volume;
			m_Stored.MusicVolume = m_Settings.MusicVolume;
			m_Stored.EffectsVolume = m_Settings.EffectsVolume;
			m_Stored.Muted = m_Settings.Muted;
			break;
		case SettingsGroup::Visuals:
			m_Stored.Shadows = m_Settings.Shadows;
			m_Stored.Gi = m_Settings.Gi;
			m_Stored.Fullscreen = m_Settings.Fullscreen;
			m_Stored.Vsync = m_Settings.Vsync;
			m_Stored.FrameCap = m_Settings.FrameCap;
			m_Stored.ShowControls = m_Settings.ShowControls;
			break;
		case SettingsGroup::Identity:
			m_Stored.PlayerName = m_Settings.PlayerName;
			m_Stored.JoinAddress = m_Settings.JoinAddress;
			m_Stored.Port = m_Settings.Port;
			m_Stored.Kit = m_Settings.Kit;
			m_Stored.FriendsCanJoin = m_Settings.FriendsCanJoin;
			break;
		}
		m_PreferencesDelay = 0.5f;
		m_PreferencesDirty = m_Settings.SaveProfiles;
	}

	void GameApp::SavePreferences()
	{
		m_PreferencesDirty = false;
		std::ofstream file(PreferencesPath);
		file << "PlayerName=" << m_Stored.PlayerName << "\nJoinAddress=" << m_Stored.JoinAddress << "\nPort=" << m_Stored.Port << "\nFriendsCanJoin=" << (m_Stored.FriendsCanJoin ? 1 : 0) << "\nKit=" << GetKit(m_Stored.Kit).Name
			<< "\nVolume=" << m_Stored.Volume << "\nMusicVolume=" << m_Stored.MusicVolume << "\nEffectsVolume=" << m_Stored.EffectsVolume << "\nMuted=" << (m_Stored.Muted ? 1 : 0)
			<< "\nShadows=" << ShadowQualityNames[size_t(m_Stored.Shadows)] << "\nGi=" << GiQualityNames[size_t(m_Stored.Gi)] << "\nFullscreen=" << (m_Stored.Fullscreen ? 1 : 0) << "\nVsync=" << (m_Stored.Vsync ? 1 : 0)
			<< "\nFrameCap=" << m_Stored.FrameCap << "\nShowControls=" << (m_Stored.ShowControls ? 1 : 0) << "\n";
		if (!m_GraphicsGpu.empty())
			file << "Gpu=" << m_GraphicsGpu << "\n";
		if (m_GraphicsTier)
			file << "GraphicsTier=" << GraphicsTierNames[size_t(*m_GraphicsTier)] << "\n";
		if (!file)
			RDN_LOG_ERROR("Can't save the settings to {}", PreferencesPath);
	}

	GameApp::GameApp(const LaunchOptions& options)
	{
		m_Settings.SaveProfiles = options.SaveProfile;
		if (m_Settings.SaveProfiles)
			LoadPreferences();
		m_Stored = m_Settings;
		if (!options.Name.empty())
			m_Settings.PlayerName = options.Name;
		m_Settings.Simulation = options.Simulation;
		if (options.JoinAddress)
			m_Settings.JoinAddress = *options.JoinAddress;
		if (options.HostPort)
			m_Settings.Port = *options.HostPort;
		if (options.Kit)
			m_Settings.Kit = *options.Kit;
		if (options.Map)
			m_Settings.Map = *options.Map;
		if (options.Volume)
			m_Settings.Volume = *options.Volume;
		if (options.MusicVolume)
			m_Settings.MusicVolume = *options.MusicVolume;
		if (options.EffectsVolume)
			m_Settings.EffectsVolume = *options.EffectsVolume;
		if (options.Muted)
			m_Settings.Muted = true;
		m_Settings.FirstWave = options.FirstWave;
		m_Settings.Peaceful = options.Peaceful;
		m_Settings.Leviathan = options.Leviathan;
		m_Settings.LootRate = options.LootRate;
		m_Settings.StartMode = options.StartMode;
		m_Settings.Tier = options.Tier;
		m_Settings.Modifiers = options.Modifiers;
		m_Settings.Rules = options.Rules;
		m_ScreenshotPath = options.ScreenshotPath;
		m_ScreenshotFrames = std::max(options.ScreenshotFrames, 1u);
		m_ScreenshotBurst = std::max(options.ScreenshotBurst, 1u);
		m_BurstInterval = options.BurstInterval;
		m_ScreenshotDelay = options.ScreenshotDelay;
		m_AudioCapturePath = options.AudioCapturePath;
		m_Settings.UnlimitedMaterials = options.UnlimitedMaterials;
		NetLibrary::Initialize();
		NetLibrary::SetSimulation(m_Settings.Simulation);
		m_Steam = std::make_unique<SteamPlatform>(options.Steam);
		m_Steam->EnableAchievements(m_Settings.SaveProfiles);
		if (!m_NamedInSettings && options.Name.empty() && m_Steam->IsReady())
			if (const std::string persona = NameFromPersona(m_Steam->GetPersonaName()); !persona.empty())
				m_Settings.PlayerName = persona;
		if (options.ConnectLobby)
			m_Steam->JoinLobby(*options.ConnectLobby);

		std::string error;
		if (options.ExportProps)
		{
			if (SavePropLibrary(PropLibraryPath, error))
				RDN_LOG("Wrote the built-in prop definitions to {}", PropLibraryPath);
			else
				RDN_LOG_ERROR("Can't export the props: {}", error);
		}
		if (std::filesystem::exists(PropLibraryPath))
		{
			if (LoadPropLibrary(PropLibraryPath, error))
				RDN_LOG("Loaded {} prop definitions from {}", CountCustomProps(), PropLibraryPath);
			else
				RDN_LOG_ERROR("Using the built-in props because {} failed to load: {}", PropLibraryPath, error);
		}

		Rdn::WindowDescription windowDesc;
		windowDesc.Title = "RadianceEngine Game";
		windowDesc.Width = 1600;
		windowDesc.Height = 900;
		windowDesc.Fullscreen = m_Settings.Fullscreen;
		m_Window = std::make_unique<Rdn::Window>(windowDesc);

		Rdn::DeviceConfig deviceConfig;
		deviceConfig.WindowHandle = m_Window->GetHandle();
		deviceConfig.InstanceExtensions = m_Window->GetInstanceExtensions();
		deviceConfig.FramesInFlight = 2;
		deviceConfig.EnableValidation = options.Validation;
		m_Device = std::make_unique<Rdn::Device>(deviceConfig);
		const Rdn::DeviceProperties& gpu = m_Device->GetProperties();
		m_RecommendedGraphics = RecommendGraphics(gpu);
		RDN_LOG("{} is {} with {} MB of device memory, recommended graphics tier {}", gpu.Name, gpu.Discrete ? "a discrete GPU" : gpu.Integrated ? "an integrated GPU" : "not a GPU",
			gpu.DeviceLocalMemory >> 20, GraphicsTierNames[size_t(m_RecommendedGraphics)]);
		if (m_Settings.SaveProfiles && m_GraphicsGpu != gpu.Name)
			AdoptGraphicsTier();
		if (options.Vsync)
			m_Settings.Vsync = *options.Vsync;
		if (options.FrameCap)
			m_Settings.FrameCap = *options.FrameCap;
		m_Settings.ShowPerformance = options.ShowPerformance;
		m_PresentQueue = std::make_unique<Rdn::PresentQueue>(m_Device.get(), PresentModeFor(m_Settings.Vsync));
		m_Allocator = std::make_unique<Rdn::ResourceAllocator>(m_Device.get());
		m_Graph = std::make_unique<Rdn::RenderGraph>(m_Device.get(), m_Allocator.get());
		m_ProfileGpu = options.ProfileGpu;
		m_Graph->SetTimingEnabled(m_ProfileGpu);

		Timbre::AudioEngineConfig audioConfig;
		if (!m_AudioCapturePath.empty())
		{
			audioConfig.NoOutput = true;
			audioConfig.SampleRate = AudioCaptureRate;
		}
		audioConfig.Log =[](Timbre::LogLevel level, std::string_view message)
		{
			switch (level)
			{
			case Timbre::LogLevel::Info:
				RDN_LOG("Audio: {}", message);
				break;
			case Timbre::LogLevel::Warning:
				RDN_LOG_WARNING("Audio: {}", message);
				break;
			case Timbre::LogLevel::Error:
				RDN_LOG_ERROR("Audio: {}", message);
				break;
			}
		};
		m_Audio = std::make_unique<Timbre::AudioEngine>(audioConfig);
		m_GameAudio = std::make_unique<GameAudio>(*m_Audio);
		m_GameAudio->SetTrace(options.LogAudio);
		ApplyAudioSettings();

		m_DebugUi = std::make_unique<ImGuiLayer>(*m_Device, *m_Allocator, m_PresentQueue->GetSurfaceFormat());
		m_UiRenderer = std::make_unique<UiRenderer>(*m_Device, *m_Allocator, m_PresentQueue->GetSurfaceFormat());
		m_UiAssets = std::make_unique<UiAssets>();
		if (std::string uiError; !m_UiAssets->Load(*m_UiRenderer, uiError))
		{
			RDN_LOG_FATAL("Can't load the UI: {}", uiError);
			std::exit(1);
		}
		m_Ui = std::make_unique<Ui>(*m_UiAssets, *m_Window);
		m_Performance = std::make_unique<PerformanceWindow>();
		m_WorldRenderer = std::make_unique<WorldRenderer>(*m_Device, *m_Allocator, m_PresentQueue->GetSurfaceFormat());
		if (std::string modelError; !m_WorldRenderer->LoadModels(ModelPacks, modelError))
		{
			RDN_LOG_FATAL("Can't load the models: {}", modelError);
			std::exit(1);
		}
		RegisterSpellMeshes(*m_WorldRenderer);
		m_MenuBackdrop = std::make_unique<MenuBackdrop>(*m_Device, *m_Allocator, m_PresentQueue->GetSurfaceFormat());
		m_WorldRenderer->SetDebugView(DebugView(options.DebugView));
		if (options.Shadows)
			m_Settings.Shadows = ShadowQuality(*options.Shadows);
		m_WorldRenderer->SetShadowQuality(m_Settings.Shadows);
		if (options.Gi)
			m_Settings.Gi = GiQuality(*options.Gi);
		m_WorldRenderer->SetGiQuality(m_Settings.Gi);
		PostProcess& post = m_WorldRenderer->GetPost();
		if (options.Tonemap)
			post.Tonemap = Tonemapper(*options.Tonemap);
		if (options.Exposure)
			post.Exposure = *options.Exposure;
		post.Grading = options.Grading;
		if (std::filesystem::exists(GradingLutPath))
		{
			if (std::string lutError; post.LoadGradingLut(GradingLutPath, lutError))
				RDN_LOG("Loaded the grading LUT {}", GradingLutPath);
			else
				RDN_LOG_ERROR("Using the built-in grade because {}", lutError);
		}

		if (options.LightingTest || !options.PbrTest.empty())
			m_State = std::make_unique<LightingTestState>(*this, options.PbrTest);
		else if (options.HostPort || options.JoinAddress)
		{
			SessionConfig config;
			config.Host = options.HostPort.has_value();
			config.Port = m_Settings.Port;
			config.Address = m_Settings.JoinAddress;
			config.Map = m_Settings.Map;
			config.StartMode = m_Settings.StartMode;
			config.Profile = LoadCharacter(m_Settings.PlayerName, m_Settings.Kit, options.NewCharacter);
			m_State = std::make_unique<SessionState>(*this, std::move(config));
		}
		else
			m_State = std::make_unique<MainMenuState>(*this);
	}

	CharacterProfile GameApp::LoadCharacter(const std::string& name, StarterKit kit, bool fresh) const
	{
		if (!fresh && m_Settings.SaveProfiles)
		{
			std::string error;
			if (std::optional<CharacterProfile> profile = LoadProfile(GetProfilePath(name), error))
			{
				profile->Name = name;
				return *profile;
			}
			if (std::filesystem::exists(GetProfilePath(name)))
				RDN_LOG_ERROR("Starting a new character because the save didn't load: {}", error);
		}
		return MakeNewProfile(name, kit);
	}

	void GameApp::JoinFriend(const LobbyJoin& join)
	{
		m_NextState.reset();
		m_State.reset();
		SessionConfig config;
		config.Host = false;
		config.RelayHost = join.Host;
		config.HostName = join.HostName;
		config.LobbyTicket = join.Ticket;
		config.Profile = LoadCharacter(m_Settings.PlayerName, m_Settings.Kit, false);
		m_GameAudio->PlayUi(Cue::UiRift, 0.9f);
		m_State = std::make_unique<SessionState>(*this, std::move(config));
	}

	void GameApp::SaveCharacter(const CharacterProfile& profile) const
	{
		if (!m_Settings.SaveProfiles)
			return;
		std::string error;
		if (!SaveProfile(GetProfilePath(profile.Name), profile, error))
			RDN_LOG_ERROR("Can't save the character: {}", error);
	}

	GameApp::~GameApp()
	{
		if (m_PreferencesDirty)
			SavePreferences();
		m_NextState.reset();
		m_State.reset();
		m_Steam.reset();
		m_Device->WaitIdle();
		m_MenuBackdrop.reset();
		m_WorldRenderer.reset();
		m_Ui.reset();
		m_UiAssets.reset();
		m_UiRenderer.reset();
		m_DebugUi.reset();
		m_Graph->ReleaseTransientResources();
		m_Allocator->FreeResources();
		m_GameAudio.reset();
		m_Audio.reset();
		NetLibrary::Shutdown();
	}

	void GameApp::ApplyAudioSettings()
	{
		m_Audio->GetMasterBus().SetVolume(m_Settings.Muted ? 0.0f : m_Settings.Volume);
		m_GameAudio->SetMix(m_Settings.MusicVolume, m_Settings.EffectsVolume);
	}

	void GameApp::ApplyVisualSettings()
	{
		m_WorldRenderer->SetShadowQuality(m_Settings.Shadows);
		m_WorldRenderer->SetGiQuality(m_Settings.Gi);
		m_Window->SetFullscreen(m_Settings.Fullscreen);
		m_PresentQueue->SetPresentMode(PresentModeFor(m_Settings.Vsync));
	}

	void GameApp::AdoptGraphicsTier()
	{
		const GraphicsPreset previous = m_GraphicsTier ? GraphicsPresets[size_t(*m_GraphicsTier)] : GraphicsPreset{};
		const GraphicsPreset& next = GraphicsPresets[size_t(m_RecommendedGraphics)];
		if (m_Settings.Shadows == previous.Shadows)
			m_Settings.Shadows = next.Shadows;
		if (m_Settings.Gi == previous.Gi)
			m_Settings.Gi = next.Gi;
		if (m_Settings.FrameCap == previous.FrameCap)
			m_Settings.FrameCap = next.FrameCap;
		m_Stored.Shadows = m_Settings.Shadows;
		m_Stored.Gi = m_Settings.Gi;
		m_Stored.FrameCap = m_Settings.FrameCap;
		m_GraphicsGpu = m_Device->GetProperties().Name;
		m_GraphicsTier = m_RecommendedGraphics;
		m_PreferencesDirty = true;
		m_PreferencesDelay = 0.0f;
		RDN_LOG("Graphics for this GPU: indirect light {}, shadows {}, frame cap {}", GiQualityNames[size_t(m_Settings.Gi)], ShadowQualityNames[size_t(m_Settings.Shadows)], m_Settings.FrameCap);
	}

	void GameApp::ApplyRecommendedGraphics()
	{
		const GraphicsPreset& preset = GraphicsPresets[size_t(m_RecommendedGraphics)];
		m_Settings.Shadows = preset.Shadows;
		m_Settings.Gi = preset.Gi;
		m_Settings.FrameCap = preset.FrameCap;
		ApplyVisualSettings();
		RememberSettings(SettingsGroup::Visuals);
	}

	void GameApp::ChangeState(std::unique_ptr<AppState> state)
	{
		m_NextState = std::move(state);
	}

	int GameApp::Run()
	{
		m_Clock.Reset();
		double previous = 0.0;
		while (m_Running && !m_Window->IsClosed())
		{
			m_Window->PollEvents();
			NetLibrary::RunCallbacks();
			const double now = m_Clock.ElapsedSeconds();
			m_Uptime = now;
			const double interval = now - previous;
			const float dt = float(std::min(interval, 0.25));
			previous = now;

			const bool minimized = m_Window->IsMinimized();
			Frame(dt, minimized);
			if (m_NextState)
				m_State = std::move(m_NextState);
			double slept = 0.0;
			if (minimized)
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			else if (!m_Settings.Vsync)
				slept = m_Limiter.Wait(double(m_Settings.FrameCap));
			if (m_Settings.ShowPerformance && !minimized)
			{
				m_FrameTiming.Interval = float(interval * 1000.0);
				m_FrameTiming.Sleep = float(slept * 1000.0);
				m_Performance->Record(m_FrameTiming, m_Graph->GetPassTimings());
			}
		}
		if (!m_AudioCapturePath.empty())
			SaveAudioCapture();
		return 0;
	}

	void GameApp::CaptureAudio(float dt)
	{
		m_AudioClock += dt;
		const uint64_t due = uint64_t(m_AudioClock * double(AudioCaptureRate));
		if (due < m_AudioCaptured + 512)
			return;
		const uint32_t frames = uint32_t(due - m_AudioCaptured);
		const size_t offset = m_AudioCapture.size();
		m_AudioCapture.resize(offset + size_t(frames) * 2);
		m_Audio->Render(m_AudioCapture.data() + offset, frames);
		m_AudioCaptured = due;
	}

	void GameApp::SaveAudioCapture() const
	{
		std::ofstream file(m_AudioCapturePath, std::ios::binary);
		const auto write = [&](const auto& value) { file.write(reinterpret_cast<const char*>(&value), sizeof(value)); };
		const uint32_t bytes = uint32_t(m_AudioCapture.size() * sizeof(float));
		file.write("RIFF", 4);
		write(uint32_t(36 + bytes));
		file.write("WAVEfmt ", 8);
		write(uint32_t(16));
		write(uint16_t(3));
		write(uint16_t(2));
		write(AudioCaptureRate);
		write(uint32_t(AudioCaptureRate * 2 * sizeof(float)));
		write(uint16_t(2 * sizeof(float)));
		write(uint16_t(32));
		file.write("data", 4);
		write(bytes);
		file.write(reinterpret_cast<const char*>(m_AudioCapture.data()), bytes);
		if (file)
			RDN_LOG("Audio capture saved to {} ({:.1f} s)", m_AudioCapturePath, double(m_AudioCapture.size() / 2) / double(AudioCaptureRate));
		else
			RDN_LOG_ERROR("Can't save the audio capture to {}", m_AudioCapturePath);
	}

	void GameApp::ProfileGpu()
	{
		const std::vector<Rdn::PassTiming>& timings = m_Graph->GetPassTimings();
		if (timings.empty())
			return;
		for (const Rdn::PassTiming& timing : timings)
		{
			const auto it = std::find_if(m_ProfileTotals.begin(), m_ProfileTotals.end(), [&](const auto& entry) { return entry.first == timing.Name; });
			if (it == m_ProfileTotals.end())
				m_ProfileTotals.emplace_back(timing.Name, timing.Milliseconds);
			else
				it->second += timing.Milliseconds;
		}
		m_ProfileFrames++;
		if (m_Uptime - m_ProfileStart < 2.0)
			return;
		std::string line;
		double total = 0.0;
		for (const auto& [name, sum] : m_ProfileTotals)
		{
			total += sum / m_ProfileFrames;
			line += std::format("{} {:.3f}, ", name, sum / m_ProfileFrames);
		}
		RDN_LOG("GPU ms over {} frames: {}total {:.3f}", m_ProfileFrames, line, total);
		m_ProfileTotals.clear();
		m_ProfileFrames = 0;
		m_ProfileStart = m_Uptime;
	}

	bool GameApp::UiWantsMouse() const
	{
		return m_Ui->WantsMouse() || m_DebugUi->WantsMouse();
	}

	bool GameApp::UiWantsKeyboard() const
	{
		return m_Ui->WantsKeyboard() || m_DebugUi->WantsKeyboard();
	}

	void GameApp::Frame(float dt, bool minimized)
	{
		const double start = m_Clock.ElapsedSeconds();
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F2) && !UiWantsKeyboard())
			m_Settings.ShowPerformance = !m_Settings.ShowPerformance;
		m_Graph->SetTimingEnabled(m_ProfileGpu || m_Settings.ShowPerformance);
		const Rdn::Extent2D extent = m_PresentQueue->GetExtent();
		m_DebugUi->BeginFrame(dt, extent);
		m_Ui->BeginFrame(dt, glm::vec2(float(extent.Width), float(extent.Height)), !m_DebugUi->WantsMouse());
		m_Steam->Update(dt);
		if (const std::optional<LobbyJoin> join = m_Steam->TakeJoin())
			JoinFriend(*join);
		m_State->Update(dt);
		m_State->DrawUi();
		if (m_Settings.ShowPerformance)
			m_Performance->Draw(*this, m_Settings.ShowPerformance);
		m_Ui->EndFrame();
		m_DebugUi->EndFrame();
		if (m_Ui->TakeClicks() > 0)
			m_GameAudio->PlayUi(Cue::UiClick);
		if (const int hover = m_Ui->TakeHover(); hover >= 0)
			m_GameAudio->PlayHover(hover);
		if (m_PreferencesDirty && (m_PreferencesDelay -= dt) <= 0.0f)
			SavePreferences();
		m_GameAudio->Update(dt);
		m_Audio->Update();
		if (!m_AudioCapturePath.empty())
			CaptureAudio(dt);
		m_FrameTiming.Update = float((m_Clock.ElapsedSeconds() - start) * 1000.0);
		if (!minimized)
			RenderFrame();
	}

	void GameApp::RenderFrame()
	{
		if (m_PresentQueue->NeedsRecreate() && !m_PresentQueue->Recreate())
			return;
		const double begin = m_Clock.ElapsedSeconds();
		std::optional<Rdn::FrameContext> frame = m_PresentQueue->BeginFrame();
		if (!frame)
			return;
		const double acquired = m_Clock.ElapsedSeconds();

		m_Graph->Reset();
		const Rdn::RGImage backbuffer = m_Graph->ImportBackbuffer(*frame);
		const bool drewScene = m_State->Render(*m_Graph, backbuffer, frame->PresentExtent, frame->FrameIndex);
		m_Ui->Collect(m_UiPrimitives);
		m_UiRenderer->Render(*m_Graph, backbuffer, frame->FrameIndex, m_UiPrimitives, m_Ui->GetSize(), m_Ui->GetScale(), m_Ui->GetTime(), !drewScene);
		m_DebugUi->Render(*m_Graph, backbuffer, frame->FrameIndex, false);
		const double due = m_ScreenshotDelay + double(m_Captures.size()) * m_BurstInterval;
		const bool capture = !m_ScreenshotPath.empty() && ++m_RenderedFrames >= m_ScreenshotFrames && m_Uptime >= due && m_Captures.size() < m_ScreenshotBurst;
		if (capture)
			CaptureFrame(backbuffer, frame->PresentExtent);
		m_Graph->Execute(frame->Cmd, frame->FrameIndex);
		const double recorded = m_Clock.ElapsedSeconds();
		m_PresentQueue->Submit();
		const double presented = m_Clock.ElapsedSeconds();
		m_FrameTiming.Render = float((recorded - acquired) * 1000.0);
		m_FrameTiming.Wait = float((acquired - begin + presented - recorded) * 1000.0);
		if (m_ProfileGpu)
			ProfileGpu();
		if (capture && m_Captures.size() == m_ScreenshotBurst)
			SaveCapture();
	}

	void GameApp::CaptureFrame(Rdn::RGImage backbuffer, Rdn::Extent2D extent)
	{
		auto& image = m_Captures.emplace_back(std::make_unique<Rdn::GpuImage>());
		m_Allocator->CreateGpuImage(image.get(), Rdn::GpuImageDesc{
			.ImageSize = { extent.Width, extent.Height, 1 },
			.Format = m_PresentQueue->GetSurfaceFormat(),
			.UsageFlags = Rdn::ImageUsage::TransferDst | Rdn::ImageUsage::TransferSrc,
		});
		const Rdn::RGImage target = m_Graph->ImportImage("Screenshot", image.get());
		m_Graph->AddPass("Screenshot", Rdn::PassType::Transfer,
			[=](Rdn::PassBuilder& builder)
			{
				builder.UseImage(backbuffer, Rdn::RGImageUsage::TransferSrc);
				builder.UseImage(target, Rdn::RGImageUsage::TransferDst);
				builder.NeverCull();
			},
			[=](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BlitImage(context.GetImage(backbuffer), context.GetImageExtent(backbuffer), context.GetImage(target), context.GetImageExtent(target), Rdn::Filter::Nearest);
			});
	}

	void GameApp::SaveCapture()
	{
		m_Device->WaitIdle();
		const std::filesystem::path base(m_ScreenshotPath);
		for (size_t index = 0; index < m_Captures.size(); index++)
		{
			Rdn::GpuImage& image = *m_Captures[index];
			std::vector<uint8_t> pixels = m_Allocator->GetImageData(&image);
			const Rdn::Extent3D size = image.GetImageSize();
			const Rdn::Format format = image.GetFormat();
			const bool swizzle = format == Rdn::Format::B8G8R8A8_Unorm || format == Rdn::Format::B8G8R8A8_Srgb;
			for (size_t i = 0; i + 3 < pixels.size(); i += 4)
			{
				if (swizzle)
					std::swap(pixels[i], pixels[i + 2]);
				pixels[i + 3] = 255;
			}
			const std::string path = m_Captures.size() == 1 ? m_ScreenshotPath
				: (base.parent_path() / (base.stem().string() + "_" + std::to_string(index) + base.extension().string())).string();
			if (!pixels.empty() && stbi_write_png(path.c_str(), int(size.Width), int(size.Height), 4, pixels.data(), int(size.Width) * 4))
				RDN_LOG("Screenshot saved to {} after {} frames", path, m_RenderedFrames - uint32_t(m_Captures.size() - 1 - index));
			else
				RDN_LOG_ERROR("Can't save the screenshot to {}", path);
			m_Allocator->ReleaseResource(&image);
		}
		m_Captures.clear();
		m_Running = false;
	}
}
