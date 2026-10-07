#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr int32_t TownSize = 96;
		constexpr float FaceSouth = 0.0f;
		constexpr float FaceNorth = Pi;
		constexpr float FaceEast = Pi * 0.5f;
		constexpr float FaceWest = -Pi * 0.5f;
		constexpr float TerraceHeight = 1.0f;
		constexpr float DaisHeight = 1.25f;
		constexpr float GalleryHeight = 3.0f;
		constexpr Vec2 LibraryDoor = { 48.0f, 68.0f };
		constexpr Vec2 GalleryDoor = { 83.0f, 72.0f };

		uint32_t Scatter(LevelBuilder& builder, const TileRect& area, PropId id, uint32_t count, float spacing, uint32_t attempts = 400)
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

		void BuildSquare(LevelBuilder& builder)
		{
			builder.Fill({ 30, 7, 36, 33 }, TerrainId::Flagstone, 0.0f);
			builder.Disc({ 48.0f, 26.0f }, 4.5f, TerrainId::Cobblestone);
			builder.SetPlayerSpawn({ 48.0f, 13.0f });

			builder.Prop(PropId::LongHouse, { 35.0f, 5.0f }, FaceNorth);
			builder.Prop(PropId::House, { 42.5f, 5.0f }, FaceNorth);
			builder.Prop(PropId::LongHouse, { 53.5f, 5.0f }, FaceNorth);
			builder.Prop(PropId::House, { 61.0f, 5.0f }, FaceNorth);
			builder.Prop(PropId::Statue, { 47.5f, 4.6f }, FaceNorth);

			builder.Prop(PropId::Fountain, { 48.0f, 26.0f });
			for (const Vec2 lamp : { Vec2(36.0f, 12.0f), Vec2(60.0f, 12.0f), Vec2(36.0f, 33.0f), Vec2(60.0f, 33.0f), Vec2(42.8f, 26.0f), Vec2(53.2f, 26.0f) })
				builder.Prop(PropId::LampPost, lamp);
			builder.Prop(PropId::Bench, { 48.0f, 21.6f });
			builder.Prop(PropId::Bench, { 43.6f, 29.5f }, FaceEast);
			builder.Prop(PropId::Bench, { 52.4f, 29.5f }, FaceWest);

			for (const float y : { 11.0f, 16.5f, 22.0f })
				builder.Prop(PropId::MarketStall, { 62.6f, y }, FaceWest);
			builder.Prop(PropId::Signpost, { 31.5f, 21.0f });
			Scatter(builder, { 63, 8, 3, 18 }, PropId::Barrel, 4, 0.3f);
			Scatter(builder, { 63, 8, 3, 18 }, PropId::Crate, 3, 0.3f);
			Scatter(builder, { 31, 8, 30, 3 }, PropId::Barrel, 4, 0.4f);
			Scatter(builder, { 31, 8, 30, 3 }, PropId::CrateStack, 3, 0.4f);
		}

		void BuildLibrary(LevelBuilder& builder)
		{
			builder.Fill({ 30, 40, 36, 28 }, TerrainId::Marble, TerraceHeight);
			builder.Fill({ 32, 44, 32, 24 }, TerrainId::WoodFloor);
			builder.Fill({ 47, 44, 2, 11 }, TerrainId::Carpet);
			builder.Fill({ 45, 55, 6, 5 }, TerrainId::Marble, DaisHeight);
			builder.Stairs({ 42, 38, 12, 2 }, Direction::North, 0.0f, TerraceHeight, TerrainId::Marble);
			builder.Reserve({ 42, 37, 12, 4 });

			builder.Wall(WallId::Balustrade, { 30.0f, 40.0f }, { 42.0f, 40.0f });
			builder.Wall(WallId::Balustrade, { 54.0f, 40.0f }, { 66.0f, 40.0f });
			builder.Wall(WallId::Balustrade, { 42.0f, 38.0f }, { 42.0f, 40.0f });
			builder.Wall(WallId::Balustrade, { 54.0f, 38.0f }, { 54.0f, 40.0f });
			builder.Wall(WallId::Balustrade, { 30.0f, 40.0f }, { 30.0f, 68.0f });
			builder.Wall(WallId::Balustrade, { 66.0f, 40.0f }, { 66.0f, 68.0f });

			builder.Wall(WallId::Library, { 32.0f, 44.0f }, { 46.0f, 44.0f });
			builder.Wall(WallId::Library, { 50.0f, 44.0f }, { 64.0f, 44.0f });
			builder.Wall(WallId::Library, { 32.0f, 44.0f }, { 32.0f, 68.0f });
			builder.Wall(WallId::Library, { 64.0f, 44.0f }, { 64.0f, 68.0f });
			builder.Wall(WallId::Library, { 32.0f, 68.0f }, { 47.0f, 68.0f });
			builder.Wall(WallId::Library, { 49.0f, 68.0f }, { 64.0f, 68.0f });
			builder.Prop(PropId::Door, LibraryDoor, FaceSouth);
			builder.Portal({ 48.0f, 66.4f }, { 83.0f, 75.6f }, Pi * 0.5f, "Upper gallery");

			for (const float x : { 33.0f, 37.0f, 41.0f, 55.0f, 59.0f, 63.0f })
				builder.Prop(PropId::MarblePillar, { x, 42.0f });
			builder.Prop(PropId::MarblePillar, { 44.5f, 42.0f });
			builder.Prop(PropId::MarblePillar, { 51.5f, 42.0f });
			builder.Prop(PropId::Brazier, { 30.62f, 40.62f });
			builder.Prop(PropId::Brazier, { 65.38f, 40.62f });
			for (const float x : { 35.0f, 39.0f, 57.0f, 61.0f })
				builder.Prop(PropId::Window, { x, 43.74f }, FaceSouth);

			for (const float y : { 47.5f, 51.5f, 55.5f, 59.5f })
			{
				for (const float x : { 35.5f, 38.5f, 41.5f, 54.5f, 57.5f, 60.5f })
					builder.Prop(PropId::DoubleBookshelf, { x, y });
			}
			for (const float y : { 47.0f, 50.0f, 53.0f, 56.0f, 59.0f })
			{
				builder.Prop(PropId::Bookshelf, { 32.47f, y }, FaceEast);
				builder.Prop(PropId::Bookshelf, { 63.53f, y }, FaceWest);
			}
			for (const float x : { 35.0f, 38.0f, 41.0f, 55.0f, 58.0f, 61.0f })
				builder.Prop(PropId::Bookshelf, { x, 67.53f }, FaceSouth);
			for (const Vec2 pillar : { Vec2(43.5f, 49.5f), Vec2(43.5f, 57.5f), Vec2(52.5f, 49.5f), Vec2(52.5f, 57.5f) })
				builder.Prop(PropId::MarblePillar, pillar);

			builder.Prop(PropId::ReadingTable, { 45.3f, 50.0f }, FaceEast);
			builder.Prop(PropId::ReadingTable, { 50.7f, 50.0f }, FaceEast);
			for (const float y : { 49.3f, 50.7f })
			{
				builder.Prop(PropId::Chair, { 44.4f, y }, FaceEast);
				builder.Prop(PropId::Chair, { 51.6f, y }, FaceWest);
			}
			builder.Prop(PropId::Candelabra, { 44.6f, 54.3f });
			builder.Prop(PropId::Candelabra, { 51.4f, 54.3f });
			builder.Prop(PropId::Globe, { 48.0f, 57.5f });
			builder.Prop(PropId::Lectern, { 46.0f, 58.9f });
			builder.Prop(PropId::Lectern, { 50.0f, 58.9f });

			builder.Prop(PropId::Desk, { 40.0f, 63.4f });
			builder.Prop(PropId::Chair, { 40.0f, 64.5f });
			builder.Prop(PropId::Fireplace, { 32.62f, 64.5f }, FaceEast);
			builder.Prop(PropId::Rug, { 35.6f, 64.5f }, FaceEast);
			builder.Prop(PropId::Armchair, { 35.3f, 63.4f }, FaceWest);
			builder.Prop(PropId::Armchair, { 35.3f, 65.6f }, FaceWest);
			builder.Prop(PropId::RoundTable, { 58.0f, 63.6f });
			builder.Prop(PropId::Armchair, { 56.6f, 63.6f }, FaceEast);
			builder.Prop(PropId::Armchair, { 59.4f, 63.6f }, FaceWest);
			builder.Prop(PropId::Banner, { 45.6f, 67.7f });
			builder.Prop(PropId::Banner, { 50.4f, 67.7f });
			for (const Vec2 plant : { Vec2(33.2f, 45.2f), Vec2(62.8f, 45.2f), Vec2(62.8f, 66.4f) })
				builder.Prop(PropId::PottedPlant, plant);
			builder.Region("Library hall", { 33, 45, 30, 22 });
		}

		void BuildGallery(LevelBuilder& builder)
		{
			builder.Fill({ 70, 68, 23, 25 }, TerrainId::Void, 0.0f);
			builder.Fill({ 74, 72, 18, 18 }, TerrainId::WoodFloor, GalleryHeight);
			builder.Wall(WallId::Library, { 74.0f, 72.0f }, { 82.0f, 72.0f });
			builder.Wall(WallId::Library, { 84.0f, 72.0f }, { 92.0f, 72.0f });
			builder.Wall(WallId::Library, { 74.0f, 90.0f }, { 92.0f, 90.0f });
			builder.Wall(WallId::WoodRailing, { 74.0f, 72.0f }, { 74.0f, 90.0f });
			builder.Wall(WallId::WoodRailing, { 92.0f, 72.0f }, { 92.0f, 90.0f });
			builder.Prop(PropId::Door, GalleryDoor, FaceNorth);
			builder.Portal({ 83.0f, 73.4f }, { 48.0f, 64.2f }, -Pi * 0.5f, "Library hall");

			for (const float x : { 77.0f, 81.0f, 85.0f, 89.0f })
				builder.Prop(PropId::Window, { x, 89.74f }, FaceSouth);
			for (const float y : { 78.5f, 82.5f, 86.5f })
				for (const float x : { 77.5f, 80.5f, 85.5f, 88.5f })
					builder.Prop(PropId::DoubleBookshelf, { x, y });
			builder.Prop(PropId::ReadingTable, { 77.5f, 75.4f });
			builder.Prop(PropId::ReadingTable, { 88.5f, 75.4f });
			for (const float x : { 76.8f, 78.2f, 87.8f, 89.2f })
				builder.Prop(PropId::Chair, { x, 76.3f }, FaceSouth);
			builder.Prop(PropId::Candelabra, { 80.6f, 74.3f });
			builder.Prop(PropId::Candelabra, { 85.4f, 74.3f });
			builder.Prop(PropId::Globe, { 83.0f, 88.4f });
			builder.Prop(PropId::PottedPlant, { 75.0f, 89.0f });
			builder.Prop(PropId::PottedPlant, { 91.0f, 89.0f });
			builder.Region("Upper gallery", { 75, 73, 16, 16 });
		}

		void BuildGrove(LevelBuilder& builder)
		{
			builder.Disc({ 9.0f, 33.0f }, 4.2f, TerrainId::Water);
			builder.Fill({ 4, 58, 12, 13 }, TerrainId::Grass, TerraceHeight);
			builder.Stairs({ 9, 56, 3, 2 }, Direction::North, 0.0f, TerraceHeight, TerrainId::Grass);
			builder.Reserve({ 8, 54, 5, 5 });
			builder.Stroke({ 30.5f, 24.0f }, { 19.0f, 24.0f }, 2.6f, TerrainId::Dirt);
			builder.Stroke({ 19.0f, 24.0f }, { 16.0f, 50.0f }, 2.6f, TerrainId::Dirt);
			builder.Stroke({ 16.0f, 50.0f }, { 10.5f, 55.0f }, 2.4f, TerrainId::Dirt);
			builder.Stroke({ 16.0f, 50.0f }, { 22.0f, 72.0f }, 2.6f, TerrainId::Dirt);
			builder.Stroke({ 22.0f, 72.0f }, { 30.5f, 80.0f }, 2.6f, TerrainId::Dirt);
			builder.Wall(WallId::WoodFence, { 22.0f, 22.4f }, { 27.0f, 22.4f });
			builder.Wall(WallId::WoodFence, { 22.0f, 25.6f }, { 27.0f, 25.6f });

			const Vec2 circle = { 10.0f, 64.5f };
			builder.Prop(PropId::Statue, circle);
			for (int32_t i = 0; i < 6; i++)
			{
				const Vec2 position = circle + FromAngle(float(i) * Pi / 3.0f) * 4.0f;
				builder.Prop(i == 4 ? PropId::BrokenPillar : PropId::StonePillar, position, float(i));
			}
			builder.Reserve(circle, 2.4f);

			const TileRect grove = { 4, 4, 25, 88 };
			Scatter(builder, grove, PropId::Boulder, 7, 1.2f);
			Scatter(builder, grove, PropId::FallenLog, 6, 1.0f);
			Scatter(builder, grove, PropId::OakTree, 55, 1.6f, 1500);
			Scatter(builder, grove, PropId::PineTree, 45, 1.6f, 1500);
			Scatter(builder, grove, PropId::Rock, 16, 0.6f);
			Scatter(builder, grove, PropId::Bush, 45, 0.5f);
			Scatter(builder, grove, PropId::Flowers, 40, 0.2f);
			builder.Region("Grove", { 4, 4, 25, 50 });
			builder.Region("North woods", { 4, 54, 25, 38 });
		}

		void BuildGarden(LevelBuilder& builder)
		{
			builder.Stroke({ 30.0f, 80.0f }, { 69.0f, 80.0f }, 2.2f, TerrainId::Dirt);
			builder.Stroke({ 48.0f, 70.0f }, { 48.0f, 92.0f }, 2.2f, TerrainId::Dirt);
			builder.Disc({ 48.0f, 80.0f }, 3.4f, TerrainId::Dirt);
			builder.Reserve({ 48.0f, 80.0f }, 3.6f);
			builder.Wall(WallId::Stone, { 70.0f, 68.0f }, { 70.0f, 93.0f });
			builder.Wall(WallId::IronFence, { 70.0f, 68.0f }, { 93.0f, 68.0f });
			builder.Prop(PropId::Well, { 48.0f, 80.0f });

			for (const float y : { 77.5f, 82.5f })
			{
				builder.Wall(WallId::Hedge, { 33.0f, y }, { 38.0f, y });
				builder.Wall(WallId::Hedge, { 40.0f, y }, { 45.0f, y });
				builder.Wall(WallId::Hedge, { 51.0f, y }, { 56.0f, y });
				builder.Wall(WallId::Hedge, { 58.0f, y }, { 66.0f, y });
			}
			builder.Wall(WallId::Hedge, { 45.5f, 70.0f }, { 45.5f, 76.0f });
			builder.Wall(WallId::Hedge, { 50.5f, 70.0f }, { 50.5f, 76.0f });
			builder.Wall(WallId::Hedge, { 45.5f, 84.0f }, { 45.5f, 91.0f });
			builder.Wall(WallId::Hedge, { 50.5f, 84.0f }, { 50.5f, 91.0f });

			builder.Prop(PropId::Statue, { 38.5f, 73.5f });
			builder.Prop(PropId::Bench, { 38.5f, 71.0f });
			for (const float x : { 35.0f, 37.5f, 40.0f, 42.5f })
				for (const float y : { 85.5f, 88.0f, 90.5f })
					builder.TryProp(PropId::Gravestone, { x, y }, FaceSouth, 0.1f);
			for (const Vec2 tree : { Vec2(54.0f, 86.5f), Vec2(58.5f, 89.5f), Vec2(62.5f, 86.0f), Vec2(65.5f, 90.5f) })
				builder.TryProp(PropId::OakTree, tree, tree.x, 0.5f);
			builder.Prop(PropId::Bench, { 58.0f, 73.0f });
			builder.Prop(PropId::Bench, { 62.0f, 73.0f });
			Scatter(builder, { 51, 70, 15, 7 }, PropId::Flowers, 10, 0.2f);
			Scatter(builder, { 33, 70, 12, 7 }, PropId::Flowers, 8, 0.2f);
			Scatter(builder, { 51, 83, 15, 9 }, PropId::Bush, 6, 0.4f);
			builder.Region("Garden", { 31, 69, 38, 23 });
		}

		void BuildQuarter(LevelBuilder& builder)
		{
			builder.Fill({ 66, 3, 27, 65 }, TerrainId::Cobblestone, 0.0f);
			builder.Reserve({ 66, 30, 27, 6 });
			builder.Reserve({ 66, 36, 4, 32 });

			builder.Prop(PropId::LongHouse, { 74.5f, 39.5f }, FaceSouth);
			builder.Prop(PropId::House, { 82.0f, 39.5f }, FaceSouth);
			builder.Prop(PropId::LongHouse, { 89.5f, 39.5f }, FaceSouth);
			builder.Prop(PropId::House, { 73.0f, 48.5f }, FaceSouth);
			builder.Prop(PropId::LongHouse, { 80.5f, 48.5f }, FaceSouth);
			builder.Prop(PropId::House, { 88.5f, 48.5f }, FaceSouth);
			builder.Prop(PropId::LongHouse, { 75.0f, 56.5f }, FaceSouth);
			builder.Prop(PropId::House, { 83.0f, 56.5f }, FaceSouth);
			builder.Prop(PropId::House, { 89.5f, 56.5f }, FaceSouth);
			builder.Prop(PropId::House, { 74.0f, 64.0f }, FaceSouth);
			builder.Prop(PropId::LongHouse, { 82.5f, 64.0f }, FaceSouth);
			builder.Prop(PropId::House, { 90.0f, 64.0f }, FaceSouth);

			builder.Prop(PropId::House, { 73.0f, 26.5f }, FaceNorth);
			builder.Prop(PropId::LongHouse, { 90.0f, 26.5f }, FaceNorth);
			builder.Prop(PropId::LongHouse, { 74.5f, 14.0f }, FaceNorth);
			builder.Prop(PropId::House, { 83.0f, 14.0f }, FaceNorth);
			builder.Prop(PropId::House, { 90.0f, 14.0f }, FaceNorth);
			builder.Prop(PropId::House, { 72.0f, 5.5f }, FaceNorth);
			builder.Prop(PropId::LongHouse, { 80.0f, 5.5f }, FaceNorth);
			builder.Prop(PropId::House, { 89.0f, 5.5f }, FaceNorth);

			builder.Prop(PropId::Well, { 81.5f, 23.0f });
			builder.Prop(PropId::Cart, { 84.0f, 19.5f }, 0.3f);
			for (const float x : { 72.0f, 80.0f, 88.0f })
				builder.Prop(PropId::LampPost, { x, 36.6f });
			builder.Prop(PropId::Signpost, { 67.5f, 33.0f });
			Scatter(builder, { 70, 17, 23, 13 }, PropId::Barrel, 8, 0.4f);
			Scatter(builder, { 70, 17, 23, 13 }, PropId::Crate, 6, 0.4f);
			Scatter(builder, { 70, 42, 23, 22 }, PropId::Barrel, 8, 0.6f);
			Scatter(builder, { 70, 42, 23, 22 }, PropId::CrateStack, 5, 0.6f);
			builder.Region("Old quarter north", { 70, 37, 23, 29 });
			builder.Region("Old quarter south", { 70, 4, 23, 26 });
		}
	}

	Level BuildTownLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::Town, seed, TownSize, TownSize, TerrainId::Rock);
		builder.Fill({ 3, 3, 90, 90 }, TerrainId::Grass, 0.0f);
		BuildSquare(builder);
		BuildLibrary(builder);
		BuildGallery(builder);
		BuildQuarter(builder);
		BuildGarden(builder);
		BuildGrove(builder);
		builder.SetMissionStart({ 84.0f, 33.0f });
		builder.SetBossArena({ 48.0f, 20.0f }, 11.0f);
		return builder.Finish();
	}
}
