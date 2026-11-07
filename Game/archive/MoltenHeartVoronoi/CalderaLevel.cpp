#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include "GameShared/Content/Brawl.h"
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

		float Jitter(uint32_t seed, int32_t index, int32_t salt)
		{
			return CollapseNoise(index, salt, seed) - 0.5f;
		}
	}

	Level BuildCalderaLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::Caldera, seed, CalderaSize, CalderaSize, TerrainId::Rock);
		builder.Disc(Center, LakeRadius, TerrainId::Lava);
		builder.Disc(Center, PlatformRadius, TerrainId::Basalt);
		builder.SetPlayerSpawn(Center);

		const float turn = 2.0f * Pi * CollapseNoise(1, 0x7E57, seed);
		for (int32_t i = 0; i < 3; i++)
		{
			const float angle = turn + float(i) * 2.0f * Pi / 3.0f + 0.35f * Jitter(seed, i, 0x7E01);
			builder.Prop(PropId::SulfurVent, Around(5.2f + 1.0f * Jitter(seed, i, 0x7E02), angle), 2.0f * Pi * CollapseNoise(i, 0x7E03, seed));
		}
		for (int32_t i = 0; i < 2; i++)
		{
			const float angle = turn + Pi / 3.0f + float(i) * 2.0f * Pi / 3.0f + 0.3f * Jitter(seed, i, 0x7E11);
			builder.Prop(PropId::ObsidianCluster, Around(6.6f + 0.8f * Jitter(seed, i, 0x7E12), angle), 2.0f * Pi * CollapseNoise(i, 0x7E13, seed));
		}
		const float anchorTurn = turn + Pi / 6.0f;
		for (int32_t i = 0; i < 4; i++)
		{
			const float angle = anchorTurn + float(i) * 0.5f * Pi + 0.2f * Jitter(seed, i, 0x7E22);
			builder.Prop(PropId::MoltenAnchor, Around(8.4f + 0.5f * Jitter(seed, i, 0x7E23), angle), angle);
		}
		builder.Prop(PropId::MoltenCone, Around(17.4f, 0.5f * Pi + 0.25f * Jitter(seed, 0, 0x7E31)), 0.0f);

		builder.Region("The Molten Heart", { 0, 0, CalderaSize, CalderaSize }, false);
		return builder.Finish();
	}
}
