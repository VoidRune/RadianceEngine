#include "BotBrain.h"
#include "GameShared/Simulation/PlayerSimulation.h"
#include "GameShared/Simulation/World.h"
#include "GameShared/World/Collision.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <unordered_map>

namespace Game
{
	namespace
	{
		constexpr float GridStep = 0.5f;
		constexpr float NeverLava = 1e9f;
		constexpr float AlwaysLava = -1e9f;
		constexpr int32_t LavaSample = -2;
		constexpr int32_t SolidSample = -1;
		constexpr float FieldInterval = 0.1f;
		constexpr float FieldLimit = 12.0f;
		constexpr float LandingDrag = 0.92f;
		constexpr float ChainReach = 5.5f;
		constexpr uint32_t VelocityTicks = 6;
		constexpr float EscapeClearance = 1.1f;
		constexpr float WanderSeconds = 3.0f;
		constexpr float ComboOffset = 1.0f;
		constexpr float ComboLate = 0.3f;
		constexpr float HarmScale = 30.0f;
		constexpr float FinishBonus = 0.35f;
		constexpr float ManaPrice = 0.2f;
		constexpr float WaryHealth = 0.35f;
		constexpr int32_t DodgeDamage = 10;
		constexpr float RollClearance = 0.3f;
		constexpr float RollTurns[] = { 0.4f, -0.4f, 0.8f, -0.8f };
		constexpr float BlinkReach = 3.5f;
		constexpr float BloodHealth = 0.7f;
		constexpr float BloodPrice = 0.15f;
		constexpr float BloodMagicFloor = 0.25f;
		constexpr float PlantReach = 7.5f;
		constexpr float PlantNear = 2.5f;
		constexpr float PlantCrowd = 6.0f;
		constexpr float PlantHealth = 0.25f;
		constexpr uint32_t PlantCrowdMax = 2;
		constexpr float PlantHorizon = 0.6f;
		constexpr float PlantClearance = 1.5f;
		constexpr float PlantCollapse = 3.0f;
		constexpr float CycleReach = 7.0f;
		constexpr float CycleScore = 0.05f;
		constexpr float IslandSpan = 5.0f;
		constexpr float GustLook = 1.2f;
		constexpr float FlightSeconds = 0.4f;
		constexpr float StrikeHorizon = BoltWarning + 0.2f;
		constexpr float StrikeLead = 0.4f;
		constexpr float StrikeMargin = 0.45f;
		constexpr float RingLook = 2.0f;
		constexpr float RingMargin = 1.2f;
		constexpr float RouteSeconds = 0.4f;
		constexpr float RouteStep = 0.4f;

		bool Climbable(const Level& level, Vec2 from, Vec2 to)
		{
			if (level.Relief.IsEmpty())
				return true;
			float floor = FloorHeight(level, from);
			const int32_t steps = std::max(1, int32_t(std::ceil(Length(to - from) / RouteStep)));
			for (int32_t i = 1; i <= steps; i++)
			{
				const float height = FloorHeight(level, Lerp(from, to, float(i) / float(steps)));
				if (height - floor > ClimbHeight)
					return false;
				floor = height;
			}
			return true;
		}

		struct SkillParams
		{
			float Reaction = 0.24f;
			float AimNoise = 0.06f;
			float ThinkInterval = 0.12f;
			float DodgeChance = 0.65f;
			float Eagerness = 1.0f;
			float Recovery = 0.7f;
			float Caution = 1.0f;
			float Combo = 0.5f;
			float Reserve = 1.0f;
		};

		constexpr SkillParams SkillTable[] = {
			{ 0.38f, 0.13f, 0.2f, 0.3f, 0.8f, 0.35f, 0.5f, 0.0f, 0.0f },
			{ 0.24f, 0.065f, 0.12f, 0.65f, 1.0f, 0.7f, 0.8f, 0.5f, 0.5f },
			{ 0.12f, 0.025f, 0.08f, 0.95f, 1.0f, 0.95f, 1.0f, 0.9f, 1.0f },
		};
		static_assert(std::size(SkillTable) == size_t(BotSkill::Count));

		const SkillParams& ParamsOf(BotSkill skill)
		{
			return SkillTable[size_t(skill) < std::size(SkillTable) ? size_t(skill) : 1];
		}

		constexpr BrawlSpells BalancedSets[] = {
			{ AbilityId::Gust, AbilityId::Warp, AbilityId::Comet },
			{ AbilityId::FrostSpear, AbilityId::Gust, AbilityId::Warp },
			{ AbilityId::ArcLightning, AbilityId::Comet, AbilityId::MirrorWard },
			{ AbilityId::Shockwave, AbilityId::Swap, AbilityId::Warp },
			{ AbilityId::Firebomb, AbilityId::StormStrike, AbilityId::Warp },
			{ AbilityId::SplittingBolt, AbilityId::Gust, AbilityId::MirrorWard },
		};
		constexpr BrawlSpells BruiserSets[] = {
			{ AbilityId::Gust, AbilityId::Shockwave, AbilityId::Warp },
			{ AbilityId::Gust, AbilityId::Shockwave, AbilityId::MirrorWard },
			{ AbilityId::Gust, AbilityId::Swap, AbilityId::Warp },
			{ AbilityId::ChainHook, AbilityId::Gust, AbilityId::Shockwave },
			{ AbilityId::ChainHook, AbilityId::Firebomb, AbilityId::Warp },
		};
		constexpr BrawlSpells SniperSets[] = {
			{ AbilityId::FrostSpear, AbilityId::Comet, AbilityId::Warp },
			{ AbilityId::ArcLightning, AbilityId::Comet, AbilityId::Warp },
			{ AbilityId::FrostSpear, AbilityId::ArcLightning, AbilityId::MirrorWard },
			{ AbilityId::StormStrike, AbilityId::SplittingBolt, AbilityId::Warp },
			{ AbilityId::Firebomb, AbilityId::ArcLightning, AbilityId::Warp },
		};
		constexpr BrawlSpells TricksterSets[] = {
			{ AbilityId::Swap, AbilityId::GravityWell, AbilityId::Warp },
			{ AbilityId::GravityWell, AbilityId::Comet, AbilityId::MirrorWard },
			{ AbilityId::Swap, AbilityId::FrostSpear, AbilityId::GravityWell },
			{ AbilityId::ChainHook, AbilityId::Swap, AbilityId::Warp },
			{ AbilityId::SplittingBolt, AbilityId::GravityWell, AbilityId::Firebomb },
		};

		constexpr const char* BotNames[] = {
			"Ashwyn", "Brimble", "Cinder", "Dusk", "Fenwick", "Gale", "Hollis", "Ivy", "Jinx", "Kestrel", "Lumen", "Morrow",
			"Nyx", "Oriel", "Pyra", "Quill", "Rook", "Sable", "Thistle", "Umber", "Vesper", "Wren", "Yarrow", "Zephyr",
		};

		float Threshold(AbilityId spell)
		{
			switch (spell)
			{
			case AbilityId::EmberBolt:
				return 0.05f;
			case AbilityId::FrostSpear:
				return 0.09f;
			case AbilityId::ArcLightning:
				return 0.1f;
			case AbilityId::Gust:
			case AbilityId::Shockwave:
				return 0.16f;
			case AbilityId::Comet:
				return 0.13f;
			case AbilityId::Swap:
				return 0.2f;
			case AbilityId::GravityWell:
				return 0.22f;
			case AbilityId::ChainHook:
				return 0.18f;
			case AbilityId::Firebomb:
				return 0.14f;
			case AbilityId::SplittingBolt:
				return 0.1f;
			case AbilityId::StormStrike:
				return 0.12f;
			default:
				return 1e9f;
			}
		}

		float StyleWeight(BotStyle style, AbilityId spell)
		{
			switch (style)
			{
			case BotStyle::Bruiser:
				return spell == AbilityId::Gust || spell == AbilityId::Shockwave || spell == AbilityId::ChainHook ? 1.35f : spell == AbilityId::EmberBolt ? 0.9f : 1.0f;
			case BotStyle::Sniper:
				return spell == AbilityId::FrostSpear || spell == AbilityId::ArcLightning || spell == AbilityId::Comet || spell == AbilityId::StormStrike || spell == AbilityId::SplittingBolt ? 1.2f : spell == AbilityId::EmberBolt ? 1.1f : spell == AbilityId::Gust ? 0.85f : 1.0f;
			case BotStyle::Trickster:
				return spell == AbilityId::Swap || spell == AbilityId::GravityWell || spell == AbilityId::ChainHook ? 1.4f : spell == AbilityId::Firebomb ? 1.15f : 1.0f;
			default:
				return 1.0f;
			}
		}

		void Chamfer(std::vector<float>& field, int32_t width, int32_t height)
		{
			constexpr float Diagonal = 1.41421356f;
			const auto at = [&](int32_t x, int32_t y) -> float& { return field[size_t(y) * size_t(width) + size_t(x)]; };
			for (int32_t y = 0; y < height; y++)
				for (int32_t x = 0; x < width; x++)
				{
					float& value = at(x, y);
					if (x > 0)
						value = std::min(value, at(x - 1, y) + 1.0f);
					if (y > 0)
					{
						value = std::min(value, at(x, y - 1) + 1.0f);
						if (x > 0)
							value = std::min(value, at(x - 1, y - 1) + Diagonal);
						if (x + 1 < width)
							value = std::min(value, at(x + 1, y - 1) + Diagonal);
					}
				}
			for (int32_t y = height - 1; y >= 0; y--)
				for (int32_t x = width - 1; x >= 0; x--)
				{
					float& value = at(x, y);
					if (x + 1 < width)
						value = std::min(value, at(x + 1, y) + 1.0f);
					if (y + 1 < height)
					{
						value = std::min(value, at(x, y + 1) + 1.0f);
						if (x + 1 < width)
							value = std::min(value, at(x + 1, y + 1) + Diagonal);
						if (x > 0)
							value = std::min(value, at(x - 1, y + 1) + Diagonal);
					}
				}
		}

		float Mobility(const PlayerController& controller)
		{
			const PlayerState& state = controller.State;
			if (state.IsStunned() || state.IsFalling())
				return 0.1f;
			float mobility = 1.0f;
			if (state.CastSlot >= 0)
				mobility *= std::max(controller.Traits.Skills[size_t(state.CastSlot)].MoveScale, 0.15f);
			if (state.Chill > 0.0f)
				mobility *= 0.75f;
			if (LengthSquared(state.Push) > PushControlSpeed * PushControlSpeed * 0.45f)
				mobility *= 0.35f;
			return std::clamp(mobility, 0.05f, 1.0f);
		}
	}

	BrawlSpells PickBotSpells(BotStyle style, Random& random, bool uniform)
	{
		BrawlSpells spells = DefaultBrawlSpells;
		if (uniform)
		{
			std::array<AbilityId, std::size(BrawlSpellPool)> pool{};
			std::copy(std::begin(BrawlSpellPool), std::end(BrawlSpellPool), pool.begin());
			for (size_t i = pool.size(); i > 1; i--)
				std::swap(pool[i - 1], pool[random.Below(uint32_t(i))]);
			std::copy(pool.begin(), pool.begin() + SkillSlots, spells.begin());
			return spells;
		}
		switch (style)
		{
		case BotStyle::Bruiser:
			spells = BruiserSets[random.Below(uint32_t(std::size(BruiserSets)))];
			break;
		case BotStyle::Sniper:
			spells = SniperSets[random.Below(uint32_t(std::size(SniperSets)))];
			break;
		case BotStyle::Trickster:
			spells = TricksterSets[random.Below(uint32_t(std::size(TricksterSets)))];
			break;
		default:
			spells = BalancedSets[random.Below(uint32_t(std::size(BalancedSets)))];
			break;
		}
		if (random.Float() < 0.25f)
		{
			const AbilityId swap = BrawlSpellPool[random.Below(uint32_t(std::size(BrawlSpellPool)))];
			if (std::find(spells.begin(), spells.end(), swap) == spells.end())
				spells[random.Below(SkillSlots)] = swap;
		}
		return spells;
	}

	uint32_t PickBotCard(const BrawlLoadout& loadout, BotStyle style, Random& random, BrawlMode mode)
	{
		const TagMask owned = mode == BrawlMode::Damage ? CardTags(loadout.Cards) : TagMask(0);
		const TagMask liked = mode != BrawlMode::Damage ? TagMask(0) : style == BotStyle::Bruiser ? TagBlood | TagFire : style == BotStyle::Sniper ? TagStorm | TagFrost : style == BotStyle::Trickster ? TagMana | TagGuard : TagMask(0);
		uint32_t best = 0;
		float bestScore = -1e9f;
		for (uint32_t i = 0; i < CardChoices; i++)
		{
			const CardId id = loadout.Offer[i];
			if (id == CardId::Count)
				continue;
			const CardDef& card = GetCard(id);
			float score = card.Rarity == CardRarity::Apex ? 3.0f : card.Rarity == CardRarity::Legendary ? 0.45f : card.Rarity == CardRarity::Rare ? 0.25f : 0.0f;
			if (card.Kind == CardKind::Upgrade)
				score += 0.6f + (card.Spell == BrawlBasic ? 0.25f : 0.0f);
			else
				switch (id)
				{
				case CardId::GlassCannon:
					score += style == BotStyle::Bruiser ? 0.5f : 0.1f;
					break;
				case CardId::Featherfall:
					score += 0.55f;
					break;
				case CardId::HeavyRobes:
					score += style == BotStyle::Sniper ? 0.4f : 0.2f;
					break;
				case CardId::Wildfire:
				case CardId::Overload:
				case CardId::Shatter:
				case CardId::Conductive:
					score += 0.55f;
					break;
				case CardId::DeepWell:
				case CardId::Quickening:
				case CardId::Siphon:
				case CardId::Brimming:
				case CardId::Kindling:
				case CardId::Brittle:
				case CardId::Barrier:
				case CardId::Precise:
				case CardId::VampiricEmbers:
					score += 0.4f;
					break;
				case CardId::BloodPact:
				case CardId::LastStand:
				case CardId::SecondWind:
					score += 0.3f;
					break;
				default:
					score += 0.2f;
					break;
				}
			score += (card.Tags & owned) != 0 ? 0.25f : 0.0f;
			score += (card.Tags & liked) != 0 ? 0.15f : 0.0f;
			score += random.Float() * 0.5f;
			if (score > bestScore)
			{
				bestScore = score;
				best = i;
			}
		}
		return best;
	}

	const char* PickBotName(uint32_t index)
	{
		return BotNames[index % std::size(BotNames)];
	}

	void BotArena::Update(World& world)
	{
		const TileMap& map = world.GetMap();
		const int32_t width = int32_t(std::ceil(float(map.GetWidth()) / GridStep));
		const int32_t height = int32_t(std::ceil(float(map.GetHeight()) / GridStep));
		if (m_SampleCell.empty() || m_LevelSeed != world.GetSettings().Seed || m_LevelKind != world.GetLevel().Kind || width != m_Width || height != m_Height)
		{
			m_Width = width;
			m_Height = height;
			m_LevelSeed = world.GetSettings().Seed;
			m_LevelKind = world.GetLevel().Kind;
			BuildCells(world);
			m_PlanSeed = 0;
		}
		const BrawlStatus& brawl = world.GetMatch().GetBrawl();
		if (brawl.Plan.Seed != m_PlanSeed || brawl.Plan.Center != m_PlanCenter || brawl.Plan.StartRadius != m_PlanRadius || brawl.Plan.CraterCount != m_Craters)
		{
			m_PlanSeed = brawl.Plan.Seed;
			m_PlanCenter = brawl.Plan.Center;
			m_PlanRadius = brawl.Plan.StartRadius;
			m_Craters = brawl.Plan.CraterCount;
			PlanCollapse(world);
			m_FieldClock = -1e9f;
		}
		m_Clock = brawl.Phase == BrawlPhase::Fight || brawl.Phase == BrawlPhase::Over ? std::max(brawl.Clock, 0.0f) : 0.0f;
		if (std::abs(m_Clock - m_FieldClock) >= FieldInterval)
		{
			RefreshClearance(world);
			m_FieldClock = m_Clock;
		}
	}

	void BotArena::BuildCells(const World& world)
	{
		const TileMap& map = world.GetMap();
		const Elevation& relief = world.GetLevel().Relief;
		m_SampleCell.assign(size_t(m_Width) * size_t(m_Height), SolidSample);
		m_Floor.assign(relief.IsEmpty() ? size_t(0) : m_SampleCell.size(), 0.0f);
		m_Slide.assign(m_SampleCell.size(), 1.0f);
		for (int32_t y = 0; y < m_Height; y++)
			for (int32_t x = 0; x < m_Width; x++)
			{
				const Vec2 sample((float(x) + 0.5f) * GridStep, (float(y) + 0.5f) * GridStep);
				const CellCoord cell = CellAt(sample);
				int32_t& code = m_SampleCell[size_t(Index(x, y))];
				const TerrainId terrain = map.GetTerrain(cell.X, cell.Y);
				if (!relief.IsEmpty())
				{
					const int32_t tileX = std::clamp(int32_t(sample.x), 0, map.GetWidth() - 1);
					const int32_t tileY = std::clamp(int32_t(sample.y), 0, map.GetHeight() - 1);
					code = relief.RimDistance(sample) < 0.0f ? LavaSample : !map.IsWalkable(tileX, tileY) ? SolidSample : tileY * map.GetWidth() + tileX;
					m_Floor[size_t(Index(x, y))] = relief.Height(sample);
				}
				else if (IsBrawlHazard(terrain))
					code = LavaSample;
				else if (IsBrawlFloor(terrain))
					code = cell.Y * map.GetWidth() + cell.X;
				else
					code = world.GetLevel().Skyborne || IsBrawlHazard(map.GetTerrain(int32_t(std::floor(sample.x)), int32_t(std::floor(sample.y)))) ? LavaSample : SolidSample;
				m_Slide[size_t(Index(x, y))] = 1.0f / SurfaceFriction(world.GetLevel(), sample);
			}
		m_Collapse.assign(m_SampleCell.size(), NeverLava);
		m_Clearance.assign(m_SampleCell.size(), FieldLimit);
	}

	void BotArena::PlanCollapse(const World& world)
	{
		const TileMap& map = world.GetMap();
		const CollapsePlan& plan = world.GetMatch().GetBrawl().Plan;
		m_CellTime.assign(size_t(map.GetWidth()) * size_t(map.GetHeight()), NeverLava);
		for (int32_t y = 0; y < map.GetHeight(); y++)
			for (int32_t x = 0; x < map.GetWidth(); x++)
				if (IsBrawlFloor(map.GetTerrain(x, y)))
					m_CellTime[size_t(y) * size_t(map.GetWidth()) + size_t(x)] = TileCollapseTime(plan, x, y);
		for (size_t i = 0; i < m_SampleCell.size(); i++)
		{
			const int32_t code = m_SampleCell[i];
			m_Collapse[i] = code >= 0 ? m_CellTime[size_t(code)] : code == LavaSample ? AlwaysLava : NeverLava;
		}
	}

	void BotArena::RefreshClearance(const World& world)
	{
		const float horizon = m_Clock + CollapseWarning;
		m_Cover.assign(m_Collapse.size(), 0);
		const CollapsePlan& plan = world.GetMatch().GetBrawl().Plan;
		for (const DriftIsland& island : world.GetLevel().Islands)
		{
			if (IslandCollapseTime(plan, island) <= horizon)
				continue;
			const Vec2 offset = IslandOffset(island, m_Clock);
			const Vec2 middle = island.Center + offset;
			const int32_t minX = std::max(int32_t((middle.x - IslandSpan) / GridStep), 0);
			const int32_t maxX = std::min(int32_t((middle.x + IslandSpan) / GridStep), m_Width - 1);
			const int32_t minY = std::max(int32_t((middle.y - IslandSpan) / GridStep), 0);
			const int32_t maxY = std::min(int32_t((middle.y + IslandSpan) / GridStep), m_Height - 1);
			for (int32_t y = minY; y <= maxY; y++)
				for (int32_t x = minX; x <= maxX; x++)
					if (HoldsCell(island, CellAt(Vec2((float(x) + 0.5f) * GridStep, (float(y) + 0.5f) * GridStep) - offset)))
						m_Cover[size_t(Index(x, y))] = 1;
		}
		m_Strike.assign(m_Collapse.size(), NeverLava);
		m_Storm.assign(m_Collapse.size(), 0);
		if (plan.Shape == BrawlShape::Peak && world.GetMatch().GetBrawl().Phase == BrawlPhase::Fight)
		{
			for (const LightningBolt& bolt : BoltsOf(plan))
			{
				if (bolt.Time <= m_Clock || bolt.Time > m_Clock + StrikeHorizon)
					continue;
				const float reach = BoltRadius + StrikeMargin;
				for (int32_t y = std::max(int32_t((bolt.Center.y - reach) / GridStep), 0); y <= std::min(int32_t((bolt.Center.y + reach) / GridStep), m_Height - 1); y++)
					for (int32_t x = std::max(int32_t((bolt.Center.x - reach) / GridStep), 0); x <= std::min(int32_t((bolt.Center.x + reach) / GridStep), m_Width - 1); x++)
						if (const size_t i = size_t(Index(x, y)); LengthSquared(Vec2((float(x) + 0.5f) * GridStep, (float(y) + 0.5f) * GridStep) - bolt.Center) <= reach * reach)
							m_Strike[i] = std::min(m_Strike[i], bolt.Time);
			}
			if (const StormRing ring = RingAt(plan, m_Clock + RingLook); ring.Active)
				for (int32_t y = 0; y < m_Height; y++)
					for (int32_t x = 0; x < m_Width; x++)
						m_Storm[size_t(Index(x, y))] = ring.Outside(Vec2((float(x) + 0.5f) * GridStep, (float(y) + 0.5f) * GridStep)) ? 1 : 0;
		}
		m_ToDanger.resize(m_Collapse.size());
		m_ToSafety.resize(m_Collapse.size());
		for (size_t i = 0; i < m_Collapse.size(); i++)
		{
			const bool danger = (m_Collapse[i] <= horizon && m_Cover[i] == 0) || m_Strike[i] < NeverLava || m_Storm[i] != 0;
			m_ToDanger[i] = danger ? 0.0f : FieldLimit / GridStep;
			m_ToSafety[i] = danger ? FieldLimit / GridStep : 0.0f;
		}
		Chamfer(m_ToDanger, m_Width, m_Height);
		Chamfer(m_ToSafety, m_Width, m_Height);
		for (size_t i = 0; i < m_Collapse.size(); i++)
			m_Clearance[i] = m_ToDanger[i] > 0.0f ? (m_ToDanger[i] - 0.5f) * GridStep : -(m_ToSafety[i] - 0.5f) * GridStep;
	}

	float BotArena::Clearance(Vec2 position) const
	{
		if (m_Clearance.empty())
			return FieldLimit;
		const float fx = position.x / GridStep - 0.5f;
		const float fy = position.y / GridStep - 0.5f;
		const int32_t x = int32_t(std::floor(fx));
		const int32_t y = int32_t(std::floor(fy));
		if (x < 0 || y < 0 || x + 1 >= m_Width || y + 1 >= m_Height)
			return -FieldLimit;
		const float u = fx - float(x);
		const float v = fy - float(y);
		const float bottom = std::lerp(m_Clearance[size_t(Index(x, y))], m_Clearance[size_t(Index(x + 1, y))], u);
		const float top = std::lerp(m_Clearance[size_t(Index(x, y + 1))], m_Clearance[size_t(Index(x + 1, y + 1))], u);
		return std::lerp(bottom, top, v);
	}

	float BotArena::Slide(Vec2 position) const
	{
		const int32_t x = int32_t(std::floor(position.x / GridStep));
		const int32_t y = int32_t(std::floor(position.y / GridStep));
		if (m_Slide.empty() || x < 0 || y < 0 || x >= m_Width || y >= m_Height)
			return 1.0f;
		return m_Slide[size_t(Index(x, y))];
	}

	std::optional<Vec2> BotArena::Route(Vec2 from, Vec2 to) const
	{
		if (m_Floor.empty())
			return std::nullopt;
		const auto node = [&](Vec2 point) { return point.x >= 0.0f && point.y >= 0.0f && point.x < float(m_Width) * GridStep && point.y < float(m_Height) * GridStep ? Index(int32_t(point.x / GridStep), int32_t(point.y / GridStep)) : -1; };
		const int32_t start = node(from);
		const int32_t goal = node(to);
		if (start < 0 || goal < 0 || m_SampleCell[size_t(goal)] < 0)
			return std::nullopt;
		std::vector<int32_t> parent(m_SampleCell.size(), -1);
		std::vector<int32_t> queue;
		queue.reserve(2048);
		parent[size_t(start)] = start;
		queue.push_back(start);
		for (size_t head = 0; head < queue.size() && parent[size_t(goal)] < 0; head++)
		{
			const int32_t current = queue[head];
			const int32_t cx = current % m_Width;
			const int32_t cy = current / m_Width;
			const float floor = m_Floor[size_t(current)];
			for (int32_t dy = -1; dy <= 1; dy++)
				for (int32_t dx = -1; dx <= 1; dx++)
				{
					const int32_t nx = cx + dx;
					const int32_t ny = cy + dy;
					if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 || nx >= m_Width || ny >= m_Height)
						continue;
					const int32_t next = Index(nx, ny);
					if (parent[size_t(next)] >= 0 || m_SampleCell[size_t(next)] < 0 || m_Floor[size_t(next)] - floor > ClimbHeight)
						continue;
					if (dx != 0 && dy != 0 && (m_SampleCell[size_t(Index(cx, ny))] < 0 || m_SampleCell[size_t(Index(nx, cy))] < 0))
						continue;
					parent[size_t(next)] = current;
					queue.push_back(next);
				}
		}
		if (parent[size_t(goal)] < 0)
			return std::nullopt;
		std::vector<int32_t> path;
		for (int32_t at = goal; at != start; at = parent[size_t(at)])
			path.push_back(at);
		if (path.empty())
			return to;
		const int32_t step = path[path.size() - std::min<size_t>(path.size(), 3)];
		return Vec2((float(step % m_Width) + 0.5f) * GridStep, (float(step / m_Width) + 0.5f) * GridStep);
	}

	float BotArena::CollapseIn(Vec2 position) const
	{
		const int32_t x = int32_t(std::floor(position.x / GridStep));
		const int32_t y = int32_t(std::floor(position.y / GridStep));
		if (m_Collapse.empty() || x < 0 || y < 0 || x >= m_Width || y >= m_Height)
			return AlwaysLava;
		const float strike = size_t(Index(x, y)) < m_Strike.size() && m_Strike[size_t(Index(x, y))] < NeverLava ? m_Strike[size_t(Index(x, y))] - m_Clock - StrikeLead : NeverLava;
		if (size_t(Index(x, y)) < m_Cover.size() && m_Cover[size_t(Index(x, y))] != 0)
			return strike;
		return std::min(m_Collapse[size_t(Index(x, y))] - m_Clock, strike);
	}

	BotBrain::BotBrain(uint32_t playerId, const BotProfile& profile)
		: m_PlayerId(playerId), m_Profile(profile), m_Random(uint64_t(profile.Seed) * 0x9E3779B97F4A7C15ULL + playerId)
	{
	}

	float BotBrain::Noise(float spread)
	{
		return (m_Random.Float() + m_Random.Float() + m_Random.Float() - 1.5f) * spread * 1.41f;
	}

	AbilityId BotBrain::SpellIn(uint32_t slot) const
	{
		return m_Traits && slot < AbilitySlots ? m_Traits->Skills[slot].Ability : AbilityId::Count;
	}

	bool BotBrain::Ready(uint32_t slot) const
	{
		return m_State && m_Traits && slot < AbilitySlots && m_State->CastSlot < 0 && m_State->DashDistance <= 0.0f && !m_State->IsStunned() && CanCast(*m_State, *m_Traits, slot);
	}

	const BotBrain::Rival* BotBrain::FindRival(uint32_t playerId) const
	{
		const auto found = std::find_if(m_Rivals.begin(), m_Rivals.end(), [&](const Rival& rival) { return rival.PlayerId == playerId; });
		return found != m_Rivals.end() ? &*found : nullptr;
	}

	void BotBrain::Perceive(World& world, Entity self)
	{
		EntityRegistry& entities = world.GetEntities();
		const PlayerController* controller = entities.TryGet<PlayerController>(self);
		const Transform* transform = entities.TryGet<Transform>(self);
		const Health* health = entities.TryGet<Health>(self);
		m_State = controller ? &controller->State : nullptr;
		m_Traits = controller ? &controller->Traits : nullptr;
		m_Alive = controller && transform && health && controller->State.Alive;
		if (!m_Alive)
			return;
		const MatchDirector& match = world.GetMatch();
		m_Terrain = &world.GetLevel();
		m_Position = transform->Position;
		m_Push = controller->State.Push;
		m_Wind = world.GetLevel().Skyborne ? GustAt(match.GetBrawl().Plan, match.GetBrawl().Clock).Velocity : Vec2();
		m_Health = float(std::max(health->Current, 0)) / float(std::max(health->Max, 1));
		m_Life = std::max(health->Current, 0);
		m_SelfFling = match.GetKnockbackScale(NoPlayer, m_PlayerId, m_Health);
		m_Burning = match.IsBurning(m_PlayerId);
		m_Loadout = match.GetLoadout(m_PlayerId);
		m_DamageMode = world.IsDamageBrawl();
		m_Knockback = float(match.GetRules().Knockback) / 100.0f;
		m_Reserve = 0.0f;
		for (uint32_t slot = 1; slot < AbilitySlots && m_DamageMode; slot++)
			if (SpellIn(slot) == AbilityId::Warp)
				m_Reserve = m_Traits->Skills[slot].ManaCost * ParamsOf(m_Profile.Skill).Reserve;

		const SkillParams& params = ParamsOf(m_Profile.Skill);
		const double now = double(world.GetTick());
		const double delay = double(params.Reaction) * double(world.GetSettings().TickRate);
		const float window = float(VelocityTicks) * world.GetTickSeconds();
		m_Rivals.clear();
		entities.Each<PlayerController>([&](Entity entity, PlayerController& other)
		{
			if (other.PlayerId == m_PlayerId || !other.State.Alive || other.State.IsFalling() || !match.IsPresent(other.PlayerId) || match.AreAllies(m_PlayerId, other.PlayerId))
				return;
			const Transform* place = entities.TryGet<Transform>(entity);
			const Health* life = entities.TryGet<Health>(entity);
			if (!place || !life)
				return;
			Rival rival;
			rival.Handle = entity;
			rival.PlayerId = other.PlayerId;
			Vec2 seen = place->Position;
			Vec2 before = place->Position;
			if (!world.GetPastPosition(entity, now - delay, seen))
				seen = place->Position;
			if (!world.GetPastPosition(entity, now - delay - double(VelocityTicks), before))
				before = seen;
			rival.Position = seen;
			rival.Velocity = ClampLength((seen - before) / window, 14.0f);
			const float fraction = float(std::max(life->Current, 0)) / float(std::max(life->Max, 1));
			rival.Fling = match.GetKnockbackScale(m_PlayerId, other.PlayerId, fraction);
			rival.Health = fraction;
			rival.Life = std::max(life->Current, 0);
			rival.Chilled = other.State.Chill > 0.0f;
			rival.Marked = match.GetMarkedBy(other.PlayerId) == m_PlayerId;
			rival.Mobility = Mobility(other);
			rival.Warded = other.State.Ward > 0.0f;
			rival.Shielded = other.State.Invulnerable > 0.0f || other.State.DashDistance > 0.0f;
			rival.CanDodge = other.State.DodgeCooldown <= 0.6f;
			rival.Points = match.GetPoints(other.PlayerId);
			m_Rivals.push_back(rival);
		});

		m_Missiles.clear();
		m_HasBolt = false;
		entities.Each<Projectile>([&](Entity entity, Projectile& projectile)
		{
			const Replicated* replicated = entities.TryGet<Replicated>(entity);
			const Transform* place = entities.TryGet<Transform>(entity);
			if (!replicated || !place || projectile.Lifetime <= 0.0f)
				return;
			if (replicated->Owner == m_PlayerId)
			{
				if (projectile.Ability == uint8_t(AbilityId::SplittingBolt) && projectile.Source == self)
				{
					m_HasBolt = true;
					m_Bolt = place->Position;
					m_BoltVelocity = projectile.Velocity;
					m_BoltLifetime = projectile.Lifetime;
				}
				return;
			}
			if (match.AreAllies(m_PlayerId, replicated->Owner))
				return;
			m_Missiles.push_back({ replicated->NetId, place->Position, projectile.Velocity, projectile.Radius, projectile.Lifetime, AbilityId(projectile.Ability), projectile.Damage });
		});
		const float dt = world.GetTickSeconds();
		for (Seen& seen : m_Seen)
			seen.Since += dt;
		for (const Missile& missile : m_Missiles)
			if (std::none_of(m_Seen.begin(), m_Seen.end(), [&](const Seen& seen) { return seen.NetId == missile.NetId; }))
				m_Seen.push_back({ missile.NetId, 0.0f, false });

		m_Zones.clear();
		m_HasWell = false;
		entities.Each<PendingStrike>([&](Entity entity, PendingStrike& strike)
		{
			const Transform* place = entities.TryGet<Transform>(entity);
			if (!place || strike.Remaining <= 0.0f)
				return;
			if (strike.Owner == m_PlayerId)
			{
				if (strike.Push < 0.0f)
				{
					m_HasWell = true;
					m_Well = place->Position;
					m_WellRemaining = strike.Remaining;
				}
				return;
			}
			const Replicated* replicated = entities.TryGet<Replicated>(entity);
			if (!replicated || strike.Shape != StrikeShape::Circle || match.AreAllies(m_PlayerId, strike.Owner))
				return;
			m_Zones.push_back({ replicated->NetId, place->Position, strike.Radius, strike.Remaining });
		});
		std::erase_if(m_Seen, [&](const Seen& seen) { return std::none_of(m_Missiles.begin(), m_Missiles.end(), [&](const Missile& missile) { return missile.NetId == seen.NetId; }) && std::none_of(m_Zones.begin(), m_Zones.end(), [&](const Zone& zone) { return zone.NetId == seen.NetId; }); });
		for (const Zone& zone : m_Zones)
			if (std::none_of(m_Seen.begin(), m_Seen.end(), [&](const Seen& seen) { return seen.NetId == zone.NetId; }))
				m_Seen.push_back({ zone.NetId, 0.0f, false });
	}

	void BotBrain::Lobby(World& world, Entity self, const BrawlStatus& brawl)
	{
		if (brawl.Phase != BrawlPhase::Draft && brawl.Phase != BrawlPhase::Pick)
			return;
		if (brawl.Phase == BrawlPhase::Draft && !m_Drafted)
		{
			const BrawlSpells spells = PickBotSpells(m_Profile.Style, m_Random, m_Profile.RandomSpells);
			for (uint32_t slot = 0; slot < SkillSlots; slot++)
				world.PickBrawlSpell(self, slot, spells[slot]);
			m_Drafted = true;
		}
		const BrawlLoadout& loadout = world.GetMatch().GetLoadout(m_PlayerId);
		if (loadout.OfferPending && m_PhaseTime >= m_CardDelay)
			world.ChooseCard(self, PickBotCard(loadout, m_Profile.Style, m_Random, world.GetMatch().GetBrawlMode()));
		if (!m_Readied && !world.GetMatch().GetLoadout(m_PlayerId).OfferPending && m_PhaseTime >= m_ReadyDelay)
			m_Readied = world.SetBrawlReady(self, true);
	}

	Vec2 BotBrain::SafeDirection(const BotArena& arena, Vec2 from, float step) const
	{
		const Vec2 gradient(arena.Clearance(from + Vec2(step, 0.0f)) - arena.Clearance(from - Vec2(step, 0.0f)), arena.Clearance(from + Vec2(0.0f, step)) - arena.Clearance(from - Vec2(0.0f, step)));
		return LengthSquared(gradient) > 1e-6f ? Normalize(gradient) : Vec2();
	}

	int32_t BotBrain::ComboSlot(uint32_t slot, float castTime) const
	{
		for (uint32_t other = 1; other < AbilitySlots; other++)
			if (other != slot && SpellIn(other) == AbilityId::Comet && m_State->Cooldowns[other] <= castTime + 0.1f)
				return int32_t(other);
		return -1;
	}

	Vec2 BotBrain::SafePoint(const BotArena& arena, float reach, float minimum, Vec2 drift) const
	{
		Vec2 best = m_Position;
		float bestScore = -1e9f;
		for (float radius = 0.75f; radius <= reach + 1e-3f; radius += 0.75f)
			for (uint32_t step = 0; step < 16; step++)
			{
				const Vec2 point = m_Position + FromAngle(2.0f * Pi * float(step) / 16.0f) * radius;
				if (m_Terrain && !Climbable(*m_Terrain, m_Position, point))
					continue;
				const float clearance = std::min(arena.Clearance(point), arena.Clearance(point + drift));
				const float score = std::min(clearance, 3.5f) - 0.12f * radius + (clearance >= minimum ? 2.0f : 0.0f);
				if (score > bestScore)
				{
					bestScore = score;
					best = point;
				}
			}
		return best;
	}

	void BotBrain::PressDodge(const BotArena& arena, PlayerInput& input, Vec2 direction, bool path)
	{
		if (!m_State || m_State->DodgeCooldown > 0.0f || (m_State->HeldButtons & uint8_t(InputButton::Dodge)) != 0 || m_State->DashDistance > 0.0f || LengthSquared(direction) < 1e-8f)
			return;
		const float reach = GetAbility(AbilityId::Roll).DashDistance;
		const auto clear = [&](Vec2 heading)
		{
			for (float t = 0.25f; t <= 1.0f; t += 0.25f)
				if (arena.Clearance(m_Position + heading * (reach * t)) < RollClearance)
					return false;
			return true;
		};
		Vec2 heading = Normalize(direction);
		if (path && !clear(heading))
		{
			const auto turned = std::find_if(std::begin(RollTurns), std::end(RollTurns), [&](float turn) { return clear(Rotate(heading, FromAngle(turn))); });
			if (turned == std::end(RollTurns))
				return;
			heading = Rotate(heading, FromAngle(*turned));
		}
		input.Buttons |= uint8_t(InputButton::Dodge);
		input.Move = heading;
	}

	void BotBrain::Cast(PlayerInput& input, uint32_t slot, float aim, float distance, float charge)
	{
		const SkillParams& params = ParamsOf(m_Profile.Skill);
		const SkillTraits& skill = m_Traits->Skills[slot];
		m_Casting = int32_t(slot);
		m_ChargeUntil = m_Time + skill.CastTime + charge * std::max(skill.ChargeTime - skill.CastTime, 0.0f);
		m_ChargeSpeed = SpearShot(charge).Speed;
		m_CastNoise = Noise(params.AimNoise);
		m_CastStretch = 1.0f + Noise(params.AimNoise * 0.6f);
		m_CastAim = aim + m_CastNoise;
		m_CastDistance = std::clamp(distance * m_CastStretch, 0.0f, MaxAimDistance);
		input.Buttons |= uint8_t(SlotButton(slot));
		input.Aim = m_CastAim;
		input.AimDistance = m_CastDistance;
	}

	bool BotBrain::Escape(const World& world, const BotArena& arena, PlayerInput& input)
	{
		const BrawlStatus& brawl = world.GetMatch().GetBrawl();
		const bool lava = IsBrawlLava(world.GetLevel(), brawl, m_Position) || m_Burning;
		const float collapse = arena.CollapseIn(m_Position);
		const Gust gust = GustAt(brawl.Plan, brawl.Clock);
		const Vec2 drift = gust.Warning > 0.0f ? gust.Direction * (GustSpeed * GustLook) : Vec2();
		const bool windy = LengthSquared(drift) > 0.0f && arena.Clearance(m_Position + drift) < EscapeClearance;
		if (!lava && !windy && collapse > 0.9f)
		{
			m_Danger = 0.0f;
			return false;
		}
		if (m_Danger <= 0.0f)
			m_Composed = m_Random.Float() < ParamsOf(m_Profile.Skill).Recovery;
		m_Danger += m_TickSeconds;
		const bool act = m_Composed || m_Danger > 0.8f;
		const Vec2 safe = SafePoint(arena, lava ? 6.5f : 4.0f, EscapeClearance, drift);
		const Vec2 offset = safe - m_Position;
		const float distance = Length(offset);
		input.Move = distance > 1e-3f ? offset / distance : SafeDirection(arena, m_Position, 0.5f);
		if (m_State->CastSlot < 0)
		{
			input.Aim = AngleOf(input.Move);
			input.AimDistance = distance;
		}
		for (uint32_t slot = 1; slot < AbilitySlots && lava && act && m_State->CastSlot < 0; slot++)
		{
			const AbilityDef& def = GetAbility(SpellIn(slot));
			if (!def.Swap || !Ready(slot))
				continue;
			const Rival* best = nullptr;
			float bestScore = EscapeClearance;
			Vec2 point;
			for (const Rival& rival : m_Rivals)
			{
				if (rival.Warded || rival.Shielded)
					continue;
				const Vec2 aim = Lead(rival, m_Traits->Skills[slot].CastTime, def.ProjectileSpeed);
				const float reach = Length(aim - m_Position);
				if (reach > std::min(def.ProjectileRange + def.SpawnOffset - 0.3f, 7.5f) || !HasLineOfFire(world.GetLevel(), m_Position, aim))
					continue;
				if (const float score = arena.Clearance(rival.Position) - 0.12f * reach; score > bestScore)
				{
					bestScore = score;
					best = &rival;
					point = aim;
				}
			}
			if (best)
			{
				Cast(input, slot, AngleOf(point - m_Position), Length(point - m_Position));
				m_CastTarget = best->PlayerId;
				return true;
			}
		}
		for (uint32_t slot = 1; slot < AbilitySlots && lava && act; slot++)
			if (SpellIn(slot) == AbilityId::Warp && Ready(slot) && distance > 1.3f)
			{
				Cast(input, slot, AngleOf(offset), std::min(distance + 0.6f, GetAbility(AbilityId::Warp).Range));
				m_CastTarget = NoPlayer;
				return true;
			}
		for (uint32_t slot = 1; slot < AbilitySlots && lava && act && !m_Yanking && m_State->CastSlot < 0; slot++)
			if (SpellIn(slot) == AbilityId::ChainHook && Ready(slot) && distance > 1.6f)
			{
				Cast(input, slot, AngleOf(offset), distance);
				m_CastTarget = NoPlayer;
				m_Yanking = true;
				m_YankTarget = safe + (distance > 1e-3f ? offset / distance : Vec2()) * 0.6f;
				return true;
			}
		if ((lava || collapse < 0.5f) && distance > 1.0f && act)
			PressDodge(arena, input, input.Move, false);
		return true;
	}

	bool BotBrain::Recover(const BotArena& arena, PlayerInput& input)
	{
		if (LengthSquared(m_Push) < 2.5f * 2.5f)
			return false;
		const Vec2 landing = m_Position + m_Push * (LandingDrag * arena.Slide(m_Position) / PushFriction) + m_Wind * FlightSeconds;
		const float danger = arena.Clearance(landing);
		if (danger > 0.35f)
		{
			m_Flung = 0.0f;
			return false;
		}
		if (m_Flung <= 0.0f)
			m_FlingComposed = m_Random.Float() < ParamsOf(m_Profile.Skill).Recovery;
		m_Flung += m_TickSeconds;
		Vec2 best = SafeDirection(arena, landing, 0.5f);
		float bestClearance = danger;
		for (uint32_t step = 0; step < 16; step++)
		{
			const Vec2 direction = FromAngle(2.0f * Pi * float(step) / 16.0f);
			const float clearance = arena.Clearance(m_Position + direction * GetAbility(AbilityId::Roll).DashDistance + m_Push * (0.5f * LandingDrag * arena.Slide(m_Position) / PushFriction));
			if (clearance > bestClearance)
			{
				bestClearance = clearance;
				best = direction;
			}
		}
		input.Move = LengthSquared(best) > 0.0f ? best : -Normalize(m_Push);
		if (m_State->CastSlot < 0)
			for (uint32_t slot = 1; slot < AbilitySlots; slot++)
				if (SpellIn(slot) == AbilityId::Warp && Ready(slot) && bestClearance < 0.6f && m_FlingComposed)
				{
					const Vec2 safe = SafePoint(arena, GetAbility(AbilityId::Warp).Range, EscapeClearance);
					Cast(input, slot, AngleOf(safe - m_Position), Length(safe - m_Position) + 0.4f);
					m_CastTarget = NoPlayer;
					return true;
				}
		if (bestClearance > danger + 0.3f && m_FlingComposed)
			PressDodge(arena, input, best, false);
		return true;
	}

	bool BotBrain::Evade(const World& world, const BotArena& arena, PlayerInput& input)
	{
		if (m_State->Ward > 0.0f || m_State->Invulnerable > 0.0f || m_State->DashDistance > 0.0f)
			return false;
		const SkillParams& params = ParamsOf(m_Profile.Skill);
		const float speed = m_Traits->MoveSpeed;
		for (const Missile& missile : m_Missiles)
		{
			const float speedSquared = LengthSquared(missile.Velocity);
			if (speedSquared < 1e-4f)
				continue;
			const Vec2 relative = missile.Position - m_Position;
			const float time = -Dot(relative, missile.Velocity) / speedSquared;
			if (time < 0.0f || time > std::min(missile.Lifetime, 0.55f))
				continue;
			const float miss = Length(relative + missile.Velocity * time);
			if (miss > missile.Radius + PlayerRadius + 0.25f)
				continue;
			const auto seen = std::find_if(m_Seen.begin(), m_Seen.end(), [&](const Seen& entry) { return entry.NetId == missile.NetId; });
			if (seen == m_Seen.end() || seen->Since < params.Reaction)
				continue;
			if (!seen->Answered)
			{
				seen->Answered = true;
				if (m_Random.Float() > params.DodgeChance)
					seen->Since = -1e9f;
			}
			if (seen->Since < 0.0f)
				continue;
			const Vec2 across = Normalize(Vec2(-missile.Velocity.y, missile.Velocity.x));
			const float left = arena.Clearance(m_Position + across * 2.0f);
			const float right = arena.Clearance(m_Position - across * 2.0f);
			const Vec2 side = left >= right ? across : -across;
			const Vec2 hitLanding = m_Position + Normalize(missile.Velocity) * (11.5f * m_SelfFling * LandingDrag * arena.Slide(m_Position) / PushFriction);
			const bool hurts = m_DamageMode && (missile.Damage >= DodgeDamage || missile.Damage >= m_Life);
			const bool dangerous = missile.Ability == AbilityId::ChainHook || arena.Clearance(hitLanding) < 0.5f || hurts;
			if (m_State->CastSlot < 0 && (dangerous || m_Profile.Style == BotStyle::Trickster))
				for (uint32_t slot = 1; slot < AbilitySlots; slot++)
					if (SpellIn(slot) == AbilityId::MirrorWard && Ready(slot))
					{
						Cast(input, slot, AngleOf(-relative), Length(relative));
						m_CastTarget = NoPlayer;
						input.Move = side;
						return true;
					}
			input.Move = side;
			if (time < 0.3f && dangerous)
			{
				if (m_State->DodgeCooldown > 0.0f)
					Blink(arena, input, side);
				else
					PressDodge(arena, input, side);
			}
			m_Sidestep = side;
			m_SidestepTime = 0.25f;
			return true;
		}
		for (const Zone& zone : m_Zones)
		{
			const Vec2 offset = m_Position - zone.Center;
			const float distance = Length(offset);
			const auto seen = std::find_if(m_Seen.begin(), m_Seen.end(), [&](const Seen& entry) { return entry.NetId == zone.NetId; });
			if (distance > zone.Radius + 0.3f || seen == m_Seen.end() || seen->Since < params.Reaction)
				continue;
			if (!seen->Answered)
			{
				seen->Answered = true;
				if (m_Random.Float() > params.DodgeChance)
					seen->Since = -1e9f;
			}
			if (seen->Since < 0.0f)
				continue;
			Vec2 out =distance > 1e-3f ? offset / distance : SafeDirection(arena, m_Position, 0.5f);
			if (arena.Clearance(zone.Center + out * (zone.Radius + 0.6f)) < 0.5f)
			{
				const Vec2 across(-out.y, out.x);
				out = arena.Clearance(zone.Center + across * (zone.Radius + 0.6f)) > arena.Clearance(zone.Center - across * (zone.Radius + 0.6f)) ? across : -across;
			}
			input.Move = out;
			const float exit = (zone.Radius + 0.4f - Dot(offset, out)) / std::max(speed, 0.1f);
			if (exit > zone.Remaining && m_Random.Float() < params.DodgeChance)
			{
				if (m_State->DodgeCooldown > 0.0f)
					Blink(arena, input, out);
				else
					PressDodge(arena, input, out);
			}
			return true;
		}
		(void)world;
		return false;
	}

	bool BotBrain::Blink(const BotArena& arena, PlayerInput& input, Vec2 direction)
	{
		if (!m_DamageMode || m_State->CastSlot >= 0 || LengthSquared(direction) < 1e-4f)
			return false;
		for (uint32_t slot = 1; slot < AbilitySlots; slot++)
			if (SpellIn(slot) == AbilityId::Warp && Ready(slot))
			{
				const float reach = std::min(GetAbility(AbilityId::Warp).Range, BlinkReach);
				if (arena.Clearance(m_Position + direction * reach) < EscapeClearance)
					return false;
				Cast(input, slot, AngleOf(direction), reach);
				m_CastTarget = NoPlayer;
				return true;
			}
		return false;
	}

	bool BotBrain::FollowUp(const BotArena& arena, PlayerInput& input, BotTally& tally)
	{
		if (m_FollowSlot < 0)
			return false;
		if (m_Time > m_FollowUntil)
		{
			m_FollowSlot = -1;
			return false;
		}
		if (m_State->CastSlot >= 0 || !m_HasWell)
			return true;
		const uint32_t slot = uint32_t(m_FollowSlot);
		m_FollowSlot = -1;
		if (!Ready(slot) || m_WellRemaining < ComboLate)
			return false;
		const Vec2 point = m_Well + SafeDirection(arena, m_Well, 0.75f) * ComboOffset;
		Cast(input, slot, AngleOf(point - m_Position), Length(point - m_Position));
		m_CastTarget = NoPlayer;
		tally.Combos++;
		return true;
	}

	bool BotBrain::Yank(PlayerInput& input)
	{
		if (!m_Yanking)
			return false;
		const int32_t slot = m_State->RecastSlot;
		if (slot < 0 || AbilityId(m_State->RecastAbility) != AbilityId::ChainHook)
		{
			if (m_State->CastSlot < 0)
				m_Yanking = false;
			return false;
		}
		const float flown = m_State->RecastAge * GetAbility(AbilityId::ChainHook).ProjectileSpeed;
		if (flown < std::min(Length(m_YankTarget - m_State->RecastOrigin), m_State->RecastReach) - 0.2f)
			return false;
		input.Buttons |= uint8_t(SlotButton(uint32_t(slot)));
		m_Yanking = false;
		return true;
	}

	bool BotBrain::Split(PlayerInput& input)
	{
		const int32_t slot = m_State->RecastSlot;
		if (!m_HasBolt || !m_SplitArmed || slot < 0 || AbilityId(m_State->RecastAbility) != AbilityId::SplittingBolt)
			return false;
		const float speed = Length(m_BoltVelocity);
		if (speed < 1e-3f)
			return false;
		const Vec2 heading = m_BoltVelocity / speed;
		const AbilityDef& bolt = GetAbility(AbilityId::SplittingBolt);
		const AbilityDef& shard = GetAbility(AbilityId::BoltShard);
		const uint32_t count = HasCard(m_Loadout.Cards, CardId::Fivefold) ? FivefoldShards : BoltShards;
		const float spread = BoltShardSpread * float(BoltShards - 1) / float(count - 1);
		bool caught = false;
		for (const Rival& rival : m_Rivals)
		{
			if (rival.Warded)
				continue;
			const Vec2 offset = rival.Position - m_Bolt;
			const float along = Dot(offset, heading);
			if (along > 0.0f && along < speed * m_BoltLifetime && std::abs(Dot(offset, Vec2(-heading.y, heading.x))) < bolt.ProjectileRadius + PlayerRadius)
				return false;
			const Vec2 ahead = offset + rival.Velocity * (Length(offset) / shard.ProjectileSpeed);
			const float distance = Length(ahead);
			if (distance > shard.ProjectileRange + PlayerRadius || distance < 0.4f)
				continue;
			const float bearing = AngleBetween(AngleOf(heading), AngleOf(ahead));
			const float tolerance = std::atan2(shard.ProjectileRadius + PlayerRadius * 0.8f, distance);
			for (uint32_t i = 0; i < count; i++)
				caught = caught || std::abs(bearing - spread * (float(i) - float(count - 1) * 0.5f)) < tolerance;
		}
		if (!caught)
			return false;
		input.Buttons |= uint8_t(SlotButton(uint32_t(slot)));
		m_SplitArmed = false;
		return true;
	}

	float BotBrain::LandingValue(const BotArena& arena, Vec2 position, Vec2 impulse) const
	{
		const float magnitude = Length(impulse);
		if (magnitude < 0.1f)
			return 0.0f;
		const Vec2 landing = position + impulse * (LandingDrag * arena.Slide(position) / PushFriction);
		const float before = arena.Clearance(position);
		const float after = arena.Clearance(landing);
		if (after < 0.0f)
			return 0.6f + 0.4f * std::clamp(-after / 2.0f, 0.0f, 1.0f);
		return 0.04f + 0.3f * std::clamp((before - after) / 5.0f, 0.0f, 1.0f) + (after < 1.0f ? 0.12f : 0.0f);
	}

	float BotBrain::Harm(float damage, const Rival& rival, AbilityId spell) const
	{
		if (!m_DamageMode || damage <= 0.0f || rival.Warded)
			return 0.0f;
		if (HasCard(m_Loadout.Cards, CardId::GlassCannon))
			damage *= GlassCannonDamageDealt;
		if (rival.Chilled && HasCard(m_Loadout.Cards, CardId::Brittle))
			damage *= BrittleScale;
		if (rival.Marked && spell == AbilityId::EmberBolt)
			damage *= ConductiveScale;
		return damage / HarmScale + (damage >= float(rival.Life) ? FinishBonus : 0.0f);
	}

	float BotBrain::HitChance(float travel, float distance, const Rival& rival) const
	{
		float chance = 1.12f - travel * 0.85f * rival.Mobility - (distance > 8.0f ? 0.1f : 0.0f);
		if (rival.Shielded)
			chance *= 0.3f;
		return std::clamp(chance, 0.08f, 0.95f);
	}

	Vec2 BotBrain::Lead(const Rival& rival, float delay, float speed) const
	{
		Vec2 point = rival.Position + rival.Velocity * delay;
		for (int iteration = 0; iteration < 2; iteration++)
		{
			const float flight = speed > 0.0f ? Length(point - m_Position) / speed : 0.0f;
			point = rival.Position + rival.Velocity * (delay + flight);
		}
		return point;
	}

	BotBrain::Option BotBrain::PlanCast(const World& world, const BotArena& arena, uint32_t slot) const
	{
		Option best;
		best.Slot = int32_t(slot);
		const AbilityId spell = SpellIn(slot);
		if (spell == AbilityId::Count || !Ready(slot))
			return best;
		const AbilityDef& def = GetAbility(spell);
		const SkillTraits& skill = m_Traits->Skills[slot];
		const bool free = IsOverloaded(*m_State, *m_Traits, slot);
		if (m_DamageMode && !free && skill.ManaCost > 0.0f && m_State->Mana - skill.ManaCost < m_Reserve && m_Traits->BloodRate <= 0.0f)
			return best;
		const bool bleeding = m_DamageMode && !free && m_State->Mana + 1e-3f < skill.ManaCost;
		const bool bloodMagic = m_DamageMode && m_Traits->MaxMana <= 0.0f && m_Traits->BloodRate > 0.0f;
		if (bleeding && m_Health < (bloodMagic ? BloodMagicFloor : BloodHealth))
			return best;
		const float price = bloodMagic ? skill.ManaCost * m_Traits->BloodRate / float(BrawlHealth) : (m_DamageMode ? ManaPrice * skill.ManaCost / std::max(m_Traits->MaxMana, 1.0f) : 0.0f) + (bleeding ? BloodPrice : 0.0f);
		const float threshold = Threshold(spell) / ParamsOf(m_Profile.Skill).Eagerness + price;
		const float weight = StyleWeight(m_Profile.Style, spell);
		const float damage = float(def.BrawlDamage);
		const Level& level = world.GetLevel();
		const auto setupReady = [&]
		{
			for (uint32_t other = 1; other < AbilitySlots; other++)
				if (other != slot && (SpellIn(other) == AbilityId::Comet || SpellIn(other) == AbilityId::Gust) && m_State->Cooldowns[other] < 0.5f)
					return true;
			return false;
		};
		float charge = 0.0f;
		const auto consider = [&](float score, float aim, float distance, uint32_t target, Vec2 point, int32_t follow = -1)
		{
			if (score > best.Score)
			{
				best.Score = score;
				best.Aim = aim;
				best.Distance = distance;
				best.Target = target;
				best.Point = point;
				best.FollowSlot = follow;
				best.Charge = charge;
			}
		};
		switch (def.Kind)
		{
		case AbilityKind::Projectile:
			if (IsCharged(skill))
			{
				const float span = std::max(skill.ChargeTime - skill.CastTime, 0.0f);
				for (const Rival& rival : m_Rivals)
				{
					if (rival.Warded)
						continue;
					const float gap = Length(rival.Position - m_Position);
					const float needed = std::clamp((gap + 0.4f - def.ProjectileRange - def.SpawnOffset) / (SpearFullRange - def.ProjectileRange), 0.0f, 1.0f);
					for (const float power : { needed, std::max(needed, 0.5f), 1.0f })
					{
						const ChargedShot shot = SpearShot(power);
						const float hold = skill.CastTime + power * span;
						const Vec2 point = Lead(rival, hold, shot.Speed);
						const Vec2 offset = point - m_Position;
						const float distance = Length(offset);
						if (distance < 0.3f || distance > shot.Range + def.SpawnOffset - 0.2f || !HasLineOfFire(level, m_Position, point))
							continue;
						float value = LandingValue(arena, point, offset / distance * (shot.Push * rival.Fling)) + 0.08f + (setupReady() ? 0.12f : 0.0f);
						value += Harm(damage * shot.DamageScale, rival, spell);
						charge = power;
						consider(HitChance(hold + distance / shot.Speed, distance, rival) * value * weight - threshold, AngleOf(offset), distance, rival.PlayerId, point);
					}
				}
				break;
			}
			if (spell == AbilityId::ChainHook)
			{
				const float pull = (HasCard(m_Loadout.Cards, CardId::Whiplash) ? WhiplashPull : HookPull) * (m_DamageMode ? DamagePullScale : 1.0f) * m_Knockback;
				const float reach = def.ProjectileRange * skill.RangeScale + def.SpawnOffset - 0.2f;
				for (const Rival& rival : m_Rivals)
				{
					if (rival.Warded || rival.Shielded)
						continue;
					const Vec2 point = Lead(rival, skill.CastTime, def.ProjectileSpeed);
					const Vec2 offset = point - m_Position;
					const float distance = Length(offset);
					if (distance < 1.2f || distance > reach || !HasLineOfFire(level, m_Position, point))
						continue;
					const float value = LandingValue(arena, point, -offset * (PushFriction * pull)) + Harm(damage, rival);
					const float travel = skill.CastTime + distance / def.ProjectileSpeed;
					consider(HitChance(travel, distance, rival) * value * weight - threshold, AngleOf(offset), distance, rival.PlayerId, point);
				}
				break;
			}
			for (const Rival& rival : m_Rivals)
			{
				if (rival.Warded)
					continue;
				const Vec2 point = Lead(rival, skill.CastTime, def.ProjectileSpeed);
				const Vec2 offset = point - m_Position;
				const float distance = Length(offset);
				if (distance < 0.3f || distance > def.ProjectileRange + def.SpawnOffset - 0.2f || !HasLineOfFire(level, m_Position, point))
					continue;
				const Vec2 direction = offset / distance;
				float value = 0.0f;
				if (def.Swap)
				{
					const float mine = arena.Clearance(m_Position);
					const float theirs = arena.Clearance(rival.Position);
					if (mine < 1.2f && theirs > mine + 1.5f)
						value += 0.4f + 0.3f * std::clamp((theirs - mine) / 4.0f, 0.0f, 1.0f);
					if (mine < 0.3f)
						value += 0.5f;
				}
				else
				{
					value = LandingValue(arena, point, direction * (def.Push * rival.Fling));
					if (def.Chain > 0)
					{
						float chained = 0.0f;
						for (const Rival& other : m_Rivals)
							if (other.PlayerId != rival.PlayerId && !other.Warded && LengthSquared(other.Position - point) < ChainReach * ChainReach)
								chained = std::max(chained, LandingValue(arena, other.Position, Normalize(other.Position - point) * (def.Push * other.Fling)) + Harm(damage, other));
						value += 0.5f * chained;
					}
					if (def.Chill > 0.0f)
						value += 0.08f + (setupReady() ? 0.12f : 0.0f);
				}
				value += Harm(damage, rival, spell);
				const float travel = skill.CastTime + distance / def.ProjectileSpeed;
				consider(HitChance(travel, distance, rival) * value * weight - threshold, AngleOf(direction), distance, rival.PlayerId, point);
			}
			break;
		case AbilityKind::Melee:
			for (const Rival& aimed : m_Rivals)
			{
				const Vec2 point = aimed.Position + aimed.Velocity * skill.CastTime;
				const Vec2 offset = point - m_Position;
				const float distance = Length(offset);
				if (distance < 1e-3f || distance > def.Reach + PlayerRadius * 0.5f)
					continue;
				const float aim = AngleOf(offset);
				const Vec2 axis = offset / distance;
				float value = 0.0f;
				for (const Rival& rival : m_Rivals)
				{
					const Vec2 position = rival.Position + rival.Velocity * skill.CastTime;
					const Vec2 reach = position - m_Position;
					const float range = Length(reach);
					if (rival.Warded || rival.Shielded || range > def.Reach || std::abs(AngleBetween(aim, AngleOf(reach))) > def.HalfArc)
						continue;
					const Vec2 radial = range > 1e-3f ? reach / range : axis;
					const Vec2 push = Normalize(axis * 0.65f + radial * 0.35f) * (def.Push * rival.Fling);
					value += (range < def.Reach - 0.5f ? 0.9f : 0.6f) * (LandingValue(arena, position, push) + Harm(damage, rival));
				}
				consider(value * weight - threshold, aim, distance, aimed.PlayerId, point);
			}
			break;
		case AbilityKind::Nova:
		{
			float value = 0.0f;
			for (const Rival& rival : m_Rivals)
			{
				const Vec2 position = rival.Position + rival.Velocity * skill.CastTime;
				const Vec2 reach = position - m_Position;
				const float range = Length(reach);
				if (rival.Warded || rival.Shielded || range > def.Reach || range < 1e-3f)
					continue;
				const Vec2 push = reach / range * (def.Push * (1.0f - 0.4f * range / def.Reach) * rival.Fling);
				value += (range < def.Reach - 0.4f ? 0.85f : 0.55f) * (LandingValue(arena, position, push) + Harm(damage, rival));
			}
			consider(value * weight - threshold, m_Aim, 0.0f, NoPlayer, m_Position);
			break;
		}
		case AbilityKind::Ground:
		{
			const int32_t follow = def.Push < 0.0f && m_ComboArmed ? ComboSlot(slot, skill.CastTime) : -1;
			for (const Rival& rival : m_Rivals)
			{
				const float delay = skill.CastTime + def.ImpactDelay;
				const Vec2 position = rival.Position + rival.Velocity * (delay * 0.55f);
				const Vec2 inward = SafeDirection(arena, position, 0.75f);
				if (follow >= 0)
				{
					const AbilityDef& comet = GetAbility(SpellIn(uint32_t(follow)));
					const float clearance = arena.Clearance(position);
					const float distance = Length(position - m_Position);
					if (clearance > 0.6f && clearance < 4.5f && LengthSquared(inward) > 0.5f && distance > 1.5f && distance < std::min(def.Range, comet.Range) - 0.6f)
					{
						const float push = comet.Push * (1.0f - 0.4f * ComboOffset / comet.ImpactRadius);
						const float gather = (def.ImpactRadius - 0.4f) * (def.ImpactRadius - 0.4f);
						float value = 0.0f;
						for (const Rival& other : m_Rivals)
							if (!other.Warded && LengthSquared(other.Position - position) < gather)
								value += LandingValue(arena, position, -inward * (push * other.Fling)) + Harm(float(comet.BrawlDamage), other);
						const float chance = std::clamp((rival.CanDodge ? 0.3f : 0.7f) + 0.25f * (1.0f - rival.Mobility), 0.25f, 0.9f);
						consider(chance * value * weight - threshold, AngleOf(position - m_Position), distance, rival.PlayerId, position - rival.Position, follow);
					}
				}
				Vec2 center;
				float value = 0.0f;
				if (def.Push < 0.0f)
				{
					const float clearance = std::max(arena.Clearance(position), 0.0f);
					const float reach = def.ImpactRadius * 0.6f;
					const bool edge = clearance <= reach - 0.7f && LengthSquared(inward) >= 0.5f;
					if (!edge && !m_DamageMode)
						continue;
					center = edge ? position - inward * reach : position;
					const float lava = -arena.Clearance(center);
					for (const Rival& other : m_Rivals)
						if (!other.Warded && LengthSquared(other.Position - center) < def.ImpactRadius * def.ImpactRadius)
							value += (edge ? (lava > 0.0f ? 0.4f + 0.2f * std::min(lava, 2.0f) : 0.1f) : 0.0f) + Harm(damage, other);
				}
				else
				{
					center = position + inward * (def.ImpactRadius * (m_DamageMode ? 0.0f : 0.38f));
					for (const Rival& other : m_Rivals)
					{
						const Vec2 at = other.Position + other.Velocity * (delay * 0.55f);
						const Vec2 offset = at - center;
						const float range = Length(offset);
						if (other.Warded || range > def.ImpactRadius)
							continue;
						const Vec2 radial = range > 1e-3f ? offset / range : -inward;
						value += LandingValue(arena, at, radial * (def.Push * (1.0f - 0.4f * range / def.ImpactRadius) * other.Fling)) + Harm(damage, other);
					}
					if (spell == AbilityId::StormStrike && m_DamageMode)
					{
						const AbilityDef& jump = GetAbility(AbilityId::StormJump);
						const Rival* next = nullptr;
						float nearest = jump.Range * jump.Range;
						for (const Rival& other : m_Rivals)
							if (const float gap = LengthSquared(other.Position - center); gap > def.ImpactRadius * def.ImpactRadius && gap < nearest)
							{
								nearest = gap;
								next = &other;
							}
						if (next)
							value += 0.7f * Harm(float(jump.BrawlDamage), *next);
					}
					if (spell == AbilityId::Firebomb && LengthSquared(center - m_Position) > 1e-4f)
					{
						const AbilityDef& skip = GetAbility(AbilityId::FirebombSkip);
						const Vec2 bounce = center + Normalize(center - m_Position) * FirebombSkipDistance;
						const float later = skill.CastTime + skip.ImpactDelay;
						for (const Rival& other : m_Rivals)
						{
							const Vec2 at = other.Position + other.Velocity * (later * 0.55f);
							const Vec2 offset = at - bounce;
							const float range = Length(offset);
							if (other.Warded || range > skip.ImpactRadius)
								continue;
							value += 0.8f * (LandingValue(arena, at, (range > 1e-3f ? offset / range : Normalize(bounce - m_Position)) * (skip.Push * (1.0f - 0.4f * range / skip.ImpactRadius) * other.Fling)) + Harm(float(skip.BrawlDamage), other));
						}
					}
				}
				const Vec2 offset = center - m_Position;
				const float distance = Length(offset);
				if (distance > def.Range || distance < 1.0f)
					continue;
				const float quick = std::clamp(0.85f / std::max(def.ImpactDelay, 0.2f), 1.0f, 2.2f);
				const float chance = std::clamp((0.22f + 0.65f * (1.0f - rival.Mobility)) * quick + (rival.CanDodge ? 0.0f : 0.2f), 0.18f, 0.9f);
				consider(chance * value * weight - threshold, AngleOf(offset), distance, rival.PlayerId, center - rival.Position);
			}
			break;
		}
		default:
			break;
		}
		if (best.Score <= 0.0f && slot > 0 && def.Swap && m_DamageMode && HasCard(m_Loadout.Cards, CardId::UnstableArcana))
			if (const Rival* target = FindRival(m_Target); target && LengthSquared(target->Position - m_Position) < CycleReach * CycleReach)
				consider(CycleScore, AngleOf(target->Position - m_Position), Length(target->Position - m_Position), target->PlayerId, target->Position);
		return best;
	}

	void BotBrain::ChooseTarget(const BotArena& arena)
	{
		uint16_t leader = 0;
		for (const Rival& rival : m_Rivals)
			leader = std::max(leader, rival.Points);
		float bestScore = -1e9f;
		uint32_t best = NoPlayer;
		for (const Rival& rival : m_Rivals)
		{
			const float distance = Length(rival.Position - m_Position);
			float score = 1.0f - std::clamp(distance / 12.0f, 0.0f, 1.0f);
			score += 0.8f * (1.0f - std::clamp(arena.Clearance(rival.Position) / 5.0f, 0.0f, 1.0f));
			score += m_DamageMode ? 0.9f * (1.0f - rival.Health) : 0.4f * (rival.Fling - 1.0f) / MissingHealthKnockback;
			score += rival.PlayerId == m_Target ? 0.35f : 0.0f;
			score -= rival.Warded ? 0.6f : 0.0f;
			score += leader > 0 && rival.Points == leader ? 0.15f : 0.0f;
			score += Noise(0.1f);
			if (score > bestScore)
			{
				bestScore = score;
				best = rival.PlayerId;
			}
		}
		m_Target = best;
	}

	void BotBrain::ChooseGoal(const World& world, const BotArena& arena)
	{
		const BrawlStatus& brawl = world.GetMatch().GetBrawl();
		const TileMap& map = world.GetMap();
		const StormRing ring = RingAt(brawl.Plan, brawl.Clock + RingLook);
		const float safe = std::max(ring.Radius - RingMargin, 0.0f);
		if (ring.Active && Length(m_Position - ring.Center) > safe)
		{
			const Vec2 inward = m_Position - ring.Center;
			m_Goal = ring.Center + (LengthSquared(inward) > 1e-6f ? Normalize(inward) : Vec2(1.0f, 0.0f)) * (safe * 0.5f);
			m_HasGoal = true;
			return;
		}
		const Rival* target = FindRival(m_Target);
		bool closeIn = false;
		for (uint32_t slot = 1; slot < AbilitySlots; slot++)
			if ((SpellIn(slot) == AbilityId::Gust || SpellIn(slot) == AbilityId::Shockwave) && m_State->Cooldowns[slot] < 0.6f)
				closeIn = true;
		const float caution = ParamsOf(m_Profile.Skill).Caution;
		const bool wary = m_DamageMode && m_Health < WaryHealth;
		const float preferred = closeIn && !wary ? 2.4f : (m_Profile.Style == BotStyle::Sniper ? 5.5f : 4.2f) + (wary ? 1.5f : 0.0f);
		const float radius = std::max(brawl.Plan.StartRadius, 1.0f);
		const auto score = [&](Vec2 point)
		{
			if (!map.IsWalkableAt(point) || !Climbable(world.GetLevel(), m_Position, point))
				return -1e9f;
			if (ring.Active && Length(point - ring.Center) > safe)
				return -5.0f - Length(point - ring.Center);
			const float clearance = arena.Clearance(point);
			if (clearance < 0.5f)
				return -10.0f + clearance;
			float value = 1.6f * caution * std::min(clearance, 4.0f) / 4.0f;
			value -= 0.25f * Length(point - brawl.Plan.Center) / radius;
			for (const Rival& rival : m_Rivals)
			{
				const Vec2 offset = point - rival.Position;
				const float distance = Length(offset);
				if (distance > 7.5f || distance < 1e-3f)
					continue;
				const float reach = (distance < 4.0f ? 6.5f : 3.6f) * m_SelfFling;
				const float landing = arena.Clearance(point + offset / distance * reach);
				if (landing < 0.3f)
					value -= 0.8f * caution * (1.0f - distance / 7.5f) * (landing < -1.0f ? 1.2f : 1.0f);
				if (distance < 2.2f && !closeIn)
					value -= 0.25f;
			}
			if (target)
			{
				const float distance = Length(point - target->Position);
				value -= 0.08f * std::abs(distance - preferred);
				if (distance < 1.6f)
					value -= 0.5f;
			}
			for (const Zone& zone : m_Zones)
				if (LengthSquared(point - zone.Center) < (zone.Radius + 0.4f) * (zone.Radius + 0.4f))
					value -= 2.0f;
			for (const Missile& missile : m_Missiles)
			{
				const float speedSquared = LengthSquared(missile.Velocity);
				if (speedSquared < 1e-4f)
					continue;
				const float time = std::clamp(-Dot(missile.Position - point, missile.Velocity) / speedSquared, 0.0f, missile.Lifetime);
				if (time < 1.2f && Length(missile.Position + missile.Velocity * time - point) < missile.Radius + PlayerRadius + 0.3f)
					value -= 0.8f;
			}
			value -= 0.05f * Length(point - m_Position);
			if (m_HasGoal && LengthSquared(point - m_Goal) < 0.64f)
				value += 0.15f;
			if (arena.Clearance((point + m_Position) * 0.5f) < 0.3f)
				value -= 1.0f;
			return value;
		};
		Vec2 best = m_Position;
		float bestScore = score(m_Position);
		for (const float reach : { 1.2f, 2.4f, 3.6f })
			for (uint32_t step = 0; step < 16; step++)
			{
				const Vec2 point = m_Position + FromAngle(2.0f * Pi * (float(step) + (reach > 2.0f ? 0.5f : 0.0f)) / 16.0f) * reach;
				if (const float value = score(point); value > bestScore)
				{
					bestScore = value;
					best = point;
				}
			}
		m_Goal = best;
		m_HasGoal = true;
	}

	void BotBrain::Think(const World& world, const BotArena& arena)
	{
		ChooseTarget(arena);
		ChooseGoal(world, arena);
	}

	bool BotBrain::Planted(const BotArena& arena) const
	{
		if (!m_DamageMode || m_Burning || !HasCard(m_Loadout.Cards, CardId::Leyline))
			return false;
		const Rival* target = FindRival(m_Target);
		if (!target || m_Health < PlantHealth || LengthSquared(target->Position - m_Position) > PlantReach * PlantReach)
			return false;
		uint32_t near = 0;
		for (const Rival& rival : m_Rivals)
		{
			const float gap = LengthSquared(rival.Position - m_Position);
			if (gap < PlantNear * PlantNear)
				return false;
			near += gap < PlantCrowd * PlantCrowd ? 1u : 0u;
		}
		if (near > PlantCrowdMax)
			return false;
		for (const Missile& missile : m_Missiles)
		{
			const float speedSquared = LengthSquared(missile.Velocity);
			if (speedSquared < 1e-4f)
				continue;
			const float time = std::clamp(-Dot(missile.Position - m_Position, missile.Velocity) / speedSquared, 0.0f, missile.Lifetime);
			if (time < PlantHorizon && Length(missile.Position + missile.Velocity * time - m_Position) < missile.Radius + PlayerRadius + 0.5f)
				return false;
		}
		return arena.Clearance(m_Position) > PlantClearance && arena.CollapseIn(m_Position) > PlantCollapse;
	}

	void BotBrain::Steer(PlayerInput& input) const
	{
		if (m_SidestepTime > 0.0f)
		{
			input.Move = m_Sidestep;
			return;
		}
		const Vec2 offset = (m_Routed ? m_Route : m_Goal) - m_Position;
		const float distance = Length(offset);
		Vec2 move = distance > 0.2f ? offset / std::max(distance, 0.6f) : Vec2();
		if (const Rival* target = FindRival(m_Target); target && LengthSquared(target->Position - m_Position) < 81.0f)
		{
			const Vec2 toward = Normalize(target->Position - m_Position);
			move += Vec2(-toward.y, toward.x) * (0.45f * m_Strafe);
		}
		input.Move = ClampLength(move, 1.0f);
	}

	void BotBrain::RouteGoal(const World& world, const BotArena& arena)
	{
		m_Routed = false;
		const Level& level = world.GetLevel();
		if (level.Relief.IsEmpty() || !m_HasGoal || Climbable(level, m_Position, m_Goal))
			return;
		if (m_Time >= m_RouteTime || LengthSquared(m_RouteGoal - m_Goal) > 0.25f)
		{
			const std::optional<Vec2> step = arena.Route(m_Position, m_Goal);
			m_RouteTime = m_Time + RouteSeconds;
			m_RouteGoal = m_Goal;
			m_RouteValid = step.has_value();
			m_Route = step.value_or(m_Goal);
		}
		m_Routed = m_RouteValid;
	}

	void BotBrain::Update(World& world, const BotArena& arena, BotTally& tally)
	{
		const float dt = world.GetTickSeconds();
		m_Time += dt;
		m_TickSeconds = dt;
		const Entity self = world.FindPlayer(m_PlayerId);
		if (!self.IsValid())
			return;
		const BrawlStatus& brawl = world.GetMatch().GetBrawl();
		if (brawl.Phase != m_Phase || brawl.Round != m_Round)
		{
			m_Phase = brawl.Phase;
			m_Round = brawl.Round;
			m_PhaseTime = 0.0f;
			m_Readied = false;
			m_ReadyDelay = m_Profile.Patient ? 1.0f + 3.5f * m_Random.Float() : 0.0f;
			m_CardDelay = m_Profile.Patient ? 1.5f + 3.5f * m_Random.Float() : 0.0f;
			m_Casting = -1;
			m_HasGoal = false;
			m_Target = NoPlayer;
			m_Seen.clear();
			m_FollowSlot = -1;
			m_ComboArmed = m_Random.Float() < ParamsOf(m_Profile.Skill).Combo;
			m_Yanking = false;
			m_SplitArmed = false;
		}
		m_PhaseTime += dt;
		Perceive(world, self);
		Lobby(world, self, brawl);

		PlayerInput input;
		input.Aim = m_Aim;
		input.AimDistance = m_AimDistance;
		m_SidestepTime = std::max(m_SidestepTime - dt, 0.0f);
		m_StrafeTime -= dt;
		if (m_StrafeTime <= 0.0f)
		{
			m_StrafeTime = 0.6f + 0.8f * m_Random.Float();
			m_Strafe = m_Random.Float() < 0.5f ? -1.0f : 1.0f;
		}
		const bool fighting = brawl.Phase == BrawlPhase::Fight && m_Alive && m_State && m_Traits && arena.IsReady();
		if (fighting)
		{
			if (m_State->CastSlot < 0 && m_Casting >= 0 && m_PhaseTime > 0.0f)
				m_Casting = -1;
			Yank(input);
			const bool urgent = Escape(world, arena, input) || Recover(arena, input) || Evade(world, arena, input);
			const bool holding = !urgent && (FollowUp(arena, input, tally) || Split(input));
			m_Planting = !urgent && m_SidestepTime <= 0.0f && Planted(arena);
			m_Think -= dt;
			if (m_Think <= 0.0f || !m_HasGoal)
			{
				const SkillParams& params = ParamsOf(m_Profile.Skill);
				m_Think = params.ThinkInterval * (0.8f + 0.4f * m_Random.Float());
				Think(world, arena);
				if (!urgent && !holding && m_State->CastSlot < 0 && (input.Buttons & ~uint8_t(InputButton::Dodge)) == 0)
				{
					Option chosen;
					for (uint32_t slot = 0; slot < AbilitySlots; slot++)
						if (const Option option = PlanCast(world, arena, slot); option.Score > chosen.Score)
							chosen = option;
					if (chosen.Slot >= 0 && chosen.Score > 0.0f)
					{
						Cast(input, uint32_t(chosen.Slot), chosen.Aim, chosen.Distance, chosen.Charge);
						m_CastTarget = chosen.Target;
						m_CastOffset = chosen.Point;
						if (m_DamageMode && HasCard(m_Loadout.Cards, CardId::EchoChamber) && EchoesInChamber(SpellIn(uint32_t(chosen.Slot))))
						{
							m_EchoSpell = SpellIn(uint32_t(chosen.Slot));
							m_EchoUntil = m_Time + m_Traits->Skills[size_t(chosen.Slot)].CastTime + EchoChamberDelay + 0.1f;
						}
						if (SpellIn(uint32_t(chosen.Slot)) == AbilityId::SplittingBolt)
							m_SplitArmed = m_Random.Float() < 0.5f + 0.5f * params.Combo;
						const AbilityDef& def = GetAbility(SpellIn(uint32_t(chosen.Slot)));
						if (def.Kind == AbilityKind::Ground && def.Push < 0.0f)
						{
							m_FollowSlot = chosen.FollowSlot;
							m_FollowUntil = m_Time + m_Traits->Skills[size_t(chosen.Slot)].CastTime + def.ImpactDelay + 0.3f;
							m_ComboArmed = m_Random.Float() < params.Combo;
						}
					}
				}
			}
			if (!urgent && !m_Planting)
			{
				RouteGoal(world, arena);
				Steer(input);
			}
			const bool echoing = m_State->CastSlot < 0 && m_Time < m_EchoUntil;
			const bool charging = !echoing && m_State->CastSlot >= 0 && m_Casting == m_State->CastSlot && IsCharged(m_Traits->Skills[size_t(m_Casting)]);
			if (charging && m_Time < m_ChargeUntil)
				input.Buttons |= uint8_t(SlotButton(uint32_t(m_Casting)));
			if ((m_State->CastSlot >= 0 && m_Casting >= 0) || echoing)
			{
				const AbilityDef& def = GetAbility(echoing ? m_EchoSpell : SpellIn(uint32_t(m_Casting)));
				const Rival* target = FindRival(m_CastTarget);
				const float lead = echoing ? 0.0f : charging ? std::max(m_ChargeUntil - m_Time, 0.0f) : m_State->CastTimer;
				const float speed = charging ? m_ChargeSpeed : def.ProjectileSpeed;
				if (target && (def.Kind == AbilityKind::Projectile || def.Kind == AbilityKind::Melee || def.Kind == AbilityKind::Ground))
				{
					const Vec2 point = def.Kind == AbilityKind::Ground ? target->Position + m_CastOffset : def.Kind == AbilityKind::Projectile ? Lead(*target, lead, speed) : target->Position + target->Velocity * lead;
					const Vec2 offset = point - m_Position;
					input.Aim = AngleOf(offset) + m_CastNoise;
					input.AimDistance = std::clamp(Length(offset) * m_CastStretch, 0.0f, MaxAimDistance);
				}
				else if (!echoing)
				{
					input.Aim = m_CastAim;
					input.AimDistance = m_CastDistance;
				}
			}
		}
		else if (m_Alive && (brawl.Phase == BrawlPhase::Draft || brawl.Phase == BrawlPhase::Pick))
		{
			m_Wander -= dt;
			if (!m_HasGoal || m_Wander <= 0.0f || LengthSquared(m_Goal - m_Position) < 0.25f)
			{
				m_Goal = brawl.Plan.Center + FromAngle(m_Random.Float() * 2.0f * Pi) * (brawl.Plan.StartRadius * 0.45f * std::sqrt(m_Random.Float()));
				m_HasGoal = true;
				m_Wander = WanderSeconds * (0.6f + 0.8f * m_Random.Float());
			}
			const Vec2 offset = m_Goal - m_Position;
			input.Move = LengthSquared(offset) > 0.25f ? Normalize(offset) * 0.45f : Vec2();
			input.Aim = AngleOf(LengthSquared(offset) > 1e-4f ? offset : Vec2(1.0f, 0.0f));
		}
		else if (m_Alive)
			input.Aim = AngleOf(brawl.Plan.Center - m_Position);
		m_Aim = input.Aim;
		m_AimDistance = input.AimDistance;
		input.Sequence = ++m_Sequence;
		input.ViewTick = world.GetTick();
		input.ViewFraction = 0;
		std::array<AbilityId, AbilitySlots> spells{};
		for (uint32_t slot = 0; slot < AbilitySlots; slot++)
			spells[slot] = SpellIn(slot);
		const PlayerActions actions = world.ApplyPlayerInput(self, input);
		for (uint32_t slot = 0; slot < AbilitySlots; slot++)
			if (actions.HasReleased(slot) && size_t(spells[slot]) < tally.Casts.size())
				tally.Casts[size_t(spells[slot])]++;
		if (actions.Dodged)
			tally.Dodges++;
	}

	void BotSquad::Add(uint32_t playerId, const BotProfile& profile)
	{
		Remove(playerId);
		m_Brains.emplace_back(playerId, profile);
		m_Tallies.emplace_back();
	}

	void BotSquad::Remove(uint32_t playerId)
	{
		for (size_t i = 0; i < m_Brains.size(); i++)
			if (m_Brains[i].GetPlayerId() == playerId)
			{
				m_Brains.erase(m_Brains.begin() + ptrdiff_t(i));
				m_Tallies.erase(m_Tallies.begin() + ptrdiff_t(i));
				return;
			}
	}

	bool BotSquad::Contains(uint32_t playerId) const
	{
		return std::any_of(m_Brains.begin(), m_Brains.end(), [&](const BotBrain& brain) { return brain.GetPlayerId() == playerId; });
	}

	const BotTally* BotSquad::FindTally(uint32_t playerId) const
	{
		for (size_t i = 0; i < m_Brains.size(); i++)
			if (m_Brains[i].GetPlayerId() == playerId)
				return &m_Tallies[i];
		return nullptr;
	}

	void BotSquad::Update(World& world)
	{
		if (m_Brains.empty() || world.GetMode() != GameMode::Brawl)
			return;
		m_Arena.Update(world);
		for (size_t i = 0; i < m_Brains.size(); i++)
			m_Brains[i].Update(world, m_Arena, m_Tallies[i]);
	}
}
