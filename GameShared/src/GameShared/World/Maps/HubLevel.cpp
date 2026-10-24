#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr int32_t HubWidth = 48;
		constexpr int32_t HubHeight = 42;
		constexpr float FaceSouth = 0.0f;
		constexpr float FaceEast = Pi * 0.5f;
		constexpr float FaceNorth = Pi;
		constexpr Vec2 PlazaCenter = { 21.0f, 20.5f };
		constexpr Vec2 WellCenter = { 33.5f, 17.5f };
		constexpr Vec2 LoftCenter = { 31.5f, 28.5f };
		constexpr Vec2 GateCenter = { 21.0f, 37.0f };
		constexpr Vec2 ObservatoryCenter = { 41.0f, 32.5f };
		constexpr Vec2 ProvingCenter = { 38.5f, 7.0f };
		constexpr Vec2 ShrineCenter = { 21.0f, 4.2f };
		constexpr Vec2 CatCenter = { 45.6f, 24.0f };
		constexpr Vec2 BoardSpot = { 26.2f, 12.4f };
		constexpr Vec2 WishSpot = { 22.2f, 33.0f };
		constexpr Vec2 ChessCenter = { 32.8f, 25.3f };
		constexpr float ConstellationReach = 6.6f;
		constexpr AbilityId Constellations[] = { AbilityId::EmberBolt, AbilityId::Gust, AbilityId::Comet, AbilityId::Warp, AbilityId::Shockwave, AbilityId::GravityWell, AbilityId::ArcLightning, AbilityId::Swap, AbilityId::MirrorWard, AbilityId::Frostbite };
		constexpr Vec2 ShrineTethers[][2] = { { { 19.6f, 9.45f }, { 19.7f, 6.5f } }, { { 21.2f, 9.45f }, { 21.9f, 6.5f } }, { { 12.2f, 9.4f }, { 18.6f, 4.4f } } };
		constexpr Vec2 PierRails[][5] = { { { 19.0f, 27.0f }, { 19.15f, 29.2f }, { 19.15f, 31.4f }, { 19.15f, 33.6f }, { 19.15f, 35.6f } }, { { 23.0f, 27.0f }, { 22.85f, 29.2f }, { 22.85f, 31.4f } } };
		constexpr size_t PierRailPosts[] = { 5, 3 };

		void Set(TileMap& map, int32_t x, int32_t y, TerrainId terrain, float height)
		{
			if (map.IsInside(x, y))
				map.At(x, y) = { terrain, Direction::None, ToLevel(height), 0 };
		}

		void Blob(LevelBuilder& builder, Vec2 center, float radius, float wobble, float phase, TerrainId terrain, float height)
		{
			TileMap& map = builder.GetMap();
			const float reach = radius * (1.0f + wobble);
			for (int32_t y = int32_t(std::floor(center.y - reach)); y <= int32_t(std::floor(center.y + reach)); y++)
				for (int32_t x = int32_t(std::floor(center.x - reach)); x <= int32_t(std::floor(center.x + reach)); x++)
				{
					const Vec2 offset = Vec2(float(x) + 0.5f, float(y) + 0.5f) - center;
					const float angle = AngleOf(offset);
					const float edge = radius * (1.0f + wobble * (0.6f * std::sin(3.0f * angle + phase) + 0.4f * std::sin(5.0f * angle + 2.3f * phase)));
					if (Length(offset) <= edge)
						Set(map, x, y, terrain, height);
				}
		}

		void Paint(LevelBuilder& builder, Vec2 center, float radius, TerrainId terrain, float height)
		{
			TileMap& map = builder.GetMap();
			for (int32_t y = int32_t(std::floor(center.y - radius)); y <= int32_t(std::floor(center.y + radius)); y++)
				for (int32_t x = int32_t(std::floor(center.x - radius)); x <= int32_t(std::floor(center.x + radius)); x++)
					if (map.IsInside(x, y) && map.GetTerrain(x, y) != TerrainId::Void && LengthSquared(Vec2(float(x) + 0.5f, float(y) + 0.5f) - center) <= radius * radius)
						Set(map, x, y, terrain, height);
		}

		void Paint(LevelBuilder& builder, const TileRect& area, TerrainId terrain, float height)
		{
			TileMap& map = builder.GetMap();
			for (int32_t y = area.Y; y < area.Top(); y++)
				for (int32_t x = area.X; x < area.Right(); x++)
					if (map.IsInside(x, y) && map.GetTerrain(x, y) != TerrainId::Void)
						Set(map, x, y, terrain, height);
		}

		void Bridge(LevelBuilder& builder, Vec2 from, Vec2 to, float width, TerrainId terrain, float height)
		{
			TileMap& map = builder.GetMap();
			const float half = width * 0.5f;
			for (int32_t y = int32_t(std::floor(std::min(from.y, to.y) - half)); y <= int32_t(std::floor(std::max(from.y, to.y) + half)); y++)
				for (int32_t x = int32_t(std::floor(std::min(from.x, to.x) - half)); x <= int32_t(std::floor(std::max(from.x, to.x) + half)); x++)
					if (PointSegmentDistance({ float(x) + 0.5f, float(y) + 0.5f }, from, to) <= half)
						Set(map, x, y, terrain, height);
		}

		void Starpath(LevelBuilder& builder, Vec2 from, Vec2 to, float width)
		{
			TileMap& map = builder.GetMap();
			const float half = width * 0.5f;
			for (int32_t y = int32_t(std::floor(std::min(from.y, to.y) - half)); y <= int32_t(std::floor(std::max(from.y, to.y) + half)); y++)
				for (int32_t x = int32_t(std::floor(std::min(from.x, to.x) - half)); x <= int32_t(std::floor(std::max(from.x, to.x) + half)); x++)
					if (map.IsInside(x, y) && map.GetTerrain(x, y) == TerrainId::Void && PointSegmentDistance({ float(x) + 0.5f, float(y) + 0.5f }, from, to) <= half)
						Set(map, x, y, TerrainId::Starpath, 0.0f);
		}

		void Ring(LevelBuilder& builder, const TileRect& area, PropId id, uint32_t count, float spacing)
		{
			Random& random = builder.GetRandom();
			uint32_t placed = 0;
			for (uint32_t attempt = 0; attempt < 600 && placed < count; attempt++)
			{
				const Vec2 position = { float(area.X) + random.Float() * float(area.Width), float(area.Y) + random.Float() * float(area.Height) };
				if (builder.TryProp(id, position, random.Float() * 2.0f * Pi, spacing))
					placed++;
			}
		}
	}

	Level BuildHubLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::Hub, seed, HubWidth, HubHeight, TerrainId::Void);
		Blob(builder, { 21.0f, 20.0f }, 11.0f, 0.08f, 0.7f, TerrainId::Grass, 0.5f);
		Blob(builder, { 11.5f, 27.5f }, 5.4f, 0.12f, 2.6f, TerrainId::Grass, 0.5f);
		Blob(builder, { 10.8f, 15.6f }, 7.0f, 0.08f, 1.9f, TerrainId::Grass, 0.25f);
		Blob(builder, WellCenter, 7.4f, 0.06f, 4.1f, TerrainId::Grass, 0.0f);
		Blob(builder, LoftCenter, 5.6f, 0.08f, 5.3f, TerrainId::Grass, 1.5f);
		Paint(builder, PlazaCenter, 6.0f, TerrainId::Cobblestone, 0.5f);
		Paint(builder, { 5, 11, 11, 8 }, TerrainId::Cobblestone, 0.25f);
		Paint(builder, WellCenter, 6.0f, TerrainId::Cobblestone, 0.0f);
		builder.Disc(WellCenter, 4.4f, TerrainId::Lava);
		builder.Disc(WellCenter, 2.2f, TerrainId::Basalt);
		Paint(builder, LoftCenter, 4.4f, TerrainId::Flagstone, 1.5f);
		Paint(builder, { 29, 23, 7, 1 }, TerrainId::Grass, 1.5f);
		Paint(builder, { 26, 26, 1, 1 }, TerrainId::Grass, 1.5f);
		Bridge(builder, { 21.0f, 25.5f }, { 21.0f, 34.5f }, 3.0f, TerrainId::Flagstone, 0.25f);
		Blob(builder, GateCenter, 2.8f, 0.0f, 0.0f, TerrainId::Flagstone, 0.25f);
		builder.Stairs({ 25, 16, 2, 4 }, Direction::West, 0.0f, 0.5f, TerrainId::Dirt);
		builder.Stairs({ 25, 23, 4, 3 }, Direction::East, 0.5f, 1.5f, TerrainId::Dirt);
		Blob(builder, ObservatoryCenter, 2.8f, 0.1f, 3.3f, TerrainId::Grass, 2.5f);
		Paint(builder, ObservatoryCenter + Vec2(0.2f, 0.4f), 1.7f, TerrainId::Flagstone, 2.5f);
		builder.Stairs({ 36, 30, 4, 2 }, Direction::East, 1.5f, 2.5f, TerrainId::Dirt);
		Blob(builder, ProvingCenter, 3.0f, 0.1f, 1.1f, TerrainId::Grass, 0.0f);
		Paint(builder, ProvingCenter, 2.1f, TerrainId::Flagstone, 0.0f);
		Bridge(builder, { 35.0f, 12.0f }, { 37.2f, 9.2f }, 2.2f, TerrainId::Dirt, 0.0f);
		Blob(builder, ShrineCenter, 2.7f, 0.1f, 2.2f, TerrainId::Grass, -2.5f);
		Paint(builder, ShrineCenter + Vec2(0.0f, 0.4f), 1.6f, TerrainId::Flagstone, -2.5f);
		Blob(builder, CatCenter, 1.6f, 0.1f, 0.4f, TerrainId::Grass, 0.0f);
		Starpath(builder, { 39.4f, 20.4f }, { 42.0f, 21.0f }, 1.6f);
		Starpath(builder, { 42.0f, 21.0f }, { 44.6f, 23.6f }, 1.6f);
		builder.Stroke({ 18.0f, 15.0f }, { 13.5f, 13.6f }, 1.6f, TerrainId::Dirt);
		builder.Stroke({ 16.2f, 24.0f }, { 12.0f, 26.6f }, 1.5f, TerrainId::Dirt);
		builder.SetRounded();
		builder.SetPlayerSpawn({ 21.0f, 14.8f });

		builder.Prop(PropId::Fountain, PlazaCenter);
		builder.Prop(PropId::Bench, PlazaCenter + Vec2(-3.6f, 0.0f), FaceEast);
		builder.Prop(PropId::Bench, PlazaCenter + Vec2(3.6f, 0.0f), -FaceEast);
		for (const Vec2 lamp : { Vec2(17.2f, 24.0f), Vec2(24.4f, 22.5f), Vec2(17.2f, 16.8f), Vec2(24.6f, 15.3f), Vec2(19.0f, 27.0f), Vec2(23.0f, 27.0f), Vec2(14.4f, 24.4f) })
			builder.Prop(PropId::LampPost, lamp);
		builder.Prop(PropId::Signpost, { 19.0f, 15.6f });
		builder.Reserve(PlazaCenter, 6.2f);

		builder.Prop(PropId::NoticeBoard, BoardSpot);
		builder.Dummy({ 22.4f, 10.1f }, { 22.4f, 10.1f });
		builder.Dummy({ 24.8f, 11.0f }, { 24.8f, 11.0f });
		builder.Reserve({ 21, 9, 6, 5 });

		builder.Prop(PropId::OfferingBowl, WishSpot + Vec2(0.42f, 0.0f));
		builder.Prop(PropId::CandleStand, WishSpot + Vec2(0.45f, 1.05f));
		builder.Station(StationKind::WishingVoid, WishSpot, 1.2f);

		builder.Prop(PropId::CalderaGate, GateCenter + Vec2(0.0f, 1.0f), FaceSouth);
		builder.Prop(PropId::FireBowl, GateCenter + Vec2(-2.2f, 0.4f));
		builder.Prop(PropId::FireBowl, GateCenter + Vec2(2.2f, 0.4f));
		builder.Station(StationKind::CalderaGate, GateCenter + Vec2(0.0f, -1.2f), 2.6f);
		builder.Reserve(GateCenter, 3.4f);
		builder.Reserve({ 19, 25, 5, 10 });
		for (size_t rail = 0; rail < std::size(PierRails); rail++)
			for (size_t post = 1; post < PierRailPosts[rail]; post++)
				builder.Prop(PropId::PierPost, PierRails[rail][post]);
		for (const auto& [high, low] : ShrineTethers)
		{
			const Vec2 toward = Normalize(low - high);
			builder.Prop(PropId::ChainAnchor, high, std::atan2(toward.x, -toward.y));
			builder.Prop(PropId::ChainAnchor, low, std::atan2(-toward.x, toward.y));
		}

		builder.Prop(PropId::StashChest, { 6.4f, 15.4f }, FaceEast);
		builder.Prop(PropId::Barrel, { 5.8f, 13.8f });
		builder.Prop(PropId::CrateStack, { 6.2f, 17.2f });
		builder.Prop(PropId::Banner, { 5.5f, 16.4f }, FaceSouth);
		builder.Station(StationKind::Stash, { 7.4f, 15.4f }, 2.4f);

		builder.Prop(PropId::Forge, { 8.2f, 11.6f }, FaceNorth);
		builder.Prop(PropId::Anvil, { 10.6f, 12.2f }, FaceSouth);
		builder.Prop(PropId::Brazier, { 6.6f, 12.4f });
		builder.Station(StationKind::Forge, { 8.6f, 13.4f }, 2.6f);

		builder.Prop(PropId::GemAltar, { 13.4f, 17.2f });
		for (const Vec2 crystal : { Vec2(12.2f, 18.4f), Vec2(14.8f, 18.5f), Vec2(15.2f, 16.0f) })
			builder.Prop(PropId::Crystal, crystal, crystal.x);
		builder.Station(StationKind::Lapidary, { 13.2f, 15.8f }, 2.4f);

		builder.Prop(PropId::MissionBoard, { 14.2f, 12.0f }, FaceNorth);
		builder.Prop(PropId::Brazier, { 12.6f, 11.8f });
		builder.Prop(PropId::Brazier, { 15.8f, 12.2f });
		builder.Station(StationKind::MissionBoard, { 14.2f, 13.8f }, 2.6f);
		builder.Reserve({ 5, 11, 11, 8 });

		builder.Prop(PropId::Lectern, LoftCenter + Vec2(1.3f, 0.3f), FaceEast);
		builder.Prop(PropId::CandleStand, LoftCenter + Vec2(1.1f, 1.7f));
		builder.Prop(PropId::Bookshelf, LoftCenter + Vec2(-2.4f, 2.9f), FaceSouth);
		builder.Prop(PropId::Bookshelf, LoftCenter + Vec2(-0.6f, 3.4f), FaceSouth);
		builder.Prop(PropId::ReadingTable, LoftCenter + Vec2(3.3f, -0.6f));
		builder.Prop(PropId::Brazier, LoftCenter + Vec2(-3.0f, -2.4f));
		builder.Station(StationKind::Spellbook, LoftCenter + Vec2(0.3f, 0.3f), 2.0f);
		builder.Prop(PropId::ChessTable, ChessCenter);
		builder.Station(StationKind::Chess, ChessCenter, 2.5f);
		builder.Reserve(LoftCenter, 4.6f);

		builder.Dummy(WellCenter + Vec2(-1.0f, 1.3f), WellCenter + Vec2(-1.0f, 1.3f));
		builder.Dummy(WellCenter + Vec2(1.0f, 1.3f), WellCenter + Vec2(1.0f, 1.3f));
		builder.Dummy(WellCenter + Vec2(0.0f, -1.7f), WellCenter + Vec2(0.0f, -1.7f));
		builder.Dummy(WellCenter + Vec2(-1.7f, -0.2f), WellCenter + Vec2(1.7f, -0.2f));
		builder.Dummy(WellCenter + Vec2(3.1f, -2.8f), WellCenter + Vec2(3.1f, -2.8f), true);
		for (int32_t i = 0; i < 32; i++)
		{
			const float angle = 2.0f * Pi * (float(i) + 0.5f) / 32.0f;
			if (std::abs(std::remainder(angle - Pi, 2.0f * Pi)) < 0.24f)
				continue;
			const float radius = 4.75f + 0.2f * std::sin(float(i) * 2.7f);
			builder.Prop(PropId::Rock, WellCenter + FromAngle(angle) * radius, angle * 2.3f);
		}
		builder.Reserve(WellCenter, 6.2f);

		builder.Prop(PropId::Globe, ObservatoryCenter + Vec2(0.4f, 0.6f));
		builder.Prop(PropId::Telescope, ObservatoryCenter + Vec2(1.6f, -0.1f), FaceEast + 0.3f);
		builder.Prop(PropId::CandleStand, ObservatoryCenter + Vec2(-1.2f, 1.2f));
		builder.Prop(PropId::Flowers, ObservatoryCenter + Vec2(1.6f, -1.0f));
		builder.Reserve(ObservatoryCenter, 2.4f);

		builder.Prop(PropId::Statue, ProvingCenter + Vec2(2.2f, 0.2f), -FaceEast);
		builder.Prop(PropId::Banner, ProvingCenter + Vec2(2.0f, 1.6f), -0.5f * FaceEast);
		builder.Prop(PropId::Banner, ProvingCenter + Vec2(2.2f, -1.4f), -0.5f * FaceEast);
		builder.Station(StationKind::ProvingGrounds, ProvingCenter + Vec2(-0.1f, 0.0f), 2.6f);
		builder.Reserve(ProvingCenter, 3.2f);

		builder.Prop(PropId::Statue, ShrineCenter + Vec2(0.0f, 1.4f), FaceSouth);
		builder.Prop(PropId::FireBowl, ShrineCenter + Vec2(-1.8f, 0.8f));
		builder.Prop(PropId::FireBowl, ShrineCenter + Vec2(1.8f, 0.8f));
		builder.Prop(PropId::Flowers, ShrineCenter + Vec2(-1.6f, -1.0f));
		builder.Prop(PropId::Flowers, ShrineCenter + Vec2(1.6f, -1.0f));
		builder.Prop(PropId::MossBoulder, { 16.2f, 11.4f });
		builder.Prop(PropId::Crystal, { 18.9f, 10.4f }, 0.6f);
		builder.Prop(PropId::Mushrooms, { 16.6f, 12.6f });
		builder.Portal({ 17.6f, 10.6f }, ShrineCenter + Vec2(0.0f, -0.4f), 0.0f, "Ember Shrine", true);
		builder.Portal(ShrineCenter + Vec2(1.9f, -0.7f), { 21.0f, 14.8f }, Pi, "plaza");
		builder.Region("Ember Shrine", { 18, 1, 6, 7 }, false);
		builder.Reserve({ 17, 0, 8, 8 });

		builder.Prop(PropId::Bench, CatCenter + Vec2(0.2f, 0.5f), -FaceEast);
		builder.Prop(PropId::Flowers, CatCenter + Vec2(-0.6f, -0.8f));
		builder.Reserve(CatCenter, 2.0f);
		builder.Prop(PropId::LampPost, { 39.1f, 21.6f });
		builder.Reserve({ 37, 19, 4, 4 });

		builder.Prop(PropId::MageTower, { 10.5f, 28.0f }, FaceSouth);
		Ring(builder, { 4, 20, 9, 11 }, PropId::OakTree, 4, 1.4f);
		Ring(builder, { 37, 14, 3, 5 }, PropId::PineTree, 2, 1.4f);
		Ring(builder, { 3, 8, 40, 30 }, PropId::Bush, 7, 0.4f);
		Ring(builder, { 3, 8, 40, 30 }, PropId::Flowers, 14, 0.2f);
		builder.Region("Emberhold", { 0, 0, HubWidth, HubHeight }, false);

		Level level = builder.Finish();
		level.Floating = true;
		level.Railings = { { 25, 22, 13, 13 } };
		level.ChessTable = ChessSpot{ ChessCenter, 0.0f };
		for (const auto& [high, low] : ShrineTethers)
		{
			const Vec2 toward = Normalize(low - high);
			level.Tethers.push_back({ high + toward * 0.36f, low - toward * 0.36f, 0.5f, 0.19f, 1.5f });
		}
		for (size_t rail = 0; rail < std::size(PierRails); rail++)
			for (size_t post = 1; post < PierRailPosts[rail]; post++)
				level.Tethers.push_back({ PierRails[rail][post - 1], PierRails[rail][post], 0.22f, 0.76f, 0.75f });
		level.Landmarks = {
			{ LandmarkKind::FallBoard, BoardSpot, 9.0f },
			{ LandmarkKind::Observatory, ObservatoryCenter, 3.2f },
			{ LandmarkKind::CatIslet, CatCenter, 1.9f },
		};
		const float span = 235.0f * Pi / 180.0f;
		for (size_t i = 0; i < std::size(Constellations); i++)
		{
			const float angle = 0.75f * Pi - span * float(i) / float(std::size(Constellations) - 1);
			level.Constellations.push_back({ Constellations[i], ObservatoryCenter + FromAngle(angle) * ConstellationReach, 2.5f, 1.3f });
		}
		level.DriftLanes = {
			{ { 1.0f, 36.2f }, { 47.0f, 36.8f } },
			{ { 1.0f, 7.6f }, { 34.0f, 7.9f } },
			{ { 43.4f, 1.0f }, { 43.2f, 18.6f } },
			{ { 2.0f, 41.0f }, { 2.0f, 6.0f } },
		};
		return level;
	}
}
