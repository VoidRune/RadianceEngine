#pragma once
#include "GameShared/Content/Definitions.h"
#include <array>
#include <cstdint>
#include <iterator>

namespace Game
{
	inline constexpr uint8_t SigilSlots = 5;
	inline constexpr uint8_t ShopSlots = 6;

	enum class SigilId : uint8_t
	{
		WallSlam,
		Domino,
		Afterimage,
		HungryComet,
		MirrorDebt,
		OddHours,
		ShardHeart,
		GlassEcho,
		Grudge,
		Leftmost,
		RiftStep,
		SecondWind,
		MomentumEngine,
		EmberPyre,
		Count,
		None = 0xFF,
	};

	struct SigilDef
	{
		SigilId Id;
		const char* Name;
		const char* Text;
		const char* Twice;
		uint16_t Price;
	};

	inline constexpr SigilDef Sigils[] = {
		{ SigilId::WallSlam, "Wall Slam", "Enemies you knock into a wall or a rock take damage based on how fast they hit it.", "Slams deal double damage.", 90 },
		{ SigilId::Domino, "Domino", "An enemy you send flying knocks over the enemies it crashes into, and both take damage.", "Crashes hit twice as hard.", 90 },
		{ SigilId::Afterimage, "Afterimage Arsenal", "Dodging leaves a copy of your last spell where you stood, at 60% strength.", "Two copies.", 100 },
		{ SigilId::HungryComet, "Hungry Comet", "Every kill takes 0.5 s off Comet's cooldown, but each Comet you cast costs 2 health.", "Kills take 1 s off.", 80 },
		{ SigilId::MirrorDebt, "Mirror Debt", "Your bolts and shards that miss turn around once and fly back.", "They return at double damage.", 90 },
		{ SigilId::OddHours, "Odd Hours", "Your odd-numbered spell casts deal 30% less damage. Your even-numbered casts deal double.", "Even casts deal triple.", 100 },
		{ SigilId::ShardHeart, "Shard Heart", "Every 2 shards your kills spill fire a crystal bolt at the nearest enemy.", "Two bolts.", 80 },
		{ SigilId::GlassEcho, "Glass Echo", "Your basic attack recasts your last spell at 25% strength, at most once a second.", "Recasts at 50%.", 110 },
		{ SigilId::Grudge, "Grudge", "The last enemy that hit you takes double damage from you until it dies.", "Triple damage.", 70 },
		{ SigilId::Leftmost, "Leftmost Rule", "The sigil in your first slot works twice as hard. Reorder sigils at a shop.", "", 80 },
		{ SigilId::RiftStep, "Rift Step", "Dodging through enemies cuts them and stuns them briefly.", "Double damage and stun.", 80 },
		{ SigilId::SecondWind, "Second Wind", "Once per room, a hit that would down you leaves you at 1 health and untouchable for 1.5 s.", "Twice per room.", 100 },
		{ SigilId::MomentumEngine, "Momentum Engine", "Momentum also raises your damage, up to +40% at full Momentum.", "Up to +80%.", 90 },
		{ SigilId::EmberPyre, "Ember Pyre", "Enemies that die while burning explode for a third of their health.", "They explode for two thirds.", 90 },
	};
	static_assert(std::size(Sigils) == size_t(SigilId::Count));

	inline const SigilDef& GetSigil(SigilId id)
	{
		return Sigils[size_t(id) < std::size(Sigils) ? size_t(id) : 0];
	}

	enum class DuoId : uint8_t
	{
		FrozenComet,
		AbyssalEdge,
		StormSkip,
		ThunderSpear,
		ObsidianQuake,
		VioletSingularity,
		Count,
	};

	struct DuoDef
	{
		DuoId Id;
		const char* Name;
		const char* Text;
		AbilityId First;
		AbilityId Second;
	};

	inline constexpr DuoDef Duos[] = {
		{ DuoId::FrozenComet, "Frozen Comet", "Comet chills what it hits, and freezes enemies that were already chilled for 1 s.", AbilityId::Comet, AbilityId::GlacialNova },
		{ DuoId::AbyssalEdge, "Abyssal Edge", "Crystal Sword's overhead cleave opens a small void eye on the enemy it hits that drags its pack together.", AbilityId::CrystalSword, AbilityId::GravityWell },
		{ DuoId::StormSkip, "Storm Skip", "Every Firebomb burst arcs lightning to the nearest enemy.", AbilityId::Firebomb, AbilityId::ArcLightning },
		{ DuoId::ThunderSpear, "Thunder Spear", "Frost Spear hits on chilled enemies call a storm strike down on them.", AbilityId::FrostSpear, AbilityId::StormStrike },
		{ DuoId::ObsidianQuake, "Obsidian Quake", "Earth Spike bursts into a ring of obsidian blades.", AbilityId::EarthSpike, AbilityId::ObsidianBlades },
		{ DuoId::VioletSingularity, "Violet Singularity", "Purple Fireball blasts collapse into a void eye that pulls survivors in.", AbilityId::PurpleFireball, AbilityId::GravityWell },
	};
	static_assert(std::size(Duos) == size_t(DuoId::Count));

	inline const DuoDef& GetDuo(DuoId id)
	{
		return Duos[size_t(id) < std::size(Duos) ? size_t(id) : 0];
	}

	enum class CurseId : uint8_t
	{
		GlassPact,
		Greed,
		BrittleBones,
		Fever,
		Count,
	};

	struct CurseDef
	{
		CurseId Id;
		const char* Name;
		const char* Text;
	};

	inline constexpr CurseDef Curses[] = {
		{ CurseId::GlassPact, "Glass Pact", "You deal 35% more damage. Enemies deal 30% more damage to you." },
		{ CurseId::Greed, "Hoarder's Greed", "You earn double shards. All your healing is halved." },
		{ CurseId::BrittleBones, "Brittle Bones", "One more dodge charge and dodges recharge 30% faster. You take 25% more damage." },
		{ CurseId::Fever, "Fever", "Your spells cast and recharge 20% faster. You lose 1 health every 4 seconds while enemies are awake." },
	};
	static_assert(std::size(Curses) == size_t(CurseId::Count));

	inline const CurseDef& GetCurse(CurseId id)
	{
		return Curses[size_t(id) < std::size(Curses) ? size_t(id) : 0];
	}

	enum class ServiceId : uint8_t
	{
		Heal,
		Reroll,
		Banish,
		Count,
	};

	struct ServiceDef
	{
		ServiceId Id;
		const char* Name;
		const char* Text;
		uint16_t Price;
	};

	inline constexpr ServiceDef Services[] = {
		{ ServiceId::Heal, "Crystal Draught", "Heal 25% of your health. The price rises each time you buy one.", 35 },
		{ ServiceId::Reroll, "Reroll Token", "One more reroll for card offers.", 25 },
		{ ServiceId::Banish, "Banish Token", "One more banish for card offers.", 25 },
	};
	static_assert(std::size(Services) == size_t(ServiceId::Count));

	inline const ServiceDef& GetService(ServiceId id)
	{
		return Services[size_t(id) < std::size(Services) ? size_t(id) : 0];
	}

	enum class ShrineChoice : uint8_t
	{
		None,
		Cleanse,
		Transform,
		BloodRank,
		Count,
	};

	inline constexpr const char* ShrineNames[] = { "", "Cleanse", "Transform", "Blood Rank" };
	inline constexpr const char* ShrineTexts[] = { "", "Lift every curse you carry. Without curses, gain 40 shards instead.", "Turn one of your cards, at random, into a different card.", "Give up 15% of your max health for the rest of the run to rank up a spell." };
	static_assert(std::size(ShrineNames) == size_t(ShrineChoice::Count));

	inline constexpr uint16_t CardPrices[] = { 40, 60, 85 };
	inline constexpr uint16_t ShardsPerRoom = 5;
	inline constexpr uint16_t ShardsPerTier = 2;
	inline constexpr uint16_t MedalShards[] = { 0, 2, 4, 6 };
	inline constexpr uint16_t EliteShards = 15;
	inline constexpr uint16_t CacheShards = 40;
	inline constexpr uint16_t CleanseShards = 40;
	inline constexpr uint16_t MaxShards = 9999;
	inline constexpr float SigilSellShare = 0.5f;
	inline constexpr float BloodRankShare = 0.15f;
	inline constexpr float ShopHeal = 0.25f;
	inline constexpr uint16_t DraughtPriceStep = 20;
	inline constexpr uint8_t MaxCurses = 2;
	inline constexpr float CurseOfferChance = 0.08f;
	inline constexpr float DuoOfferShare = 0.15f;
	inline constexpr uint8_t DuoRank = 3;

	inline constexpr float WallSlamSpeed = 5.0f;
	inline constexpr float WallSlamBase = 18.0f;
	inline constexpr float WallSlamPerSpeed = 5.0f;
	inline constexpr float DominoSpeed = 6.0f;
	inline constexpr float DominoShare = 0.7f;
	inline constexpr float AfterimageShare = 0.6f;
	inline constexpr float HungryCometRefund = 0.5f;
	inline constexpr float HungryCometCost = 0.02f;
	inline constexpr float GlassEchoShare = 0.25f;
	inline constexpr float GlassEchoSpacing = 1.0f;
	inline constexpr float OddHoursWeak = 0.7f;
	inline constexpr uint32_t ShardHeartWorth = 20;
	inline constexpr int32_t ShardHeartDamage = 14;
	inline constexpr float RiftStepSeconds = 0.3f;
	inline constexpr float RiftStepRadius = 1.0f;
	inline constexpr int32_t RiftStepDamage = 35;
	inline constexpr float RiftStepStun = 0.6f;
	inline constexpr float SecondWindSeconds = 1.5f;
	inline constexpr float MomentumEngineDamage = 0.4f;
	inline constexpr float EmberPyreShare = 0.34f;
	inline constexpr float EmberPyreRadius = 2.8f;
	inline constexpr float GlassPactDealt = 1.35f;
	inline constexpr float GlassPactTaken = 1.3f;
	inline constexpr float BrittleTaken = 1.25f;
	inline constexpr float BrittleDodge = 0.7f;
	inline constexpr float FeverSpeed = 0.8f;
	inline constexpr float FeverDrain = 0.25f;
	inline constexpr float DuoSpacing = 0.6f;
	inline constexpr float DuoShare = 0.5f;
}
