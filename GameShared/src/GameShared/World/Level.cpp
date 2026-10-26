#include "Level.h"
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float LedgeHalfThickness = 0.02f;
		constexpr float BoundaryDepth = 0.25f;

		void SettleTerrainLevels(TileMap& map)
		{
			for (int32_t y = 0; y < map.GetHeight(); y++)
			{
				for (int32_t x = 0; x < map.GetWidth(); x++)
				{
					Cell& cell = map.At(x, y);
					if (GetTerrain(cell.Terrain).Walkable)
						continue;
					int32_t highest = INT32_MIN;
					int32_t lowest = INT32_MAX;
					for (int32_t dy = -1; dy <= 1; dy++)
						for (int32_t dx = -1; dx <= 1; dx++)
						{
							if (!map.IsWalkable(x + dx, y + dy))
								continue;
							const Cell& neighbor = map.Get(x + dx, y + dy);
							highest = std::max(highest, int32_t(neighbor.Level) + (neighbor.Slope != Direction::None ? int32_t(neighbor.Climb) : 0));
							lowest = std::min(lowest, int32_t(neighbor.Level));
						}
					if (highest == INT32_MIN)
						continue;
					cell.Slope = Direction::None;
					cell.Climb = 0;
					cell.Level = int8_t(std::clamp(cell.Terrain == TerrainId::Rock ? highest : lowest, -127, 127));
				}
			}
		}

		void AddLedge(std::vector<Shape>& shapes, Vec2 center, Vec2 half)
		{
			shapes.push_back(MakeBox(center, half, 0.0f, Block::Movement));
		}

		void AddBoundary(std::vector<Boundary>& boundaries, const TileMap& map, Vec2 from, Vec2 to, TileCoord a, TileCoord b)
		{
			const Vec2 middle = (from + to) * 0.5f;
			const Vec2 normal = Normalize(Perpendicular(to - from));
			const Vec2 toward = Dot(Vec2(float(a.X) + 0.5f, float(a.Y) + 0.5f) - middle, normal) > 0.0f ? normal : normal * -1.0f;
			const bool walkA = map.IsWalkable(a.X, a.Y);
			const bool walkB = map.IsWalkable(b.X, b.Y);
			if (walkA && walkB)
			{
				const float heightA = map.GetHeightAt(a.X, a.Y, middle.x - float(a.X), middle.y - float(a.Y));
				const float heightB = map.GetHeightAt(b.X, b.Y, middle.x - float(b.X), middle.y - float(b.Y));
				boundaries.push_back({ from, to, heightA >= heightB ? toward : toward * -1.0f, BoundaryKind::Ledge });
				return;
			}
			const BoundaryKind kind = map.IsAbyss(a.X, a.Y) || map.IsAbyss(b.X, b.Y) ? BoundaryKind::Void : BoundaryKind::Blocked;
			boundaries.push_back({ from, to, walkA ? toward : toward * -1.0f, kind });
		}

		void TraceBoundaries(const TileMap& map, std::vector<Boundary>& boundaries)
		{
			const auto halves = [&](Vec2 from, Vec2 to, TileCoord a, TileCoord b)
			{
				const Vec2 middle = (from + to) * 0.5f;
				if (!map.IsCutVertex(int32_t(std::lround(from.x)), int32_t(std::lround(from.y))))
					AddBoundary(boundaries, map, from, middle, a, b);
				if (!map.IsCutVertex(int32_t(std::lround(to.x)), int32_t(std::lround(to.y))))
					AddBoundary(boundaries, map, middle, to, a, b);
			};
			for (int32_t y = 0; y < map.GetHeight(); y++)
				for (int32_t x = 0; x < map.GetWidth(); x++)
				{
					if (x + 1 < map.GetWidth() && !map.Joins(x, y, Direction::East))
						halves({ float(x + 1), float(y) }, { float(x + 1), float(y + 1) }, { x, y }, { x + 1, y });
					if (y + 1 < map.GetHeight() && !map.Joins(x, y, Direction::North))
						halves({ float(x), float(y + 1) }, { float(x + 1), float(y + 1) }, { x, y }, { x, y + 1 });
					const uint8_t cuts = map.GetCuts(x, y);
					const Vec2 corner = { float(x), float(y) };
					if (cuts & CornerSouthWest)
						AddBoundary(boundaries, map, corner + Vec2(0.0f, 0.5f), corner + Vec2(0.5f, 0.0f), { x, y }, { x - 1, y });
					if (cuts & CornerSouthEast)
						AddBoundary(boundaries, map, corner + Vec2(0.5f, 0.0f), corner + Vec2(1.0f, 0.5f), { x, y }, { x + 1, y });
					if (cuts & CornerNorthEast)
						AddBoundary(boundaries, map, corner + Vec2(1.0f, 0.5f), corner + Vec2(0.5f, 1.0f), { x, y }, { x + 1, y });
					if (cuts & CornerNorthWest)
						AddBoundary(boundaries, map, corner + Vec2(0.5f, 1.0f), corner + Vec2(0.0f, 0.5f), { x, y }, { x - 1, y });
				}
		}

		void AddBoundaryShape(std::vector<Shape>& shapes, const Boundary& boundary)
		{
			const Vec2 span = boundary.To - boundary.From;
			const Vec2 middle = (boundary.From + boundary.To) * 0.5f;
			const float yaw = AngleOf(span);
			if (boundary.Kind == BoundaryKind::Ledge)
				shapes.push_back(MakeBox(middle, { Length(span) * 0.5f, LedgeHalfThickness }, yaw, Block::Movement));
			else
				shapes.push_back(MakeBox(middle - boundary.Inside * BoundaryDepth, { Length(span) * 0.5f, BoundaryDepth }, yaw, boundary.Kind == BoundaryKind::Void ? Block::Void : Block::Movement));
		}
	}

	void AppendPropShapes(const PlacedProp& prop, std::vector<Shape>& shapes)
	{
		const Vec2 axis = FromAngle(prop.Yaw);
		for (const PropCollider& collider : GetProp(prop.Id).Colliders)
		{
			const Vec2 center = prop.Position + Rotate(collider.Offset, axis);
			if (collider.Kind == ShapeKind::Circle)
				shapes.push_back(MakeCircle(center, collider.Radius, collider.Blocks));
			else
				shapes.push_back(MakeBox(center, collider.Half, prop.Yaw + collider.Yaw, collider.Blocks));
		}
	}

	Shape MakeWallShape(const WallSegment& wall)
	{
		const WallDef& def = GetWall(wall.Style);
		const Vec2 span = wall.To - wall.From;
		return MakeBox((wall.From + wall.To) * 0.5f, { Length(span) * 0.5f + def.Thickness * 0.5f, def.Thickness * 0.5f }, AngleOf(span), def.Blocks);
	}

	void Level::Finalize()
	{
		SettleTerrainLevels(Map);
		if (Rounded)
			Map.RoundCorners();
		for (PlacedProp& prop : Props)
			prop.Base = Map.GetHeight(prop.Position);
		for (WallSegment& wall : Walls)
		{
			const Vec2 middle = (wall.From + wall.To) * 0.5f;
			const Vec2 side = Normalize(Perpendicular(wall.To - wall.From)) * 0.3f;
			const float left = Map.GetHeight(middle + side);
			const float right = Map.GetHeight(middle - side);
			const bool leftWalkable = Map.IsWalkable(int32_t(std::floor(middle.x + side.x)), int32_t(std::floor(middle.y + side.y)));
			const bool rightWalkable = Map.IsWalkable(int32_t(std::floor(middle.x - side.x)), int32_t(std::floor(middle.y - side.y)));
			wall.Base = leftWalkable && rightWalkable ? std::max(left, right) : leftWalkable ? left : right;
			wall.Bottom = leftWalkable && rightWalkable ? std::min(left, right) : wall.Base;
		}

		Shapes.clear();
		ShapeProps.clear();
		Sunk.clear();
		for (uint32_t i = 0; i < Props.size(); i++)
		{
			AppendPropShapes(Props[i], Shapes);
			ShapeProps.resize(Shapes.size(), i);
		}
		for (const WallSegment& wall : Walls)
			Shapes.push_back(MakeWallShape(wall));
		Boundaries.clear();
		if (Rounded)
		{
			TraceBoundaries(Map, Boundaries);
			for (const Boundary& boundary : Boundaries)
				AddBoundaryShape(Shapes, boundary);
		}
		else
			for (int32_t y = 0; y < Map.GetHeight(); y++)
			{
				for (int32_t x = 0; x < Map.GetWidth(); x++)
				{
					if (!Map.IsWalkable(x, y))
						continue;
					if (Map.IsWalkable(x + 1, y) && !Map.IsConnected(x, y, Direction::East))
						AddLedge(Shapes, { float(x + 1), float(y) + 0.5f }, { LedgeHalfThickness, 0.5f });
					if (Map.IsWalkable(x, y + 1) && !Map.IsConnected(x, y, Direction::North))
						AddLedge(Shapes, { float(x) + 0.5f, float(y + 1) }, { 0.5f, LedgeHalfThickness });
				}
			}

		const int32_t width = Map.GetWidth();
		const int32_t height = Map.GetHeight();
		const auto tileRange = [&](const Shape& shape, int32_t& minX, int32_t& minY, int32_t& maxX, int32_t& maxY)
		{
			Vec2 min;
			Vec2 max;
			GetBounds(shape, min, max);
			minX = std::max(int32_t(std::floor(min.x)), 0);
			minY = std::max(int32_t(std::floor(min.y)), 0);
			maxX = std::min(int32_t(std::floor(max.x)), width - 1);
			maxY = std::min(int32_t(std::floor(max.y)), height - 1);
		};
		BucketStart.assign(size_t(width) * size_t(height) + 1, 0);
		for (const Shape& shape : Shapes)
		{
			int32_t minX, minY, maxX, maxY;
			tileRange(shape, minX, minY, maxX, maxY);
			for (int32_t y = minY; y <= maxY; y++)
				for (int32_t x = minX; x <= maxX; x++)
					BucketStart[size_t(y) * width + x + 1]++;
		}
		for (size_t i = 1; i < BucketStart.size(); i++)
			BucketStart[i] += BucketStart[i - 1];
		BucketShapes.assign(BucketStart.back(), 0);
		std::vector<uint32_t> cursor(BucketStart.begin(), BucketStart.end() - 1);
		for (uint32_t index = 0; index < Shapes.size(); index++)
		{
			int32_t minX, minY, maxX, maxY;
			tileRange(Shapes[index], minX, minY, maxX, maxY);
			for (int32_t y = minY; y <= maxY; y++)
				for (int32_t x = minX; x <= maxX; x++)
					BucketShapes[cursor[size_t(y) * width + x]++] = index;
		}
	}

	Level BuildLevel(MapKind kind, uint32_t seed)
	{
		switch (kind)
		{
		case MapKind::Hub:
			return BuildHubLevel(seed);
		case MapKind::Town:
			return BuildTownLevel(seed);
		case MapKind::Crucible:
			return BuildCrucibleLevel(seed);
		case MapKind::Ashlands:
			return BuildAshlandsLevel(seed);
		case MapKind::Abbey:
			return BuildAbbeyLevel(seed);
		case MapKind::Caldera:
			return BuildCalderaLevel(seed);
		case MapKind::FrozenLake:
			return BuildFrozenLakeLevel(seed);
		case MapKind::SkyRuins:
			return BuildSkyRuinsLevel(seed);
		case MapKind::StormPeak:
			return BuildStormPeakLevel(seed);
		default:
			return BuildCavesLevel(seed);
		}
	}
}
