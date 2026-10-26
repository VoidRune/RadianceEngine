#include "LevelBuilder.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace Game
{
	namespace
	{
		constexpr float DecorationRadius = 0.4f;

		float SeparationLowerBound(const Shape& a, const Shape& b)
		{
			return std::max(DistanceTo(a, b.Center) - b.Radius, DistanceTo(b, a.Center) - a.Radius);
		}
	}

	int8_t ToLevel(float height)
	{
		return int8_t(std::clamp(int32_t(std::lround(height / HeightUnit)), -127, 127));
	}

	float Lobe::Reach(float angle) const
	{
		return Radius * (1.0f + Wobble * (0.6f * std::sin(3.0f * angle + Phase[0]) + 0.3f * std::sin(5.0f * angle + Phase[1]) + 0.2f * std::sin(7.0f * angle + Phase[2])));
	}

	bool Lobe::Contains(Vec2 point) const
	{
		const Vec2 offset = point - Middle;
		return Length(offset) < Reach(std::atan2(offset.y, offset.x));
	}

	Lobe MakeLobe(Random& random, Vec2 middle, float radius, float wobble)
	{
		Lobe blob{ middle, radius };
		for (float& phase : blob.Phase)
			phase = random.Float() * 2.0f * Pi;
		blob.Wobble = wobble;
		return blob;
	}

	LevelBuilder::LevelBuilder(MapKind kind, uint32_t seed, int32_t width, int32_t height, TerrainId fill)
		: m_Random(uint64_t(seed) * 0x2545F4914F6CDD1DULL + uint64_t(kind) + 7), m_Reserved(size_t(width) * size_t(height), 0)
	{
		m_Level.Kind = kind;
		m_Level.Seed = seed;
		m_Level.Map = TileMap(width, height, fill);
	}

	void LevelBuilder::Fill(const TileRect& area, TerrainId terrain)
	{
		TileMap& map = m_Level.Map;
		for (int32_t y = std::max(area.Y, 0); y < std::min(area.Top(), map.GetHeight()); y++)
			for (int32_t x = std::max(area.X, 0); x < std::min(area.Right(), map.GetWidth()); x++)
				map.At(x, y).Terrain = terrain;
	}

	void LevelBuilder::Fill(const TileRect& area, TerrainId terrain, float height)
	{
		Fill(area, terrain);
		Raise(area, height);
	}

	void LevelBuilder::Raise(const TileRect& area, float height)
	{
		TileMap& map = m_Level.Map;
		const int8_t level = ToLevel(height);
		for (int32_t y = std::max(area.Y, 0); y < std::min(area.Top(), map.GetHeight()); y++)
			for (int32_t x = std::max(area.X, 0); x < std::min(area.Right(), map.GetWidth()); x++)
			{
				Cell& cell = map.At(x, y);
				cell.Level = level;
				cell.Slope = Direction::None;
				cell.Climb = 0;
			}
	}

	void LevelBuilder::Stairs(const TileRect& area, Direction up, float bottom, float top, TerrainId terrain)
	{
		TileMap& map = m_Level.Map;
		const int32_t low = ToLevel(bottom);
		const int32_t total = ToLevel(top) - low;
		const bool vertical = up == Direction::North || up == Direction::South;
		const int32_t depth = std::max(vertical ? area.Height : area.Width, 1);
		for (int32_t y = std::max(area.Y, 0); y < std::min(area.Top(), map.GetHeight()); y++)
			for (int32_t x = std::max(area.X, 0); x < std::min(area.Right(), map.GetWidth()); x++)
			{
				int32_t row = 0;
				switch (up)
				{
				case Direction::North:
					row = y - area.Y;
					break;
				case Direction::South:
					row = area.Top() - 1 - y;
					break;
				case Direction::East:
					row = x - area.X;
					break;
				default:
					row = area.Right() - 1 - x;
					break;
				}
				const int32_t start = low + total * row / depth;
				const int32_t end = low + total * (row + 1) / depth;
				Cell& cell = map.At(x, y);
				cell.Terrain = terrain;
				cell.Level = int8_t(std::clamp(start, -127, 127));
				cell.Slope = end != start ? up : Direction::None;
				cell.Climb = uint8_t(std::clamp(end - start, 0, 255));
			}
	}

	void LevelBuilder::Disc(Vec2 center, float radius, TerrainId terrain)
	{
		TileMap& map = m_Level.Map;
		for (int32_t y = int32_t(std::floor(center.y - radius)); y <= int32_t(std::floor(center.y + radius)); y++)
			for (int32_t x = int32_t(std::floor(center.x - radius)); x <= int32_t(std::floor(center.x + radius)); x++)
				if (map.IsInside(x, y) && LengthSquared(Vec2(float(x) + 0.5f, float(y) + 0.5f) - center) <= radius * radius)
					map.At(x, y).Terrain = terrain;
	}

	void LevelBuilder::Stroke(Vec2 from, Vec2 to, float width, TerrainId terrain)
	{
		TileMap& map = m_Level.Map;
		const float half = width * 0.5f;
		for (int32_t y = int32_t(std::floor(std::min(from.y, to.y) - half)); y <= int32_t(std::floor(std::max(from.y, to.y) + half)); y++)
			for (int32_t x = int32_t(std::floor(std::min(from.x, to.x) - half)); x <= int32_t(std::floor(std::max(from.x, to.x) + half)); x++)
				if (map.IsInside(x, y) && PointSegmentDistance({ float(x) + 0.5f, float(y) + 0.5f }, from, to) <= half)
				{
					map.At(x, y).Terrain = terrain;
					m_Reserved[size_t(y) * size_t(map.GetWidth()) + size_t(x)] = 1;
				}
	}

	void LevelBuilder::Reserve(const TileRect& area)
	{
		const TileMap& map = m_Level.Map;
		for (int32_t y = std::max(area.Y, 0); y < std::min(area.Top(), map.GetHeight()); y++)
			for (int32_t x = std::max(area.X, 0); x < std::min(area.Right(), map.GetWidth()); x++)
				m_Reserved[size_t(y) * size_t(map.GetWidth()) + size_t(x)] = 1;
	}

	void LevelBuilder::Reserve(Vec2 center, float radius)
	{
		const TileMap& map = m_Level.Map;
		for (int32_t y = int32_t(std::floor(center.y - radius)); y <= int32_t(std::floor(center.y + radius)); y++)
			for (int32_t x = int32_t(std::floor(center.x - radius)); x <= int32_t(std::floor(center.x + radius)); x++)
				if (map.IsInside(x, y))
					m_Reserved[size_t(y) * size_t(map.GetWidth()) + size_t(x)] = 1;
	}

	std::vector<Shape> LevelBuilder::Footprint(PropId id, Vec2 position, float yaw) const
	{
		std::vector<Shape> shapes;
		AppendPropShapes({ .Id = id, .Position = position, .Yaw = yaw }, shapes);
		if (shapes.empty())
			shapes.push_back(MakeCircle(position, DecorationRadius, Block::None));
		return shapes;
	}

	bool LevelBuilder::IsFootprintFree(const std::vector<Shape>& footprint, float spacing) const
	{
		const TileMap& map = m_Level.Map;
		const float height = map.GetHeight(footprint.front().Center);
		for (const Shape& shape : footprint)
		{
			Vec2 min;
			Vec2 max;
			GetBounds(shape, min, max);
			for (const Vec2 sample : { shape.Center, min, max, Vec2(min.x, max.y), Vec2(max.x, min.y) })
			{
				const TileCoord cell = map.CellAt(sample);
				if (!map.IsWalkable(cell.X, cell.Y) || GetTerrain(map.GetTerrain(cell.X, cell.Y)).Style == TerrainStyle::Void || m_Reserved[size_t(cell.Y) * size_t(map.GetWidth()) + size_t(cell.X)] || std::abs(map.GetHeight(sample) - height) > 0.01f)
					return false;
			}
			for (const Shape& other : m_Occupied)
				if (SeparationLowerBound(shape, other) < spacing)
					return false;
		}
		return true;
	}

	void LevelBuilder::Prop(PropId id, Vec2 position, float yaw)
	{
		m_Level.Props.push_back({ .Id = id, .Position = position, .Yaw = yaw });
		const std::vector<Shape> footprint = Footprint(id, position, yaw);
		m_Occupied.insert(m_Occupied.end(), footprint.begin(), footprint.end());
	}

	bool LevelBuilder::TryProp(PropId id, Vec2 position, float yaw, float spacing)
	{
		const std::vector<Shape> footprint = Footprint(id, position, yaw);
		if (!IsFootprintFree(footprint, spacing))
			return false;
		m_Level.Props.push_back({ .Id = id, .Position = position, .Yaw = yaw });
		m_Occupied.insert(m_Occupied.end(), footprint.begin(), footprint.end());
		return true;
	}

	void LevelBuilder::Wall(WallId style, Vec2 from, Vec2 to)
	{
		m_Level.Walls.push_back({ .Style = style, .From = from, .To = to });
		m_Occupied.push_back(MakeWallShape(m_Level.Walls.back()));
	}

	int32_t LevelBuilder::Portal(Vec2 position, Vec2 destination, float destinationFacing, std::string label, bool hidden)
	{
		m_Level.Portals.push_back({ .Position = position, .Destination = destination, .DestinationFacing = destinationFacing, .Label = std::move(label), .Hidden = hidden });
		Reserve(position, 1.6f);
		Reserve(destination, 1.6f);
		return int32_t(m_Level.Portals.size() - 1);
	}

	void LevelBuilder::Region(std::string name, const TileRect& area, bool enemySpawns)
	{
		m_Level.Regions.push_back({ .Name = std::move(name), .Min = { float(area.X), float(area.Y) }, .Max = { float(area.Right()), float(area.Top()) }, .EnemySpawns = enemySpawns });
	}

	void LevelBuilder::SetPlayerSpawn(Vec2 position)
	{
		m_Level.PlayerSpawn = position;
		m_Level.MissionStart = position;
		Reserve(position, 4.0f);
	}

	void LevelBuilder::SetMissionStart(Vec2 position)
	{
		m_Level.MissionStart = position;
		Reserve(position, 3.0f);
	}

	void LevelBuilder::SetBossArena(Vec2 center, float radius)
	{
		m_Level.BossArena = center;
		m_Level.BossArenaRadius = radius;
	}

	void LevelBuilder::Station(StationKind kind, Vec2 position, float radius)
	{
		m_Level.Stations.push_back({ kind, position, radius });
	}

	void LevelBuilder::Dummy(Vec2 position, Vec2 patrol, bool duck)
	{
		m_Level.Dummies.push_back({ position, patrol, duck });
	}

	void LevelBuilder::SpawnPoint(Vec2 position)
	{
		m_Level.SpawnPoints.push_back(position);
		Reserve(position, 1.5f);
	}

	void LevelBuilder::LootSpot(Vec2 position, bool vault)
	{
		m_Level.LootSpots.push_back({ position, vault });
		Reserve(position, vault ? 2.0f : 1.2f);
	}

	void LevelBuilder::Pad(Vec2 position, PadKind kind)
	{
		m_Level.Pads.push_back({ position, kind });
		Reserve(position, 1.2f);
	}

	void LevelBuilder::Roof(const TileRect& area, float height)
	{
		m_Level.Roofs.push_back({ .Min = { float(area.X), float(area.Y) }, .Max = { float(area.Right()), float(area.Top()) }, .Height = height });
	}

	void LevelBuilder::SetRounded()
	{
		m_Level.Rounded = true;
		m_Level.Map.RoundCorners();
	}

	Level LevelBuilder::Finish()
	{
		m_Level.Finalize();
		return std::move(m_Level);
	}
}
