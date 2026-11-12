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
		Aftershock,
		TwinComets,
		Afterimage,
		IceShrapnel,
		Implosion,
		Pinball,
		Overcharge,
		ThornedMirror,
		BlackIce,
		GlassCannon,
		Echo,
		HeavyRobes,
		LongChain,
		Whiplash,
		ClusterBomb,
		StickyFire,
		RazorGlass,
		SeekerBlades,
		ForkedLightning,
		StaticCharge,
		DeepWell,
		Quickening,
		Siphon,
		Brimming,
		BloodPact,
		LastStand,
		Vampirism,
		Kindling,
		Brittle,
		Conductive,
		Overload,
		CrystalWard,
		Rally,
		Precise,
		WideSigils,
		Epicenter,
		ResonantGround,
		SwiftRunes,
		Flow,
		Rewind,
		QuickTongue,
		Tempo,
		SnapCast,
		ApexOfMagic,
		ManaSingularity,
		MindOverMatter,
		BloodMagic,
		EchoChamber,
		UnstableArcana,
		Quicksilver,
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
		{ CardId::Aftershock, CardKind::Upgrade, CardRarity::Rare, AbilityId::Comet, "Aftershock", "Half a second after the comet lands, a shockwave 60% wider bursts out of the crater and shoves everyone again.", "Half a second after the comet lands, a shockwave 60% wider bursts out of the crater for half damage.", TagFire },
		{ CardId::TwinComets, CardKind::Upgrade, CardRarity::Common, AbilityId::Comet, "Twin Comets", "A smaller second comet lands on the same spot half a second later.", "A second comet lands on the same spot half a second later.", TagFire },
		{ CardId::Afterimage, CardKind::Upgrade, CardRarity::Rare, AbilityId::Warp, "Afterimage", "Warp leaves a shockwave where you left.", "Warp leaves a shockwave where you left that hits for 16.", TagMana },
		{ CardId::IceShrapnel, CardKind::Upgrade, CardRarity::Common, AbilityId::GlacialNova, "Ice Shrapnel", "When the erupted spikes shatter, their shards burst outward and shove rivals again.", "When the erupted spikes shatter, their shards burst outward for 50% damage.", TagFrost },
		{ CardId::Implosion, CardKind::Upgrade, CardRarity::Rare, AbilityId::GravityWell, "Implosion", "When the eye closes the void bursts, flinging everyone outward.", "When the eye closes the void bursts, hitting everyone inside for 5.", TagMana },
		{ CardId::Pinball, CardKind::Upgrade, CardRarity::Rare, AbilityId::ArcLightning, "Pinball", "Every jump shoves half again as hard as the one before.", "Every jump hits half again as hard as the one before.", TagStorm },
		{ CardId::Overcharge, CardKind::Upgrade, CardRarity::Common, AbilityId::ArcLightning, "Overcharge", "Arc Lightning shoves 25% harder, jumps 60% farther and reaches one more rival.", "Arc Lightning hits 25% harder, jumps 60% farther and reaches one more rival.", TagStorm },
		{ CardId::ThornedMirror, CardKind::Upgrade, CardRarity::Common, AbilityId::DiamondWard, "Razor Facets", "Spells your Diamond Ward reflects come back 50% stronger.", nullptr, TagGuard },
		{ CardId::BlackIce, CardKind::Upgrade, CardRarity::Common, AbilityId::FrostSpear, "Black Ice", "A rival your Ice Spear hits freezes solid for half a second, then slides 60% further for 2 seconds.", "A rival your Ice Spear hits freezes solid for half a second and stays chilled a second longer.", TagFrost },
		{ CardId::GlassCannon, CardKind::Wild, CardRarity::Legendary, AbilityId::Count, "Glass Cannon", "Your spells shove 50% harder, but you fly 15% further.", "You deal 40% more damage, but take 10% more.", TagBlood },
		{ CardId::Echo, CardKind::Wild, CardRarity::Legendary, AbilityId::Count, "Echo", "After a dodge, your next spell casts again 0.3 s later at half strength.", nullptr, TagMana },
		{ CardId::HeavyRobes, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Heavy Robes", "You get shoved 12% less, but move 15% slower.", "You take 12% less damage, but move 15% slower.", TagGuard },
		{ CardId::LongChain, CardKind::Upgrade, CardRarity::Common, AbilityId::ChainHook, "Long Chain", "Chain Hook flies 20% farther and recharges 20% faster.", "Chain Hook flies 20% farther, recharges 20% faster and hits 25% harder.", TagBlood },
		{ CardId::Whiplash, CardKind::Upgrade, CardRarity::Rare, AbilityId::ChainHook, "Whiplash", "A rival you hook flies much further past you.", "A rival you hook flies further past you and takes 80% more damage.", TagBlood },
		{ CardId::ClusterBomb, CardKind::Upgrade, CardRarity::Common, AbilityId::Firebomb, "Cluster Bomb", "Firebomb skips twice and bursts three times, the last time biggest.", nullptr, TagFire },
		{ CardId::StickyFire, CardKind::Upgrade, CardRarity::Rare, AbilityId::Firebomb, "Sticky Fire", "Every Firebomb landing keeps burning for a second, shoving anyone who walks in.", "Every Firebomb landing keeps burning for a second, scorching anyone who walks in.", TagFire },
		{ CardId::RazorGlass, CardKind::Upgrade, CardRarity::Common, AbilityId::ObsidianBlades, "Razor Glass", "Obsidian blades shove 20% harder and cut through the first rival they hit.", "Obsidian blades deal 20% more damage and cut through the first rival they hit.", TagMana },
		{ CardId::SeekerBlades, CardKind::Upgrade, CardRarity::Rare, AbilityId::ObsidianBlades, "Seeker Blades", "Obsidian blades curve toward the nearest rival, but shove 15% less.", "Obsidian blades curve toward the nearest rival, but deal 40% less and shove 15% less.", TagMana },
		{ CardId::ForkedLightning, CardKind::Upgrade, CardRarity::Common, AbilityId::StormStrike, "Forked Lightning", "A second bolt strikes the same spot 0.4 s later and shoves at 60% strength.", "A second bolt strikes the same spot 0.4 s later for 60% damage.", TagStorm },
		{ CardId::StaticCharge, CardKind::Upgrade, CardRarity::Rare, AbilityId::StormStrike, "Static Charge", "Rivals Storm Strike hits can't dodge for 1.5 seconds.", nullptr, TagStorm },
		{ CardId::DeepWell, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Deep Well", "50 more maximum mana.", nullptr, TagMana, InDamage },
		{ CardId::Quickening, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Quickening", "Your mana comes back 40% faster.", nullptr, TagMana, InDamage },
		{ CardId::Siphon, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Siphon", "Each hit you land restores 4 mana.", nullptr, TagMana, InDamage },
		{ CardId::Brimming, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Brimming", "Spells cast with over 80% mana deal 30% more damage.", nullptr, TagMana, InDamage },
		{ CardId::BloodPact, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Blood Pact", "Every point of damage you take restores 2 mana.", nullptr, TagBlood | TagMana, InDamage },
		{ CardId::LastStand, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Last Stand", "You deal 50% more damage while under 40% health.", nullptr, TagBlood, InDamage },
		{ CardId::Vampirism, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Vampirism", "Heal for 12% of all the damage you deal.", nullptr, TagBlood, InDamage },
		{ CardId::Kindling, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Kindling", "Your spells, except Ember Bolt, set rivals alight for 4 damage over 2 seconds.", nullptr, TagFire, InDamage },
		{ CardId::Brittle, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Brittle", "Your chills last twice as long, and chilled rivals take 25% more damage from you.", nullptr, TagFrost, InDamage, AbilityId::FrostSpear, AbilityId::GlacialNova },
		{ CardId::Conductive, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Conductive", "Your lightning marks rivals. Your next Ember Bolt on a marked rival deals double.", nullptr, TagStorm, InDamage, AbilityId::ArcLightning, AbilityId::StormStrike },
		{ CardId::Overload, CardKind::Wild, CardRarity::Legendary, AbilityId::Count, "Overload", "Every fourth spell you cast costs nothing and deals 50% more.", nullptr, TagStorm | TagMana, InDamage },
		{ CardId::CrystalWard, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Crystal Ward", "A crystal ward blocks the next hit completely, then shatters and re-forms 20 seconds later. Burns and lava get through.", nullptr, TagGuard, InDamage },
		{ CardId::Rally, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Rally", "Under 40% health you regenerate 1.25 health a second, up to 40%. Taking damage pauses it for 2 seconds.", nullptr, TagGuard, InDamage },
		{ CardId::Precise, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Precise", "Your spells have a 15% chance to deal double damage.", nullptr, TagGuard, InDamage },
		{ CardId::WideSigils, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Wide Sigils", "Your area spells and explosions cover 25% more ground.", nullptr, TagFire },
		{ CardId::Epicenter, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Epicenter", "Your area spells and explosions cover 45% more ground, but shove 10% less.", "Your area spells and explosions cover 45% more ground, but deal 15% less damage.", TagFire },
		{ CardId::ResonantGround, CardKind::Wild, CardRarity::Legendary, AbilityId::Count, "Resonant Ground", "A second after your area spells land, the ground there pulses again at half strength.", nullptr, TagFire | TagStorm },
		{ CardId::SwiftRunes, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Swift Runes", "Your spells recharge 20% faster.", nullptr, TagMana },
		{ CardId::Flow, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Flow", "Every hit you land takes 0.6 s off your other spells' cooldowns.", nullptr, TagMana | TagStorm },
		{ CardId::Rewind, CardKind::Wild, CardRarity::Legendary, AbilityId::Count, "Rewind", "Each spell you cast has a 20% chance to skip its cooldown.", nullptr, TagMana },
		{ CardId::QuickTongue, CardKind::Wild, CardRarity::Common, AbilityId::Count, "Quick Tongue", "You cast and charge spells 20% faster.", nullptr, TagStorm },
		{ CardId::Tempo, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Tempo", "Each spell you cast makes your casting 6% faster for 3 seconds, stacking up to 30%.", nullptr, TagStorm },
		{ CardId::SnapCast, CardKind::Wild, CardRarity::Rare, AbilityId::Count, "Snap Cast", "The first spell you cast within 1.5 s of a dodge goes off instantly. Charged spells start fully charged.", nullptr, TagStorm | TagGuard },
		{ CardId::ApexOfMagic, CardKind::Wild, CardRarity::Apex, AbilityId::Count, "Apex of Magic", "Casting and cooldowns are 300% faster, but every spell costs 50% more mana.", nullptr, TagStorm | TagMana, InDamage },
		{ CardId::ManaSingularity, CardKind::Wild, CardRarity::Apex, AbilityId::Count, "Mana Singularity", "Spells cost no mana and deal 1% more damage for every 2 mana you hold. Hits drain your mana as well as your health.", nullptr, TagMana, InDamage },
		{ CardId::MindOverMatter, CardKind::Wild, CardRarity::Apex, AbilityId::Count, "Mind over Matter", "Half of every hit drains your mana instead of your health. After a hit, your mana comes back 75% slower for 2 seconds.", nullptr, TagMana | TagGuard, InDamage },
		{ CardId::BloodMagic, CardKind::Wild, CardRarity::Apex, AbilityId::Count, "Blood Magic", "You have no mana. Spells cost 1 health per 5 mana instead, deal 30% more and heal you for all the damage they deal. You also get 10 more health.", nullptr, TagBlood, InDamage },
		{ CardId::EchoChamber, CardKind::Wild, CardRarity::Apex, AbilityId::Count, "Echo Chamber", "Every spell casts two extra copies at 60% strength: projectiles fan out beside it and area spells land around your target. Cooldowns are 40% longer. Warp, wards, Chain Hook and Frost Nova don't echo.", nullptr, TagStorm, InDamage },
		{ CardId::UnstableArcana, CardKind::Wild, CardRarity::Apex, AbilityId::Count, "Unstable Arcana", "Every spell you cast surges at random: double damage, 75% more area, two extra copies, or no cooldown.", nullptr, 0, InDamage },
		{ CardId::Quicksilver, CardKind::Wild, CardRarity::Apex, AbilityId::Count, "Quicksilver", "Keep moving to build charge. After 1.2 s on the move your spells deal double damage. Standing still drains it and every hit you take halves it.", nullptr, TagStorm, InDamage },
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
	inline constexpr float SplitRange = 3.0f;
	inline constexpr float SplitPushScale = 0.6f;
	inline constexpr float SplitRadiusScale = 0.8f;
	inline constexpr float TwinDelay = 0.5f;
	inline constexpr float TwinScale = 0.7f;
	inline constexpr float AftershockDelay = 0.5f;
	inline constexpr float AftershockRadius = 1.6f;
	inline constexpr float AfterimageDelay = 0.05f;
	inline constexpr float AfterimageRadius = 2.4f;
	inline constexpr float AfterimagePush = 14.0f;
	inline constexpr float ShrapnelDelay = 0.55f;
	inline constexpr float ShrapnelScale = 0.5f;
	inline constexpr float ShrapnelReach = 1.2f;
	inline constexpr float ImplosionDelay = 0.35f;
	inline constexpr float ImplosionPush = 18.0f;
	inline constexpr float PinballGrowth = 1.5f;
	inline constexpr float OverchargeReach = 1.6f;
	inline constexpr float OverchargeScale = 1.25f;
	inline constexpr float ThornScale = 1.5f;
	inline constexpr float BlackIceSeconds = 2.0f;
	inline constexpr float BlackIceFriction = 1.0f / 1.6f;
	inline constexpr float BlackIceFreeze = 0.5f;
	inline constexpr float GlassCannonDealt = 1.5f;
	inline constexpr float GlassCannonTaken = 1.15f;
	inline constexpr float HeavyRobesTaken = 0.88f;
	inline constexpr float HeavyRobesSpeed = 0.85f;
	inline constexpr float EchoDelay = 0.3f;
	inline constexpr float EchoPushScale = 0.5f;
	inline constexpr float LongChainScale = 1.2f;
	inline constexpr float LongChainCooldown = 0.8f;
	inline constexpr float WhiplashPull = 2.4f;
	inline constexpr uint32_t ClusterSkips = 2;
	inline constexpr uint32_t StickyPulses = 3;
	inline constexpr float StickyFirstDelay = 0.3f;
	inline constexpr float StickyInterval = 0.35f;
	inline constexpr float StickyRadius = 1.7f;
	inline constexpr float StickyPush = 9.0f;
	inline constexpr float RazorGlassScale = 1.2f;
	inline constexpr float SeekRange = 5.0f;
	inline constexpr float SeekTurnRate = 8.0f;
	inline constexpr float ForkedDelay = 0.4f;
	inline constexpr float ForkedScale = 0.6f;
	inline constexpr float StaticSeconds = 1.5f;
	inline constexpr float DeepWellMana = 50.0f;
	inline constexpr float QuickeningRegen = 1.4f;
	inline constexpr float SiphonMana = 4.0f;
	inline constexpr float BrimmingShare = 0.8f;
	inline constexpr float BrimmingScale = 1.3f;
	inline constexpr float BloodPactMana = 2.0f;
	inline constexpr float LastStandHealth = 0.4f;
	inline constexpr float LastStandScale = 1.5f;
	inline constexpr float VampirismShare = 0.12f;
	inline constexpr float KindlingDamage = 4.0f;
	inline constexpr float KindlingSeconds = 2.0f;
	inline constexpr float BrittleScale = 1.25f;
	inline constexpr float BrittleChill = 2.0f;
	inline constexpr float ConductiveSeconds = 4.0f;
	inline constexpr float ConductiveScale = 2.0f;
	inline constexpr uint8_t OverloadEvery = 4;
	inline constexpr float OverloadScale = 1.5f;
	inline constexpr float CrystalWardSeconds = 20.0f;
	inline constexpr float RallyHealth = 0.4f;
	inline constexpr float RallyRate = 1.25f;
	inline constexpr float RallyPause = 2.0f;
	inline constexpr float PreciseChance = 0.15f;
	inline constexpr float PreciseMultiplier = 2.0f;
	inline constexpr float WideSigilsArea = 0.25f;
	inline constexpr float EpicenterArea = 0.45f;
	inline constexpr float EpicenterDamage = 0.85f;
	inline constexpr float EpicenterPush = 0.9f;
	inline constexpr float ResonanceDelay = 1.0f;
	inline constexpr float ResonanceScale = 0.5f;
	inline constexpr float SwiftRunesCooldown = 0.8f;
	inline constexpr float FlowRefund = 0.6f;
	inline constexpr float FlowSpacing = 0.15f;
	inline constexpr float RewindChance = 0.2f;
	inline constexpr float QuickTongueSpeed = 1.2f;
	inline constexpr float TempoStep = 0.06f;
	inline constexpr uint8_t TempoMaxStacks = 5;
	inline constexpr float TempoSeconds = 3.0f;
	inline constexpr float SnapWindow = 1.5f;
	inline constexpr float GlassCannonDamageDealt = 1.4f;
	inline constexpr float GlassCannonDamageTaken = 1.1f;
	inline constexpr float HeavyRobesDamageTaken = 0.88f;
	inline constexpr float BlackIceChill = 2.0f;
	inline constexpr float WhiplashDamage = 1.8f;
	inline constexpr float LongChainDamage = 1.25f;
	inline constexpr float ImplosionDamage = 0.45f;
	inline constexpr float SeekerDamage = 0.6f;
	inline constexpr float SeekerPush = 0.85f;
	inline constexpr float TwinDamage = 1.0f;
	inline constexpr float AfterimageDamage = 1.4f;
	inline constexpr uint8_t ApexFromRound = 4;
	inline constexpr float ApexChance = 0.12f;
	inline constexpr float ApexCatchUp = 0.25f;
	inline constexpr float ApexMagicSpeed = 4.0f;
	inline constexpr float ApexMagicCost = 1.5f;
	inline constexpr float ManaSingularityDamage = 0.005f;
	inline constexpr float ManaSingularityDrain = 1.0f;
	inline constexpr float MindOverMatterShare = 0.5f;
	inline constexpr float MindOverMatterRatio = 1.25f;
	inline constexpr float MindOverMatterStall = 2.0f;
	inline constexpr float MindOverMatterRegen = 0.25f;
	inline constexpr float BloodMagicRate = 0.2f;
	inline constexpr float BloodMagicDamage = 1.3f;
	inline constexpr float BloodMagicHeal = 1.0f;
	inline constexpr int32_t BloodMagicHealth = 10;
	inline constexpr float BloodMagicRegen = 0.0f;
	inline constexpr float EchoChamberShare = 0.6f;
	inline constexpr float EchoChamberCooldown = 1.4f;
	inline constexpr float EchoChamberFan = 0.21f;
	inline constexpr float EchoChamberArea = 0.7f;
	inline constexpr float ArcanaDamage = 2.0f;
	inline constexpr float ArcanaArea = 1.75f;
	inline constexpr float QuicksilverSeconds = 1.2f;
	inline constexpr float QuicksilverDamage = 2.0f;
	inline constexpr float QuicksilverDrain = 2.0f;
	inline constexpr float QuicksilverHit = 0.5f;

	enum class ArcanaSurge : uint8_t
	{
		None,
		Power,
		Area,
		Copies,
		Refund,
	};

	enum ProjectilePerk : uint8_t
	{
		PerkSplit = 1 << 0,
		PerkPinball = 1 << 1,
		PerkSlick = 1 << 2,
		PerkSeek = 1 << 3,
		PerkBurst = 1 << 4,
		PerkReach = 1 << 5,
	};

	enum StrikePerk : uint8_t
	{
		PerkAlong = 1 << 1,
		PerkStatic = 1 << 3,
	};
}
