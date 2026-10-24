#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <cmath>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr int32_t LakeWidth = 64;
		constexpr int32_t LakeHeight = 48;
		constexpr Vec2 Center = { 32.0f, 24.0f };
		constexpr Vec2 WaterAxes = { 27.0f, 16.5f };
		constexpr Vec2 IceAxes = { 21.5f, 12.3f };

		struct Outline
		{
			Vec2 Axes;
			float Phase[3] = {};
			float Wobble = 0.0f;

			float Measure(Vec2 point) const
			{
				const Vec2 offset = point - Center;
				const float angle = std::atan2(offset.y, offset.x);
				const float scale = 1.0f + Wobble * (0.6f * std::sin(3.0f * angle + Phase[0]) + 0.3f * std::sin(5.0f * angle + Phase[1]) + 0.2f * std::sin(8.0f * angle + Phase[2]));
				return Length(Vec2(offset.x / Axes.x, offset.y / Axes.y)) / scale;
			}
		};

		Outline MakeOutline(Random& random, Vec2 axes, float wobble)
		{
			Outline outline{ axes };
			for (float& phase : outline.Phase)
				phase = random.Float() * 2.0f * Pi;
			outline.Wobble = wobble;
			return outline;
		}

		Vec2 OnEllipse(Vec2 axes, float angle, float fraction)
		{
			return Center + Vec2(std::cos(angle) * axes.x, std::sin(angle) * axes.y) * fraction;
		}
	}

	Level BuildFrozenLakeLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::FrozenLake, seed, LakeWidth, LakeHeight, TerrainId::Rock);
		Random& random = builder.GetRandom();
		const Outline water = MakeOutline(random, WaterAxes, 0.07f);
		const Outline ice = MakeOutline(random, IceAxes, 0.04f);
		for (int32_t y = 0; y < LakeHeight; y++)
			for (int32_t x = 0; x < LakeWidth; x++)
			{
				const Vec2 point(float(x) + 0.5f, float(y) + 0.5f);
				if (ice.Measure(point) < 1.0f)
					builder.Fill({ x, y, 1, 1 }, TerrainId::Ice);
				else if (water.Measure(point) < 1.0f)
					builder.Fill({ x, y, 1, 1 }, TerrainId::Icewater);
			}

		const float drift = random.Float() * 2.0f * Pi;
		for (int32_t i = 0; i < 5; i++)
		{
			const float angle = drift + float(i) * 2.0f * Pi / 5.0f + (random.Float() - 0.5f) * 0.6f;
			const Vec2 spot = OnEllipse(IceAxes, angle, 0.42f + 0.3f * random.Float());
			const float radius = 1.3f + 0.7f * random.Float();
			for (int32_t y = int32_t(spot.y - radius) - 1; y <= int32_t(spot.y + radius) + 1; y++)
				for (int32_t x = int32_t(spot.x - radius) - 1; x <= int32_t(spot.x + radius) + 1; x++)
					if (builder.GetMap().GetTerrain(x, y) == TerrainId::Ice && LengthSquared(Vec2(float(x) + 0.5f, float(y) + 0.5f) - spot) < radius * radius)
						builder.Fill({ x, y, 1, 1 }, TerrainId::Snow);
		}

		std::vector<Vec2> pillars;
		for (int32_t attempt = 0; attempt < 200 && pillars.size() < 7; attempt++)
		{
			const Vec2 spot = OnEllipse(IceAxes, random.Float() * 2.0f * Pi, 0.22f + 0.45f * std::sqrt(random.Float()));
			bool spaced = Length(spot - Center) > 3.0f;
			for (const Vec2& other : pillars)
				spaced = spaced && Length(spot - other) > 5.0f;
			if (!spaced)
				continue;
			pillars.push_back(spot);
			builder.Prop(PropId::IcePillar, spot, random.Float() * 2.0f * Pi);
		}

		for (int32_t i = 0; i < 14; i++)
		{
			const float angle = float(i) * 2.0f * Pi / 14.0f + (random.Float() - 0.5f) * 0.3f;
			const Vec2 shore = OnEllipse(WaterAxes, angle, 1.04f + 0.04f * random.Float());
			builder.Prop(PropId::SnowRock, shore, random.Float() * 2.0f * Pi);
		}
		for (int32_t i = 0; i < 8; i++)
		{
			const float angle = float(i) * 2.0f * Pi / 8.0f + Pi / 8.0f;
			builder.Prop(i % 2 == 0 ? PropId::Brazier : PropId::FireBowl, OnEllipse(WaterAxes, angle, 1.09f), angle);
		}
		for (int32_t y = 1; y < LakeHeight - 1; y += 2)
			for (int32_t x = 1; x < LakeWidth - 1; x += 2)
			{
				const Vec2 point(float(x) + random.Float(), float(y) + random.Float());
				const float distance = water.Measure(point);
				if (distance < 1.18f || random.Float() > 0.42f)
					continue;
				builder.Prop(PropId::PineTree, point, random.Float() * 2.0f * Pi);
			}

		builder.SetPlayerSpawn(Center);
		builder.Region("The Frozen Lake", { 0, 0, LakeWidth, LakeHeight }, false);
		Level level = builder.Finish();
		level.Bouncy = true;
		return level;
	}
}
