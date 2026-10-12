#include "SessionState.h"
#include "App/GameApp.h"
#include "Audio/GameAudio.h"
#include "Rendering/MenuBackdrop.h"
#include "Rendering/Models.h"
#include "Rendering/SpellVisuals.h"
#include "Rendering/WorldRenderer.h"
#include "States/MainMenuState.h"
#include "UI/ItemUi.h"
#include "UI/Palette.h"
#include "UI/SettingsUi.h"
#include "UI/Ui.h"
#include <GameShared/Content/Definitions.h>
#include <GameShared/World/Collision.h>
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Window/Input.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <utility>

namespace Game
{
	namespace
	{
		constexpr float HurtFlashSeconds = 0.15f;
		constexpr float DamageNumberSeconds = 0.9f;
		constexpr size_t MaxDamageNumbers = 160;
		constexpr float NoticeSeconds = 3.5f;
		constexpr size_t MaxNotices = 4;
		constexpr float SaveDelay = 1.5f;
		constexpr const char* SlotKeys[] = { "LMB", "RMB", "Q", "R" };
		static_assert(std::size(SlotKeys) == AbilitySlots);
		constexpr Rdn::KeyCode UpgradeKeys[] = { Rdn::KeyCode::Key1, Rdn::KeyCode::Key2, Rdn::KeyCode::Key3 };
		static_assert(std::size(UpgradeKeys) == UpgradeChoices);
		constexpr glm::vec3 EliteGold{ 1.0f, 0.78f, 0.25f };
		constexpr glm::vec3 EnemyFire{ 1.0f, 0.35f, 0.15f };
		constexpr float PortalHoverRadius = 1.9f;
		constexpr float ProjectileSink = 3.0f;
		constexpr float CameraNear = 0.5f;
		constexpr float CorpseSeconds = 3.0f;
		constexpr float CorpseSinkSeconds = 1.0f;
		constexpr size_t MaxCorpses = 24;
		constexpr float KillWindow = 0.5f;
		constexpr int PackPayload = 1;
		constexpr int GearPayload = 2;
		constexpr int GemPayload = 3;

		float HealthBarHeight(EntityKind kind)
		{
			switch (kind)
			{
			case EntityKind::Chaser:
				return 1.05f;
			case EntityKind::Shooter:
				return 1.45f;
			case EntityKind::Brute:
				return 1.75f;
			case EntityKind::Caster:
				return 1.75f;
			default:
				return 2.0f;
			}
		}

		bool IsItemLoot(const EntityState& entity)
		{
			return entity.Kind == EntityKind::Loot && LootKind(entity.Variant) == LootKind::Item;
		}

		bool HoldsItem(const ServerInventory& inventory, uint32_t id)
		{
			const auto has = [id](const Item& item) { return item.Id == id; };
			return std::any_of(inventory.Gear.begin(), inventory.Gear.end(), has) || std::any_of(inventory.Gems.begin(), inventory.Gems.end(), has) || std::any_of(inventory.Pack.begin(), inventory.Pack.end(), has);
		}

		bool IsBackpackFull(const ServerInventory& inventory)
		{
			return std::all_of(inventory.Pack.begin(), inventory.Pack.end(), [](const Item& item) { return item.IsValid(); });
		}

		SceneLighting MapLighting(MapKind map, const WorldRenderer& renderer)
		{
			SceneLighting lighting;
			if (map == MapKind::Caves)
			{
				lighting.SunColor = { 0.55f, 0.66f, 0.95f };
				lighting.SunIntensity = 0.3f;
				lighting.SkyColor = { 0.035f, 0.042f, 0.06f };
				lighting.GroundColor = { 0.025f, 0.021f, 0.018f };
				lighting.EmissionGi = 6.0f;
			}
			else if (map == MapKind::Caldera)
			{
				lighting.SunDirection = { -0.35f, -1.0f, 0.55f };
				lighting.SunColor = { 1.0f, 0.62f, 0.45f };
				lighting.SunIntensity = 0.7f;
				lighting.SkyColor = { 0.07f, 0.035f, 0.045f };
				lighting.GroundColor = { 0.09f, 0.03f, 0.015f };
				lighting.EmissionGi = 4.5f;
			}
			else if (map == MapKind::Abbey)
			{
				constexpr float SunAzimuth = -0.65f;
				constexpr float SunElevation = 0.42f;
				if (const std::optional<int32_t> environment = renderer.FindEnvironment("kiara_8_sunset"))
				{
					const ModelEnvironment& source = renderer.GetModels().GetData().Environments[size_t(*environment)];
					const glm::vec3 toSun = -source.SunDirection;
					lighting.Environment = *environment;
					lighting.EnvironmentRotation = SunAzimuth - std::atan2(toSun.x, toSun.z);
					lighting.EnvironmentIntensity = 1.4f;
					const float luminance = glm::dot(source.SunIrradiance, glm::vec3(0.2126f, 0.7152f, 0.0722f));
					if (luminance > 0.0f)
						lighting.SunColor = glm::mix(source.SunIrradiance / luminance, glm::vec3(1.0f), 0.45f);
				}
				lighting.SunDirection = -glm::vec3(std::cos(SunElevation) * std::sin(SunAzimuth), std::sin(SunElevation), std::cos(SunElevation) * std::cos(SunAzimuth));
				lighting.SunIntensity = 5.5f;
				lighting.EmissionGi = 4.0f;
			}
			return lighting;
		}
	}

	SessionState::SessionState(GameApp& app, SessionConfig config)
		: m_App(app), m_Config(std::move(config)), m_Extent{ app.GetWindow().Width(), app.GetWindow().Height() }
	{
		m_Characters = std::make_unique<CharacterLibrary>(app.GetWorldRenderer().GetModels());
		m_Effects = std::make_unique<EffectSystem>(app.GetWorldRenderer().GetParticles());
		m_MeteorMesh = app.GetWorldRenderer().GetModels().FindMesh("Rock_3_F_Color1");
		NetLibrary::SetSimulation(app.GetSettings().Simulation);
		app.GetGameAudio().Reset();
		if (m_Config.Host)
		{
			const Settings& settings = app.GetSettings();
			m_Server = std::make_unique<GameServer>(ServerConfig{
				.Port = m_Config.Port,
				.StartMode = m_Config.StartMode,
				.Map = m_Config.Map,
				.Tier = settings.Tier,
				.Modifiers = settings.Modifiers,
				.Rules = settings.Rules,
				.IntermissionSeconds = settings.Peaceful ? 1e9f : 6.0f,
				.FirstWave = settings.FirstWave,
				.LootRate = settings.LootRate,
				.UnlimitedMaterials = settings.UnlimitedMaterials,
			});
			if (!m_Server->Start())
			{
				m_Server.reset();
				m_Error = "Can't host on UDP port " + std::to_string(m_Config.Port) + ", it may be in use";
				return;
			}
			m_Client.ConnectLocal(m_Server->OpenLocalConnection(), m_Config.Profile);
		}
		else if (!m_Client.Connect(m_Config.Address, m_Config.Profile))
			m_Error = m_Client.GetDisconnectReason();
	}

	SessionState::~SessionState()
	{
		SaveProgress(true);
		m_Client.Disconnect();
		m_Server.reset();
	}

	void SessionState::ReturnToMenu(std::string message)
	{
		if (m_Leaving)
			return;
		m_Leaving = true;
		SaveProgress(true);
		m_App.ChangeState(std::make_unique<MainMenuState>(m_App, std::move(message)));
	}

	bool SessionState::IsProfileDirty() const
	{
		return m_Client.GetInventoryVersion() != m_SavedInventoryVersion || m_Client.GetStashVersion() != m_SavedStashVersion || m_Client.GetMaterials() != m_SavedMaterials;
	}

	void SessionState::SaveProgress(bool force)
	{
		if (!m_Client.HasInventory() || !IsProfileDirty() || (!force && m_SaveTimer > 0.0f))
			return;
		m_App.SaveCharacter(m_Client.GetProfile());
		m_SavedInventoryVersion = m_Client.GetInventoryVersion();
		m_SavedStashVersion = m_Client.GetStashVersion();
		m_SavedMaterials = m_Client.GetMaterials();
		m_SaveTimer = -1.0f;
	}

	void SessionState::OnTravel()
	{
		m_SeenTravels = m_Client.GetTravelCount();
		m_Station.reset();
		m_ShowInventory = false;
		m_Fx.clear();
		m_Corpses.clear();
		m_Effects->Clear();
		m_App.GetWorldRenderer().GetParticles().Clear();
		m_DamageNumbers.clear();
		m_ProjectileHeights.clear();
		m_LevelRenderer = LevelRenderer{};
		m_CameraPlaced = false;
		m_HoveredPortal = -1;
		m_ReachablePortal = -1;
		m_ChosenUpgrade.reset();
		const SessionInfo& session = m_Client.GetSession();
		if (m_SeenMode != GameMode::Count)
		{
			m_Fade = 1.0f;
			m_App.GetGameAudio().Reset();
			m_App.GetGameAudio().PlayEvent(AudioEvent::Travel);
		}
		m_SeenMode = session.Mode;
		m_SeenMission = MissionPhase::Explore;
		m_SeenMatch = MatchPhase::Countdown;
		m_SeenCountdown = -1;
		m_SeenStormStage = 0;
		m_SeenStormShrinking = false;
		m_HasPlan = false;
		m_RestoreAge = 1e9f;
		m_Risen.clear();
		m_Fallen.clear();
		m_SeenBrawlPhase = BrawlPhase::Draft;
		m_SeenRound = 0;
		m_FoughtRound = 0;
		m_LoadoutOpen = true;
		m_PickingSlot.reset();
		m_ChosenBoon.reset();
		m_Feed.clear();
		m_Client.TakeKills();
		m_BannerTitle = GetMapDef(session.Map).Name;
		m_BannerColor = WithAlpha(EliteGold);
		if (session.Mode == GameMode::Hub)
			m_BannerText = "The stash, the forge, the lapidary, the mission board and the proving grounds await";
		else if (session.Mode == GameMode::Horde)
			m_BannerText = "Endless horde. Survive as long as you can";
		else if (session.Mode == GameMode::Arena)
			m_BannerText = (session.Rules.Teams ? "Team arena, first team to " : "Free for all, first to ") + std::to_string(session.Rules.ScoreGoal) + " kills wins";
		else if (session.Mode == GameMode::Royale)
			m_BannerText = "Battle royale. Loot fast, outrun the ash storm, be the last one standing";
		else if (session.Mode == GameMode::Brawl)
			m_BannerText = "Spellbrawl, " + std::to_string(session.Rules.Rounds) + " rounds. Pick three spells, then shove everyone into the lava";
		else
		{
			const EnemyDef* boss = FindEnemyDef(session.Map == MapKind::Caves ? EntityKind::Wyrm : EntityKind::Archivist);
			m_BannerText = "Tier " + std::to_string(session.Tier) + " mission. " + (boss ? std::string(boss->Name) + " waits within" : std::string());
		}
		m_BannerTime = 4.0f;
	}

	void SessionState::OpenStation(StationKind kind)
	{
		if (m_Station == kind)
		{
			m_Station.reset();
			return;
		}
		m_Station = kind;
		m_ShowInventory = false;
		m_SourceTab = 0;
		m_App.GetGameAudio().PlayStation(kind);
	}

	void SessionState::UpdateInteractions()
	{
		m_NearStation = -1;
		m_NearChest = 0;
		m_NearGateway = 0;
		const Level* level = m_Client.GetLevel();
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		const bool alive = self && !self->Has(EntityFlags::Dead);
		if (!level || !alive)
		{
			m_Station.reset();
			return;
		}
		float nearest = 1e9f;
		if (m_Client.GetSession().Mode == GameMode::Hub)
			for (size_t i = 0; i < level->Stations.size(); i++)
			{
				const float distance = Length(level->Stations[i].Position - self->Position);
				if (distance <= level->Stations[i].Radius && distance < nearest)
				{
					nearest = distance;
					m_NearStation = int32_t(i);
				}
			}
		float chest = InteractRange;
		for (const EntityState& entity : m_Client.GetEntities())
		{
			const float distance = Length(entity.Position - self->Position);
			if (entity.Kind == EntityKind::Chest && entity.Variant == 0 && distance <= chest)
			{
				chest = distance;
				m_NearChest = entity.NetId;
			}
			else if (entity.Kind == EntityKind::Gateway && distance <= Gateway{}.Radius + InteractRange * 0.5f - 0.1f)
				m_NearGateway = entity.NetId;
		}
		if (m_Station)
		{
			const Station* station = level->FindStation(*m_Station);
			if (m_Client.GetSession().Mode != GameMode::Hub || !station || Length(station->Position - self->Position) > station->Radius + 2.0f)
				m_Station.reset();
		}
	}

	bool SessionState::Interact()
	{
		if (m_NearChest)
		{
			m_Client.OpenChest();
			return true;
		}
		if (m_NearGateway)
		{
			m_Client.UseGateway();
			return true;
		}
		const Level* level = m_Client.GetLevel();
		if (m_NearStation >= 0 && level && size_t(m_NearStation) < level->Stations.size())
		{
			OpenStation(level->Stations[size_t(m_NearStation)].Kind);
			return true;
		}
		return false;
	}

	void SessionState::Update(float dt)
	{
		if (!m_Error.empty())
			return ReturnToMenu(m_Error);
		if (m_Leaving)
			return;
		m_Time += dt;
		if (dt > 0.0f)
			m_Fps = m_Fps <= 0.0f ? 1.0f / dt : glm::mix(m_Fps, 1.0f / dt, 0.05f);
		if (m_Client.GetState() == ClientState::InGame)
			HandleHotkeys();

		LatchButtonPresses();
		UpdateAim();
		if (m_Server)
			m_Client.SetTickPhase(m_Server->GetTickPhase());
		m_Client.Predict(dt, [this] { return SampleInput(); });
		if (m_Server)
			m_Server->Update(dt);
		m_Client.Receive(dt);
		if (m_Client.GetState() == ClientState::Disconnected)
			return ReturnToMenu(m_Client.GetDisconnectReason().empty() ? "Disconnected from the server" : m_Client.GetDisconnectReason());
		if (m_Client.GetState() != ClientState::InGame)
			return;
		if (m_Client.GetTravelCount() != m_SeenTravels || m_SeenMode == GameMode::Count)
			OnTravel();

		if (m_Client.HasInventory() && IsProfileDirty())
		{
			if (m_SaveTimer < 0.0f)
				m_SaveTimer = SaveDelay;
			else if ((m_SaveTimer -= dt) <= 0.0f)
				SaveProgress(true);
		}
		m_BannerTime = std::max(m_BannerTime - dt, 0.0f);
		UpdateCamera(dt);
		m_AimPoint = MouseAimPoint();
		UpdateInteractions();
		UpdatePortals(dt);
		UpdateArena(dt);
		m_App.GetWorldRenderer().GetParticles().Advance(dt);
		UpdateEffects(dt);
		UpdateFeedback(dt);
		UpdateMatchFeedback(dt);
		GameAudio& audio = m_App.GetGameAudio();
		audio.Observe(m_Client, dt, m_CameraTarget, m_Camera.Eye);
		audio.SetOverlay(m_ShowInventory, m_Station.has_value(), m_Paused);
	}

	float SessionState::GroundHeight(Vec2 position) const
	{
		const Level* level = m_Client.GetLevel();
		if (!level)
			return 0.0f;
		if (m_Client.GetSession().Mode == GameMode::Brawl)
			return BrawlGroundHeight(*level, m_Client.GetStatus().Brawl, position);
		return level->Map.GetHeight(position);
	}

	void SessionState::UsePortal(int32_t portal)
	{
		const Level* level = m_Client.GetLevel();
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		if (!level || !self || portal < 0 || size_t(portal) >= level->Portals.size())
			return;
		if (!IsInPortalRange(*level, self->Position, uint32_t(portal)))
		{
			m_Hint = "Get closer to use the door to the " + level->Portals[size_t(portal)].Label;
			m_HintTime = 2.0f;
			return;
		}
		m_PendingInteract = uint8_t(portal + 1);
	}

	void SessionState::UpdatePortals(float dt)
	{
		m_HintTime = std::max(m_HintTime - dt, 0.0f);
		m_Fade = std::max(m_Fade - dt / 0.35f, 0.0f);
		if (m_Client.GetTeleportCount() != m_SeenTeleports)
		{
			m_SeenTeleports = m_Client.GetTeleportCount();
			m_Fade = 1.0f;
			m_App.GetGameAudio().PlayEvent(AudioEvent::Teleport);
		}

		m_HoveredPortal = -1;
		m_ReachablePortal = -1;
		const Level* level = m_Client.GetLevel();
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		if (!level || !self || m_Overview)
			return;
		float nearest = 1e9f;
		const bool mouseFree = !m_Paused && !m_App.UiWantsMouse();
		for (size_t i = 0; i < level->Portals.size(); i++)
		{
			const Portal& portal = level->Portals[i];
			const float distance = LengthSquared(portal.Position - self->Position);
			if (IsInPortalRange(*level, self->Position, uint32_t(i)) && distance < nearest)
			{
				nearest = distance;
				m_ReachablePortal = int32_t(i);
			}
			if (!mouseFree || distance > 400.0f)
				continue;
			const std::optional<glm::vec3> point = MouseOnPlane(level->Map.GetHeight(portal.Position) + 1.0f);
			if (point && LengthSquared(Vec2(point->x, point->z) - portal.Position) < PortalHoverRadius * PortalHoverRadius)
				m_HoveredPortal = int32_t(i);
		}
	}

	void SessionState::HandleHotkeys()
	{
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::Escape) && !m_App.UiWantsKeyboard())
		{
			const BrawlPhase brawlPhase = m_Client.GetStatus().Brawl.Phase;
			const bool choosing = m_Client.GetSession().Mode == GameMode::Brawl && m_Client.GetStatus().Match != MatchPhase::Ended && (brawlPhase == BrawlPhase::Draft || brawlPhase == BrawlPhase::Shop);
			if (m_Paused && m_PauseSettings)
				m_PauseSettings = false;
			else if (m_Station && !m_Paused)
				m_Station.reset();
			else if (m_ShowInventory && !m_Paused)
				m_ShowInventory = false;
			else if (choosing && m_LoadoutOpen && !m_Paused)
				m_LoadoutOpen = false;
			else
			{
				m_Paused = !m_Paused;
				m_PauseSettings = false;
				m_PauseTime = 0.0f;
			}
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F3))
			m_ShowNetStats = !m_ShowNetStats;
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F4))
			m_Overview = !m_Overview;
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F6))
		{
			WorldRenderer& renderer = m_App.GetWorldRenderer();
			renderer.SetDebugView(DebugView((uint32_t(renderer.GetDebugView()) + 1) % uint32_t(DebugView::Count)));
			RDN_LOG("Debug view: {}", DebugViewNames[size_t(renderer.GetDebugView())]);
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F7))
		{
			PostProcess& post = m_App.GetWorldRenderer().GetPost();
			post.Tonemap = Tonemapper((uint32_t(post.Tonemap) + 1) % uint32_t(Tonemapper::Count));
			RDN_LOG("Tonemapper: {}", TonemapperNames[size_t(post.Tonemap)]);
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F8))
		{
			Settings& settings = m_App.GetSettings();
			settings.Shadows = ShadowQuality((uint32_t(settings.Shadows) + 1) % uint32_t(ShadowQuality::Count));
			m_App.GetWorldRenderer().SetShadowQuality(settings.Shadows);
			RDN_LOG("Shadows: {}", ShadowQualityNames[size_t(settings.Shadows)]);
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F9))
		{
			Settings& settings = m_App.GetSettings();
			settings.Gi = GiQuality((uint32_t(settings.Gi) + 1) % uint32_t(GiQuality::Count));
			m_App.GetWorldRenderer().SetGiQuality(settings.Gi);
			RDN_LOG("Global illumination: {}", GiQualityNames[size_t(settings.Gi)]);
		}
		const GameMode sessionMode = m_Client.GetSession().Mode;
		if (!m_Paused && !m_App.UiWantsMouse() && (sessionMode == GameMode::Royale || sessionMode == GameMode::Brawl) && !m_Client.GetPredictedState().Alive && m_Client.GetLocalPlayer().Watching != NoPlayer)
		{
			if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::MouseLeft))
				m_Client.Spectate(1);
			if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::MouseRight))
				m_Client.Spectate(-1);
		}
		if (m_Paused || m_App.UiWantsKeyboard())
			return;
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::I) || Rdn::Input::IsKeyPressed(Rdn::KeyCode::Tab))
		{
			if (sessionMode == GameMode::Brawl)
				m_LoadoutOpen = !m_LoadoutOpen;
			else
			{
				m_ShowInventory = !m_ShowInventory;
				if (m_ShowInventory)
					m_Station.reset();
			}
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::E) && !Interact())
		{
			if (m_ReachablePortal >= 0)
				UsePortal(m_ReachablePortal);
			else if (m_HoveredPortal >= 0)
				UsePortal(m_HoveredPortal);
		}
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		if (!local.OfferPending)
			return;
		for (uint32_t i = 0; i < UpgradeChoices; i++)
			if (Rdn::Input::IsKeyPressed(UpgradeKeys[i]))
				PickUpgrade(i);
	}

	void SessionState::PickUpgrade(uint32_t choice)
	{
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		if (m_ChosenUpgrade || !local.OfferPending || choice >= UpgradeChoices || local.Offer[choice] == UpgradeId::Count)
			return;
		m_Client.ChooseUpgrade(choice);
		m_ChosenUpgrade = choice;
	}

	void SessionState::UpdateAim()
	{
		const bool mouseFree = !m_Paused && !m_App.UiWantsMouse();
		if (const EntityState* player = m_Client.FindEntity(m_Client.GetPlayerNetId()); player && mouseFree)
			if (const std::optional<glm::vec3> point = MouseAimPoint())
			{
				const Vec2 offset = Vec2(point->x, point->z) - player->Position;
				m_Aim = AngleOf(offset);
				m_AimDistance = Length(offset);
			}
		m_Client.SetViewAim(m_Aim);
	}

	PlayerInput SessionState::SampleInput()
	{
		PlayerInput input;
		const bool mouseFree = !m_Paused && !m_App.UiWantsMouse();
		input.Aim = m_Aim;
		input.AimDistance = m_AimDistance;
		if (m_Paused || m_App.UiWantsKeyboard())
			return input;

		Vec2 move;
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::W))
			move.y += 1.0f;
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::S))
			move.y -= 1.0f;
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::D))
			move.x += 1.0f;
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::A))
			move.x -= 1.0f;
		input.Move = Normalize(move);
		if (!Rdn::Input::IsKeyDown(Rdn::KeyCode::MouseLeft))
			m_SuppressPrimary = false;
		if (mouseFree)
		{
			if (Rdn::Input::IsKeyDown(Rdn::KeyCode::MouseLeft) && !m_SuppressPrimary)
				input.Buttons |= uint8_t(InputButton::Primary);
			if (Rdn::Input::IsKeyDown(Rdn::KeyCode::MouseRight))
				input.Buttons |= uint8_t(InputButton::Skill1);
		}
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::Q))
			input.Buttons |= uint8_t(InputButton::Skill2);
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::R))
			input.Buttons |= uint8_t(InputButton::Skill3);
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::Space))
			input.Buttons |= uint8_t(InputButton::Dodge);
		input.Buttons |= std::exchange(m_LatchedButtons, uint8_t(0));
		input.Interact = std::exchange(m_PendingInteract, uint8_t(0));
		return input;
	}

	void SessionState::ExplainBlockedSkill(uint32_t slot)
	{
		if (m_Client.GetState() != ClientState::InGame)
			return;
		const SkillTraits& skill = m_Client.GetTraits().Skills[slot];
		const PlayerState& state = m_Client.GetPredictedState();
		if (!state.Alive)
			return;
		if (skill.IsEmpty())
			m_Hint = std::string("No skill gem on ") + SlotKeys[slot] + ", put one in with the inventory (I)";
		else if (!skill.Usable)
			m_Hint = std::string(GetAbility(skill.Ability).Name) + " needs " + DescribeRequirement(GetAbility(skill.Ability).Needs);
		else if (state.Mana + 1e-3f < skill.ManaCost)
			m_Hint = std::string("Not enough mana for ") + GetAbility(skill.Ability).Name;
		else
			return;
		m_HintTime = 1.6f;
	}

	void SessionState::LatchButtonPresses()
	{
		if (m_Paused)
		{
			m_LatchedButtons = 0;
			return;
		}
		if (!m_App.UiWantsMouse())
		{
			if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::MouseLeft))
			{
				if (m_HoveredPortal >= 0)
				{
					UsePortal(m_HoveredPortal);
					m_SuppressPrimary = true;
				}
				else
					m_LatchedButtons |= uint8_t(InputButton::Primary);
			}
			if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::MouseRight))
			{
				m_LatchedButtons |= uint8_t(InputButton::Skill1);
				ExplainBlockedSkill(1);
			}
		}
		if (m_App.UiWantsKeyboard())
			return;
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::Q))
		{
			m_LatchedButtons |= uint8_t(InputButton::Skill2);
			ExplainBlockedSkill(2);
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::R))
		{
			m_LatchedButtons |= uint8_t(InputButton::Skill3);
			ExplainBlockedSkill(3);
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::Space))
			m_LatchedButtons |= uint8_t(InputButton::Dodge);
	}

	std::optional<glm::vec3> SessionState::MouseOnPlane(float height) const
	{
		if (m_Extent.Width == 0 || m_Extent.Height == 0)
			return std::nullopt;
		const float x = 2.0f * Rdn::Input::GetMouseX() / float(m_Extent.Width) - 1.0f;
		const float y = 2.0f * Rdn::Input::GetMouseY() / float(m_Extent.Height) - 1.0f;
		const glm::mat4 inverse = glm::inverse(m_ViewProjection);
		glm::vec4 nearPoint = inverse * glm::vec4(x, y, 0.0f, 1.0f);
		glm::vec4 farPoint = inverse * glm::vec4(x, y, 1.0f, 1.0f);
		nearPoint /= nearPoint.w;
		farPoint /= farPoint.w;
		const glm::vec3 direction = glm::vec3(farPoint - nearPoint);
		if (std::abs(direction.y) < 1e-6f)
			return std::nullopt;
		return glm::vec3(nearPoint) + direction * ((height - nearPoint.y) / direction.y);
	}

	std::optional<glm::vec3> SessionState::MouseAimPoint() const
	{
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		return MouseOnPlane((self ? GroundHeight(self->Position) : 0.0f) + ProjectileHeight);
	}

	std::optional<glm::vec2> SessionState::ProjectToScreen(const glm::vec3& position) const
	{
		const glm::vec4 clip = m_ViewProjection * glm::vec4(position, 1.0f);
		if (clip.w <= 1e-4f)
			return std::nullopt;
		return glm::vec2((clip.x / clip.w * 0.5f + 0.5f) * float(m_Extent.Width), (clip.y / clip.w * 0.5f + 0.5f) * float(m_Extent.Height));
	}

	void SessionState::UpdateCamera(float dt)
	{
		const Level* level = m_Client.GetLevel();
		glm::vec3 target = m_CameraTarget;
		const EntityState* watched = nullptr;
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		if (local.Valid && !local.State.Alive && local.Watching != NoPlayer)
			for (const EntityState& entity : m_Client.GetEntities())
				if (entity.Kind == EntityKind::Player && entity.Owner == local.Watching)
					watched = &entity;
		if (watched)
			target = Ground(watched->Position);
		else if (const EntityState* player = m_Client.FindEntity(m_Client.GetPlayerNetId()))
			target = Ground(player->Position);
		else if (level)
			target = Ground(level->PlayerSpawn);
		if (!m_CameraPlaced || m_Fade > 0.5f || LengthSquared(Vec2(target.x - m_CameraTarget.x, target.z - m_CameraTarget.z)) > 25.0f)
			m_CameraTarget = target;
		else
		{
			m_CameraTarget.x = target.x;
			m_CameraTarget.z = target.z;
			m_CameraTarget.y += (target.y - m_CameraTarget.y) * std::min(1.0f, dt * 10.0f);
		}
		m_CameraPlaced = true;

		const float aspect = float(std::max(m_Extent.Width, 1u)) / float(std::max(m_Extent.Height, 1u));
		glm::vec3 focus = m_CameraTarget - glm::vec3(0.0f, 0.0f, CameraFocusOffset);
		glm::vec3 eye = focus + glm::vec3(0.0f, CameraHeight, -CameraBack);
		if (m_Overview && level)
		{
			const float extent = float(std::max(level->Map.GetWidth(), level->Map.GetHeight()));
			focus = { float(level->Map.GetWidth()) * 0.5f, 0.0f, float(level->Map.GetHeight()) * 0.5f };
			eye = focus + glm::vec3(0.0f, extent * 1.25f, -extent * 0.45f);
		}
		const float far = m_Overview ? 600.0f : 200.0f;
		glm::mat4 projection = glm::perspectiveLH_ZO(glm::radians(CameraFov), aspect, CameraNear, far);
		projection[1][1] *= -1.0f;
		const glm::mat4 view = glm::lookAtLH(eye, focus, glm::vec3(0.0f, 1.0f, 0.0f));
		m_ViewProjection = projection * view;
		m_Camera = WorldCamera{ .View = view, .Projection = projection, .ViewProjection = m_ViewProjection, .Eye = eye, .Near = CameraNear, .Far = far };
	}

	LevelView SessionState::ComputeView() const
	{
		LevelView view;
		view.Time = m_Time;
		view.Focus = { m_CameraTarget.x, m_CameraTarget.z };
		view.FocusHeight = m_CameraTarget.y;
		view.Cutaway = !m_Overview;
		view.HoveredPortal = m_HoveredPortal;
		view.ReachablePortal = m_ReachablePortal;
		if (m_Client.GetSession().Mode == GameMode::Brawl)
		{
			view.Brawl = &m_Client.GetStatus().Brawl;
			view.Risen = &m_Risen;
			view.Restore = std::min(m_RestoreAge / RestoreSeconds, 1.0f);
		}
		const Level* level = m_Client.GetLevel();
		if (m_Overview || !level)
		{
			view.Min = { -1.0f, -1.0f };
			view.Max = { 1e4f, 1e4f };
			return view;
		}
		Vec2 min = { 1e9f, 1e9f };
		Vec2 max = { -1e9f, -1e9f };
		const glm::mat4 inverse = glm::inverse(m_ViewProjection);
		const float overscan = 1.0f + 2.0f * m_App.GetWorldRenderer().GetGuardBand();
		for (const glm::vec2 corner : { glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, -1.0f), glm::vec2(-1.0f, 1.0f), glm::vec2(1.0f, 1.0f) })
			for (const float plane : { m_CameraTarget.y - 4.0f, m_CameraTarget.y + 3.5f })
			{
				glm::vec4 nearPoint = inverse * glm::vec4(corner * overscan, 0.0f, 1.0f);
				glm::vec4 farPoint = inverse * glm::vec4(corner * overscan, 1.0f, 1.0f);
				nearPoint /= nearPoint.w;
				farPoint /= farPoint.w;
				const glm::vec3 direction = glm::vec3(farPoint - nearPoint);
				if (std::abs(direction.y) < 1e-6f)
					continue;
				const glm::vec3 point = glm::vec3(nearPoint) + direction * ((plane - nearPoint.y) / direction.y);
				min = { std::min(min.x, point.x), std::min(min.y, point.z) };
				max = { std::max(max.x, point.x), std::max(max.y, point.z) };
			}
		view.Min = min - Vec2(1.5f, 1.5f);
		view.Max = max + Vec2(1.5f, 1.5f);
		view.CameraMin = view.Min;
		view.CameraMax = view.Max;
		if (m_App.GetWorldRenderer().GetShadowQuality() != ShadowQuality::Off)
		{
			const glm::vec3 sun = glm::normalize(m_Lighting.SunDirection);
			const float reach = ShadowCasterHeight / std::max(-sun.y, 0.2f);
			view.Min -= Vec2(std::max(sun.x, 0.0f), std::max(sun.z, 0.0f)) * reach;
			view.Max += Vec2(std::max(-sun.x, 0.0f), std::max(-sun.z, 0.0f)) * reach;
		}
		return view;
	}

	void SessionState::UpdateEffects(float dt)
	{
		m_Effects->Update(dt);
		for (auto& [netId, fx] : m_Fx)
		{
			fx.Seen = false;
			fx.Hurt = false;
			fx.SinceHit += dt;
			fx.Flash = std::max(fx.Flash - dt, 0.0f);
			fx.Age += dt;
		}
		for (const EntityState& entity : m_Client.GetEntities())
		{
			const auto [it, inserted] = m_Fx.try_emplace(entity.NetId, EntityFx{ .LastHealth = entity.Health, .Last = entity });
			EntityFx& fx = it->second;
			if (inserted)
				OnEntityAppeared(entity);
			else if (entity.Kind == EntityKind::Player && entity.Has(EntityFlags::Dashing) && !fx.Last.Has(EntityFlags::Dashing) && !entity.Has(EntityFlags::Dead))
				m_Effects->Burst(Fx::Dodge, Ground(entity.Position, 0.1f));
			if (!inserted && entity.Kind == EntityKind::Player && !entity.Has(EntityFlags::Dead))
			{
				if (entity.Has(EntityFlags::Flung) && !fx.Last.Has(EntityFlags::Flung))
					m_Effects->Burst(Fx::Dodge, Ground(entity.Position, 0.2f), glm::vec3(0.0f, 1.0f, 0.0f), 1.4f);
				if (entity.Has(EntityFlags::Burning) && !fx.Last.Has(EntityFlags::Burning))
					m_Effects->Burst(Fx::FireBurst, Ground(entity.Position, 0.1f), glm::vec3(0.0f, 1.0f, 0.0f), 0.7f);
			}
			if (!inserted && entity.Health < fx.LastHealth)
			{
				fx.Flash = HurtFlashSeconds;
				fx.Hurt = true;
			}
			fx.LastHealth = entity.Health;
			fx.Last = entity;
			fx.Seen = true;
		}
		for (const HitEvent& hit : m_Client.GetNewHits())
		{
			const auto it = m_Fx.find(hit.Target);
			if (it != m_Fx.end())
				it->second.SinceHit = 0.0f;
			const glm::vec3 point = Ground(hit.Position, it != m_Fx.end() ? BarHeight(it->second.Last) * 0.5f : 0.8f);
			m_Effects->Burst(HitFor(hit.Damages), point, glm::vec3(0.0f, 1.0f, 0.0f), hit.Critical ? 1.4f : 1.0f);
			if (hit.Critical)
				m_Effects->Burst(Fx::Critical, point);
		}
		for (auto it = m_Fx.begin(); it != m_Fx.end();)
		{
			const EntityFx& fx = it->second;
			if (fx.Seen)
			{
				++it;
				continue;
			}
			if (fx.SinceHit < KillWindow && m_Characters->ForEnemy(fx.Last.Kind))
			{
				m_Corpses.push_back({ fx.Last, fx.Animator });
				m_Corpses.back().Entity.Flags |= uint16_t(EntityFlags::Dead);
				m_Effects->Burst(Fx::Death, Ground(fx.Last.Position, 0.6f), glm::vec3(0.0f, 1.0f, 0.0f), fx.Last.GetScale());
			}
			else if (fx.Last.Kind == EntityKind::Projectile)
			{
				if (const std::optional<Fx> impact = ProjectileImpactFor(AbilityOf(fx.Last)))
					m_Effects->Burst(*impact, ProjectilePoint(fx.Last));
			}
			it = m_Fx.erase(it);
		}
		UpdateAnimators(dt);

		for (auto& [netId, flight] : m_ProjectileHeights)
			flight.Seen = false;
		for (const EntityState& entity : m_Client.GetEntities())
		{
			if (entity.Kind != EntityKind::Projectile)
				continue;
			const float target = GroundHeight(entity.Position) + ProjectileHeight;
			const auto [it, inserted] = m_ProjectileHeights.try_emplace(entity.NetId, Flight{ target, true });
			Flight& flight = it->second;
			flight.Height = target >= flight.Height ? target : std::max(target, flight.Height - ProjectileSink * dt);
			flight.Seen = true;
		}
		std::erase_if(m_ProjectileHeights, [](const auto& item) { return !item.second.Seen; });
		EmitContinuous(dt);
		m_Effects->EndFrame();

		for (DamageNumber& number : m_DamageNumbers)
			number.Age += dt;
		std::erase_if(m_DamageNumbers, [](const DamageNumber& number) { return number.Age >= DamageNumberSeconds; });
		for (const HitEvent& hit : m_Client.GetNewHits())
		{
			if (hit.Damage == 0)
				continue;
			const bool hurtsUs = hit.OnPlayer && (!IsPvpMode(m_Client.GetSession().Mode) || hit.Target == m_Client.GetPlayerNetId());
			if (m_Client.GetSession().Mode == GameMode::Brawl && !hurtsUs)
				continue;
			const float drift = std::fmod(float(m_HitCounter++) * 0.618034f, 1.0f) - 0.5f;
			m_DamageNumbers.push_back({ .Position = hit.Position, .Drift = drift * 0.9f, .Damage = hit.Damage, .Critical = hit.Critical, .OnPlayer = hurtsUs, .Damages = hit.Damages });
		}
		if (m_DamageNumbers.size() > MaxDamageNumbers)
			m_DamageNumbers.erase(m_DamageNumbers.begin(), m_DamageNumbers.end() - MaxDamageNumbers);
	}

	AbilityId SessionState::AbilityOf(const EntityState& entity) const
	{
		return AbilityId(std::min<uint32_t>(entity.Variant, uint32_t(AbilityId::Count) - 1));
	}

	glm::vec3 SessionState::ProjectilePoint(const EntityState& entity) const
	{
		const auto it = m_ProjectileHeights.find(entity.NetId);
		return { entity.Position.x, it != m_ProjectileHeights.end() ? it->second.Height : GroundHeight(entity.Position) + ProjectileHeight, entity.Position.y };
	}

	glm::vec3 SessionState::EffectColor(const EntityState& entity) const
	{
		const AbilityDef& def = GetAbility(AbilityOf(entity));
		return entity.Owner == NoPlayer && def.Damages == Element::Physical ? EnemyFire : SkillColor(def.Id);
	}

	glm::vec3 SessionState::MeteorPoint(const EntityState& entity, float progress) const
	{
		const float fall = 1.0f - progress * progress;
		return Ground(entity.Position, 0.45f) + glm::vec3(2.4f, 12.5f, 5.0f) * fall;
	}

	void SessionState::DrawMeteor(WorldRenderer& renderer, const EntityState& entity, float progress) const
	{
		const glm::vec3 target = Ground(entity.Position);
		const glm::vec3 position = MeteorPoint(entity, progress);
		const glm::vec3 direction = glm::normalize(target - MeteorPoint(entity, 0.0f));
		if (m_MeteorMesh)
		{
			const glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
				* glm::rotate(glm::mat4(1.0f), m_Time * 3.0f + float(entity.NetId % 31u), glm::normalize(glm::vec3(0.6f, 0.3f, 0.75f)))
				* glm::scale(glm::mat4(1.0f), glm::vec3(0.75f));
			renderer.DrawModel(*m_MeteorMesh, transform, NoPalette, glm::vec4(1.9f, 0.75f, 0.42f, 0.0f), 0.5f, target.y);
		}
		else
			renderer.Draw(MeshKind::Sphere, position, glm::vec3(0.4f), { 0.45f, 0.22f, 0.12f }, m_Time * 3.0f, 0.5f, target.y);
		DrawGlow(renderer, position, 1.4f, { 1.7f, 0.55f, 0.12f });
		DrawStreak(renderer, position - direction * 2.4f, position, 0.42f, { 2.8f, 1.1f, 0.25f });
		DrawGlow(renderer, position, 0.5f, { 3.0f, 1.6f, 0.5f });
		const AbilityDef& def = GetAbility(AbilityId::Meteor);
		DrawGroundGlow(renderer, target, def.ImpactRadius * entity.GetScale() * (0.6f + 0.4f * progress), { 1.5f, 0.45f, 0.1f }, 0.25f + 0.9f * progress * progress);
	}

	void SessionState::OnEntityAppeared(const EntityState& entity)
	{
		if (entity.Kind != EntityKind::Effect || entity.Has(EntityFlags::Pending))
			return;
		const AbilityId ability = AbilityOf(entity);
		const AbilityDef& def = GetAbility(ability);
		const float scale = entity.GetScale();
		m_Effects->Impact(ability, Ground(entity.Position), entity.Facing, scale, EffectColor(entity), entity.NetId, [this](const glm::vec2& point) { return GroundHeight(Vec2(point.x, point.y)); });
		const std::optional<Fx> fx = EffectBurstFor(ability);
		if (!fx)
			return;
		const glm::vec3 center = Ground(entity.Position, 0.5f);
		const glm::vec3 forward = Heading(entity.Facing);
		switch (def.Shape)
		{
		case StrikeShape::Line:
		{
			const float length = def.Reach * scale;
			const int points = std::max(2, int(length / 1.5f));
			for (int i = 0; i < points; i++)
				m_Effects->Burst(*fx, center + forward * (length * (float(i) + 0.5f) / float(points)), forward, 0.6f);
			return;
		}
		case StrikeShape::Ring:
		{
			const float radius = def.ImpactRadius * scale;
			for (int i = 0; i < 12; i++)
				m_Effects->Burst(*fx, center + Heading(float(i) * Pi / 6.0f) * radius, glm::vec3(0.0f, 1.0f, 0.0f), 0.6f);
			return;
		}
		case StrikeShape::Cone:
		{
			const float reach = def.Reach * scale;
			for (int i = 0; i < 5; i++)
			{
				const float angle = entity.Facing + def.HalfArc * (float(i) * 0.5f - 1.0f);
				m_Effects->Burst(*fx, center + Heading(angle) * (reach * 0.6f), Heading(angle), 0.7f);
			}
			return;
		}
		default:
			break;
		}
		if (def.Kind == AbilityKind::Melee)
			m_Effects->Burst(*fx, center + forward * (def.Reach * scale * 0.4f), forward, scale);
		else
			m_Effects->Burst(*fx, center, forward, scale);
	}

	void SessionState::EmitContinuous(float dt)
	{
		if (const Level* level = m_Client.GetLevel(); level && m_Client.GetSession().Mode == GameMode::Brawl)
		{
			const BrawlStatus& brawl = m_Client.GetStatus().Brawl;
			const uint32_t epoch = uint32_t(m_Time / 2.5f);
			for (uint32_t i = 0; i < 10; i++)
			{
				const uint32_t salt = (epoch + i * 7u) * 2654435761u + i * 40503u;
				const float u = float(salt & 0xFFFFu) / 65535.0f;
				const float v = float((salt >> 16) & 0xFFFFu) / 65535.0f;
				const Vec2 spot = Vec2(m_CameraTarget.x, m_CameraTarget.z) + Vec2(u - 0.5f, v - 0.5f) * 28.0f;
				if (IsBrawlLava(*level, brawl, spot))
					m_Effects->Trail(Fx::Campfire, (4ull << 40) | (uint64_t(epoch) << 8) | i, { spot.x, LavaSurface + 0.02f, spot.y }, dt, 0.4f + 0.3f * u);
			}
		}
		const std::vector<LevelFire>& fires = m_LevelRenderer.GetFires();
		for (size_t i = 0; i < fires.size(); i++)
		{
			const LevelFire& fire = fires[i];
			if (LengthSquared(Vec2(fire.Position.x, fire.Position.z) - Vec2(m_CameraTarget.x, m_CameraTarget.z)) > 26.0f * 26.0f)
				continue;
			const Fx fx = fire.Kind == FireKind::Campfire ? Fx::Campfire : fire.Kind == FireKind::Lantern ? Fx::LanternFlame : Fx::CandleFlame;
			m_Effects->Trail(fx, (2ull << 40) | uint64_t(i), fire.Position, dt, fire.Scale);
		}
		for (const EntityState& entity : m_Client.GetEntities())
		{
			switch (entity.Kind)
			{
			case EntityKind::Projectile:
				if (const std::optional<Fx> trail = TrailFor(AbilityOf(entity)))
					m_Effects->Trail(*trail, entity.NetId, ProjectilePoint(entity), dt, 1.0f, AbilityOf(entity) == AbilityId::PiercingShot ? glm::vec3(0.5f, 1.8f, 0.5f) : glm::vec3(1.0f));
				break;
			case EntityKind::Loot:
				if (IsItemLoot(entity) && ItemTagRarity(entity.Tag) >= Rarity::Legendary)
					m_Effects->Trail(Fx::LootSparkle, entity.NetId, Ground(entity.Position, 0.35f), dt, 1.0f, glm::vec3(RarityColor(ItemTagRarity(entity.Tag))));
				break;
			case EntityKind::Gateway:
				m_Effects->Trail(Fx::GatewaySwirl, entity.NetId, Ground(entity.Position, 0.15f), dt);
				break;
			case EntityKind::Effect:
				if (entity.Has(EntityFlags::Pending) && (AbilityOf(entity) == AbilityId::Meteor || AbilityOf(entity) == AbilityId::Comet))
				{
					const float delay = GetAbility(AbilityOf(entity)).ImpactDelay;
					m_Effects->Trail(Fx::MeteorTrail, entity.NetId, MeteorPoint(entity, delay > 0.0f ? std::clamp(AgeOf(entity.NetId) / delay, 0.0f, 1.0f) : 1.0f), dt);
				}
				break;
			case EntityKind::Player:
				if (entity.Has(EntityFlags::Attacking) && !entity.Has(EntityFlags::Dead) && entity.Variant < uint8_t(AbilityId::Count))
					if (const std::optional<Fx> cast = CastFor(AbilityId(entity.Variant)))
					{
						const glm::vec3 hand = Ground(entity.Position, 1.0f) + Heading(entity.Facing) * 0.35f + Heading(entity.Facing - HalfPi) * 0.3f;
						m_Effects->Trail(*cast, uint64_t(entity.NetId) | (1ull << 40), hand, dt);
					}
				if (entity.Has(EntityFlags::Burning) && !entity.Has(EntityFlags::Dead))
					m_Effects->Trail(Fx::Campfire, uint64_t(entity.NetId) | (3ull << 40), Ground(entity.Position, 0.05f), dt, 0.55f);
				break;
			default:
				break;
			}
		}
	}

	void SessionState::UpdateFeedback(float dt)
	{
		if (!m_Client.GetLocalPlayer().OfferPending)
			m_ChosenUpgrade.reset();

		for (Notice& notice : m_Notices)
			notice.Age += dt;
		std::erase_if(m_Notices, [](const Notice& notice) { return notice.Age >= NoticeSeconds; });
		for (const ServerNotice& notice : m_Client.TakeNotices())
		{
			m_Notices.push_back({ notice.Text, notice.Success ? Theme::AccentLight : Theme::Danger });
			m_App.GetGameAudio().PlayNotice(notice.Success, m_Station);
		}
		const MaterialStock& materials = m_Client.GetMaterials();
		if (m_SeenAnyMaterials && m_Client.GetLocalPlayer().Valid)
			for (size_t i = 0; i < materials.size(); i++)
			{
				const uint32_t gained = materials[i] > m_SeenMaterials[i] ? materials[i] - m_SeenMaterials[i] : 0;
				if (gained > 0 && (Material(i) != Material::Gold || gained >= 100))
					m_Notices.push_back({ "+" + std::to_string(gained) + " " + GetMaterial(Material(i)).Name, MaterialColor(Material(i)) });
			}
		m_SeenMaterials = materials;
		m_SeenAnyMaterials = m_Client.GetLocalPlayer().Valid;
		if (!m_Client.HasInventory())
			return;
		const ServerInventory& inventory = m_Client.GetInventory();
		if (m_SeenAnyInventory)
		{
			for (const Item& item : inventory.Pack)
				if (item.IsValid() && !HoldsItem(m_SeenInventory, item.Id))
					m_Notices.push_back({ "Picked up " + GetItemName(item), ItemColor(item) });
			for (size_t i = 0; i < inventory.Upgrades.size(); i++)
			{
				if (inventory.Upgrades[i] <= m_SeenInventory.Upgrades[i])
					continue;
				const UpgradeDef& upgrade = GetUpgrade(UpgradeId(i));
				m_Notices.push_back({ std::string(upgrade.Name) + " " + std::to_string(inventory.Upgrades[i]) + "/" + std::to_string(upgrade.MaxPicks) + ": " + upgrade.Description, WithAlpha(EliteGold) });
			}
		}
		if (m_Notices.size() > MaxNotices)
			m_Notices.erase(m_Notices.begin(), m_Notices.end() - MaxNotices);
		m_SeenInventory = inventory;
		m_SeenAnyInventory = true;
	}

	float SessionState::FlashOf(uint32_t netId) const
	{
		const auto it = m_Fx.find(netId);
		return it != m_Fx.end() ? it->second.Flash / HurtFlashSeconds : 0.0f;
	}

	float SessionState::AgeOf(uint32_t netId) const
	{
		const auto it = m_Fx.find(netId);
		return it != m_Fx.end() ? it->second.Age : 0.0f;
	}

	float SessionState::BarHeight(const EntityState& entity) const
	{
		if (entity.Kind == EntityKind::Player)
			return m_Characters->ForPlayer(LookOf(entity)).Height + 0.1f;
		if (const CharacterLook* look = m_Characters->ForEnemy(entity.Kind))
			return look->Height * entity.GetScale() + 0.1f;
		return HealthBarHeight(entity.Kind) * (IsEnemyKind(entity.Kind) ? entity.GetScale() : 1.0f);
	}

	const CharacterAnimator* SessionState::AnimatorOf(uint32_t netId) const
	{
		const auto it = m_Fx.find(netId);
		return it != m_Fx.end() ? &it->second.Animator : nullptr;
	}

	CharacterAnimator::Input SessionState::AnimationInput(const EntityState& entity) const
	{
		CharacterAnimator::Input input;
		input.Position = entity.Position;
		input.Facing = entity.Facing;
		input.Dead = entity.Has(EntityFlags::Dead);
		input.Dashing = entity.Has(EntityFlags::Dashing);
		input.Stunned = entity.Has(EntityFlags::Stunned);
		input.Attacking = entity.Has(EntityFlags::Attacking);
		if (entity.Kind == EntityKind::Player)
		{
			const CharacterLook& look = m_Characters->ForPlayer(LookOf(entity));
			input.Scale = look.Scale;
			input.Pose = look.Pose;
			if (input.Attacking && entity.Variant < uint8_t(AbilityId::Count))
			{
				const AbilityId ability = AbilityId(entity.Variant);
				input.Attack = PlayerAttackAnim(ability);
				input.Windup = GetAbility(ability).CastTime;
				const int8_t slot = m_Client.GetPredictedState().CastSlot;
				if (entity.NetId == m_Client.GetPlayerNetId() && slot >= 0)
					input.Windup = m_Client.GetTraits().Skills[size_t(slot)].CastTime;
			}
			return input;
		}
		const CharacterLook* look = m_Characters->ForEnemy(entity.Kind);
		const EnemyDef* def = FindEnemyDef(entity.Kind);
		input.Scale = (look ? look->Scale : 1.0f) * entity.GetScale();
		input.Pose = look ? look->Pose : Stance::Skeleton;
		input.Undead = true;
		input.Attack = EnemyAttackAnim(entity.Kind);
		input.Windup = def ? def->Windup : 0.5f;
		return input;
	}

	void SessionState::UpdateAnimators(float dt)
	{
		for (auto& [netId, fx] : m_Fx)
		{
			if (fx.Last.Kind != EntityKind::Player && !m_Characters->ForEnemy(fx.Last.Kind))
				continue;
			CharacterAnimator::Input input = AnimationInput(fx.Last);
			input.Hurt = fx.Hurt;
			fx.Animator.Update(*m_Characters, input, dt);
		}
		for (Corpse& corpse : m_Corpses)
		{
			corpse.Age += dt;
			corpse.Animator.Update(*m_Characters, AnimationInput(corpse.Entity), dt);
		}
		std::erase_if(m_Corpses, [](const Corpse& corpse) { return corpse.Age > CorpseSeconds; });
		if (m_Corpses.size() > MaxCorpses)
			m_Corpses.erase(m_Corpses.begin(), m_Corpses.end() - MaxCorpses);
	}

	glm::vec4 SessionState::CharacterTint(const EntityState& entity) const
	{
		glm::vec3 color(1.0f);
		if (entity.Has(EntityFlags::Elite))
			color = glm::mix(color, glm::vec3(1.0f, 0.82f, 0.45f), 0.4f);
		if (entity.Has(EntityFlags::Chilled))
			color = glm::mix(color, glm::vec3(0.6f, 0.85f, 1.0f), 0.5f);
		if (entity.Has(EntityFlags::Burning))
			color = glm::mix(color, glm::vec3(1.0f, 0.5f, 0.28f), 0.45f + 0.15f * std::sin(m_Time * 11.0f + float(entity.NetId)));
		return glm::vec4(color, FlashOf(entity.NetId) * 0.45f);
	}

	void SessionState::DrawCorpses(WorldRenderer& renderer) const
	{
		for (const Corpse& corpse : m_Corpses)
		{
			const CharacterLook* look = m_Characters->ForEnemy(corpse.Entity.Kind);
			if (!look)
				continue;
			const float sink = std::max(corpse.Age - CorpseSeconds + CorpseSinkSeconds, 0.0f) / CorpseSinkSeconds;
			const glm::vec3 base = Ground(corpse.Entity.Position) - glm::vec3(0.0f, sink * 0.6f, 0.0f);
			DrawCharacterModel(renderer, *m_Characters, *look, corpse.Animator, base, corpse.Entity.Facing, look->Scale * corpse.Entity.GetScale(), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f);
		}
	}

	PlayerLook SessionState::LookOf(const EntityState& player) const
	{
		if (player.Owner == m_Client.GetPlayerId() && m_Client.HasInventory())
		{
			const Equipment& gear = m_Client.GetInventory().Gear;
			const auto base = [&](GearSlot slot) { return gear[size_t(slot)].IsValid() ? gear[size_t(slot)].Base : ItemBase::Count; };
			const Item& weapon = gear[size_t(GearSlot::Weapon)];
			return { weapon.IsValid() ? weapon.Base : ItemBase::Wand, base(GearSlot::Head), base(GearSlot::Body), base(GearSlot::Feet), weapon.Quality };
		}
		if (const PlayerInfo* info = m_Client.FindPlayer(player.Owner))
			return info->Look;
		return {};
	}

	bool SessionState::Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D extent, uint32_t frameIndex)
	{
		const Level* level = m_Client.GetLevel();
		WorldRenderer& renderer = m_App.GetWorldRenderer();
		if (!level || !renderer.IsReady())
		{
			if (m_Client.GetState() == ClientState::InGame)
				return false;
			BackdropView view = BackdropMoods[size_t(m_Config.Host ? BackdropMood::Kindle : BackdropMood::Seek)];
			view.Time = m_App.GetUi().GetTime() + 30.0f;
			return m_App.GetMenuBackdrop().Render(graph, target, extent, view);
		}
		if (extent != m_Extent)
		{
			m_Extent = extent;
			UpdateCamera(0.0f);
		}

		m_Lighting = MapLighting(level->Kind, renderer);
		renderer.Begin(m_Camera, m_Lighting, m_Overview ? 0.0f : m_CameraTarget.y, !m_Overview);
		m_LevelRenderer.Draw(renderer, *level, ComputeView());
		const std::vector<LevelFire>& fires = m_LevelRenderer.GetFires();
		for (size_t i = 0; i < fires.size(); i++)
		{
			const LevelFire& fire = fires[i];
			if (LengthSquared(Vec2(fire.Position.x, fire.Position.z) - Vec2(m_CameraTarget.x, m_CameraTarget.z)) > 26.0f * 26.0f)
				continue;
			const float phase = float(i) * 1.7f;
			const float flicker = 0.85f + 0.1f * std::sin(m_Time * 13.0f + phase) + 0.05f * std::sin(m_Time * 29.0f + phase * 2.3f);
			const float size = fire.Kind == FireKind::Campfire ? 0.9f : fire.Kind == FireKind::Lantern ? 0.32f : 0.16f;
			renderer.DrawSprite({ .Position = fire.Position + glm::vec3(0.0f, fire.Kind == FireKind::Campfire ? 0.25f : 0.02f, 0.0f), .Size = size * fire.Scale, .Color = glm::vec4(glm::vec3(1.0f, 0.55f, 0.22f) * flicker, 0.35f), .Shape = ParticleShape::Glow, .Softness = 1.0f });
		}
		m_Effects->Draw(renderer);
		for (const EntityState& entity : m_Client.GetEntities())
			DrawEntity(renderer, entity);
		DrawCorpses(renderer);
		DrawStorm(renderer);
		const PlayerState& state = m_Client.GetPredictedState();
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		if (m_AimPoint && self && !m_Paused && !m_Overview && m_HoveredPortal < 0)
		{
			DrawReticle(renderer, *m_AimPoint, 0.2f, { 1.0f, 0.92f, 0.75f });
			if (state.CastSlot >= 0)
			{
				const AbilityDef& ability = GetAbility(m_Client.GetTraits().Skills[size_t(state.CastSlot)].Ability);
				if (ability.Kind == AbilityKind::Ground)
				{
					const Vec2 point = GetGroundTarget(*level, self->Position, FromAngle(m_Aim), m_AimDistance, ability.Range);
					DrawRuneCircle(renderer, { .Center = Ground(point), .Radius = ability.ImpactRadius * m_Client.GetTraits().AreaScale, .Color = SkillColor(ability.Id), .Intensity = 0.6f, .Spin = 0.6f, .Seed = 7u });
				}
			}
		}
		renderer.Render(graph, target, extent, frameIndex, { 0.05f, 0.055f, 0.07f, 1.0f });
		return true;
	}

	void SessionState::DrawEntity(WorldRenderer& renderer, const EntityState& entity) const
	{
		const glm::vec3 base = Ground(entity.Position);
		switch (entity.Kind)
		{
		case EntityKind::Player:
			DrawPlayer(renderer, entity);
			break;
		case EntityKind::Chaser:
		case EntityKind::Shooter:
		case EntityKind::Brute:
		case EntityKind::Caster:
			DrawEnemy(renderer, entity);
			break;
		case EntityKind::Archivist:
		case EntityKind::Wyrm:
			DrawBoss(renderer, entity);
			break;
		case EntityKind::Chest:
			DrawChest(renderer, entity);
			break;
		case EntityKind::Gateway:
			DrawGateway(renderer, entity);
			break;
		case EntityKind::Projectile:
			DrawProjectile(renderer, entity);
			break;
		case EntityKind::Effect:
			DrawEffect(renderer, entity);
			break;
		case EntityKind::Loot:
		{
			const float phase = float(entity.NetId % 97);
			if (IsItemLoot(entity))
			{
				DrawItemDrop(renderer, *m_Characters, ItemTagBase(entity.Tag), ItemTagRarity(entity.Tag), ItemTagSkill(entity.Tag), base, m_Time, phase);
				break;
			}
			const glm::vec3 color = LootColors[std::min<size_t>(entity.Variant, std::size(LootColors) - 1)];
			const float bob = 0.38f + 0.08f * std::sin(m_Time * 3.0f + phase);
			const MeshKind mesh = LootKind(entity.Variant) == LootKind::Health || LootKind(entity.Variant) == LootKind::Mana ? MeshKind::Sphere : MeshKind::Box;
			renderer.Draw(mesh, base + glm::vec3(0.0f, bob, 0.0f), { 0.14f, 0.14f, 0.14f }, color, m_Time * 2.0f + phase, 0.75f, base.y);
			DrawGlow(renderer, base + glm::vec3(0.0f, bob, 0.0f), 0.32f, color * 0.55f);
			DrawGroundGlow(renderer, base, 0.4f, color, 0.6f);
			break;
		}
		default:
			break;
		}
	}

	void SessionState::DrawPlayer(WorldRenderer& renderer, const EntityState& entity) const
	{
		const CharacterAnimator* animator = AnimatorOf(entity.NetId);
		if (!animator)
			return;
		const glm::vec3 base = Ground(entity.Position);
		const CharacterLook& look = m_Characters->ForPlayer(LookOf(entity));
		const bool dead = entity.Has(EntityFlags::Dead);
		const CharacterPose pose = DrawCharacterModel(renderer, *m_Characters, look, *animator, base, entity.Facing, look.Scale, CharacterTint(entity), 0.0f);
		if (dead)
			return;
		if (m_Client.GetSession().Mode == GameMode::Brawl)
			DrawBrawlPlayer(renderer, entity, base);
		DrawGroundRing(renderer, base, 0.42f, ColorOf(entity.Owner), 1.2f, 0.022f);
		if (m_Client.IsHostile(entity))
			DrawGroundRing(renderer, base, 0.56f, { 1.0f, 0.25f, 0.18f }, 1.6f, 0.03f, 12.0f, 0.15f);
		if (entity.Has(EntityFlags::Stunned))
			DrawStunStars(renderer, pose.JointPosition(m_Characters->GetHead()) + glm::vec3(0.0f, 0.75f * look.Scale, 0.0f));
		if (!entity.Has(EntityFlags::Attacking) || entity.Variant >= uint8_t(AbilityId::Count))
			return;
		const AbilityId casting = AbilityId(entity.Variant);
		const glm::vec3 glow = SkillColor(casting);
		const float pulse = 0.6f + 0.4f * std::sin(m_Time * 14.0f);
		if (GetAbility(casting).Gem)
		{
			DrawRuneCircle(renderer, { .Center = base, .Radius = 0.8f, .Color = glow, .Intensity = 0.8f + 0.35f * pulse, .Spin = 1.4f, .Seed = entity.NetId });
			const glm::vec3 hand = pose.JointPosition(m_Characters->GetRightHand());
			DrawGlow(renderer, hand, 0.3f + 0.08f * pulse, glow * 1.4f);
			DrawGlow(renderer, hand, 0.09f, glm::mix(glow, glm::vec3(1.0f), 0.5f) * 3.0f);
		}
	}

	void SessionState::DrawStunStars(WorldRenderer& renderer, const glm::vec3& center) const
	{
		for (uint32_t i = 0; i < 3; i++)
		{
			const float angle = m_Time * 5.0f + float(i) * 2.094f;
			DrawStar(renderer, center + Heading(angle) * 0.3f, 0.14f, { 2.4f, 2.1f, 0.6f }, angle);
		}
	}

	void SessionState::DrawEnemy(WorldRenderer& renderer, const EntityState& entity) const
	{
		const CharacterLook* look = m_Characters->ForEnemy(entity.Kind);
		const CharacterAnimator* animator = AnimatorOf(entity.NetId);
		if (!look || !animator)
			return;
		const float size = entity.GetScale();
		const bool elite = entity.Has(EntityFlags::Elite);
		const glm::vec3 base = Ground(entity.Position);
		const bool attacking = entity.Has(EntityFlags::Attacking);
		const float pulse = 0.55f + 0.45f * std::sin(m_Time * 16.0f);
		const EnemyDef* def = FindEnemyDef(entity.Kind);
		const AbilityDef& attack = GetAbility(def ? def->Attack : AbilityId::Bite);
		const float reach = attack.Reach * (elite ? EliteAreaScale : 1.0f);
		const CharacterPose pose = DrawCharacterModel(renderer, *m_Characters, *look, *animator, base, entity.Facing, look->Scale * size, CharacterTint(entity), elite ? 0.12f : 0.0f);
		switch (entity.Kind)
		{
		case EntityKind::Chaser:
			if (attacking)
				DrawSectorTelegraph(renderer, base, entity.Facing, reach, attack.HalfArc, { 1.0f, 0.22f, 0.12f }, 0.8f + 0.4f * pulse, -1.0f);
			break;
		case EntityKind::Shooter:
			if (attacking)
				DrawLaneTelegraph(renderer, base + Heading(entity.Facing) * attack.SpawnOffset, entity.Facing, attack.ProjectileRange, 0.24f, { 0.55f, 1.0f, 0.3f }, 0.7f + 0.35f * pulse, -1.0f);
			break;
		case EntityKind::Brute:
			if (attacking)
				DrawRuneCircle(renderer, { .Center = base, .Radius = reach, .Color = { 1.0f, 0.25f, 0.1f }, .Intensity = 0.8f + 0.4f * pulse, .Spin = 1.5f, .Seed = entity.NetId });
			break;
		case EntityKind::Caster:
		{
			const glm::vec3 tip = pose.JointPosition(m_Characters->GetRightHand()) + glm::vec3(0.0f, 0.95f * look->Scale * size, 0.0f);
			DrawGlow(renderer, tip, (attacking ? 0.45f + 0.12f * pulse : 0.25f) * size, EnemyFire * (attacking ? 2.2f : 1.2f));
			DrawGlow(renderer, tip, 0.1f * size, { 2.5f, 1.8f, 1.0f });
			if (attacking)
				DrawRuneCircle(renderer, { .Center = base, .Radius = 0.8f * size, .Color = EnemyFire, .Intensity = 0.9f + 0.4f * pulse, .Spin = 2.0f, .Seed = entity.NetId });
			break;
		}
		default:
			break;
		}
		const float top = BarHeight(entity);
		if (entity.Has(EntityFlags::Stunned))
			DrawStunStars(renderer, base + glm::vec3(0.0f, top + 0.15f, 0.0f));
		if (!elite)
			return;
		const float shimmer = 0.6f + 0.3f * std::sin(m_Time * 5.0f + float(entity.NetId));
		DrawGroundRing(renderer, base, (def ? def->Radius : 0.4f) * size + 0.22f, EliteGold, 1.0f + 0.5f * shimmer, 0.035f, 10.0f, 0.08f);
		DrawStar(renderer, base + glm::vec3(0.0f, top - 0.12f, 0.0f), 0.2f * size, EliteGold * 2.2f, m_Time * 1.5f);
	}

	void SessionState::DrawTelegraph(WorldRenderer& renderer, const EntityState& entity, float progress) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const AbilityDef& def = GetAbility(AbilityOf(entity));
		const glm::vec3 color = EffectColor(entity);
		const float scale = entity.GetScale();
		const float appear = std::clamp(AgeOf(entity.NetId) / 0.3f, 0.0f, 1.0f);
		const float intensity = 0.75f + 0.35f * progress;
		switch (def.Shape)
		{
		case StrikeShape::Line:
			DrawLaneTelegraph(renderer, base, entity.Facing, def.Reach * scale, def.Width * scale, color, intensity, progress, appear);
			break;
		case StrikeShape::Cone:
			DrawSectorTelegraph(renderer, base, entity.Facing, def.Reach * scale, def.HalfArc, color, intensity, progress, appear);
			break;
		case StrikeShape::Ring:
			DrawRuneCircle(renderer, { .Center = base, .Radius = def.ImpactRadius * scale, .Color = color, .Intensity = intensity, .Spin = 0.25f, .Progress = progress, .Appear = appear, .Inner = def.InnerRadius / std::max(def.ImpactRadius, 0.01f), .Seed = entity.NetId });
			break;
		default:
			DrawRuneCircle(renderer, { .Center = base, .Radius = def.ImpactRadius * scale, .Color = color, .Intensity = intensity, .Spin = 0.5f, .Progress = progress, .Appear = appear, .Seed = entity.NetId });
			break;
		}
	}

	void SessionState::DrawBoss(WorldRenderer& renderer, const EntityState& entity) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const float flash = FlashOf(entity.NetId);
		const bool attacking = entity.Has(EntityFlags::Attacking);
		const bool chilled = entity.Has(EntityFlags::Chilled);
		const float pulse = 0.55f + 0.45f * std::sin(m_Time * 9.0f);
		const glm::vec3 forward = Heading(entity.Facing);
		const glm::vec3 side = Heading(entity.Facing + HalfPi);
		const auto shade = [&](const glm::vec3& color)
		{
			const glm::vec3 tone = chilled ? glm::mix(color, glm::vec3(0.55f, 0.85f, 1.0f), 0.35f) : color;
			return glm::mix(tone, glm::vec3(1.0f), flash * 0.7f);
		};
		if (entity.Kind == EntityKind::Archivist)
		{
			const glm::vec3 body = base;
			const glm::vec3 arcane(0.75f, 0.55f, 1.0f);
			const CharacterLook* look = m_Characters->ForEnemy(entity.Kind);
			if (const CharacterAnimator* animator = AnimatorOf(entity.NetId); look && animator)
				DrawCharacterModel(renderer, *m_Characters, *look, *animator, base, entity.Facing, look->Scale, CharacterTint(entity), 0.0f);
			const uint32_t books = 6;
			for (uint32_t i = 0; i < books; i++)
			{
				const float angle = m_Time * (attacking ? 2.6f : 1.1f) + float(i) * 2.0f * Pi / float(books);
				const glm::vec3 position = body + Heading(angle) * 1.25f + glm::vec3(0.0f, 1.5f + 0.25f * std::sin(m_Time * 2.0f + float(i)), 0.0f);
				renderer.DrawBox(position, { 0.2f, 0.06f, 0.15f }, i % 2 ? glm::vec3(0.5f, 0.2f, 0.15f) : glm::vec3(0.2f, 0.25f, 0.45f), angle + HalfPi, attacking ? 0.6f + 0.4f * pulse : 0.15f);
				renderer.DrawBox(position + glm::vec3(0.0f, 0.065f, 0.0f), { 0.17f, 0.01f, 0.13f }, glm::vec3(0.95f, 0.9f, 0.75f), angle + HalfPi, 0.4f);
			}
			DrawRuneCircle(renderer, { .Center = base, .Radius = 1.25f + (attacking ? 0.15f * pulse : 0.0f), .Color = arcane, .Intensity = attacking ? 1.2f : 0.6f, .Spin = attacking ? 1.2f : 0.3f, .Seed = entity.NetId });
			return;
		}

		const glm::vec3 crystal(0.45f, 0.9f, 1.0f);
		const glm::vec3 hide(0.22f, 0.26f, 0.34f);
		if (entity.Variant == 1)
		{
			renderer.Draw(MeshKind::Sphere, base + glm::vec3(0.0f, 0.05f, 0.0f), { 1.1f, 0.3f, 1.1f }, glm::vec3(0.32f, 0.26f, 0.2f), m_Time, 0.0f, base.y);
			for (uint32_t i = 0; i < 5; i++)
			{
				const float angle = float(i) * 1.257f + m_Time * 0.5f;
				renderer.Draw(MeshKind::Cone, base + Heading(angle) * 0.55f + glm::vec3(0.0f, 0.35f, 0.0f), { 0.12f, 0.35f, 0.12f }, crystal, angle, 0.7f, base.y);
			}
			DrawGroundRing(renderer, base, 1.3f + 0.15f * pulse, glm::vec3(0.55f, 0.45f, 0.35f), 0.6f, 0.05f);
			return;
		}
		const uint32_t segments = 6;
		for (uint32_t i = segments; i-- > 0;)
		{
			const float along = float(i) * 0.75f;
			const float sway = std::sin(m_Time * 3.0f - float(i) * 0.8f) * 0.25f * float(i) / float(segments);
			const float size = 0.62f - 0.06f * float(i);
			const glm::vec3 center = base - forward * along + side * sway + glm::vec3(0.0f, size + 0.05f, 0.0f);
			renderer.Draw(MeshKind::Sphere, center, glm::vec3(size), shade(i == 0 ? hide * 1.2f : hide), entity.Facing, flash, base.y);
			renderer.Draw(MeshKind::Cone, center + glm::vec3(0.0f, size + 0.15f, 0.0f), { 0.13f, 0.32f + 0.05f * float(i % 2), 0.13f }, crystal, entity.Facing + float(i), attacking ? 0.6f + 0.4f * pulse : 0.45f, base.y);
		}
		const glm::vec3 head = base + forward * 0.55f + glm::vec3(0.0f, 0.75f, 0.0f);
		renderer.DrawBox(head, { 0.5f, 0.3f, 0.38f }, shade(hide * 1.3f), entity.Facing, flash);
		renderer.DrawBox(head + forward * 0.45f - glm::vec3(0.0f, 0.1f, 0.0f), { 0.25f, 0.14f, 0.28f }, shade(hide * 1.1f), entity.Facing, flash);
		for (const float sign : { -1.0f, 1.0f })
		{
			renderer.DrawBox(head + forward * 0.25f + side * (0.24f * sign) + glm::vec3(0.0f, 0.12f, 0.0f), { 0.06f, 0.05f, 0.05f }, crystal, entity.Facing, 1.0f);
			renderer.Draw(MeshKind::Cone, head - forward * 0.15f + side * (0.22f * sign) + glm::vec3(0.0f, 0.42f, 0.0f), { 0.08f, 0.3f, 0.08f }, crystal, entity.Facing, 0.7f, base.y);
		}
		if (attacking)
			DrawRuneCircle(renderer, { .Center = base, .Radius = 1.6f + 0.2f * pulse, .Color = crystal, .Intensity = 1.0f, .Spin = 1.0f, .Seed = entity.NetId });
	}

	void SessionState::DrawChest(WorldRenderer& renderer, const EntityState& entity) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const bool opened = entity.Variant != 0;
		const ChestKind kind = ChestKind(std::min<uint32_t>(entity.Tag, uint32_t(ChestKind::Count) - 1));
		const float pulse = 0.5f + 0.5f * std::sin(m_Time * 3.0f);
		if (kind == ChestKind::Common)
		{
			const glm::vec3 wood(0.5f, 0.32f, 0.18f);
			const glm::vec3 iron(0.35f, 0.35f, 0.4f);
			renderer.DrawBox(base + glm::vec3(0.0f, 0.22f, 0.0f), { 0.4f, 0.22f, 0.28f }, opened ? wood * 0.6f : wood, entity.Facing);
			for (const float x : { -0.3f, 0.3f })
				renderer.DrawBox(base + Heading(entity.Facing) * x + glm::vec3(0.0f, 0.23f, 0.0f), { 0.04f, 0.23f, 0.29f }, iron, entity.Facing);
			if (opened)
				renderer.DrawBox(base + glm::vec3(0.0f, 0.6f, 0.0f) - Heading(entity.Facing + HalfPi) * 0.28f, { 0.4f, 0.18f, 0.04f }, wood * 0.55f, entity.Facing);
			else
			{
				renderer.DrawBox(base + glm::vec3(0.0f, 0.5f, 0.0f), { 0.41f, 0.07f, 0.29f }, wood * 1.15f, entity.Facing);
				DrawGroundRing(renderer, base, 0.7f, { 0.95f, 0.85f, 0.6f }, 0.5f + 0.4f * pulse, 0.025f);
			}
			return;
		}
		if (kind == ChestKind::Cache)
		{
			const glm::vec3 ember(1.0f, 0.45f, 0.12f);
			const glm::vec3 stone(0.22f, 0.2f, 0.22f);
			renderer.Draw(MeshKind::Sphere, base + glm::vec3(0.0f, 0.35f, 0.0f), { 0.7f, 0.45f, 0.7f }, opened ? stone * 0.6f : stone, entity.Facing, 0.0f, base.y);
			for (uint32_t i = 0; i < 5; i++)
			{
				const float angle = float(i) * 1.2566f + entity.Facing;
				renderer.Draw(MeshKind::Cone, base + Heading(angle) * 0.42f + glm::vec3(0.0f, 0.55f, 0.0f), { 0.12f, 0.4f, 0.12f }, ember, angle, opened ? 0.2f : 0.7f + 0.3f * pulse, base.y);
			}
			if (opened)
				return;
			DrawRuneCircle(renderer, { .Center = base, .Radius = 1.4f + 0.15f * pulse, .Color = ember, .Intensity = 0.8f, .Spin = 0.4f, .Seed = entity.NetId });
			for (uint32_t i = 0; i < 6; i++)
			{
				const float t = std::fmod(m_Time * 0.5f + float(i) / 6.0f, 1.0f);
				DrawGlow(renderer, base + Heading(float(i) * 1.05f + m_Time * 0.7f) * (0.5f + 0.4f * t) + glm::vec3(0.0f, 0.8f + 3.5f * t, 0.0f), 0.14f * (1.0f - t) + 0.03f, ember * 2.5f);
			}
			return;
		}
		const glm::vec3 wood = kind == ChestKind::Vault ? glm::vec3(0.28f, 0.26f, 0.3f) : glm::vec3(0.42f, 0.24f, 0.12f);
		const glm::vec3 gold(1.0f, 0.78f, 0.3f);
		renderer.DrawBox(base + glm::vec3(0.0f, 0.3f, 0.0f), { 0.55f, 0.3f, 0.38f }, wood, entity.Facing);
		for (const float x : { -0.42f, 0.0f, 0.42f })
			renderer.DrawBox(base + Heading(entity.Facing) * x + glm::vec3(0.0f, 0.31f, 0.0f), { 0.05f, 0.31f, 0.39f }, gold, entity.Facing, opened ? 0.1f : 0.35f);
		if (opened)
			renderer.DrawBox(base + glm::vec3(0.0f, 0.82f, 0.0f) - Heading(entity.Facing + HalfPi) * 0.38f, { 0.55f, 0.25f, 0.05f }, wood * 0.9f, entity.Facing);
		else
		{
			renderer.DrawBox(base + glm::vec3(0.0f, 0.7f, 0.0f), { 0.56f, 0.1f, 0.39f }, wood * 1.1f, entity.Facing);
			renderer.DrawBox(base + glm::vec3(0.0f, 0.5f, 0.0f) + Heading(entity.Facing + HalfPi) * 0.4f, { 0.08f, 0.1f, 0.03f }, gold, entity.Facing, 0.8f);
			DrawGroundRing(renderer, base, 0.95f + 0.1f * pulse, gold, 0.9f + 0.6f * pulse, 0.035f);
			for (uint32_t i = 0; i < 4; i++)
			{
				const float t = std::fmod(m_Time * 0.6f + float(i) * 0.25f, 1.0f);
				DrawStar(renderer, base + Heading(float(i) * 1.6f + m_Time) * 0.35f + glm::vec3(0.0f, 0.8f + 1.2f * t, 0.0f), 0.13f * (1.0f - t) + 0.03f, gold * 2.2f, m_Time * 3.0f);
			}
		}
	}

	void SessionState::DrawGateway(WorldRenderer& renderer, const EntityState& entity) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const glm::vec3 inner(0.75f, 0.55f, 1.0f);
		const glm::vec3 outer(1.0f, 0.8f, 0.4f);
		DrawRuneCircle(renderer, { .Center = base, .Radius = 1.7f, .Color = outer, .Intensity = 0.9f, .Spin = 0.3f, .Seed = entity.NetId });
		DrawGroundRing(renderer, base, 1.15f, inner, 1.0f, 0.04f, 16.0f, -0.1f);
		for (uint32_t level = 0; level < 3; level++)
		{
			const float t = std::fmod(m_Time * 0.45f + float(level) / 3.0f, 1.0f);
			DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.2f + 2.6f * t, 0.0f), 1.3f * (1.0f - 0.5f * t), glm::mix(inner, outer, t) * (1.6f * (1.0f - t)), 0.035f);
		}
		for (uint32_t i = 0; i < 8; i++)
		{
			const float angle = float(i) * 0.785f + m_Time * 0.8f;
			const float t = std::fmod(m_Time * 0.7f + float(i) * 0.125f, 1.0f);
			const glm::vec3 point = base + Heading(angle) * (1.35f - 0.6f * t) + glm::vec3(0.0f, 0.2f + 2.2f * t, 0.0f);
			DrawStreak(renderer, point - glm::vec3(0.0f, 0.18f, 0.0f), point + glm::vec3(0.0f, 0.18f, 0.0f), 0.04f, inner * (1.8f * (1.0f - 0.5f * t)));
		}
		DrawGlow(renderer, base + glm::vec3(0.0f, 1.3f, 0.0f), 0.8f, inner * 0.9f);
		DrawGlow(renderer, base + glm::vec3(0.0f, 1.3f, 0.0f), 0.26f + 0.05f * std::sin(m_Time * 4.0f), { 2.2f, 1.8f, 3.0f });
	}

	void SessionState::DrawProjectile(WorldRenderer& renderer, const EntityState& entity) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const glm::vec3 center = ProjectilePoint(entity);
		const AbilityId ability = AbilityOf(entity);
		switch (ability)
		{
		case AbilityId::Arrow:
		case AbilityId::Volley:
		case AbilityId::PiercingShot:
		case AbilityId::Quarrel:
			if (const std::optional<uint32_t> mesh = m_Characters->ArrowMesh(ability))
				renderer.DrawModel(*mesh, ModelTransform(center, entity.Facing, ability == AbilityId::PiercingShot ? 0.75f : 0.55f), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.25f, base.y);
			break;
		default:
			break;
		}
		const glm::vec3 color = entity.Owner == NoPlayer ? SkillColor(ability) : glm::mix(PlayerColor(entity.Owner), SkillColor(ability), 0.6f);
		DrawProjectileVisual(renderer, ability, center, entity.Facing, color, m_Time, entity.NetId);
	}

	void SessionState::DrawEffect(WorldRenderer& renderer, const EntityState& entity) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const AbilityId ability = AbilityOf(entity);
		const AbilityDef& def = GetAbility(ability);
		const glm::vec3 owner = entity.Owner == NoPlayer ? EnemyFire : PlayerColor(entity.Owner);
		const glm::vec3 color = EffectColor(entity);
		const glm::vec3 bright = glm::mix(owner, glm::vec3(1.0f), 0.6f);
		const float age = AgeOf(entity.NetId);
		const float scale = entity.GetScale();
		if (entity.Has(EntityFlags::Pending))
		{
			const float progress = def.ImpactDelay > 0.0f ? std::clamp(age / def.ImpactDelay, 0.0f, 1.0f) : 1.0f;
			DrawTelegraph(renderer, entity, progress);
			if (ability == AbilityId::Meteor || ability == AbilityId::Comet)
				DrawMeteor(renderer, entity, progress);
			if (ability == AbilityId::GravityWell)
			{
				const float radius = def.ImpactRadius * scale;
				for (uint32_t i = 0; i < 3; i++)
				{
					const float phase = std::fmod(m_Time * 1.4f + float(i) / 3.0f, 1.0f);
					DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.12f + 0.25f * phase, 0.0f), radius * (1.0f - phase) + 0.15f, color * (1.6f * phase), 0.03f);
				}
				DrawGlow(renderer, base + glm::vec3(0.0f, 0.4f, 0.0f), 0.5f + 0.4f * progress, color * (0.8f + progress));
			}
			return;
		}

		const float t = std::clamp(age / std::max(def.EffectDuration, 0.05f), 0.0f, 1.0f);
		const float swing = 1.0f - (1.0f - t) * (1.0f - t);
		const glm::vec3 forward = Heading(entity.Facing);
		const glm::vec3 chest = base + glm::vec3(0.0f, 0.45f, 0.0f);
		const bool mirrored = (entity.NetId & 1u) != 0;
		switch (def.Kind)
		{
		case AbilityKind::Nova:
			if (ability == AbilityId::Cyclone)
				for (uint32_t i = 0; i < 3; i++)
					DrawSlash(renderer, base + glm::vec3(0.0f, 0.35f + 0.15f * float(i), 0.0f), m_Time * 17.0f + float(i) * 2.094f, def.Reach * scale * (0.7f + 0.12f * float(i)), 1.3f, 1.0f, glm::mix(bright, color, 0.4f) * 0.7f, false, 0.55f);
			else if (ability == AbilityId::Shockwave)
			{
				DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.35f, 0.0f), def.Reach * scale * swing, color * (2.6f * (1.0f - t)), 0.07f);
				DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.2f, 0.0f), def.Reach * scale * swing * 0.7f, glm::mix(color, glm::vec3(1.0f), 0.5f) * (1.8f * (1.0f - t)), 0.04f);
			}
			break;
		case AbilityKind::Ward:
			DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.9f, 0.0f), 0.5f + 0.5f * swing, color * (2.4f * (1.0f - t)), 0.05f);
			DrawGlow(renderer, base + glm::vec3(0.0f, 0.9f, 0.0f), 0.9f * (1.0f - t) + 0.2f, color * 1.4f);
			break;
		case AbilityKind::Melee:
		{
			const float reach = def.Reach * scale;
			switch (ability)
			{
			case AbilityId::Bite:
				for (const bool jaw : { false, true })
					DrawSlash(renderer, chest, entity.Facing, reach * 0.75f, def.HalfArc, swing, { 1.5f, 0.22f, 0.12f }, jaw);
				break;
			case AbilityId::Stab:
			{
				const glm::vec3 tip = chest + forward * (reach * (0.55f + 0.35f * swing));
				DrawStreak(renderer, chest + forward * 0.3f, tip, 0.07f, bright * 1.3f);
				DrawStar(renderer, tip, 0.22f * (1.0f - t) + 0.05f, bright * 1.4f);
				break;
			}
			case AbilityId::Thrust:
			{
				const float extent = reach * (0.55f + 0.45f * std::min(t * 2.5f, 1.0f));
				const float halfWidth = def.Width * scale * 0.5f;
				const float fade = 1.0f - t;
				const glm::vec3 tip = chest + forward * extent;
				DrawStreak(renderer, chest, tip, 0.1f, bright * (1.5f * fade + 0.3f));
				DrawStar(renderer, tip, 0.32f * fade + 0.08f, glm::mix(bright, glm::vec3(1.0f), 0.4f) * 1.6f);
				for (const float across : { -1.0f, 1.0f })
				{
					const glm::vec3 side = Heading(entity.Facing + HalfPi) * (halfWidth * across);
					DrawStreak(renderer, chest + side + forward * (extent * 0.2f), chest + side * 0.6f + forward * (extent * 0.85f), 0.035f, owner * (1.0f * fade));
				}
				break;
			}
			case AbilityId::GroundSlam:
				DrawSlash(renderer, base + glm::vec3(0.0f, 0.12f, 0.0f), entity.Facing, reach, def.HalfArc, swing, glm::mix(color, glm::vec3(1.0f, 0.8f, 0.5f), 0.5f) * 1.1f, mirrored, 0.2f);
				break;
			case AbilityId::Gust:
				for (uint32_t i = 0; i < 3; i++)
					DrawSlash(renderer, chest + glm::vec3(0.0f, -0.15f + 0.15f * float(i), 0.0f) + forward * (0.3f * float(i)), entity.Facing, reach * (0.55f + 0.2f * float(i)) * (0.6f + 0.4f * swing), def.HalfArc, swing, color * (1.6f - 0.35f * float(i)), i % 2 == 0, 0.6f);
				break;
			default:
				DrawSlash(renderer, chest, entity.Facing, reach * 0.85f, def.HalfArc, swing, glm::mix(owner, glm::vec3(1.0f, 0.9f, 0.75f), ability == AbilityId::Chop ? 0.6f : 0.45f) * 1.25f, mirrored);
				break;
			}
			break;
		}
		case AbilityKind::Projectile:
		case AbilityKind::Ground:
			if (ability == AbilityId::RainOfArrows)
				if (const std::optional<uint32_t> arrow = m_Characters->ArrowMesh(AbilityId::Arrow))
				{
					const float radius = def.ImpactRadius * scale;
					for (uint32_t i = 0; i < 9; i++)
					{
						const float angle = float(i) * 2.4f;
						const glm::vec3 tip = base + Heading(angle) * (radius * std::sqrt(float(i) / 9.0f)) + glm::vec3(0.0f, 0.15f + 1.6f * (1.0f - swing), 0.0f);
						const glm::mat4 transform = glm::translate(glm::mat4(1.0f), tip) * glm::rotate(glm::mat4(1.0f), -angle, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), HalfPi, glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.55f));
						renderer.DrawModel(*arrow, transform, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.25f, base.y);
					}
				}
			break;
		case AbilityKind::Heal:
			for (uint32_t i = 0; i < 3; i++)
			{
				const float rise = std::fmod(t + float(i) * 0.33f, 1.0f);
				DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.2f + 1.3f * rise, 0.0f), 0.55f - 0.15f * rise, color * (2.2f * (1.0f - rise)), 0.035f);
			}
			break;
		case AbilityKind::Dash:
			DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.5f, 0.0f), 0.3f + 0.6f * t, color * (2.0f * (1.0f - t)), 0.04f);
			break;
		}
	}

	void SessionState::DrawUi()
	{
		if (m_Leaving)
			return;
		Ui& ui = m_App.GetUi();
		if (m_Client.GetState() != ClientState::InGame)
		{
			DrawConnecting();
			return;
		}
		ui.SetLayer(UiLayer::World);
		const GameStatus& status = m_Client.GetStatus();
		if (status.Mode == GameMode::Royale && status.Match == MatchPhase::Fight && m_Client.GetPredictedState().Alive)
			if (const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId()); self && !status.Storm.Contains(self->Position))
			{
				UiDrawList& tint = ui.Draw();
				const float throb = 0.75f + 0.25f * std::sin(m_Time * 4.0f);
				tint.Rect(ui.GetScreen(), { .Fill = Rgba(0.25f, 0.05f, 0.35f, 0.08f), .Glow = Rgba(0.5f, 0.12f, 0.65f, 0.6f * throb), .GlowSize = 180.0f });
				tint.TextAligned({ 0.0f, ui.GetSize().y * 0.2f, ui.GetSize().x, 24.0f }, "You are in the ash storm! Get back inside the circle", Theme::Bold(18.0f, Hex(0xE7B8FF)), { 0.5f, 0.5f });
			}
		DrawLavaWarning();
		DrawWorldUi();
		ui.SetLayer(UiLayer::Hud);
		DrawOverlay();
		DrawStatus();
		DrawHud();
		DrawConnectionWarning();
		DrawBanner();
		ui.SetLayer(UiLayer::Windows);
		DrawUpgradeChoice();
		if (m_ShowInventory)
			DrawInventory();
		DrawStationWindow();
		DrawBrawlLoadout();
		DrawMatchResults();
		if (m_Paused)
		{
			ui.SetLayer(UiLayer::Popup);
			DrawPauseMenu();
		}
		if (m_ShowNetStats)
			DrawNetworkWindow();

		ui.SetLayer(UiLayer::Overlay);
		UiDrawList& draw = ui.Draw();
		if (m_HintTime > 0.0f && !m_Hint.empty())
		{
			const float alpha = std::clamp(m_HintTime / 0.4f, 0.0f, 1.0f);
			const TextStyle style = Theme::Bold(17.0f, WithAlpha(Theme::AccentLight, alpha));
			const float width = draw.MeasureWidth(m_Hint, style) + 40.0f;
			const UiRect pill = UiRect::Centered({ ui.GetSize().x * 0.5f, ui.GetSize().y * 0.64f }, { width, 36.0f });
			draw.Rect(pill.Translate({ 0.0f, 4.0f }), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.5f * alpha), .Softness = 10.0f, .Chamfer = 10.0f });
			draw.Rect(pill, { .Fill = Rgba(0.06f, 0.045f, 0.11f, 0.9f * alpha), .FillBottom = Rgba(0.03f, 0.024f, 0.06f, 0.9f * alpha), .Border = WithAlpha(Theme::Edge, alpha), .BorderWidth = 1.2f, .Chamfer = 9.0f });
			draw.TextAligned(pill, m_Hint, style, { 0.5f, 0.5f });
		}
		if (m_Fade > 0.0f)
			draw.Rect(ui.GetScreen(), { .Fill = Rgba(0.0f, 0.0f, 0.0f, 0.9f * m_Fade) });
	}

	void SessionState::DrawConnecting()
	{
		Ui& ui = m_App.GetUi();
		ui.SetMotion(1.0f);
		ui.SetLayer(UiLayer::Windows);
		UiDrawList& draw = ui.Draw();
		const glm::vec2 center = ui.GetSize() * 0.5f + glm::vec2(0.0f, -40.0f);
		const float time = ui.GetTime();
		draw.Rect(UiRect::Centered(center, glm::vec2(330.0f)), { .Fill = Rgba(0.01f, 0.008f, 0.03f, 0.6f), .Radius = 165.0f, .Softness = 60.0f });
		draw.Sigil(center, 112.0f, { .Color = WithAlpha(Theme::AccentLight, 0.75f), .Accent = WithAlpha(Theme::Astral, 0.85f), .Speed = 3.0f, .Phase = 0.21f, .LineWidth = 1.2f, .Glow = 0.8f, .Points = 7, .Skip = 3 });
		for (int i = 0; i < 3; i++)
		{
			const float angle = time * 1.7f + float(i) * 2.0943951f;
			const glm::vec2 spark = center + glm::vec2(std::cos(angle), std::sin(angle)) * 104.0f;
			draw.Rect(UiRect::Centered(spark, glm::vec2(22.0f)), { .Fill = WithAlpha(Theme::Astral, 0.5f), .Radius = 11.0f, .Softness = 8.0f, .Additive = 1.0f });
			draw.Rect(UiRect::Centered(spark, glm::vec2(4.0f)), { .Fill = Rgba(0.9f, 1.0f, 1.0f, 1.0f), .Radius = 2.0f, .Additive = 1.0f });
		}
		TextStyle title = Theme::Title(30.0f);
		title.Tracking = 4.0f;
		title.Shimmer = 0.5f;
		draw.TextAligned({ 0.0f, center.y + 136.0f, ui.GetSize().x, 40.0f }, m_Config.Host ? "Opening the Rift" : "Following the Rift", title, { 0.5f, 0.5f });
		const std::string detail = m_Config.Host ? "Binding UDP port " + std::to_string(m_Config.Port) : "Reaching " + m_Config.Address;
		draw.TextAligned({ 0.0f, center.y + 178.0f, ui.GetSize().x, 24.0f }, detail, Theme::Body(16.0f, Theme::Muted), { 0.5f, 0.5f });
		if (ui.MenuEntry(Ui::MakeId("cancel"), UiRect::Centered({ center.x, center.y + 238.0f }, { 320.0f, 44.0f }), "Turn Back", {}, false, 0.5f))
			ReturnToMenu({});
	}

	void SessionState::DrawWorldUi()
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const glm::vec2 size = ui.GetSize();
		const auto screen = [&](const glm::vec3& position) -> std::optional<glm::vec2>
		{
			const std::optional<glm::vec2> pixels = ProjectToScreen(position);
			return pixels ? std::optional<glm::vec2>(ui.ToLogical(*pixels)) : std::nullopt;
		};
		const auto onScreen = [&](const glm::vec2& point) { return point.x > -60.0f && point.y > -60.0f && point.x < size.x + 60.0f && point.y < size.y + 60.0f; };
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		const bool packFull = m_Client.HasInventory() && IsBackpackFull(m_Client.GetInventory());
		const float pulse = 0.5f + 0.5f * std::sin(m_Time * 3.2f);

		struct LootLabel
		{
			const EntityState* Entity = nullptr;
			glm::vec2 Anchor{ 0.0f };
			UiRect Pill;
		};
		std::vector<LootLabel> labels;
		for (const EntityState& entity : m_Client.GetEntities())
			if (IsItemLoot(entity))
				if (const std::optional<glm::vec2> point = screen(Ground(entity.Position, 1.4f)); point && onScreen(*point))
					labels.push_back({ &entity, *point });
		std::sort(labels.begin(), labels.end(), [](const LootLabel& a, const LootLabel& b) { return a.Anchor.y > b.Anchor.y; });
		std::vector<UiRect> placed;
		for (LootLabel& label : labels)
		{
			const EntityState& entity = *label.Entity;
			const ItemBase base = ItemTagBase(entity.Tag);
			const bool gem = base == ItemBase::Gem;
			const Rarity rarity = ItemTagRarity(entity.Tag);
			const AbilityId skill = ItemTagSkill(entity.Tag);
			const UiColor color = ItemColor(base, rarity, skill);
			const std::string name = gem ? std::string(GetAbility(skill).Name) + " Gem" : std::string(GetRarity(rarity).Name) + " " + GetItemBase(base).Name;
			const TextStyle style{ .Font = UiFont::Bold, .Size = 15.0f, .Color = Lighten(color, 0.2f), .ShadowOffset = { 0.0f, 1.0f }, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.8f), .ShadowBlur = 1.5f };
			const UiImage* emblem = gem ? nullptr : RarityGem(rarity);
			const float marker = emblem ? 22.0f : 18.0f;
			UiRect pill = UiRect::Centered({ label.Anchor.x, label.Anchor.y - 14.0f }, { draw.MeasureWidth(name, style) + marker + 30.0f, 28.0f });
			for (int attempt = 0; attempt < 16; attempt++)
			{
				const auto blocker = std::find_if(placed.begin(), placed.end(), [&](const UiRect& other) { return other.Expand(2.0f).Intersect(pill).W > 0.0f && other.Expand(2.0f).Intersect(pill).H > 0.0f; });
				if (blocker == placed.end())
					break;
				pill.Y = blocker->Y - pill.H - 3.0f;
			}
			placed.push_back(pill);
			if (pill.Bottom() < label.Anchor.y - 2.0f)
				draw.Line(label.Anchor, { label.Anchor.x, pill.Bottom() }, 1.2f, WithAlpha(color, 0.6f));
			if (!gem && rarity >= Rarity::Legendary)
				draw.Shadow(pill, 14.0f, 10.0f, WithAlpha(color, 0.35f + 0.25f * pulse), { 0.0f, 0.0f });
			draw.Rect(pill, { .Fill = Rgba(0.05f, 0.04f, 0.1f, 0.9f), .FillBottom = Rgba(0.025f, 0.02f, 0.05f, 0.9f), .Border = WithAlpha(color, 0.9f), .BorderWidth = 1.2f, .Chamfer = 7.0f });
			const UiRect markerRect{ pill.X + 10.0f, pill.Center().y - marker * 0.5f, marker, marker };
			if (emblem)
				draw.Image(*emblem, markerRect);
			else
				draw.Icon(gem ? IconFor(skill) : IconFor(base), markerRect, { .Color = Lighten(color, 0.4f), .ColorBottom = color, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.7f) });
			draw.TextAligned({ markerRect.Right() + 7.0f, pill.Y, pill.Right() - markerRect.Right() - 7.0f, pill.H }, name, style, { 0.0f, 0.5f });
			if (packFull && self && LengthSquared(self->Position - entity.Position) < 6.0f)
				draw.TextAligned({ pill.X - 40.0f, label.Anchor.y + 2.0f, pill.W + 80.0f, 18.0f }, "Backpack full", Theme::Floating(14.0f, Theme::Danger), { 0.5f, 0.0f });
		}

		for (const EntityState& entity : m_Client.GetEntities())
		{
			if (IsItemLoot(entity) || IsBossKind(entity.Kind))
				continue;
			const bool enemy = IsEnemyKind(entity.Kind);
			const bool elite = entity.Has(EntityFlags::Elite);
			const bool teammate = entity.Kind == EntityKind::Player && entity.NetId != m_Client.GetPlayerNetId() && !entity.Has(EntityFlags::Dead);
			if (!(enemy && (entity.Health < 255 || elite)) && !teammate)
				continue;
			const std::optional<glm::vec2> point = screen(Ground(entity.Position, BarHeight(entity)));
			if (!point || !onScreen(*point))
				continue;
			const bool rival = m_Client.IsHostile(entity);
			const float width = (entity.Kind == EntityKind::Brute ? 70.0f : rival ? 58.0f : 46.0f) * (elite ? 1.25f : 1.0f);
			const float height = elite || rival ? 7.0f : 6.0f;
			const UiRect bar = UiRect::Centered(*point - glm::vec2(0.0f, height * 0.5f), { width, height });
			const float fraction = float(entity.Health) / 255.0f;
			const UiColor color = rival ? Hex(0xE04A3C) : !enemy ? WithAlpha(ColorOf(entity.Owner)) : elite ? WithAlpha(EliteGold) : Hex(0xE04A3C);
			draw.Rect(bar.Expand(1.5f), { .Fill = Rgba(0.0f, 0.0f, 0.0f, 0.78f), .Radius = 3.5f, .Border = elite ? WithAlpha(EliteGold, 0.9f) : UiColor(0.0f), .BorderWidth = elite ? 1.0f : 0.0f });
			draw.Rect({ bar.X, bar.Y, bar.W * fraction, bar.H }, { .Fill = Lighten(color, 0.3f), .FillBottom = Darken(color, 0.25f), .Radius = 2.5f });
			if (elite)
				draw.Icon(UiIcon::Crown, { bar.X - 19.0f, bar.Center().y - 8.0f, 16.0f, 16.0f }, { .Color = WithAlpha(EliteGold), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.8f) });
			if (!teammate)
				continue;
			if (const PlayerInfo* player = m_Client.FindPlayer(entity.Owner))
				draw.TextAligned({ point->x - 120.0f, bar.Y - 22.0f, 240.0f, 18.0f }, player->Name, Theme::Floating(14.0f, rival ? Hex(0xFF9A8A) : Lighten(WithAlpha(ColorOf(entity.Owner)), 0.45f)), { 0.5f, 1.0f });
		}

		if (const Level* level = m_Client.GetLevel(); level && self && !m_Overview)
			for (size_t i = 0; i < level->Portals.size(); i++)
			{
				const Portal& portal = level->Portals[i];
				const bool hovered = int32_t(i) == m_HoveredPortal;
				const bool reachable = int32_t(i) == m_ReachablePortal;
				if (!hovered && !reachable && LengthSquared(portal.Position - self->Position) > 100.0f)
					continue;
				const std::optional<glm::vec2> point = screen(Ground(portal.Position, 2.4f));
				if (!point || !onScreen(*point))
					continue;
				const std::string title = "To the " + portal.Label;
				const char* action = reachable ? "Click or press E" : hovered ? "Get closer to use" : nullptr;
				const TextStyle titleStyle = Theme::Bold(15.0f, Theme::AccentLight);
				const TextStyle actionStyle = Theme::Body(13.0f, Theme::Text);
				const float width = std::max(draw.MeasureWidth(title, titleStyle), action ? draw.MeasureWidth(action, actionStyle) : 0.0f) + 54.0f;
				const float height = action ? 46.0f : 30.0f;
				const UiRect box{ point->x - width * 0.5f, point->y - height, width, height };
				const float strength = hovered || reachable ? 1.0f : 0.7f;
				draw.Rect(box.Translate({ 0.0f, 3.0f }), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.5f), .Softness = 8.0f, .Chamfer = 8.0f });
				draw.Rect(box, { .Fill = Rgba(0.06f, 0.05f, 0.11f, 0.92f), .FillBottom = Rgba(0.03f, 0.025f, 0.06f, 0.92f), .Border = WithAlpha(Theme::Accent, strength), .BorderWidth = hovered ? 1.8f : 1.2f, .Glow = WithAlpha(Theme::Accent, hovered ? 0.12f : 0.0f), .GlowSize = 10.0f, .Chamfer = 8.0f});
				draw.Icon(UiIcon::Door, { box.X + 9.0f, box.Y + 6.0f, 18.0f, 18.0f }, { .Color = Theme::AccentLight, .ColorBottom = Theme::Accent });
				draw.Text({ box.X + 34.0f, box.Y + 5.0f }, title, titleStyle);
				if (action)
					draw.Text({ box.X + 34.0f, box.Y + 25.0f }, action, actionStyle);
			}

		const PlayerState& state = m_Client.GetPredictedState();
		if (self && state.CastSlot >= 0)
			if (const std::optional<glm::vec2> point = screen(Ground(self->Position)))
			{
				const SkillTraits& skill = m_Client.GetTraits().Skills[size_t(state.CastSlot)];
				const float progress = skill.CastTime > 0.0f ? 1.0f - std::clamp(state.CastTimer / skill.CastTime, 0.0f, 1.0f) : 1.0f;
				const UiColor color = WithAlpha(SkillColor(skill.Ability));
				const UiRect bar = UiRect::Centered({ point->x + 11.0f, point->y + 30.0f }, { 116.0f, 9.0f });
				draw.Rect(bar.Expand(1.5f), { .Fill = Rgba(0.0f, 0.0f, 0.0f, 0.8f), .Radius = 5.0f, .Border = WithAlpha(color, 0.6f), .BorderWidth = 1.0f });
				draw.Rect({ bar.X, bar.Y, bar.W * progress, bar.H }, { .Fill = Lighten(color, 0.45f), .FillBottom = color, .Radius = 4.0f });
				draw.Shadow({ bar.X + bar.W * progress - 3.0f, bar.Y, 6.0f, bar.H }, 3.0f, 5.0f, WithAlpha(Lighten(color, 0.6f), 0.8f), { 0.0f, 0.0f });
				draw.Icon(IconFor(skill.Ability), { bar.X - 26.0f, bar.Center().y - 11.0f, 22.0f, 22.0f }, { .Color = Lighten(color, 0.5f), .ColorBottom = color, .Outline = 1.0f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.85f) });
				const std::string label = std::string(GetAbility(skill.Ability).Name) + (skill.MoveScale <= 0.01f ? "  (rooted)" : "");
				draw.TextAligned({ bar.X - 60.0f, bar.Bottom() + 3.0f, bar.W + 120.0f, 18.0f }, label, Theme::Floating(13.5f, Theme::Text), { 0.5f, 0.0f });
			}

		for (const DamageNumber& number : m_DamageNumbers)
		{
			const float t = number.Age / DamageNumberSeconds;
			const std::optional<glm::vec2> point = screen(Ground(number.Position + Vec2(number.Drift, 0.0f), 1.3f + 1.4f * t));
			if (!point || !onScreen(*point))
				continue;
			const float pop = number.Critical ? 1.0f + 0.7f * std::max(0.0f, 1.0f - number.Age / 0.16f) : 1.0f + 0.25f * std::max(0.0f, 1.0f - number.Age / 0.1f);
			const float fontSize = (number.Critical ? 27.0f : number.OnPlayer ? 21.0f : 18.0f) * pop;
			const float alpha = std::clamp((1.0f - t) / 0.35f, 0.0f, 1.0f);
			UiColor color = number.OnPlayer ? Hex(0xFF5A4E) : number.Critical ? Hex(0xFFE27A) : number.Damages != Element::Physical ? Lighten(WithAlpha(ElementColor(number.Damages)), 0.2f) : Hex(0xF4F1EA);
			TextStyle style = Theme::Floating(fontSize, WithAlpha(color, alpha));
			style.OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.9f * alpha);
			style.ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.5f * alpha);
			style.Outline = number.Critical ? 2.2f : 1.7f;
			if (number.Critical)
				style.ColorBottom = WithAlpha(Hex(0xFF9A1F), alpha);
			char text[16];
			std::snprintf(text, sizeof(text), number.Critical ? "%u!" : "%u", unsigned(number.Damage));
			draw.TextAligned(UiRect::Centered(*point, { 200.0f, fontSize }), text, style, { 0.5f, 0.5f });
		}

		DrawInteractionLabels();
		const GameStatus& status = m_Client.GetStatus();
		if (status.Mode != GameMode::Horde || status.Phase != GamePhase::Wave || status.EnemiesLeft > 5)
			return;
		constexpr float Margin = 30.0f;
		const glm::vec2 center = size * 0.5f;
		for (const EntityState& entity : m_Client.GetEntities())
		{
			if (!IsEnemyKind(entity.Kind) || entity.Health == 0)
				continue;
			const std::optional<glm::vec2> point = screen(Ground(entity.Position, 0.5f));
			if (!point || (std::abs(point->x - center.x) < center.x - Margin && std::abs(point->y - center.y) < center.y - Margin))
				continue;
			const glm::vec2 direction = *point - center;
			const float reach = std::min((center.x - Margin) / std::max(std::abs(direction.x), 1e-3f), (center.y - Margin) / std::max(std::abs(direction.y), 1e-3f));
			const glm::vec2 forward = glm::normalize(direction);
			const glm::vec2 tip = center + direction * reach - forward * 8.0f;
			const UiColor color = entity.Has(EntityFlags::Elite) ? WithAlpha(EliteGold) : Hex(0xEB4B3C);
			draw.Triangle(tip, { 18.0f, 20.0f }, std::atan2(forward.x, -forward.y), WithAlpha(color, 0.65f + 0.35f * pulse), Rgba(0.0f, 0.0f, 0.0f, 0.85f), 1.5f);
		}
	}

	std::string SessionState::PlayerName(uint32_t playerId) const
	{
		const PlayerInfo* player = m_Client.FindPlayer(playerId);
		return player ? player->Name : "Someone";
	}

	glm::vec3 SessionState::ColorOf(uint32_t playerId) const
	{
		if (m_Client.GetSession().Mode == GameMode::Arena && m_Client.GetSession().Rules.Teams)
			if (const uint8_t team = m_Client.GetTeam(playerId); team != NoTeam)
				return TeamColor(team);
		return PlayerColor(playerId);
	}

	void SessionState::DrawOverlay()
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		std::vector<PlayerInfo> players = m_Client.GetPlayers();
		const GameMode mode = m_Client.GetSession().Mode;
		if (mode == GameMode::Arena)
			std::stable_sort(players.begin(), players.end(), [](const PlayerInfo& a, const PlayerInfo& b) { return a.Team != b.Team ? a.Team < b.Team : a.Kills != b.Kills ? a.Kills > b.Kills : a.Deaths < b.Deaths; });
		else if (mode == GameMode::Royale)
			std::stable_sort(players.begin(), players.end(), [](const PlayerInfo& a, const PlayerInfo& b) { return (a.Placement == 0) != (b.Placement == 0) ? a.Placement == 0 : a.Placement != b.Placement ? a.Placement < b.Placement : a.Kills > b.Kills; });
		else if (mode == GameMode::Brawl)
			std::stable_sort(players.begin(), players.end(), [](const PlayerInfo& a, const PlayerInfo& b) { return a.Score != b.Score ? a.Score > b.Score : a.Rounds > b.Rounds; });
		const UiRect frame{ 14.0f, 14.0f, 318.0f, 40.0f + float(players.size()) * 30.0f };
		draw.Rect(frame.Translate({ 0.0f, 4.0f }), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.4f), .Softness = 10.0f, .Chamfer = 10.0f });
		draw.Rect(frame, { .Fill = Hex(0x120E24, 0.78f), .FillBottom = Hex(0x07060E, 0.8f), .Border = WithAlpha(Theme::Edge, 0.75f), .BorderWidth = 1.0f, .Chamfer = 10.0f, .Aether = 0.4f, .Seed = 0.13f });
		const ConnectionStats connection = m_Client.GetConnectionStats();
		char line[96];
		if (m_Server)
			std::snprintf(line, sizeof(line), "Hosting on port %u", unsigned(m_Server->GetConfig().Port));
		else
			std::snprintf(line, sizeof(line), "Connected to %s", m_Config.Address.c_str());
		draw.Text({ frame.X + 12.0f, frame.Y + 8.0f }, line, Theme::Body(13.5f, Theme::Muted));
		std::snprintf(line, sizeof(line), "%d ms   %.0f fps", connection.Ping, double(m_Fps));
		draw.TextAligned({ frame.X, frame.Y + 8.0f, frame.W - 12.0f, 18.0f }, line, Theme::Body(13.5f, Theme::Faint), { 1.0f, 0.0f });
		float y = frame.Y + 32.0f;
		for (const PlayerInfo& player : players)
		{
			const UiColor color = WithAlpha(ColorOf(player.PlayerId));
			const bool you = player.PlayerId == m_Client.GetPlayerId();
			if (mode == GameMode::Royale && player.Placement > 1)
				draw.Icon(UiIcon::SkullCrossedBones, { frame.X + 154.0f, y + 7.0f, 16.0f, 16.0f }, { .Color = Theme::Danger, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			draw.Circle({ frame.X + 18.0f, y + 15.0f }, 5.0f, color, Rgba(0.0f, 0.0f, 0.0f, 0.6f), 1.0f);
			const TextStyle nameStyle = Theme::Bold(15.0f, Lighten(color, 0.45f));
			const float nameWidth = std::min(draw.MeasureWidth(player.Name, nameStyle), 120.0f);
			draw.TextFit({ frame.X + 30.0f, y, 120.0f, 30.0f }, player.Name, nameStyle);
			if (you)
				draw.TextAligned({ frame.X + 34.0f + nameWidth, y, 40.0f, 30.0f }, "you", Theme::Body(12.5f, Theme::Faint), { 0.0f, 0.5f });
			const UiColor weapon = WithAlpha(RarityColor(player.Look.WeaponQuality));
			draw.Icon(IconFor(player.Look.Weapon), { frame.X + 178.0f, y + 6.0f, 18.0f, 18.0f }, { .Color = Lighten(weapon, 0.35f), .ColorBottom = weapon, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			if (mode == GameMode::Brawl)
				std::snprintf(line, sizeof(line), "%u pts", unsigned(player.Score));
			else
				std::snprintf(line, sizeof(line), "%u / %u", player.Kills, player.Deaths);
			draw.TextAligned({ frame.X + 202.0f, y, 60.0f, 30.0f }, line, Theme::Body(13.5f, Theme::Text), { 0.0f, 0.5f });
			std::snprintf(line, sizeof(line), "%d ms", player.Ping);
			draw.TextAligned({ frame.X, y, frame.W - 12.0f, 30.0f }, line, Theme::Body(12.5f, Theme::Faint), { 1.0f, 0.5f });
			y += 30.0f;
		}
		if (!m_App.GetSettings().ShowControls)
			return;
		draw.Text({ 16.0f, ui.GetSize().y - 46.0f }, "WASD move   LMB attack   RMB / Q / R skills   Space dodge", Theme::Floating(13.0f, Theme::Muted, UiFont::Body));
		draw.Text({ 16.0f, ui.GetSize().y - 26.0f }, mode == GameMode::Brawl ? "Tab spellbook   F4 map   F3 network   Esc menu" : "E interact   I inventory   F4 map   F3 network   Esc menu", Theme::Floating(13.0f, Theme::Muted, UiFont::Body));
	}

	void SessionState::DrawNetworkWindow()
	{
		const ConnectionStats connection = m_Client.GetConnectionStats();
		ImGui::SetNextWindowPos(ImVec2(16.0f, 220.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(0.85f);
		if (!ImGui::Begin("Network (F3)", &m_ShowNetStats, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::End();
			return;
		}
		const double loss = connection.Quality > 0.0f ? (1.0 - double(connection.Quality)) * 100.0 : 0.0;
		const double tickMs = 1000.0 / double(std::max(m_Client.GetTickRate(), 1u));
		const double delayMs = (double(m_Client.GetLatestTick()) - m_Client.GetRenderTick()) * tickMs;
		const double behindMs = std::max(m_Client.GetPresentTick() - m_Client.GetRenderTick(), 0.0) * tickMs;
		ImGui::Text("Ping %d ms, %.1f%% loss, interpolation %.0f ms, world drawn %.0f ms behind you", connection.Ping, loss, delayMs, behindMs);
		ImGui::Text("Down %.1f KB/s, up %.1f KB/s", double(connection.InBytesPerSecond) / 1024.0, double(connection.OutBytesPerSecond) / 1024.0);
		ImGui::Text("Interpolation target %.2f ticks, %u starved frames", m_Client.GetInterpolationDelay(), m_Client.GetStarvedFrames());
		ImGui::Text("%u unacknowledged inputs, %u corrections (last %.3f)", m_Client.GetPendingInputCount(), m_Client.GetCorrectionCount(), double(m_Client.GetLastCorrection()));
		ImGui::Text("%u sounds played, %u UI primitives", m_App.GetGameAudio().GetPlayedCount(), m_App.GetUiPrimitiveCount());
		const Timbre::AudioStats audio = m_App.GetAudio().GetStats();
		ImGui::Text("Audio: %u voices (%u audible, %u virtual), %u streams, %.1f%% CPU, %llu underruns, %.1f MB", audio.PlayingVoices, audio.AudibleVoices, audio.VirtualVoices, audio.Streams, double(audio.CpuLoad) * 100.0,
			static_cast<unsigned long long>(audio.StreamUnderruns), double(audio.SoundMemory) / (1024.0 * 1024.0));
		if (m_Server)
		{
			ImGui::Separator();
			ImGui::Text("Server: %u starved player ticks", m_Server->GetStarvedTicks());
			for (const ServerPlayerStats& player : m_Server->GetPlayerStats())
				ImGui::Text("  %s: ping %d ms, %.1f%% loss, input buffer %u of %u", player.Name.c_str(), player.Connection.Ping, player.Connection.Quality > 0.0f ? (1.0 - double(player.Connection.Quality)) * 100.0 : 0.0, player.QueuedInputs, player.InputBufferTarget);
		}
		ImGui::End();
	}

	void SessionState::DrawStatus()
	{
		if (IsPvpMode(m_Client.GetSession().Mode))
		{
			DrawMatchStatus();
			return;
		}
		if (m_Client.GetSession().Mode != GameMode::Horde)
		{
			DrawMissionStatus();
			return;
		}
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const GameStatus& status = m_Client.GetStatus();
		const float center = ui.GetSize().x * 0.5f;
		const unsigned seconds = unsigned(std::ceil(std::max(status.PhaseTime, 0.0f)));
		char title[64];
		char subtitle[96];
		bool danger = false;
		switch (status.Phase)
		{
		case GamePhase::Intermission:
			if (status.PhaseTime > 1e5f)
			{
				std::snprintf(title, sizeof(title), "Peaceful");
				std::snprintf(subtitle, sizeof(subtitle), "No waves will come");
			}
			else if (status.Wave == 0)
			{
				std::snprintf(title, sizeof(title), "Prepare");
				std::snprintf(subtitle, sizeof(subtitle), "The first wave arrives in %u", seconds);
			}
			else
			{
				std::snprintf(title, sizeof(title), "Wave %u Cleared", unsigned(status.Wave));
				std::snprintf(subtitle, sizeof(subtitle), "Next wave in %u", seconds);
			}
			break;
		case GamePhase::Wave:
			std::snprintf(title, sizeof(title), "Wave %u", unsigned(status.Wave));
			std::snprintf(subtitle, sizeof(subtitle), "%u %s left", unsigned(status.EnemiesLeft), status.EnemiesLeft == 1 ? "enemy" : "enemies");
			break;
		case GamePhase::Defeat:
			std::snprintf(title, sizeof(title), "Defeat");
			if (m_Server && m_Server->GetConfig().StartMode == GameMode::Horde)
				std::snprintf(subtitle, sizeof(subtitle), "Everyone fell on wave %u, restarting in %u", unsigned(status.Wave), seconds);
			else
				std::snprintf(subtitle, sizeof(subtitle), "Everyone fell on wave %u, returning to Emberhold", unsigned(status.Wave));
			danger = true;
			break;
		}
		TextStyle titleStyle = Theme::Title(30.0f);
		titleStyle.Tracking = 2.0f;
		if (danger)
		{
			titleStyle.Color = Hex(0xFF8A7A);
			titleStyle.ColorBottom = Hex(0xC23B30);
		}
		const float titleWidth = draw.MeasureWidth(title, titleStyle);
		draw.Shadow(UiRect::Centered({ center, 44.0f }, { titleWidth + 160.0f, 56.0f }), 28.0f, 30.0f, Rgba(0.0f, 0.0f, 0.0f, 0.55f), { 0.0f, 0.0f });
		draw.TextAligned({ 0.0f, 16.0f, ui.GetSize().x, 36.0f }, title, titleStyle, { 0.5f, 0.5f });
		DrawTitleOrnament(center, 33.0f, titleWidth);
		draw.TextAligned({ 0.0f, 56.0f, ui.GetSize().x, 22.0f }, subtitle, Theme::Floating(16.0f, danger ? Theme::Danger : Theme::Text, UiFont::Body), { 0.5f, 0.5f });
		if (status.Phase == GamePhase::Intermission && status.PendingChoices > 0)
		{
			if (m_Client.GetLocalPlayer().OfferPending)
				draw.TextAligned({ 0.0f, 80.0f, ui.GetSize().x, 20.0f }, "Pick an upgrade with 1, 2 or 3, the next wave waits for it", Theme::Floating(15.0f, Theme::AccentLight, UiFont::Body), { 0.5f, 0.5f });
			else
			{
				char waiting[64];
				std::snprintf(waiting, sizeof(waiting), "Waiting for %u player%s to pick an upgrade", unsigned(status.PendingChoices), status.PendingChoices == 1 ? "" : "s");
				draw.TextAligned({ 0.0f, 80.0f, ui.GetSize().x, 20.0f }, waiting, Theme::Floating(15.0f, Theme::Muted, UiFont::Body), { 0.5f, 0.5f });
			}
		}
	}

	void SessionState::DrawHud()
	{
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		if (!local.Valid)
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const PlayerState& state = m_Client.GetPredictedState();
		const PlayerTraits& traits = m_Client.GetTraits();
		const glm::vec2 size = ui.GetSize();
		const float center = size.x * 0.5f;
		constexpr float SlotSize = 62.0f;
		constexpr float SlotGap = 9.0f;
		constexpr float OrbRadius = 58.0f;
		const float barWidth = float(AbilitySlots + 1) * SlotSize + float(AbilitySlots) * SlotGap;
		const UiRect slots{ center - barWidth * 0.5f, size.y - 22.0f - SlotSize, barWidth, SlotSize };
		const UiRect plate = slots.Expand(12.0f);
		draw.Rect(plate.Translate({ 0.0f, 6.0f }).Expand(4.0f), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.55f), .Softness = 16.0f, .Chamfer = 18.0f });
		draw.Rect(plate, { .Fill = Hex(0x130F26, 0.9f), .FillBottom = Hex(0x07060E, 0.94f), .Border = WithAlpha(Theme::Edge, 0.95f), .BorderWidth = 1.3f, .Glow = WithAlpha(Theme::Accent, 0.06f), .GlowSize = 30.0f, .Chamfer = 14.0f, .Aether = 0.7f, .Seed = 0.42f });
		draw.Rect(plate.Shrink(4.0f), { .Border = WithAlpha(Theme::AccentLight, 0.06f), .BorderWidth = 1.0f, .Chamfer = 12.0f });
		draw.Diamond({ plate.Center().x, plate.Y }, 10.0f, Theme::AccentLight, WithAlpha(Theme::Ink, 0.9f), 1.2f);
		ui.Block(plate);
		DrawAbilityBar(slots);

		const auto orb = [&](float x, float fill, UiColor light, UiColor deep, UiColor rune, const std::string& text, float phase, float danger)
		{
			const glm::vec2 orbCenter(x, size.y - 18.0f - OrbRadius);
			const float frame = OrbRadius + 11.0f;
			const UiRect bounds = UiRect::Centered(orbCenter, glm::vec2(frame * 2.0f));
			draw.Rect(bounds.Translate({ 0.0f, 5.0f }).Expand(2.0f), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.6f), .Radius = frame + 2.0f, .Softness = 14.0f });
			if (danger > 0.01f)
				draw.Rect(bounds.Expand(10.0f), { .Fill = WithAlpha(rune, 0.3f * danger * (0.6f + 0.4f * std::sin(m_Time * 5.0f))), .Radius = frame + 10.0f, .Softness = 16.0f, .Additive = 1.0f });
			draw.Circle(orbCenter, frame, Hex(0x0B0918, 0.96f), WithAlpha(glm::mix(Theme::Edge, rune, danger), 0.95f), 1.3f);
			draw.Sigil(orbCenter, frame, { .Color = WithAlpha(glm::mix(Theme::AccentLight, rune, 0.5f + 0.5f * danger), 0.6f + 0.35f * danger), .Speed = 3.0f * danger, .Phase = phase, .LineWidth = 1.0f, .Glow = 0.35f + 0.6f * danger, .Layers = SigilRings });
			draw.Orb(orbCenter, OrbRadius, fill, light, deep, Rgba(0.04f, 0.03f, 0.08f, 0.95f), Rgba(1.0f, 1.0f, 1.0f, 0.3f), phase);
			draw.Circle(orbCenter, OrbRadius + 1.0f, UiColor(0.0f), WithAlpha(Theme::AccentLight, 0.55f), 1.4f);
			draw.Diamond(orbCenter - glm::vec2(0.0f, frame), 12.0f, Lighten(rune, 0.45f), WithAlpha(Theme::Ink, 0.9f), 1.2f);
			draw.TextAligned(UiRect::Centered(orbCenter, { 160.0f, 24.0f }), text, Theme::Floating(17.0f, Hex(0xFFF7FF)), { 0.5f, 0.5f });
			ui.Block(bounds);
		};
		const float healthFraction = local.MaxHealth > 0 ? float(std::max(local.Health, 0)) / float(local.MaxHealth) : 0.0f;
		const float manaFraction = traits.MaxMana > 0.0f ? state.Mana / traits.MaxMana : 0.0f;
		const float danger = state.Alive ? std::clamp((0.35f - healthFraction) / 0.25f, 0.0f, 1.0f) : 0.0f;
		orb(plate.X - OrbRadius - 8.0f, healthFraction, Hex(0xFF5F6F), Hex(0x5E0A24), Hex(0xFF7A96), std::to_string(std::max(local.Health, 0)) + " / " + std::to_string(local.MaxHealth), 0.0f, danger);
		if (m_Client.GetSession().Mode == GameMode::Brawl)
		{
			const float missing = state.Alive ? 1.0f - healthFraction : 0.0f;
			const float rise = std::clamp((missing - 0.4f) / 0.5f, 0.0f, 1.0f);
			orb(plate.Right() + OrbRadius + 8.0f, missing, Hex(0xFFB45A), Hex(0x5E2208), Hex(0xFF9A4A), "+" + std::to_string(int(std::lround(missing * MissingHealthKnockback * 100.0f))) + "% fling", 2.1f, rise);
			if (ui.Interact(Ui::MakeId("flingorb"), UiRect::Centered({ plate.Right() + OrbRadius + 8.0f, size.y - 18.0f - OrbRadius }, glm::vec2(OrbRadius * 2.0f))).Hovered)
				ui.Tooltip().Header("Fling", Hex(0xFFB45A), std::nullopt, UiIcon::Whirlwind).Text("Spells don't hurt you, but the less health you have, the further they throw you.").Muted("Lava and its afterburn are the only things that cost health.");
		}
		else
			orb(plate.Right() + OrbRadius + 8.0f, manaFraction, Hex(0x7FA8FF), Hex(0x13135E), Theme::Astral, std::to_string(int(state.Mana)) + " / " + std::to_string(int(traits.MaxMana)), 2.1f, 0.0f);

		float buffX = plate.X;
		const float buffY = plate.Y - 40.0f;
		for (size_t i = 1; i < local.Buffs.size(); i++)
		{
			if (local.Buffs[i] == 0)
				continue;
			const UiRect tile{ buffX, buffY, 32.0f, 32.0f };
			const UiColor color = WithAlpha(LootColors[std::min(i, std::size(LootColors) - 1)]);
			draw.Rect(tile, { .Fill = WithAlpha(Theme::SlotTop, 0.92f), .FillBottom = WithAlpha(Theme::SlotBottom, 0.92f), .Border = WithAlpha(color, 0.8f), .BorderWidth = 1.2f, .Glow = WithAlpha(color, 0.15f), .GlowSize = 8.0f, .Chamfer = 6.0f });
			draw.Icon(IconFor(LootKind(i)), tile.Shrink(5.0f), { .Color = Lighten(color, 0.4f), .ColorBottom = color, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			draw.TextAligned(tile.Shrink(2.0f, 0.0f).Translate({ 3.0f, 3.0f }), std::to_string(local.Buffs[i]), Theme::Floating(12.5f, Theme::Text), { 1.0f, 1.0f });
			if (ui.Interact(Ui::MakeId("buff", i), tile).Hovered)
				ui.Tooltip().Header(LootDefs[i].Name, color, std::nullopt, IconFor(LootKind(i))).Text(std::string(LootDefs[i].Effect) + " per stack").Muted(std::to_string(local.Buffs[i]) + " of " + std::to_string(LootDefs[i].MaxStacks) + " stacks, lost on defeat");
			buffX += 38.0f;
		}

		float noticeY = buffY - 10.0f;
		for (auto it = m_Notices.rbegin(); it != m_Notices.rend(); ++it)
		{
			const float alpha = std::clamp((NoticeSeconds - it->Age) / 0.6f, 0.0f, 1.0f) * std::clamp(it->Age / 0.15f, 0.0f, 1.0f);
			noticeY -= 24.0f;
			draw.TextAligned({ 0.0f, noticeY, size.x, 22.0f }, it->Text, Theme::Floating(16.0f, WithAlpha(it->Color, it->Color.a * alpha), UiFont::Bold), { 0.5f, 0.5f });
		}

		const GameStatus& match = m_Client.GetStatus();
		const bool brawlFight = match.Mode == GameMode::Brawl && match.Match != MatchPhase::Ended && match.Brawl.Phase == BrawlPhase::Fight;
		if (!state.Alive && brawlFight && m_FoughtRound != match.Brawl.Round)
		{
			TextStyle waiting = Theme::Title(40.0f);
			waiting.Tracking = 3.0f;
			draw.TextAligned({ 0.0f, size.y * 0.3f, size.x, 48.0f }, "Next Round Soon", waiting, { 0.5f, 0.5f });
			const std::string watching = local.Watching != NoPlayer ? PlayerName(local.Watching) : std::string();
			const std::string line = watching.empty() ? std::string("This round started without you. You join the next one") : "This round started without you. Spectating " + watching + ", click to switch";
			draw.TextAligned({ 0.0f, size.y * 0.3f + 52.0f, size.x, 24.0f }, line, Theme::Floating(18.0f, Theme::Text, UiFont::Body), { 0.5f, 0.5f });
		}
		else if (!state.Alive && (match.Mode != GameMode::Brawl || brawlFight))
		{
			TextStyle died = Theme::Title(56.0f);
			died.Color = Hex(0xFF7A6A);
			died.ColorBottom = Hex(0x9E2620);
			died.Tracking = 4.0f;
			died.ShadowOffset = { 0.0f, 0.0f };
			died.ShadowColor = Rgba(0.6f, 0.0f, 0.0f, 0.6f);
			died.ShadowBlur = 16.0f;
			const GameStatus& status = m_Client.GetStatus();
			const bool royaleOut = status.Mode == GameMode::Royale && status.Match != MatchPhase::Countdown;
			const bool brawlOut = status.Mode == GameMode::Brawl;
			draw.TextAligned({ 0.0f, size.y * 0.3f, size.x, 64.0f }, brawlOut ? "Burned Away" : royaleOut ? "Eliminated" : IsPvpMode(status.Mode) ? "Slain" : "You Died", died, { 0.5f, 0.5f });
			char respawn[160];
			respawn[0] = 0;
			const std::string killer = local.KilledBy != NoPlayer ? PlayerName(local.KilledBy) : std::string();
			if (brawlOut)
			{
				const std::string watching = local.Watching != NoPlayer ? PlayerName(local.Watching) : std::string();
				const std::string cause = killer.empty() ? std::string("The lava got you") : killer + " shoved you into the lava";
				if (!watching.empty())
					std::snprintf(respawn, sizeof(respawn), "%s. Spectating %s, click to switch. You're back next round", cause.c_str(), watching.c_str());
				else
					std::snprintf(respawn, sizeof(respawn), "%s. You're back next round", cause.c_str());
			}
			else if (royaleOut)
			{
				const std::string watching = local.Watching != NoPlayer ? PlayerName(local.Watching) : std::string();
				if (!watching.empty() && status.Match == MatchPhase::Fight)
					std::snprintf(respawn, sizeof(respawn), "%s%s%sSpectating %s, click to switch", killer.empty() ? "" : "Slain by ", killer.c_str(), killer.empty() ? "" : ". ", watching.c_str());
				else if (!killer.empty())
					std::snprintf(respawn, sizeof(respawn), "Slain by %s", killer.c_str());
			}
			else if (IsPvpMode(status.Mode))
			{
				if (status.Match == MatchPhase::Ended)
					std::snprintf(respawn, sizeof(respawn), "%s%s", killer.empty() ? "" : "Slain by ", killer.c_str());
				else
					std::snprintf(respawn, sizeof(respawn), "%s%s%sBack in %.1f", killer.empty() ? "" : "Slain by ", killer.c_str(), killer.empty() ? "" : ". ", double(local.RespawnTime));
			}
			else if (status.Mode == GameMode::Mission && status.Mission == MissionPhase::Failed)
				std::snprintf(respawn, sizeof(respawn), "The mission is lost");
			else if (status.Mode == GameMode::Mission && status.Revives == 0 && status.Mission != MissionPhase::Victory)
				std::snprintf(respawn, sizeof(respawn), "No revives left, your party fights on without you");
			else if (local.RespawnTime > 0.0f)
				std::snprintf(respawn, sizeof(respawn), status.Mode == GameMode::Mission ? "Reviving in %.1f, it costs one of the party's revives" : "Respawning in %.1f", double(local.RespawnTime));
			else if (status.Phase != GamePhase::Defeat)
				std::snprintf(respawn, sizeof(respawn), "Reviving...");
			draw.TextAligned({ 0.0f, size.y * 0.3f + 66.0f, size.x, 24.0f }, respawn, Theme::Floating(18.0f, Theme::Text, UiFont::Body), { 0.5f, 0.5f });
		}

		const GameMode mode = m_Client.GetSession().Mode;
		if (mode != GameMode::Horde && !IsPvpMode(mode))
		{
			const UiRect strip{ size.x - 374.0f, 14.0f, 360.0f, 34.0f };
			draw.Rect(strip, { .Fill = Hex(0x120E24, 0.8f), .FillBottom = Hex(0x07060E, 0.82f), .Border = WithAlpha(Theme::Edge, 0.75f), .BorderWidth = 1.0f, .Chamfer = 9.0f, .Aether = 0.4f, .Seed = 0.61f });
			DrawMaterials(strip.Shrink(12.0f, 0.0f), true);
		}
		if (mode == GameMode::Royale)
			DrawMinimap();
		if (IsPvpMode(mode))
			DrawKillFeed();
	}

	void SessionState::DrawAbilityBar(const UiRect& area)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const PlayerState& state = m_Client.GetPredictedState();
		const PlayerTraits& traits = m_Client.GetTraits();
		const float slotSize = area.H;
		const float gap = (area.W - slotSize * float(AbilitySlots + 1)) / float(AbilitySlots);
		for (uint32_t slot = 0; slot <= AbilitySlots; slot++)
		{
			const bool dodge = slot == AbilitySlots;
			const SkillTraits& skill = dodge ? traits.Dodge : traits.Skills[slot];
			const UiRect rect{ area.X + float(slot) * (slotSize + gap), area.Y, slotSize, slotSize };
			const UiInteraction interaction = ui.Interact(Ui::MakeId("ability", slot), rect);
			const bool empty = skill.IsEmpty();
			const float cooldown = dodge ? state.DodgeCooldown : state.Cooldowns[slot];
			const float ready = skill.Cooldown > 0.0f ? 1.0f - std::clamp(cooldown / skill.Cooldown, 0.0f, 1.0f) : 1.0f;
			const bool affordable = state.Mana + 1e-3f >= skill.ManaCost;
			const bool casting = state.CastSlot == int8_t(slot);
			const bool active = !empty && skill.Usable && affordable;
			const UiColor color = empty ? Theme::Faint : dodge ? Hex(0xC9D4E8) : WithAlpha(SkillColor(skill.Ability));
			if (casting)
				draw.Shadow(rect, 9.0f, 9.0f, WithAlpha(Lighten(color, 0.4f), 0.75f), { 0.0f, 0.0f });
			UiColor border = WithAlpha(color, active ? 0.9f : 0.4f);
			if (casting)
				border = Hex(0xFFF6DD);
			else if (interaction.Hovered)
				border = Lighten(border, 0.35f);
			draw.Rect(rect, { .Fill = Theme::SlotTop, .FillBottom = Theme::SlotBottom, .Border = border, .BorderWidth = casting ? 2.2f : 1.4f, .Glow = WithAlpha(color, active ? 0.16f : 0.0f), .GlowSize = 14.0f, .Chamfer = 10.0f, .Seed = float(slot) * 0.21f });
			if (empty)
				draw.Icon(UiIcon::Emerald, rect.Shrink(17.0f), { .Color = Rgba(0.62f, 0.56f, 0.9f, 0.2f) });
			else
			{
				const UiColor top = active ? Lighten(color, 0.55f) : Hex(0x8A86A0);
				const UiColor bottom = active ? color : Hex(0x4A4760);
				draw.Icon(IconFor(skill.Ability), rect.Shrink(10.0f), { .Color = top, .ColorBottom = bottom, .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			}
			if (ready < 1.0f)
			{
				draw.Rect(rect.Shrink(1.5f), { .Fill = Rgba(0.02f, 0.015f, 0.05f, 0.74f), .Sweep = ready, .Chamfer = 9.0f });
				if (cooldown > 0.25f)
				{
					char text[16];
					std::snprintf(text, sizeof(text), cooldown < 3.0f ? "%.1f" : "%.0f", double(cooldown));
					draw.TextAligned(rect, text, Theme::Floating(19.0f, Hex(0xF4F0FF)), { 0.5f, 0.5f });
				}
			}
			if (!empty && !skill.Usable)
			{
				draw.Rect(rect.Shrink(1.5f), { .Fill = Rgba(0.45f, 0.05f, 0.12f, 0.35f), .Chamfer = 9.0f });
				draw.Icon(UiIcon::Padlock, UiRect::Centered(rect.Center(), { 26.0f, 26.0f }), { .Color = Hex(0xFF8A7A), .ColorBottom = Hex(0xB83A30), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.8f) });
			}
			const char* key = dodge ? "Space" : SlotKeys[slot];
			const TextStyle keyStyle = Theme::Bold(11.5f, Theme::AccentLight);
			const float keyWidth = draw.MeasureWidth(key, keyStyle) + 10.0f;
			const UiRect badge{ rect.X + 3.0f, rect.Bottom() - 17.0f, keyWidth, 15.0f };
			draw.Rect(badge, { .Fill = Rgba(0.03f, 0.025f, 0.06f, 0.94f), .Border = WithAlpha(Theme::Edge, 0.9f), .BorderWidth = 1.0f, .Chamfer = 3.0f });
			draw.TextAligned(badge, key, keyStyle, { 0.5f, 0.5f });
			if (!empty && skill.ManaCost > 0.0f)
			{
				char cost[16];
				std::snprintf(cost, sizeof(cost), "%.0f", double(skill.ManaCost));
				draw.TextAligned(rect.Shrink(5.0f, 3.0f), cost, Theme::Floating(13.0f, affordable ? Hex(0x9DBBFF) : Theme::Danger), { 1.0f, 1.0f });
			}
			if (interaction.Hovered)
			{
				UiTooltip& tooltip = ui.Tooltip();
				if (dodge)
					tooltip.Muted("Dodge roll on Space, it cancels a cast without spending its mana").Space(4.0f);
				DescribeSkill(tooltip, skill, traits);
			}
		}
	}

	void SessionState::DrawUpgradeChoice()
	{
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		if (!local.Valid || !local.OfferPending)
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const ServerInventory& inventory = m_Client.GetInventory();
		constexpr float CardWidth = 250.0f;
		constexpr float CardHeight = 178.0f;
		constexpr float CardGap = 20.0f;
		uint32_t count = 0;
		for (uint32_t i = 0; i < UpgradeChoices; i++)
			count += local.Offer[i] != UpgradeId::Count ? 1u : 0u;
		const float total = float(count) * CardWidth + float(std::max(count, 1u) - 1) * CardGap;
		const float top = ui.GetSize().y * 0.2f;
		const float center = ui.GetSize().x * 0.5f;
		draw.Shadow(UiRect::Centered({ center, top + 110.0f }, { total + 120.0f, 260.0f }), 40.0f, 60.0f, Rgba(0.0f, 0.0f, 0.0f, 0.55f), { 0.0f, 0.0f });
		draw.TextAligned({ 0.0f, top - 4.0f, ui.GetSize().x, 36.0f }, "Choose an Upgrade", Theme::Title(30.0f), { 0.5f, 0.5f });
		draw.TextAligned({ 0.0f, top + 32.0f, ui.GetSize().x, 20.0f }, m_ChosenUpgrade ? "Sending your choice..." : "Press 1, 2 or 3, or click a card", Theme::Floating(15.0f, Theme::Muted, UiFont::Body), { 0.5f, 0.5f });
		float x = center - total * 0.5f;
		uint32_t shown = 0;
		for (uint32_t i = 0; i < UpgradeChoices; i++)
		{
			if (local.Offer[i] == UpgradeId::Count)
				continue;
			const UpgradeDef& upgrade = GetUpgrade(local.Offer[i]);
			const UiRect card{ x + float(shown) * (CardWidth + CardGap), top + 64.0f, CardWidth, CardHeight };
			shown++;
			const UiId id = Ui::MakeId("upgrade", i);
			const UiInteraction interaction = ui.Interact(id, card);
			if (interaction.Clicked)
				PickUpgrade(i);
			const bool chosen = m_ChosenUpgrade && *m_ChosenUpgrade == i;
			const float hover = ui.Animate(id, chosen || (!m_ChosenUpgrade && interaction.Hovered));
			const UiRect lifted = card.Translate({ 0.0f, -4.0f * hover });
			draw.Shadow(lifted, 10.0f, 14.0f + 8.0f * hover, Rgba(0.0f, 0.0f, 0.0f, 0.6f), { 0.0f, 6.0f });
			if (hover > 0.01f)
				draw.Shadow(lifted, 10.0f, 12.0f, WithAlpha(Theme::Accent, 0.3f * hover), { 0.0f, 0.0f });
			draw.Rect(lifted, { .Fill = glm::mix(Theme::CardTop, Hex(0x251C4A, 0.95f), hover), .FillBottom = glm::mix(Theme::CardBottom, Hex(0x110C27, 0.96f), hover), .Border = glm::mix(WithAlpha(Theme::Edge, 0.9f), Theme::Gold, hover), .BorderWidth = 1.4f + 0.6f * hover, .Glow = WithAlpha(Theme::Accent, 0.06f + 0.1f * hover), .GlowSize = 22.0f, .Chamfer = 12.0f, .Aether = 0.4f + 0.4f * hover, .Seed = float(i) * 0.31f });
			const UiRect icon = UiRect::Centered({ lifted.Center().x, lifted.Y + 38.0f }, { 48.0f, 48.0f });
			draw.Sigil(icon.Center(), 34.0f, { .Color = WithAlpha(glm::mix(Theme::AccentLight, Theme::Gold, hover), 0.35f + 0.4f * hover), .Accent = WithAlpha(Theme::Astral, 0.4f + 0.4f * hover), .Speed = 0.15f + 1.2f * hover, .Phase = float(i) * 0.27f, .LineWidth = 1.0f, .Glow = 0.3f + 0.5f * hover, .Layers = SigilRings | SigilRunes });
			draw.Icon(IconFor(upgrade.Id), icon.Shrink(4.0f), { .Color = Theme::AccentLight, .ColorBottom = Theme::Accent, .Outline = 0.8f, .OutlineColor = Rgba(0.0f, 0.0f, 0.02f, 0.7f), .ShadowColor = Rgba(0.0f, 0.0f, 0.02f, 0.6f), .GlowColor = WithAlpha(Theme::Accent, 0.3f * hover) });
			draw.TextAligned({ lifted.X, lifted.Y + 68.0f, lifted.W, 24.0f }, upgrade.Name, Theme::Bold(19.0f, Theme::AccentLight), { 0.5f, 0.5f });
			draw.TextWrapped({ lifted.X + 16.0f, lifted.Y + 96.0f, lifted.W - 32.0f, 44.0f }, upgrade.Description, Theme::Body(15.0f, Theme::Text), 0.5f);
			char footer[48];
			std::snprintf(footer, sizeof(footer), "taken %u of %u", unsigned(inventory.Upgrades[size_t(upgrade.Id)]), unsigned(upgrade.MaxPicks));
			const UiRect badge{ lifted.X + 14.0f, lifted.Bottom() - 30.0f, 24.0f, 20.0f };
			draw.Rect(badge, { .Fill = Rgba(0.04f, 0.035f, 0.05f, 0.95f), .Radius = 4.0f, .Border = WithAlpha(Theme::Accent, 0.8f), .BorderWidth = 1.0f });
			draw.TextAligned(badge, std::to_string(i + 1), Theme::Bold(13.0f, Theme::AccentLight), { 0.5f, 0.5f });
			draw.TextAligned({ badge.Right() + 8.0f, badge.Y, lifted.Right() - badge.Right() - 22.0f, badge.H }, footer, Theme::Body(13.5f, Theme::Muted), { 1.0f, 0.5f });
		}
	}

	void SessionState::DrawInventory()
	{
		Ui& ui = m_App.GetUi();
		const ServerInventory& inventory = m_Client.GetInventory();
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		const PlayerTraits current = ComputeTraits(inventory.Gear, inventory.Gems, local.Buffs, inventory.Upgrades);
		const float currentRating = RateItem(current);
		const Item& weapon = inventory.Gear[size_t(GearSlot::Weapon)];
		const uint8_t affinity = weapon.IsValid() ? GetItemBase(weapon.Base).Affinity : 0;
		constexpr float GearSize = 72.0f;
		constexpr float GearGap = 10.0f;
		constexpr float PackSize = 62.0f;
		constexpr float PackGap = 8.0f;
		constexpr uint32_t Columns = 4;
		constexpr float ColumnGap = 30.0f;
		const float dollWidth = 3.0f * GearSize + 2.0f * GearGap;
		const float packWidth = float(Columns) * PackSize + float(Columns - 1) * PackGap;
		const float width = dollWidth + ColumnGap + packWidth + 36.0f;
		const float height = 862.0f;
		const UiRect home{ ui.GetSize().x - width - 24.0f, std::max((ui.GetSize().y - height) * 0.5f, 12.0f), width, height };
		UiRect content = ui.Window(Ui::MakeId("inventory"), "Inventory", home, &m_ShowInventory);
		UiDrawList& draw = ui.Draw();
		const UiPayload* drag = ui.GetDrag();

		UiRect top = content.CutTop(450.0f);
		UiRect left = top.CutLeft(dollWidth);
		top.CutLeft(ColumnGap);
		UiRect right = top;

		ui.Separator(left.CutTop(20.0f), "EQUIPPED");
		left.CutTop(10.0f);
		constexpr GearSlot Layout[3][3] = {
			{ GearSlot::Ring1, GearSlot::Head, GearSlot::Amulet },
			{ GearSlot::Weapon, GearSlot::Body, GearSlot::Ring2 },
			{ GearSlot::Count, GearSlot::Feet, GearSlot::Count },
		};
		for (size_t row = 0; row < 3; row++)
			for (size_t column = 0; column < 3; column++)
			{
				const GearSlot slot = Layout[row][column];
				if (slot == GearSlot::Count)
					continue;
				const UiRect rect{ left.X + float(column) * (GearSize + GearGap), left.Y + float(row) * (GearSize + GearGap), GearSize, GearSize };
				const UiId id = Ui::MakeId("gear", uint64_t(slot));
				const Item& item = inventory.Gear[size_t(slot)];
				const UiInteraction interaction = ui.Interact(id, rect);
				const bool target = drag && drag->Type == PackPayload && CanEquip(inventory.Pack[size_t(drag->Value)], slot);
				DrawItemSlot(ui, rect, item, { .Hovered = interaction.Hovered, .Target = target, .EmptyIcon = IconFor(slot), .EmptyLabel = GetGearSlotName(slot) });
				if (interaction.RightClicked && slot != GearSlot::Weapon && item.IsValid())
					m_Client.UnequipItem(slot);
				if (slot != GearSlot::Weapon && item.IsValid())
					ui.OfferDrag(id, GearPayload, int(slot));
				if (const std::optional<int> from = ui.AcceptDrop(rect, PackPayload); from && CanEquip(inventory.Pack[size_t(*from)], slot))
					m_Client.EquipItemTo(uint32_t(*from), slot);
				if (interaction.Hovered && item.IsValid() && !drag)
				{
					UiTooltip& tooltip = ui.Tooltip();
					DescribeItem(tooltip, item, affinity);
					tooltip.Space(4.0f).Muted(slot == GearSlot::Weapon ? "Swap it by equipping another weapon" : "Right click or drag to the backpack to take it off", 13.5f);
				}
			}
		left.CutTop(3.0f * GearSize + 2.0f * GearGap + 18.0f);

		ui.Separator(left.CutTop(20.0f), "SKILL GEMS");
		left.CutTop(10.0f);
		UiRect gemRow = left.CutTop(GearSize);
		for (uint32_t gem = 0; gem < SkillSlots; gem++)
		{
			const UiRect rect = gemRow.CutLeft(GearSize);
			gemRow.CutLeft(GearGap);
			const UiId id = Ui::MakeId("gem", gem);
			const Item& item = inventory.Gems[gem];
			const SkillTraits& skill = current.Skills[gem + 1];
			const UiInteraction interaction = ui.Interact(id, rect);
			const bool target = drag && drag->Type == PackPayload && inventory.Pack[size_t(drag->Value)].IsGem();
			DrawItemSlot(ui, rect, item, { .Hovered = interaction.Hovered, .Dim = item.IsValid() && !skill.Usable, .Target = target, .EmptyIcon = UiIcon::Emerald, .KeyLabel = SlotKeys[gem + 1] });
			if (interaction.RightClicked && item.IsValid())
				m_Client.UnslotGem(gem);
			if (item.IsValid())
				ui.OfferDrag(id, GemPayload, int(gem));
			if (const std::optional<int> from = ui.AcceptDrop(rect, PackPayload); from && inventory.Pack[size_t(*from)].IsGem())
				m_Client.SlotGem(uint32_t(*from), gem);
			if (interaction.Hovered && !drag)
			{
				UiTooltip& tooltip = ui.Tooltip();
				tooltip.Muted(std::string("Cast with ") + SlotKeys[gem + 1], 13.5f).Space(2.0f);
				DescribeSkill(tooltip, skill, current);
				if (item.IsValid())
					tooltip.Space(4.0f).Muted("Right click or drag to the backpack to remove it", 13.5f);
			}
		}
		left.CutTop(10.0f);
		draw.TextWrapped(left, "Your weapon's attack is always on LMB, gems go on RMB, Q and R", Theme::Body(13.5f, Theme::Faint));

		const uint32_t carried = uint32_t(std::count_if(inventory.Pack.begin(), inventory.Pack.end(), [](const Item& item) { return item.IsValid(); }));
		char heading[48];
		std::snprintf(heading, sizeof(heading), "BACKPACK  %u / %u", carried, unsigned(BackpackSize));
		ui.Separator(right.CutTop(20.0f), heading);
		right.CutTop(10.0f);
		const UiRect grid{ right.X, right.Y, packWidth, float(BackpackSize / Columns) * (PackSize + PackGap) - PackGap };
		for (uint32_t slot = 0; slot < BackpackSize; slot++)
		{
			const UiRect rect{ grid.X + float(slot % Columns) * (PackSize + PackGap), grid.Y + float(slot / Columns) * (PackSize + PackGap), PackSize, PackSize };
			const UiId id = Ui::MakeId("pack", slot);
			const Item& item = inventory.Pack[slot];
			const UiInteraction interaction = ui.Interact(id, rect);
			bool better = false;
			PlayerTraits candidate = current;
			if (item.IsValid())
			{
				Equipment gear = inventory.Gear;
				GemSlots gems = inventory.Gems;
				if (item.IsGem())
				{
					const auto open = std::find_if(gems.begin(), gems.end(), [](const Item& gem) { return !gem.IsValid(); });
					*(open != gems.end() ? open : gems.end() - 1) = item;
				}
				else if (const std::optional<GearSlot> target = PreferredGearSlot(item, gear))
					gear[size_t(*target)] = item;
				candidate = ComputeTraits(gear, gems, local.Buffs, inventory.Upgrades);
				better = !item.IsGem() && RateItem(candidate) > currentRating * 1.005f;
			}
			const bool target = drag && (drag->Type == GearPayload || drag->Type == GemPayload);
			const bool dragged = drag && drag->Type == PackPayload && drag->Value == int(slot);
			DrawItemSlot(ui, rect, dragged ? Item{} : item, { .Hovered = interaction.Hovered, .Better = better, .Target = target && !item.IsValid() });
			if (interaction.Clicked && item.IsValid())
				m_Client.EquipItem(slot);
			if (interaction.RightClicked && item.IsValid())
				m_Client.DropItem(slot);
			if (item.IsValid())
				ui.OfferDrag(id, PackPayload, int(slot));
			if (const std::optional<int> gear = ui.AcceptDrop(rect, GearPayload))
				m_Client.UnequipItem(GearSlot(*gear));
			if (const std::optional<int> gem = ui.AcceptDrop(rect, GemPayload))
				m_Client.UnslotGem(uint32_t(*gem));
			if (interaction.Hovered && item.IsValid() && !drag)
			{
				UiTooltip& tooltip = ui.Tooltip();
				DescribeItem(tooltip, item, affinity);
				if (!item.IsGem())
				{
					tooltip.Separator().Text("If equipped", Theme::AccentLight, 14.0f);
					CompareTraits(tooltip, current, candidate);
				}
				tooltip.Space(4.0f).Muted(item.IsGem() ? "Left click to slot it, right click to drop" : "Left click to equip, right click to drop", 13.5f);
			}
		}
		right.CutTop(grid.H + 12.0f);
		draw.TextWrapped(right, "Click equips, right click drops, drag items onto a slot. A green arrow marks an upgrade.", Theme::Body(13.5f, Theme::Faint));

		ui.Separator(content.CutTop(20.0f), "CHARACTER");
		content.CutTop(8.0f);
		content.CutTop(DrawTraitsTable(ui, content, current) + 14.0f);

		ui.Separator(content.CutTop(20.0f), "UPGRADES THIS RUN");
		content.CutTop(10.0f);
		UiRect upgrades = content.CutTop(36.0f);
		bool any = false;
		for (size_t i = 0; i < inventory.Upgrades.size(); i++)
		{
			if (inventory.Upgrades[i] == 0)
				continue;
			any = true;
			const UpgradeDef& upgrade = GetUpgrade(UpgradeId(i));
			const UiRect tile = upgrades.CutLeft(36.0f);
			upgrades.CutLeft(6.0f);
			draw.Rect(tile, { .Fill = Theme::SlotTop, .FillBottom = Theme::SlotBottom, .Radius = 6.0f, .Border = WithAlpha(Theme::Edge, 0.9f), .BorderWidth = 1.2f });
			draw.Icon(IconFor(upgrade.Id), tile.Shrink(6.0f), { .Color = Theme::AccentLight, .ColorBottom = Theme::Accent, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			draw.TextAligned(tile.Shrink(2.0f, 0.0f).Translate({ 3.0f, 3.0f }), std::to_string(inventory.Upgrades[i]), Theme::Floating(12.0f, Theme::Text), { 1.0f, 1.0f });
			if (ui.Interact(Ui::MakeId("taken", i), tile).Hovered && !drag)
				ui.Tooltip().Header(upgrade.Name, Theme::AccentLight, std::nullopt, IconFor(upgrade.Id)).Text(upgrade.Description).Muted("Taken " + std::to_string(inventory.Upgrades[i]) + " of " + std::to_string(upgrade.MaxPicks) + " times this run");
		}
		if (!any)
			draw.TextAligned(upgrades, m_Client.GetSession().Mode == GameMode::Horde ? "None yet, you pick one after every wave" : "Upgrades only exist in endless horde runs", Theme::Body(14.5f, Theme::Faint), { 0.0f, 0.5f });
		content.CutTop(10.0f);
		const char* saving = m_Client.GetSession().Mode == GameMode::Royale ? "Everything here only lasts for this match. Your own gear waits for you in Emberhold" : m_App.GetSettings().SaveProfiles ? "Your gear, gems, stash and materials are saved, upgrades and buffs last for this run" : "Saving is off for this run";
		draw.TextWrapped(content, saving, Theme::Body(13.5f, m_Client.GetSession().Mode == GameMode::Royale ? Theme::Warning : Theme::Faint));

		if (drag)
		{
			const Item* held = nullptr;
			if (drag->Type == PackPayload)
				held = &inventory.Pack[size_t(drag->Value)];
			else if (drag->Type == GearPayload)
				held = &inventory.Gear[size_t(drag->Value)];
			else if (drag->Type == GemPayload)
				held = &inventory.Gems[size_t(drag->Value)];
			if (held && held->IsValid())
			{
				UiDrawList& layer = ui.Draw(UiLayer::Drag);
				const UiRect icon = UiRect::Centered(ui.GetMouse(), { 56.0f, 56.0f });
				layer.Shadow(icon, 20.0f, 14.0f, WithAlpha(ItemColor(*held), 0.45f), { 0.0f, 0.0f });
				DrawItemIcon(layer, *held, icon);
			}
		}
	}

	void SessionState::DrawConnectionWarning()
	{
		const float age = m_Client.GetSnapshotAge();
		if (age < 0.5f)
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		char text[96];
		std::snprintf(text, sizeof(text), "Connection problem: no update from the server for %.1f s", double(age));
		const TextStyle style = Theme::Bold(15.0f, Theme::Warning);
		const UiRect pill = UiRect::Centered({ ui.GetSize().x * 0.5f, 122.0f }, { draw.MeasureWidth(text, style) + 64.0f, 34.0f });
		draw.Rect(pill, { .Fill = Rgba(0.16f, 0.1f, 0.04f, 0.92f), .Border = WithAlpha(Theme::Warning, 0.8f), .BorderWidth = 1.2f, .Chamfer = 9.0f });
		draw.Icon(UiIcon::Hourglass, { pill.X + 14.0f, pill.Center().y - 10.0f, 20.0f, 20.0f }, { .Color = Theme::Warning });
		draw.TextAligned({ pill.X + 40.0f, pill.Y, pill.W - 52.0f, pill.H }, text, style, { 0.0f, 0.5f });
	}

	void SessionState::DrawPauseMenu()
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		m_PauseTime += ui.GetDeltaTime();
		const float appear = std::min(m_PauseTime / 0.3f, 1.0f);
		const UiRect screen = ui.GetScreen();
		const glm::vec2 center = screen.Center();
		draw.Rect(screen, { .Fill = Rgba(0.02f, 0.012f, 0.05f, 0.64f * appear), .Glow = Rgba(0.0f, 0.0f, 0.02f, 0.75f * appear), .GlowSize = 320.0f });
		ui.Block(screen);
		draw.Sigil(center, std::min(screen.H * 0.46f, 400.0f), { .Color = WithAlpha(Theme::AccentLight, 0.13f * appear), .Accent = WithAlpha(Theme::Astral, 0.17f * appear), .Speed = 0.5f, .Phase = 0.53f, .LineWidth = 1.2f, .Glow = 0.35f,
			.Points = 9, .Skip = 4, .Progress = std::min(m_PauseTime / 1.1f, 1.0f) });

		TextStyle title = Theme::Title(m_PauseSettings ? 30.0f : 44.0f);
		title.Tracking = m_PauseSettings ? 2.5f : 7.0f;
		title.GlowColor = WithAlpha(Theme::Accent, 0.55f);
		title.GlowBlur = 10.0f;
		title.Shimmer = 0.5f;
		if (m_PauseSettings)
		{
			const UiRect panel = UiRect::Centered(center, { 600.0f, std::min(700.0f, screen.H - 40.0f) });
			ui.Panel(panel, 18.0f);
			UiRect content = panel.Shrink(36.0f, 30.0f);
			draw.Text({ content.X, content.Y }, "Attunement", title);
			draw.TextWrapped({ content.X, content.Y + 44.0f, content.W, 40.0f }, "Changes apply at once and are remembered.", Theme::Body(15.0f, Theme::Muted));
			ui.Separator({ content.X, content.Y + 74.0f, content.W, 10.0f });
			content.CutTop(100.0f);
			UiRect back = content.CutBottom(44.0f);
			DrawSettings(ui, m_App, content);
			if (ui.Button(Ui::MakeId("pause.back"), back.CutLeft(170.0f), "Back"))
				m_PauseSettings = false;
			draw.TextAligned(back, "Esc returns", Theme::Body(13.5f, Theme::Faint), { 1.0f, 0.5f });
			return;
		}

		const GameMode mode = m_Client.GetSession().Mode;
		const bool canReturn = mode != GameMode::Hub && (!IsPvpMode(mode) || m_Server || m_Client.GetStatus().Match == MatchPhase::Ended);
		const char* returnLabel = mode == GameMode::Mission ? "Abandon the Mission" : IsPvpMode(mode) && m_Client.GetStatus().Match != MatchPhase::Ended ? "End the Match" : "Return to Emberhold";
		const float count = canReturn ? 5.0f : 4.0f;
		const float top = center.y - (count * 54.0f + 120.0f) * 0.5f;
		draw.TextAligned({ 0.0f, top, screen.W, 56.0f }, "Paused", title, { 0.5f, 0.5f });
		TextStyle motto = Theme::Caps(14.0f, Theme::Muted, 4.0f);
		motto.ColorBottom = Theme::Faint;
		draw.TextAligned({ 0.0f, top + 56.0f, screen.W, 20.0f }, "the world does not wait", motto, { 0.5f, 0.5f });
		ui.Separator({ center.x - 150.0f, top + 88.0f, 300.0f, 10.0f });
		UiRect entries{ center.x - 230.0f, top + 116.0f, 460.0f, count * 54.0f };
		const auto entry = [&](const char* id, const char* label)
		{
			const bool clicked = ui.MenuEntry(Ui::MakeId(id), entries.CutTop(48.0f), label, {}, false, 0.5f);
			entries.CutTop(6.0f);
			return clicked;
		};
		if (entry("pause.resume", "Resume"))
			m_Paused = false;
		if (entry("pause.settings", "Attunement"))
			m_PauseSettings = true;
		if (canReturn && entry("pause.hub", returnLabel))
		{
			m_Client.ReturnToHub();
			m_Paused = false;
		}
		if (entry("pause.leave", m_Server ? "Stop Hosting" : "Leave the Game"))
			ReturnToMenu({});
		if (entry("pause.quit", "Quit to Desktop"))
			m_App.Quit();
	}
}
