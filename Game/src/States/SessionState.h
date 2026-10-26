#pragma once
#include "App/AppState.h"
#include "Online/SteamPlatform.h"
#include "Rendering/LevelRenderer.h"
#include "Rendering/Characters.h"
#include "Rendering/Effects.h"
#include "Rendering/Lighting.h"
#include "Rendering/SpellVisuals.h"
#include "UI/UiTypes.h"
#include <GameShared/Client/GameClient.h>
#include <GameShared/Save/CharacterProfile.h>
#include <GameShared/Server/GameServer.h>
#include <glm/glm.hpp>
#include <array>
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
	class UiTooltip;
	class WorldRenderer;

	struct SessionConfig
	{
		bool Host = true;
		uint16_t Port = DefaultPort;
		std::string Address;
		uint64_t RelayHost = 0;
		std::string HostName;
		uint32_t LobbyTicket = 0;
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
		enum class Critter : uint8_t
		{
			CatBody,
			CatHead,
			CatTail,
			Duck,
			Balloon,
			DummyPost,
			DummyBody,
			Count,
		};

		static constexpr float BalloonLift = 1.55f;
		static constexpr float FallGravity = 21.0f;

		struct ChessGlide
		{
			uint8_t Cell = 0;
			uint8_t From = 0;
			uint8_t To = 0;
			float Start = 0.0f;
		};

		struct ChessSink
		{
			uint8_t Cell = 0;
			uint8_t Square = 0;
			float Start = 0.0f;
		};

		struct ChessPending
		{
			Chess::Move Move;
			uint32_t Revision = 0;
			float Time = 0.0f;
		};

		struct EntityFx
		{
			uint8_t LastHealth = 255;
			float Flash = 0.0f;
			float Age = 0.0f;
			bool Seen = false;
			bool Hurt = false;
			float SinceHit = 1e9f;
			float FallAge = -1.0f;
			float FallFrom = 0.0f;
			float SpearAge = -1.0f;
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

		struct Body
		{
			float Height = 0.0f;
			float Speed = 0.0f;
			bool Aloft = false;
			bool Seen = false;
		};

		struct SpearPath
		{
			Vec2 Start;
			Vec2 Last;
			float Facing = 0.0f;
			float Size = 1.0f;
			uint32_t Seed = 0;
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
		bool IsOpenAir(Vec2 position) const;
		float LaunchHeight(const EntityState& entity) const;
		glm::vec3 Ground(Vec2 position, float lift = 0.0f) const { return { position.x, GroundHeight(position) + lift, position.y }; }
		void UpdateCamera(float dt);
		void UpdatePortals(float dt);
		void UpdateArena(float dt);
		void UpdateEffects(float dt);
		void UpdateFeedback(float dt);
		void UpdateWonders();
		float FlashOf(uint32_t netId) const;
		float AgeOf(uint32_t netId) const;
		const CharacterAnimator* AnimatorOf(uint32_t netId) const;
		float BarHeight(const EntityState& entity) const;
		glm::vec3 BodyBase(const EntityState& entity, float lift = 0.0f) const;
		void DrawStunStars(WorldRenderer& renderer, const glm::vec3& center) const;
		void UpdateAnimators(float dt);
		void OnEntityAppeared(const EntityState& entity);
		void EmitContinuous(float dt);
		AbilityId AbilityOf(const EntityState& entity) const;
		glm::vec3 ProjectilePoint(const EntityState& entity) const;
		glm::vec3 EffectColor(const EntityState& entity) const;
		glm::vec3 MeteorPoint(const EntityState& entity, float progress) const;
		void DrawMeteor(WorldRenderer& renderer, const EntityState& entity, float progress) const;
		const EntityState* FindBody(uint32_t owner) const;
		glm::vec3 HandOf(const EntityState& body) const;
		std::optional<glm::vec3> LobPoint(const EntityState& entity, float age) const;
		void DrawLob(WorldRenderer& renderer, const EntityState& entity, float age) const;
		void DrawThunder(WorldRenderer& renderer, const EntityState& entity, float age, bool pending) const;
		void DrawHook(WorldRenderer& renderer, const EntityState& entity, float age) const;
		CharacterAnimator::Input AnimationInput(const EntityState& entity) const;
		void DrawCorpses(WorldRenderer& renderer) const;
		glm::vec4 CharacterTint(const EntityState& entity) const;
		PlayerLook LookOf(const EntityState& player) const;
		LevelView ComputeView() const;
		void DrawEntity(WorldRenderer& renderer, const EntityState& entity) const;
		void DrawPlayer(WorldRenderer& renderer, const EntityState& entity) const;
		IceSpearLook ChargingSpear(const EntityState& caster, float age) const;
		void SpearMoments(EntityFx& fx, const EntityState& entity, float dt);
		void DrawSpearCharge(WorldRenderer& renderer, const EntityState& entity, const glm::vec3& base) const;
		GroundDecal FrostLane(const SpearPath& path) const;
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
		void DrawSpectatorBar();
		void DrawScoreboard();
		void DrawAbilityBar(const UiRect& area);
		void DrawUpgradeChoice();
		void DrawInventory();
		void DrawConnectionWarning();
		void DrawMissionStatus();
		void DrawBossBar(float top);
		void DrawBanner();
		void DrawTitleOrnament(float center, float y, float titleWidth);
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
		void DrawPeak(WorldRenderer& renderer) const;
		void UpdateMatchFeedback(float dt);
		void UpdateBrawlFeedback();
		void DrawBrawlStatus();
		void DrawBrawlLoadout();
		void DrawSpellPicker(UiRect& area, const BrawlSpells& spells);
		void UpdateCardNews(float dt);
		void DrawApexNews();
		void TakeCard(uint32_t choice, CardId card);
		void DrawCardFace(const UiRect& rect, CardId id, float hover, float fade, std::optional<uint32_t> key, bool taken, float reveal, float burst);
		void DrawCardRow(UiRect area, CardMask cards, float size, const char* empty);
		void DrawSpellStrip(const UiRect& area);
		bool AcceptSpellDrop(const UiRect& rect, uint32_t slot, bool clicked);
		bool CanEditSpells() const;
		static constexpr int SpellPayload = 30;
		static constexpr int KeyPayload = 31;
		void DrawDraggedSpell();
		void DrawSpellbook();
		void DrawCalderaGate();
		void DrawGateMatch(UiRect area);
		void DrawGateCustom(UiRect area);
		void DrawGateTeams(UiRect area);
		void DrawGateFooter(UiRect area);
		void DrawBotRules(UiRect& area);
		void DrawArenaChoice(UiRect& area);
		void DrawModeChoice(UiRect& area);
		void DrawTeamChoice(UiRect& area);
		void SetBrawlTeams(uint8_t teams);
		void TidyBrawlSeats();
		void AddBrawlBot(int delta);
		std::array<uint32_t, MaxTeams> CountTeamMembers() const;
		static constexpr int SeatPayload = 32;
		BrawlMode ShownBrawlMode() const;
		bool WonBrawlRound(uint32_t playerId) const;
		void DescribeBrawlSpell(UiTooltip& tooltip, AbilityId spell) const;
		MapKind ChosenArena() const;
		std::string EnterLabel() const;
		void SyncSpellbook();
		void DrawBrawlPlayer(WorldRenderer& renderer, const EntityState& entity, const glm::vec3& base) const;
		void DrawDummy(WorldRenderer& renderer, const EntityState& entity) const;
		void DrawBalloon(WorldRenderer& renderer, const EntityState& entity) const;
		void DrawCritter(WorldRenderer& renderer, Critter critter, const glm::mat4& transform, const glm::vec4& tint = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), float emission = 0.0f, float ground = 0.0f) const;
		glm::vec3 BalloonAnchor(const EntityState& entity) const;
		float LeviathanPass() const;
		void DrawWonders(WorldRenderer& renderer) const;
		void DrawChess();
		void DrawChessTable(WorldRenderer& renderer) const;
		void UpdateChess();
		void CommitChessMove(uint8_t from, uint8_t to, Chess::Piece promotion);
		Chess::Position ShownChessPosition() const;
		void DrawLavaWarning();
		void DrawSkyWarnings();
		void UpdateStorm(const BrawlStatus& brawl);
		bool DescribeOutcome(std::string& title, std::string& text) const;
		bool BrawlRanksAbove(const PlayerInfo& a, const PlayerInfo& b) const;
		void SortStandings(std::vector<PlayerInfo>& players) const;
		std::vector<uint8_t> TeamOrder() const;
		std::string PlayerName(uint32_t playerId) const;
		glm::vec3 ColorOf(uint32_t playerId) const;
		bool DrawItemSource(const UiRect& area, bool gems, uint32_t selectedId, ItemRef& picked);
		void DrawResultLine(const UiRect& area);
		void DrawPauseMenu();
		void DrawConnecting();
		void DrawNetworkWindow();
		void ReturnToMenu(std::string message);
		void UpdateSteam(float dt);
		Presence DescribePresence() const;
		void DrawInvitePage(glm::vec2 center);
		void DrawAchievementsPage(glm::vec2 center);
		void DrawAchievementToasts();

		GameApp& m_App;
		SessionConfig m_Config;
		std::unique_ptr<GameServer> m_Server;
		GameClient m_Client;
		std::string m_Error;
		bool m_Leaving = false;
		bool m_Paused = false;
		bool m_PauseSettings = false;
		bool m_PauseWardrobe = false;
		bool m_PauseInvite = false;
		bool m_PauseAchievements = false;
		float m_PauseTime = 0.0f;
		uint32_t m_LobbyTicket = 0;
		struct Toast
		{
			Achievement Id = Achievement::FirstFling;
			float Age = 0.0f;
		};
		std::vector<Toast> m_Toasts;
		std::vector<std::pair<uint64_t, float>> m_Invited;
		size_t m_InviteScroll = 0;
		bool m_ShowNetStats = false;
		bool m_ShowInventory = false;
		float m_Aim = 0.0f;
		float m_AimDistance = 0.0f;
		float m_Time = 0.0f;
		float m_Fps = 0.0f;
		float m_FpsWindow = 0.0f;
		uint32_t m_FpsFrames = 0;
		uint8_t m_LatchedButtons = 0;
		std::optional<glm::vec3> m_AimPoint;
		std::optional<uint32_t> m_ChosenUpgrade;
		std::unordered_map<uint32_t, EntityFx> m_Fx;
		std::vector<Corpse> m_Corpses;
		std::unique_ptr<CharacterLibrary> m_Characters;
		std::unique_ptr<EffectSystem> m_Effects;
		std::optional<uint32_t> m_MeteorMesh;
		SpellMeshes m_SpellMeshes;
		std::unordered_map<uint32_t, SpearPath> m_SpearPaths;
		std::array<std::vector<uint32_t>, size_t(Critter::Count)> m_Critters;
		std::array<std::vector<uint32_t>, 12> m_ChessMeshes;
		uint32_t m_ChessSeen = UINT32_MAX;
		std::vector<uint16_t> m_ChessSeenMoves;
		Chess::Position m_ChessSeenPosition;
		Chess::Verdict m_ChessSeenResult = Chess::Verdict::Playing;
		std::vector<std::string> m_ChessNotation;
		std::array<std::vector<Chess::Piece>, 2> m_ChessCaptured;
		std::vector<ChessGlide> m_ChessGlides;
		std::vector<ChessSink> m_ChessSinks;
		int32_t m_ChessPick = -1;
		std::optional<std::pair<uint8_t, uint8_t>> m_ChessPromotion;
		std::optional<ChessPending> m_ChessPending;
		std::optional<std::pair<size_t, uint16_t>> m_ChessVoiced;
		WorldCamera m_Camera;
		SceneLighting m_Lighting;
		std::vector<DamageNumber> m_DamageNumbers;
		uint32_t m_HitCounter = 0;
		std::vector<Notice> m_Notices;
		ServerInventory m_SeenInventory;
		bool m_SeenAnyInventory = false;
		uint32_t m_SavedInventoryVersion = 0;
		uint32_t m_SavedSpellsVersion = 0;
		uint32_t m_SavedFeatsVersion = 0;
		uint32_t m_SavedStyleVersion = 0;
		uint32_t m_SpellsSentEpoch = UINT32_MAX;
		float m_SaveTimer = -1.0f;
		float m_WishCooldown = 0.0f;
		float m_CameraGlide = 0.0f;
		float m_Zoom = 1.0f;
		float m_ZoomTarget = 1.0f;
		uint64_t m_SeenStarlit = 0;
		bool m_StarlitKnown = false;
		float m_SeenLeviathan = -1.0f;
		LevelRenderer m_LevelRenderer;
		std::unordered_map<uint32_t, Flight> m_ProjectileHeights;
		std::unordered_map<uint32_t, Body> m_BodyHeights;
		std::unordered_map<uint32_t, glm::vec3> m_Lobs;
		struct Flame
		{
			glm::vec3 Position{ 0.0f };
			float Life = 0.0f;
			uint32_t Key = 0;
		};
		std::vector<Flame> m_Flames;
		uint32_t m_FlameKeys = 0;
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
		MatchRules m_BrawlRules;
		int m_BrawlArena = 0;
		int m_GateTab = 0;
		CollapsePlan m_SeenPlan;
		bool m_HasPlan = false;
		float m_SeenClock = -1e5f;
		float m_RestoreAge = 1e9f;
		std::vector<uint8_t> m_Risen;
		std::vector<uint8_t> m_Fallen;
		float m_CrumbleCooldown = 0.0f;
		float m_Flash = 0.0f;
		float m_StormClock = -1e9f;
		float m_RitualPulse = 0.0f;
		float m_LastSpireStrike = 0.0f;
		BrawlPhase m_SeenBrawlPhase = BrawlPhase::Draft;
		uint8_t m_SeenRound = 0;
		uint8_t m_FoughtRound = 0;
		bool m_LoadoutOpen = true;
		std::optional<uint32_t> m_PickingSlot;
		std::optional<uint32_t> m_ChosenCard;
		float m_CardReveal = 0.0f;
		float m_TakenAge = 0.0f;
		std::optional<AbilityId> m_SelectedSpell;
		struct CardNews
		{
			uint32_t PlayerId = NoPlayer;
			CardId Card = CardId::Count;
			float Age = 0.0f;
		};
		std::vector<CardNews> m_CardNews;
		std::vector<CardNews> m_ApexNews;
		std::array<CardMask, MaxPlayers> m_KnownCards{};
		bool m_CardsKnown = false;
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
