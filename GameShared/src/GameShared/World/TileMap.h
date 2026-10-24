#pragma once
#include "GameShared/Content/Terrain.h"
#include "GameShared/Core/Math.h"
#include <cstdint>
#include <vector>

namespace Game
{
	enum class Direction : uint8_t
	{
		North,
		East,
		South,
		West,
		None,
	};

	inline constexpr float HeightUnit = 0.25f;
	inline constexpr float MaxStepHeight = 0.3f;
	inline constexpr float ProjectileHeight = 0.55f;
	inline constexpr float NoFloor = -1e30f;

	struct TileCoord
	{
		int32_t X = 0;
		int32_t Y = 0;
	};

	enum TileCorner : uint8_t
	{
		CornerSouthWest = 1 << 0,
		CornerSouthEast = 1 << 1,
		CornerNorthEast = 1 << 2,
		CornerNorthWest = 1 << 3,
	};

	struct Cell
	{
		TerrainId Terrain = TerrainId::Rock;
		Direction Slope = Direction::None;
		int8_t Level = 0;
		uint8_t Climb = 0;

		bool operator==(const Cell&) const = default;
	};

	// One tile is one world unit; tile (x, y) covers [x, x + 1) x [y, y + 1).
	class TileMap
	{
	public:
		TileMap() = default;
		TileMap(int32_t width, int32_t height, TerrainId fill = TerrainId::Rock);

		int32_t GetWidth() const { return m_Width; }
		int32_t GetHeight() const { return m_Height; }
		bool IsInside(int32_t x, int32_t y) const { return x >= 0 && y >= 0 && x < m_Width && y < m_Height; }
		const Cell& Get(int32_t x, int32_t y) const { return IsInside(x, y) ? m_Cells[size_t(y) * m_Width + x] : s_Outside; }
		Cell& At(int32_t x, int32_t y) { return m_Cells[size_t(y) * m_Width + x]; }
		void Set(int32_t x, int32_t y, const Cell& cell);
		TerrainId GetTerrain(int32_t x, int32_t y) const { return Get(x, y).Terrain; }
		bool IsWalkable(int32_t x, int32_t y) const { return Game::GetTerrain(GetTerrain(x, y)).Walkable; }
		bool IsAbyss(int32_t x, int32_t y) const { return IsInside(x, y) && !IsWalkable(x, y) && Game::GetTerrain(GetTerrain(x, y)).Style == TerrainStyle::Void; }
		bool BlocksProjectiles(int32_t x, int32_t y) const { return Game::GetTerrain(GetTerrain(x, y)).BlocksProjectiles; }
		float GetHeightAt(int32_t x, int32_t y, float u, float v) const;
		float GetHeight(Vec2 position) const;
		float GetTopHeight(int32_t x, int32_t y) const;
		bool IsConnected(int32_t x, int32_t y, Direction side) const;
		bool IsUpwardLedge(int32_t fromX, int32_t fromY, int32_t toX, int32_t toY, Vec2 crossing) const;
		bool Joins(int32_t x, int32_t y, Direction side) const;
		void RoundCorners();
		bool IsRounded() const { return !m_Cuts.empty(); }
		uint8_t GetCuts(int32_t x, int32_t y) const { return IsInside(x, y) && !m_Cuts.empty() ? m_Cuts[size_t(y) * m_Width + x] : uint8_t(0); }
		bool IsCutVertex(int32_t x, int32_t y) const;
		TileCoord CellAt(Vec2 position) const;
		bool IsWalkableAt(Vec2 position) const;
		bool IsAbyssAt(Vec2 position) const;
		Vec2 GetCenter() const { return { m_Width * 0.5f, m_Height * 0.5f }; }
		bool operator==(const TileMap& other) const = default;

		static void Step(Direction side, int32_t& x, int32_t& y);

	private:
		static const Cell s_Outside;

		int32_t m_Width = 0;
		int32_t m_Height = 0;
		std::vector<Cell> m_Cells;
		std::vector<uint8_t> m_Cuts;
	};
}
