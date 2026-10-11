#pragma once
#include "App/AppState.h"
#include "Rendering/LevelRenderer.h"
#include "Rendering/Characters.h"
#include "Rendering/Effects.h"
#include "Rendering/Lighting.h"
#include "UI/UiTypes.h"
#include <GameShared/Client/GameClient.h>
#include <GameShared/Save/CharacterProfile.h>
#include <GameShared/Server/GameServer.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Game
{
	class GameApp;
	class WorldRenderer;

	struct SessionConfig
	{
		bool Host = true;
		uint16_t Port = DefaultPort;
		std::string Address;
		MapKind Map = MapKind::Town;
		GameMode StartMode = GameMode::Hub;
		CharacterProfile Profile;
	};

	class SessionState final : public AppState
	{
	public:
		SessionState(GameApp& app, SessionConfig config);
		~SessionState() override;

		void Update(float dt) override;
		void DrawUi() override;
		bool Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D extent, uint32_t frameIndex) override;

	private:
		struct EntityFx
		{
			uint8_t LastHealth = 255;
			float Flash = 0.0f;
			float Age = 0.0f;
			bool Seen = false;
			bool Hurt = false;
			float SinceHit = 1e9f;
			EntityState Last;
			CharacterAnimator Animator;
		};

		struct Corpse
		{
			EntityState Entity;
			CharacterAnimator Animator;
			float Age = 0.0f;
		};

		struct DamageNumber
		{
			Vec2 Position;
			float Age = 0.0f;
			float Drift = 0.0f;
			uint16_t Damage = 0;
			bool Critical = false;
			bool OnPlayer = false;
			Element Damages = Element::Physical;
		};

		struct Flight
		{
			float Height = 0.0f;
			bool Seen = false;
		};

		struct Notice
		{
			std::string Text;
			glm::vec4 Color{ 1.0f };
			float Age = 0.0f;
		};

		PlayerInput SampleInput();
		void UpdateAim();
		void OnTravel();
		void UpdateInteractions();
		bool Interact();
		void OpenStation(StationKind kind);
		const Item* FindClientItem(ItemRef ref) const;
		std::optional<ItemRef> LocateItem(uint32_t id) const;
		void LatchButtonPresses();
		void ExplainBlockedSkill(uint32_t slot);
		void HandleHotkeys();
		void PickUpgrade(uint32_t choice);
		void UsePortal(int32_t portal);
		bool IsProfileDirty() const;
		void SaveProgress(bool force);
		std::optional<glm::vec3> MouseOnPlane(float height) const;
		std::optional<glm::vec3> MouseAimPoint() const;
		std::optional<glm::vec2> ProjectToScreen(const glm::vec3& position) const;
		float GroundHeight(Vec2 position) const;
		glm::vec3 Ground(Vec2 position, float lift = 0.0f) const { return { position.x, GroundHeight(position) + lift, position.y }; }
		void UpdateCamera(float dt);
		void UpdatePortals(float dt);
		void UpdateEffects(float dt);
		void UpdateFeedback(float dt);
		float FlashOf(uint32_t netId) const;
		float AgeOf(uint32_t netId) const;
		const CharacterAnimator* AnimatorOf(uint32_t netId) const;
		float BarHeight(const EntityState& entity) const;
		void DrawStunStars(WorldRenderer& renderer, const glm::vec3& center) const;
		void UpdateAnimators(float dt);
		void OnEntityAppeared(const EntityState& entity);
		void EmitContinuous(float dt);
		AbilityId AbilityOf(const EntityState& entity) const;
		glm::vec3 ProjectilePoint(const EntityState& entity) const;
		glm::vec3 EffectColor(const EntityState& entity) const;
		glm::vec3 MeteorPoint(const EntityState& entity, float progress) const;
		void DrawMeteor(WorldRenderer& renderer, const EntityState& entity, float progress) const;
		CharacterAnimator::Input AnimationInput(const EntityState& entity) const;
		void DrawCorpses(WorldRenderer& renderer) const;
		glm::vec4 CharacterTint(const EntityState& entity) const;
		PlayerLook LookOf(const EntityState& player) const;
		LevelView ComputeView() const;
		void DrawEntity(WorldRenderer& renderer, const EntityState& entity) const;
		void DrawPlayer(WorldRenderer& renderer, const EntityState& entity) const;
		void DrawEnemy(WorldRenderer& renderer, const EntityState& entity) const;
		void DrawProjectile(WorldRenderer& renderer, const EntityState& entity) const;
		void DrawEffect(WorldRenderer& renderer, const EntityState& entity) const;
		void DrawTelegraph(WorldRenderer& renderer, const EntityState& entity, float progress) const;
		void DrawBoss(WorldRenderer& renderer, const EntityState& entity) const;
		void DrawChest(WorldRenderer& renderer, const EntityState& entity) const;
		void DrawGateway(WorldRenderer& renderer, const EntityState& entity) const;

		void DrawWorldUi();
		void DrawOverlay();
		void DrawStatus();
		void DrawHud();
		void DrawAbilityBar(const UiRect& area);
		void DrawUpgradeChoice();
		void DrawInventory();
		void DrawConnectionWarning();
		void DrawMissionStatus();
		void DrawBossBar(float top);
		void DrawBanner();
		void DrawMaterials(const UiRect& area, bool compact);
		void DrawInteractionLabels();
		void DrawStationWindow();
		void DrawStashWindow();
		void DrawForgeWindow();
		void DrawLapidaryWindow();
		void DrawMissionBoard();
		void DrawProvingGrounds();
		void DrawMatchStatus();
		void DrawMatchResults();
		void DrawKillFeed();
		void DrawMinimap();
		void DrawStorm(WorldRenderer& renderer) const;
		void UpdateMatchFeedback(float dt);
		bool DescribeOutcome(std::string& title, std::string& text) const;
		std::string PlayerName(uint32_t playerId) const;
		glm::vec3 ColorOf(uint32_t playerId) const;
		bool DrawItemSource(const UiRect& area, bool gems, uint32_t selectedId, ItemRef& picked);
		void DrawResultLine(const UiRect& area);
		void DrawPauseMenu();
		void DrawConnecting();
		void DrawNetworkWindow();
		void ReturnToMenu(std::string message);

		GameApp& m_App;
		SessionConfig m_Config;
		std::unique_ptr<GameServer> m_Server;
		GameClient m_Client;
		std::string m_Error;
		bool m_Leaving = false;
		bool m_Paused = false;
		bool m_ShowNetStats = false;
		bool m_ShowInventory = false;
		float m_Aim = 0.0f;
		float m_AimDistance = 0.0f;
		float m_Time = 0.0f;
		float m_Fps = 0.0f;
		uint8_t m_LatchedButtons = 0;
		std::optional<glm::vec3> m_AimPoint;
		std::optional<uint32_t> m_ChosenUpgrade;
		std::unordered_map<uint32_t, EntityFx> m_Fx;
		std::vector<Corpse> m_Corpses;
		std::unique_ptr<CharacterLibrary> m_Characters;
		std::unique_ptr<EffectSystem> m_Effects;
		std::optional<uint32_t> m_MeteorMesh;
		WorldCamera m_Camera;
		SceneLighting m_Lighting;
		std::vector<DamageNumber> m_DamageNumbers;
		uint32_t m_HitCounter = 0;
		std::vector<Notice> m_Notices;
		ServerInventory m_SeenInventory;
		bool m_SeenAnyInventory = false;
		uint32_t m_SavedInventoryVersion = 0;
		float m_SaveTimer = -1.0f;
		LevelRenderer m_LevelRenderer;
		std::unordered_map<uint32_t, Flight> m_ProjectileHeights;
		int32_t m_HoveredPortal = -1;
		int32_t m_ReachablePortal = -1;
		uint8_t m_PendingInteract = 0;
		bool m_SuppressPrimary = false;
		uint32_t m_SeenTeleports = 0;
		float m_Fade = 0.0f;
		std::string m_Hint;
		float m_HintTime = 0.0f;
		bool m_Overview = false;
		uint32_t m_SeenTravels = 0;
		std::optional<StationKind> m_Station;
		int32_t m_NearStation = -1;
		uint32_t m_NearChest = 0;
		uint32_t m_NearGateway = 0;
		uint32_t m_ForgeItem = 0;
		uint32_t m_ForgeAffix = 0;
		StatCategory m_ImbueCategory = StatCategory::Offense;
		uint32_t m_LapidaryGem = 0;
		int m_SourceTab = 0;
		MapKind m_BoardMap = MapKind::Town;
		int m_BoardTier = 1;
		ModifierMask m_BoardModifiers = 0;
		MatchRules m_ArenaRules;
		MapKind m_RoyaleMap = MapKind::Ashlands;
		struct FeedEntry
		{
			KillRecord Kill;
			float Age = 0.0f;
		};
		std::vector<FeedEntry> m_Feed;
		MatchPhase m_SeenMatch = MatchPhase::Countdown;
		int32_t m_SeenCountdown = -1;
		uint8_t m_SeenStormStage = 0;
		bool m_SeenStormShrinking = false;
		std::vector<glm::vec4> m_MinimapLand;
		uint32_t m_MinimapTravel = UINT32_MAX;
		MissionPhase m_SeenMission = MissionPhase::Explore;
		GameMode m_SeenMode = GameMode::Count;
		std::string m_BannerTitle;
		std::string m_BannerText;
		glm::vec4 m_BannerColor{ 1.0f };
		float m_BannerTime = 0.0f;
		MaterialStock m_SeenMaterials{};
		bool m_SeenAnyMaterials = false;
		uint32_t m_SavedStashVersion = 0;
		MaterialStock m_SavedMaterials{};
		bool m_CameraPlaced = false;
		glm::vec3 m_CameraTarget{ 0.0f };
		glm::mat4 m_ViewProjection{ 1.0f };
		Rdn::Extent2D m_Extent;
	};
}
