#include "SessionState.h"
#include "App/GameApp.h"
#include "Audio/GameAudio.h"
#include "Rendering/Models.h"
#include "Rendering/WorldRenderer.h"
#include "States/MainMenuState.h"
#include "UI/ItemUi.h"
#include "UI/Palette.h"
#include "UI/Ui.h"
#include <GameShared/Content/Definitions.h>
#include <GameShared/World/Collision.h>
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
	}

	SessionState::SessionState(GameApp& app, SessionConfig config)
		: m_App(app), m_Config(std::move(config)), m_Extent{ app.GetWindow().Width(), app.GetWindow().Height() }
	{
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
			m_App.GetGameAudio().Play(Sfx::Door, std::nullopt, 1.0f, 0.6f);
		}
		m_SeenMode = session.Mode;
		m_SeenMission = MissionPhase::Explore;
		m_SeenMatch = MatchPhase::Countdown;
		m_SeenCountdown = -1;
		m_SeenStormStage = 0;
		m_SeenStormShrinking = false;
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
		m_App.GetGameAudio().Play(Sfx::Equip, std::nullopt, 0.8f, 0.8f);
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
		UpdateEffects(dt);
		UpdateFeedback(dt);
		UpdateMatchFeedback(dt);
		m_App.GetGameAudio().Observe(m_Client);
	}

	float SessionState::GroundHeight(Vec2 position) const
	{
		const Level* level = m_Client.GetLevel();
		return level ? level->Map.GetHeight(position) : 0.0f;
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
			m_App.GetGameAudio().Play(Sfx::Door);
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
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::Escape))
		{
			if (m_Station && !m_Paused)
				m_Station.reset();
			else if (m_ShowInventory && !m_Paused)
				m_ShowInventory = false;
			else
				m_Paused = !m_Paused;
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F3))
			m_ShowNetStats = !m_ShowNetStats;
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F4))
			m_Overview = !m_Overview;
		if (!m_Paused && !m_App.UiWantsMouse() && m_Client.GetSession().Mode == GameMode::Royale && !m_Client.GetPredictedState().Alive && m_Client.GetLocalPlayer().Watching != NoPlayer)
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
			m_ShowInventory = !m_ShowInventory;
			if (m_ShowInventory)
				m_Station.reset();
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
		glm::vec3 focus = m_CameraTarget;
		glm::vec3 eye = m_CameraTarget + glm::vec3(0.0f, 17.0f, -11.0f);
		if (m_Overview && level)
		{
			const float extent = float(std::max(level->Map.GetWidth(), level->Map.GetHeight()));
			focus = { float(level->Map.GetWidth()) * 0.5f, 0.0f, float(level->Map.GetHeight()) * 0.5f };
			eye = focus + glm::vec3(0.0f, extent * 1.25f, -extent * 0.45f);
		}
		glm::mat4 projection = glm::perspectiveLH_ZO(glm::radians(40.0f), aspect, 0.5f, m_Overview ? 600.0f : 200.0f);
		projection[1][1] *= -1.0f;
		m_ViewProjection = projection * glm::lookAtLH(eye, focus, glm::vec3(0.0f, 1.0f, 0.0f));
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
		for (const glm::vec2 corner : { glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, -1.0f), glm::vec2(-1.0f, 1.0f), glm::vec2(1.0f, 1.0f) })
			for (const float plane : { m_CameraTarget.y - 4.0f, m_CameraTarget.y + 3.5f })
			{
				glm::vec4 nearPoint = inverse * glm::vec4(corner, 0.0f, 1.0f);
				glm::vec4 farPoint = inverse * glm::vec4(corner, 1.0f, 1.0f);
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
		return view;
	}

	void SessionState::UpdateEffects(float dt)
	{
		for (auto& [netId, fx] : m_Fx)
		{
			fx.Seen = false;
			fx.Flash = std::max(fx.Flash - dt, 0.0f);
			fx.Age += dt;
		}
		for (const EntityState& entity : m_Client.GetEntities())
		{
			const auto [it, inserted] = m_Fx.try_emplace(entity.NetId, EntityFx{ .LastHealth = entity.Health });
			EntityFx& fx = it->second;
			if (!inserted && entity.Health < fx.LastHealth)
				fx.Flash = HurtFlashSeconds;
			fx.LastHealth = entity.Health;
			fx.Seen = true;
		}
		std::erase_if(m_Fx, [](const auto& item) { return !item.second.Seen; });

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

		for (DamageNumber& number : m_DamageNumbers)
			number.Age += dt;
		std::erase_if(m_DamageNumbers, [](const DamageNumber& number) { return number.Age >= DamageNumberSeconds; });
		for (const HitEvent& hit : m_Client.GetNewHits())
		{
			const float drift = std::fmod(float(m_HitCounter++) * 0.618034f, 1.0f) - 0.5f;
			const bool hurtsUs = hit.OnPlayer && (!IsPvpMode(m_Client.GetSession().Mode) || hit.Target == m_Client.GetPlayerNetId());
			m_DamageNumbers.push_back({ .Position = hit.Position, .Drift = drift * 0.9f, .Damage = hit.Damage, .Critical = hit.Critical, .OnPlayer = hurtsUs, .Damages = hit.Damages });
		}
		if (m_DamageNumbers.size() > MaxDamageNumbers)
			m_DamageNumbers.erase(m_DamageNumbers.begin(), m_DamageNumbers.end() - MaxDamageNumbers);
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
			m_Notices.push_back({ notice.Text, notice.Success ? Theme::GoldLight : Theme::Danger });
			m_App.GetGameAudio().Play(notice.Success ? Sfx::Upgrade : Sfx::Hurt, std::nullopt, notice.Success ? 0.7f : 0.4f, notice.Success ? 1.2f : 1.6f);
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
			return false;
		if (extent != m_Extent)
		{
			m_Extent = extent;
			UpdateCamera(0.0f);
		}

		renderer.Begin(m_ViewProjection, glm::vec3(0.45f, -1.0f, 0.3f), m_Overview ? -100.0f : m_CameraTarget.y);
		m_LevelRenderer.Draw(renderer, *level, ComputeView());
		for (const EntityState& entity : m_Client.GetEntities())
			DrawEntity(renderer, entity);
		DrawStorm(renderer);
		const PlayerState& state = m_Client.GetPredictedState();
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		if (m_AimPoint && self && !m_Paused && !m_Overview && m_HoveredPortal < 0)
		{
			renderer.DrawBox(*m_AimPoint, { 0.08f, 0.02f, 0.08f }, { 1.0f, 0.95f, 0.8f }, 0.785f, 0.8f, m_AimPoint->y - ProjectileHeight);
			if (state.CastSlot >= 0)
			{
				const AbilityDef& ability = GetAbility(m_Client.GetTraits().Skills[size_t(state.CastSlot)].Ability);
				if (ability.Kind == AbilityKind::Ground)
				{
					const Vec2 point = GetGroundTarget(*level, self->Position, FromAngle(m_Aim), m_AimDistance, ability.Range);
					DrawRing(renderer, Ground(point, 0.06f), ability.ImpactRadius * m_Client.GetTraits().AreaScale, 28, SkillColor(ability.Id), 0.6f, 0.035f);
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
				DrawItemDrop(renderer, ItemTagBase(entity.Tag), ItemTagRarity(entity.Tag), ItemTagSkill(entity.Tag), base, m_Time, phase);
				break;
			}
			const glm::vec3 color = LootColors[std::min<size_t>(entity.Variant, std::size(LootColors) - 1)];
			const float bob = 0.38f + 0.08f * std::sin(m_Time * 3.0f + phase);
			const MeshKind mesh = LootKind(entity.Variant) == LootKind::Health || LootKind(entity.Variant) == LootKind::Mana ? MeshKind::Sphere : MeshKind::Box;
			renderer.Draw(mesh, base + glm::vec3(0.0f, bob, 0.0f), { 0.14f, 0.14f, 0.14f }, color, m_Time * 2.0f + phase, 0.75f, base.y);
			renderer.DrawBox(base + glm::vec3(0.0f, 0.01f, 0.0f), { 0.16f, 0.01f, 0.16f }, color * 0.4f, m_Time * 2.0f + phase, 0.3f);
			break;
		}
		default:
			break;
		}
	}

	void SessionState::DrawPlayer(WorldRenderer& renderer, const EntityState& entity) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const glm::vec3 color = ColorOf(entity.Owner);
		if (entity.Has(EntityFlags::Dead))
		{
			renderer.DrawBox(base + glm::vec3(0.0f, 0.08f, 0.0f), { 0.36f, 0.08f, 0.36f }, color * 0.3f, entity.Facing);
			return;
		}
		const float flash = FlashOf(entity.NetId);
		const float emission = entity.Has(EntityFlags::Dashing) ? 0.4f : 0.0f;
		const bool chilled = entity.Has(EntityFlags::Chilled);
		DrawCharacter(renderer, LookOf(entity), base, entity.Facing, chilled ? glm::mix(color, glm::vec3(0.55f, 0.85f, 1.0f), 0.55f) : color, chilled ? std::max(flash, 0.25f) : flash, emission, m_Time);
		if (m_Client.IsHostile(entity))
			DrawRing(renderer, base + glm::vec3(0.0f, 0.03f, 0.0f), 0.55f, 18, { 1.0f, 0.25f, 0.18f }, 0.55f, 0.03f);
		if (entity.Has(EntityFlags::Stunned))
			for (uint32_t i = 0; i < 3; i++)
			{
				const float angle = m_Time * 5.0f + float(i) * 2.094f;
				renderer.DrawBox(base + glm::vec3(0.0f, 2.05f, 0.0f) + Heading(angle) * 0.3f, { 0.05f, 0.05f, 0.05f }, { 1.0f, 0.95f, 0.4f }, angle, 1.0f);
			}
		if (!entity.Has(EntityFlags::Attacking) || entity.Variant >= uint8_t(AbilityId::Count))
			return;
		const AbilityId casting = AbilityId(entity.Variant);
		const glm::vec3 glow = SkillColor(casting);
		const float pulse = 0.6f + 0.4f * std::sin(m_Time * 14.0f);
		if (GetAbility(casting).Gem)
		{
			DrawRing(renderer, base + glm::vec3(0.0f, 0.05f, 0.0f), 0.55f + 0.06f * pulse, 16, glow, 0.6f + 0.4f * pulse, 0.035f);
			renderer.Draw(MeshKind::Sphere, base + Heading(entity.Facing) * 0.32f - Heading(entity.Facing + HalfPi) * 0.42f + glm::vec3(0.0f, 0.95f, 0.0f), glm::vec3(0.09f + 0.03f * pulse), glow, m_Time * 6.0f, 1.0f, base.y);
		}
	}

	void SessionState::DrawEnemy(WorldRenderer& renderer, const EntityState& entity) const
	{
		const float size = entity.GetScale();
		const bool elite = entity.Has(EntityFlags::Elite);
		const bool chilled = entity.Has(EntityFlags::Chilled);
		const glm::vec3 base = Ground(entity.Position);
		const glm::vec3 forward = Heading(entity.Facing) * size;
		const glm::vec3 side = Heading(entity.Facing + HalfPi) * size;
		const glm::vec3 up(0.0f, size, 0.0f);
		const float flash = FlashOf(entity.NetId);
		const float aura = elite ? 0.2f : 0.0f;
		const auto shade = [&](const glm::vec3& color)
		{
			glm::vec3 tone = elite ? glm::mix(color, EliteGold, 0.3f) : color;
			if (chilled)
				tone = glm::mix(tone, glm::vec3(0.55f, 0.85f, 1.0f), 0.45f);
			return glm::mix(tone, glm::vec3(1.0f), flash * 0.8f);
		};
		const bool attacking = entity.Has(EntityFlags::Attacking);
		const float pulse = 0.55f + 0.45f * std::sin(m_Time * 16.0f);
		const EnemyDef* def = FindEnemyDef(entity.Kind);
		const AbilityDef& attack = GetAbility(def ? def->Attack : AbilityId::Bite);
		const float reach = attack.Reach * (elite ? EliteAreaScale : 1.0f);
		switch (entity.Kind)
		{
		case EntityKind::Chaser:
		{
			const float swell = attacking ? 1.12f : 1.0f;
			const glm::vec3 color = attacking ? glm::vec3(0.95f, 0.25f, 0.2f) : glm::vec3(0.55f, 0.28f, 0.68f);
			renderer.DrawBox(base + up * (0.4f * swell), glm::vec3(0.36f, 0.4f, 0.36f) * (size * swell), shade(color), entity.Facing, std::max({ flash, attacking ? 0.35f : 0.0f, aura }));
			renderer.DrawBox(base + forward * (0.36f * swell) + up * (0.55f * swell), glm::vec3(0.06f, 0.07f, 0.18f) * size, { 1.0f, 0.85f, 0.3f }, entity.Facing, 0.9f);
			if (attacking)
				for (const float fraction : { 0.45f, 0.75f, 1.0f })
					DrawArc(renderer, base + glm::vec3(0.0f, 0.05f, 0.0f), entity.Facing, attack.HalfArc, reach * fraction, 9, { 0.95f, 0.2f, 0.1f }, 0.4f + 0.5f * pulse, 0.05f);
			break;
		}
		case EntityKind::Shooter:
			renderer.DrawBox(base + up * 0.6f, glm::vec3(0.28f, 0.6f, 0.28f) * size, shade({ 0.2f, 0.62f, 0.52f }), entity.Facing, std::max(flash, aura));
			renderer.DrawBox(base + forward * 0.34f + up * 0.85f, glm::vec3(0.16f, 0.07f, 0.07f) * size, attacking ? glm::vec3(0.6f, 1.0f, 0.3f) : glm::vec3(0.1f, 0.3f, 0.26f), entity.Facing, attacking ? 1.0f : 0.0f);
			if (attacking)
			{
				constexpr float Length = 7.0f;
				renderer.DrawBox(base + Heading(entity.Facing) * (0.5f + Length * 0.5f) + glm::vec3(0.0f, 0.06f, 0.0f), { Length * 0.5f, 0.01f, 0.03f }, { 0.55f, 1.0f, 0.3f }, entity.Facing, 0.3f + 0.6f * pulse);
			}
			break;
		case EntityKind::Brute:
		{
			const float swell = attacking ? 1.06f : 1.0f;
			const glm::vec3 color = attacking ? glm::vec3(0.9f, 0.3f, 0.15f) : glm::vec3(0.45f, 0.25f, 0.2f);
			renderer.DrawBox(base + up * (0.7f * swell), glm::vec3(0.6f, 0.7f, 0.6f) * (size * swell), shade(color), entity.Facing, std::max({ flash, attacking ? 0.3f : 0.0f, aura }));
			renderer.DrawBox(base + side * 0.68f + forward * 0.2f + up * 0.5f, glm::vec3(0.18f) * size, color * 0.6f, entity.Facing);
			renderer.DrawBox(base - side * 0.68f + forward * 0.2f + up * 0.5f, glm::vec3(0.18f) * size, color * 0.6f, entity.Facing);
			renderer.DrawBox(base + forward * 0.6f + up * 1.05f, glm::vec3(0.06f, 0.08f, 0.3f) * size, { 1.0f, 0.5f, 0.1f }, entity.Facing, 0.9f);
			if (attacking)
			{
				DrawRing(renderer, base + glm::vec3(0.0f, 0.04f, 0.0f), reach, 28, { 1.0f, 0.25f, 0.1f }, 0.5f + 0.5f * pulse, 0.06f);
				DrawRing(renderer, base + glm::vec3(0.0f, 0.04f, 0.0f), reach * 0.6f, 20, { 1.0f, 0.25f, 0.1f }, 0.3f + 0.4f * pulse, 0.04f);
			}
			break;
		}
		case EntityKind::Caster:
		{
			const glm::vec3 robe = shade({ 0.55f, 0.18f, 0.12f });
			renderer.Draw(MeshKind::Cone, base + up * 0.55f, glm::vec3(0.34f, 0.55f, 0.34f) * size, robe, entity.Facing, std::max(flash, aura), base.y);
			renderer.Draw(MeshKind::Sphere, base + up * 1.2f, glm::vec3(0.19f) * size, shade({ 0.3f, 0.1f, 0.08f }), entity.Facing, 0.0f, base.y);
			renderer.DrawBox(base + forward * 0.12f + up * 1.2f, glm::vec3(0.1f, 0.03f, 0.05f) * size, { 1.0f, 0.6f, 0.2f }, entity.Facing, 0.9f);
			const glm::vec3 hand = base + forward * 0.3f + side * 0.3f + up * 0.9f;
			renderer.DrawBox(hand - up * 0.35f, glm::vec3(0.025f, 0.55f, 0.025f) * size, { 0.35f, 0.25f, 0.15f }, entity.Facing);
			renderer.Draw(MeshKind::Sphere, hand + up * 0.25f, glm::vec3(attacking ? 0.16f + 0.05f * pulse : 0.09f) * size, EnemyFire, m_Time * 5.0f, attacking ? 1.0f : 0.6f, base.y);
			if (attacking)
				DrawRing(renderer, base + glm::vec3(0.0f, 0.05f, 0.0f), 0.7f * size, 14, EnemyFire, 0.5f + 0.5f * pulse, 0.04f);
			break;
		}
		default:
			break;
		}
		if (entity.Has(EntityFlags::Stunned))
			for (uint32_t i = 0; i < 3; i++)
			{
				const float angle = m_Time * 5.0f + float(i) * 2.094f;
				renderer.DrawBox(base + up * (HealthBarHeight(entity.Kind) + 0.15f) + Heading(angle) * 0.3f, { 0.05f, 0.05f, 0.05f }, { 1.0f, 0.95f, 0.4f }, angle, 1.0f);
			}
		if (!elite)
			return;
		const float shimmer = 0.6f + 0.3f * std::sin(m_Time * 5.0f + float(entity.NetId));
		DrawRing(renderer, base + glm::vec3(0.0f, 0.03f, 0.0f), (def ? def->Radius : 0.4f) * size + 0.22f, 20, EliteGold, shimmer, 0.04f);
		renderer.DrawBox(base + up * (HealthBarHeight(entity.Kind) - 0.12f), glm::vec3(0.13f, 0.05f, 0.13f) * size, EliteGold, m_Time * 1.5f, 0.9f);
	}

	void SessionState::DrawTelegraph(WorldRenderer& renderer, const EntityState& entity, float progress) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const AbilityDef& def = GetAbility(AbilityId(std::min<uint32_t>(entity.Variant, uint32_t(AbilityId::Count) - 1)));
		const bool enemy = entity.Owner == NoPlayer;
		const glm::vec3 color = enemy ? (def.Damages == Element::Physical ? EnemyFire : SkillColor(def.Id)) : SkillColor(def.Id);
		const float scale = entity.GetScale();
		const float pulse = 0.55f + 0.45f * std::sin(m_Time * 12.0f);
		const glm::vec3 lift(0.0f, 0.04f, 0.0f);
		switch (def.Shape)
		{
		case StrikeShape::Line:
		{
			const float length = def.Reach * scale;
			const float half = def.Width * scale * 0.5f;
			const glm::vec3 direction = Heading(entity.Facing);
			const glm::vec3 side = Heading(entity.Facing + HalfPi);
			renderer.DrawBox(base + direction * (length * 0.5f) + lift, { length * 0.5f, 0.01f, half }, color * 0.3f, entity.Facing, 0.15f);
			renderer.DrawBox(base + direction * (length * progress * 0.5f) + lift + glm::vec3(0.0f, 0.005f, 0.0f), { length * progress * 0.5f, 0.012f, half }, color, entity.Facing, 0.55f);
			for (const float sign : { -1.0f, 1.0f })
				renderer.DrawBox(base + direction * (length * 0.5f) + side * (half * sign) + lift, { length * 0.5f, 0.02f, 0.045f }, color, entity.Facing, 0.5f + 0.4f * pulse);
			renderer.DrawBox(base + direction * length + lift, { 0.045f, 0.02f, half }, color, entity.Facing, 0.5f + 0.4f * pulse);
			break;
		}
		case StrikeShape::Ring:
		{
			const float outer = def.ImpactRadius * scale;
			const float inner = def.InnerRadius * scale;
			DrawRing(renderer, base + lift, outer, 56, color, 0.5f + 0.4f * pulse, 0.07f);
			DrawRing(renderer, base + lift, inner, 28, color, 0.5f + 0.4f * pulse, 0.07f);
			for (const float fraction : { 0.33f, 0.66f })
				DrawRing(renderer, base + lift, inner + (outer - inner) * fraction, 44, color * 0.5f, 0.25f, 0.03f);
			DrawRing(renderer, base + lift, inner + (outer - inner) * progress, 48, color, 0.85f, 0.1f);
			break;
		}
		case StrikeShape::Cone:
		{
			const float reach = def.Reach * scale;
			for (const float fraction : { 0.35f, 0.7f, 1.0f })
				DrawArc(renderer, base + lift, entity.Facing, def.HalfArc, reach * fraction, 13, color, fraction >= 1.0f ? 0.5f + 0.4f * pulse : 0.3f, fraction >= 1.0f ? 0.06f : 0.035f);
			for (const float sign : { -1.0f, 1.0f })
			{
				const float angle = entity.Facing + def.HalfArc * sign;
				renderer.DrawBox(base + Heading(angle) * (reach * 0.5f) + lift, { reach * 0.5f, 0.02f, 0.045f }, color, angle, 0.5f + 0.4f * pulse);
			}
			DrawArc(renderer, base + lift, entity.Facing, def.HalfArc, std::max(reach * progress, 0.2f), 13, color, 0.9f, 0.09f);
			break;
		}
		default:
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
			const float hover = 0.25f + 0.08f * std::sin(m_Time * 2.0f);
			const glm::vec3 body = base + glm::vec3(0.0f, hover, 0.0f);
			const glm::vec3 robe(0.32f, 0.12f, 0.42f);
			const glm::vec3 arcane(0.75f, 0.55f, 1.0f);
			renderer.Draw(MeshKind::Cone, body + glm::vec3(0.0f, 0.95f, 0.0f), { 0.75f, 0.95f, 0.75f }, shade(robe), entity.Facing, flash, base.y);
			renderer.DrawBox(body + glm::vec3(0.0f, 1.55f, 0.0f), { 0.42f, 0.22f, 0.36f }, shade(robe * 0.8f), entity.Facing, flash);
			renderer.Draw(MeshKind::Sphere, body + glm::vec3(0.0f, 2.05f, 0.0f), { 0.3f, 0.3f, 0.3f }, shade({ 0.18f, 0.08f, 0.22f }), entity.Facing, flash, base.y);
			renderer.Draw(MeshKind::Cone, body + glm::vec3(0.0f, 2.45f, 0.0f) - forward * 0.05f, { 0.34f, 0.32f, 0.34f }, shade(robe * 0.9f), entity.Facing, flash, base.y);
			for (const float sign : { -1.0f, 1.0f })
				renderer.DrawBox(body + glm::vec3(0.0f, 2.08f, 0.0f) + forward * 0.26f + side * (0.1f * sign), { 0.05f, 0.035f, 0.035f }, arcane, entity.Facing, 1.0f);
			const uint32_t books = 6;
			for (uint32_t i = 0; i < books; i++)
			{
				const float angle = m_Time * (attacking ? 2.6f : 1.1f) + float(i) * 2.0f * Pi / float(books);
				const glm::vec3 position = body + Heading(angle) * 1.25f + glm::vec3(0.0f, 1.5f + 0.25f * std::sin(m_Time * 2.0f + float(i)), 0.0f);
				renderer.DrawBox(position, { 0.2f, 0.06f, 0.15f }, i % 2 ? glm::vec3(0.5f, 0.2f, 0.15f) : glm::vec3(0.2f, 0.25f, 0.45f), angle + HalfPi, attacking ? 0.6f + 0.4f * pulse : 0.15f);
				renderer.DrawBox(position + glm::vec3(0.0f, 0.065f, 0.0f), { 0.17f, 0.01f, 0.13f }, glm::vec3(0.95f, 0.9f, 0.75f), angle + HalfPi, 0.4f);
			}
			const glm::vec3 tome = body + forward * 0.7f + glm::vec3(0.0f, 1.25f, 0.0f);
			renderer.DrawBox(tome, { 0.28f, 0.05f, 0.2f }, { 0.55f, 0.18f, 0.12f }, entity.Facing, 0.2f);
			renderer.DrawBox(tome + glm::vec3(0.0f, 0.06f, 0.0f), { 0.25f, 0.015f, 0.18f }, arcane, entity.Facing, attacking ? 1.0f : 0.5f);
			DrawRing(renderer, base + glm::vec3(0.0f, 0.04f, 0.0f), 1.1f + (attacking ? 0.15f * pulse : 0.0f), 28, arcane, attacking ? 0.9f : 0.4f, 0.05f);
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
			DrawRing(renderer, base + glm::vec3(0.0f, 0.06f, 0.0f), 1.3f + 0.15f * pulse, 24, glm::vec3(0.55f, 0.45f, 0.35f), 0.3f, 0.06f);
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
			DrawRing(renderer, base + glm::vec3(0.0f, 0.05f, 0.0f), 1.4f + 0.2f * pulse, 28, crystal, 0.8f, 0.05f);
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
				DrawRing(renderer, base + glm::vec3(0.0f, 0.03f, 0.0f), 0.7f, 18, { 0.95f, 0.85f, 0.6f }, 0.25f + 0.25f * pulse, 0.025f);
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
			DrawRing(renderer, base + glm::vec3(0.0f, 0.04f, 0.0f), 1.3f + 0.15f * pulse, 28, ember, 0.8f, 0.06f);
			for (uint32_t i = 0; i < 6; i++)
			{
				const float t = std::fmod(m_Time * 0.5f + float(i) / 6.0f, 1.0f);
				renderer.DrawBox(base + Heading(float(i) * 1.05f + m_Time * 0.7f) * (0.5f + 0.4f * t) + glm::vec3(0.0f, 0.8f + 3.5f * t, 0.0f), glm::vec3(0.06f * (1.0f - t)), ember, m_Time, 1.0f);
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
			DrawRing(renderer, base + glm::vec3(0.0f, 0.04f, 0.0f), 0.95f + 0.1f * pulse, 24, gold, 0.5f + 0.4f * pulse, 0.04f);
			for (uint32_t i = 0; i < 4; i++)
			{
				const float t = std::fmod(m_Time * 0.6f + float(i) * 0.25f, 1.0f);
				renderer.DrawBox(base + Heading(float(i) * 1.6f + m_Time) * 0.35f + glm::vec3(0.0f, 0.8f + 1.2f * t, 0.0f), glm::vec3(0.04f * (1.0f - t)), gold, m_Time * 3.0f, 1.0f);
			}
		}
	}

	void SessionState::DrawGateway(WorldRenderer& renderer, const EntityState& entity) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const glm::vec3 inner(0.75f, 0.55f, 1.0f);
		const glm::vec3 outer(1.0f, 0.8f, 0.4f);
		DrawRing(renderer, base + glm::vec3(0.0f, 0.04f, 0.0f), 1.6f, 36, outer, 0.8f, 0.07f);
		DrawRing(renderer, base + glm::vec3(0.0f, 0.04f, 0.0f), 1.15f, 28, inner, 0.6f, 0.05f);
		for (uint32_t level = 0; level < 3; level++)
		{
			const float t = std::fmod(m_Time * 0.45f + float(level) / 3.0f, 1.0f);
			DrawRing(renderer, base + glm::vec3(0.0f, 0.2f + 2.6f * t, 0.0f), 1.3f * (1.0f - 0.5f * t), 24, glm::mix(inner, outer, t), 1.0f - t, 0.04f);
		}
		for (uint32_t i = 0; i < 8; i++)
		{
			const float angle = float(i) * 0.785f + m_Time * 0.8f;
			const float t = std::fmod(m_Time * 0.7f + float(i) * 0.125f, 1.0f);
			renderer.DrawBox(base + Heading(angle) * (1.35f - 0.6f * t) + glm::vec3(0.0f, 0.2f + 2.2f * t, 0.0f), { 0.03f, 0.18f, 0.03f }, inner, angle, 1.0f - 0.5f * t);
		}
		renderer.Draw(MeshKind::Sphere, base + glm::vec3(0.0f, 1.3f, 0.0f), glm::vec3(0.3f + 0.05f * std::sin(m_Time * 4.0f)), inner, m_Time, 1.0f, base.y);
	}

	void SessionState::DrawProjectile(WorldRenderer& renderer, const EntityState& entity) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const auto lifted = m_ProjectileHeights.find(entity.NetId);
		const glm::vec3 center = { entity.Position.x, lifted != m_ProjectileHeights.end() ? lifted->second.Height : base.y + ProjectileHeight, entity.Position.y };
		const glm::vec3 owner = PlayerColor(entity.Owner);
		const AbilityId ability = AbilityId(std::min<uint32_t>(entity.Variant, uint32_t(AbilityId::Count) - 1));
		const glm::vec3 skill = SkillColor(ability);
		switch (ability)
		{
		case AbilityId::Spit:
			renderer.Draw(MeshKind::Sphere, center, { 0.2f, 0.2f, 0.2f }, { 0.5f, 1.0f, 0.25f }, entity.Facing + m_Time * 6.0f, 1.0f, base.y);
			renderer.DrawBox(center - Heading(entity.Facing) * 0.25f, { 0.14f, 0.06f, 0.06f }, { 0.3f, 0.7f, 0.15f }, entity.Facing, 0.8f);
			break;
		case AbilityId::Arrow:
		case AbilityId::Volley:
			renderer.DrawBox(center, { 0.3f, 0.035f, 0.035f }, glm::mix(owner, glm::vec3(0.95f, 0.85f, 0.6f), 0.55f), entity.Facing, 0.6f);
			break;
		case AbilityId::PiercingShot:
			renderer.DrawBox(center, { 0.45f, 0.045f, 0.045f }, glm::mix(skill, glm::vec3(1.0f), 0.4f), entity.Facing, 1.0f);
			renderer.DrawBox(center - Heading(entity.Facing) * 0.5f, { 0.35f, 0.02f, 0.02f }, skill, entity.Facing, 0.7f);
			break;
		case AbilityId::Quarrel:
			renderer.DrawBox(center, { 0.26f, 0.05f, 0.05f }, glm::mix(owner, glm::vec3(0.8f, 0.82f, 0.88f), 0.65f), entity.Facing, 0.5f);
			renderer.DrawBox(center - Heading(entity.Facing) * 0.24f, { 0.05f, 0.03f, 0.09f }, owner, entity.Facing, 0.8f);
			break;
		case AbilityId::Orb:
			renderer.Draw(MeshKind::Sphere, center, { 0.22f, 0.22f, 0.22f }, glm::mix(owner, skill, 0.7f), m_Time * 4.0f, 1.0f, base.y);
			renderer.DrawBox(center, { 0.28f, 0.04f, 0.28f }, glm::mix(skill, glm::vec3(1.0f), 0.5f), -m_Time * 6.0f, 0.8f);
			break;
		case AbilityId::Fireball:
			renderer.Draw(MeshKind::Sphere, center, { 0.24f, 0.24f, 0.24f }, skill, m_Time * 5.0f, 1.0f, base.y);
			renderer.Draw(MeshKind::Sphere, center - Heading(entity.Facing) * 0.28f, { 0.15f, 0.15f, 0.15f }, { 1.0f, 0.3f, 0.1f }, m_Time * 3.0f, 0.9f, base.y);
			break;
		case AbilityId::IceSpear:
			renderer.DrawBox(center, { 0.38f, 0.06f, 0.06f }, skill, entity.Facing, 0.9f);
			renderer.DrawBox(center + Heading(entity.Facing) * 0.32f, { 0.08f, 0.09f, 0.09f }, glm::vec3(0.9f, 0.97f, 1.0f), entity.Facing + 0.785f, 1.0f);
			break;
		case AbilityId::ChainLightning:
			renderer.DrawBox(center, { 0.22f, 0.04f, 0.04f }, skill, entity.Facing + 0.5f * std::sin(m_Time * 40.0f), 1.0f);
			renderer.DrawBox(center, { 0.16f, 0.04f, 0.04f }, glm::vec3(1.0f), entity.Facing - 0.6f, 1.0f);
			break;
		case AbilityId::TomeVolley:
			renderer.DrawBox(center, { 0.24f, 0.07f, 0.18f }, { 0.35f, 0.3f, 0.75f }, m_Time * 9.0f + float(entity.NetId), 0.5f);
			renderer.DrawBox(center + glm::vec3(0.0f, 0.075f, 0.0f), { 0.21f, 0.015f, 0.15f }, glm::mix(skill, glm::vec3(1.0f), 0.4f), m_Time * 9.0f + float(entity.NetId), 1.0f);
			break;
		case AbilityId::InkSpiral:
			renderer.Draw(MeshKind::Sphere, center, { 0.24f, 0.24f, 0.24f }, { 0.25f, 0.05f, 0.35f }, m_Time * 4.0f, 0.6f, base.y);
			renderer.Draw(MeshKind::Sphere, center - Heading(entity.Facing) * 0.22f, { 0.14f, 0.14f, 0.14f }, { 0.55f, 0.25f, 0.85f }, 0.0f, 1.0f, base.y);
			break;
		case AbilityId::ShardBurst:
			renderer.DrawBox(center, { 0.28f, 0.07f, 0.07f }, { 0.55f, 0.95f, 1.0f }, entity.Facing, 1.0f);
			renderer.DrawBox(center + Heading(entity.Facing) * 0.22f, { 0.08f, 0.08f, 0.08f }, glm::vec3(0.9f, 1.0f, 1.0f), entity.Facing + 0.785f, 1.0f);
			break;
		default:
			renderer.DrawBox(center, { 0.22f, 0.08f, 0.08f }, glm::mix(owner, skill, 0.6f), entity.Facing, 1.0f);
			break;
		}
	}

	void SessionState::DrawEffect(WorldRenderer& renderer, const EntityState& entity) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const AbilityId ability = AbilityId(std::min<uint32_t>(entity.Variant, uint32_t(AbilityId::Count) - 1));
		const AbilityDef& def = GetAbility(ability);
		const bool enemy = entity.Owner == NoPlayer;
		const glm::vec3 owner = enemy ? EnemyFire : PlayerColor(entity.Owner);
		const glm::vec3 skill = enemy && def.Damages == Element::Physical ? EnemyFire : SkillColor(ability);
		const glm::vec3 bright = glm::mix(owner, glm::vec3(1.0f), 0.6f);
		const float age = AgeOf(entity.NetId);
		const float scale = entity.GetScale();
		const float pulse = 0.55f + 0.45f * std::sin(m_Time * 12.0f);

		if (entity.Has(EntityFlags::Pending) && def.Shape != StrikeShape::Circle)
		{
			DrawTelegraph(renderer, entity, def.ImpactDelay > 0.0f ? std::clamp(age / def.ImpactDelay, 0.0f, 1.0f) : 1.0f);
			return;
		}
		if (entity.Has(EntityFlags::Pending))
		{
			const float radius = def.ImpactRadius * scale;
			const float progress = def.ImpactDelay > 0.0f ? std::clamp(age / def.ImpactDelay, 0.0f, 1.0f) : 1.0f;
			const glm::vec3 color = enemy ? EnemyFire : skill;
			DrawRing(renderer, base + glm::vec3(0.0f, 0.05f, 0.0f), radius, 32, color, 0.5f + 0.4f * pulse, 0.05f);
			DrawRing(renderer, base + glm::vec3(0.0f, 0.04f, 0.0f), std::max(radius * progress, 0.1f), 24, color, 0.8f, 0.08f);
			renderer.DrawBox(base + glm::vec3(0.0f, 0.03f, 0.0f), { radius * 0.12f, 0.01f, radius * 0.12f }, color, m_Time * 2.0f, 0.6f);
			if (ability == AbilityId::Meteor)
				renderer.Draw(MeshKind::Sphere, base + glm::vec3(0.0f, 0.6f + 9.0f * (1.0f - progress), 0.0f), { 0.55f, 0.55f, 0.55f }, { 1.0f, 0.45f, 0.15f }, m_Time * 3.0f, 1.0f, base.y);
			return;
		}

		const float duration = std::max(def.EffectDuration, 0.05f);
		const float t = std::clamp(age / duration, 0.0f, 1.0f);
		switch (def.Kind)
		{
		case AbilityKind::Nova:
		{
			const float reach = def.Reach * scale;
			if (ability == AbilityId::Cyclone)
			{
				for (uint32_t i = 0; i < 3; i++)
					DrawArc(renderer, base + glm::vec3(0.0f, 0.4f + 0.15f * float(i), 0.0f), m_Time * 18.0f + float(i) * 2.1f, 0.9f, reach * (0.6f + 0.15f * float(i)), 6, glm::mix(bright, skill, 0.4f), 1.0f, 0.06f);
				break;
			}
			const glm::vec3 color = ability == AbilityId::Slam ? glm::vec3(1.0f, 0.45f, 0.15f) : ability == AbilityId::FrostNova ? skill : glm::mix(owner, skill, 0.5f);
			DrawRing(renderer, base + glm::vec3(0.0f, 0.2f, 0.0f), reach * (0.4f + 0.6f * t), 28, color, 1.0f, 0.09f);
			DrawRing(renderer, base + glm::vec3(0.0f, 0.12f, 0.0f), reach * (0.25f + 0.4f * t), 18, color, 0.8f, 0.05f);
			if (ability == AbilityId::FrostNova)
				for (uint32_t i = 0; i < 10; i++)
				{
					const float angle = float(i) * 0.628f;
					renderer.Draw(MeshKind::Cone, base + Heading(angle) * (reach * (0.4f + 0.55f * t)) + glm::vec3(0.0f, 0.25f, 0.0f), { 0.08f, 0.25f, 0.08f }, glm::vec3(0.85f, 0.95f, 1.0f), angle, 0.8f, base.y);
				}
			break;
		}
		case AbilityKind::Melee:
		{
			const float reach = def.Reach * scale;
			const glm::vec3 center = base + glm::vec3(0.0f, 0.45f, 0.0f);
			switch (ability)
			{
			case AbilityId::Bite:
				DrawArc(renderer, center, entity.Facing, def.HalfArc, reach * 0.7f, 7, { 0.85f, 0.15f, 0.1f }, 1.0f);
				break;
			case AbilityId::Chop:
				DrawArc(renderer, center, entity.Facing, def.HalfArc, reach * 0.75f, 7, glm::mix(owner, glm::vec3(1.0f, 0.75f, 0.4f), 0.6f), 1.0f, 0.09f);
				DrawArc(renderer, center - glm::vec3(0.0f, 0.1f, 0.0f), entity.Facing, def.HalfArc * 0.8f, reach * 0.5f, 5, bright, 0.7f);
				break;
			case AbilityId::Stab:
				renderer.DrawBox(center + Heading(entity.Facing) * (reach * 0.55f), { reach * 0.35f, 0.04f, 0.05f }, bright, entity.Facing, 1.0f);
				DrawArc(renderer, center, entity.Facing, def.HalfArc, reach * 0.85f, 3, bright, 0.8f);
				break;
			case AbilityId::GroundSlam:
				for (const float fraction : { 0.35f, 0.6f, 0.85f, 1.0f })
					DrawArc(renderer, base + glm::vec3(0.0f, 0.08f, 0.0f), entity.Facing, def.HalfArc, reach * fraction * (0.5f + 0.5f * t), 9, glm::mix(skill, glm::vec3(1.0f, 0.8f, 0.5f), fraction), 1.0f, 0.08f);
				break;
			default:
				DrawArc(renderer, center, entity.Facing, def.HalfArc, reach * 0.7f, 7, bright, 1.0f);
				break;
			}
			break;
		}
		case AbilityKind::Projectile:
		case AbilityKind::Ground:
		{
			const float radius = def.ImpactRadius * scale;
			const glm::vec3 color = enemy ? EnemyFire : skill;
			if (def.Shape == StrikeShape::Line)
			{
				const float length = def.Reach * scale;
				const glm::vec3 direction = Heading(entity.Facing);
				for (uint32_t i = 0; i < 6; i++)
				{
					const float along = length * (float(i) + 0.5f) / 6.0f;
					renderer.DrawBox(base + direction * along + glm::vec3(0.0f, 0.25f + 0.9f * (1.0f - t) * float(i % 2), 0.0f), { length / 12.0f, 0.18f * (1.0f - t) + 0.04f, def.Width * scale * 0.4f }, glm::mix(color, glm::vec3(1.0f), 0.3f), entity.Facing + 0.3f * float(i % 3), 1.0f);
				}
				break;
			}
			if (def.Shape == StrikeShape::Cone)
			{
				for (const float fraction : { 0.4f, 0.7f, 1.0f })
					DrawArc(renderer, base + glm::vec3(0.0f, 0.3f, 0.0f), entity.Facing, def.HalfArc, def.Reach * scale * fraction * (0.6f + 0.4f * t), 11, glm::mix(color, glm::vec3(1.0f), 0.3f), 1.0f, 0.1f);
				break;
			}
			if (def.Shape == StrikeShape::Ring)
			{
				DrawRing(renderer, base + glm::vec3(0.0f, 0.3f, 0.0f), radius * (0.9f + 0.1f * t), 48, color, 1.0f, 0.14f);
				DrawRing(renderer, base + glm::vec3(0.0f, 0.3f, 0.0f), def.InnerRadius * scale * (1.0f + 0.2f * t), 24, color, 1.0f, 0.1f);
				DrawRing(renderer, base + glm::vec3(0.0f, 0.5f + 0.6f * t, 0.0f), (radius + def.InnerRadius * scale) * 0.5f, 36, glm::mix(color, glm::vec3(1.0f), 0.4f), 1.0f - t, 0.08f);
				break;
			}
			DrawRing(renderer, base + glm::vec3(0.0f, 0.15f, 0.0f), radius * (0.3f + 0.7f * t), 28, color, 1.0f, 0.1f);
			if (ability == AbilityId::RainOfArrows)
				for (uint32_t i = 0; i < 9; i++)
				{
					const float angle = float(i) * 2.4f;
					const float distance = radius * std::sqrt(float(i) / 9.0f);
					renderer.DrawBox(base + Heading(angle) * distance + glm::vec3(0.0f, 0.3f + 1.6f * (1.0f - t), 0.0f), { 0.02f, 0.3f, 0.02f }, { 0.9f, 0.85f, 0.6f }, angle, 0.5f);
				}
			else
				renderer.Draw(MeshKind::Sphere, base + glm::vec3(0.0f, 0.3f, 0.0f), glm::vec3(radius * 0.45f * (1.0f - 0.6f * t)), glm::mix(color, glm::vec3(1.0f, 0.9f, 0.6f), 0.3f), 0.0f, 1.0f, base.y);
			break;
		}
		case AbilityKind::Heal:
			for (uint32_t i = 0; i < 3; i++)
				DrawRing(renderer, base + glm::vec3(0.0f, 0.2f + 1.2f * std::fmod(t + float(i) * 0.33f, 1.0f), 0.0f), 0.5f, 14, skill, 0.9f, 0.04f);
			break;
		case AbilityKind::Dash:
			DrawRing(renderer, base + glm::vec3(0.0f, 0.5f, 0.0f), 0.3f + 0.6f * t, 16, skill, 1.0f, 0.05f);
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
			const TextStyle style = Theme::Bold(17.0f, WithAlpha(Theme::GoldLight, alpha));
			const float width = draw.MeasureWidth(m_Hint, style) + 40.0f;
			const UiRect pill = UiRect::Centered({ ui.GetSize().x * 0.5f, ui.GetSize().y * 0.64f }, { width, 36.0f });
			draw.Shadow(pill, 18.0f, 12.0f, Rgba(0.0f, 0.0f, 0.0f, 0.5f * alpha), { 0.0f, 4.0f });
			draw.Rect(pill, { .Fill = Rgba(0.07f, 0.06f, 0.08f, 0.9f * alpha), .Radius = 18.0f, .Border = WithAlpha(Theme::GoldDark, alpha), .BorderWidth = 1.3f });
			draw.TextAligned(pill, m_Hint, style, { 0.5f, 0.5f });
		}
		if (m_Fade > 0.0f)
			draw.Rect(ui.GetScreen(), { .Fill = Rgba(0.0f, 0.0f, 0.0f, 0.9f * m_Fade) });
	}

	void SessionState::DrawConnecting()
	{
		Ui& ui = m_App.GetUi();
		ui.SetLayer(UiLayer::Background);
		ui.Draw().Rect(ui.GetScreen(), { .Fill = Hex(0x18131D), .FillBottom = Hex(0x09080B) });
		ui.SetLayer(UiLayer::Windows);
		const UiRect panel = UiRect::Centered(ui.GetSize() * 0.5f, { 440.0f, 196.0f });
		ui.Panel(panel, 12.0f);
		UiDrawList& draw = ui.Draw();
		draw.TextAligned({ panel.X, panel.Y + 22.0f, panel.W, 34.0f }, m_Config.Host ? "Starting the Game" : "Connecting", Theme::Title(26.0f), { 0.5f, 0.5f });
		const std::string detail = m_Config.Host ? "Opening UDP port " + std::to_string(m_Config.Port) : "Reaching " + m_Config.Address;
		draw.TextAligned({ panel.X, panel.Y + 60.0f, panel.W, 24.0f }, detail, Theme::Body(16.0f, Theme::Muted), { 0.5f, 0.5f });
		const glm::vec2 spinner(panel.Center().x, panel.Y + 108.0f);
		for (int i = 0; i < 8; i++)
		{
			const float angle = float(i) / 8.0f * 6.2831853f;
			const float phase = std::fmod(ui.GetTime() * 1.4f - float(i) / 8.0f + 8.0f, 1.0f);
			draw.Circle(spinner + glm::vec2(std::cos(angle), std::sin(angle)) * 14.0f, 3.2f, WithAlpha(Theme::Gold, 0.2f + 0.8f * (1.0f - phase)));
		}
		if (ui.Button(Ui::MakeId("cancel"), UiRect::Centered({ panel.Center().x, panel.Bottom() - 36.0f }, { 180.0f, 38.0f }), "Cancel"))
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
			draw.Rect(pill, { .Fill = Rgba(0.06f, 0.055f, 0.07f, 0.9f), .FillBottom = Rgba(0.03f, 0.025f, 0.035f, 0.9f), .Radius = 14.0f, .Border = WithAlpha(color, 0.9f), .BorderWidth = 1.3f });
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
			const std::optional<glm::vec2> point = screen(Ground(entity.Position, HealthBarHeight(entity.Kind) * (enemy ? entity.GetScale() : 1.0f)));
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
				const TextStyle titleStyle = Theme::Bold(15.0f, Theme::GoldLight);
				const TextStyle actionStyle = Theme::Body(13.0f, Theme::Text);
				const float width = std::max(draw.MeasureWidth(title, titleStyle), action ? draw.MeasureWidth(action, actionStyle) : 0.0f) + 54.0f;
				const float height = action ? 46.0f : 30.0f;
				const UiRect box{ point->x - width * 0.5f, point->y - height, width, height };
				const float strength = hovered || reachable ? 1.0f : 0.7f;
				draw.Shadow(box, 8.0f, 10.0f, Rgba(0.0f, 0.0f, 0.0f, 0.5f), { 0.0f, 3.0f });
				draw.Rect(box, { .Fill = Rgba(0.08f, 0.065f, 0.05f, 0.92f), .FillBottom = Rgba(0.04f, 0.035f, 0.03f, 0.92f), .Radius = 8.0f, .Border = WithAlpha(Theme::Gold, strength), .BorderWidth = hovered ? 2.0f : 1.2f, .Glow = WithAlpha(Theme::Gold, hovered ? 0.12f : 0.0f), .GlowSize = 10.0f });
				draw.Icon(UiIcon::Door, { box.X + 9.0f, box.Y + 6.0f, 18.0f, 18.0f }, { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold });
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
		const UiRect frame{ 14.0f, 14.0f, 318.0f, 40.0f + float(players.size()) * 30.0f };
		draw.Shadow(frame, 8.0f, 12.0f, Rgba(0.0f, 0.0f, 0.0f, 0.4f), { 0.0f, 3.0f });
		draw.Rect(frame, { .Fill = Rgba(0.08f, 0.07f, 0.09f, 0.72f), .FillBottom = Rgba(0.04f, 0.035f, 0.05f, 0.72f), .Radius = 8.0f, .Border = WithAlpha(Theme::GoldDark, 0.6f), .BorderWidth = 1.0f });
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
			std::snprintf(line, sizeof(line), "%u / %u", player.Kills, player.Deaths);
			draw.TextAligned({ frame.X + 202.0f, y, 60.0f, 30.0f }, line, Theme::Body(13.5f, Theme::Text), { 0.0f, 0.5f });
			std::snprintf(line, sizeof(line), "%d ms", player.Ping);
			draw.TextAligned({ frame.X, y, frame.W - 12.0f, 30.0f }, line, Theme::Body(12.5f, Theme::Faint), { 1.0f, 0.5f });
			y += 30.0f;
		}
		draw.Text({ 16.0f, ui.GetSize().y - 46.0f }, "WASD move   LMB attack   RMB / Q / R skills   Space dodge", Theme::Floating(13.0f, Theme::Muted, UiFont::Body));
		draw.Text({ 16.0f, ui.GetSize().y - 26.0f }, "E interact   I inventory   F4 map   F3 network   Esc menu", Theme::Floating(13.0f, Theme::Muted, UiFont::Body));
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
		for (const float side : { -1.0f, 1.0f })
		{
			const float start = center + side * (titleWidth * 0.5f + 18.0f);
			const float length = 90.0f;
			const UiRect line{ side < 0.0f ? start - length : start, 33.0f, length, 1.5f };
			draw.Rect(line, { .Fill = WithAlpha(Theme::Gold, side < 0.0f ? 0.0f : 0.8f), .FillBottom = WithAlpha(Theme::Gold, side < 0.0f ? 0.8f : 0.0f), .Horizontal = true });
			draw.Rect(UiRect::Centered({ start, 33.75f }, { 6.0f, 6.0f }), { .Fill = Theme::Gold, .Rotation = 0.7853982f });
		}
		draw.TextAligned({ 0.0f, 56.0f, ui.GetSize().x, 22.0f }, subtitle, Theme::Floating(16.0f, danger ? Theme::Danger : Theme::Text, UiFont::Body), { 0.5f, 0.5f });
		if (status.Phase == GamePhase::Intermission && status.PendingChoices > 0)
		{
			if (m_Client.GetLocalPlayer().OfferPending)
				draw.TextAligned({ 0.0f, 80.0f, ui.GetSize().x, 20.0f }, "Pick an upgrade with 1, 2 or 3, the next wave waits for it", Theme::Floating(15.0f, Theme::GoldLight, UiFont::Body), { 0.5f, 0.5f });
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
		draw.Shadow(plate, 12.0f, 18.0f, Rgba(0.0f, 0.0f, 0.0f, 0.55f), { 0.0f, 5.0f });
		draw.Rect(plate, { .Fill = Rgba(0.1f, 0.09f, 0.11f, 0.9f), .FillBottom = Rgba(0.05f, 0.045f, 0.06f, 0.92f), .Radius = 12.0f, .Border = WithAlpha(Theme::GoldDark, 0.95f), .BorderWidth = 1.5f });
		draw.Rect(plate.Shrink(4.0f), { .Radius = 9.0f, .Border = Rgba(1.0f, 0.86f, 0.55f, 0.07f), .BorderWidth = 1.0f });
		ui.Block(plate);
		DrawAbilityBar(slots);

		const auto orb = [&](float x, float fill, UiColor light, UiColor deep, const std::string& text, float phase)
		{
			const glm::vec2 orbCenter(x, size.y - 18.0f - OrbRadius);
			const UiRect bounds = UiRect::Centered(orbCenter, glm::vec2((OrbRadius + 8.0f) * 2.0f));
			draw.Shadow(bounds, OrbRadius + 8.0f, 14.0f, Rgba(0.0f, 0.0f, 0.0f, 0.6f), { 0.0f, 4.0f });
			draw.Circle(orbCenter, OrbRadius + 8.0f, Hex(0x17141B), WithAlpha(Theme::GoldDark, 0.95f), 2.0f);
			draw.Orb(orbCenter, OrbRadius, fill, light, deep, Rgba(0.07f, 0.06f, 0.08f, 0.95f), Rgba(1.0f, 1.0f, 1.0f, 0.32f), phase);
			draw.Circle(orbCenter, OrbRadius + 1.5f, UiColor(0.0f), WithAlpha(Theme::Gold, 0.85f), 2.0f);
			draw.Rect(UiRect::Centered(orbCenter - glm::vec2(0.0f, OrbRadius + 8.0f), { 9.0f, 9.0f }), { .Fill = Theme::GoldLight, .FillBottom = Theme::GoldDark, .Radius = 1.0f, .Rotation = 0.7853982f });
			draw.TextAligned(UiRect::Centered(orbCenter, { 160.0f, 24.0f }), text, Theme::Floating(17.0f, Hex(0xFFF6E8)), { 0.5f, 0.5f });
			ui.Block(bounds);
		};
		const float healthFraction = local.MaxHealth > 0 ? float(std::max(local.Health, 0)) / float(local.MaxHealth) : 0.0f;
		const float manaFraction = traits.MaxMana > 0.0f ? state.Mana / traits.MaxMana : 0.0f;
		orb(plate.X - OrbRadius - 4.0f, healthFraction, Hex(0xFF6A55), Hex(0x6A0A10), std::to_string(std::max(local.Health, 0)) + " / " + std::to_string(local.MaxHealth), 0.0f);
		orb(plate.Right() + OrbRadius + 4.0f, manaFraction, Hex(0x78A6FF), Hex(0x111E66), std::to_string(int(state.Mana)) + " / " + std::to_string(int(traits.MaxMana)), 2.1f);

		float buffX = plate.X;
		const float buffY = plate.Y - 40.0f;
		for (size_t i = 1; i < local.Buffs.size(); i++)
		{
			if (local.Buffs[i] == 0)
				continue;
			const UiRect tile{ buffX, buffY, 32.0f, 32.0f };
			const UiColor color = WithAlpha(LootColors[std::min(i, std::size(LootColors) - 1)]);
			draw.Rect(tile, { .Fill = Hex(0x17151A, 0.9f), .FillBottom = Hex(0x0D0C10, 0.9f), .Radius = 6.0f, .Border = WithAlpha(color, 0.8f), .BorderWidth = 1.2f, .Glow = WithAlpha(color, 0.15f), .GlowSize = 8.0f });
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

		if (!state.Alive)
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
			draw.TextAligned({ 0.0f, size.y * 0.3f, size.x, 64.0f }, royaleOut ? "Eliminated" : IsPvpMode(status.Mode) ? "Slain" : "You Died", died, { 0.5f, 0.5f });
			char respawn[160];
			respawn[0] = 0;
			const std::string killer = local.KilledBy != NoPlayer ? PlayerName(local.KilledBy) : std::string();
			if (royaleOut)
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
			draw.Rect(strip, { .Fill = Rgba(0.08f, 0.07f, 0.09f, 0.72f), .FillBottom = Rgba(0.04f, 0.035f, 0.05f, 0.72f), .Radius = 8.0f, .Border = WithAlpha(Theme::GoldDark, 0.6f), .BorderWidth = 1.0f });
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
			draw.Rect(rect, { .Fill = Theme::SlotTop, .FillBottom = Theme::SlotBottom, .Radius = 8.0f, .Border = border, .BorderWidth = casting ? 2.4f : 1.5f, .Glow = WithAlpha(color, active ? 0.16f : 0.0f), .GlowSize = 14.0f });
			if (empty)
				draw.Icon(UiIcon::Emerald, rect.Shrink(17.0f), { .Color = Rgba(0.6f, 0.55f, 0.45f, 0.2f) });
			else
			{
				const UiColor top = active ? Lighten(color, 0.55f) : Hex(0x8A857D);
				const UiColor bottom = active ? color : Hex(0x4E4A44);
				draw.Icon(IconFor(skill.Ability), rect.Shrink(10.0f), { .Color = top, .ColorBottom = bottom, .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			}
			if (ready < 1.0f)
			{
				draw.Rect(rect.Shrink(1.5f), { .Fill = Rgba(0.02f, 0.02f, 0.03f, 0.7f), .Radius = 7.0f, .Sweep = ready });
				if (cooldown > 0.25f)
				{
					char text[16];
					std::snprintf(text, sizeof(text), cooldown < 3.0f ? "%.1f" : "%.0f", double(cooldown));
					draw.TextAligned(rect, text, Theme::Floating(19.0f, Hex(0xFFF3DC)), { 0.5f, 0.5f });
				}
			}
			if (!empty && !skill.Usable)
			{
				draw.Rect(rect.Shrink(1.5f), { .Fill = Rgba(0.45f, 0.05f, 0.04f, 0.35f), .Radius = 7.0f });
				draw.Icon(UiIcon::Padlock, UiRect::Centered(rect.Center(), { 26.0f, 26.0f }), { .Color = Hex(0xFF8A7A), .ColorBottom = Hex(0xB83A30), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.8f) });
			}
			const char* key = dodge ? "Space" : SlotKeys[slot];
			const TextStyle keyStyle = Theme::Bold(11.5f, Theme::GoldLight);
			const float keyWidth = draw.MeasureWidth(key, keyStyle) + 10.0f;
			const UiRect badge{ rect.X + 3.0f, rect.Bottom() - 17.0f, keyWidth, 15.0f };
			draw.Rect(badge, { .Fill = Rgba(0.04f, 0.035f, 0.05f, 0.92f), .Radius = 4.0f, .Border = WithAlpha(Theme::GoldDark, 0.9f), .BorderWidth = 1.0f });
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
				draw.Shadow(lifted, 10.0f, 12.0f, WithAlpha(Theme::Gold, 0.3f * hover), { 0.0f, 0.0f });
			draw.Rect(lifted, { .Fill = glm::mix(Hex(0x221F26), Hex(0x33281A), hover), .FillBottom = glm::mix(Hex(0x131216), Hex(0x1A130B), hover), .Radius = 10.0f, .Border = glm::mix(WithAlpha(Theme::GoldDark, 0.9f), Theme::GoldLight, hover), .BorderWidth = 1.5f + hover, .Glow = WithAlpha(Theme::Gold, 0.06f + 0.1f * hover), .GlowSize = 22.0f });
			const UiRect icon = UiRect::Centered({ lifted.Center().x, lifted.Y + 38.0f }, { 48.0f, 48.0f });
			draw.Shadow(icon.Shrink(6.0f), 20.0f, 14.0f, WithAlpha(Theme::Gold, 0.25f + 0.2f * hover), { 0.0f, 0.0f });
			draw.Icon(IconFor(upgrade.Id), icon, { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold, .Outline = 0.8f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.7f), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			draw.TextAligned({ lifted.X, lifted.Y + 68.0f, lifted.W, 24.0f }, upgrade.Name, Theme::Bold(19.0f, Theme::GoldLight), { 0.5f, 0.5f });
			draw.TextWrapped({ lifted.X + 16.0f, lifted.Y + 96.0f, lifted.W - 32.0f, 44.0f }, upgrade.Description, Theme::Body(15.0f, Theme::Text), 0.5f);
			char footer[48];
			std::snprintf(footer, sizeof(footer), "taken %u of %u", unsigned(inventory.Upgrades[size_t(upgrade.Id)]), unsigned(upgrade.MaxPicks));
			const UiRect badge{ lifted.X + 14.0f, lifted.Bottom() - 30.0f, 24.0f, 20.0f };
			draw.Rect(badge, { .Fill = Rgba(0.04f, 0.035f, 0.05f, 0.95f), .Radius = 4.0f, .Border = WithAlpha(Theme::Gold, 0.8f), .BorderWidth = 1.0f });
			draw.TextAligned(badge, std::to_string(i + 1), Theme::Bold(13.0f, Theme::GoldLight), { 0.5f, 0.5f });
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
					tooltip.Separator().Text("If equipped", Theme::GoldLight, 14.0f);
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
			draw.Rect(tile, { .Fill = Theme::SlotTop, .FillBottom = Theme::SlotBottom, .Radius = 6.0f, .Border = WithAlpha(Theme::GoldDark, 0.9f), .BorderWidth = 1.2f });
			draw.Icon(IconFor(upgrade.Id), tile.Shrink(6.0f), { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			draw.TextAligned(tile.Shrink(2.0f, 0.0f).Translate({ 3.0f, 3.0f }), std::to_string(inventory.Upgrades[i]), Theme::Floating(12.0f, Theme::Text), { 1.0f, 1.0f });
			if (ui.Interact(Ui::MakeId("taken", i), tile).Hovered && !drag)
				ui.Tooltip().Header(upgrade.Name, Theme::GoldLight, std::nullopt, IconFor(upgrade.Id)).Text(upgrade.Description).Muted("Taken " + std::to_string(inventory.Upgrades[i]) + " of " + std::to_string(upgrade.MaxPicks) + " times this run");
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
		draw.Rect(pill, { .Fill = Rgba(0.18f, 0.11f, 0.03f, 0.92f), .Radius = 17.0f, .Border = WithAlpha(Theme::Warning, 0.8f), .BorderWidth = 1.3f });
		draw.Icon(UiIcon::Hourglass, { pill.X + 14.0f, pill.Center().y - 10.0f, 20.0f, 20.0f }, { .Color = Theme::Warning });
		draw.TextAligned({ pill.X + 40.0f, pill.Y, pill.W - 52.0f, pill.H }, text, style, { 0.0f, 0.5f });
	}

	void SessionState::DrawPauseMenu()
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		draw.Rect(ui.GetScreen(), { .Fill = Rgba(0.0f, 0.0f, 0.0f, 0.55f) });
		ui.Block(ui.GetScreen());
		const GameMode mode = m_Client.GetSession().Mode;
		const bool canReturn = mode != GameMode::Hub && (!IsPvpMode(mode) || m_Server || m_Client.GetStatus().Match == MatchPhase::Ended);
		const UiRect panel = UiRect::Centered(ui.GetSize() * 0.5f, { 360.0f, canReturn ? 358.0f : 300.0f });
		ui.Panel(panel, 12.0f);
		draw.TextAligned({ panel.X, panel.Y + 20.0f, panel.W, 40.0f }, "Paused", Theme::Title(32.0f), { 0.5f, 0.5f });
		ui.Separator({ panel.X + 24.0f, panel.Y + 66.0f, panel.W - 48.0f, 8.0f });
		UiRect buttons{ panel.X + 36.0f, panel.Y + 92.0f, panel.W - 72.0f, panel.H - 110.0f };
		if (ui.Button(Ui::MakeId("resume"), buttons.CutTop(46.0f), "Resume", ButtonKind::Primary))
			m_Paused = false;
		buttons.CutTop(12.0f);
		const char* returnLabel = mode == GameMode::Mission ? "Abandon mission" : IsPvpMode(mode) && m_Client.GetStatus().Match != MatchPhase::Ended ? "End the match" : "Return to Emberhold";
		if (canReturn && ui.Button(Ui::MakeId("tohub"), buttons.CutTop(46.0f), returnLabel, ButtonKind::Normal, true, UiIcon::MagicPortal))
		{
			m_Client.ReturnToHub();
			m_Paused = false;
		}
		if (canReturn)
			buttons.CutTop(12.0f);
		if (ui.Button(Ui::MakeId("leave"), buttons.CutTop(46.0f), m_Server ? "Stop hosting" : "Leave game", ButtonKind::Normal, true, UiIcon::Door))
			ReturnToMenu({});
		buttons.CutTop(12.0f);
		if (ui.Button(Ui::MakeId("desktop"), buttons.CutTop(46.0f), "Quit to desktop", ButtonKind::Danger, true, UiIcon::ExitDoor))
			m_App.Quit();
	}
}
