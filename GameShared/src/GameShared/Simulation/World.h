#pragma once
#include "GameShared/Content/Crafting.h"
#include "GameShared/Content/Definitions.h"
#include "GameShared/Content/Items.h"
#include "GameShared/Content/Missions.h"
#include "GameShared/Content/Traits.h"
#include "GameShared/Core/Random.h"
#include "GameShared/Entity/Components.h"
#include "GameShared/Entity/EntityRegistry.h"
#include "GameShared/Entity/PlayerComponents.h"
#include "GameShared/Simulation/Abilities.h"
#include "GameShared/Simulation/GameStatus.h"
#include "GameShared/Simulation/HitEvent.h"
#include "GameShared/Simulation/MatchDirector.h"
#include "GameShared/Simulation/MissionDirector.h"
#include "GameShared/Simulation/PlayerSimulation.h"
#include "GameShared/Simulation/WaveDirector.h"
#include "GameShared/World/Level.h"
#include "GameShared/World/Navigation.h"
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Game
{
	constexpr float NarrowBodyRadius = 0.47f;
	constexpr float WideBodyRadius = 0.75f;
	constexpr float InteractRange = 2.6f;

	struct WorldSettings
	{
		uint32_t Seed = 1;
		MapKind Map = MapKind::Town;
		GameMode Mode = GameMode::Horde;
		uint8_t Tier = 1;
		ModifierMask Modifiers = 0;
		MatchRules Rules;
		uint32_t Players = 1;
		uint32_t TickRate = 60;
		float IntermissionSeconds = 6.0f;
		uint32_t FirstWave = 1;
		uint32_t UpgradeEvery = 1;
		float LootRate = 1.0f;
		bool LagCompensation = true;
		bool UnlimitedMaterials = false;
		bool RestartOnDefeat = true;
	};

	enum class WorldEventType : uint8_t
	{
		EnemyKilled,
		PlayerDied,
		LootPicked,
		InventoryChanged,
		MaterialsChanged,
		StashChanged,
		UpgradesOffered,
		WaveStarted,
		WaveCleared,
		Defeat,
		Teleported,
		BossAwakened,
		BossDefeated,
		MissionFailed,
		ChestOpened,
		MatchStarted,
		MatchEnded,
		StormClosing,
		CacheDropped,
		RoundStarted,
		RoundEnded,
	};

	struct WorldEvent
	{
		WorldEventType Type = WorldEventType::EnemyKilled;
		uint32_t PlayerId = NoPlayer;
		uint8_t Detail = 0;
	};

	struct WorldStats
	{
		uint32_t PlayerProjectiles = 0;
		uint32_t PlayerProjectileHits = 0;
		double RewindTicks = 0.0;
	};

	class World
	{
	public:
		explicit World(const WorldSettings& settings);

		void Step();
		PlayerActions ApplyPlayerInput(Entity player, const PlayerInput& input);

		Entity SpawnPlayer(uint32_t playerId, const GearSet& gear);
		Entity SpawnPlayer(uint32_t playerId, StarterKit kit);
		Entity SpawnPlayer(uint32_t playerId, const Inventory& inventory);
		void RespawnPlayer(Entity player);
		void RemovePlayer(Entity player);
		void KillPlayer(Entity player, uint32_t killer, uint8_t ability);
		void RefreshTraits(Entity player);
		Entity FindPlayer(uint32_t playerId) const;
		Entity SpawnEnemy(EntityKind kind, Vec2 position, const EnemyScaling& scaling = {});
		Entity SpawnBoss(EntityKind kind, Vec2 position, uint32_t level);
		Entity SpawnProjectile(const AbilityDef& ability, const ProjectilePattern& pattern, Vec2 position, Vec2 direction, int32_t damage, const AbilityContext& context);
		Entity SpawnProjectileCopy(const Projectile& projectile, Vec2 position, uint32_t owner);
		Entity SpawnEffect(const AbilityDef& ability, const AbilityContext& context, Vec2 position, uint16_t tag);
		Entity SpawnEffect(AbilityId ability, Vec2 position, float facing, uint32_t owner, uint16_t tag, float scale);
		Entity SpawnPendingStrike(const AbilityDef& ability, Vec2 position, int32_t damage, const AbilityContext& context);
		Entity SpawnLoot(LootKind kind, Vec2 position);
		Entity SpawnItem(const Item& item, Vec2 position, uint32_t blockedPlayer, uint32_t forPlayer = NoPlayer);
		Entity SpawnChest(Vec2 position, const Chest& chest);
		Entity SpawnGateway(Vec2 position);
		bool ApplyLoot(Entity player, const Loot& loot);
		bool EquipItem(Entity player, uint32_t packSlot);
		bool EquipItemTo(Entity player, uint32_t packSlot, GearSlot slot);
		bool UnequipItem(Entity player, GearSlot slot);
		bool DropItem(Entity player, uint32_t packSlot);
		bool SlotGem(Entity player, uint32_t packSlot, uint32_t gemSlot);
		bool UnslotGem(Entity player, uint32_t gemSlot);
		bool ChooseUpgrade(Entity player, uint32_t choice);
		bool StashItem(Entity player, uint32_t packSlot, uint32_t stashSlot);
		bool TakeFromStash(Entity player, uint32_t stashSlot, uint32_t packSlot);
		bool SalvageItem(Entity player, ItemRef ref);
		CraftResult ForgeItem(Entity player, ItemRef ref, ForgeAction action, uint32_t affix, StatCategory category);
		CraftResult CraftGem(Entity player, ItemRef ref, GemAction action);
		bool OpenChest(Entity player);
		void Spectate(Entity player, int32_t direction);
		bool PickBrawlSpell(Entity player, uint32_t slot, AbilityId spell);
		bool ChooseBoon(Entity player, uint32_t choice);
		bool SetBrawlReady(Entity player, bool ready);
		bool IsNearGateway(Entity player) const;
		bool IsNearStation(Entity player, StationKind kind) const;
		void GrantMaterials(Entity player, const MaterialStock& materials);
		void OfferUpgrades();
		uint32_t CountPendingUpgrades() const;
		void DestroyEntity(Entity entity);
		void QueueDestroy(Entity entity) { m_Destroyed.push_back(entity); }
		void ResetRun();
		void WakePack(uint16_t pack);
		void OnEnemyKilled(Entity entity, const Enemy& enemy, Vec2 position, uint32_t attacker);
		bool ConsumeRevive();

		bool GetPastPosition(Entity entity, double tick, Vec2& position) const;
		Vec2 FindSpawnPosition(uint32_t playerId) const;
		Vec2 FindFreeSpotNear(Vec2 position, float radius) const;
		Vec2 PlaceDrop(Vec2 origin, Vec2 desired) const;
		std::optional<Vec2> FindEnemySpawn(float minDistance, float bodyRadius = 0.0f);
		uint32_t CountPlayers() const;

		void Emit(const WorldEvent& event) { m_Events.push_back(event); }
		std::vector<WorldEvent> TakeEvents() { return std::exchange(m_Events, {}); }
		void RecordHit(const HitEvent& hit) { m_Hits.push_back(hit); }
		std::vector<HitEvent> TakeHits() { return std::exchange(m_Hits, {}); }
		std::vector<KillRecord> TakeKills() { return std::exchange(m_Kills, {}); }
		void Notify(uint32_t playerId, std::string text, bool success) { m_Notices.push_back({ playerId, std::move(text), success }); }
		uint32_t AllocateItemId() { return m_NextItemId++; }

		struct Notice
		{
			uint32_t PlayerId = NoPlayer;
			std::string Text;
			bool Success = true;
		};
		std::vector<Notice> TakeNotices() { return std::exchange(m_Notices, {}); }

		const WorldSettings& GetSettings() const { return m_Settings; }
		GameMode GetMode() const { return m_Settings.Mode; }
		bool IsHub() const { return m_Settings.Mode == GameMode::Hub; }
		bool IsPvp() const { return IsPvpMode(m_Settings.Mode); }
		bool AreRivals(uint32_t a, uint32_t b) const;
		uint8_t GetTeam(uint32_t playerId) const { return IsPvp() ? m_Match.GetTeam(playerId) : NoTeam; }
		const MatchDirector& GetMatch() const { return m_Match; }
		MissionRewards GetRewards() const;
		float GetTickSeconds() const { return 1.0f / float(m_Settings.TickRate); }
		const Level& GetLevel() const { return m_Level; }
		const TileMap& GetMap() const { return m_Level.Map; }
		EntityRegistry& GetEntities() { return m_Entities; }
		const EntityRegistry& GetEntities() const { return m_Entities; }
		const FlowField& GetNavigation(float bodyRadius) const { return bodyRadius > NarrowBodyRadius ? m_WideNavigation : m_Navigation; }
		const NavGrid& GetNavGrid() const { return m_NavGrid; }
		GameStatus GetStatus() const;
		const MissionDirector& GetMission() const { return m_Mission; }
		WorldStats& GetStats() { return m_Stats; }
		const WorldStats& GetStats() const { return m_Stats; }
		Random& GetRandom() { return m_Random; }
		uint32_t GetTick() const { return m_Tick; }

	private:
		uint32_t AllocateNetId() { return m_NextNetId++; }
		void CollectBodies(Vec2 around, std::vector<BodyCircle>& bodies);
		Item* FindFreePackSlot(Inventory& inventory);
		void AutoEquip(Entity player, uint32_t packSlot);
		void OpenMatchChest(Entity player, const Chest& chest, Vec2 origin);
		Item* ResolveItem(Inventory& inventory, ItemRef ref);
		bool PayFor(Inventory& inventory, const MaterialStock& cost);
		void InventoryChanged(Entity player);
		void UpdateNavigation(float dt);
		void RecordHistory();
		void FlushDestroyed();

		WorldSettings m_Settings;
		Level m_Level;
		EntityRegistry m_Entities;
		NavGrid m_NavGrid;
		FlowField m_Navigation;
		FlowField m_WideNavigation;
		WaveDirector m_Director;
		MissionDirector m_Mission;
		MatchDirector m_Match;
		Random m_Random;
		WorldStats m_Stats;
		std::vector<Entity> m_Destroyed;
		std::vector<WorldEvent> m_Events;
		std::vector<HitEvent> m_Hits;
		std::vector<KillRecord> m_Kills;
		std::vector<Notice> m_Notices;
		std::vector<BodyCircle> m_Bodies;
		float m_NavigationTimer = 0.0f;
		uint32_t m_Tick = 0;
		uint32_t m_NextNetId = 1;
		uint32_t m_NextItemId = 1;
	};
}
