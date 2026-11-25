#include "GameShared/Content/HubLayout.h"
#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float FaceSouth = 0.0f;
		constexpr float FaceEast = Pi * 0.5f;
		constexpr float FaceWest = -Pi * 0.5f;
		constexpr float FaceNorth = Pi;
		constexpr Vec2 Origin = EmberCenter;
		constexpr float LampReach = 8.4f;
		constexpr float LampAngles[] = { 30.0f, 150.0f, 210.0f, 330.0f };
		constexpr Vec2 PathLamps[] = { { 3.6f, 11.4f }, { -3.6f, 11.4f }, { 3.3f, -13.0f }, { -3.3f, -13.5f }, { 3.0f, -20.2f }, { -3.2f, -20.2f }, { -13.0f, 2.4f }, { -20.4f, 3.0f }, { 12.6f, -3.2f }, { 20.2f, -9.4f }, { 7.4f, 11.0f }, { -9.2f, 10.8f }, { -27.4f, 5.2f }, { 31.4f, -8.6f } };
		constexpr float RiftCrystalRing = 1.35f;
		constexpr Vec2 Oaks[] = { { -11.5f, 24.5f }, { -6.5f, 26.0f }, { 6.5f, 26.3f }, { -15.5f, 23.0f }, { -14.6f, 15.6f }, { -20.5f, 6.8f }, { -22.0f, -3.0f }, { -25.5f, -8.2f }, { -29.0f, 11.5f }, { -10.0f, -9.5f }, { 37.5f, 6.0f }, { 24.5f, -3.4f }, { -33.5f, -11.0f } };
		constexpr Vec2 Pines[] = { { 14.0f, 25.8f }, { 19.0f, 26.3f }, { 24.5f, 26.0f }, { 34.5f, 24.0f }, { 37.8f, 18.5f } };
		constexpr Vec2 Shrubs[] = { { -6.0f, -12.0f }, { -9.0f, -18.0f }, { 6.0f, -16.5f }, { 12.0f, -9.0f }, { 19.0f, -13.6f }, { 5.0f, -6.5f }, { -5.0f, -6.8f }, { -12.0f, -4.0f }, { 16.0f, -2.5f }, { 31.0f, 0.5f }, { -24.0f, -11.5f }, { -17.0f, -12.2f }, { 7.6f, 14.6f }, { -7.6f, 14.6f }, { 33.0f, 16.0f } };
		constexpr Vec2 Boulders[] = { { -8.6f, 15.4f }, { -7.8f, 21.4f }, { 8.4f, 16.2f }, { 9.6f, 21.0f }, { -3.6f, 24.8f }, { 3.8f, 25.0f }, { -26.0f, 18.6f }, { -18.4f, 20.4f }, { -39.0f, -2.4f }, { 38.6f, 0.6f }, { 13.0f, -16.6f }, { -4.8f, -21.4f }, { -36.2f, 0.4f } };
		constexpr Vec2 StoneRing = { 31.5f, 9.5f };

		Vec2 World(Vec2 local)
		{
			return Origin + local;
		}

		float Facing(Vec2 from, Vec2 to)
		{
			const Vec2 d = to - from;
			return std::atan2(d.x, -d.y);
		}

		void Set(TileMap& map, int32_t x, int32_t y, TerrainId terrain, float height)
		{
			if (map.IsInside(x, y))
				map.At(x, y) = { terrain, Direction::None, ToLevel(height), 0 };
		}

		void Island(LevelBuilder& builder)
		{
			TileMap& map = builder.GetMap();
			for (int32_t y = 0; y < map.GetHeight(); y++)
				for (int32_t x = 0; x < map.GetWidth(); x++)
				{
					const Vec2 local = Vec2(float(x) + 0.5f, float(y) + 0.5f) - Origin;
					if (!HubIsLand(local))
						continue;
					const HubPave paving = HubPaving(local);
					Set(map, x, y, paving.Inside > 0.0f ? paving.Stone : TerrainId::Grass, HubFloor(local));
				}
			for (const HubStair& stair : HubStairs)
				builder.Stairs({ int32_t(Origin.x) + stair.X, int32_t(Origin.y) + stair.Y, stair.Width, stair.Height }, stair.Up, stair.Bottom, stair.Top, TerrainId::Dirt);
			for (int32_t y = 0; y < map.GetHeight(); y++)
				for (int32_t x = 0; x < map.GetWidth(); x++)
				{
					const Vec2 local = Vec2(float(x) + 0.5f, float(y) + 0.5f) - Origin;
					if (Length(local) <= HubDeckRadius)
						Set(map, x, y, TerrainId::Deck, 0.0f);
					else if (HubStairTread(local))
						continue;
					else if (const std::optional<float> deck = HubDeckHeight(local))
						Set(map, x, y, TerrainId::Deck, *deck);
				}
			const int32_t reach = int32_t(std::ceil(PortalTerraceReach));
			const Vec2 gate = World(HubGateSpot);
			for (int32_t y = -reach; y <= reach; y++)
				for (int32_t x = -reach; x <= reach; x++)
				{
					const int32_t cx = int32_t(gate.x) + x;
					const int32_t cy = int32_t(gate.y) + y;
					const Vec2 center{ float(cx) + 0.5f, float(cy) + 0.5f };
					const int32_t level = PortalTerraceLevel(center - gate, HeightUnit);
					if (level > 0 && HubIsLand(center - Origin))
						Set(map, cx, cy, TerrainId::Deck, HubNorthLevel + float(level) * HeightUnit);
				}
		}
	}

	Level BuildHubLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::Hub, seed, HubWidth, HubHeight, TerrainId::Void);
		const Vec2 trainingCenter = World(HubTrainingSpot);
		Island(builder);
		builder.SetRounded();
		builder.SetPlayerSpawn(World(HubSpawnSpot));

		const Vec2 gate = World(HubGateSpot);
		builder.Prop(PropId::CalderaGate, gate, FaceSouth);
		builder.Station(StationKind::CalderaGate, gate, 3.2f);
		builder.Reserve(gate, PortalTerraceReach);

		const Vec2 rift = World(HubRiftSpot);
		for (int i = 0; i < 3; i++)
		{
			const float angle = Pi + float(i) * Pi * 2.0f / 3.0f;
			builder.Prop(PropId::Crystal, rift + FromAngle(angle) * RiftCrystalRing, angle);
		}
		builder.Station(StationKind::ShardRift, rift, 2.4f);
		builder.Reserve(rift, 2.7f);

		const Vec2 chess = World(HubChessSpot);
		builder.Prop(PropId::ChessTable, chess);
		builder.Station(StationKind::Chess, chess, 2.5f);
		builder.Reserve(chess, 2.7f);
		builder.Prop(PropId::Bench, chess + Vec2(0.0f, 2.6f), FaceSouth);
		builder.Prop(PropId::Bench, chess + Vec2(-2.6f, 0.3f), FaceEast);
		builder.Prop(PropId::Bench, chess + Vec2(2.4f, -0.9f), FaceWest);

		const Vec2 maple = World(HubMapleSpot);
		builder.Prop(PropId::AutumnTree, maple, FaceSouth);
		builder.Reserve(maple, 4.6f);

		for (const float degrees : LampAngles)
			builder.Prop(PropId::LampPost, Origin + FromAngle(degrees * Pi / 180.0f) * LampReach);
		for (const Vec2 spot : PathLamps)
			builder.Prop(PropId::LampPost, World(spot));
		for (const auto& arc : HubWallArcs)
		{
			for (const float degrees : { arc[0] - 3.0f, arc[1] + 3.0f })
				builder.Prop(PropId::Banner, Origin + FromAngle(degrees * Pi / 180.0f) * HubWallArcRadius, (degrees - 90.0f) * Pi / 180.0f);
			const Vec2 seat = Origin + FromAngle(0.5f * (arc[0] + arc[1]) * Pi / 180.0f) * (HubWallArcRadius - 0.9f);
			builder.Prop(PropId::Bench, seat, Facing(seat, Origin));
		}
		builder.Reserve(Origin, EmberRingOuter + 0.5f);

		for (const HubSign& sign : HubSigns)
			builder.Prop(PropId::Signpost, World(sign.Position), sign.Yaw);
		for (const Vec2 spot : HubCrystals)
			builder.Prop(PropId::Crystal, World(spot), spot.x * 1.7f + spot.y);
		for (const Vec2 spot : HubSpires)
			builder.Prop(PropId::CrystalSpire, World(spot), spot.x - spot.y);
		for (const Vec2 spot : Oaks)
			builder.Prop(PropId::OakTree, World(spot), spot.x * 0.7f + spot.y * 1.3f);
		for (const Vec2 spot : Pines)
			builder.Prop(PropId::PineTree, World(spot), spot.x * 1.1f);
		for (const Vec2 spot : Shrubs)
			builder.Prop(PropId::Bush, World(spot), spot.x + spot.y);
		for (const Vec2 spot : Boulders)
			builder.Prop(PropId::MossBoulder, World(spot), spot.x * 2.3f - spot.y);
		for (int i = 0; i < 5; i++)
		{
			const float angle = float(i) * 2.0f * Pi / 5.0f + 0.4f;
			builder.Prop(i % 2 == 0 ? PropId::RuinBrokenColumn : PropId::RuinStump, World(StoneRing + FromAngle(angle) * 1.9f), angle);
		}

		builder.Prop(PropId::Fountain, World(HubFountainSpot));
		builder.Reserve(World(HubFountainSpot), 2.0f);
		builder.Prop(PropId::Well, World(HubWellSpot));
		builder.Prop(PropId::MarketStall, World({ -25.6f, -5.6f }), FaceEast);
		builder.Prop(PropId::Cart, World({ 16.4f, -8.6f }), 0.45f);
		builder.Prop(PropId::RuinFallenColumn, World({ -12.5f, -12.5f }), 0.4f);
		builder.Prop(PropId::RuinArch, World({ -19.8f, -13.7f }), FaceEast);
		builder.Prop(PropId::Telescope, World({ -29.0f, -13.8f }), FaceSouth);
		builder.Prop(PropId::CrateStack, World({ 21.6f, -8.8f }), 0.3f);
		builder.Prop(PropId::Crate, World({ 22.8f, -15.8f }), 0.9f);
		builder.Prop(PropId::Barrel, World({ 27.8f, -16.0f }));
		builder.Prop(PropId::Barrel, World({ 28.4f, -8.6f }));
		builder.Prop(PropId::CrateStack, World({ 38.4f, -15.0f }), -0.3f);
		builder.Prop(PropId::Barrel, World({ 33.0f, -15.2f }));
		builder.Prop(PropId::Crate, World({ 37.8f, -9.9f }), 0.2f);
		builder.Prop(PropId::CrateStack, World({ -38.8f, 6.8f }), 0.5f);
		builder.Prop(PropId::Crate, World({ -34.4f, 10.2f }), -0.6f);
		builder.Prop(PropId::Barrel, World({ -35.6f, -3.6f }));
		builder.Prop(PropId::Crate, World({ -38.6f, -7.0f }), 0.2f);

		const Vec2 board = World(HubBoardSpot);
		builder.Prop(PropId::NoticeBoard, board, Facing(board, board + Vec2(0.7f, -1.0f)));
		const Vec2 wish = World(HubWishSpot);
		builder.Prop(PropId::OfferingBowl, wish + Vec2(-0.42f, 0.0f));
		builder.Prop(PropId::CandleStand, wish + Vec2(-0.45f, 1.05f));
		builder.Station(StationKind::WishingVoid, wish, 1.2f);

		const Vec2 trainingSpawn = trainingCenter + Vec2(0.0f, -2.6f);
		builder.Portal(World(HubDoorSpot), trainingSpawn, FaceNorth, "Training grounds");
		builder.Portal(trainingCenter + Vec2(0.0f, -4.6f), World(HubDoorReturn), FaceSouth, "Sun's Cradle");
		builder.Prop(PropId::Lectern, trainingCenter + Vec2(0.0f, 3.3f), FaceSouth);
		builder.Prop(PropId::CandleStand, trainingCenter + Vec2(1.2f, 3.6f));
		builder.Station(StationKind::Spellbook, trainingCenter + Vec2(0.0f, 3.0f), 2.0f);
		builder.Prop(PropId::FireBowl, trainingCenter + Vec2(-3.3f, 2.8f));
		builder.Prop(PropId::FireBowl, trainingCenter + Vec2(3.3f, 2.8f));
		builder.Dummy(trainingCenter + Vec2(-2.2f, 0.8f), trainingCenter + Vec2(-2.2f, 0.8f));
		builder.Dummy(trainingCenter + Vec2(2.2f, 0.8f), trainingCenter + Vec2(2.2f, 0.8f));
		builder.Dummy(trainingCenter + Vec2(-1.6f, -1.2f), trainingCenter + Vec2(1.6f, -1.2f));
		builder.Dummy(trainingCenter + Vec2(4.9f, -1.2f), trainingCenter + Vec2(4.9f, -1.2f));
		builder.Dummy(trainingCenter + Vec2(4.7f, 1.6f), trainingCenter + Vec2(4.7f, 1.6f));
		builder.Reserve(trainingCenter, HubTrainingRadius);
		builder.Region("Training grounds", { int32_t(trainingCenter.x - HubTrainingRadius) - 1, int32_t(trainingCenter.y - HubTrainingRadius) - 1, int32_t(2.0f * HubTrainingRadius) + 3, int32_t(2.0f * HubTrainingRadius) + 3 }, false);
		builder.Region("Sun's Cradle", { 0, 0, HubWidth, HubHeight }, false);

		Level level = builder.Finish();
		level.Floating = true;
		level.ChessTable = ChessSpot{ chess, 0.0f };
		level.Landmarks = { { LandmarkKind::FallBoard, board, 9.0f } };
		level.DriftLanes = {
			{ { 4.0f, 67.5f }, { 128.0f, 67.5f } },
			{ { 4.0f, 3.5f }, { 128.0f, 4.0f } },
			{ { 8.0f, 4.0f }, { 8.0f, 66.0f } },
			{ { 106.5f, 66.0f }, { 106.5f, 4.0f } },
		};
		return level;
	}
}
