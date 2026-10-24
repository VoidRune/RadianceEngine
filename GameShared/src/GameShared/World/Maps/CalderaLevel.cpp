#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr int32_t CalderaSize = 48;
		constexpr Vec2 Center = { 24.0f, 24.0f };
		constexpr float PlatformRadius = 13.3f;
		constexpr float LakeRadius = 20.5f;

		Vec2 Around(float radius, float angle)
		{
			return Center + FromAngle(angle) * radius;
		}
	}

	Level BuildCalderaLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::Caldera, seed, CalderaSize, CalderaSize, TerrainId::Rock);
		builder.Disc(Center, LakeRadius, TerrainId::Lava);
		builder.Disc(Center, PlatformRadius, TerrainId::Basalt);
		builder.SetPlayerSpawn(Center);

		for (int32_t i = 0; i < 7; i++)
		{
			const float angle = 0.4f + float(i) * 2.0f * Pi / 7.0f;
			builder.Prop(i % 2 == 0 ? PropId::Boulder : PropId::Rock, Around(16.8f + 0.9f * float(i % 3), angle), angle * 1.7f);
		}
		for (int32_t i = 0; i < 4; i++)
			builder.Prop(PropId::ObsidianCluster, Around(18.6f, 1.2f + float(i) * Pi * 0.5f), float(i) * 1.7f);
		for (int32_t i = 0; i < 12; i++)
		{
			const float angle = float(i) * Pi / 6.0f + Pi / 12.0f;
			builder.Prop(i % 3 == 0 ? PropId::Brazier : PropId::FireBowl, Around(21.6f, angle), angle);
		}

		builder.Region("The Caldera", { 0, 0, CalderaSize, CalderaSize }, false);
		return builder.Finish();
	}
}
