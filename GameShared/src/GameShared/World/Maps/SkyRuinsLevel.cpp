#include "GameShared/Content/Brawl.h"
#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <cmath>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr int32_t RuinsWidth = 84;
		constexpr int32_t RuinsHeight = 72;
		constexpr Vec2 Center = { 42.0f, 36.0f };
		constexpr float TempleRadius = 11.5f;
		constexpr float IsletDistance = 20.0f;
		constexpr float IsletRadius = 4.2f;
		constexpr float IslandRadius = 3.1f;
		constexpr float IslandReach = 4.6f;
		constexpr float IslandOverlap = 0.35f;
		constexpr float CausewayHalfWidth = 1.25f;
		constexpr uint32_t Islets = 3;

		template<typename Inside>
		void Paint(LevelBuilder& builder, Inside&& inside)
		{
			TileMap& map = builder.GetMap();
			for (int32_t y = 1; y < map.GetHeight() - 1; y++)
				for (int32_t x = 1; x < map.GetWidth() - 1; x++)
					if (inside(CellSeed({ x, y })))
						builder.Fill({ x, y, 1, 1 }, TerrainId::Skystone);
		}

		bool OnStone(const TileMap& map, Vec2 point, float margin)
		{
			const auto stone = [&](Vec2 sample)
			{
				const CellCoord cell = CellAt(sample);
				return map.GetTerrain(cell.X, cell.Y) == TerrainId::Skystone;
			};
			if (!stone(point))
				return false;
			for (int32_t i = 0; i < 8; i++)
				if (!stone(point + FromAngle(float(i) * (Pi * 0.25f)) * margin))
					return false;
			return true;
		}

		bool Place(LevelBuilder& builder, PropId id, Vec2 position, float yaw, float margin, float spacing = 0.3f)
		{
			return OnStone(builder.GetMap(), position, margin) && builder.TryProp(id, position, yaw, spacing);
		}
	}

	Level BuildSkyRuinsLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::SkyRuins, seed, RuinsWidth, RuinsHeight, TerrainId::Sky);
		Random& random = builder.GetRandom();
		const Lobe temple = MakeLobe(random, Center, TempleRadius, 0.07f);
		Paint(builder, [&](Vec2 seedPoint) { return temple.Contains(seedPoint); });

		const float turn = random.Float() * 2.0f * Pi;
		std::vector<Lobe> islets;
		std::vector<float> isletAngles;
		for (uint32_t i = 0; i < Islets; i++)
		{
			const float angle = turn + float(i) * 2.0f * Pi / float(Islets) + (random.Float() - 0.5f) * 0.3f;
			const Vec2 direction = FromAngle(angle);
			const Lobe islet = MakeLobe(random, Center + direction * IsletDistance, IsletRadius * (0.9f + 0.2f * random.Float()), 0.12f);
			const Vec2 from = Center + direction * (temple.Reach(angle) - 1.0f);
			const Vec2 to = islet.Middle - direction * (islet.Radius - 1.0f);
			const float sway = (random.Float() - 0.5f) * 0.6f;
			Paint(builder, [&](Vec2 seedPoint)
			{
				const float along = std::clamp(Dot(seedPoint - from, to - from) / LengthSquared(to - from), 0.0f, 1.0f);
				const Vec2 axis = Lerp(from, to, along) + Perpendicular(direction) * (sway * std::sin(along * Pi));
				return islet.Contains(seedPoint) || Length(seedPoint - axis) < CausewayHalfWidth;
			});
			islets.push_back(islet);
			isletAngles.push_back(angle);
		}

		const TileMap& map = builder.GetMap();
		std::vector<DriftIsland> islands;
		for (uint32_t i = 0; i < Islets; i++)
		{
			const float angle = turn + (float(i) + 0.5f) * 2.0f * Pi / float(Islets) + (random.Float() - 0.5f) * 0.2f;
			const Vec2 direction = FromAngle(angle);
			float rim = temple.Reach(angle) - 1.0f;
			while (rim < TempleRadius * 2.0f)
			{
				const CellCoord cell = CellAt(Center + direction * rim);
				if (map.GetTerrain(cell.X, cell.Y) != TerrainId::Skystone)
					break;
				rim += 0.1f;
			}
			const float radius = IslandRadius * (0.92f + 0.16f * random.Float());
			const Lobe shape = MakeLobe(random, Center + direction * (rim + radius - IslandOverlap), radius, 0.1f);
			DriftIsland island;
			island.Center = shape.Middle;
			island.Axis = direction;
			island.Reach = IslandReach * (0.9f + 0.2f * random.Float());
			island.Period = 18.0f + 8.0f * random.Float();
			island.Phase = random.Float();
			for (int32_t y = int32_t(shape.Middle.y - radius) - 2; y <= int32_t(shape.Middle.y + radius) + 2; y++)
				for (int32_t x = int32_t(shape.Middle.x - radius) - 2; x <= int32_t(shape.Middle.x + radius) + 2; x++)
					if (map.GetTerrain(x, y) == TerrainId::Sky && shape.Contains(CellSeed({ x, y })))
						island.Cells.push_back({ x, y });
			islands.push_back(std::move(island));
		}

		for (uint32_t i = 0; i < Islets; i++)
		{
			const Vec2 direction = FromAngle(isletAngles[i]);
			for (float inset = 1.6f; inset < 3.5f; inset += 0.3f)
				if (builder.TryProp(PropId::RuinArch, Center + direction * (temple.Reach(isletAngles[i]) - inset), AngleOf(Perpendicular(direction)), 0.1f))
					break;
		}
		const float colonnade = 6.6f;
		for (uint32_t i = 0; i < 10; i++)
		{
			const float angle = turn + float(i) * 2.0f * Pi / 10.0f;
			const PropId kind = i % 5 == 1 ? PropId::RuinStump : i % 3 == 0 ? PropId::RuinBrokenColumn : PropId::RuinColumn;
			if (i % 5 != 3)
				Place(builder, kind, Center + FromAngle(angle) * colonnade, random.Float() * 2.0f * Pi, 0.6f);
			else
				Place(builder, PropId::RuinDrum, Center + FromAngle(angle) * colonnade, angle + Pi * 0.5f + (random.Float() - 0.5f) * 0.6f, 0.7f);
		}
		Place(builder, PropId::RuinPedestal, Center, turn, 0.8f);
		for (uint32_t i = 0; i < 4; i++)
		{
			const float angle = turn + Pi / 3.0f + float(i) * Pi * 0.5f + (random.Float() - 0.5f) * 0.4f;
			const Vec2 spot = Center + FromAngle(angle) * (9.0f + random.Float() * 0.6f);
			Place(builder, i % 2 == 0 ? PropId::RuinWall : PropId::RuinWallShort, spot, angle + Pi * 0.5f, 0.9f);
		}
		for (uint32_t i = 0; i < Islets; i++)
		{
			const Vec2 direction = FromAngle(isletAngles[i]);
			const Lobe& islet = islets[i];
			const PropId feature[] = { PropId::RuinPedestal, PropId::RuinWall, PropId::RuinColumn };
			Place(builder, feature[i % std::size(feature)], islet.Middle + direction * 0.6f, isletAngles[i] + Pi * 0.5f, 0.9f);
			for (uint32_t k = 0; k < 3; k++)
			{
				const float around = isletAngles[i] + Pi * 0.5f + float(k) * Pi * 0.5f + (random.Float() - 0.5f) * 0.5f;
				const PropId kind = k == 1 ? PropId::RuinCapital : k == 2 ? PropId::RuinStump : PropId::RuinBrokenColumn;
				Place(builder, kind, islet.Middle + FromAngle(around) * (islet.Radius * 0.55f), random.Float() * 2.0f * Pi, 0.6f);
			}
		}
		for (uint32_t attempt = 0, placed = 0; attempt < 300 && placed < 26; attempt++)
		{
			const Vec2 spot = Center + FromAngle(random.Float() * 2.0f * Pi) * (3.0f + random.Float() * 20.0f);
			if (Place(builder, PropId::RuinRubble, spot, random.Float() * 2.0f * Pi, 0.5f, 0.8f))
				placed++;
		}

		builder.SetPlayerSpawn(Center);
		builder.Region("The Sky Ruins", { 0, 0, RuinsWidth, RuinsHeight }, false);
		Level level = builder.Finish();
		level.Skyborne = true;
		level.Islands = std::move(islands);
		return level;
	}
}
