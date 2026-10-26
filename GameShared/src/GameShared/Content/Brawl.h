#pragma once
#include "GameShared/Content/Cards.h"
#include "GameShared/Content/Definitions.h"
#include "GameShared/Content/Pvp.h"
#include "GameShared/Content/Traits.h"
#include "GameShared/Core/Random.h"
#include "GameShared/World/Level.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>

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
		AbilityId::FrostSpear,
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

	struct LightningBolt
	{
		Vec2 Center;
		float Time = 0.0f;
	};

	inline constexpr size_t MaxBolts = 8;

	enum class BrawlShape : uint8_t
	{
		Disc,
		Lake,
		Ruins,
		Peak,
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
		std::array<LightningBolt, MaxBolts> Bolts{};
		uint8_t BoltCount = 0;
		bool Steady = false;
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
		uint8_t RoundTeam = NoTeam;
		uint8_t Ready = 0;
		uint8_t Players = 0;
	};

	inline constexpr int32_t BrawlHealth = 100;
	inline constexpr float BrawlMoveSpeed = 4.5f;
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
	inline constexpr float ChargeCrackSeconds = 1.0f;
	inline constexpr float ChargeCrackPower = 0.55f;
	inline constexpr float SpearFullSpeed = 23.0f;
	inline constexpr float SpearFullRange = 7.5f;
	inline constexpr float SpearFullRadius = 0.34f;
	inline constexpr float SpearFullPush = 24.0f;
	inline constexpr int32_t SpearFullDamage = 22;
	inline constexpr float SpearFullChill = 2.2f;
	inline constexpr uint8_t SpearFullPierce = 2;
	inline constexpr float SpearFullSize = 2.6f;
	inline constexpr float SpearBurstRadius = 1.3f;
	inline constexpr int32_t SpearBurstDamage = 10;
	inline constexpr float SpearBurstPush = 10.0f;
	inline constexpr float SpearBurstChill = 1.0f;
	inline constexpr float FirebombSkipDistance = 2.0f;
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
	inline constexpr float RuinsScale = 2.0f;
	inline constexpr float RuinsStartRadius = 13.2f;
	inline constexpr float RuinsSpawnRank = 4.0f;
	inline constexpr float SkyFallSeconds = 0.9f;
	inline constexpr float SkyCatchSeconds = 0.25f;
	inline constexpr float GustFirst = 10.0f;
	inline constexpr float GustInterval = 16.0f;
	inline constexpr float GustJitter = 5.0f;
	inline constexpr float GustWarning = 2.0f;
	inline constexpr float GustDuration = 3.2f;
	inline constexpr float GustRamp = 0.45f;
	inline constexpr float GustSpeed = 2.2f;
	inline constexpr float IslandDwell = 0.3f;
	inline constexpr float IslandTravel = 0.2f;
	inline constexpr float PeakScale = 2.0f;
	inline constexpr float PeakStartRadius = 8.2f;
	inline constexpr float PeakSpawnRank = 4.3f;
	inline constexpr float TierHeight = 1.0f;
	inline constexpr float ClimbHeight = 0.55f;
	inline constexpr float DashClimb = 1.45f;
	inline constexpr float SpellClimb = 1.45f;
	inline constexpr float BoltFirst = 7.0f;
	inline constexpr float BoltWarning = 1.6f;
	inline constexpr float BoltLinger = 1.0f;
	inline constexpr float BoltRadius = 1.35f;
	inline constexpr float BoltIntervalEarly = 3.2f;
	inline constexpr float BoltIntervalLate = 1.3f;
	inline constexpr float BoltRampEnd = 70.0f;
	inline constexpr float BoltPush = 7.0f;
	inline constexpr float BoltStun = 0.35f;
	inline constexpr int32_t BoltDamage = 14;
	inline constexpr float RingStart = 30.0f;
	inline constexpr float RingWarning = 6.0f;
	inline constexpr float RingSettle = 66.0f;
	inline constexpr float RingClose = 78.0f;
	inline constexpr float RingClosing = 10.0f;
	inline constexpr float RingOuter = 17.5f;
	inline constexpr float RingInner = 4.4f;
	inline constexpr float RingDamageEarly = 7.0f;
	inline constexpr float RingDamageLate = 16.0f;

	inline float BrawlStartRadius(uint32_t players, BrawlShape shape = BrawlShape::Disc)
	{
		if (shape == BrawlShape::Ruins)
			return RuinsStartRadius;
		if (shape == BrawlShape::Peak)
			return PeakStartRadius;
		return std::clamp(6.5f + 1.1f * float(players), 8.5f, 12.0f);
	}

	inline BrawlShape ShapeOf(MapKind map)
	{
		switch (map)
		{
		case MapKind::FrozenLake:
			return BrawlShape::Lake;
		case MapKind::SkyRuins:
			return BrawlShape::Ruins;
		case MapKind::StormPeak:
			return BrawlShape::Peak;
		default:
			return BrawlShape::Disc;
		}
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
		static constexpr BrawlTerms Ruins{ "abyss", "stone", "The ruins are crumbling", "Sudden death, the last stones are falling", "Lost to the Sky", "fell from the ruins", "The ground is giving way! Move", "ruins", "the Sky Ruins", "Spells don't hurt, they shove, and there is no lava: whoever falls off is out. Drifting islands carry you out and back, gusts push everyone one way, and the ruins crumble from the edge inward.",
			"Spells hurt and cost mana, and they still nudge. Whoever falls off is out. Drifting islands carry you out and back, gusts push everyone one way, and the ruins crumble from the edge inward." };
		static constexpr BrawlTerms Peak{ "abyss", "rock", "The storm is closing in", "Sudden death, the storm has closed over the circle", "Lost to the Storm", "fell from the peak", "The storm is tearing at you! Get inside the circle", "peak", "the Storm Peak", "Spells don't hurt, they shove, and off the mountain you're out. Lightning falls where the ground crackles and hunts anyone who stands still. Later the storm closes in on the old ritual circle at the summit, and outside it the storm tears at you.",
			"Spells hurt and cost mana, and they still nudge. Off the mountain you're out. Lightning falls where the ground crackles and hunts anyone who stands still, and later the storm closes in on the ritual circle at the summit." };
		switch (map)
		{
		case MapKind::FrozenLake:
			return Lake;
		case MapKind::SkyRuins:
			return Ruins;
		case MapKind::StormPeak:
			return Peak;
		default:
			return Caldera;
		}
	}

	inline float ShapeScale(BrawlShape shape)
	{
		return shape == BrawlShape::Ruins ? RuinsScale : shape == BrawlShape::Peak ? PeakScale : 1.0f;
	}

	inline Vec2 ArenaPoint(const CollapsePlan& plan, float angle, float radius)
	{
		const Vec2 direction = FromAngle(angle) * radius;
		if (plan.Shape == BrawlShape::Lake)
			return plan.Center + Vec2(direction.x * LakeScale, direction.y * LakeScale / LakeAspect);
		return plan.Center + direction * ShapeScale(plan.Shape);
	}

	inline float ArenaRank(const CollapsePlan& plan, Vec2 point)
	{
		const Vec2 offset = point - plan.Center;
		if (plan.Shape == BrawlShape::Lake)
			return Length(Vec2(offset.x, offset.y * LakeAspect)) / LakeScale;
		return Length(offset) / ShapeScale(plan.Shape);
	}

	inline float SpawnRank(const CollapsePlan& plan)
	{
		return plan.Shape == BrawlShape::Ruins ? RuinsSpawnRank : plan.Shape == BrawlShape::Peak ? PeakSpawnRank : plan.StartRadius * BrawlSpawnRing;
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

	inline float FloorHeight(const Level& level, Vec2 position)
	{
		return level.Relief.IsEmpty() ? 0.0f : level.Relief.Height(position);
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
		if (plan.Shape == BrawlShape::Peak)
			return 1e6f;
		const float rank = ArenaRank(plan, seed) + (CollapseNoise(chunk.X, chunk.Y, plan.Seed) - 0.5f) * CollapseJitter;
		if (rank >= plan.StartRadius)
			return -1e6f;
		const float hole = HoleCollapseTime(plan, seed);
		if (plan.Steady)
			return hole < 0.0f ? hole : 1e6f;
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
		if (IsBrawlHazard(terrain))
			return true;
		if (IsBrawlFloor(terrain))
			return false;
		return IsBrawlHazard(level.Map.GetTerrain(int32_t(std::floor(position.x)), int32_t(std::floor(position.y))));
	}

	inline float IslandDrift(const DriftIsland& island, float clock)
	{
		const float cycle = std::max(clock, 0.0f) / island.Period + island.Phase;
		const float t = cycle - std::floor(cycle);
		const auto ease = [](float x)
		{
			x = std::clamp(x, 0.0f, 1.0f);
			return x * x * (3.0f - 2.0f * x);
		};
		return t < 0.5f ? ease((t - IslandDwell) / IslandTravel) : 1.0f - ease((t - 0.5f - IslandDwell) / IslandTravel);
	}

	inline Vec2 IslandOffset(const DriftIsland& island, float clock)
	{
		return island.Axis * (island.Reach * IslandDrift(island, clock));
	}

	inline float IslandCollapseTime(const CollapsePlan& plan, const DriftIsland& island)
	{
		const TileCoord anchor = island.Cells.empty() ? TileCoord{} : island.Cells.front();
		return PlannedCollapseTime(plan, island.Center + island.Axis * (island.Reach * 0.5f), { anchor.X, anchor.Y });
	}

	inline bool HoldsCell(const DriftIsland& island, CellCoord cell)
	{
		return std::any_of(island.Cells.begin(), island.Cells.end(), [&](TileCoord own) { return own.X == cell.X && own.Y == cell.Y; });
	}

	inline int32_t IslandAt(const std::vector<DriftIsland>& islands, const CollapsePlan& plan, float clock, Vec2 position)
	{
		for (size_t i = 0; i < islands.size(); i++)
		{
			const DriftIsland& island = islands[i];
			const Vec2 local = position - IslandOffset(island, clock);
			if (LengthSquared(local - island.Center) > 64.0f || IslandCollapseTime(plan, island) <= clock)
				continue;
			if (HoldsCell(island, CellAt(local)))
				return int32_t(i);
		}
		return -1;
	}

	inline int32_t IslandAt(const Level& level, const CollapsePlan& plan, float clock, Vec2 position)
	{
		return IslandAt(level.Islands, plan, clock, position);
	}

	inline bool IsBrawlLava(const Level& level, const CollapsePlan& plan, float clock, Vec2 position)
	{
		if (!level.Relief.IsEmpty())
			return level.Relief.RimDistance(position) < 0.0f;
		if (!level.Islands.empty() && IslandAt(level, plan, clock, position) >= 0)
			return false;
		const CellCoord cell = CellAt(position);
		const TerrainId terrain = level.Map.GetTerrain(cell.X, cell.Y);
		if (IsBrawlHazard(terrain))
			return true;
		if (IsBrawlFloor(terrain))
			return TileCollapseTime(plan, cell.X, cell.Y) <= clock;
		return level.Skyborne || IsBrawlHazard(level.Map.GetTerrain(int32_t(std::floor(position.x)), int32_t(std::floor(position.y))));
	}

	inline bool IsBrawlLava(const Level& level, const BrawlStatus& brawl, Vec2 position)
	{
		return IsBrawlLava(level, brawl.Plan, brawl.Clock, position);
	}

	inline float GroundLeft(const Level& level, const CollapsePlan& plan, float clock, Vec2 position)
	{
		if (!level.Relief.IsEmpty())
			return level.Relief.RimDistance(position) < 0.0f ? 0.0f : 1e6f;
		if (const int32_t island = IslandAt(level, plan, clock, position); island >= 0)
			return IslandCollapseTime(plan, level.Islands[size_t(island)]) - clock;
		const CellCoord cell = CellAt(position);
		if (!IsBrawlFloor(level.Map.GetTerrain(cell.X, cell.Y)))
			return 0.0f;
		return TileCollapseTime(plan, cell.X, cell.Y) - clock;
	}

	inline bool PropFallen(const Level& level, const CollapsePlan& plan, float clock, const PlacedProp& prop)
	{
		const CellCoord cell = CellAt(prop.Position);
		return level.Skyborne && TileCollapseTime(plan, cell.X, cell.Y) <= clock;
	}

	inline void SinkFallenProps(Level& level, const CollapsePlan& plan, float clock)
	{
		if (!level.Skyborne)
			return;
		level.Sunk.resize(level.Shapes.size(), 0);
		uint32_t prop = UINT32_MAX;
		uint8_t fallen = 0;
		for (size_t i = 0; i < level.ShapeProps.size(); i++)
		{
			if (level.ShapeProps[i] != prop)
			{
				prop = level.ShapeProps[i];
				fallen = PropFallen(level, plan, clock, level.Props[prop]) ? 1 : 0;
			}
			level.Sunk[i] = fallen;
		}
	}

	struct Gust
	{
		Vec2 Velocity;
		Vec2 Direction;
		float Warning = 0.0f;
		float Strength = 0.0f;
	};

	inline Gust GustAt(const CollapsePlan& plan, float clock)
	{
		Gust gust;
		if (plan.Shape != BrawlShape::Ruins || clock < 0.0f)
			return gust;
		const int32_t nearest = int32_t(std::floor((clock - GustFirst) / GustInterval));
		for (int32_t k = std::max(nearest - 1, 0); k <= nearest + 1; k++)
		{
			const float since = clock - (GustFirst + float(k) * GustInterval + (CollapseNoise(k, 0x6057, plan.Seed) - 0.5f) * GustJitter);
			if (since < -GustWarning || since > GustDuration)
				continue;
			gust.Direction = FromAngle(2.0f * Pi * CollapseNoise(k, 0x61AD, plan.Seed));
			gust.Warning = std::min(1.0f + since / GustWarning, 1.0f);
			if (since >= 0.0f)
			{
				gust.Strength = std::clamp(std::min(since, GustDuration - since) / GustRamp, 0.0f, 1.0f);
				gust.Velocity = gust.Direction * (GustSpeed * gust.Strength);
			}
			return gust;
		}
		return gust;
	}

	struct StormRing
	{
		Vec2 Center;
		float Radius = 1e9f;
		float Damage = 0.0f;
		bool Active = false;

		bool Outside(Vec2 position) const { return Active && LengthSquared(position - Center) > Radius * Radius; }
	};

	inline StormRing RingAt(const CollapsePlan& plan, float clock)
	{
		StormRing ring;
		ring.Center = plan.Center;
		if (plan.Shape != BrawlShape::Peak || plan.Steady || clock < RingStart)
			return ring;
		ring.Active = true;
		ring.Radius = RingOuter + (RingInner - RingOuter) * std::clamp((clock - RingStart) / (RingSettle - RingStart), 0.0f, 1.0f);
		if (clock > RingClose)
			ring.Radius *= std::max(1.0f - (clock - RingClose) / RingClosing, 0.0f);
		ring.Damage = RingDamageEarly + (RingDamageLate - RingDamageEarly) * std::clamp((clock - RingStart) / (RingClose + RingClosing - RingStart), 0.0f, 1.0f);
		return ring;
	}

	inline float StormBuild(const CollapsePlan& plan, float clock)
	{
		return plan.Shape == BrawlShape::Peak ? std::clamp(0.25f + 0.75f * clock / RingSettle, 0.25f, 1.0f) : 0.0f;
	}

	inline float BoltInterval(float clock)
	{
		return BoltIntervalEarly + (BoltIntervalLate - BoltIntervalEarly) * std::clamp((clock - BoltFirst) / (BoltRampEnd - BoltFirst), 0.0f, 1.0f);
	}

	inline std::span<const LightningBolt> BoltsOf(const CollapsePlan& plan)
	{
		return { plan.Bolts.data(), std::min<size_t>(plan.BoltCount, MaxBolts) };
	}

	inline const LightningBolt* BoltStriking(const CollapsePlan& plan, Vec2 position, float from, float to)
	{
		for (const LightningBolt& bolt : BoltsOf(plan))
			if (bolt.Time > from && bolt.Time <= to && LengthSquared(position - bolt.Center) <= BoltRadius * BoltRadius)
				return &bolt;
		return nullptr;
	}

	inline std::optional<float> BoltThreat(const CollapsePlan& plan, Vec2 position, float clock, float margin = 0.0f)
	{
		std::optional<float> first;
		for (const LightningBolt& bolt : BoltsOf(plan))
			if (bolt.Time > clock && LengthSquared(position - bolt.Center) <= (BoltRadius + margin) * (BoltRadius + margin) && (!first || bolt.Time < *first))
				first = bolt.Time;
		return first;
	}

	inline float SurfaceFriction(const Level& level, Vec2 position)
	{
		const CellCoord cell = CellAt(position);
		return level.Map.GetTerrain(cell.X, cell.Y) == TerrainId::Ice ? IceFriction : 1.0f;
	}

	struct ChargedShot
	{
		float Speed = 0.0f;
		float Range = 0.0f;
		float Radius = 0.0f;
		float Push = 0.0f;
		float DamageScale = 1.0f;
		float Chill = 0.0f;
		uint8_t Pierce = 0;
		float Size = 1.0f;
	};

	inline ChargedShot SpearShot(float power)
	{
		const AbilityDef& spear = GetAbility(AbilityId::FrostSpear);
		const float p = std::clamp(power, 0.0f, 1.0f);
		const auto mix = [p](float tap, float full) { return tap + (full - tap) * p; };
		return {
			.Speed = mix(spear.ProjectileSpeed, SpearFullSpeed),
			.Range = mix(spear.ProjectileRange, SpearFullRange),
			.Radius = mix(spear.ProjectileRadius, SpearFullRadius),
			.Push = mix(spear.Push, SpearFullPush),
			.DamageScale = mix(float(spear.BrawlDamage), float(SpearFullDamage)) / float(std::max(spear.BrawlDamage, 1)),
			.Chill = mix(spear.Chill, SpearFullChill),
			.Pierce = p >= 0.999f ? SpearFullPierce : uint8_t(0),
			.Size = mix(1.0f, SpearFullSize),
		};
	}

	inline ProjectilePattern ChargedPattern(const AbilityDef& ability, ProjectilePattern pattern, float charge, float speedScale)
	{
		if (ability.Id != AbilityId::FrostSpear)
			return pattern;
		const ChargedShot shot = SpearShot(charge);
		pattern.Speed = shot.Speed * speedScale;
		pattern.Pierce = uint8_t(std::min(pattern.Pierce + shot.Pierce, 8));
		return pattern;
	}

	inline float ChargedRange(const AbilityDef& ability, float charge)
	{
		return ability.Id == AbilityId::FrostSpear ? SpearShot(charge).Range : ability.ProjectileRange;
	}

	inline SkillTraits MakeBrawlSkill(AbilityId id, float cooldownScale, BrawlMode mode)
	{
		const AbilityDef& def = GetAbility(id);
		SkillTraits skill;
		skill.Ability = id;
		skill.Usable = true;
		skill.CastTime = def.CastTime;
		skill.ChargeTime = def.ChargeTime;
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
				skill.ChargeTime /= ApexMagicSpeed;
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

	inline PlayerTraits MakeBrawlTraits(const BrawlLoadout& loadout, const MatchRules& rules)
	{
		const BrawlMode mode = rules.Mode;
		PlayerTraits traits;
		traits.MoveSpeed = BrawlMoveSpeed * float(rules.MoveSpeed) / 100.0f * (HasCard(loadout.Cards, CardId::HeavyRobes) ? HeavyRobesSpeed : 1.0f);
		traits.MaxHealth = rules.Health;
		traits.MaxMana = mode == BrawlMode::Damage ? float(rules.Mana) + (HasCard(loadout.Cards, CardId::DeepWell) ? DeepWellMana : 0.0f) : 0.0f;
		traits.ManaRegen = mode == BrawlMode::Damage ? BrawlManaRegen * float(rules.ManaRegen) / 100.0f * (HasCard(loadout.Cards, CardId::Quickening) ? QuickeningRegen : 1.0f) : 0.0f;
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
		for (SkillTraits& skill : traits.Skills)
		{
			skill.CastTime /= float(rules.CastSpeed) / 100.0f;
			skill.ChargeTime /= float(rules.CastSpeed) / 100.0f;
			skill.Cooldown /= float(rules.CooldownSpeed) / 100.0f;
		}
		traits.Dodge = MakeBrawlSkill(AbilityId::Roll, 1.0f, mode);
		traits.Dodge.Cooldown = BrawlDodgeCooldown;
		if (mode == BrawlMode::Damage)
			ApplyApex(traits, loadout.Cards);
		return traits;
	}

	inline PlayerTraits MakeBrawlTraits(const BrawlLoadout& loadout, BrawlMode mode)
	{
		MatchRules rules;
		rules.Mode = mode;
		return MakeBrawlTraits(loadout, rules);
	}

	inline bool NeedsChosenSpell(const CardDef& card)
	{
		return (card.Kind == CardKind::Upgrade && card.Spell != BrawlBasic) || card.Needs != AbilityId::Count;
	}

	inline BrawlSpells DealSpells(Random& random)
	{
		std::array<AbilityId, std::size(BrawlSpellPool)> pool{};
		std::copy(std::begin(BrawlSpellPool), std::end(BrawlSpellPool), pool.begin());
		BrawlSpells spells{};
		for (uint32_t slot = 0; slot < SkillSlots; slot++)
		{
			const uint32_t pick = slot + random.Below(uint32_t(pool.size()) - slot);
			std::swap(pool[slot], pool[pick]);
			spells[slot] = pool[slot];
		}
		return spells;
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
