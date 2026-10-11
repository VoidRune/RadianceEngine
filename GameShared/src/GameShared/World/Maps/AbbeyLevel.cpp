#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr int32_t AbbeyWidth = 60;
		constexpr int32_t AbbeyHeight = 52;
		constexpr float FaceSouth = 0.0f;
		constexpr float FaceNorth = Pi;
		constexpr float FaceEast = Pi * 0.5f;
		constexpr float FaceWest = -Pi * 0.5f;

		void Scatter(LevelBuilder& builder, const TileRect& area, PropId id, uint32_t count, float spacing)
		{
			Random& random = builder.GetRandom();
			uint32_t placed = 0;
			for (uint32_t attempt = 0; attempt < count * 40 && placed < count; attempt++)
			{
				const Vec2 position = { float(area.X) + random.Float() * float(area.Width), float(area.Y) + random.Float() * float(area.Height) };
				if (builder.TryProp(id, position, random.Float() * 2.0f * Pi, spacing))
					placed++;
			}
		}

		void Hem(LevelBuilder& builder, Vec2 from, Vec2 to, Vec2 inward, float chance)
		{
			Random& random = builder.GetRandom();
			const Vec2 span = to - from;
			const int32_t steps = int32_t(Length(span) / 0.55f);
			for (int32_t i = 0; i <= steps; i++)
			{
				if (random.Float() > chance)
					continue;
				const Vec2 position = from + span * (float(i) / float(std::max(steps, 1))) + inward * (0.45f + random.Float() * 0.5f);
				const float roll = random.Float();
				builder.TryProp(roll < 0.55f ? PropId::GrassTuft : roll < 0.85f ? PropId::Weeds : PropId::Fern, position, random.Float() * 2.0f * Pi, 0.05f);
			}
		}
	}

	Level BuildAbbeyLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::Abbey, seed, AbbeyWidth, AbbeyHeight, TerrainId::Rock);
		builder.Fill({ 14, 1, 32, 10 }, TerrainId::Grass, 0.0f);
		builder.Fill({ 26, 1, 8, 10 }, TerrainId::Dirt);
		builder.Fill({ 14, 11, 32, 23 }, TerrainId::Cobblestone, 0.0f);
		builder.Fill({ 3, 13, 11, 19 }, TerrainId::Grass, 0.0f);
		builder.Fill({ 14, 20, 1, 3 }, TerrainId::Grass);
		builder.Fill({ 46, 12, 11, 13 }, TerrainId::Dirt, 0.0f);
		builder.Fill({ 45, 15, 1, 3 }, TerrainId::Dirt);
		builder.Fill({ 20, 34, 20, 14 }, TerrainId::Flagstone, 0.0f);
		builder.Fill({ 29, 34, 2, 1 }, TerrainId::Flagstone);
		builder.SetPlayerSpawn({ 30.0f, 5.0f });

		builder.Wall(WallId::AbbeyWall, { 14.0f, 11.0f }, { 28.5f, 11.0f });
		builder.Wall(WallId::AbbeyWall, { 31.5f, 11.0f }, { 46.0f, 11.0f });
		builder.Wall(WallId::AbbeyWall, { 14.0f, 11.0f }, { 14.0f, 20.0f });
		builder.Wall(WallId::AbbeyWall, { 14.0f, 23.0f }, { 14.0f, 34.0f });
		builder.Wall(WallId::AbbeyWall, { 46.0f, 11.0f }, { 46.0f, 15.0f });
		builder.Wall(WallId::AbbeyWall, { 46.0f, 18.0f }, { 46.0f, 34.0f });
		builder.Wall(WallId::AbbeyWall, { 14.0f, 34.0f }, { 20.0f, 34.0f });
		builder.Wall(WallId::AbbeyWall, { 40.0f, 34.0f }, { 46.0f, 34.0f });
		builder.Wall(WallId::AbbeyWall, { 3.0f, 13.0f }, { 14.0f, 13.0f });
		builder.Wall(WallId::AbbeyWall, { 3.0f, 32.0f }, { 14.0f, 32.0f });
		builder.Wall(WallId::AbbeyWall, { 3.0f, 13.0f }, { 3.0f, 32.0f });
		builder.Wall(WallId::AbbeyWall, { 46.0f, 12.0f }, { 57.0f, 12.0f });
		builder.Wall(WallId::AbbeyWall, { 46.0f, 25.0f }, { 57.0f, 25.0f });
		builder.Wall(WallId::AbbeyWall, { 57.0f, 12.0f }, { 57.0f, 25.0f });
		builder.Wall(WallId::CryptWall, { 20.0f, 34.0f }, { 28.8f, 34.0f });
		builder.Wall(WallId::CryptWall, { 31.2f, 34.0f }, { 40.0f, 34.0f });
		builder.Wall(WallId::CryptWall, { 20.0f, 34.0f }, { 20.0f, 48.0f });
		builder.Wall(WallId::CryptWall, { 40.0f, 34.0f }, { 40.0f, 48.0f });
		builder.Wall(WallId::CryptWall, { 20.0f, 48.0f }, { 40.0f, 48.0f });
		builder.Roof({ 20, 34, 20, 14 }, 3.0f);

		builder.Prop(PropId::IronGate, { 30.0f, 11.0f }, FaceSouth);
		builder.Prop(PropId::CryptDoor, { 30.0f, 34.0f }, FaceSouth);
		builder.Reserve({ 28, 1, 4, 47 });

		builder.Prop(PropId::FirePit, { 30.0f, 20.5f });
		for (const Vec2 stool : { Vec2(27.7f, 19.6f), Vec2(32.4f, 19.8f), Vec2(28.6f, 22.9f) })
			builder.Prop(PropId::Stool, stool, std::atan2(30.0f - stool.x, 20.5f - stool.y));
		builder.Prop(PropId::Bucket, { 32.6f, 22.4f });
		builder.Prop(PropId::BronzeStatue, { 38.6f, 28.6f }, 0.6f);
		builder.Prop(PropId::MarbleBust, { 21.4f, 28.8f }, -0.5f);
		for (const Vec2 lantern : { Vec2(27.7f, 32.9f), Vec2(32.3f, 32.9f), Vec2(27.4f, 12.3f), Vec2(32.6f, 12.3f), Vec2(48.4f, 23.6f) })
			builder.Prop(PropId::LanternPost, lantern);
		builder.Prop(PropId::PlankTable, { 18.6f, 15.6f }, 0.12f);
		builder.Prop(PropId::Stool, { 17.4f, 14.3f }, 0.4f);
		builder.Prop(PropId::Stool, { 19.9f, 14.4f }, -0.3f);
		builder.Prop(PropId::WineBarrel, { 44.9f, 20.8f });
		builder.Prop(PropId::WineBarrel, { 44.8f, 22.0f }, 1.3f);
		builder.Prop(PropId::OldBarrel, { 43.7f, 21.3f }, 0.4f);
		builder.Prop(PropId::LongCrate, { 44.9f, 27.0f }, 0.05f);
		builder.Prop(PropId::LowCrate, { 43.4f, 28.4f }, 1.4f);
		builder.Prop(PropId::OldBarrel, { 44.9f, 29.3f }, 2.2f);
		builder.Prop(PropId::LowCrate, { 15.5f, 25.6f }, 1.6f);
		builder.Prop(PropId::OldBarrel, { 15.4f, 27.0f }, 0.9f);
		builder.Prop(PropId::WineBarrel, { 15.5f, 30.9f }, 0.2f);
		builder.Prop(PropId::Bucket, { 16.6f, 31.9f });

		builder.Prop(PropId::MossBoulder, { 6.2f, 29.0f }, 0.7f);
		builder.Prop(PropId::MossRock, { 9.6f, 27.6f }, 2.1f);
		builder.Prop(PropId::MossRock, { 5.2f, 19.0f }, 4.0f);
		builder.Prop(PropId::TreeStump, { 8.4f, 23.4f }, 1.1f);
		builder.Prop(PropId::DeadLog, { 7.0f, 15.6f }, 0.25f);
		Scatter(builder, { 4, 14, 9, 17 }, PropId::Fern, 34, 0.15f);
		Scatter(builder, { 4, 14, 9, 17 }, PropId::DryBranches, 5, 0.4f);
		Scatter(builder, { 4, 14, 9, 17 }, PropId::GrassTuft, 190, 0.02f);
		Scatter(builder, { 4, 14, 9, 17 }, PropId::Weeds, 60, 0.03f);

		builder.Prop(PropId::WineBarrel, { 54.6f, 22.6f });
		builder.Prop(PropId::WineBarrel, { 55.5f, 21.7f }, 0.8f);
		builder.Prop(PropId::WineBarrel, { 53.6f, 23.4f }, 1.9f);
		builder.Prop(PropId::OldBarrel, { 51.0f, 23.7f }, 0.3f);
		builder.Prop(PropId::LongCrate, { 55.9f, 15.6f }, 0.0f);
		builder.Prop(PropId::LowCrate, { 54.4f, 13.9f }, 0.2f);
		builder.Prop(PropId::Bucket, { 50.2f, 14.9f });
		Scatter(builder, { 47, 13, 9, 11 }, PropId::Weeds, 10, 0.1f);

		for (const float y : { 37.6f, 41.0f, 44.4f })
		{
			builder.Prop(PropId::CryptPillar, { 25.0f, y });
			builder.Prop(PropId::CryptPillar, { 35.0f, y });
		}
		builder.Prop(PropId::Sarcophagus, { 22.4f, 39.3f }, FaceEast);
		builder.Prop(PropId::Sarcophagus, { 22.4f, 42.8f }, FaceEast);
		builder.Prop(PropId::Sarcophagus, { 37.6f, 39.3f }, FaceWest);
		builder.Prop(PropId::Sarcophagus, { 37.6f, 42.8f }, FaceWest);
		builder.Prop(PropId::Altar, { 30.0f, 46.4f }, FaceSouth);
		builder.Prop(PropId::TreasureChest, { 34.2f, 46.6f }, 0.15f);
		for (const Vec2 candle : { Vec2(27.6f, 46.6f), Vec2(32.4f, 46.6f), Vec2(22.4f, 41.05f), Vec2(37.6f, 41.05f), Vec2(25.9f, 36.2f), Vec2(34.1f, 36.2f) })
			builder.Prop(PropId::CandleStand, candle);
		for (const Vec2 bowl : { Vec2(26.6f, 44.6f), Vec2(33.4f, 44.6f), Vec2(23.0f, 36.0f), Vec2(37.0f, 36.0f) })
			builder.Prop(PropId::FireBowl, bowl);

		Hem(builder, { 15.0f, 11.0f }, { 27.5f, 11.0f }, { 0.0f, 1.0f }, 0.55f);
		Hem(builder, { 32.5f, 11.0f }, { 45.0f, 11.0f }, { 0.0f, 1.0f }, 0.55f);
		Hem(builder, { 14.0f, 12.0f }, { 14.0f, 33.0f }, { 1.0f, 0.0f }, 0.5f);
		Hem(builder, { 46.0f, 12.0f }, { 46.0f, 33.0f }, { -1.0f, 0.0f }, 0.5f);
		Hem(builder, { 15.0f, 34.0f }, { 19.5f, 34.0f }, { 0.0f, -1.0f }, 0.7f);
		Hem(builder, { 40.5f, 34.0f }, { 45.0f, 34.0f }, { 0.0f, -1.0f }, 0.7f);
		Hem(builder, { 15.0f, 11.0f }, { 45.0f, 11.0f }, { 0.0f, -1.0f }, 0.45f);
		Scatter(builder, { 15, 2, 11, 8 }, PropId::GrassTuft, 40, 0.05f);
		Scatter(builder, { 34, 2, 11, 8 }, PropId::GrassTuft, 40, 0.05f);
		Scatter(builder, { 15, 2, 11, 8 }, PropId::Weeds, 14, 0.1f);
		Scatter(builder, { 34, 2, 11, 8 }, PropId::Weeds, 14, 0.1f);
		builder.Prop(PropId::MossRock, { 18.0f, 4.5f }, 1.3f);
		builder.Prop(PropId::TreeStump, { 41.5f, 6.0f }, 2.6f);
		builder.Prop(PropId::DeadLog, { 38.5f, 3.2f }, -0.35f);

		for (const Vec2 spawn : { Vec2(8.0f, 25.0f), Vec2(52.0f, 18.0f), Vec2(30.0f, 41.0f), Vec2(20.0f, 6.0f), Vec2(40.0f, 6.0f) })
			builder.SpawnPoint(spawn);
		builder.Region("Vesper Abbey", { 0, 0, AbbeyWidth, AbbeyHeight }, true);
		return builder.Finish();
	}
}
