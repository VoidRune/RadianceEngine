#pragma once
#include "GameShared/World/Level.h"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace Game
{
	class NavGrid
	{
	public:
		static constexpr int32_t CellsPerTile = 2;
		static constexpr float CellSize = 1.0f / float(CellsPerTile);

		struct Link
		{
			int32_t Entry = 0;
			int32_t Exit = 0;
			int32_t Portal = 0;
		};

		void Build(const Level& level);
		int32_t GetWidth() const { return m_Width; }
		int32_t GetHeight() const { return m_Height; }
		int32_t CellAt(Vec2 position) const;
		Vec2 CellCenter(int32_t cell) const { return { (float(cell % m_Width) + 0.5f) * CellSize, (float(cell / m_Width) + 0.5f) * CellSize }; }
		float GetClearance(int32_t cell) const { return m_Clearance[size_t(cell)]; }
		bool IsOpen(int32_t cell, int32_t step) const { return (m_Edges[size_t(cell)] & (1u << step)) != 0; }
		bool IsExit(int32_t cell) const { return m_Exits[size_t(cell)] != 0; }
		std::span<const Link> GetLinks() const { return m_Links; }

	private:
		int32_t m_Width = 0;
		int32_t m_Height = 0;
		std::vector<float> m_Clearance;
		std::vector<uint8_t> m_Edges;
		std::vector<uint8_t> m_Exits;
		std::vector<Link> m_Links;
	};

	class FlowField
	{
	public:
		static constexpr uint32_t Unreachable = UINT32_MAX;
		static constexpr uint32_t StraightCost = 10;
		static constexpr uint32_t DiagonalCost = 14;
		static constexpr uint32_t PortalCost = 10;

		struct Step
		{
			Vec2 Direction;
			int32_t Portal = -1;
		};

		void Build(const Level& level, const NavGrid& grid, float radius, std::span<const Vec2> targets);
		Step GetStep(Vec2 position, float bodyRadius) const;
		uint32_t GetDistance(Vec2 position) const;
		float GetTravelDistance(Vec2 position) const;
		bool IsReachable(Vec2 position) const { return m_Distance.empty() || GetDistance(position) != Unreachable; }
		float GetRadius() const { return m_Radius; }

	private:
		bool IsPassable(int32_t cell) const;
		bool CanStep(int32_t cell, int32_t step) const;
		int32_t Neighbor(int32_t cell, int32_t step) const;
		bool Relax(int32_t cell, uint32_t cost, int32_t via);
		bool SeedAround(const Level& level, Vec2 target, int32_t center);
		void SeedNearest(int32_t center);
		void Sweep();
		Step Escape(int32_t cell, Vec2 position, float bodyRadius) const;

		const NavGrid* m_Grid = nullptr;
		float m_Radius = 0.0f;
		std::vector<uint32_t> m_Distance;
		std::vector<int16_t> m_Via;
		std::array<std::vector<int32_t>, 64> m_Buckets;
		size_t m_Pending = 0;
	};
}
