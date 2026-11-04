#include "GameShared/Content/Brawl.h"
#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr int32_t RuinsWidth = 64;
		constexpr int32_t RuinsHeight = 56;
		constexpr Vec2 Center = { 32.0f, 28.0f };
		constexpr float GridSpacing = 0.25f;
		constexpr float PropLane = 3.8f;
		constexpr float PropInner = 6.6f;
		constexpr float PropOuter = 11.2f;

		float Wobble(Random& random, float amount)
		{
			return (random.Float() - 0.5f) * 2.0f * amount;
		}

		float SegmentDistanceTo(Vec2 point, Vec2 a, Vec2 b)
		{
			const Vec2 along = b - a;
			const float t = std::clamp(Dot(point - a, along) / std::max(LengthSquared(along), 1e-8f), 0.0f, 1.0f);
			return Length(point - a - along * t);
		}

		Rotunda MakeHall(Random& random)
		{
			Rotunda hall;
			hall.Center = Center;
			hall.Radius = RotundaRadius;
			hall.HoleRadius = RotundaHole;
			const float phases[4] = { random.Float() * 6.28f, random.Float() * 6.28f, random.Float() * 6.28f, random.Float() * 6.28f };
			const auto holeAt = [&](float angle)
			{
				const float smooth = 0.34f * std::sin(angle * 3.0f + phases[0]) + 0.24f * std::sin(angle * 5.0f + phases[1]);
				const float jag = 0.16f * std::sin(angle * 11.0f + phases[2]) + 0.12f * std::sin(angle * 23.0f + phases[3]);
				return RotundaHole + smooth + jag;
			};
			for (const float degrees : { RotundaDoorAngle, 180.0f - RotundaDoorAngle, RotundaSouthBalcony + Wobble(random, 3.0f), 180.0f - RotundaSouthBalcony + Wobble(random, 3.0f) })
			{
				const float angle = degrees * Pi / 180.0f;
				const float radius = 2.4f + 0.35f * random.Float();
				hall.Balconies.push_back({ Center + FromAngle(angle) * (RotundaRadius + RotundaWallDepth + radius - 0.1f), radius, angle });
			}

			Plateau& plate = hall.Outline;
			plate.Origin = { 0.0f, 0.0f };
			plate.Spacing = GridSpacing;
			plate.Columns = int32_t(float(RuinsWidth) / GridSpacing) + 1;
			plate.Rows = int32_t(float(RuinsHeight) / GridSpacing) + 1;
			plate.Center = Center;
			plate.Rim.resize(size_t(plate.Columns) * size_t(plate.Rows));
			for (int32_t j = 0; j < plate.Rows; j++)
				for (int32_t i = 0; i < plate.Columns; i++)
				{
					const Vec2 point = plate.Origin + Vec2(float(i), float(j)) * GridSpacing;
					const Vec2 offset = point - Center;
					const float distance = Length(offset);
					float rim = std::min(RotundaRadius - distance, distance - holeAt(AngleOf(offset)));
					for (size_t b = 0; b < hall.Balconies.size(); b++)
					{
						const Rotunda::Balcony& balcony = hall.Balconies[b];
						const Vec2 door = Center + FromAngle(balcony.Angle) * (RotundaRadius - 0.3f);
						const float path = RotundaDoorHalf - SegmentDistanceTo(point, door, balcony.Center);
						const float own = std::max(balcony.Radius - Length(point - balcony.Center), path);
						rim = std::max(rim, own);
					}
					plate.Rim[size_t(j) * size_t(plate.Columns) + size_t(i)] = rim;
				}

			return hall;
		}

		bool OpenFloor(const Rotunda& hall, Vec2 point, float margin)
		{
			if (hall.Outline.RimDistance(point) < margin || Length(point - hall.Center) > RotundaRadius - margin)
				return false;
			for (const Rotunda::Balcony& balcony : hall.Balconies)
				if (SegmentDistanceTo(point, hall.Center + FromAngle(balcony.Angle) * RotundaRadius, balcony.Center) < RotundaDoorHalf + margin + 0.6f)
					return false;
			return true;
		}
	}

	Level BuildSkyRuinsLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::SkyRuins, seed, RuinsWidth, RuinsHeight, TerrainId::Sky);
		Random& random = builder.GetRandom();
		Rotunda hall = MakeHall(random);
		TileMap& map = builder.GetMap();
		for (int32_t y = 1; y < map.GetHeight() - 1; y++)
			for (int32_t x = 1; x < map.GetWidth() - 1; x++)
				if (hall.Outline.RimDistance(CellSeed({ x, y })) >= 0.0f)
					builder.Fill({ x, y, 1, 1 }, TerrainId::Skystone);

		const float wallRadius = RotundaRadius + 0.5f;
		const float halfBay = RotundaBayDegrees * 0.5f * Pi / 180.0f;
		const float chordHalf = RotundaRadius * std::tan(halfBay);
		for (int32_t k = 0; k < RotundaBaysPerSide; k++)
			for (const float side : { -1.0f, 1.0f })
			{
				const float angle = RotundaBayAngle(k, side);
				const Vec2 direction = FromAngle(angle);
				const Vec2 middle = Center + direction * wallRadius;
				const float yaw = angle + Pi * 0.5f;
				if (k == RotundaDoorBay)
				{
					const float jamb = 0.5f * (chordHalf + RotundaDoorHalf);
					const Vec2 along = FromAngle(yaw);
					builder.Prop(PropId::ObservatoryJamb, middle + along * jamb, yaw);
					builder.Prop(PropId::ObservatoryJamb, middle - along * jamb, yaw);
				}
				else
					builder.Prop(PropId::ObservatoryWall, middle, yaw);
				if (k > 0)
					builder.Prop(PropId::ObservatoryPier, Center + FromAngle(angle + side * halfBay) * (RotundaRadius + 0.1f));
				if (k == RotundaNicheBayA || k == RotundaNicheBayB)
					builder.Prop(PropId::ObservatoryStatue, Center + direction * (RotundaRadius - 0.5f), yaw + Pi);
			}
		builder.Prop(PropId::ObservatoryDais, Center + Vec2(0.0f, RotundaRadius - 0.3f));

		std::vector<Vec2> placed;
		const auto place = [&](PropId id, float yaw)
		{
			for (uint32_t attempt = 0; attempt < 60; attempt++)
			{
				const float angle = random.Float() * 2.0f * Pi;
				const float radius = PropInner + (PropOuter - PropInner) * random.Float();
				const Vec2 spot = Center + FromAngle(angle) * radius;
				bool crowded = !OpenFloor(hall, spot, 1.3f);
				for (const Vec2 other : placed)
					crowded = crowded || LengthSquared(other - spot) < PropLane * PropLane;
				if (!crowded && builder.TryProp(id, spot, yaw < 0.0f ? angle + Pi * 0.5f + Wobble(random, 0.45f) : yaw, 0.4f))
				{
					placed.push_back(spot);
					return;
				}
			}
		};
		place(PropId::RuinFallenColumn, -1.0f);
		place(PropId::RuinFallenColumn, -1.0f);
		place(PropId::ObservatoryInstrument, random.Float() * 2.0f * Pi);
		place(PropId::ObservatoryInstrument, random.Float() * 2.0f * Pi);
		place(random.Float() < 0.5f ? PropId::RuinStump : PropId::RuinBrokenColumn, random.Float() * 2.0f * Pi);

		for (uint32_t attempt = 0, debris = 0; attempt < 200 && debris < 8; attempt++)
		{
			const Vec2 spot = Center + FromAngle(random.Float() * 2.0f * Pi) * (5.6f + 7.0f * random.Float());
			if (OpenFloor(hall, spot, 0.9f) && builder.TryProp(PropId::RuinRubble, spot, random.Float() * 2.0f * Pi, 1.2f))
				debris++;
		}

		builder.SetPlayerSpawn(Center + Vec2(0.0f, -9.0f));
		builder.Region("The Sky Observatory", { 0, 0, RuinsWidth, RuinsHeight }, false);
		builder.SetRotunda(std::move(hall));
		Level level = builder.Finish();
		level.Skyborne = true;
		return level;
	}
}
