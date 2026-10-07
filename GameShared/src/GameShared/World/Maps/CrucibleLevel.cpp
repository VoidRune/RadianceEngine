#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr int32_t CrucibleSize = 56;
		constexpr Vec2 Center = { 28.0f, 28.0f };
		constexpr float DaisHeight = 0.5f;

		Vec2 Around(float radius, float angle)
		{
			return Center + FromAngle(angle) * radius;
		}

		float Facing(Vec2 position)
		{
			const Vec2 inward = Center - position;
			return std::atan2(inward.x, -inward.y);
		}
	}

	Level BuildCrucibleLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::Crucible, seed, CrucibleSize, CrucibleSize, TerrainId::Rock);
		builder.Disc(Center, 24.0f, TerrainId::Dirt);
		builder.Disc(Center, 19.5f, TerrainId::Flagstone);
		builder.Disc(Center, 6.5f, TerrainId::Cobblestone);
		builder.Fill({ 24, 24, 8, 8 }, TerrainId::Marble, DaisHeight);
		builder.Stairs({ 27, 22, 2, 2 }, Direction::North, 0.0f, DaisHeight, TerrainId::Marble);
		builder.Stairs({ 27, 32, 2, 2 }, Direction::South, 0.0f, DaisHeight, TerrainId::Marble);
		builder.Stairs({ 22, 27, 2, 2 }, Direction::East, 0.0f, DaisHeight, TerrainId::Marble);
		builder.Stairs({ 32, 27, 2, 2 }, Direction::West, 0.0f, DaisHeight, TerrainId::Marble);
		builder.Reserve({ 21, 21, 14, 14 });
		builder.SetPlayerSpawn({ 28.0f, 8.0f });

		for (int32_t i = 0; i < 4; i++)
		{
			const float angle = Pi * 0.25f + float(i) * Pi * 0.5f;
			const Vec2 pool = Around(14.5f, angle);
			builder.Disc(pool, 3.1f, TerrainId::Water);
			builder.Prop(PropId::Statue, Around(21.6f, angle), Facing(Around(21.6f, angle)));
			for (const float side : { -1.0f, 1.0f })
				builder.Prop(PropId::Rock, pool + FromAngle(angle + side * 1.9f) * 3.6f, angle);
		}

		for (int32_t i = 0; i < 8; i++)
		{
			const float angle = Pi / 8.0f + float(i) * Pi * 0.25f;
			builder.Prop(i % 3 == 1 ? PropId::BrokenPillar : PropId::StonePillar, Around(9.0f, angle), angle);
		}

		for (int32_t i = 0; i < 8; i++)
		{
			const float angle = Pi / 8.0f + float(i) * Pi * 0.25f;
			const Vec2 middle = Around(16.0f, angle);
			const Vec2 along = FromAngle(angle + Pi * 0.5f) * 1.6f;
			builder.Wall(WallId::Stone, middle - along, middle + along);
		}

		for (int32_t i = 0; i < 12; i++)
		{
			const float angle = float(i) * Pi / 6.0f + Pi / 12.0f;
			builder.Prop(PropId::Brazier, Around(23.0f, angle));
		}
		for (int32_t i = 0; i < 4; i++)
		{
			const float angle = float(i) * Pi * 0.5f;
			for (const float side : { -0.12f, 0.12f })
			{
				const Vec2 banner = Around(23.2f, angle + side);
				builder.Prop(PropId::Banner, banner, Facing(banner));
			}
		}
		for (const Vec2 corner : { Vec2(25.0f, 25.0f), Vec2(31.0f, 25.0f), Vec2(25.0f, 31.0f), Vec2(31.0f, 31.0f) })
			builder.Prop(PropId::Brazier, corner);

		builder.Pad(Center, PadKind::Power);
		builder.Pad(Around(12.0f, Pi * 0.5f), PadKind::Health);
		builder.Pad(Around(12.0f, -Pi * 0.5f), PadKind::Health);
		builder.Pad(Around(12.0f, 0.0f), PadKind::Mana);
		builder.Pad(Around(12.0f, Pi), PadKind::Mana);
		for (int32_t i = 0; i < 8; i++)
			builder.SpawnPoint(Around(20.0f, float(i) * Pi * 0.25f));

		builder.Region("The Crucible", { 4, 4, 48, 48 }, false);
		return builder.Finish();
	}
}
