#pragma once
#include "GameShared/Content/Definitions.h"
#include "GameShared/Content/Traits.h"
#include "GameShared/Net/Protocol.h"
#include "GameShared/Net/Transport.h"
#include "GameShared/Save/CharacterProfile.h"
#include "GameShared/Simulation/PlayerSimulation.h"
#include "GameShared/World/Level.h"
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Game
{
	enum class ClientState
	{
		Disconnected,
		Connecting,
		Joining,
		InGame,
	};

	using InputSampler = std::function<PlayerInput()>;

	class GameClient
	{
	public:
		static constexpr uint32_t PredictedNetIdBase = 0x80000000u;

		bool Connect(const std::string& address, const CharacterProfile& profile);
		void ConnectLocal(ConnectionId connection, const CharacterProfile& profile);
		void Disconnect();

		void Update(float dt, const InputSampler& sampleInput = {});
		void Predict(float dt, const InputSampler& sampleInput);
		void Receive(float dt);
		void SetTickPhase(float accumulator) { m_Accumulator = accumulator; m_TickLocked = m_Local; }
		void SetViewAim(float aim) { m_ViewAim = aim; m_HasViewAim = true; }
		void EquipItem(uint32_t packSlot);
		void EquipItemTo(uint32_t packSlot, GearSlot slot);
		void UnequipItem(GearSlot slot);
		void DropItem(uint32_t packSlot);
		void SlotGem(uint32_t packSlot, uint32_t gemSlot);
		void UnslotGem(uint32_t gemSlot);
		void ChooseUpgrade(uint32_t choice);
		void DepositItem(uint32_t packSlot, uint32_t stashSlot = AnySlot);
		void WithdrawItem(uint32_t stashSlot, uint32_t packSlot = AnySlot);
		void SalvageItem(ItemRef item);
		void ForgeItem(ItemRef item, ForgeAction action, uint32_t affix = 0, StatCategory category = StatCategory::Offense);
		void CraftGem(ItemRef gem, GemAction action);
		void StartMission(MapKind map, uint8_t tier, ModifierMask modifiers);
		void StartHorde(MapKind map);
		void StartArena(MapKind map, const MatchRules& rules);
		void StartRoyale(MapKind map);
		void StartBrawl(MapKind map, const MatchRules& rules);
		void PickBrawlSpell(uint32_t slot, AbilityId spell);
		void ChooseBoon(uint32_t choice);
		void SetBrawlReady(bool ready);
		void Spectate(int32_t direction);
		void ReturnToHub();
		void OpenChest();
		void UseGateway();

		ClientState GetState() const { return m_State; }
		bool IsLocal() const { return m_Local; }
		const std::string& GetDisconnectReason() const { return m_DisconnectReason; }
		const Level* GetLevel() const { return m_Level.get(); }
		const TileMap* GetMap() const { return m_Level ? &m_Level->Map : nullptr; }
		uint32_t GetTeleportCount() const { return m_Teleports; }
		const std::vector<EntityState>& GetEntities() const { return m_View; }
		const EntityState* FindEntity(uint32_t netId) const;
		const std::vector<PlayerInfo>& GetPlayers() const { return m_Players; }
		const PlayerInfo* FindPlayer(uint32_t playerId) const;
		const LocalPlayerState& GetLocalPlayer() const { return m_LocalPlayer; }
		const PlayerState& GetPredictedState() const { return m_PredictedState; }
		const PlayerTraits& GetTraits() const { return m_LocalPlayer.Traits; }
		const ServerInventory& GetInventory() const { return m_Inventory; }
		bool HasInventory() const { return m_HasInventory; }
		uint32_t GetInventoryVersion() const { return m_InventoryVersion; }
		const StashTab& GetStash() const { return m_Profile.Gear.Stash; }
		uint32_t GetStashVersion() const { return m_StashVersion; }
		const MaterialStock& GetMaterials() const { return m_Profile.Gear.Materials; }
		const SessionInfo& GetSession() const { return m_Session; }
		uint32_t GetTravelCount() const { return m_Travels; }
		std::vector<ServerNotice> TakeNotices() { return std::exchange(m_Notices, {}); }
		std::vector<KillRecord> TakeKills() { return std::exchange(m_Kills, {}); }
		bool IsHostile(const EntityState& entity) const;
		uint8_t GetTeam(uint32_t playerId) const;
		const std::vector<HitEvent>& GetNewHits() const { return m_NewHits; }
		const CharacterProfile& GetProfile() const { return m_Profile; }
		const GameStatus& GetStatus() const { return m_Status; }
		uint32_t GetPlayerId() const { return m_Welcome.PlayerId; }
		uint32_t GetPlayerNetId() const { return m_Welcome.PlayerNetId; }
		uint32_t GetTickRate() const { return m_Welcome.TickRate; }
		double GetRenderTick() const { return m_RenderTick; }
		double GetPresentTick() const { return m_PresentTick; }
		double GetInterpolationDelay() const { return m_InterpolationDelay; }
		uint32_t GetLatestTick() const { return m_Snapshots.empty() ? 0 : m_Snapshots.back().Tick; }
		float GetSnapshotAge() const { return m_SnapshotAge; }
		int32_t GetPing() const { return m_Net.GetPing(); }
		ConnectionStats GetConnectionStats() const { return m_Net.GetStats(); }
		uint32_t GetPendingInputCount() const { return uint32_t(m_PendingInputs.size()); }
		uint32_t GetCorrectionCount() const { return m_Corrections; }
		float GetLastCorrection() const { return m_LastCorrection; }
		uint32_t GetStarvedFrames() const { return m_StarvedFrames; }

	private:
		struct PredictedProjectile
		{
			uint16_t Tag = 0;
			uint8_t Index = 0;
			AbilityId Ability = AbilityId::Bolt;
			Vec2 Position;
			Vec2 Previous;
			Vec2 Velocity;
			float Lifetime = 0.0f;
			float Radius = 0.0f;
			uint8_t Pierce = 0;
			bool Explodes = false;
			uint32_t LastHit = 0;
		};

		struct PredictedEffect
		{
			uint16_t Tag = 0;
			uint8_t Index = 0;
			AbilityId Ability = AbilityId::Bolt;
			Vec2 Position;
			float Facing = 0.0f;
			float Remaining = 0.0f;
			uint8_t Scale = 16;
			bool Pending = false;
		};

		struct PendingHit
		{
			uint32_t Tick = 0;
			HitEvent Hit;
		};

		void HandleEvent(const NetEvent& event);
		void HandleMessage(const NetMessage& message);
		void OnSnapshot(ServerSnapshot&& snapshot);
		void OnTravel(const ServerTravel& travel);
		void ResetView();
		void Reconcile(const ServerSnapshot& snapshot);
		void PredictTick(PlayerInput input);
		void CollectBodies(uint32_t ticksAhead, std::vector<BodyCircle>& bodies) const;
		void PredictAbility(const SkillTraits& skill, uint16_t tag, const PlayerActions& actions, const PlayerTraits& traits);
		void ReleaseHits();
		void SendAction(ActionType action, uint32_t value, uint32_t target = 0, uint32_t extra = 0);
		void StepPredictedEffects(float dt);
		void AdvanceClock(float dt);
		void Interpolate();
		void ComposeView(float alpha);
		Vec2 ExtrapolateProjectile(const EntityState& entity, double ticks) const;
		void RememberTag(uint16_t tag);
		bool IsPredictedTag(uint16_t tag) const;
		float GetTickSeconds() const { return 1.0f / float(m_Welcome.TickRate > 0 ? m_Welcome.TickRate : 60u); }
		void Reset(std::string reason);

		template<typename T>
		void Send(const T& message, SendMode mode)
		{
			m_Net.Send(Encode(message), mode);
		}

		NetClient m_Net;
		ClientState m_State = ClientState::Disconnected;
		bool m_Local = false;
		CharacterProfile m_Profile;
		std::string m_DisconnectReason;
		ServerWelcome m_Welcome;
		SessionInfo m_Session;
		uint32_t m_Travels = 0;
		uint32_t m_StashVersion = 0;
		std::vector<ServerNotice> m_Notices;
		std::vector<KillRecord> m_Kills;
		std::unique_ptr<Level> m_Level;
		uint32_t m_Teleports = 0;
		std::vector<PlayerInfo> m_Players;
		GameStatus m_Status;
		LocalPlayerState m_LocalPlayer;
		ServerInventory m_Inventory;
		bool m_HasInventory = false;
		uint32_t m_InventoryVersion = 0;
		std::deque<PendingHit> m_PendingHits;
		std::vector<HitEvent> m_NewHits;

		std::deque<ServerSnapshot> m_Snapshots;
		double m_RenderTick = 0.0;
		double m_PresentTick = 0.0;
		bool m_ClockStarted = false;
		bool m_TickLocked = false;
		double m_InterpolationDelay = 3.0;
		float m_StableTime = 0.0f;
		float m_GrowCooldown = 0.0f;
		bool m_Starved = false;
		uint32_t m_StarvedFrames = 0;
		float m_SnapshotAge = 0.0f;
		std::vector<EntityState> m_Entities;
		std::vector<EntityState> m_View;

		float m_Accumulator = 0.0f;
		uint32_t m_InputSequence = 0;
		std::deque<PlayerInput> m_PendingInputs;
		bool m_HasPrediction = false;
		Transform m_PredictedMotion;
		Transform m_PreviousMotion;
		PlayerState m_PredictedState;
		Vec2 m_CorrectionOffset;
		uint32_t m_Corrections = 0;
		float m_LastCorrection = 0.0f;
		std::vector<BodyCircle> m_Bodies;
		float m_ViewAim = 0.0f;
		bool m_HasViewAim = false;
		std::vector<PredictedProjectile> m_PredictedProjectiles;
		std::vector<PredictedEffect> m_PredictedEffects;
		std::array<uint16_t, 64> m_PredictedTags{};
		uint32_t m_PredictedTagCount = 0;

		std::vector<NetEvent> m_Events;
		std::vector<NetMessage> m_Messages;
	};
}
