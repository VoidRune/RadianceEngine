#pragma once
#include "GameShared/Content/Brawl.h"
#include "GameShared/Content/Pvp.h"
#include "GameShared/Core/Math.h"
#include "GameShared/Entity/EntityRegistry.h"
#include "GameShared/Simulation/GameStatus.h"
#include "GameShared/World/Level.h"
#include <array>
#include <cstdint>
#include <vector>

namespace Game
{
	class World;
	struct PlayerController;
	struct PlayerState;

	struct MatchScore
	{
		uint16_t Kills = 0;
		uint16_t Deaths = 0;
		uint16_t Assists = 0;
		uint32_t Damage = 0;
		uint8_t Placement = 0;
		bool Entered = false;
		bool Out = false;
	};

	class MatchDirector
	{
	public:
		MatchDirector() { m_Teams.fill(NoTeam); }

		void Begin(World& world);
		void Update(World& world, float dt);
		void OnPlayerSpawned(World& world, Entity player, uint32_t playerId);
		void OnPlayerRemoved(World& world, uint32_t playerId);
		void OnPlayerKilled(World& world, Entity player, uint32_t victim, uint32_t killer);
		Vec2 FindSpawn(World& world, uint32_t playerId);
		bool CanRespawn() const;
		bool CanRespawn(uint32_t playerId) const { return !IsSpectator(playerId) && CanRespawn(); }
		bool IsFighting() const;
		void SetSeat(uint32_t playerId, uint8_t seat);
		bool JoinMatch(World& world, Entity player, uint32_t playerId);
		void Describe(const World& world, GameStatus& status) const;
		void Watch(World& world, Entity player, int32_t direction);
		bool PickSpell(World& world, Entity player, uint32_t playerId, uint32_t slot, AbilityId spell);
		bool ChooseCard(World& world, Entity player, uint32_t playerId, uint32_t choice);
		bool GrantCard(World& world, Entity player, uint32_t playerId, CardId card);
		bool PresentCards(uint32_t playerId, const CardOffer& offer);
		void AddCrater(Vec2 center, float radius);
		void ArmEcho(uint32_t playerId);
		bool ConsumeEcho(uint32_t playerId);
		bool SetReady(uint32_t playerId, bool ready);

		MatchPhase GetPhase() const { return m_Phase; }
		float GetPhaseTime() const { return m_PhaseTime; }
		uint8_t GetTeam(uint32_t playerId) const { return playerId < MaxPlayers ? m_Teams[playerId] : NoTeam; }
		bool AreAllies(uint32_t a, uint32_t b) const { return m_Rules.Teams >= 2 && a != b && GetTeam(a) != NoTeam && GetTeam(a) == GetTeam(b); }
		bool IsPresent(uint32_t playerId) const { return playerId < MaxPlayers && m_Present[playerId]; }
		bool IsSpectator(uint32_t playerId) const { return m_Mode == GameMode::Brawl && playerId < MaxPlayers && m_Seats[playerId] == SpectatorSeat; }
		bool IsTeamBrawl() const { return m_Mode == GameMode::Brawl && m_Rules.Teams >= 2; }
		const MatchRules& GetRules() const { return m_Rules; }
		const MatchScore& GetScore(uint32_t playerId) const { return m_Scores[playerId < MaxPlayers ? playerId : 0]; }
		const StormState& GetStorm() const { return m_Storm; }
		uint32_t GetWinner() const { return m_Winner; }
		uint8_t GetWinningTeam() const { return m_WinningTeam; }
		uint32_t GetEntrants() const { return m_Entrants; }
		uint32_t CountAlive() const;
		MatchReward GetReward(const World& world, uint32_t playerId) const;
		const std::vector<Vec2>& GetSpawnPoints() const { return m_SpawnPoints; }
		const BrawlStatus& GetBrawl() const { return m_Brawl; }
		const BrawlLoadout& GetLoadout(uint32_t playerId) const { return m_Brawlers[playerId < MaxPlayers ? playerId : 0].Loadout; }
		PlayerTraits GetBrawlTraits(uint32_t playerId) const;
		void RollArcana(World& world, uint32_t playerId, int32_t slot);
		BrawlMode GetBrawlMode() const { return m_Rules.Mode; }
		bool IsDamageMode() const { return m_Mode == GameMode::Brawl && m_Rules.Mode == BrawlMode::Damage; }
		CardMask GetCards(uint32_t playerId) const;
		bool HasPlayerCard(uint32_t playerId, CardId id) const { return HasCard(GetCards(playerId), id); }
		uint32_t GetCardRevision() const { return m_CardRevision; }
		float GetKnockbackScale(uint32_t attacker, uint32_t victim, float healthFraction, bool pull = false) const;
		void RecordDamage(uint32_t victim, uint32_t attacker, int32_t amount, uint8_t ability);
		uint32_t CreditKill(uint32_t victim, uint8_t& ability) const;
		bool IsBurning(uint32_t playerId) const { return playerId < MaxPlayers && m_Brawlers[playerId].Burning; }
		bool IsIgnited(uint32_t playerId) const { return playerId < MaxPlayers && m_Brawlers[playerId].IgniteLeft > 0.0f; }
		uint32_t GetMarkedBy(uint32_t playerId) const { return playerId < MaxPlayers && m_Brawlers[playerId].MarkTime > 0.0f ? m_Brawlers[playerId].MarkedBy : NoPlayer; }
		float GetShield(uint32_t playerId) const { return playerId < MaxPlayers ? m_Brawlers[playerId].Shield : 0.0f; }
		float CastDamageScale(uint32_t playerId, float healthFraction, float manaBefore, float maxMana, bool overloaded, float still) const;
		int32_t ShapeDamage(PlayerState& state, uint32_t victim, uint32_t attacker, uint8_t ability, bool unmitigated, bool chilled, int32_t amount);
		void AfterDamage(World& world, Entity target, uint32_t victim, uint32_t attacker, uint8_t ability, int32_t dealt);
		void NoteDeath(uint32_t victim, uint32_t killer, Vec2 position, bool chilled);
		void NoteCast(uint32_t playerId);
		void NoteBlow(uint32_t victim, uint32_t attacker, uint8_t ability, bool pull, uint32_t returned);
		void NoteFling(World& world, uint32_t victim, uint32_t attacker, uint8_t ability);
		void NoteSwap(World& world, uint32_t victim, uint32_t attacker, Vec2 landing);
		void NoteKnockout(World& world, uint32_t victim, uint32_t killer, bool hazard);
		uint16_t GetPoints(uint32_t playerId) const { return playerId < MaxPlayers ? m_Brawlers[playerId].Points : uint16_t(0); }
		uint8_t GetRoundWins(uint32_t playerId) const { return playerId < MaxPlayers ? m_Brawlers[playerId].RoundWins : uint8_t(0); }

	private:
		struct PadState
		{
			PickupPad Pad;
			Entity Loot;
			float Timer = 0.0f;
		};

		struct DamageEntry
		{
			uint32_t Attacker = NoPlayer;
			float Amount = 0.0f;
			float Time = 0.0f;
			uint8_t Ability = uint8_t(AbilityId::Count);
		};

		static constexpr size_t LedgerSize = 32;

		struct DamageLedger
		{
			std::array<DamageEntry, LedgerSize> Entries{};
			uint32_t Next = 0;
		};

		struct FeatMark
		{
			uint32_t By = NoPlayer;
			float At = 0.0f;
		};

		struct Fling
		{
			uint32_t Attacker = NoPlayer;
			uint8_t Ability = 0;
			uint32_t Victims = 0;
		};

		struct Brawler
		{
			BrawlLoadout Loadout;
			uint16_t Points = 0;
			uint8_t RoundWins = 0;
			float Afterburn = 0.0f;
			float LavaBank = 0.0f;
			Vec2 Anchor;
			float Still = 0.0f;
			bool Burning = false;
			float FeatherGrace = 0.0f;
			bool FeatherUsed = false;
			bool EchoArmed = false;
			float Shield = 0.0f;
			float IgniteLeft = 0.0f;
			float IgniteRate = 0.0f;
			float IgniteBank = 0.0f;
			uint32_t IgniteBy = NoPlayer;
			float MarkTime = 0.0f;
			uint32_t MarkedBy = NoPlayer;
			bool SecondWindUsed = false;
			BrawlSpells Arcana{ AbilityId::Count, AbilityId::Count, AbilityId::Count };
			uint32_t RoundKnockouts = 0;
			uint32_t RoundCasts = 0;
			uint16_t EnteredRounds = 0;
			uint16_t WonRounds = 0;
			FeatMark Hooked;
			FeatMark Swapped;
			FeatMark Returned;
		};

		struct Burst
		{
			Vec2 Center;
			uint32_t Owner = NoPlayer;
			CardId Card = CardId::Count;
		};

		void BeginArena(World& world);
		void BeginRoyale(World& world);
		void StartFight(World& world);
		void EndMatch(World& world, uint32_t winner, uint8_t team);
		void UpdateArena(World& world, float dt);
		void UpdateRoyale(World& world, float dt);
		void UpdateStorm(World& world, float dt);
		void UpdateWatchers(World& world);
		void SpawnChest(World& world, Vec2 position, ChestKind kind);
		void SpawnGuardians(World& world, Vec2 position);
		void DropCache(World& world);
		void DropBelongings(World& world, Entity player);
		void PlanStorm(World& world);
		void FinishRoyaleIfDecided(World& world);
		Vec2 RandomPointIn(World& world, Vec2 center, float radius, float clearance) const;
		void SetPhase(MatchPhase phase);
		void BeginBrawl(World& world);
		void UpdateBrawl(World& world, float dt);
		void PlanBrawlRound(World& world);
		void FitBrawlArena();
		void PlaceBrawlers(World& world);
		void StartBrawlCountdown(World& world);
		void StartBrawlRound(World& world);
		void EndBrawlRound(World& world);
		void OpenBrawlPick(World& world);
		void FinishBrawl(World& world);
		void UpdateLava(World& world, float dt);
		void UpdateFalls(World& world, float dt);
		void UpdateBolts(World& world, float dt);
		std::optional<Vec2> AimBolt(World& world);
		bool Catch(World& world, Entity entity, PlayerController& controller);
		void UpdateCards(World& world, float dt);
		void Ignite(uint32_t victim, uint32_t attacker, float damage, float seconds);
		void OfferCards(World& world, uint32_t playerId);
		bool Featherfall(World& world, PlayerController& controller, Vec2 position);
		void OnBrawlerKilled(World& world, Entity player, uint32_t victim, uint32_t killer);
		void AwardAssists(uint32_t victim, uint32_t killer);
		void AwardPlacements(uint32_t alive);
		uint32_t CountPresent() const;
		bool AllBrawlersReady() const;
		void Bench(World& world, Entity player);
		uint8_t SmallestTeam(uint32_t playerId) const;
		uint32_t CountTeams(bool aliveOnly) const;
		bool TeamClinched() const;
		bool WonRound(uint32_t playerId) const;
		bool IsApexDraft() const { return IsDamageMode() && m_Rules.Apex && m_Rules.ApexDraft; }
		void OfferApexPick(World& world, uint32_t playerId);
		bool TracksFeats() const { return m_Mode == GameMode::Brawl && m_Brawl.Phase == BrawlPhase::Fight; }
		void NoteRoundWin(World& world, uint32_t playerId);
		void NoteMatchWin(World& world, uint32_t playerId);
		void NoteDeck(World& world, uint32_t playerId, CardId card);

		GameMode m_Mode = GameMode::Arena;
		MatchRules m_Rules;
		MatchPhase m_Phase = MatchPhase::Countdown;
		float m_PhaseTime = 0.0f;
		float m_Clock = 0.0f;
		std::array<uint8_t, MaxPlayers> m_Teams{};
		MatchSeats m_Seats = AutoSeats;
		std::array<bool, MaxPlayers> m_Present{};
		std::array<MatchScore, MaxPlayers> m_Scores{};
		std::array<uint16_t, MaxTeams> m_TeamScores{};
		uint32_t m_Winner = NoPlayer;
		uint8_t m_WinningTeam = NoTeam;
		uint32_t m_Entrants = 0;
		std::vector<uint32_t> m_Fallen;
		std::vector<Vec2> m_SpawnPoints;
		std::vector<PadState> m_Pads;
		StormState m_Storm;
		Vec2 m_ShrinkFrom;
		float m_ShrinkFromRadius = 0.0f;
		float m_ShrinkDuration = 1.0f;
		float m_StormTick = 0.0f;
		float m_TimeScale = 1.0f;
		uint16_t m_NextPack = 1;
		uint32_t m_SpawnCounter = 0;
		bool m_Deferring = false;
		std::array<Brawler, MaxPlayers> m_Brawlers{};
		std::array<DamageLedger, MaxPlayers> m_Ledgers{};
		std::vector<Burst> m_Bursts;
		std::vector<Fling> m_Flings;
		uint32_t m_FlingTick = UINT32_MAX;
		float m_IgniteTick = 0.0f;
		BrawlStatus m_Brawl;
		float m_LavaTick = 0.0f;
		float m_NextBolt = 0.0f;
		uint32_t m_CardRevision = 0;
	};
}
