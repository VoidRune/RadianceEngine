#include "GameShared/Content/EmberIsland.h"
#include "GameShared/Content/PortalTerrace.h"
#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr int32_t HubWidth = 70;
		constexpr int32_t HubHeight = 62;
		constexpr float FaceSouth = 0.0f;
		constexpr float FaceEast = Pi * 0.5f;
		constexpr float FaceNorth = Pi;
		constexpr Vec2 GateCenter = EmberCenter + EmberGateOffset;
		constexpr float GateTerrace = PortalTerraceReach;
		constexpr Vec2 RiftCenter = EmberCenter - EmberPadOffset;
		constexpr Vec2 ChessCenter = EmberCenter + EmberPadOffset;
		constexpr Vec2 MapleCenter = EmberCenter + EmberMapleOffset;
		constexpr Vec2 BoardSpot = EmberCenter + EmberBoardOffset;
		constexpr Vec2 WishSpot = EmberCenter + EmberWishOffset;
		constexpr Vec2 TrainingGate = EmberCenter + EmberTrainingOffset;
		constexpr Vec2 SpawnSpot = EmberCenter + Vec2(0.0f, -8.0f);
		constexpr float LampReach = 8.4f;
		constexpr float LampAngles[] = { 30.0f, 150.0f, 210.0f, 330.0f };
		constexpr float RiftCrystalRing = 1.35f;
		constexpr Vec2 TrainingCenter = { 61.5f, 26.5f };
		constexpr float TrainingRadius = 6.3f;

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

		void Island(LevelBuilder& builder)
		{
			TileMap& map = builder.GetMap();
			for (int32_t y = 0; y < map.GetHeight(); y++)
				for (int32_t x = 0; x < map.GetWidth(); x++)
					if (EmberEdgeDistance({ float(x) + 0.5f, float(y) + 0.5f }) >= EmberWalkInset)
						Set(map, x, y, TerrainId::Deck, 0.0f);
			const int32_t reach = int32_t(std::ceil(PortalTerraceReach));
			for (int32_t y = -reach; y <= reach; y++)
				for (int32_t x = -reach; x <= reach; x++)
				{
					const int32_t cx = int32_t(GateCenter.x) + x;
					const int32_t cy = int32_t(GateCenter.y) + y;
					const Vec2 center{ float(cx) + 0.5f, float(cy) + 0.5f };
					const int32_t level = PortalTerraceLevel(center - GateCenter, HeightUnit);
					if (level > 0 && EmberEdgeDistance(center) >= EmberWalkInset)
						Set(map, cx, cy, TerrainId::Deck, float(level) * HeightUnit);
				}
		}
	}

	Level BuildHubLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::Hub, seed, HubWidth, HubHeight, TerrainId::Void);
		Island(builder);
		Blob(builder, TrainingCenter, TrainingRadius, 0.08f, 1.9f, TerrainId::Grass, 0.0f);
		Paint(builder, TrainingCenter + Vec2(0.0f, 0.4f), 3.4f, TerrainId::Flagstone, 0.0f);
		builder.SetRounded();
		builder.SetPlayerSpawn(SpawnSpot);

		builder.Prop(PropId::CalderaGate, GateCenter, FaceSouth);
		builder.Station(StationKind::CalderaGate, GateCenter, 3.2f);
		builder.Reserve(GateCenter, GateTerrace);

		for (int i = 0; i < 3; i++)
		{
			const float angle = Pi * 0.5f + float(i) * Pi * 2.0f / 3.0f;
			builder.Prop(PropId::Crystal, RiftCenter + FromAngle(angle) * RiftCrystalRing, angle);
		}
		builder.Station(StationKind::ShardRift, RiftCenter, 2.4f);
		builder.Reserve(RiftCenter, EmberPadRadius);

		builder.Prop(PropId::ChessTable, ChessCenter);
		builder.Station(StationKind::Chess, ChessCenter, 2.5f);
		builder.Reserve(ChessCenter, EmberPadRadius);

		builder.Prop(PropId::AutumnTree, MapleCenter, FaceSouth);
		builder.Reserve(MapleCenter, 4.6f);

		for (const float degrees : LampAngles)
			builder.Prop(PropId::LampPost, EmberCenter + FromAngle(degrees * Pi / 180.0f) * LampReach);
		builder.Reserve(EmberCenter, EmberRingOuter + 0.5f);

		for (size_t i = 0; i + 3 < std::size(EmberOutcrops); i += 4)
		{
			const Vec2 center = EmberCenter + Vec2(EmberOutcrops[i], EmberOutcrops[i + 1]);
			const float radius = EmberOutcrops[i + 2];
			builder.Prop(PropId::EmberStone, center);
			if (radius > EmberOutcropBlock)
				for (int k = 0; k < 6; k++)
					builder.Prop(PropId::EmberStone, center + FromAngle(float(k) * Pi / 3.0f) * (radius - EmberOutcropBlock * 0.8f));
		}
		builder.Prop(PropId::NoticeBoard, BoardSpot);
		builder.Prop(PropId::OfferingBowl, WishSpot + Vec2(0.42f, 0.0f));
		builder.Prop(PropId::CandleStand, WishSpot + Vec2(0.45f, 1.05f));
		builder.Station(StationKind::WishingVoid, WishSpot, 1.2f);

		const Vec2 trainingSpawn = TrainingCenter + Vec2(0.0f, -2.6f);
		builder.Portal(TrainingGate, trainingSpawn, FaceNorth, "Training grounds");
		builder.Portal(TrainingCenter + Vec2(0.0f, -4.6f), TrainingGate + Vec2(-1.4f, 1.0f), FaceNorth, "Sun's Cradle");
		builder.Prop(PropId::Lectern, TrainingCenter + Vec2(0.0f, 3.3f), FaceSouth);
		builder.Prop(PropId::CandleStand, TrainingCenter + Vec2(1.2f, 3.6f));
		builder.Station(StationKind::Spellbook, TrainingCenter + Vec2(0.0f, 3.0f), 2.0f);
		builder.Prop(PropId::FireBowl, TrainingCenter + Vec2(-3.3f, 2.8f));
		builder.Prop(PropId::FireBowl, TrainingCenter + Vec2(3.3f, 2.8f));
		builder.Dummy(TrainingCenter + Vec2(-2.2f, 0.8f), TrainingCenter + Vec2(-2.2f, 0.8f));
		builder.Dummy(TrainingCenter + Vec2(2.2f, 0.8f), TrainingCenter + Vec2(2.2f, 0.8f));
		builder.Dummy(TrainingCenter + Vec2(-1.6f, -1.2f), TrainingCenter + Vec2(1.6f, -1.2f));
		builder.Dummy(TrainingCenter + Vec2(4.9f, -1.2f), TrainingCenter + Vec2(4.9f, -1.2f));
		builder.Dummy(TrainingCenter + Vec2(4.7f, 1.6f), TrainingCenter + Vec2(4.7f, 1.6f));
		builder.Reserve(TrainingCenter, TrainingRadius);
		builder.Region("Training grounds", { int32_t(TrainingCenter.x - TrainingRadius) - 1, int32_t(TrainingCenter.y - TrainingRadius) - 1, int32_t(2.0f * TrainingRadius) + 3, int32_t(2.0f * TrainingRadius) + 3 }, false);
		builder.Region("Sun's Cradle", { 0, 0, HubWidth, HubHeight }, false);

		Level level = builder.Finish();
		level.Floating = true;
		level.ChessTable = ChessSpot{ ChessCenter, 0.0f };
		level.Landmarks = { { LandmarkKind::FallBoard, BoardSpot, 9.0f } };
		level.DriftLanes = {
			{ { 2.0f, 54.5f }, { 68.0f, 55.0f } },
			{ { 2.0f, 6.5f }, { 68.0f, 7.0f } },
			{ { 6.0f, 2.0f }, { 6.0f, 60.0f } },
			{ { 51.6f, 60.0f }, { 51.6f, 3.0f } },
		};
		return level;
	}
}
