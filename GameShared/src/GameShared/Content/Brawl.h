#pragma once
#include "GameShared/Content/Cards.h"
#include "GameShared/Content/Definitions.h"
#include "GameShared/Content/Pvp.h"
#include "GameShared/Content/Traits.h"
#include "GameShared/World/Level.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>

namespace Game
{
	enum class BrawlPhase : uint8_t
	{
		Draft,
		Countdown,
		Fight,
		Over,
		Pick,
	};

	using CardOffer = std::array<CardId, CardChoices>;
	using BrawlSpells = std::array<AbilityId, SkillSlots>;

	inline constexpr AbilityId BrawlBasic = AbilityId::EmberBolt;
	inline constexpr AbilityId BrawlSpellPool[] = {
		AbilityId::Gust,
		AbilityId::Comet,
		AbilityId::Warp,
		AbilityId::Shockwave,
		AbilityId::GravityWell,
		AbilityId::ArcLightning,
		AbilityId::Swap,
		AbilityId::MirrorWard,
		AbilityId::Frostbite,
		AbilityId::ChainHook,
		AbilityId::Firebomb,
		AbilityId::SplittingBolt,
		AbilityId::StormStrike,
	};
	inline constexpr BrawlSpells DefaultBrawlSpells = { AbilityId::Gust, AbilityId::Warp, AbilityId::Comet };

	inline bool IsBrawlSpell(AbilityId id)
	{
		return std::find(std::begin(BrawlSpellPool), std::end(BrawlSpellPool), id) != std::end(BrawlSpellPool);
	}

	inline bool AssignSpell(BrawlSpells& spells, uint32_t slot, AbilityId spell)
	{
		if (slot >= spells.size() || !IsBrawlSpell(spell))
			return false;
		const auto existing = std::find(spells.begin(), spells.end(), spell);
		if (existing != spells.end())
			std::swap(*existing, spells[slot]);
		else
			spells[slot] = spell;
		return true;
	}

	inline constexpr float DummyRadius = 0.36f;
	inline constexpr int32_t DummyHealth = 100;
	inline constexpr float DummyBurnPerSecond = 60.0f;
	inline constexpr float DummyRegenPerSecond = 8.0f;
	inline constexpr float DummyMendDelay = 3.0f;
	inline constexpr float DummyWalkSpeed = 1.8f;
	inline constexpr float DummyPatrolSpeed = 2.2f;
	inline constexpr float DummyRestSeconds = 1.2f;
	inline constexpr float DummyRespawnSeconds = 2.5f;
	inline constexpr float DummyStraySeconds = 8.0f;
	inline constexpr uint32_t GoldenDummyOdds = 12;

	struct BrawlLoadout
	{
		BrawlSpells Spells = DefaultBrawlSpells;
		CardMask Cards = 0;
		CardOffer Offer{ CardId::Count, CardId::Count, CardId::Count };
		bool OfferPending = false;
		bool Ready = false;
	};

	struct Crater
	{
		Vec2 Center;
		float Radius = 0.0f;
		float Time = 0.0f;
	};

	inline constexpr size_t MaxCraters = 16;

	enum class BrawlShape : uint8_t
	{
		Disc,
		Lake,
		Count,
	};

	struct CollapsePlan
	{
		Vec2 Center;
		float StartRadius = 12.0f;
		uint32_t Seed = 1;
		BrawlShape Shape = BrawlShape::Disc;
		std::array<Crater, MaxCraters> Craters{};
		uint8_t CraterCount = 0;
	};

	struct BrawlStatus
	{
		BrawlPhase Phase = BrawlPhase::Draft;
		uint8_t Round = 0;
		uint8_t Rounds = 5;
		float Timer = 0.0f;
		float Clock = -1e5f;
		CollapsePlan Plan;
		uint32_t RoundWinner = NoPlayer;
		uint8_t Ready = 0;
		uint8_t Players = 0;
	};

	inline constexpr int32_t BrawlHealth = 100;
	inline constexpr float BrawlMoveSpeed = 5.0f;
	inline constexpr float BrawlDodgeCooldown = 2.4f;
	inline constexpr float PushFriction = 3.2f;
	inline constexpr float PushControlSpeed = 9.0f;
	inline constexpr float PushControlLoss = 0.85f;
	inline constexpr float PushStop = 0.25f;
	inline constexpr float MaxPush = 42.0f;
	inline constexpr float PushStagger = 12.0f;
	inline constexpr float MissingHealthKnockback = 0.6f;
	inline constexpr float HookPull = 1.8f;
	inline constexpr float HookAnchorSeconds = 1.0f;
	inline constexpr float HookYankSpeed = 20.0f;
	inline constexpr float HookYankStop = 0.6f;
	inline constexpr float FirebombSkipDistance = 2.6f;
	inline constexpr uint32_t BoltShards = 3;
	inline constexpr float BoltShardSpread = 0.4f;
	inline constexpr float LavaDamagePerSecond = 14.0f;
	inline constexpr float AfterburnSeconds = 0.8f;
	inline constexpr float AfterburnDamagePerSecond = 8.0f;
	inline constexpr float BrawlDraftSeconds = 20.0f;
	inline constexpr float BrawlPickSeconds = 20.0f;
	inline constexpr float BrawlCountdownSeconds = 3.0f;
	inline constexpr float BrawlOverSeconds = 3.5f;
	inline constexpr float CollapseGrace = 7.0f;
	inline constexpr float CollapseRate = 0.16f;
	inline constexpr float CollapseMinRadius = 2.8f;
	inline constexpr float CollapseJitter = 1.6f;
	inline constexpr float FinalCollapseStart = 78.0f;
	inline constexpr float FinalCollapseRate = 0.35f;
	inline constexpr float CollapseWarning = 2.5f;
	inline constexpr float BrawlSpawnRing = 0.62f;
	inline constexpr uint16_t RoundWinPoints = 3;
	inline constexpr uint16_t KnockoutPoints = 1;
	inline constexpr uint16_t PlacementPoints[] = { 3, 2, 1 };
	inline constexpr float DamagePushScale = 0.3f;
	inline constexpr float DamagePullScale = 0.55f;
	inline constexpr float BrawlMana = 100.0f;
	inline constexpr float BrawlManaRegen = 16.0f;
	inline constexpr float DamageCreditSeconds = 8.0f;
	inline constexpr float AssistShare = 0.3f;
	inline constexpr float LakeScale = 1.4f;
	inline constexpr float LakeAspect = 1.8f;
	inline constexpr float IceFriction = 0.625f;
	inline constexpr float IceBounce = 0.8f;
	inline constexpr float BounceSpeed = 2.0f;
	inline constexpr float HoleStartRadius = 0.9f;
	inline constexpr float HoleRate = 0.075f;
	inline constexpr float HoleSpawnClearance = 2.6f;
	inline constexpr float WaterDamagePerSecond = 13.0f;

	inline float BrawlStartRadius(uint32_t players)
	{
		return std::clamp(6.5f + 1.1f * float(players), 8.5f, 12.0f);
	}

	inline BrawlShape ShapeOf(MapKind map)
	{
		return map == MapKind::FrozenLake ? BrawlShape::Lake : BrawlShape::Disc;
	}

	struct BrawlTerms
	{
		const char* Hazard = "";
		const char* Floor = "";
		const char* Breaking = "";
		const char* Sudden = "";
		const char* Out = "";
		const char* Fell = "";
		const char* Danger = "";
		const char* Place = "";
		const char* Title = "";
		const char* Rules = "";
		const char* DamageRules = "";
	};

	inline const BrawlTerms& TermsFor(MapKind map)
	{
		static constexpr BrawlTerms Caldera{ "lava", "stone", "The caldera is crumbling", "Sudden death, the last stone is sinking", "Burned Away", "burned in the lava", "You are burning! Get back onto the stone", "caldera", "the Caldera", "Spells don't hurt, they shove. Lava burns, and the stone crumbles from the edge inward. The lower your health, the further you fly.",
			"Spells hurt and cost mana, and they still nudge. Lava burns, and the stone crumbles from the edge inward." };
		static constexpr BrawlTerms Lake{ "freezing water", "ice", "The ice is breaking up", "Sudden death, the last floe is sinking", "Through the Ice", "went through the ice", "You are freezing! Get back onto the ice", "lake", "the Frozen Lake", "Spells don't hurt, they shove, and on the ice every shove slides far. Snow drifts grip, ice pillars bounce you back, and the ice breaks up from the shore and around the air holes.",
			"Spells hurt and cost mana, and on the ice even a nudge slides far. Snow drifts grip, ice pillars bounce you back, and the ice breaks up from the shore and around the air holes." };
		return map == MapKind::FrozenLake ? Lake : Caldera;
	}

	inline Vec2 ArenaPoint(const CollapsePlan& plan, float angle, float radius)
	{
		const Vec2 direction = FromAngle(angle) * radius;
		return plan.Shape == BrawlShape::Lake ? plan.Center + Vec2(direction.x * LakeScale, direction.y * LakeScale / LakeAspect) : plan.Center + direction;
	}

	inline float ArenaRank(const CollapsePlan& plan, Vec2 point)
	{
		const Vec2 offset = point - plan.Center;
		return plan.Shape == BrawlShape::Lake ? Length(Vec2(offset.x, offset.y * LakeAspect)) / LakeScale : Length(offset);
	}

	inline float CollapseNoise(int32_t x, int32_t y, uint32_t seed)
	{
		uint32_t h = uint32_t(x) * 0x8DA6B343u ^ uint32_t(y) * 0xD8163841u ^ seed * 0xCB1AB31Fu;
		h ^= h >> 15;
		h *= 0x2C1B3C6Du;
		h ^= h >> 12;
		h *= 0x297A2D39u;
		h ^= h >> 15;
		return float(h & 0xFFFFu) / 65535.0f;
	}

	struct CellCoord
	{
		int32_t X = 0;
		int32_t Y = 0;

		bool operator==(const CellCoord&) const = default;
	};

	inline constexpr float ChunkSize = 2.0f;

	inline Vec2 CellSeed(CellCoord cell)
	{
		return { float(cell.X) + 0.2f + 0.6f * CollapseNoise(cell.X, cell.Y, 0x51A7u), float(cell.Y) + 0.2f + 0.6f * CollapseNoise(cell.X, cell.Y, 0x7E3Bu) };
	}

	inline Vec2 ChunkSeed(CellCoord chunk)
	{
		return Vec2(float(chunk.X) + 0.2f + 0.6f * CollapseNoise(chunk.X, chunk.Y, 0x3C91u), float(chunk.Y) + 0.2f + 0.6f * CollapseNoise(chunk.X, chunk.Y, 0x95D3u)) * ChunkSize;
	}

	template<typename SeedOf>
	CellCoord NearestSeed(Vec2 point, Vec2 lattice, SeedOf seedOf)
	{
		const int32_t x = int32_t(std::floor(lattice.x));
		const int32_t y = int32_t(std::floor(lattice.y));
		CellCoord best{ x, y };
		float nearest = std::numeric_limits<float>::max();
		for (int32_t dy = -1; dy <= 1; dy++)
			for (int32_t dx = -1; dx <= 1; dx++)
				if (const float distance = LengthSquared(point - seedOf(CellCoord{ x + dx, y + dy })); distance < nearest)
				{
					nearest = distance;
					best = { x + dx, y + dy };
				}
		return best;
	}

	inline CellCoord CellAt(Vec2 position)
	{
		return NearestSeed(position, position, CellSeed);
	}

	inline CellCoord ChunkOf(CellCoord cell)
	{
		const Vec2 seed = CellSeed(cell);
		return NearestSeed(seed, seed / ChunkSize, ChunkSeed);
	}

	inline uint32_t HoleCount(const CollapsePlan& plan)
	{
		return plan.Shape != BrawlShape::Lake ? 0u : plan.StartRadius >= 10.5f ? 3u : 2u;
	}

	inline Vec2 HoleCenter(const CollapsePlan& plan, uint32_t index)
	{
		const float angle = 2.0f * Pi * (float(index) + 0.7f * CollapseNoise(int32_t(index), 0x4C, plan.Seed)) / float(std::max(HoleCount(plan), 1u));
		return ArenaPoint(plan, angle, plan.StartRadius * (0.36f + 0.3f * CollapseNoise(int32_t(index), 0x4D, plan.Seed)));
	}

	inline float HoleCollapseTime(const CollapsePlan& plan, Vec2 point)
	{
		float time = 1e6f;
		for (uint32_t i = 0; i < HoleCount(plan); i++)
		{
			const float reach = Length(point - HoleCenter(plan, i)) - HoleStartRadius;
			time = std::min(time, reach <= 0.0f ? -1e6f : reach / HoleRate);
		}
		return time;
	}

	inline float PlannedCollapseTime(const CollapsePlan& plan, Vec2 seed, CellCoord chunk)
	{
		const float rank = ArenaRank(plan, seed) + (CollapseNoise(chunk.X, chunk.Y, plan.Seed) - 0.5f) * CollapseJitter;
		if (rank >= plan.StartRadius)
			return -1e6f;
		const float hole = HoleCollapseTime(plan, seed);
		if (rank >= CollapseMinRadius)
			return std::min(hole, CollapseGrace + (plan.StartRadius - rank) / CollapseRate);
		const float settled = CollapseGrace + (plan.StartRadius - CollapseMinRadius) / CollapseRate;
		return std::min(hole, std::max(settled, FinalCollapseStart) + (CollapseMinRadius - rank) / FinalCollapseRate);
	}

	inline float ChunkCollapseTime(const CollapsePlan& plan, CellCoord chunk)
	{
		const Vec2 seed = ChunkSeed(chunk);
		float time = PlannedCollapseTime(plan, seed, chunk);
		for (uint32_t i = 0; i < std::min<uint32_t>(plan.CraterCount, MaxCraters); i++)
			if (const Crater& crater = plan.Craters[i]; LengthSquared(seed - crater.Center) <= crater.Radius * crater.Radius)
				time = std::min(time, crater.Time);
		return time;
	}

	inline float TileCollapseTime(const CollapsePlan& plan, int32_t x, int32_t y)
	{
		return ChunkCollapseTime(plan, ChunkOf({ x, y }));
	}

	inline bool IsLavaTerrain(const Level& level, Vec2 position)
	{
		const CellCoord cell = CellAt(position);
		const TerrainId terrain = level.Map.GetTerrain(cell.X, cell.Y);
		if (IsHazardLiquid(terrain))
			return true;
		if (IsBrawlFloor(terrain))
			return false;
		return IsHazardLiquid(level.Map.GetTerrain(int32_t(std::floor(position.x)), int32_t(std::floor(position.y))));
	}

	inline bool IsBrawlLava(const Level& level, const BrawlStatus& brawl, Vec2 position)
	{
		const CellCoord cell = CellAt(position);
		const TerrainId terrain = level.Map.GetTerrain(cell.X, cell.Y);
		if (IsHazardLiquid(terrain))
			return true;
		if (IsBrawlFloor(terrain))
			return TileCollapseTime(brawl.Plan, cell.X, cell.Y) <= brawl.Clock;
		return IsHazardLiquid(level.Map.GetTerrain(int32_t(std::floor(position.x)), int32_t(std::floor(position.y))));
	}

	inline float SurfaceFriction(const Level& level, Vec2 position)
	{
		const CellCoord cell = CellAt(position);
		return level.Map.GetTerrain(cell.X, cell.Y) == TerrainId::Ice ? IceFriction : 1.0f;
	}

	inline SkillTraits MakeBrawlSkill(AbilityId id, float cooldownScale, BrawlMode mode)
	{
		const AbilityDef& def = GetAbility(id);
		SkillTraits skill;
		skill.Ability = id;
		skill.Usable = true;
		skill.CastTime = def.CastTime;
		skill.Cooldown = (mode == BrawlMode::Damage && def.BrawlCooldown > 0.0f ? def.BrawlCooldown : def.Cooldown) * cooldownScale;
		skill.MoveScale = def.CastMove;
		skill.ManaCost = mode == BrawlMode::Damage ? def.BrawlMana : 0.0f;
		return skill;
	}

	inline bool EchoesInChamber(AbilityId id)
	{
		const AbilityKind kind = GetAbility(id).Kind;
		return kind != AbilityKind::Dash && kind != AbilityKind::Ward;
	}

	inline void ApplyApex(PlayerTraits& traits, CardMask cards)
	{
		for (uint32_t slot = 0; slot < AbilitySlots; slot++)
		{
			SkillTraits& skill = traits.Skills[slot];
			if (skill.IsEmpty())
				continue;
			if (HasCard(cards, CardId::ApexOfMagic))
			{
				skill.CastTime /= ApexMagicSpeed;
				skill.Cooldown /= ApexMagicSpeed;
				skill.ManaCost *= ApexMagicCost;
			}
			if (HasCard(cards, CardId::ManaSingularity))
				skill.ManaCost = 0.0f;
			if (HasCard(cards, CardId::EchoChamber) && EchoesInChamber(skill.Ability))
				skill.Cooldown *= EchoChamberCooldown;
			if (HasCard(cards, CardId::UnstableArcana) && slot > 0)
				skill.DamageScale *= ArcanaPower;
		}
		if (HasCard(cards, CardId::BloodMagic))
		{
			traits.MaxMana = 0.0f;
			traits.ManaRegen = 0.0f;
			traits.BloodRate = BloodMagicRate;
		}
	}

	inline PlayerTraits MakeBrawlTraits(const BrawlLoadout& loadout, BrawlMode mode)
	{
		PlayerTraits traits;
		traits.MoveSpeed = BrawlMoveSpeed * (HasCard(loadout.Cards, CardId::HeavyRobes) ? HeavyRobesSpeed : 1.0f);
		traits.MaxHealth = BrawlHealth;
		traits.MaxMana = mode == BrawlMode::Damage ? BrawlMana + (HasCard(loadout.Cards, CardId::DeepWell) ? DeepWellMana : 0.0f) : 0.0f;
		traits.ManaRegen = mode == BrawlMode::Damage ? BrawlManaRegen * (HasCard(loadout.Cards, CardId::Quickening) ? QuickeningRegen : 1.0f) : 0.0f;
		if (mode == BrawlMode::Damage)
		{
			traits.BloodRate = HasCard(loadout.Cards, CardId::BloodPact) ? BloodPactRate : 0.0f;
			traits.OverloadEvery = HasCard(loadout.Cards, CardId::Overload) ? OverloadEvery : uint8_t(0);
			traits.CritChance = HasCard(loadout.Cards, CardId::Precise) ? PreciseChance : 0.0f;
			traits.CritMultiplier = PreciseMultiplier;
		}
		traits.Skills[0] = MakeBrawlSkill(BrawlBasic, 1.0f, mode);
		for (uint32_t i = 0; i < SkillSlots; i++)
		{
			traits.Skills[i + 1] = IsBrawlSpell(loadout.Spells[i]) ? MakeBrawlSkill(loadout.Spells[i], 1.0f, mode) : SkillTraits{};
			if (loadout.Spells[i] == AbilityId::ChainHook && HasCard(loadout.Cards, CardId::LongChain))
			{
				traits.Skills[i + 1].RangeScale = LongChainScale;
				traits.Skills[i + 1].Cooldown *= LongChainCooldown;
			}
		}
		traits.Dodge = MakeBrawlSkill(AbilityId::Roll, 1.0f, mode);
		traits.Dodge.Cooldown = BrawlDodgeCooldown;
		if (mode == BrawlMode::Damage)
			ApplyApex(traits, loadout.Cards);
		return traits;
	}

	inline const char* CardText(const CardDef& card, BrawlMode mode)
	{
		return mode == BrawlMode::Damage && card.DamageDescription ? card.DamageDescription : card.Description;
	}

	inline bool CardFitsLoadout(const BrawlLoadout& loadout, CardId id, BrawlMode mode)
	{
		const CardDef& card = GetCard(id);
		if (HasCard(loadout.Cards, id) || (card.Modes & (mode == BrawlMode::Damage ? InDamage : InKnockback)) == 0)
			return false;
		if (card.Rarity == CardRarity::Apex && (loadout.Cards & ApexMask) != 0)
			return false;
		if (card.NeedsCard != CardId::Count && !HasCard(loadout.Cards, card.NeedsCard))
			return false;
		const auto owns = [&](AbilityId spell) { return spell != AbilityId::Count && std::find(loadout.Spells.begin(), loadout.Spells.end(), spell) != loadout.Spells.end(); };
		if (card.Needs != AbilityId::Count && !owns(card.Needs) && !owns(card.NeedsAlt))
			return false;
		if (card.Kind != CardKind::Upgrade)
			return true;
		if (card.Spell != BrawlBasic && std::find(loadout.Spells.begin(), loadout.Spells.end(), card.Spell) == loadout.Spells.end())
			return false;
		uint32_t owned = 0;
		for (const CardDef& other : Cards)
			owned += other.Kind == CardKind::Upgrade && other.Spell == card.Spell && HasCard(loadout.Cards, other.Id) ? 1u : 0u;
		return owned < MaxUpgradesPerSpell;
	}
}
