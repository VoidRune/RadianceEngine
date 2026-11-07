#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include "GameShared/Content/Brawl.h"
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr int32_t CalderaSize = 48;
		constexpr float PitRadius = 23.0f;
	}

	Level BuildCalderaLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::Caldera, seed, CalderaSize, CalderaSize, TerrainId::Rock);
		builder.Disc(ForgeCenter, PitRadius, TerrainId::Lava);
		for (int32_t y = 0; y < CalderaSize; y++)
			for (int32_t x = 0; x < CalderaSize; x++)
				if (OnForgePlate(Vec2(float(x) + 0.5f, float(y) + 0.5f)))
					builder.Fill({ x, y, 1, 1 }, TerrainId::Basalt);
		builder.SetPlayerSpawn(ForgeCenter);
		builder.Region("The Molten Heart", { 0, 0, CalderaSize, CalderaSize }, false);
		return builder.Finish();
	}
}
