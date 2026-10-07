#include "GameApp.h"
#include "Audio/GameAudio.h"
#include "Rendering/WorldRenderer.h"
#include "States/MainMenuState.h"
#include "States/SessionState.h"
#include "UI/ImGuiLayer.h"
#include "UI/Ui.h"
#include "UI/UiAssets.h"
#include "UI/UiRenderer.h"
#include <GameShared/Content/PropLibrary.h>
#include <GameShared/Net/Transport.h>
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Core/Timer.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <thread>

#pragma warning(push, 0)
#pragma warning(disable : 4996)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#pragma warning(pop)

namespace Game
{
	GameApp::GameApp(const LaunchOptions& options)
	{
		if (!options.Name.empty())
			m_Settings.PlayerName = options.Name;
		m_Settings.Simulation = options.Simulation;
		if (options.JoinAddress)
			m_Settings.JoinAddress = *options.JoinAddress;
		if (options.HostPort)
			m_Settings.Port = *options.HostPort;
		if (options.Kit)
			m_Settings.Kit = *options.Kit;
		m_Settings.SaveProfiles = options.SaveProfile;
		if (options.Map)
			m_Settings.Map = *options.Map;
		if (options.Volume)
			m_Settings.Volume = *options.Volume;
		m_Settings.Muted = options.Muted;
		m_Settings.FirstWave = options.FirstWave;
		m_Settings.Peaceful = options.Peaceful;
		m_Settings.LootRate = options.LootRate;
		m_Settings.StartMode = options.StartMode;
		m_Settings.Tier = options.Tier;
		m_Settings.Modifiers = options.Modifiers;
		m_Settings.Rules = options.Rules;
		m_ScreenshotPath = options.ScreenshotPath;
		m_ScreenshotFrames = std::max(options.ScreenshotFrames, 1u);
		m_ScreenshotDelay = options.ScreenshotDelay;
		m_Settings.UnlimitedMaterials = options.UnlimitedMaterials;
		NetLibrary::Initialize();
		NetLibrary::SetSimulation(m_Settings.Simulation);

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
		m_Window = std::make_unique<Rdn::Window>(windowDesc);

		Rdn::DeviceConfig deviceConfig;
		deviceConfig.WindowHandle = m_Window->GetHandle();
		deviceConfig.InstanceExtensions = m_Window->GetInstanceExtensions();
		deviceConfig.FramesInFlight = 2;
		deviceConfig.EnableValidation = options.Validation;
		m_Device = std::make_unique<Rdn::Device>(deviceConfig);
		m_PresentQueue = std::make_unique<Rdn::PresentQueue>(m_Device.get(), Rdn::PresentMode::Mailbox);
		m_Allocator = std::make_unique<Rdn::ResourceAllocator>(m_Device.get());
		m_Graph = std::make_unique<Rdn::RenderGraph>(m_Device.get(), m_Allocator.get());

		Timbre::AudioEngineConfig audioConfig;
		audioConfig.Log = [](Timbre::LogLevel level, std::string_view message)
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
		m_WorldRenderer = std::make_unique<WorldRenderer>(*m_Device, *m_Allocator, m_PresentQueue->GetSurfaceFormat());

		if (options.HostPort || options.JoinAddress)
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
		m_NextState.reset();
		m_State.reset();
		m_Device->WaitIdle();
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
	}

	void GameApp::ChangeState(std::unique_ptr<AppState> state)
	{
		m_NextState = std::move(state);
	}

	int GameApp::Run()
	{
		Rdn::Timer timer;
		double previous = timer.ElapsedSeconds();
		while (m_Running && !m_Window->IsClosed())
		{
			m_Window->PollEvents();
			NetLibrary::RunCallbacks();
			const double now = timer.ElapsedSeconds();
			m_Uptime = now;
			const float dt = float(std::min(now - previous, 0.25));
			previous = now;

			const bool minimized = m_Window->IsMinimized();
			Frame(dt, minimized);
			if (m_NextState)
				m_State = std::move(m_NextState);
			if (minimized)
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		return 0;
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
		const Rdn::Extent2D extent = m_PresentQueue->GetExtent();
		m_DebugUi->BeginFrame(dt, extent);
		m_Ui->BeginFrame(dt, glm::vec2(float(extent.Width), float(extent.Height)), !m_DebugUi->WantsMouse());
		m_State->Update(dt);
		m_State->DrawUi();
		m_Ui->EndFrame();
		m_DebugUi->EndFrame();
		m_Audio->Update();
		if (!minimized)
			RenderFrame();
	}

	void GameApp::RenderFrame()
	{
		if (m_PresentQueue->NeedsRecreate() && !m_PresentQueue->Recreate())
			return;
		std::optional<Rdn::FrameContext> frame = m_PresentQueue->BeginFrame();
		if (!frame)
			return;

		m_Graph->Reset();
		const Rdn::RGImage backbuffer = m_Graph->ImportBackbuffer(*frame);
		const bool drewScene = m_State->Render(*m_Graph, backbuffer, frame->PresentExtent, frame->FrameIndex);
		m_Ui->Collect(m_UiPrimitives);
		m_UiRenderer->Render(*m_Graph, backbuffer, frame->FrameIndex, m_UiPrimitives, m_Ui->GetSize(), m_Ui->GetScale(), m_Ui->GetTime(), !drewScene);
		m_DebugUi->Render(*m_Graph, backbuffer, frame->FrameIndex, false);
		const bool capture = !m_ScreenshotPath.empty() && ++m_RenderedFrames >= m_ScreenshotFrames && m_Uptime >= m_ScreenshotDelay && !m_Captured;
		m_Captured = m_Captured || capture;
		if (capture)
			CaptureFrame(backbuffer, frame->PresentExtent);
		m_Graph->Execute(frame->Cmd, frame->FrameIndex);
		m_PresentQueue->Submit();
		if (capture)
			SaveCapture();
	}

	void GameApp::CaptureFrame(Rdn::RGImage backbuffer, Rdn::Extent2D extent)
	{
		m_Capture = std::make_unique<Rdn::GpuImage>();
		m_Allocator->CreateGpuImage(m_Capture.get(), Rdn::GpuImageDesc{
			.ImageSize = { extent.Width, extent.Height, 1 },
			.Format = m_PresentQueue->GetSurfaceFormat(),
			.UsageFlags = Rdn::ImageUsage::TransferDst | Rdn::ImageUsage::TransferSrc,
		});
		const Rdn::RGImage target = m_Graph->ImportImage("Screenshot", m_Capture.get());
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
		std::vector<uint8_t> pixels = m_Allocator->GetImageData(m_Capture.get());
		const Rdn::Extent3D size = m_Capture->GetImageSize();
		const Rdn::Format format = m_Capture->GetFormat();
		const bool swizzle = format == Rdn::Format::B8G8R8A8_Unorm || format == Rdn::Format::B8G8R8A8_Srgb;
		for (size_t i = 0; i + 3 < pixels.size(); i += 4)
		{
			if (swizzle)
				std::swap(pixels[i], pixels[i + 2]);
			pixels[i + 3] = 255;
		}
		if (!pixels.empty() && stbi_write_png(m_ScreenshotPath.c_str(), int(size.Width), int(size.Height), 4, pixels.data(), int(size.Width) * 4))
			RDN_LOG("Screenshot saved to {} after {} frames", m_ScreenshotPath, m_RenderedFrames);
		else
			RDN_LOG_ERROR("Can't save the screenshot to {}", m_ScreenshotPath);
		m_Allocator->ReleaseResource(m_Capture.get());
		m_Capture.reset();
		m_Running = false;
	}
}
