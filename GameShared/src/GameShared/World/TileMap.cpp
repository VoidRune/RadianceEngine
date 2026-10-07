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
		const int32_t x = int32_t(std::floor(position.x));
		const int32_t y = int32_t(std::floor(position.y));
		return GetHeightAt(x, y, position.x - float(x), position.y - float(y));
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
