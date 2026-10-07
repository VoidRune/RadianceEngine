#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <cmath>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr int32_t AshlandsSize = 128;
		constexpr float FaceSouth = 0.0f;
		constexpr float FaceNorth = Pi;
		constexpr float FaceEast = Pi * 0.5f;
		constexpr float FaceWest = -Pi * 0.5f;

		uint32_t Scatter(LevelBuilder& builder, const TileRect& area, PropId id, uint32_t count, float spacing, uint32_t attempts = 600)
		{
			Random& random = builder.GetRandom();
			uint32_t placed = 0;
			for (uint32_t attempt = 0; attempt < attempts && placed < count; attempt++)
			{
				const Vec2 position = { float(area.X) + random.Float() * float(area.Width), float(area.Y) + random.Float() * float(area.Height) };
				if (builder.TryProp(id, position, random.Float() * 2.0f * Pi, spacing))
					placed++;
			}
			return placed;
		}

		void ShapeIsland(LevelBuilder& builder)
		{
			builder.Disc({ 64.0f, 64.0f }, 56.0f, TerrainId::Grass);
			builder.Disc({ 34.0f, 96.0f }, 26.0f, TerrainId::Grass);
			builder.Disc({ 96.0f, 96.0f }, 25.0f, TerrainId::Grass);
			builder.Disc({ 96.0f, 32.0f }, 25.0f, TerrainId::Grass);
			builder.Disc({ 32.0f, 32.0f }, 25.0f, TerrainId::Grass);
			builder.Disc({ 64.0f, 3.0f }, 9.0f, TerrainId::Water);
			builder.Disc({ 3.0f, 64.0f }, 8.0f, TerrainId::Water);
			builder.Disc({ 125.0f, 70.0f }, 9.0f, TerrainId::Water);
			builder.Disc({ 60.0f, 126.0f }, 8.0f, TerrainId::Water);

			TileMap& map = builder.GetMap();
			std::vector<TileRect> beach;
			for (int32_t y = 0; y < map.GetHeight(); y++)
				for (int32_t x = 0; x < map.GetWidth(); x++)
				{
					if (map.GetTerrain(x, y) != TerrainId::Grass)
						continue;
					bool shore = false;
					for (int32_t dy = -2; dy <= 2 && !shore; dy++)
						for (int32_t dx = -2; dx <= 2 && !shore; dx++)
							shore = dx * dx + dy * dy <= 5 && map.GetTerrain(x + dx, y + dy) == TerrainId::Water;
					if (shore)
						beach.push_back({ x, y, 1, 1 });
				}
			for (const TileRect& tile : beach)
				builder.Fill(tile, TerrainId::Sand);
		}

		void BuildHill(LevelBuilder& builder)
		{
			builder.Fill({ 56, 56, 16, 16 }, TerrainId::Flagstone, 1.0f);
			builder.Fill({ 61, 61, 6, 6 }, TerrainId::Marble, 2.0f);
			builder.Stairs({ 62, 52, 4, 4 }, Direction::North, 0.0f, 1.0f, TerrainId::Cobblestone);
			builder.Stairs({ 62, 72, 4, 4 }, Direction::South, 0.0f, 1.0f, TerrainId::Cobblestone);
			builder.Stairs({ 52, 62, 4, 4 }, Direction::East, 0.0f, 1.0f, TerrainId::Cobblestone);
			builder.Stairs({ 72, 62, 4, 4 }, Direction::West, 0.0f, 1.0f, TerrainId::Cobblestone);
			builder.Stairs({ 63, 57, 2, 4 }, Direction::North, 1.0f, 2.0f, TerrainId::Marble);
			builder.Stairs({ 63, 67, 2, 4 }, Direction::South, 1.0f, 2.0f, TerrainId::Marble);
			for (const Vec2 pillar : { Vec2(61.6f, 61.6f), Vec2(66.4f, 61.6f), Vec2(61.6f, 66.4f), Vec2(66.4f, 66.4f) })
				builder.Prop(PropId::StonePillar, pillar);
			for (const Vec2 brazier : { Vec2(56.7f, 56.7f), Vec2(71.3f, 56.7f), Vec2(56.7f, 71.3f), Vec2(71.3f, 71.3f) })
				builder.Prop(PropId::Brazier, brazier);
			builder.Prop(PropId::Banner, { 64.0f, 66.5f }, FaceSouth);
			builder.LootSpot({ 64.0f, 64.0f });
			builder.LootSpot({ 58.5f, 68.5f });
			builder.LootSpot({ 69.5f, 59.5f });
			builder.Reserve({ 50, 50, 28, 28 });
			builder.Region("Watchtower Hill", { 50, 50, 28, 28 });
		}

		void BuildPaths(LevelBuilder& builder)
		{
			builder.Stroke({ 52.0f, 64.0f }, { 36.0f, 66.0f }, 2.4f, TerrainId::Dirt);
			builder.Stroke({ 64.0f, 76.0f }, { 64.0f, 92.0f }, 2.4f, TerrainId::Dirt);
			builder.Stroke({ 76.0f, 64.0f }, { 93.0f, 66.0f }, 2.4f, TerrainId::Dirt);
			builder.Stroke({ 64.0f, 52.0f }, { 64.0f, 34.0f }, 2.4f, TerrainId::Dirt);
			builder.Stroke({ 60.0f, 50.0f }, { 44.0f, 40.0f }, 2.2f, TerrainId::Dirt);
			builder.Stroke({ 68.0f, 50.0f }, { 88.0f, 36.0f }, 2.2f, TerrainId::Dirt);
			builder.Stroke({ 36.0f, 74.0f }, { 30.0f, 92.0f }, 2.0f, TerrainId::Dirt);
			builder.Stroke({ 76.0f, 104.0f }, { 92.0f, 98.0f }, 2.0f, TerrainId::Dirt);
		}

		void BuildVillage(LevelBuilder& builder)
		{
			builder.Disc({ 30.0f, 51.0f }, 4.0f, TerrainId::Ash);
			builder.Disc({ 12.0f, 74.0f }, 3.5f, TerrainId::Ash);
			builder.Disc({ 36.0f, 79.0f }, 3.0f, TerrainId::Ash);
			builder.Fill({ 16, 58, 20, 16 }, TerrainId::Cobblestone);
			builder.Prop(PropId::House, { 19.5f, 76.0f }, FaceSouth);
			builder.Prop(PropId::LongHouse, { 26.5f, 76.0f }, FaceSouth);
			builder.Prop(PropId::House, { 33.5f, 76.0f }, FaceSouth);
			builder.Prop(PropId::House, { 19.5f, 55.5f }, FaceNorth);
			builder.Prop(PropId::LongHouse, { 26.5f, 55.5f }, FaceNorth);
			builder.Prop(PropId::House, { 33.5f, 55.5f }, FaceNorth);
			builder.Prop(PropId::LongHouse, { 13.5f, 66.0f }, FaceEast);
			builder.Prop(PropId::Well, { 26.0f, 65.5f });
			builder.Prop(PropId::MarketStall, { 20.5f, 61.0f }, FaceNorth);
			builder.Prop(PropId::MarketStall, { 31.5f, 71.0f }, FaceSouth);
			builder.Prop(PropId::Cart, { 21.0f, 70.5f }, 0.4f);
			builder.Prop(PropId::Signpost, { 35.0f, 63.0f });
			for (const Vec2 lamp : { Vec2(17.0f, 59.0f), Vec2(35.0f, 59.0f), Vec2(17.0f, 73.0f), Vec2(35.0f, 73.0f) })
				builder.Prop(PropId::LampPost, lamp);
			builder.LootSpot({ 26.0f, 69.0f }, true);
			builder.LootSpot({ 23.0f, 63.5f });
			builder.LootSpot({ 31.5f, 61.5f });
			builder.LootSpot({ 18.0f, 67.5f });
			builder.LootSpot({ 26.5f, 80.5f });
			builder.LootSpot({ 26.5f, 51.0f });
			builder.Reserve({ 10, 50, 28, 32 });
			Scatter(builder, { 16, 58, 20, 16 }, PropId::Barrel, 6, 0.4f);
			Scatter(builder, { 16, 58, 20, 16 }, PropId::Crate, 4, 0.4f);
			Scatter(builder, { 10, 50, 28, 32 }, PropId::Rubble, 10, 0.2f);
			builder.Region("Cinder Village", { 10, 50, 28, 32 });
		}

		void BuildChapel(LevelBuilder& builder)
		{
			const Vec2 center = { 64.0f, 104.0f };
			builder.Disc(center, 11.0f, TerrainId::Water);
			builder.Disc(center, 5.5f, TerrainId::Marble);
			builder.Stroke({ 64.0f, 92.0f }, { 64.0f, 99.0f }, 2.2f, TerrainId::WoodFloor);
			builder.Stroke({ 76.0f, 104.0f }, { 69.0f, 104.0f }, 2.2f, TerrainId::WoodFloor);
			builder.Wall(WallId::WoodRailing, { 63.05f, 92.6f }, { 63.05f, 98.4f });
			builder.Wall(WallId::WoodRailing, { 64.95f, 92.6f }, { 64.95f, 98.4f });
			builder.Wall(WallId::WoodRailing, { 69.6f, 103.05f }, { 75.4f, 103.05f });
			builder.Wall(WallId::WoodRailing, { 69.6f, 104.95f }, { 75.4f, 104.95f });
			builder.Prop(PropId::Statue, { 64.0f, 107.6f }, FaceSouth);
			for (int32_t i = 0; i < 6; i++)
			{
				const float angle = Pi * (0.15f + 0.14f * float(i));
				builder.Prop(i % 2 == 0 ? PropId::BrokenPillar : PropId::StonePillar, center + FromAngle(angle) * 4.4f, angle);
			}
			builder.Prop(PropId::Candelabra, { 61.6f, 106.2f });
			builder.Prop(PropId::Candelabra, { 66.4f, 106.2f });
			builder.LootSpot({ 64.0f, 103.2f }, true);
			builder.LootSpot({ 52.0f, 97.0f });
			builder.LootSpot({ 77.0f, 111.0f });
			builder.LootSpot({ 56.0f, 115.0f });
			builder.Reserve({ 50, 90, 28, 28 });
			Scatter(builder, { 50, 90, 28, 28 }, PropId::Bush, 10, 0.4f);
			Scatter(builder, { 50, 90, 28, 28 }, PropId::Flowers, 10, 0.2f);
			builder.Region("Drowned Chapel", { 50, 90, 28, 28 });
		}

		void BuildGraveyard(LevelBuilder& builder)
		{
			builder.Fill({ 93, 58, 18, 16 }, TerrainId::Dirt);
			builder.Wall(WallId::IronFence, { 93.0f, 58.0f }, { 111.0f, 58.0f });
			builder.Wall(WallId::IronFence, { 111.0f, 58.0f }, { 111.0f, 74.0f });
			builder.Wall(WallId::IronFence, { 93.0f, 74.0f }, { 100.5f, 74.0f });
			builder.Wall(WallId::IronFence, { 103.5f, 74.0f }, { 111.0f, 74.0f });
			builder.Wall(WallId::IronFence, { 93.0f, 58.0f }, { 93.0f, 64.5f });
			builder.Wall(WallId::IronFence, { 93.0f, 67.5f }, { 93.0f, 74.0f });
			builder.Prop(PropId::House, { 106.5f, 66.0f }, FaceWest);
			for (const float x : { 96.0f, 98.5f, 101.0f, 103.5f })
				for (const float y : { 60.5f, 63.0f, 69.0f, 71.5f })
					builder.TryProp(PropId::Gravestone, { x, y }, FaceSouth, 0.1f);
			for (const float y : { 60.5f, 71.5f })
				builder.TryProp(PropId::Gravestone, { 108.5f, y }, FaceSouth, 0.1f);
			builder.Prop(PropId::Statue, { 96.0f, 66.0f }, FaceEast);
			builder.LootSpot({ 101.5f, 66.0f }, true);
			builder.LootSpot({ 95.5f, 72.5f });
			builder.LootSpot({ 109.5f, 59.6f });
			builder.LootSpot({ 114.0f, 66.0f });
			builder.Reserve({ 90, 54, 26, 24 });
			Scatter(builder, { 93, 58, 18, 16 }, PropId::Bones, 6, 0.2f);
			Scatter(builder, { 88, 52, 30, 28 }, PropId::PineTree, 8, 1.2f);
			builder.Region("Hollow Graveyard", { 90, 54, 26, 24 });
		}

		void BuildHollow(LevelBuilder& builder)
		{
			builder.Disc({ 98.0f, 30.0f }, 13.0f, TerrainId::CaveFloor);
			builder.Fill({ 96, 27, 6, 6 }, TerrainId::CaveFloor, 1.0f);
			builder.Stairs({ 97, 23, 3, 4 }, Direction::North, 0.0f, 1.0f, TerrainId::CaveFloor);
			builder.Fill({ 89, 33, 3, 2 }, TerrainId::Rock);
			builder.Fill({ 104, 24, 2, 4 }, TerrainId::Rock);
			builder.Fill({ 90, 25, 3, 2 }, TerrainId::Rock);
			builder.Fill({ 103, 35, 3, 3 }, TerrainId::Rock);
			for (const Vec2 crystal : { Vec2(96.6f, 32.4f), Vec2(101.4f, 32.4f), Vec2(101.4f, 27.6f), Vec2(88.5f, 30.0f), Vec2(106.5f, 30.5f), Vec2(98.0f, 39.5f) })
				builder.Prop(PropId::Crystal, crystal, crystal.x);
			builder.LootSpot({ 99.0f, 30.0f }, true);
			builder.LootSpot({ 92.0f, 28.5f });
			builder.LootSpot({ 106.0f, 33.5f });
			builder.LootSpot({ 104.0f, 20.0f });
			builder.Reserve({ 84, 16, 28, 28 });
			Scatter(builder, { 86, 18, 24, 24 }, PropId::Crystal, 10, 0.6f);
			Scatter(builder, { 86, 18, 24, 24 }, PropId::Mushrooms, 12, 0.2f);
			Scatter(builder, { 86, 18, 24, 24 }, PropId::Boulder, 4, 1.4f);
			builder.Region("Crystal Hollow", { 84, 16, 28, 28 });
		}

		void BuildLibrary(LevelBuilder& builder)
		{
			builder.Fill({ 20, 22, 22, 16 }, TerrainId::Marble, 1.0f);
			builder.Fill({ 22, 24, 18, 12 }, TerrainId::WoodFloor);
			builder.Stairs({ 29, 38, 4, 4 }, Direction::South, 0.0f, 1.0f, TerrainId::Marble);
			builder.Stairs({ 42, 28, 4, 4 }, Direction::West, 0.0f, 1.0f, TerrainId::Marble);
			builder.Wall(WallId::Library, { 22.0f, 24.0f }, { 30.0f, 24.0f });
			builder.Wall(WallId::Library, { 33.0f, 24.0f }, { 40.0f, 24.0f });
			builder.Wall(WallId::Library, { 22.0f, 24.0f }, { 22.0f, 29.0f });
			builder.Wall(WallId::Library, { 22.0f, 32.0f }, { 22.0f, 36.0f });
			builder.Wall(WallId::Library, { 22.0f, 36.0f }, { 28.0f, 36.0f });
			builder.Wall(WallId::Library, { 34.0f, 36.0f }, { 40.0f, 36.0f });
			builder.Wall(WallId::Library, { 40.0f, 24.0f }, { 40.0f, 27.0f });
			builder.Wall(WallId::Library, { 40.0f, 32.5f }, { 40.0f, 36.0f });
			for (const Vec2 shelf : { Vec2(26.0f, 27.5f), Vec2(26.0f, 32.5f), Vec2(35.5f, 27.5f), Vec2(35.5f, 32.5f) })
				builder.Prop(PropId::DoubleBookshelf, shelf);
			builder.Prop(PropId::ReadingTable, { 30.5f, 29.5f }, FaceEast);
			builder.Prop(PropId::Globe, { 33.0f, 34.6f });
			builder.Prop(PropId::Lectern, { 30.5f, 26.0f });
			builder.Prop(PropId::Candelabra, { 23.4f, 25.4f });
			builder.Prop(PropId::Candelabra, { 38.6f, 34.6f });
			for (const Vec2 pillar : { Vec2(20.7f, 22.7f), Vec2(41.3f, 22.7f), Vec2(20.7f, 37.3f) })
				builder.Prop(PropId::BrokenPillar, pillar, pillar.x);
			builder.LootSpot({ 30.5f, 33.0f }, true);
			builder.LootSpot({ 24.0f, 34.6f });
			builder.LootSpot({ 38.0f, 25.6f });
			builder.LootSpot({ 17.0f, 40.0f });
			builder.LootSpot({ 46.0f, 21.0f });
			builder.Reserve({ 16, 18, 32, 26 });
			Scatter(builder, { 16, 18, 32, 26 }, PropId::Rubble, 8, 0.2f);
			builder.Region("Sunken Library", { 16, 18, 32, 26 });
		}

		void BuildFarm(LevelBuilder& builder)
		{
			builder.Fill({ 54, 16, 8, 10 }, TerrainId::Dirt);
			builder.Fill({ 67, 16, 8, 10 }, TerrainId::Dirt);
			builder.Wall(WallId::WoodFence, { 54.0f, 16.0f }, { 62.0f, 16.0f });
			builder.Wall(WallId::WoodFence, { 54.0f, 16.0f }, { 54.0f, 26.0f });
			builder.Wall(WallId::WoodFence, { 54.0f, 26.0f }, { 59.0f, 26.0f });
			builder.Wall(WallId::WoodFence, { 67.0f, 16.0f }, { 75.0f, 16.0f });
			builder.Wall(WallId::WoodFence, { 75.0f, 16.0f }, { 75.0f, 26.0f });
			builder.Wall(WallId::WoodFence, { 70.0f, 26.0f }, { 75.0f, 26.0f });
			builder.Prop(PropId::House, { 57.5f, 31.5f }, FaceSouth);
			builder.Prop(PropId::LongHouse, { 71.0f, 31.5f }, FaceSouth);
			builder.Prop(PropId::Cart, { 64.0f, 20.0f }, 0.3f);
			builder.Prop(PropId::CrateStack, { 61.0f, 28.0f });
			builder.Prop(PropId::Well, { 64.5f, 26.5f });
			builder.LootSpot({ 64.0f, 16.5f });
			builder.LootSpot({ 52.0f, 27.0f });
			builder.LootSpot({ 77.0f, 22.5f });
			builder.Reserve({ 48, 12, 32, 24 });
			Scatter(builder, { 54, 16, 8, 10 }, PropId::Flowers, 6, 0.3f);
			Scatter(builder, { 67, 16, 8, 10 }, PropId::Mushrooms, 4, 0.3f);
			Scatter(builder, { 48, 12, 32, 24 }, PropId::Barrel, 5, 0.6f);
			builder.Region("Ashen Farm", { 48, 10, 32, 26 });
		}

		void BuildWoods(LevelBuilder& builder)
		{
			const Vec2 circle = { 32.0f, 98.0f };
			builder.Disc(circle, 4.6f, TerrainId::Dirt);
			for (int32_t i = 0; i < 6; i++)
				builder.Prop(i == 2 ? PropId::BrokenPillar : PropId::StonePillar, circle + FromAngle(float(i) * Pi / 3.0f + 0.3f) * 4.0f, float(i));
			builder.LootSpot(circle);
			builder.LootSpot({ 22.0f, 88.0f });
			builder.LootSpot({ 42.0f, 110.0f });
			builder.Reserve(circle, 5.0f);

			const Vec2 shrine = { 96.0f, 98.0f };
			builder.Disc(shrine, 4.0f, TerrainId::Flagstone);
			builder.Prop(PropId::Statue, shrine + Vec2(0.0f, 2.2f), FaceSouth);
			builder.Prop(PropId::Brazier, shrine + Vec2(-2.4f, 1.6f));
			builder.Prop(PropId::Brazier, shrine + Vec2(2.4f, 1.6f));
			builder.LootSpot(shrine + Vec2(0.0f, -0.6f));
			builder.LootSpot({ 86.0f, 108.0f });
			builder.LootSpot({ 106.0f, 88.0f });
			builder.Reserve(shrine, 4.6f);

			Scatter(builder, { 12, 80, 40, 40 }, PropId::OakTree, 50, 1.5f, 1600);
			Scatter(builder, { 12, 80, 40, 40 }, PropId::PineTree, 25, 1.5f, 1200);
			Scatter(builder, { 12, 80, 40, 40 }, PropId::FallenLog, 5, 1.0f);
			Scatter(builder, { 12, 80, 40, 40 }, PropId::Bush, 30, 0.4f);
			Scatter(builder, { 78, 80, 40, 40 }, PropId::PineTree, 55, 1.5f, 1600);
			Scatter(builder, { 78, 80, 40, 40 }, PropId::Boulder, 8, 1.2f);
			Scatter(builder, { 78, 80, 40, 40 }, PropId::Rock, 12, 0.6f);
			Scatter(builder, { 78, 80, 40, 40 }, PropId::Bush, 20, 0.4f);
			builder.Region("Whisperwood", { 12, 80, 40, 40 });
			builder.Region("Thornwood", { 78, 80, 40, 40 });
		}

		void PlaceWildSpots(LevelBuilder& builder)
		{
			for (const Vec2 spot : { Vec2(44.0f, 56.0f), Vec2(84.0f, 75.0f), Vec2(78.0f, 42.0f), Vec2(48.0f, 88.0f), Vec2(84.0f, 52.0f), Vec2(42.0f, 72.0f), Vec2(118.0f, 98.0f), Vec2(10.0f, 40.0f) })
				builder.LootSpot(spot);
			for (const Vec2 spawn : { Vec2(46.0f, 84.0f), Vec2(82.0f, 84.0f), Vec2(84.0f, 46.0f), Vec2(46.0f, 46.0f), Vec2(12.0f, 52.0f), Vec2(52.0f, 118.0f), Vec2(114.0f, 50.0f), Vec2(76.0f, 116.0f),
				Vec2(16.0f, 112.0f), Vec2(112.0f, 112.0f), Vec2(112.0f, 16.0f), Vec2(16.0f, 16.0f), Vec2(44.0f, 12.0f), Vec2(86.0f, 12.0f), Vec2(14.0f, 80.0f), Vec2(114.0f, 84.0f) })
				builder.SpawnPoint(spawn);
		}

		void BuildWilds(LevelBuilder& builder)
		{
			const TileRect island = { 6, 6, 116, 116 };
			Scatter(builder, island, PropId::OakTree, 70, 1.8f, 3000);
			Scatter(builder, island, PropId::PineTree, 45, 1.8f, 2500);
			Scatter(builder, island, PropId::Boulder, 14, 1.4f, 1500);
			Scatter(builder, island, PropId::Rock, 30, 0.6f, 1500);
			Scatter(builder, island, PropId::FallenLog, 10, 1.0f, 1500);
			Scatter(builder, island, PropId::Bush, 60, 0.4f, 2500);
			Scatter(builder, island, PropId::Flowers, 50, 0.2f, 2500);
			builder.Region("the Ashlands", { 0, 0, AshlandsSize, AshlandsSize });
		}
	}

	Level BuildAshlandsLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::Ashlands, seed, AshlandsSize, AshlandsSize, TerrainId::Water);
		ShapeIsland(builder);
		BuildPaths(builder);
		PlaceWildSpots(builder);
		BuildHill(builder);
		BuildVillage(builder);
		BuildChapel(builder);
		BuildGraveyard(builder);
		BuildHollow(builder);
		BuildLibrary(builder);
		BuildFarm(builder);
		BuildWoods(builder);
		BuildWilds(builder);
		builder.SetPlayerSpawn({ 64.0f, 48.0f });
		return builder.Finish();
	}
}
