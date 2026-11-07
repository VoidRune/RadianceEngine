#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <algorithm>
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

	LakeLayout MakeLakeLayout(uint32_t seed)
	{
		Random random(uint64_t(seed) * 0x9E3779B97F4A7C15ull ^ 0x7A3E1ull);
		LakeLayout layout;
		layout.Side = random.Float() < 0.5f ? -1.0f : 1.0f;
		layout.Mouth = -7.0f + 6.5f * random.Float();
		const float s = layout.Side;
		const float middle = -6.0f + 9.5f * random.Float();
		const float c = -s;
		float z = -6.0f + 11.0f * random.Float();
		const float xs[] = { 42.0f, 35.0f, 27.5f, 18.5f, 9.5f };
		for (size_t i = 0; i < layout.Trench.size(); i++)
		{
			layout.Trench[i] = { c * xs[i], z };
			z = std::clamp(z + (random.Float() - 0.5f) * 5.0f, -7.0f, 6.0f);
		}
		for (size_t i = 0; i < layout.Trench.size(); i++)
			layout.Route[i] = layout.Trench[i];
		layout.Route[5] = { 0.0f, middle };
		layout.Route[6] = { s * 11.0f, 0.5f * (layout.Mouth + middle) + (random.Float() - 0.5f) * 3.0f };
		layout.Route[7] = { s * 22.0f, layout.Channel(s * 22.0f) };
		layout.Route[8] = { s * 34.0f, layout.Channel(s * 34.0f) };
		layout.Route[9] = { s * 52.0f, layout.Channel(s * 52.0f) };
		return layout;
	}

	Level BuildFrozenLakeLevel(uint32_t seed)
	{
		const LakeLayout layout = MakeLakeLayout(seed);
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
				else if (water.Measure(point) < 1.0f || layout.InChannel(point - Center))
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

		const float side = random.Float() < 0.5f ? -1.0f : 1.0f;
		const Vec2 grave = Center + Vec2(side * LakeGraveX, LakeGraveZ);
		const Vec2 bankCentre = Center + Vec2(side * LakeBankX, LakeBankZ);
		for (int32_t row = -5; row <= 5; row++)
			for (int32_t column = -11; column <= 11; column++)
			{
				const Vec2 offset(1.65f * (float(column) + (row % 2 != 0 ? 0.5f : 0.0f)), 1.43f * float(row));
				if (offset.x * offset.x / (LakeBankHalfX * LakeBankHalfX) + offset.y * offset.y / (LakeBankHalfZ * LakeBankHalfZ) < 0.78f)
					builder.Prop(PropId::GraveBank, bankCentre + offset, 0.0f);
			}
		const Vec2 skull = grave + Vec2(-side * 9.0f, LakeSkullZ);
		const float skullYaw = side > 0.0f ? 2.55f : -2.55f;
		const Vec2 snout(std::sin(skullYaw), std::cos(skullYaw));
		builder.Prop(PropId::GraveSkull, skull - snout * (0.3f * LakeSkullScale), skullYaw);
		builder.Prop(PropId::GraveSnout, skull + snout * (1.7f * LakeSkullScale), skullYaw);
		for (int32_t i = 0; i < 12; i++)
		{
			const float angle = Pi * (1.05f + 0.9f * (float(i) + random.Float()) / 12.0f);
			const Vec2 rim(std::cos(angle) * LakeBankHalfX * 1.0f, std::sin(angle) * LakeBankHalfZ * 1.0f);
			builder.Prop(i % 3 == 2 ? PropId::GraveShard : PropId::GraveSlab, bankCentre + rim, random.Float() * 2.0f * Pi);
		}

		for (int32_t i = 0; i < 14; i++)
		{
			const float angle = float(i) * 2.0f * Pi / 14.0f + (random.Float() - 0.5f) * 0.3f;
			const Vec2 shore = OnEllipse(WaterAxes, angle, 1.04f + 0.04f * random.Float());
			const float yaw = random.Float() * 2.0f * Pi;
			if (!layout.InChannel(shore - Center, 2.5f) && layout.TrenchDistance(shore - Center) > 6.0f)
				builder.Prop(PropId::SnowRock, shore, yaw);
		}
		for (int32_t i = 0; i < 8; i++)
		{
			const float angle = float(i) * 2.0f * Pi / 8.0f + Pi / 8.0f;
			if (!layout.InChannel(OnEllipse(WaterAxes, angle, 1.09f) - Center, 2.5f) && layout.TrenchDistance(OnEllipse(WaterAxes, angle, 1.09f) - Center) > 6.0f)
				builder.Prop(i % 2 == 0 ? PropId::Brazier : PropId::FireBowl, OnEllipse(WaterAxes, angle, 1.09f), angle);
		}
		for (int32_t y = 1; y < LakeHeight - 1; y += 2)
			for (int32_t x = 1; x < LakeWidth - 1; x += 2)
			{
				const Vec2 point(float(x) + random.Float(), float(y) + random.Float());
				const float distance = water.Measure(point);
				if (distance < 1.18f || random.Float() > 0.42f || layout.InChannel(point - Center, 3.0f) || layout.TrenchDistance(point - Center) < 9.0f)
					continue;
				builder.Prop(PropId::PineTree, point, random.Float() * 2.0f * Pi);
			}

		const Vec2 pier = layout.PierRoot();
		for (int32_t k = 0; k < 3; k++)
			builder.Prop(PropId::HarborPier, Center + pier + Vec2(0.0f, -1.8f * float(k) - 0.9f), 0.0f);
		builder.Prop(PropId::HarborPost, Center + pier + Vec2(-0.95f, -5.4f), 0.0f);
		builder.Prop(PropId::HarborPost, Center + pier + Vec2(0.95f, -5.4f), 0.0f);
		builder.Prop(PropId::HarborPost, Center + pier + Vec2(-0.95f, -7.2f), 0.0f);
		builder.Prop(PropId::HarborBoat, Center + layout.Boat(), 0.0f);

		builder.SetPlayerSpawn(Center);
		builder.Region("The Frozen Lake", { 0, 0, LakeWidth, LakeHeight }, false);
		Level level = builder.Finish();
		level.Bouncy = true;
		return level;
	}
}
