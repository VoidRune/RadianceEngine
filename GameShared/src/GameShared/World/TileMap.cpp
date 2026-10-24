#include "TileMap.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	const Cell TileMap::s_Outside{};

	TileMap::TileMap(int32_t width, int32_t height, TerrainId fill)
		: m_Width(width), m_Height(height), m_Cells(size_t(width) * height, Cell{ .Terrain = fill })
	{
	}

	void TileMap::Set(int32_t x, int32_t y, const Cell& cell)
	{
		if (IsInside(x, y))
			m_Cells[size_t(y) * m_Width + x] = cell;
	}

	float TileMap::GetHeightAt(int32_t x, int32_t y, float u, float v) const
	{
		const Cell& cell = Get(x, y);
		u = std::clamp(u, 0.0f, 1.0f);
		v = std::clamp(v, 0.0f, 1.0f);
		float level = float(cell.Level);
		switch (cell.Slope)
		{
		case Direction::North:
			level += float(cell.Climb) * v;
			break;
		case Direction::East:
			level += float(cell.Climb) * u;
			break;
		case Direction::South:
			level += float(cell.Climb) * (1.0f - v);
			break;
		case Direction::West:
			level += float(cell.Climb) * (1.0f - u);
			break;
		case Direction::None:
			break;
		}
		return level * HeightUnit;
	}

	float TileMap::GetHeight(Vec2 position) const
	{
		const TileCoord cell = CellAt(position);
		return GetHeightAt(cell.X, cell.Y, position.x - float(cell.X), position.y - float(cell.Y));
	}

	TileCoord TileMap::CellAt(Vec2 position) const
	{
		const int32_t x = int32_t(std::floor(position.x));
		const int32_t y = int32_t(std::floor(position.y));
		const uint8_t cuts = GetCuts(x, y);
		if (cuts == 0)
			return { x, y };
		const float u = position.x - float(x);
		const float v = position.y - float(y);
		const bool east = u >= 0.5f;
		const bool north = v >= 0.5f;
		const uint8_t corner = north ? (east ? CornerNorthEast : CornerNorthWest) : (east ? CornerSouthEast : CornerSouthWest);
		const float across = east ? 1.0f - u : u;
		const float along = north ? 1.0f - v : v;
		if ((cuts & corner) == 0 || across + along >= 0.5f)
			return { x, y };
		return across < along ? TileCoord{ x + (east ? 1 : -1), y } : TileCoord{ x, y + (north ? 1 : -1) };
	}

	bool TileMap::IsWalkableAt(Vec2 position) const
	{
		const TileCoord cell = CellAt(position);
		return IsWalkable(cell.X, cell.Y);
	}

	bool TileMap::IsAbyssAt(Vec2 position) const
	{
		const TileCoord cell = CellAt(position);
		return IsAbyss(cell.X, cell.Y);
	}

	bool TileMap::Joins(int32_t x, int32_t y, Direction side) const
	{
		int32_t nx = x;
		int32_t ny = y;
		Step(side, nx, ny);
		const bool walkable = IsWalkable(x, y);
		if (walkable != IsWalkable(nx, ny))
			return false;
		return !walkable || IsConnected(x, y, side);
	}

	bool TileMap::IsCutVertex(int32_t x, int32_t y) const
	{
		return (GetCuts(x - 1, y - 1) & CornerNorthEast) || (GetCuts(x, y - 1) & CornerNorthWest) || (GetCuts(x, y) & CornerSouthWest) || (GetCuts(x - 1, y) & CornerSouthEast);
	}

	void TileMap::RoundCorners()
	{
		std::vector<uint8_t> cuts(m_Cells.size(), 0);
		for (int32_t y = 1; y < m_Height; y++)
			for (int32_t x = 1; x < m_Width; x++)
			{
				if (Get(x - 1, y - 1).Slope != Direction::None || Get(x, y - 1).Slope != Direction::None || Get(x, y).Slope != Direction::None || Get(x - 1, y).Slope != Direction::None)
					continue;
				const bool bottom = Joins(x - 1, y - 1, Direction::East);
				const bool right = Joins(x, y - 1, Direction::North);
				const bool top = Joins(x - 1, y, Direction::East);
				const bool left = Joins(x - 1, y - 1, Direction::North);
				if (!bottom && !left && right && top)
					cuts[size_t(y - 1) * m_Width + size_t(x - 1)] |= CornerNorthEast;
				else if (!bottom && !right && left && top)
					cuts[size_t(y - 1) * m_Width + size_t(x)] |= CornerNorthWest;
				else if (!right && !top && bottom && left)
					cuts[size_t(y) * m_Width + size_t(x)] |= CornerSouthWest;
				else if (!top && !left && bottom && right)
					cuts[size_t(y) * m_Width + size_t(x - 1)] |= CornerSouthEast;
			}
		m_Cuts = std::move(cuts);
	}

	float TileMap::GetTopHeight(int32_t x, int32_t y) const
	{
		const Cell& cell = Get(x, y);
		return float(int32_t(cell.Level) + (cell.Slope != Direction::None ? int32_t(cell.Climb) : 0)) * HeightUnit;
	}

	void TileMap::Step(Direction side, int32_t& x, int32_t& y)
	{
		switch (side)
		{
		case Direction::North:
			y++;
			break;
		case Direction::East:
			x++;
			break;
		case Direction::South:
			y--;
			break;
		case Direction::West:
			x--;
			break;
		case Direction::None:
			break;
		}
	}

	bool TileMap::IsConnected(int32_t x, int32_t y, Direction side) const
	{
		int32_t nx = x;
		int32_t ny = y;
		Step(side, nx, ny);
		if (side == Direction::None || !IsWalkable(x, y) || !IsWalkable(nx, ny))
			return false;
		float a0 = 0.0f;
		float a1 = 0.0f;
		float b0 = 0.0f;
		float b1 = 0.0f;
		switch (side)
		{
		case Direction::North:
			a0 = GetHeightAt(x, y, 0.0f, 1.0f);
			a1 = GetHeightAt(x, y, 1.0f, 1.0f);
			b0 = GetHeightAt(nx, ny, 0.0f, 0.0f);
			b1 = GetHeightAt(nx, ny, 1.0f, 0.0f);
			break;
		case Direction::East:
			a0 = GetHeightAt(x, y, 1.0f, 0.0f);
			a1 = GetHeightAt(x, y, 1.0f, 1.0f);
			b0 = GetHeightAt(nx, ny, 0.0f, 0.0f);
			b1 = GetHeightAt(nx, ny, 0.0f, 1.0f);
			break;
		case Direction::South:
			a0 = GetHeightAt(x, y, 0.0f, 0.0f);
			a1 = GetHeightAt(x, y, 1.0f, 0.0f);
			b0 = GetHeightAt(nx, ny, 0.0f, 1.0f);
			b1 = GetHeightAt(nx, ny, 1.0f, 1.0f);
			break;
		case Direction::West:
			a0 = GetHeightAt(x, y, 0.0f, 0.0f);
			a1 = GetHeightAt(x, y, 0.0f, 1.0f);
			b0 = GetHeightAt(nx, ny, 1.0f, 0.0f);
			b1 = GetHeightAt(nx, ny, 1.0f, 1.0f);
			break;
		case Direction::None:
			break;
		}
		return std::abs(a0 - b0) <= MaxStepHeight && std::abs(a1 - b1) <= MaxStepHeight;
	}

	bool TileMap::IsUpwardLedge(int32_t fromX, int32_t fromY, int32_t toX, int32_t toY, Vec2 crossing) const
	{
		if (!IsWalkable(fromX, fromY) || !IsWalkable(toX, toY))
			return false;
		const float from = GetHeightAt(fromX, fromY, crossing.x - float(fromX), crossing.y - float(fromY));
		const float to = GetHeightAt(toX, toY, crossing.x - float(toX), crossing.y - float(toY));
		return to - from > ProjectileHeight;
	}
}
