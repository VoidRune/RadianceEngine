#pragma once
#include "GameShared/Content/Achievements.h"
#include "GameShared/Content/Chess.h"
#include "GameShared/Content/Definitions.h"
#include "GameShared/Content/GameMode.h"
#include "GameShared/Content/Traits.h"
#include "GameShared/Core/Random.h"
#include "GameShared/Entity/Components.h"
#include "GameShared/Entity/EntityRegistry.h"
#include "GameShared/Entity/PlayerComponents.h"
#include "GameShared/Simulation/Abilities.h"
#include "GameShared/Simulation/GameStatus.h"
#include "GameShared/Simulation/HitEvent.h"
#include "GameShared/Simulation/MatchDirector.h"
#include "GameShared/Simulation/PlayerSimulation.h"
#include "GameShared/Simulation/RunDirector.h"
#include "GameShared/Simulation/TargetGrid.h"
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
		MapKind Map = MapKind::Hub;
		GameMode Mode = GameMode::Hub;
		MatchRules Rules;
		RunRules Run;
		uint32_t Players = 1;
		uint32_t TickRate = 60;
		bool LagCompensation = true;
	};

	enum class WorldEventType : uint8_t
	{
		EnemyKilled,
		PlayerDied,
		LootPicked,
		Teleported,
		BossAwakened,
		BossDefeated,
		MatchStarted,
		MatchEnded,
		RoundStarted,
		RoundEnded,
		ChessChanged,
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

		Entity SpawnPlayer(uint32_t playerId, const BrawlSpells& spells = DefaultBrawlSpells);
		void RespawnPlayer(Entity player);
		void RemovePlayer(Entity player);
		void KillPlayer(Entity player, uint32_t killer, uint8_t ability);
		void RefreshTraits(Entity player);
		Entity FindPlayer(uint32_t playerId) const;
		Entity SpawnEnemy(EntityKind kind, Vec2 position, const EnemyScaling& scaling = {});
		Entity SpawnBoss(EntityKind kind, Vec2 position, uint32_t level);
		Entity SpawnProjectile(const AbilityDef& ability, const ProjectilePattern& pattern, Vec2 position, Vec2 direction, int32_t damage, const AbilityContext& context);
		Entity SpawnProjectileCopy(const Projectile& projectile, Vec2 position, uint32_t owner, float scale = 1.0f, uint16_t tag = 0);
		Entity SpawnEffect(const AbilityDef& ability, const AbilityContext& context, Vec2 position, uint16_t tag);
		Entity SpawnEffect(AbilityId ability, Vec2 position, float facing, uint32_t owner, uint16_t tag, float scale);
		Entity SpawnPendingStrike(const AbilityDef& ability, Vec2 position, int32_t damage, const AbilityContext& context);
		Entity SpawnCardStrike(AbilityId visual, Vec2 position, float facing, float delay, float radius, float push, uint8_t perks, bool visible, const AbilityContext& context);
		Entity SpawnResonance(const AbilityDef& spell, Vec2 position, float delay, float radius, int32_t damage, float push, const AbilityContext& context);
		bool BreakCrystalWard(Entity target, uint32_t victim) { return m_Match.BreakCrystalWard(*this, target, victim); }
		void NoteSpellHit(uint32_t attacker, uint8_t ability) { m_Match.NoteSpellHit(*this, attacker, ability); }
		void AddCrust(Vec2 center, float radius) { m_Match.AddCrust(m_Level, center, radius); }
		void RecordDamage(uint32_t victim, uint32_t attacker, int32_t amount, uint8_t ability) { m_Match.RecordDamage(victim, attacker, amount, ability); }
		int32_t ShapeBrawlDamage(PlayerState& state, uint32_t victim, uint32_t attacker, uint8_t ability, bool unmitigated, bool chilled, int32_t amount) { return m_Match.ShapeDamage(state, victim, attacker, ability, unmitigated, chilled, amount); }
		void AfterBrawlDamage(Entity target, uint32_t victim, uint32_t attacker, uint8_t ability, int32_t dealt) { m_Match.AfterDamage(*this, target, victim, attacker, ability, dealt); }
		void NoteBlow(uint32_t victim, uint32_t attacker, uint8_t ability, bool pull, uint32_t returned) { m_Match.NoteBlow(victim, attacker, ability, pull, returned); }
		void NoteFling(uint32_t victim, uint32_t attacker, uint8_t ability) { m_Match.NoteFling(*this, victim, attacker, ability); }
		CardMask GetCards(uint32_t playerId) const { return m_Match.GetCards(playerId); }
		Entity SpawnLoot(LootKind kind, Vec2 position);
		bool ApplyLoot(Entity player, const Loot& loot);
		void Spectate(Entity player, int32_t direction);
		bool JoinMatch(Entity player);
		void SeatPlayer(uint32_t playerId, uint8_t seat) { m_Match.SetSeat(playerId, seat); }
		bool PickBrawlSpell(Entity player, uint32_t slot, AbilityId spell);
		bool ChooseCard(Entity player, uint32_t choice);
		bool GrantCard(Entity player, CardId card);
		bool PresentCards(Entity player, const CardOffer& offer);
		bool SetBrawlReady(Entity player, bool ready);
		void OnDummyShoved(const Dummy& dummy, uint32_t attacker);
		void PopBalloon(Dummy& dummy, uint32_t attacker);
		void LaunchBalloon(uint32_t lane);
		bool MakeWish(Entity player);
		bool PlayChess(Entity player, Chess::Command command, uint32_t argument, uint32_t revision);
		bool LeaveChess(uint32_t playerId);
		const Chess::Match& GetChess() const { return m_Chess; }
		uint16_t GetStarlit(uint32_t playerId) const { return playerId < MaxPlayers ? m_Starlit[playerId] : uint16_t(0); }
		bool IsNearStation(Entity player, StationKind kind) const;
		void DestroyEntity(Entity entity);
		void QueueDestroy(Entity entity) { m_Destroyed.push_back(entity); }
		void WakePack(uint16_t pack);
		void OnEnemyKilled(Entity entity, const Enemy& enemy, Vec2 position, uint32_t attacker);
		void LevelUp(uint32_t level, bool announce = true);
		const BrawlSpells& GetChosenSpells(uint32_t playerId) const { return m_Chosen[playerId < MaxPlayers ? playerId : 0]; }
		bool ConsumeRevive(uint32_t playerId);

		bool GetPastPosition(Entity entity, double tick, Vec2& position) const;
		void GetTargetBounds(Entity entity, Vec2 position, float radius, Vec2& low, Vec2& high) const;
		void GatherTargets(Vec2 low, Vec2 high, std::vector<Entity>& found);
		void MarkTargetsMoved() { m_TargetsMoved = true; }
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

		struct Notice
		{
			uint32_t PlayerId = NoPlayer;
			std::string Text;
			bool Success = true;
		};
		std::vector<Notice> TakeNotices() { return std::exchange(m_Notices, {}); }

		struct Award
		{
			uint32_t PlayerId = NoPlayer;
			Achievement Id = Achievement::FirstFling;
			uint32_t Detail = 0;
		};
		void Achieve(uint32_t playerId, Achievement id, uint32_t detail = 0)
		{
			if (playerId < MaxPlayers)
				m_Awards.push_back({ playerId, id, detail });
		}
		std::vector<Award> TakeAwards() { return std::exchange(m_Awards, {}); }

		const WorldSettings& GetSettings() const { return m_Settings; }
		GameMode GetMode() const { return m_Settings.Mode; }
		bool IsHub() const { return m_Settings.Mode == GameMode::Hub; }
		bool IsPvp() const { return IsPvpMode(m_Settings.Mode); }
		bool IsShardfall() const { return m_Settings.Mode == GameMode::Shardfall; }
		bool IsDamageBrawl() const { return m_Settings.Mode == GameMode::Brawl && m_Settings.Rules.Mode == BrawlMode::Damage; }
		bool UsesBrawlDamage() const { return IsDamageBrawl() || IsHub() || IsShardfall(); }
		bool AreRivals(uint32_t a, uint32_t b) const;
		ArenaState GetArena() const;
		uint8_t GetTeam(uint32_t playerId) const { return IsPvp() ? m_Match.GetTeam(playerId) : NoTeam; }
		const MatchDirector& GetMatch() const { return m_Match; }
		float GetTickSeconds() const { return 1.0f / float(m_Settings.TickRate); }
		const Level& GetLevel() const { return m_Level; }
		const TileMap& GetMap() const { return m_Level.Map; }
		EntityRegistry& GetEntities() { return m_Entities; }
		const EntityRegistry& GetEntities() const { return m_Entities; }
		const FlowField& GetNavigation(float bodyRadius) const { return bodyRadius > NarrowBodyRadius ? m_WideNavigation : m_Navigation; }
		const NavGrid& GetNavGrid() const { return m_NavGrid; }
		GameStatus GetStatus() const;
		const RunDirector& GetRun() const { return m_Run; }
		WorldStats& GetStats() { return m_Stats; }
		const WorldStats& GetStats() const { return m_Stats; }
		Random& GetRandom() { return m_Random; }
		uint32_t GetTick() const { return m_Tick; }

	private:
		uint32_t AllocateNetId() { return m_NextNetId++; }
		void CollectBodies(Vec2 around, std::vector<BodyCircle>& bodies);
		void SpawnDummy(uint32_t spot, DummyKind kind);
		void UpdateHub(float dt);
		void UpdateBalloon(float dt);
		void UpdateEchoes(float dt);
		void CastCopies(AbilityId ability, const AbilityContext& context);
		void StargazeAt(const PlayerController& controller, Vec2 origin, const PlayerActions& actions);
		PlayerTraits ComputeTraits(uint32_t playerId) const;
		void UpdateNavigation(float dt);
		void RebuildTargets();
		void RecordHistory();
		void FlushDestroyed();

		WorldSettings m_Settings;
		Level m_Level;
		EntityRegistry m_Entities;
		NavGrid m_NavGrid;
		FlowField m_Navigation;
		FlowField m_WideNavigation;
		MatchDirector m_Match;
		RunDirector m_Run;
		TargetGrid m_TargetGrid;
		std::vector<Entity> m_TargetEntities;
		std::vector<uint32_t> m_TargetFound;
		bool m_TargetsMoved = true;
		Random m_Random;
		WorldStats m_Stats;
		std::vector<Entity> m_Destroyed;
		std::vector<WorldEvent> m_Events;
		std::vector<HitEvent> m_Hits;
		std::vector<KillRecord> m_Kills;
		std::vector<Notice> m_Notices;
		std::vector<Award> m_Awards;
		std::vector<BodyCircle> m_Bodies;
		struct EchoCast
		{
			Entity Caster;
			AbilityId Ability = AbilityId::Count;
			AbilityContext Context;
			float Remaining = 0.0f;
			float Strength = EchoPushScale;
		};
		std::vector<EchoCast> m_Echoes;
		std::array<BrawlSpells, MaxPlayers> m_Chosen;
		std::vector<std::pair<uint32_t, float>> m_DummyRespawns;
		uint32_t m_ShrineVisitors = 0;
		uint32_t m_DuckFans = 0;
		uint32_t m_HubFalls = 0;
		std::array<uint16_t, MaxPlayers> m_Starlit{};
		std::array<float, MaxPlayers> m_WishCooldowns{};
		Chess::Match m_Chess;
		float m_BalloonTimer = 0.0f;
		uint32_t m_BalloonLane = 0;
		float m_NavigationTimer = 0.0f;
		float m_WideNavigationTimer = 0.0f;
		uint32_t m_Tick = 0;
		uint32_t m_NextNetId = 1;
	};
}
