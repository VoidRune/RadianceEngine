#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr int32_t HubWidth = 44;
		constexpr int32_t HubHeight = 36;
		constexpr float FaceSouth = 0.0f;
		constexpr float FaceNorth = Pi;
		constexpr float FaceEast = Pi * 0.5f;
		constexpr float FaceWest = -Pi * 0.5f;

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
		LevelBuilder builder(MapKind::Hub, seed, HubWidth, HubHeight, TerrainId::Rock);
		builder.Fill({ 2, 2, 40, 32 }, TerrainId::Grass, 0.0f);
		builder.Fill({ 7, 5, 30, 25 }, TerrainId::Cobblestone);
		builder.Disc({ 22.0f, 18.0f }, 4.2f, TerrainId::Flagstone);
		builder.Stroke({ 22.0f, 2.0f }, { 22.0f, 6.0f }, 2.6f, TerrainId::Dirt);
		builder.SetPlayerSpawn({ 22.0f, 9.0f });

		builder.Prop(PropId::Fountain, { 22.0f, 18.0f });
		builder.Reserve({ 22.0f, 18.0f }, 4.6f);
		for (const Vec2 bench : { Vec2(22.0f, 13.6f), Vec2(17.6f, 18.0f), Vec2(26.4f, 18.0f) })
			builder.Prop(PropId::Bench, bench, bench.x < 20.0f ? FaceEast : bench.x > 24.0f ? FaceWest : FaceSouth);

		builder.Prop(PropId::StashChest, { 9.6f, 15.0f }, FaceEast);
		builder.Prop(PropId::Barrel, { 8.6f, 12.8f });
		builder.Prop(PropId::CrateStack, { 8.8f, 17.4f });
		builder.Prop(PropId::Banner, { 8.2f, 15.0f }, FaceEast);
		builder.Station(StationKind::Stash, { 10.4f, 15.0f }, 2.4f);
		builder.Reserve({ 11.0f, 15.0f }, 2.2f);

		builder.Prop(PropId::Forge, { 34.6f, 16.6f }, FaceWest);
		builder.Prop(PropId::Anvil, { 31.6f, 15.0f }, FaceSouth);
		builder.Prop(PropId::Barrel, { 35.2f, 13.4f });
		builder.Prop(PropId::Crate, { 35.6f, 19.8f });
		builder.Prop(PropId::Brazier, { 32.2f, 19.2f });
		builder.Station(StationKind::Forge, { 31.8f, 16.6f }, 2.8f);
		builder.Reserve({ 30.6f, 16.6f }, 2.2f);

		builder.Prop(PropId::GemAltar, { 11.0f, 25.2f });
		for (const Vec2 crystal : { Vec2(9.2f, 26.8f), Vec2(12.9f, 27.1f), Vec2(9.0f, 23.6f) })
			builder.Prop(PropId::Crystal, crystal, crystal.x);
		builder.Station(StationKind::Lapidary, { 11.6f, 24.0f }, 2.6f);
		builder.Reserve({ 12.4f, 23.2f }, 2.0f);

		builder.Prop(PropId::MissionBoard, { 22.0f, 28.4f }, FaceSouth);
		builder.Prop(PropId::Brazier, { 19.4f, 28.2f });
		builder.Prop(PropId::Brazier, { 24.6f, 28.2f });
		builder.Prop(PropId::Banner, { 17.8f, 29.0f }, FaceSouth);
		builder.Prop(PropId::Banner, { 26.2f, 29.0f }, FaceSouth);
		builder.Station(StationKind::MissionBoard, { 22.0f, 26.6f }, 3.0f);
		builder.Reserve({ 22.0f, 25.8f }, 2.4f);

		builder.Prop(PropId::Statue, { 31.4f, 26.8f }, FaceSouth);
		builder.Prop(PropId::Brazier, { 29.4f, 26.4f });
		builder.Prop(PropId::Brazier, { 33.4f, 26.4f });
		builder.Prop(PropId::Banner, { 30.2f, 27.8f }, FaceSouth);
		builder.Prop(PropId::Banner, { 32.6f, 27.8f }, FaceSouth);
		builder.Station(StationKind::ProvingGrounds, { 31.4f, 24.6f }, 2.6f);
		builder.Reserve({ 31.4f, 24.4f }, 2.2f);
		builder.Prop(PropId::Signpost, { 24.2f, 6.2f });
		for (const Vec2 lamp : { Vec2(7.6f, 5.6f), Vec2(36.4f, 5.6f), Vec2(7.6f, 29.4f), Vec2(36.4f, 29.4f), Vec2(15.0f, 9.0f), Vec2(29.0f, 9.0f) })
			builder.Prop(PropId::LampPost, lamp);

		builder.Prop(PropId::House, { 5.6f, 31.6f }, FaceSouth);
		builder.Prop(PropId::LongHouse, { 13.8f, 31.6f }, FaceSouth);
		builder.Prop(PropId::LongHouse, { 31.4f, 31.6f }, FaceSouth);
		builder.Prop(PropId::House, { 38.4f, 31.6f }, FaceSouth);
		builder.Reserve({ 7, 5, 30, 25 });
		Ring(builder, { 2, 2, 5, 28 }, PropId::OakTree, 7, 1.4f);
		Ring(builder, { 37, 2, 5, 28 }, PropId::PineTree, 7, 1.4f);
		Ring(builder, { 2, 2, 40, 3 }, PropId::Bush, 10, 0.4f);
		Ring(builder, { 2, 2, 40, 32 }, PropId::Flowers, 18, 0.2f);
		builder.Region("Emberhold", { 2, 2, 40, 32 }, false);
		return builder.Finish();
	}
}
