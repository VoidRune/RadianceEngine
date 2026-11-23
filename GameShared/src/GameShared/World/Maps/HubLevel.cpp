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
		constexpr int32_t OriginX = int32_t(EmberCenter.x);
		constexpr int32_t OriginY = int32_t(EmberCenter.y);
		constexpr float LampReach = 8.4f;
		constexpr float LampAngles[] = { 30.0f, 150.0f, 210.0f, 330.0f };
		constexpr Vec2 PathLamps[] = { { 4.4f, 11.0f }, { -4.4f, 11.0f }, { -22.6f, 3.0f }, { -22.6f, -3.0f }, { 23.8f, 3.4f }, { 32.2f, 3.4f }, { 3.4f, -13.0f }, { -3.4f, -13.0f }, { 3.4f, -21.5f }, { -3.4f, -21.5f }, { 25.0f, -10.0f } };
		constexpr float RiftCrystalRing = 1.35f;

		Vec2 World(Vec2 local)
		{
			return Origin + local;
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
				builder.Stairs({ OriginX + stair.X, OriginY + stair.Y, stair.Width, stair.Height }, stair.Up, stair.Bottom, stair.Top, TerrainId::Dirt);
			for (int32_t y = 0; y < map.GetHeight(); y++)
				for (int32_t x = 0; x < map.GetWidth(); x++)
				{
					const Vec2 local = Vec2(float(x) + 0.5f, float(y) + 0.5f) - Origin;
					if (Length(local) <= HubDeckRadius)
						Set(map, x, y, TerrainId::Deck, 0.0f);
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
		builder.Prop(PropId::Bench, chess + Vec2(0.0f, 3.4f), FaceSouth);
		builder.Prop(PropId::Bench, chess + Vec2(-3.4f, 0.3f), FaceEast);
		builder.Prop(PropId::Bench, chess + Vec2(3.4f, -0.6f), FaceWest);

		const Vec2 maple = World(HubMapleSpot);
		builder.Prop(PropId::AutumnTree, maple, FaceSouth);
		builder.Reserve(maple, 4.6f);

		for (const float degrees : LampAngles)
			builder.Prop(PropId::LampPost, Origin + FromAngle(degrees * Pi / 180.0f) * LampReach);
		for (const Vec2 spot : PathLamps)
			builder.Prop(PropId::LampPost, World(spot));
		builder.Reserve(Origin, EmberRingOuter + 0.5f);

		for (const HubSign& sign : HubSigns)
			builder.Prop(PropId::Signpost, World(sign.Position), sign.Yaw);
		for (const Vec2 spot : HubCrystals)
			builder.Prop(PropId::Crystal, World(spot), spot.x * 1.7f + spot.y);
		for (const Vec2 spot : HubSpires)
			builder.Prop(PropId::CrystalSpire, World(spot), spot.x - spot.y);
		builder.Prop(PropId::CrateStack, World({ -27.8f, 6.2f }), 0.3f);
		builder.Prop(PropId::Crate, World({ -29.0f, 4.4f }), 0.9f);
		builder.Prop(PropId::Barrel, World({ -32.6f, 7.4f }));
		builder.Prop(PropId::Crate, World({ -29.4f, -4.2f }), -0.4f);
		builder.Prop(PropId::Barrel, World({ -28.2f, -5.6f }));
		builder.Prop(PropId::Crate, World({ 26.2f, -10.4f }), 0.2f);
		builder.Prop(PropId::Barrel, World({ 34.0f, -11.0f }));
		builder.Prop(PropId::CrateStack, World({ 36.6f, -16.4f }), -0.3f);

		const Vec2 board = World(HubBoardSpot);
		builder.Prop(PropId::NoticeBoard, board);
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
			{ { 3.0f, 4.0f }, { 3.0f, 66.0f } },
			{ { 105.5f, 66.0f }, { 105.5f, 4.0f } },
		};
		return level;
	}
}
