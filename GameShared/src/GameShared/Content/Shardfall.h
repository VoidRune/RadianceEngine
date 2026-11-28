#pragma once
#include "GameShared/Content/Brawl.h"
#include "GameShared/Content/GameMode.h"
#include "GameShared/Content/RunMap.h"
#include "GameShared/Content/RunItems.h"
#include "GameShared/Content/Runes.h"
#include "GameShared/World/Level.h"
#include "GameShared/Entity/Components.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace Game
{
	inline constexpr uint32_t ShardfallMaxPlayers = 4;
	inline constexpr uint32_t ShardfallEnemyCap = 96;
	inline constexpr float ActSeconds = 420.0f;
	inline constexpr float ScalingActSeconds = 270.0f;
	inline constexpr float RunDamagePerMinute = 0.012f;
	inline constexpr uint8_t RunActs = 3;
	inline constexpr float RespiteSeconds = 45.0f;
	inline constexpr float GateRadius = 2.6f;
	inline constexpr float RiftHoldSeconds = 1.5f;
	inline constexpr float GuardianSpread = 5.5f;
	inline constexpr float GuardianHealth = 4.0f;
	inline constexpr float EscortHealth = 2.0f;
	inline constexpr uint8_t GuardianCount = 3;
	inline constexpr uint32_t EliteWorth = 2;
	inline constexpr float EliteFighterHealth = 2.5f;
	inline constexpr float EliteFighterDamage = 1.15f;
	inline constexpr uint8_t ActEncounters = 7;
	inline constexpr float FirstLull = 3.0f;
	inline constexpr float EncounterLull = 4.0f;
	inline constexpr float EncounterTimeout = 75.0f;
	inline constexpr float EncounterSpread = 2.2f;
	inline constexpr uint32_t ClearWorth = 12;
	inline constexpr uint32_t ClearWorthPerTier = 10;
	inline constexpr uint8_t ChestEvery = 3;
	inline constexpr uint32_t FighterOnlyGoal = 3;
	inline constexpr float ReinforceSeconds = 8.0f;
	inline constexpr uint32_t ReinforceCrawlers = 3;
	inline constexpr uint8_t ReinforceWaves = 2;
	inline constexpr uint32_t EncounterCrowd = 10;
	inline constexpr uint32_t EncounterCrowdPerPlayer = 4;
	inline constexpr uint32_t ClearStragglers = 2;
	inline constexpr float PerfectDodgeWindow = 0.3f;
	inline constexpr float AscensionRadius = 4.5f;
	inline constexpr float AscensionStun = 0.5f;
	inline constexpr float ReviveBurstRadius = 3.5f;
	inline constexpr float ReviveBurstStun = 2.0f;
	inline constexpr float PickupReach = 14.0f;
	inline constexpr float BombEliteShare = 0.25f;
	inline constexpr float FrostSeconds = 4.0f;
	inline constexpr float PickupLifetime = 40.0f;
	inline constexpr float ChestLifetime = 90.0f;
	inline constexpr float MagnetChance = 0.005f;
	inline constexpr float BombChance = 0.004f;
	inline constexpr float FrostChance = 0.003f;
	inline constexpr float ReviveSeconds = 3.0f;
	inline constexpr float ReviveRange = 2.2f;
	inline constexpr float ReviveDecay = 0.5f;
	inline constexpr float ReviveHealth = 0.3f;
	inline constexpr float ReviveProtection = 2.0f;
	inline constexpr uint8_t SoloRevives = 1;
	inline constexpr float WipeSeconds = 2.0f;
	inline constexpr float RunEndSeconds = 120.0f;
	inline constexpr uint8_t RunRerolls = 2;
	inline constexpr uint8_t RunBanishes = 1;
	inline constexpr float BossHeal = 0.3f;
	inline constexpr uint8_t MaxSpellRank = 5;
	inline constexpr uint32_t RunStartSpells = 2;
	inline constexpr float RankDamage = 0.2f;
	inline constexpr float RankCooldown = 0.9f;
	inline constexpr float RunMana = 100.0f;
	inline constexpr float RunManaRegen = 1.0f;
	inline constexpr float RunOvercastCost = 60.0f;
	inline constexpr float RunBasicMana = 2.0f;
	inline constexpr float RunKillMana = 2.0f;
	inline constexpr float RunPerfectMana = 25.0f;
	inline constexpr float RunMoveSpeed = 3.4f;
	inline constexpr float RunCastMove = 0.4f;
	inline constexpr float RunDodgeCooldown = 2.5f;
	inline constexpr uint8_t RunDodgeCharges = 1;
	inline constexpr uint8_t MaxBankedOffers = 60;
	inline constexpr float ShardfallPlayerCrowd = 0.35f;
	inline constexpr float SocketNearest = 12.0f;
	inline constexpr float SocketFarthest = 16.0f;
	inline constexpr float SproutSeconds = 0.8f;
	inline constexpr float BroodReach = 5.0f;
	inline constexpr float BroodInterval = 6.0f;
	inline constexpr size_t BroodCrowdCap = 20;
	inline constexpr float ShieldScale = 0.5f;
	inline constexpr float EggHatchSeconds = 4.0f;
	inline constexpr float BurrowSeconds = 15.0f;
	inline constexpr float BurrowWarning = 0.9f;
	inline constexpr uint32_t RubyShards = 6;
	inline constexpr float OrbMagnet = 4.0f;
	inline constexpr float OrbRecall = 45.0f;
	inline constexpr float ShardsPerWorth = 0.04f;
	inline constexpr float SigilActDamage = 0.35f;

	enum class RunPhase : uint8_t
	{
		Act,
		Gate,
		Boss,
		Respite,
		Victory,
		Defeat,
	};

	inline bool IsRunOver(RunPhase phase)
	{
		return phase == RunPhase::Victory || phase == RunPhase::Defeat;
	}

	struct RunStatus
	{
		RunPhase Phase = RunPhase::Act;
		uint8_t Act = 1;
		float ActClock = 0.0f;
		float RunClock = 0.0f;
		float PhaseClock = 0.0f;
		uint16_t Alive = 0;
		uint16_t Goal = 0;
		uint16_t Rooms = 0;
		uint8_t Guardians = 0;
		uint8_t Rift = 0;
		bool Paused = false;
		uint8_t Wave = 0;
		uint8_t Waves = 0;
		float Lull = 0.0f;
		uint8_t Room = NoRoom;
		uint32_t Route = 0;
		bool Cleared = false;
		float RoomClock = 0.0f;
		float Par = 0.0f;
		RoomMedal Medal = RoomMedal::None;
		float Momentum = 0.0f;
		std::array<uint8_t, DoorSlots> Votes{};
		uint16_t Golds = 0;
		uint32_t Champion = 0;
		uint8_t ChampionPhase = 0;
		bool Fighting = false;
	};

	inline constexpr float RoomDepthMinutes = 1.1f;
	inline constexpr uint32_t ExtraEncounterDepth = 4;

	inline float RoomMinutes(uint8_t act, uint8_t room)
	{
		return float((std::max<uint8_t>(act, 1) - 1) * MapLayers + (room != NoRoom ? RoomLayer(room) : 0)) * RoomDepthMinutes;
	}

	inline uint32_t RoomEncounters(uint8_t act, uint8_t room)
	{
		return 1u + (uint32_t(std::max<uint8_t>(act, 1) - 1) + RoomLayer(room) >= ExtraEncounterDepth ? 1u : 0u);
	}

	struct EncounterMember
	{
		EntityKind Kind = EntityKind::QuartzCrawler;
		uint8_t Count = 0;
	};

	struct EncounterDef
	{
		const char* Name = "";
		uint8_t Tier = 1;
		std::array<EncounterMember, 4> Members{};
	};

	inline constexpr EncounterDef Encounters[] = {
		{ "A lone duelist and its crawlers", 1, { { { EntityKind::CrystalDuelist, 1 }, { EntityKind::QuartzCrawler, 8 } } } },
		{ "Two archers in the rocks", 1, { { { EntityKind::ShardArcher, 2 }, { EntityKind::QuartzCrawler, 6 }, { EntityKind::JadeCrawler, 1 } } } },
		{ "A prism mage and its pouncers", 1, { { { EntityKind::PrismMage, 1 }, { EntityKind::JadeCrawler, 3 }, { EntityKind::QuartzCrawler, 4 } } } },
		{ "A duelist with two archers", 2, { { { EntityKind::CrystalDuelist, 1 }, { EntityKind::ShardArcher, 2 }, { EntityKind::QuartzCrawler, 6 } } } },
		{ "Blade and prism", 2, { { { EntityKind::CrystalDuelist, 1 }, { EntityKind::PrismMage, 1 }, { EntityKind::QuartzCrawler, 6 }, { EntityKind::JadeCrawler, 2 } } } },
		{ "Twin duelists", 2, { { { EntityKind::CrystalDuelist, 2 }, { EntityKind::SapphireCrawler, 2 }, { EntityKind::QuartzCrawler, 4 } } } },
		{ "Crossfire", 3, { { { EntityKind::ShardArcher, 2 }, { EntityKind::PrismMage, 1 }, { EntityKind::JadeCrawler, 3 }, { EntityKind::QuartzCrawler, 4 } } } },
		{ "The vanguard", 3, { { { EntityKind::CrystalDuelist, 2 }, { EntityKind::ShardArcher, 1 }, { EntityKind::QuartzCrawler, 8 } } } },
		{ "The brood's guard", 3, { { { EntityKind::AmethystCrawler, 1 }, { EntityKind::CrystalDuelist, 1 }, { EntityKind::ShardArcher, 1 }, { EntityKind::JadeCrawler, 3 } } } },
		{ "A warband", 4, { { { EntityKind::CrystalDuelist, 2 }, { EntityKind::ShardArcher, 2 }, { EntityKind::PrismMage, 1 }, { EntityKind::QuartzCrawler, 6 } } } },
		{ "The coven", 4, { { { EntityKind::PrismMage, 2 }, { EntityKind::CrystalDuelist, 1 }, { EntityKind::SapphireCrawler, 2 }, { EntityKind::QuartzCrawler, 5 } } } },
		{ "Ruby vanguard", 4, { { { EntityKind::CrystalDuelist, 1 }, { EntityKind::ShardArcher, 2 }, { EntityKind::RubyCrawler, 2 }, { EntityKind::QuartzCrawler, 5 } } } },
		{ "A geode brute wakes", 1, { { { EntityKind::GeodeBrute, 1 }, { EntityKind::QuartzCrawler, 7 } } } },
		{ "Mirror moths", 1, { { { EntityKind::MirrorMoth, 2 }, { EntityKind::QuartzCrawler, 6 } } } },
		{ "A lens in the dark", 1, { { { EntityKind::LensWisp, 1 }, { EntityKind::QuartzCrawler, 6 }, { EntityKind::JadeCrawler, 1 } } } },
		{ "Brute and bow", 2, { { { EntityKind::GeodeBrute, 1 }, { EntityKind::ShardArcher, 1 }, { EntityKind::QuartzCrawler, 6 } } } },
		{ "Lens and blade", 2, { { { EntityKind::LensWisp, 1 }, { EntityKind::CrystalDuelist, 1 }, { EntityKind::QuartzCrawler, 6 } } } },
		{ "Glass wings", 2, { { { EntityKind::MirrorMoth, 2 }, { EntityKind::ShardArcher, 1 }, { EntityKind::JadeCrawler, 2 }, { EntityKind::QuartzCrawler, 3 } } } },
		{ "The obelisk's ward", 3, { { { EntityKind::ShardObelisk, 1 }, { EntityKind::CrystalDuelist, 1 }, { EntityKind::ShardArcher, 1 }, { EntityKind::QuartzCrawler, 6 } } } },
		{ "Lens battery", 3, { { { EntityKind::LensWisp, 2 }, { EntityKind::GeodeBrute, 1 }, { EntityKind::QuartzCrawler, 5 } } } },
		{ "Mirror and prism", 3, { { { EntityKind::MirrorMoth, 1 }, { EntityKind::PrismMage, 1 }, { EntityKind::CrystalDuelist, 1 }, { EntityKind::QuartzCrawler, 4 } } } },
		{ "The obelisk shrine", 4, { { { EntityKind::ShardObelisk, 1 }, { EntityKind::CrystalDuelist, 2 }, { EntityKind::LensWisp, 1 }, { EntityKind::QuartzCrawler, 5 } } } },
		{ "Siege line", 4, { { { EntityKind::GeodeBrute, 2 }, { EntityKind::LensWisp, 1 }, { EntityKind::ShardArcher, 1 }, { EntityKind::QuartzCrawler, 4 } } } },
		{ "The glass court", 4, { { { EntityKind::MirrorMoth, 2 }, { EntityKind::PrismMage, 1 }, { EntityKind::ShardObelisk, 1 }, { EntityKind::QuartzCrawler, 5 } } } },
	};

	inline constexpr MapKind ActArenas[] = { MapKind::ProvingHollow, MapKind::CrystalQuarry, MapKind::GlassTerrace };
	static_assert(std::size(ActArenas) == RunActs);

	inline MapKind ArenaForAct(uint8_t act)
	{
		return ActArenas[std::clamp<uint8_t>(act, 1, RunActs) - 1];
	}

	inline constexpr uint8_t MidActElite = 3;
	inline constexpr float FighterToughness = 1.3f;
	inline constexpr float FighterPace = 1.3f;

	inline uint32_t EncounterAffixes(uint8_t act, uint32_t index)
	{
		if (index + 1u == ActEncounters)
			return act >= 3 ? 2u : 1u;
		return act >= 2 && index == MidActElite ? 1u : 0u;
	}

	inline uint32_t GuardianAffixes(uint8_t act)
	{
		return std::min<uint32_t>(std::max<uint8_t>(act, 1) - 1u, 2u);
	}

	inline uint8_t EncounterTier(uint8_t act, uint32_t index)
	{
		return uint8_t(std::min<uint32_t>(1u + index * 3u / (ActEncounters - 1u) + (std::max<uint8_t>(act, 1) - 1u), 4u));
	}

	inline float FighterHealth(float minutes, uint32_t players)
	{
		return FighterToughness * (1.0f + 0.09f * minutes) * (1.0f + 0.4f * float(std::max(players, 1u) - 1));
	}

	inline constexpr EntityKind RunOnlyKinds[] = { EntityKind::QuartzCrawler, EntityKind::JadeCrawler, EntityKind::SapphireCrawler, EntityKind::RubyCrawler, EntityKind::AmethystCrawler, EntityKind::PrismaticCrawler, EntityKind::CrystalDuelist, EntityKind::ShardArcher, EntityKind::PrismMage, EntityKind::GeodeBrute, EntityKind::MirrorMoth, EntityKind::LensWisp, EntityKind::ShardObelisk };
	inline constexpr uint8_t RandomAffix = 7;
	inline constexpr uint8_t MaxStartMinute = 12;
	inline constexpr uint8_t MaxStartLevel = 30;
	inline constexpr uint16_t MaxSpawnRate = 300;
	inline constexpr uint16_t MinShardRate = 25;
	inline constexpr uint16_t MaxShardRate = 500;

	struct RunRules
	{
		uint8_t StartMinute = 0;
		uint8_t StartLevel = 1;
		uint16_t Spawns = 100;
		uint16_t ShardRate = 100;
		uint8_t Only = 0;
		uint8_t Affix = 0;
		bool Immortal = false;
		bool Endless = false;
		bool AllRunes = false;
		uint32_t Seed = 0;

		bool operator==(const RunRules&) const = default;
	};

	inline bool UsesRooms(const RunRules& rules)
	{
		return !rules.Endless && rules.Only == 0 && rules.Spawns > 0;
	}

	inline constexpr MapKind RoomMaps[] = { MapKind::HollowRoom, MapKind::QuarryRoom, MapKind::TerraceRoom };
	static_assert(std::size(RoomMaps) == RunActs);

	inline MapKind RoomMapFor(uint8_t act, RoomKind kind)
	{
		return kind == RoomKind::Guardian ? ArenaForAct(act) : RoomMaps[std::clamp<uint8_t>(act, 1, RunActs) - 1];
	}

	inline uint32_t RoomLevelSeed(uint32_t runSeed, uint8_t act, const ActMap& map, uint8_t room)
	{
		return RoomSeed(runSeed, act, room, DoorMaskOf(map, room), IsCalmRoom(map.At(room).Kind));
	}

	struct RoomPlace
	{
		uint8_t Act = 0;
		uint8_t Room = NoRoom;
	};

	inline RoomPlace FindRoom(uint32_t runSeed, uint32_t mapSeed)
	{
		for (uint8_t act = 1; act <= RunActs; act++)
		{
			const ActMap map = MakeActMap(runSeed, act);
			for (uint8_t room = 0; room < MapRooms; room++)
				if (map.At(room).Exists() && RoomLevelSeed(runSeed, act, map, room) == mapSeed)
					return { act, room };
		}
		return {};
	}

	inline void PrepareRunSession(MapKind& map, uint32_t& mapSeed, RunRules& run)
	{
		if (run.Seed == 0)
			run.Seed = mapSeed | 1u;
		if (!UsesRooms(run))
			return;
		const ActMap first = MakeActMap(run.Seed, 1);
		map = RoomMapFor(1, first.At(0).Kind);
		mapSeed = RoomLevelSeed(run.Seed, 1, first, 0);
	}

	inline RunRules SanitizeRunRules(RunRules rules)
	{
		rules.StartMinute = std::min(rules.StartMinute, MaxStartMinute);
		rules.StartLevel = std::clamp<uint8_t>(rules.StartLevel, 1, MaxStartLevel);
		rules.Spawns = std::min(rules.Spawns, MaxSpawnRate);
		rules.ShardRate = std::clamp(rules.ShardRate, MinShardRate, MaxShardRate);
		rules.Only = rules.Only > std::size(RunOnlyKinds) ? 0 : rules.Only;
		rules.Affix = rules.Affix > RandomAffix ? 0 : rules.Affix;
		return rules;
	}

	inline uint32_t GetPlayerCap(GameMode mode)
	{
		return mode == GameMode::Shardfall ? ShardfallMaxPlayers : MaxPlayers;
	}

	inline float PartyScale(uint32_t players)
	{
		return 1.0f + ShardfallPlayerCrowd * float(std::max(players, 1u) - 1);
	}

	inline float RunMinutes(uint8_t act, float actClock)
	{
		return (float(std::max<uint8_t>(act, 1) - 1) * ScalingActSeconds + actClock) / 60.0f;
	}

	inline uint32_t GetShardfallPopulation(float actClock, uint32_t players, uint8_t act = 1)
	{
		const float progress = std::clamp(actClock / ActSeconds, 0.0f, 1.0f);
		const float base = 14.0f + 6.0f * float(std::max<uint8_t>(act, 1) - 1);
		return std::min(uint32_t((base + 30.0f * progress) * PartyScale(players) + 0.5f), ShardfallEnemyCap);
	}

	inline float TrashHealth(float minutes, uint32_t players)
	{
		return (0.5f + 0.045f * minutes) * (1.0f + 0.15f * float(std::max(players, 1u) - 1));
	}

	inline bool IsFodder(EntityKind kind)
	{
		return kind == EntityKind::QuartzCrawler;
	}

	using SpellLoadout = std::array<AbilityId, SkillSlots>;

	struct RunSpellDef
	{
		AbilityId Spell = AbilityId::EmberBolt;
		int32_t Damage = 0;
		float Cooldown = 0.0f;
		float Cast = 0.0f;
		const char* Pitch = "";
	};

	inline constexpr RunSpellDef RunSpells[] = {
		{ AbilityId::EmberBolt, 10, 0.45f, 0.18f, "Your basic attack. A quick, weak bolt that fires for as long as you hold the button." },
		{ AbilityId::Comet, 110, 6.0f, 0.5f, "Calls a heavy comet onto your cursor. Slow to land, and the crater hits hard." },
		{ AbilityId::Warp, 0, 8.0f, 0.08f, "Blink a short way to slip out of a pack or line up your area spells." },
		{ AbilityId::GlacialNova, 40, 7.0f, 0.3f, "Frost ripens around you, then erupts on recast. Your panic button and freezer." },
		{ AbilityId::GravityWell, 90, 7.0f, 0.45f, "A void eye pulls a pack together, then bursts." },
		{ AbilityId::ArcLightning, 100, 4.0f, 0.35f, "Lightning that jumps from crawler to crawler through a pack." },
		{ AbilityId::DiamondWard, 0, 14.0f, 0.0f, "Encase yourself in crystal. Spit and shards bounce back at whoever cast them." },
		{ AbilityId::FrostSpear, 50, 4.0f, 0.15f, "Charge a spear that pierces a whole lane, chilling and freezing." },
		{ AbilityId::CrystalSword, 20, 1.2f, 0.34f, "A conjured crystal sword. Three lunging strikes; the overhead cleave reaches farthest and hits hardest." },
		{ AbilityId::Firebomb, 60, 5.0f, 0.35f, "A skipping bomb that bursts along its lane." },
		{ AbilityId::ObsidianBlades, 20, 4.0f, 0.2f, "A fan of seven blades. A shotgun for close range." },
		{ AbilityId::StormStrike, 80, 5.0f, 0.3f, "A delayed bolt on your cursor that jumps on. Good on elites and clumps." },
		{ AbilityId::EarthSpike, 125, 6.0f, 0.3f, "A rock spike bursts out of the ground in an angled lane." },
		{ AbilityId::PurpleFireball, 65, 6.0f, 0.15f, "Charge a fireball from orange to violet. Your boss nuke." },
		{ AbilityId::HexShield, 0, 0.6f, 0.0f, "Hold to raise a hex barrier in front of you." },
	};

	inline const RunSpellDef* FindRunSpell(AbilityId spell)
	{
		for (const RunSpellDef& def : RunSpells)
			if (def.Spell == spell)
				return &def;
		return nullptr;
	}

	enum class RunCardKind : uint8_t
	{
		None,
		Spell,
		Rank,
		Card,
		Sigil,
		Duo,
		Curse,
		Service,
		Rune,
	};

	struct RunCard
	{
		RunCardKind Kind = RunCardKind::None;
		uint8_t Id = 0;

		bool IsEmpty() const { return Kind == RunCardKind::None; }
		AbilityId Spell() const { return AbilityId(Id); }
		CardId Upgrade() const { return CardId(Id); }
		RuneId Rune() const { return RuneId(Id); }
		bool operator==(const RunCard&) const = default;
	};

	using RunOffer = std::array<RunCard, CardChoices>;

	inline uint16_t RunStage(uint8_t act, uint8_t room)
	{
		return uint16_t((uint16_t(act) << 8) | (room != NoRoom ? RoomLayer(room) + 1u : 0u));
	}

	struct RunPick
	{
		RunCard Card;
		uint16_t Stage = 0;
	};

	struct RunLoadout
	{
		SpellLoadout Spells{ AbilityId::Count, AbilityId::Count, AbilityId::Count, AbilityId::Count };
		std::array<uint8_t, AbilitySlots> Ranks{ 1, 1, 1, 1, 1 };
		CardMask Cards = 0;
		CardMask Second = 0;
		CardMask Third = 0;
		RunOffer Offer{};
		uint8_t Offers = 0;
		uint8_t Rerolls = RunRerolls;
		uint8_t Banishes = RunBanishes;
		uint8_t Revives = 0;
		RunCardKind Focus = RunCardKind::None;
		uint8_t Rested = 0;
		uint16_t Shards = 0;
		std::array<SigilId, SigilSlots> Sigils{ SigilId::None, SigilId::None, SigilId::None, SigilId::None, SigilId::None };
		uint8_t Duos = 0;
		uint8_t Curses = 0;
		std::array<RunCard, ShopSlots> Stock{};
		uint8_t Sold = 0;
		uint8_t Wounds = 0;
		uint8_t Draughts = 0;
		std::array<RuneSockets, AbilitySlots> Sockets = EmptyRuneBoard;
		RuneMask Runes = 0;

		bool HasOffer() const { return Offers > 0 && !Offer[0].IsEmpty(); }
		bool operator==(const RunLoadout&) const = default;
	};

	struct RunePlace
	{
		int8_t Slot = -1;
		int8_t Socket = -1;

		bool IsSet() const { return Slot >= 0; }
	};

	inline AbilityId SlotSpell(const RunLoadout& loadout, uint32_t slot)
	{
		return slot == 0 ? BrawlBasic : slot < AbilitySlots ? loadout.Spells[slot - 1] : AbilityId::Count;
	}

	inline uint8_t OpenSockets(const RunLoadout& loadout, uint32_t slot)
	{
		return SlotSpell(loadout, slot) < AbilityId::Count ? SocketsAt(loadout.Ranks[slot]) : uint8_t(0);
	}

	inline RuneMask SlotRunes(const RunLoadout& loadout, uint32_t slot)
	{
		const AbilityId spell = SlotSpell(loadout, slot);
		RuneMask runes = 0;
		for (uint32_t socket = 0; socket < OpenSockets(loadout, slot); socket++)
			if (const RuneId rune = loadout.Sockets[slot][socket]; RuneFits(rune, spell))
				runes |= RuneBit(rune);
		return runes;
	}

	inline RuneMask SocketedRunes(const RunLoadout& loadout)
	{
		RuneMask runes = 0;
		for (const RuneSockets& sockets : loadout.Sockets)
			runes |= SocketMask(sockets);
		return runes;
	}

	inline RuneMask LooseRunes(const RunLoadout& loadout)
	{
		return loadout.Runes & ~SocketedRunes(loadout);
	}

	inline RunePlace FindRune(const RunLoadout& loadout, RuneId rune)
	{
		for (uint32_t slot = 0; slot < AbilitySlots; slot++)
			for (uint32_t socket = 0; socket < MaxSockets; socket++)
				if (rune < RuneId::Count && loadout.Sockets[slot][socket] == rune)
					return { int8_t(slot), int8_t(socket) };
		return {};
	}

	inline bool CanSocket(const RunLoadout& loadout, RuneId rune, uint32_t slot, uint32_t socket)
	{
		return HasRune(loadout.Runes, rune) && slot < AbilitySlots && socket < OpenSockets(loadout, slot) && RuneFits(rune, SlotSpell(loadout, slot));
	}

	inline bool RuneFitsBuild(const RunLoadout& loadout, RuneId rune)
	{
		for (uint32_t slot = 0; slot < AbilitySlots; slot++)
			if (RuneFits(rune, SlotSpell(loadout, slot)))
				return true;
		return false;
	}

	inline AbilityId LinkedSpell(const RunLoadout& loadout, uint32_t slot)
	{
		for (uint32_t step = 1; step < AbilitySlots; step++)
		{
			const uint32_t next = (slot + step) % AbilitySlots;
			if (const AbilityId spell = SlotSpell(loadout, next); next != 0 && IsLinkTarget(spell))
				return spell;
		}
		return AbilityId::Count;
	}

	inline void ApplyRunes(SkillTraits& skill, RuneMask runes)
	{
		if (skill.IsEmpty() || runes == 0)
			return;
		if (HasRune(runes, RuneId::Multicast))
		{
			skill.ExtraProjectiles = uint8_t(skill.ExtraProjectiles + (skill.Ability == AbilityId::ObsidianBlades ? 2 : 1));
			skill.DamageScale *= 1.0f - MulticastCost;
		}
		if (HasRune(runes, RuneId::Pierce))
			skill.ExtraPierce = uint8_t(skill.ExtraPierce + RunePierce);
		if (HasRune(runes, RuneId::Ricochet))
			skill.ExtraChain = uint8_t(skill.ExtraChain + 1);
		if (HasRune(runes, RuneId::Enlarge))
			skill.AreaScale *= EnlargeArea;
		if (HasRune(runes, RuneId::Quickcast))
		{
			skill.CastTime *= QuickcastScale;
			skill.ChargeTime *= QuickcastScale;
		}
	}

	inline int32_t SigilSlotOf(const RunLoadout& loadout, SigilId id)
	{
		for (uint32_t i = 0; i < SigilSlots; i++)
			if (loadout.Sigils[i] == id)
				return int32_t(i);
		return -1;
	}

	inline bool HasSigil(const RunLoadout& loadout, SigilId id)
	{
		return SigilSlotOf(loadout, id) >= 0;
	}

	inline float SigilPower(const RunLoadout& loadout, SigilId id)
	{
		const int32_t slot = SigilSlotOf(loadout, id);
		if (slot < 0)
			return 0.0f;
		return slot == 0 && id != SigilId::Leftmost && HasSigil(loadout, SigilId::Leftmost) ? 2.0f : 1.0f;
	}

	inline uint32_t FreeSigilSlots(const RunLoadout& loadout)
	{
		return uint32_t(std::count(loadout.Sigils.begin(), loadout.Sigils.end(), SigilId::None));
	}

	inline bool HasDuo(const RunLoadout& loadout, DuoId id)
	{
		return (loadout.Duos & (1u << uint32_t(id))) != 0;
	}

	inline bool HasCurse(const RunLoadout& loadout, CurseId id)
	{
		return (loadout.Curses & (1u << uint32_t(id))) != 0;
	}

	inline uint32_t CurseCount(const RunLoadout& loadout)
	{
		return uint32_t(std::popcount(uint32_t(loadout.Curses)));
	}

	inline uint16_t CardSellPrice(CardId id)
	{
		return uint16_t(float(CardPrices[std::min<size_t>(size_t(GetCard(id).Rarity), std::size(CardPrices) - 1)]) * SigilSellShare);
	}

	inline uint16_t ItemPrice(const RunCard& card, uint8_t draughts = 0)
	{
		switch (card.Kind)
		{
		case RunCardKind::Card:
			return CardPrices[std::min<size_t>(size_t(GetCard(card.Upgrade()).Rarity), std::size(CardPrices) - 1)];
		case RunCardKind::Sigil:
			return GetSigil(SigilId(card.Id)).Price;
		case RunCardKind::Service:
			return uint16_t(GetService(ServiceId(card.Id)).Price + (ServiceId(card.Id) == ServiceId::Heal ? DraughtPriceStep * draughts : 0));
		case RunCardKind::Rune:
			return RunePrices[std::min<size_t>(size_t(GetRune(card.Rune()).Rarity), std::size(RunePrices) - 1)];
		default:
			return 0;
		}
	}

	inline bool RunCardValid(const RunCard& card)
	{
		switch (card.Kind)
		{
		case RunCardKind::None:
			return true;
		case RunCardKind::Spell:
		case RunCardKind::Rank:
			return card.Id < uint8_t(AbilityId::Count);
		case RunCardKind::Card:
			return card.Id < uint8_t(CardId::Count);
		case RunCardKind::Sigil:
			return card.Id < uint8_t(SigilId::Count);
		case RunCardKind::Duo:
			return card.Id < uint8_t(DuoId::Count);
		case RunCardKind::Curse:
			return card.Id < uint8_t(CurseId::Count);
		case RunCardKind::Service:
			return card.Id < uint8_t(ServiceId::Count);
		case RunCardKind::Rune:
			return card.Id < uint8_t(RuneId::Count);
		default:
			return false;
		}
	}

	inline constexpr CardId RunCardPool[] = {
		CardId::SplittingEmber,
		CardId::Aftershock,
		CardId::TwinComets,
		CardId::Afterimage,
		CardId::IceShrapnel,
		CardId::Implosion,
		CardId::Pinball,
		CardId::Overcharge,
		CardId::ThornedMirror,
		CardId::BlackIce,
		CardId::ColossusBlade,
		CardId::ShatteringFinale,
		CardId::Riposte,
		CardId::HurledEdge,
		CardId::ClusterBomb,
		CardId::StickyFire,
		CardId::RazorGlass,
		CardId::SeekerBlades,
		CardId::ForkedLightning,
		CardId::DeepWell,
		CardId::Quickening,
		CardId::Brimming,
		CardId::LastStand,
		CardId::Epicenter,
		CardId::ResonantGround,
		CardId::Rewind,
		CardId::Tempo,
		CardId::SnapCast,
		CardId::Kindling,
		CardId::Brittle,
		CardId::Conductive,
		CardId::Vampirism,
		CardId::Siphon,
		CardId::BloodPact,
		CardId::GlassCannon,
		CardId::HeavyRobes,
		CardId::CrystalWard,
		CardId::Flow,
		CardId::Shatter,
		CardId::Shock,
	};

	inline bool InRunPool(CardId id)
	{
		return std::find(std::begin(RunCardPool), std::end(RunCardPool), id) != std::end(RunCardPool);
	}

	inline constexpr uint8_t MaxCardStack = 3;
	inline constexpr float MultishotCost = 0.1f;
	inline constexpr float ChainReactionCost = 0.1f;
	inline constexpr float KeenEdgeDamage = 0.5f;
	inline constexpr float KeenEdgeChance = 0.05f;
	inline constexpr float StackSwiftRunes = 0.8f;
	inline constexpr float StackQuickTongue = 1.2f;
	inline constexpr float StackWideSigils = 0.2f;
	inline constexpr float RunKindlingShare = 0.25f;
	inline constexpr float RunKindlingSeconds = 2.0f;
	inline constexpr uint8_t RunVampirismKills = 8;
	inline constexpr float RunSiphonMana = 1.0f;
	inline constexpr float RunFlowRefund = 0.1f;
	inline constexpr float ConductiveMarkSeconds = 4.0f;
	inline constexpr float ShockSeconds = 3.0f;
	inline constexpr float ShockScale = 0.15f;
	inline constexpr float ShatterShare = 0.34f;
	inline constexpr float ShatterRadius = 2.5f;
	inline constexpr float BurnTickSeconds = 0.25f;

	inline constexpr CardId StackableCards[] = {
		CardId::SwiftRunes,
		CardId::WideSigils,
		CardId::QuickTongue,
		CardId::Precise,
		CardId::DeepWell,
		CardId::Quickening,
		CardId::Kindling,
		CardId::Multishot,
		CardId::Piercing,
		CardId::ChainReaction,
		CardId::KeenEdge,
		CardId::Shock,
	};

	inline bool IsStackable(CardId id)
	{
		return std::find(std::begin(StackableCards), std::end(StackableCards), id) != std::end(StackableCards);
	}

	inline uint8_t StackOf(CardMask cards, CardMask second, CardMask third, CardId id)
	{
		return HasCard(cards, id) ? uint8_t(1 + (HasCard(second, id) ? 1 : 0) + (HasCard(third, id) ? 1 : 0)) : uint8_t(0);
	}

	struct RunCardText
	{
		CardId Id;
		const char* Text;
	};

	inline constexpr RunCardText RunCardTexts[] = {
		{ CardId::Kindling, "Your spells, except Ember Bolt, set crawlers alight. They burn for a quarter of the hit over 2 seconds." },
		{ CardId::Brittle, "Your chills last twice as long, and chilled crawlers take 25% more damage from you." },
		{ CardId::Conductive, "Your lightning marks crawlers. Your next Ember Bolt on a marked crawler deals double." },
		{ CardId::Vampirism, "Every enemy you kill heals 1 health, up to 8 health in each room." },
		{ CardId::Siphon, "Each hit you land restores 1 mana." },
		{ CardId::Quickening, "You gain 40% more mana from every source." },
		{ CardId::Brimming, "Spells cast while you hold over 80% of your mana deal 30% more damage." },
		{ CardId::CrystalWard, "A crystal ward blocks the first hit you take in each room completely, then shatters. Burns get through." },
		{ CardId::Flow, "Every hit you land takes 0.1 s off your other spells' cooldowns." },
		{ CardId::Echo, "After a dodge, your next spell casts again 0.3 s later at half strength." },
		{ CardId::GlassCannon, "You deal 40% more damage, but take 10% more." },
		{ CardId::HeavyRobes, "You take 12% less damage, but move 15% slower." },
	};

	inline const char* RunCardDescription(CardId id)
	{
		for (const RunCardText& text : RunCardTexts)
			if (text.Id == id)
				return text.Text;
		const CardDef& card = GetCard(id);
		return card.RunDescription ? card.RunDescription : card.DamageDescription ? card.DamageDescription : card.Description;
	}

	inline RunLoadout MakeRunLoadout(const BrawlSpells& chosen)
	{
		RunLoadout loadout;
		for (uint32_t i = 0; i < RunStartSpells && i < chosen.size(); i++)
			if (IsBrawlSpell(chosen[i]))
				loadout.Spells[i] = chosen[i];
		return loadout;
	}

	inline int32_t FindRunSlot(const RunLoadout& loadout, AbilityId spell)
	{
		if (spell == BrawlBasic)
			return 0;
		for (uint32_t i = 0; i < SkillSlots; i++)
			if (loadout.Spells[i] == spell)
				return int32_t(i + 1);
		return -1;
	}

	inline uint32_t FreeRunSlots(const RunLoadout& loadout)
	{
		return uint32_t(std::count(loadout.Spells.begin(), loadout.Spells.end(), AbilityId::Count));
	}

	inline uint8_t RankOf(const RunLoadout& loadout, AbilityId spell)
	{
		const int32_t slot = FindRunSlot(loadout, spell);
		return slot >= 0 ? loadout.Ranks[size_t(slot)] : uint8_t(0);
	}

	inline bool IsGuardSpell(AbilityId id)
	{
		const AbilityKind kind = GetAbility(id).Kind;
		return kind == AbilityKind::Ward || kind == AbilityKind::Shield;
	}

	inline float RankCooldownScale(uint8_t rank)
	{
		return std::pow(RankCooldown, float(rank / 2));
	}

	inline float RankDamageScale(uint8_t rank)
	{
		return 1.0f + RankDamage * float(std::max<uint8_t>(rank, 1) - 1);
	}

	enum class MilestoneKind : uint8_t
	{
		None,
		Projectiles,
		Chain,
		Pierce,
		Area,
		Range,
		Durability,
		Cooldown,
		Card,
	};

	struct RankMilestone
	{
		MilestoneKind Kind = MilestoneKind::None;
		float Amount = 0.0f;
		CardId Card = CardId::Count;
		const char* Text = "";
	};

	struct SpellMilestones
	{
		AbilityId Spell = AbilityId::EmberBolt;
		RankMilestone Third;
		RankMilestone Fifth;
	};

	inline constexpr float MilestoneOwnedDamage = 1.25f;
	inline constexpr float MilestoneShotCost = 0.25f;

	inline constexpr SpellMilestones RunMilestones[] = {
		{ AbilityId::EmberBolt, { MilestoneKind::Projectiles, 1.0f, CardId::Count, "fires 2 bolts" }, { MilestoneKind::Projectiles, 1.0f, CardId::Count, "fires 3 bolts" } },
		{ AbilityId::ArcLightning, { MilestoneKind::Chain, 1.0f, CardId::Count, "jumps 1 more time" }, { MilestoneKind::Chain, 2.0f, CardId::Count, "jumps 2 more times" } },
		{ AbilityId::Comet, { MilestoneKind::Area, 0.3f, CardId::Count, "30% bigger crater" }, { MilestoneKind::Card, 0.0f, CardId::TwinComets, "a second comet lands on the spot" } },
		{ AbilityId::Firebomb, { MilestoneKind::Area, 0.3f, CardId::Count, "30% bigger blasts" }, { MilestoneKind::Card, 0.0f, CardId::ClusterBomb, "skips twice and bursts three times" } },
		{ AbilityId::PurpleFireball, { MilestoneKind::Area, 0.3f, CardId::Count, "30% bigger blast" }, { MilestoneKind::Projectiles, 1.0f, CardId::Count, "hurls 2 fireballs" } },
		{ AbilityId::FrostSpear, { MilestoneKind::Pierce, 2.0f, CardId::Count, "pierces 2 more crawlers" }, { MilestoneKind::Projectiles, 1.0f, CardId::Count, "throws 2 spears" } },
		{ AbilityId::GlacialNova, { MilestoneKind::Area, 0.3f, CardId::Count, "30% wider eruption" }, { MilestoneKind::Card, 0.0f, CardId::IceShrapnel, "the spikes burst into shrapnel" } },
		{ AbilityId::ObsidianBlades, { MilestoneKind::Projectiles, 2.0f, CardId::Count, "throws 9 blades" }, { MilestoneKind::Card, 0.0f, CardId::RazorGlass, "blades cut through the first crawler" } },
		{ AbilityId::GravityWell, { MilestoneKind::Area, 0.3f, CardId::Count, "30% wider pull" }, { MilestoneKind::Card, 0.0f, CardId::Implosion, "the eye bursts when it closes" } },
		{ AbilityId::EarthSpike, { MilestoneKind::Area, 0.3f, CardId::Count, "30% bigger spike" }, { MilestoneKind::Area, 0.3f, CardId::Count, "30% bigger again" } },
		{ AbilityId::CrystalSword, { MilestoneKind::Area, 0.3f, CardId::Count, "30% longer strikes" }, { MilestoneKind::Card, 0.0f, CardId::ShatteringFinale, "the cleave bursts into crystal shards" } },
		{ AbilityId::StormStrike, { MilestoneKind::Area, 0.3f, CardId::Count, "30% wider strike" }, { MilestoneKind::Card, 0.0f, CardId::ForkedLightning, "a second bolt follows" } },
		{ AbilityId::Warp, { MilestoneKind::Cooldown, 0.7f, CardId::Count, "recharges 30% faster" }, { MilestoneKind::Card, 0.0f, CardId::Afterimage, "leaves a shockwave where you left" } },
		{ AbilityId::DiamondWard, { MilestoneKind::Durability, 0.3f, CardId::Count, "holds 30% more" }, { MilestoneKind::Card, 0.0f, CardId::ThornedMirror, "reflections hit 50% harder" } },
		{ AbilityId::HexShield, { MilestoneKind::Durability, 0.3f, CardId::Count, "holds 30% more" }, { MilestoneKind::Durability, 0.3f, CardId::Count, "holds 30% more again" } },
	};

	inline const RankMilestone* MilestoneAt(AbilityId spell, uint8_t rank)
	{
		if (rank != 3 && rank != MaxSpellRank)
			return nullptr;
		for (const SpellMilestones& milestones : RunMilestones)
			if (milestones.Spell == spell)
				return rank == 3 ? &milestones.Third : &milestones.Fifth;
		return nullptr;
	}

	inline CardMask MilestoneCards(const RunLoadout& loadout)
	{
		CardMask cards = 0;
		for (uint32_t slot = 0; slot < AbilitySlots; slot++)
		{
			const AbilityId spell = slot == 0 ? BrawlBasic : loadout.Spells[slot - 1];
			if (spell == AbilityId::Count)
				continue;
			for (uint8_t rank = 3; rank <= loadout.Ranks[slot]; rank += 2)
				if (const RankMilestone* milestone = MilestoneAt(spell, rank); milestone && milestone->Kind == MilestoneKind::Card)
					cards |= CardBit(milestone->Card);
		}
		return cards;
	}

	inline CardMask RunCardsOf(const RunLoadout& loadout)
	{
		return loadout.Cards | MilestoneCards(loadout);
	}

	inline constexpr float RunTagWeight = 2.0f;

	inline TagMask SpellTags(AbilityId spell)
	{
		switch (spell)
		{
		case AbilityId::EmberBolt:
		case AbilityId::Comet:
		case AbilityId::Firebomb:
		case AbilityId::PurpleFireball:
			return TagFire;
		case AbilityId::FrostSpear:
		case AbilityId::GlacialNova:
			return TagFrost;
		case AbilityId::ArcLightning:
		case AbilityId::StormStrike:
			return TagStorm;
		case AbilityId::ObsidianBlades:
		case AbilityId::GravityWell:
		case AbilityId::Warp:
			return TagMana;
		case AbilityId::CrystalSword:
			return TagBlood;
		case AbilityId::EarthSpike:
		case AbilityId::DiamondWard:
		case AbilityId::HexShield:
			return TagGuard;
		default:
			return 0;
		}
	}

	inline TagMask RunTagsOf(const RunLoadout& loadout)
	{
		TagMask tags = CardTags(RunCardsOf(loadout));
		for (const AbilityId spell : loadout.Spells)
			if (spell != AbilityId::Count)
				tags |= SpellTags(spell);
		return tags;
	}

	inline bool IsOffTag(const RunCard& card, TagMask owned)
	{
		if (card.Kind == RunCardKind::Spell)
			return (SpellTags(card.Spell()) & owned) == 0;
		if (card.Kind == RunCardKind::Card)
		{
			const TagMask tags = GetCard(card.Upgrade()).Tags;
			return tags != 0 && (tags & owned) == 0;
		}
		return false;
	}

	inline void ApplyMilestone(SkillTraits& skill, const RankMilestone& milestone, CardMask picked)
	{
		switch (milestone.Kind)
		{
		case MilestoneKind::Projectiles:
			skill.ExtraProjectiles = uint8_t(skill.ExtraProjectiles + uint8_t(milestone.Amount));
			if (GetAbility(skill.Ability).ProjectileCount == 1)
				skill.DamageScale *= std::max(1.0f - MilestoneShotCost * milestone.Amount, 0.5f);
			break;
		case MilestoneKind::Chain:
			skill.ExtraChain = uint8_t(skill.ExtraChain + uint8_t(milestone.Amount));
			break;
		case MilestoneKind::Pierce:
			skill.ExtraPierce = uint8_t(skill.ExtraPierce + uint8_t(milestone.Amount));
			break;
		case MilestoneKind::Area:
			skill.AreaScale *= 1.0f + milestone.Amount;
			break;
		case MilestoneKind::Range:
			skill.RangeScale *= 1.0f + milestone.Amount;
			break;
		case MilestoneKind::Durability:
			skill.Durability *= 1.0f + milestone.Amount;
			break;
		case MilestoneKind::Cooldown:
			skill.Cooldown *= milestone.Amount;
			break;
		case MilestoneKind::Card:
			if (HasCard(picked, milestone.Card))
				skill.DamageScale *= MilestoneOwnedDamage;
			break;
		default:
			break;
		}
	}

	inline SkillTraits MakeRunSkill(AbilityId id, uint8_t rank = 1, CardMask picked = 0)
	{
		SkillTraits skill = MakeBrawlSkill(id, 1.0f, BrawlMode::Damage);
		if (const RunSpellDef* def = FindRunSpell(id))
		{
			const int32_t base = GetAbility(id).BrawlDamage;
			skill.DamageScale = base > 0 ? float(def->Damage) / float(base) : 1.0f;
			skill.Cooldown = def->Cooldown;
			skill.CastTime = def->Cast;
		}
		skill.ManaCost = 0.0f;
		if (id != BrawlBasic && GetAbility(id).Kind != AbilityKind::Shield)
			skill.MoveScale = std::min(skill.MoveScale, RunCastMove);
		rank = std::clamp<uint8_t>(rank, 1, MaxSpellRank);
		skill.Level = rank;
		skill.DamageScale *= RankDamageScale(rank);
		skill.Cooldown *= RankCooldownScale(rank);
		for (uint8_t step = 3; step <= rank; step += 2)
			if (const RankMilestone* milestone = MilestoneAt(id, step))
				ApplyMilestone(skill, *milestone, picked);
		return skill;
	}

	inline PlayerTraits MakeRunTraits(const RunLoadout& loadout)
	{
		const CardMask cards = RunCardsOf(loadout);
		PlayerTraits traits = MakeBrawlTraits(BrawlLoadout{}, BrawlMode::Damage);
		traits.MoveSpeed = RunMoveSpeed;
		traits.MaxMana = RunMana + DeepWellMana * float(StackOf(loadout.Cards, loadout.Second, loadout.Third, CardId::DeepWell));
		traits.ManaRegen = RunManaRegen * std::pow(QuickeningRegen, float(StackOf(loadout.Cards, loadout.Second, loadout.Third, CardId::Quickening)));
		traits.OvercastCost = RunOvercastCost;
		traits.Dodge.Cooldown = RunDodgeCooldown;
		traits.DodgeCharges = RunDodgeCharges;
		traits.CritChance = HasCard(cards, CardId::Precise) ? PreciseChance : 0.0f;
		traits.CritMultiplier = PreciseMultiplier;
		traits.Skills[0] = MakeRunSkill(BrawlBasic, loadout.Ranks[0], loadout.Cards);
		for (uint32_t i = 0; i < SkillSlots; i++)
		{
			const AbilityId spell = loadout.Spells[i];
			traits.Skills[i + 1] = IsBrawlSpell(spell) ? MakeRunSkill(spell, loadout.Ranks[i + 1], loadout.Cards) : SkillTraits{};
			if (spell == AbilityId::CrystalSword && HasCard(cards, CardId::ColossusBlade))
			{
				traits.Skills[i + 1].AreaScale *= ColossusArea;
				traits.Skills[i + 1].CastTime *= ColossusPace;
			}
		}
		ApplyCasting(traits, cards, BrawlMode::Damage);
		ApplyApex(traits, cards);
		for (uint32_t slot = 0; slot < AbilitySlots; slot++)
			ApplyRunes(traits.Skills[slot], SlotRunes(loadout, slot));
		const auto stack = [&](CardId id) { return StackOf(loadout.Cards, loadout.Second, loadout.Third, id); };
		const uint8_t swift = stack(CardId::SwiftRunes);
		const uint8_t tongue = stack(CardId::QuickTongue);
		const uint8_t sigils = stack(CardId::WideSigils);
		const uint8_t multishot = stack(CardId::Multishot);
		const uint8_t piercing = stack(CardId::Piercing);
		const uint8_t chains = stack(CardId::ChainReaction);
		const uint8_t keen = stack(CardId::KeenEdge);
		for (SkillTraits& skill : traits.Skills)
		{
			if (skill.IsEmpty() || GetAbility(skill.Ability).Kind == AbilityKind::Shield)
				continue;
			const AbilityDef& def = GetAbility(skill.Ability);
			if (swift > 1)
				skill.Cooldown *= std::pow(StackSwiftRunes, float(swift - 1));
			if (tongue > 1)
			{
				skill.CastTime /= std::pow(StackQuickTongue, float(tongue - 1));
				skill.ChargeTime /= std::pow(StackQuickTongue, float(tongue - 1));
			}
			if (sigils > 1 && IsAreaSpell(skill.Ability))
				skill.AreaScale *= 1.0f + StackWideSigils * float(sigils - 1);
			const bool projectile = def.Kind == AbilityKind::Projectile;
			if (projectile && multishot > 0)
			{
				skill.ExtraProjectiles = uint8_t(skill.ExtraProjectiles + multishot * (skill.Ability == AbilityId::ObsidianBlades ? 2u : 1u));
				skill.DamageScale *= std::max(1.0f - MultishotCost * float(multishot), 0.5f);
			}
			if (projectile && piercing > 0)
				skill.ExtraPierce = uint8_t(skill.ExtraPierce + piercing);
			if (chains > 0 && (def.Chain > 0 || (projectile && chains >= 2)))
			{
				skill.ExtraChain = uint8_t(skill.ExtraChain + (def.Chain > 0 ? chains : 1u));
				skill.DamageScale *= 1.0f - ChainReactionCost * float(chains);
			}
		}
		const uint8_t precise = stack(CardId::Precise);
		traits.CritChance = PreciseChance * float(precise) + KeenEdgeChance * float(keen);
		traits.CritMultiplier = PreciseMultiplier + KeenEdgeDamage * float(keen);
		if (HasCard(cards, CardId::HeavyRobes))
			traits.MoveSpeed *= HeavyRobesSpeed;
		if (HasCurse(loadout, CurseId::BrittleBones))
		{
			traits.DodgeCharges = uint8_t(traits.DodgeCharges + 1);
			traits.Dodge.Cooldown *= BrittleDodge;
		}
		for (SkillTraits& skill : traits.Skills)
		{
			if (skill.IsEmpty())
				continue;
			if (HasCurse(loadout, CurseId::Fever))
			{
				skill.Cooldown *= FeverSpeed;
				skill.CastTime *= FeverSpeed;
				skill.ChargeTime *= FeverSpeed;
			}
		}
		traits.MaxHealth = std::max(traits.MaxHealth - int32_t(loadout.Wounds), 20);
		return traits;
	}

	inline bool CardFitsRun(const RunLoadout& loadout, CardId id)
	{
		const CardDef& card = GetCard(id);
		const CardMask owned = RunCardsOf(loadout);
		if (!InRunPool(id) || card.Rarity == CardRarity::Apex)
			return false;
		if (HasCard(owned, id))
			return IsStackable(id) && HasCard(loadout.Cards, id) && StackOf(loadout.Cards, loadout.Second, loadout.Third, id) < MaxCardStack;
		if (card.NeedsCard != CardId::Count && !HasCard(owned, card.NeedsCard))
			return false;
		if (card.Needs != AbilityId::Count && FindRunSlot(loadout, card.Needs) < 0 && (card.NeedsAlt == AbilityId::Count || FindRunSlot(loadout, card.NeedsAlt) < 0))
			return false;
		if (card.Kind != CardKind::Upgrade)
			return true;
		if (FindRunSlot(loadout, card.Spell) < 0)
			return false;
		uint32_t upgrades = 0;
		for (const CardDef& other : Cards)
			upgrades += other.Kind == CardKind::Upgrade && other.Spell == card.Spell && HasCard(loadout.Cards, other.Id) ? 1u : 0u;
		return upgrades < MaxUpgradesPerSpell;
	}

	inline float CardCastScale(CardMask cards, float healthFraction, float manaBefore, float maxMana, bool overloaded, float weave)
	{
		float scale = overloaded ? OverloadScale : 1.0f;
		if (HasCard(cards, CardId::Brimming) && maxMana > 0.0f && manaBefore >= BrimmingShare * maxMana)
			scale *= BrimmingScale;
		if (HasCard(cards, CardId::LastStand) && healthFraction < LastStandHealth)
			scale *= LastStandScale;
		if (HasCard(cards, CardId::ManaSingularity))
			scale *= 1.0f + ManaSingularityDamage * manaBefore;
		if (HasCard(cards, CardId::BloodMagic))
			scale *= BloodMagicDamage;
		if (HasCard(cards, CardId::Archmage))
			scale *= 1.0f + ArchmageDamage * weave;
		return scale;
	}
}
