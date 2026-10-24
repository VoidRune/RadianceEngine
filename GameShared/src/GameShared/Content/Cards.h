#pragma once
#include "GameShared/Content/Definitions.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iterator>

namespace Game
{
	enum class CardId : uint8_t
	{
		SplittingEmber,
		LingeringGale,
		DeflectingGale,
		ShatteringImpact,
		TwinComets,
		Afterimage,
		Aftershock,
		Implosion,
		Pinball,
		Overcharge,
		HeartSwap,
		ThornedMirror,
		BlackIce,
		GlassCannon,
		Featherfall,
		Echo,
		HeavyRobes,
		LongChain,
		Whiplash,
		ClusterBomb,
		StickyFire,
		Fivefold,
		SeekerShards,
		ForkedLightning,
		StaticCharge,
		DeepWell,
		Quickening,
		Siphon,
		Brimming,
		BloodPact,
		LastStand,
		VampiricEmbers,
		Kindling,
		Wildfire,
		Brittle,
		Shatter,
		Conductive,
		Overload,
		Barrier,
		SecondWind,
		Precise,
		ApexOfMagic,
		ManaSingularity,
		MindOverMatter,
		BloodMagic,
		EchoChamber,
		UnstableArcana,
		Leyline,
		Count,
	};

	enum class CardKind : uint8_t
	{
		Upgrade,
		Wild,
	};

	enum class CardRarity : uint8_t
	{
		Common,
		Rare,
		Legendary,
		Apex,
	};

	enum class CardTag : uint8_t
	{
		Fire,
		Frost,
		Storm,
		Mana,
		Blood,
		Guard,
		Count,
	};

	inline constexpr const char* CardTagNames[] = { "Fire", "Frost", "Storm", "Mana", "Blood", "Guard" };
	static_assert(std::size(CardTagNames) == size_t(CardTag::Count));

	using TagMask = uint8_t;

	inline constexpr TagMask TagBit(CardTag tag)
	{
		return TagMask(1u << uint32_t(tag));
	}

	inline constexpr TagMask TagFire = TagBit(CardTag::Fire);
	inline constexpr TagMask TagFrost = TagBit(CardTag::Frost);
	inline constexpr TagMask TagStorm = TagBit(CardTag::Storm);
	inline constexpr TagMask TagMana = TagBit(CardTag::Mana);
	inline constexpr TagMask TagBlood = TagBit(CardTag::Blood);
	inline constexpr TagMask TagGuard = TagBit(CardTag::Guard);

	enum CardModes : uint8_t
	{
		InDamage = 1 << 0,
		InKnockback = 1 << 1,
		InBoth = InDamage | InKnockback,
	};

	struct CardDef
	{
		CardId Id = CardId::SplittingEmber;
		CardKind Kind = CardKind::Upgrade;
		CardRarity Rarity = CardRarity::Common;
		AbilityId Spell = AbilityId::Count;
		const char* Name = "";
		const char* Description = "";
		const char* DamageDescription = nullptr;
		TagMask Tags = 0;
		uint8_t Modes = InBoth;
		AbilityId Needs = AbilityId::Count;
		AbilityId NeedsAlt = AbilityId::Count;
		CardId NeedsCard = CardId::Count;
	};

	inline constexpr CardDef Cards[] = {
		{ CardId::SplittingEmber, CardKind::Upgrade, CardRarity::Common, AbilityId::EmberBolt, "Splitting Ember", "At the end of its range, Ember Bolt splits into three smaller bolts.", "At the end of its range, Ember Bolt splits into three bolts that deal 60% damage.", TagFire },
		{ CardId::LingeringGale, CardKind::Upgrade, CardRarity::Common, AbilityId::Gust, "Lingering Gale", "Gust leaves a whirlwind that keeps blasting rivals away for a second and a half.", "Gust leaves a whirlwind that hits rivals three more times over a second and a half.", TagStorm },
		{ CardId::DeflectingGale, CardKind::Upgrade, CardRarity::Rare, AbilityId::Gust, "Deflecting Gale", "Gust bats enemy spells back at whoever cast them.", nullptr, TagGuard },
		{ CardId::ShatteringImpact, CardKind::Upgrade, CardRarity::Rare, AbilityId::Comet, "Shattering Impact", "The crater cracks and sinks into the lava 1.5 s after the comet lands.", "A smaller crater cracks and sinks into the lava 3 s after the comet lands.", TagFire },
		{ CardId::TwinComets, CardKind::Upgrade, CardRarity::Common, AbilityId::Comet, "Twin Comets", "A smaller second comet lands on the same spot half a second later.", "A second comet lands on the same spot half a second later.", TagFire },
		{ CardId::Afterimage, CardKind::Upgrade, CardRarity::Rare, AbilityId::Warp, "Afterimage", "Warp leaves a shockwave where you left.", "Warp leaves a shockwave where you left that hits for 16.", TagMana },
		{ CardId::Aftershock, CardKind::Upgrade, CardRarity::Common, AbilityId::Shockwave, "Aftershock", "A second, weaker pulse follows 0.6 s later.", "A second pulse follows 0.6 s later for 60% damage.", TagStorm },
		{ CardId::Implosion, CardKind::Upgrade, CardRarity::Rare, AbilityId::GravityWell, "Implosion", "When the well closes it bursts, flinging everyone outward.", "When the well closes it bursts, hitting everyone inside for 5.", TagMana },
		{ CardId::Pinball, CardKind::Upgrade, CardRarity::Rare, AbilityId::ArcLightning, "Pinball", "Every jump shoves half again as hard as the one before.", "Every jump hits half again as hard as the one before.", TagStorm },
		{ CardId::Overcharge, CardKind::Upgrade, CardRarity::Common, AbilityId::ArcLightning, "Overcharge", "Arc Lightning jumps to one more rival.", nullptr, TagStorm },
		{ CardId::HeartSwap, CardKind::Upgrade, CardRarity::Rare, AbilityId::Swap, "Heart Swap", "Swap also trades health, and with it how far each of you flies.", "If the rival Swap hits has more health than you, you steal up to 15 of it.", TagBlood },
		{ CardId::ThornedMirror, CardKind::Upgrade, CardRarity::Common, AbilityId::MirrorWard, "Thorned Mirror", "Spells you reflect come back 50% stronger.", nullptr, TagGuard },
		{ CardId::BlackIce, CardKind::Upgrade, CardRarity::Common, AbilityId::Frostbite, "Black Ice", "A rival you chill slides 40% further for a second and a half.", "Frostbite chills half a second longer, and the rival slides 40% further.", TagFrost },
		{ CardId::GlassCannon, CardKind::Wild, CardRarity::Legendary, AbilityId::Count, "Glass Cannon", "Your spells shove 50% harder, but you fly 15% further.", "You deal 40% more damage, but take 10% more.", TagBlood },
		{ CardId::Featherfall, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Featherfall", "Once per round, your first step into lava bounces you back onto solid stone.", nullptr, TagGuard },
		{ CardId::Echo, CardKind::Wild, CardRarity::Legendary, AbilityId::Count, "Echo", "After a dodge, your next spell casts again 0.3 s later at half strength.", nullptr, TagMana },
		{ CardId::HeavyRobes, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Heavy Robes", "You get shoved 12% less, but move 15% slower.", "You take 12% less damage, but move 15% slower.", TagGuard },
		{ CardId::LongChain, CardKind::Upgrade, CardRarity::Common, AbilityId::ChainHook, "Long Chain", "Chain Hook flies half again as far and recharges 20% faster.", "Chain Hook flies half again as far, recharges 20% faster and hits 25% harder.", TagBlood },
		{ CardId::Whiplash, CardKind::Upgrade, CardRarity::Rare, AbilityId::ChainHook, "Whiplash", "A rival you hook flies much further past you.", "A rival you hook flies further past you and takes 80% more damage.", TagBlood },
		{ CardId::ClusterBomb, CardKind::Upgrade, CardRarity::Common, AbilityId::Firebomb, "Cluster Bomb", "Firebomb skips twice and bursts three times, the last time biggest.", nullptr, TagFire },
		{ CardId::StickyFire, CardKind::Upgrade, CardRarity::Rare, AbilityId::Firebomb, "Sticky Fire", "Every Firebomb landing keeps burning for a second, shoving anyone who walks in.", "Every Firebomb landing keeps burning for a second, scorching anyone who walks in.", TagFire },
		{ CardId::Fivefold, CardKind::Upgrade, CardRarity::Common, AbilityId::SplittingBolt, "Fivefold", "Splitting Bolt splits into five shards.", "Splitting Bolt splits into five shards that deal 30% less.", TagMana },
		{ CardId::SeekerShards, CardKind::Upgrade, CardRarity::Rare, AbilityId::SplittingBolt, "Seeker Shards", "The shards of Splitting Bolt curve toward the nearest rival.", "The shards of Splitting Bolt curve toward the nearest rival and deal 25% less.", TagMana },
		{ CardId::ForkedLightning, CardKind::Upgrade, CardRarity::Common, AbilityId::StormStrike, "Forked Lightning", "Storm Strike jumps to one more rival.", nullptr, TagStorm },
		{ CardId::StaticCharge, CardKind::Upgrade, CardRarity::Rare, AbilityId::StormStrike, "Static Charge", "Rivals Storm Strike hits can't dodge for 1.5 seconds.", nullptr, TagStorm },
		{ CardId::DeepWell, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Deep Well", "50 more maximum mana.", nullptr, TagMana, InDamage },
		{ CardId::Quickening, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Quickening", "Your mana comes back 40% faster.", nullptr, TagMana, InDamage },
		{ CardId::Siphon, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Siphon", "Each hit you land restores 4 mana.", nullptr, TagMana, InDamage },
		{ CardId::Brimming, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Brimming", "Spells cast with over 80% mana deal 30% more damage.", nullptr, TagMana, InDamage },
		{ CardId::BloodPact, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Blood Pact", "Spells you can't afford take the rest from your health, 1 health per 4 mana.", nullptr, TagBlood, InDamage },
		{ CardId::LastStand, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Last Stand", "You deal 30% more damage while under 30% health.", nullptr, TagBlood, InDamage },
		{ CardId::VampiricEmbers, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Vampiric Embers", "Every Ember Bolt hit heals you for 1.", nullptr, TagBlood | TagFire, InDamage },
		{ CardId::Kindling, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Kindling", "Your spells, except Ember Bolt, set rivals alight for 4 damage over 2 seconds.", nullptr, TagFire, InDamage },
		{ CardId::Wildfire, CardKind::Wild, CardRarity::Legendary, AbilityId::Count, "Wildfire", "A burning rival you kill bursts into flames that set everyone nearby alight.", nullptr, TagFire, InDamage, AbilityId::Count, AbilityId::Count, CardId::Kindling },
		{ CardId::Brittle, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Brittle", "Chilled rivals take 15% more damage from you.", nullptr, TagFrost, InDamage, AbilityId::Frostbite },
		{ CardId::Shatter, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Shatter", "A chilled rival you kill bursts for 15 damage around them.", nullptr, TagFrost, InDamage, AbilityId::Frostbite },
		{ CardId::Conductive, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Conductive", "Your lightning marks rivals. Your next Ember Bolt on a marked rival deals double.", nullptr, TagStorm, InDamage, AbilityId::ArcLightning, AbilityId::StormStrike },
		{ CardId::Overload, CardKind::Wild, CardRarity::Legendary, AbilityId::Count, "Overload", "Every fifth spell you cast costs nothing and deals 50% more.", nullptr, TagStorm | TagMana, InDamage },
		{ CardId::Barrier, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Barrier", "Start each round with an 8-point shield.", nullptr, TagGuard, InDamage },
		{ CardId::SecondWind, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Second Wind", "Once per round, heal 6 when you drop under 25% health.", nullptr, TagGuard, InDamage },
		{ CardId::Precise, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Precise", "Your spells have a 15% chance to deal double damage.", nullptr, TagGuard, InDamage },
		{ CardId::ApexOfMagic, CardKind::Wild, CardRarity::Apex, AbilityId::Count, "Apex of Magic", "Casting and cooldowns are 300% faster, but every spell costs 50% more mana.", nullptr, TagStorm | TagMana, InDamage },
		{ CardId::ManaSingularity, CardKind::Wild, CardRarity::Apex, AbilityId::Count, "Mana Singularity", "Spells cost no mana and deal 1% more damage for every 2 mana you hold. Hits drain your mana as well as your health.", nullptr, TagMana, InDamage },
		{ CardId::MindOverMatter, CardKind::Wild, CardRarity::Apex, AbilityId::Count, "Mind over Matter", "Damage drains your mana before your health. After any hit, your mana stops coming back for 2 seconds.", nullptr, TagMana | TagGuard, InDamage },
		{ CardId::BloodMagic, CardKind::Wild, CardRarity::Apex, AbilityId::Count, "Blood Magic", "You have no mana. Spells cost 1 health per 5 mana instead, deal 30% more damage and heal you for all the damage they deal.", nullptr, TagBlood, InDamage },
		{ CardId::EchoChamber, CardKind::Wild, CardRarity::Apex, AbilityId::Count, "Echo Chamber", "Every spell casts again a moment later wherever you aim, but its cooldown is 75% longer. Warp and Mirror Ward don't echo.", nullptr, TagStorm, InDamage },
		{ CardId::UnstableArcana, CardKind::Wild, CardRarity::Apex, AbilityId::Count, "Unstable Arcana", "Your spell slots hold random attack spells from the book at 250% power, and each cast rolls a new one. You never choose which.", nullptr, 0, InDamage },
		{ CardId::Leyline, CardKind::Wild, CardRarity::Apex, AbilityId::Count, "Leyline", "Your spells deal double damage once you have stood still for a second. Moving resets it, but a longer stand keeps the charge up to 1.5 seconds after you move.", nullptr, TagGuard, InDamage },
	};
	static_assert(std::size(Cards) == size_t(CardId::Count));

	using CardMask = uint64_t;
	static_assert(size_t(CardId::Count) <= 64);

	inline const CardDef& GetCard(CardId id)
	{
		return Cards[size_t(id) < std::size(Cards) ? size_t(id) : 0];
	}

	inline constexpr CardMask CardBit(CardId id)
	{
		return CardMask(1) << uint32_t(id);
	}

	inline constexpr bool HasCard(CardMask mask, CardId id)
	{
		return (mask & CardBit(id)) != 0;
	}

	inline uint32_t CountCards(CardMask mask)
	{
		return uint32_t(std::popcount(mask));
	}

	inline TagMask CardTags(CardMask mask)
	{
		TagMask tags = 0;
		for (const CardDef& card : Cards)
			if (HasCard(mask, card.Id))
				tags |= card.Tags;
		return tags;
	}

	inline constexpr CardMask ApexMask = []
	{
		CardMask mask = 0;
		for (const CardDef& card : Cards)
			if (card.Rarity == CardRarity::Apex)
				mask |= CardBit(card.Id);
		return mask;
	}();

	inline bool IsApex(CardId id)
	{
		return HasCard(ApexMask, id);
	}

	inline CardId HeldApex(CardMask mask)
	{
		const CardMask held = mask & ApexMask;
		return held != 0 ? CardId(std::countr_zero(held)) : CardId::Count;
	}

	inline constexpr size_t CardChoices = 3;
	inline constexpr uint32_t MaxUpgradesPerSpell = 2;
	inline constexpr float CardRarityByOffer[][3] = { { 70.0f, 25.0f, 5.0f }, { 60.0f, 30.0f, 10.0f }, { 50.0f, 35.0f, 15.0f }, { 40.0f, 40.0f, 20.0f } };
	inline constexpr float CatchUpRarity[] = { -35.0f, 15.0f, 20.0f };
	inline constexpr float TagAffinity = 0.5f;

	inline std::array<float, 3> OfferClassWeights(uint8_t round, float behind)
	{
		const size_t stage = std::min<size_t>(size_t(std::max<uint8_t>(round, 1)) - 1, std::size(CardRarityByOffer) - 1);
		std::array<float, 3> classes{};
		for (size_t rarity = 0; rarity < classes.size(); rarity++)
			classes[rarity] = std::max(CardRarityByOffer[stage][rarity] + CatchUpRarity[rarity] * behind, 1.0f);
		return classes;
	}

	inline constexpr uint32_t SplitShards = 3;
	inline constexpr float SplitSpread = 0.42f;
	inline constexpr float SplitRange = 4.0f;
	inline constexpr float SplitPushScale = 0.6f;
	inline constexpr float SplitRadiusScale = 0.8f;
	inline constexpr uint32_t GalePulses = 3;
	inline constexpr float GaleFirstDelay = 0.3f;
	inline constexpr float GaleInterval = 0.45f;
	inline constexpr float GaleOffset = 2.0f;
	inline constexpr float GaleDrift = 0.6f;
	inline constexpr float GaleRadius = 1.5f;
	inline constexpr float GalePush = 9.0f;
	inline constexpr float GaleDamagePush = 0.5f;
	inline constexpr float TwinDelay = 0.5f;
	inline constexpr float TwinScale = 0.7f;
	inline constexpr float AfterimageDelay = 0.05f;
	inline constexpr float AfterimageRadius = 2.4f;
	inline constexpr float AfterimagePush = 14.0f;
	inline constexpr float AftershockDelay = 0.6f;
	inline constexpr float AftershockScale = 0.6f;
	inline constexpr float ImplosionDelay = 0.35f;
	inline constexpr float ImplosionPush = 18.0f;
	inline constexpr float PinballGrowth = 1.5f;
	inline constexpr float ThornScale = 1.5f;
	inline constexpr float BlackIceSeconds = 1.5f;
	inline constexpr float BlackIceFriction = 1.0f / 1.4f;
	inline constexpr float GlassCannonDealt = 1.5f;
	inline constexpr float GlassCannonTaken = 1.15f;
	inline constexpr float HeavyRobesTaken = 0.88f;
	inline constexpr float HeavyRobesSpeed = 0.85f;
	inline constexpr float EchoDelay = 0.3f;
	inline constexpr float EchoPushScale = 0.5f;
	inline constexpr float CraterDelay = 1.5f;
	inline constexpr float CraterScale = 0.85f;
	inline constexpr float FeatherfallSearch = 5.0f;
	inline constexpr float FeatherfallMargin = 0.7f;
	inline constexpr float FeatherfallFooting = 1.0f;
	inline constexpr float FeatherfallGrace = 0.6f;
	inline constexpr float LongChainScale = 1.5f;
	inline constexpr float LongChainCooldown = 0.8f;
	inline constexpr float WhiplashPull = 2.4f;
	inline constexpr uint32_t ClusterSkips = 2;
	inline constexpr uint32_t StickyPulses = 3;
	inline constexpr float StickyFirstDelay = 0.3f;
	inline constexpr float StickyInterval = 0.35f;
	inline constexpr float StickyRadius = 1.7f;
	inline constexpr float StickyPush = 9.0f;
	inline constexpr uint32_t FivefoldShards = 5;
	inline constexpr float SeekRange = 5.0f;
	inline constexpr float SeekTurnRate = 4.5f;
	inline constexpr uint32_t ForkedJumps = 2;
	inline constexpr float StaticSeconds = 1.5f;
	inline constexpr float DeepWellMana = 50.0f;
	inline constexpr float QuickeningRegen = 1.4f;
	inline constexpr float SiphonMana = 4.0f;
	inline constexpr float BrimmingShare = 0.8f;
	inline constexpr float BrimmingScale = 1.3f;
	inline constexpr float BloodPactRate = 0.25f;
	inline constexpr float LastStandHealth = 0.3f;
	inline constexpr float LastStandScale = 1.3f;
	inline constexpr int32_t VampiricHeal = 1;
	inline constexpr float KindlingDamage = 4.0f;
	inline constexpr float KindlingSeconds = 2.0f;
	inline constexpr float WildfireRadius = 3.5f;
	inline constexpr float WildfireDamage = 10.0f;
	inline constexpr float WildfireSeconds = 2.5f;
	inline constexpr float BrittleScale = 1.15f;
	inline constexpr int32_t ShatterDamage = 15;
	inline constexpr float ConductiveSeconds = 4.0f;
	inline constexpr float ConductiveScale = 2.0f;
	inline constexpr uint8_t OverloadEvery = 5;
	inline constexpr float OverloadScale = 1.5f;
	inline constexpr float BarrierShield = 8.0f;
	inline constexpr float SecondWindHealth = 0.25f;
	inline constexpr int32_t SecondWindHeal = 6;
	inline constexpr float PreciseChance = 0.15f;
	inline constexpr float PreciseMultiplier = 2.0f;
	inline constexpr float GlassCannonDamageDealt = 1.4f;
	inline constexpr float GlassCannonDamageTaken = 1.1f;
	inline constexpr float HeavyRobesDamageTaken = 0.88f;
	inline constexpr float BlackIceChill = 1.0f;
	inline constexpr float WhiplashDamage = 1.8f;
	inline constexpr float LongChainDamage = 1.25f;
	inline constexpr float ImplosionDamage = 0.45f;
	inline constexpr float FivefoldDamage = 0.7f;
	inline constexpr float SeekerDamage = 0.75f;
	inline constexpr int32_t HeartSwapSteal = 15;
	inline constexpr float DamageCraterDelay = 3.0f;
	inline constexpr float DamageCraterScale = 0.7f;
	inline constexpr float TwinDamage = 1.0f;
	inline constexpr float AfterimageDamage = 1.4f;
	inline constexpr uint8_t ApexFromRound = 4;
	inline constexpr float ApexChance = 0.12f;
	inline constexpr float ApexCatchUp = 0.25f;
	inline constexpr float ApexMagicSpeed = 4.0f;
	inline constexpr float ApexMagicCost = 1.5f;
	inline constexpr float ManaSingularityDamage = 0.005f;
	inline constexpr float ManaSingularityDrain = 1.0f;
	inline constexpr float MindOverMatterRatio = 0.8f;
	inline constexpr float MindOverMatterStall = 2.0f;
	inline constexpr float BloodMagicRate = 0.2f;
	inline constexpr float BloodMagicDamage = 1.3f;
	inline constexpr float BloodMagicHeal = 1.0f;
	inline constexpr float EchoChamberDelay = 0.25f;
	inline constexpr float EchoChamberCooldown = 1.75f;
	inline constexpr float ArcanaPower = 2.5f;
	inline constexpr float LeylineSeconds = 1.0f;
	inline constexpr float LeylineDamage = 2.0f;
	inline constexpr float LeylineLinger = 1.5f;

	enum ProjectilePerk : uint8_t
	{
		PerkSplit = 1 << 0,
		PerkPinball = 1 << 1,
		PerkSlick = 1 << 2,
		PerkSeek = 1 << 3,
	};

	enum StrikePerk : uint8_t
	{
		PerkShatter = 1 << 0,
		PerkAlong = 1 << 1,
		PerkFork = 1 << 2,
		PerkStatic = 1 << 3,
	};
}
