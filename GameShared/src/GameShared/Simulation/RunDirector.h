#pragma once
#include "GameShared/Content/Shardfall.h"
#include "GameShared/Core/Math.h"
#include "GameShared/Entity/PlayerComponents.h"
#include "GameShared/Simulation/Abilities.h"
#include "GameShared/Simulation/OrbField.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Game
{
	class World;
	struct EnemyDef;
	struct EnemyScaling;

	struct RunFocus
	{
		RunCardKind Kind = RunCardKind::None;
		CardRarity Floor = CardRarity::Common;
	};

	struct RunSeat
	{
		RunLoadout Loadout;
		bool Present = false;
		bool Paused = false;
		uint64_t BannedSpells = 0;
		uint64_t BannedRanks = 0;
		CardMask BannedCards = 0;
		float Revive = 0.0f;
		std::vector<RunPick> Picks;
		std::array<uint32_t, size_t(AbilityId::Count)> Damage{};
		uint32_t Kills = 0;
		uint16_t Downs = 0;
		uint16_t Revived = 0;
		bool WardUp = false;
		bool WardSpent = false;
		uint8_t Sips = 0;
		float FlowAt = -1e9f;
		bool EchoArmed = false;
		float DodgeAt = -1e9f;
		bool Perfected = true;
		bool Focus = false;
		uint16_t PerfectDodges = 0;
		std::vector<RunFocus> Focused;
		CardRarity Floor = CardRarity::Common;
		Random Rng{ 1 };
		AbilityId LastSpell = AbilityId::Count;
		AbilityContext LastContext;
		uint32_t Casts = 0;
		float EchoAt = -1e9f;
		Entity Grudge;
		float RiftStep = 0.0f;
		std::vector<Entity> RiftHits;
		uint8_t Winds = 0;
		float FeverBank = 0.0f;
		std::array<float, size_t(DuoId::Count)> DuoReady{};
		RuneMask BannedRunes = 0;
		std::array<uint16_t, AbilitySlots> SlotCasts{};
		float LinkReady = -1e9f;
	};

	struct ArenaChange
	{
		MapKind Map = MapKind::ProvingHollow;
		uint32_t Seed = 0;
	};

	class RunDirector
	{
	public:
		void Begin(const RunRules& rules);
		void Update(World& world, float dt);
		void OnKilled(World& world, const EnemyDef& def, Entity entity, const Enemy& enemy, Vec2 position, uint64_t seed, uint32_t attacker);
		bool ApplyPickup(World& world, uint32_t playerId, Vec2 position, LootKind kind);
		void Shockwave(World& world, Vec2 center, float radius, AbilityId visual, uint32_t owner, bool clear, float eliteShare, float stun);
		void OnPlayerSpawned(uint32_t playerId, const BrawlSpells& chosen);
		void OnPlayerRemoved(uint32_t playerId);
		void OnPlayerDowned(uint32_t playerId);
		void NoteDamage(uint32_t attacker, uint8_t ability, int32_t amount);
		void GrantOffers(World& world, uint32_t count);
		bool Pick(World& world, uint32_t playerId, uint32_t choice);
		bool Build(World& world, uint32_t playerId, const RunLoadout& loadout);
		bool Reroll(World& world, uint32_t playerId);
		bool Banish(World& world, uint32_t playerId, uint32_t choice);
		bool Skip(World& world, uint32_t playerId);
		bool SetPaused(const World& world, uint32_t playerId, bool paused);
		bool IsPaused(const World& world) const;
		int32_t ShapeDealt(Entity target, Enemy& enemy, uint32_t attacker, uint8_t ability, int32_t amount);
		int32_t ShapeTaken(World& world, Entity target, PlayerController& controller, bool unmitigated, int32_t amount);
		void AfterDealt(World& world, Entity target, uint32_t attacker, uint8_t ability, int32_t dealt, RuneMask runes, uint8_t depth, bool killed);
		float ChillScale(uint32_t attacker) const { return Has(attacker, CardId::Brittle) ? BrittleChill : 1.0f; }
		void ArmEcho(uint32_t playerId);
		bool ConsumeEcho(uint32_t playerId);
		void NoteDodge(World& world, uint32_t playerId, Vec2 origin);
		void ShapeCast(World& world, PlayerController& controller, uint32_t slot, AbilityId ability, AbilityContext& context);
		void AfterCast(World& world, Entity player, PlayerController& controller, uint32_t slot, AbilityId ability, const AbilityContext& context);
		void OnSlammed(World& world, Entity entity, float speed);
		void OnFlung(World& world, Entity entity);
		void NoteHurt(uint32_t playerId, Entity source);
		float HealScale(uint32_t playerId) const { return playerId < MaxPlayers && HasCurse(m_Seats[playerId].Loadout, CurseId::Greed) ? 0.5f : 1.0f; }
		bool Buy(World& world, uint32_t playerId, uint32_t slot);
		bool Sell(World& world, uint32_t playerId, uint32_t slot);
		bool SellCard(World& world, uint32_t playerId, CardId card);
		bool SwapSigils(World& world, uint32_t playerId, uint32_t from, uint32_t to);
		bool UseShrine(World& world, uint32_t playerId, uint32_t choice);
		bool Socket(World& world, uint32_t playerId, RuneId rune, uint32_t slot, uint32_t socket);
		bool Unsocket(World& world, uint32_t playerId, RuneId rune);
		bool CanArrange(World& world) const;
		void PerfectDodge(World& world, Entity player, uint32_t playerId);
		bool ConsumeFocus(uint32_t playerId);
		bool HasCrystalWard(uint32_t playerId) const { return playerId < MaxPlayers && m_Seats[playerId].WardUp; }
		bool Has(uint32_t playerId, CardId card) const { return HasCard(GetCards(playerId), card); }
		uint8_t Stack(uint32_t playerId, CardId card) const;

		const RunStatus& GetStatus() const { return m_Status; }
		const RunRules& GetRules() const { return m_Rules; }
		const OrbField& GetOrbs() const { return m_Orbs; }
		const RunSeat& GetSeat(uint32_t playerId) const { return m_Seats[playerId < MaxPlayers ? playerId : 0]; }
		const RunLoadout& GetLoadout(uint32_t playerId) const { return GetSeat(playerId).Loadout; }
		CardMask GetCards(uint32_t playerId) const { return playerId < MaxPlayers ? RunCardsOf(m_Seats[playerId].Loadout) : 0; }
		float GetRevive(uint32_t playerId) const { return playerId < MaxPlayers ? m_Seats[playerId].Revive : 0.0f; }
		uint32_t GetRevision() const { return m_Revision; }
		bool IsCrumbling() const { return m_Crumbling; }
		std::optional<ArenaChange> TakeArenaChange() { return std::exchange(m_PendingArena, std::nullopt); }
		void OnArenaChanged(World& world);
		const ActMap& GetMap() const { return m_Map; }
		bool RoomsOn() const { return UsesRooms(m_Rules); }

	private:
		struct Arrival
		{
			EntityKind Kind = EntityKind::QuartzCrawler;
			Vec2 Position;
			float Delay = 0.0f;
			float Health = 1.0f;
			bool Elite = false;
			bool Member = false;
			uint8_t Affixes = 0;
			uint16_t Pack = 0;
			uint8_t Rank = 0;
			bool Champion = false;
		};

		struct Recruit
		{
			EntityKind Kind = EntityKind::QuartzCrawler;
			bool Elite = false;
			uint8_t Affixes = 0;
			uint8_t Rank = 0;
			bool Champion = false;
		};

		std::vector<Recruit> Muster(const EncounterDef& encounter, uint32_t players, uint32_t affixes, Random& random) const;
		const EncounterDef* ChooseEncounter(uint8_t tier, Random& random) const;
		void StartRooms(World& world);
		void TravelTo(World& world, uint8_t room);
		void EnterRoom(World& world);
		void SpawnRoom(World& world, uint32_t players);
		void UpdateRoom(World& world, float dt);
		void ClearRoom(World& world);
		void UpdateRest(World& world);
		void UpdateDoors(World& world, float dt);
		void GiveReward(World& world, RoomReward reward);
		void GrantFocused(World& world, RunSeat& seat, RunCardKind focus, CardRarity floor = CardRarity::Common);
		void ConsumeOffer(World& world, RunSeat& seat);
		void UpdateChampion(World& world);
		void UpdateSigils(World& world, float dt);
		void FillShop(RunSeat& seat);
		void GrantShards(uint32_t amount);
		void TriggerDuos(World& world, Entity target, uint32_t attacker, AbilityId source);
		void CastFor(World& world, uint32_t playerId, AbilityId ability, Vec2 origin, Vec2 target, float share, float delay, uint8_t depth = 1, float charge = 0.0f);
		void UpdateRunes(World& world, float dt);
		void TriggerLink(World& world, RunSeat& seat, uint32_t playerId, Entity target, uint8_t depth);
		std::optional<Entity> NearestEnemy(World& world, Vec2 position, float range, Entity except = {}) const;
		void UpdateEncounters(World& world, float dt, uint32_t players);
		void SpawnEncounter(World& world, uint32_t players);
		void ClearEncounter(World& world);
		void SpendOnly(World& world, float dt, uint32_t players);
		void Arrive(World& world, float dt, uint32_t players);
		void Recycle(World& world, float dt);
		void GainShards(World& world, uint32_t worth);
		void UpdateGate(World& world);
		void UpdateBoss(World& world);
		void UpdateRespite(World& world, float dt);
		void UpdateDowned(World& world, float dt);
		void UpdateCards(World& world, float dt);
		void ShatterAt(World& world, Vec2 position, int32_t damage, uint32_t attacker);
		void PyreAt(World& world, Vec2 position, int32_t damage, uint32_t attacker);
		void DropPickups(World& world, const EnemyDef& def, const Enemy& enemy, bool guardian, Vec2 position);
		void OpenGate(World& world);
		void StartBoss(World& world);
		void EndBoss(World& world);
		void StartAct(World& world);
		void EndRun(World& world, bool victory);
		void Crumble(World& world);
		void Deal(World& world, RunSeat& seat);
		void DealSlot(World& world, RunSeat& seat, uint32_t slot, bool offTag = false);
		void Announce(World& world, const std::string& text, bool success);
		uint32_t CountInRift(World& world, uint32_t& living) const;
		std::optional<Vec2> ChooseSocket(World& world, float radius);
		EnemyScaling Scale(uint32_t players, EntityKind kind = EntityKind::QuartzCrawler) const;
		bool UsesEncounters() const { return m_Rules.Endless && m_Rules.Spawns > 0 && m_Rules.Only == 0; }

		RunRules m_Rules;
		RunStatus m_Status;
		OrbField m_Orbs;
		std::vector<Arrival> m_Arrivals;
		std::array<RunSeat, MaxPlayers> m_Seats;
		std::vector<Entity> m_Guardians;
		std::vector<Entity> m_Members;
		float m_Budget = 4.0f;
		float m_Treasure = 150.0f;
		float m_ShardCarry = 0.0f;
		float m_RiftHold = 0.0f;
		float m_Lull = FirstLull;
		float m_EncounterClock = 0.0f;
		float m_Reinforce = ReinforceSeconds;
		uint8_t m_Reinforcements = 0;
		uint8_t m_Spawned = 0;
		uint8_t m_Open = 0;
		uint8_t m_OpenTier = 1;
		Vec2 m_LastFall;
		float m_BurnTick = 0.0f;
		float m_Wipe = 0.0f;
		uint32_t m_Revision = 0;
		bool m_Started = false;
		bool m_Crumbling = false;
		std::optional<ArenaChange> m_PendingArena;
		ActMap m_Map;
		uint16_t m_NextPack = 1;
		float m_DoorHold = 0.0f;
		uint8_t m_DoorPick = NoRoom;
		float m_SinceClear = 0.0f;
		uint8_t m_RoomTier = 1;
		RoomReward m_Reward = RoomReward::None;
		Entity m_Champion;
		struct Blast
		{
			Vec2 Position;
			int32_t Damage = 0;
			uint32_t By = NoPlayer;
			float Delay = 0.0f;
		};
		std::vector<Blast> m_Blasts;
		uint32_t m_HeartBank = 0;
	};
}
