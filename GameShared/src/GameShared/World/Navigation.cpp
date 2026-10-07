#include "Navigation.h"
#include "GameShared/World/Collision.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Game
{
	namespace
	{
		constexpr int32_t StepX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		constexpr int32_t StepY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
		constexpr int32_t Opposite[8] = { 1, 0, 3, 2, 7, 6, 5, 4 };
		constexpr float ClearanceLimit = 0.85f;
		constexpr float PassTolerance = 0.02f;
		constexpr float TargetReach = 1.6f;
		constexpr int32_t EscapeReach = 6;
		constexpr int32_t EscapeSpan = EscapeReach * 2 + 1;
		constexpr int32_t NearestReach = 16;
		constexpr float FitClearance = 0.3f;
	}

	void NavGrid::Build(const Level& level)
	{
		const TileMap& map = level.Map;
		m_Width = map.GetWidth() * CellsPerTile;
		m_Height = map.GetHeight() * CellsPerTile;
		const int32_t latticeWidth = m_Width * 2 + 1;
		const int32_t latticeHeight = m_Height * 2 + 1;
		std::vector<float> lattice(size_t(latticeWidth) * size_t(latticeHeight), 0.0f);
		for (int32_t y = 0; y < latticeHeight; y++)
			for (int32_t x = 0; x < latticeWidth; x++)
				lattice[size_t(y) * latticeWidth + x] = Game::GetClearance(level, { float(x) * CellSize * 0.5f, float(y) * CellSize * 0.5f }, ClearanceLimit);

		m_Clearance.assign(size_t(m_Width) * size_t(m_Height), 0.0f);
		for (int32_t y = 0; y < m_Height; y++)
			for (int32_t x = 0; x < m_Width; x++)
			{
				float best = 0.0f;
				for (int32_t j = 0; j <= 2; j++)
					for (int32_t i = 0; i <= 2; i++)
						best = std::max(best, lattice[size_t(2 * y + j) * latticeWidth + size_t(2 * x + i)]);
				m_Clearance[size_t(y) * m_Width + x] = best;
			}

		m_Edges.assign(m_Clearance.size(), 0);
		for (int32_t y = 0; y < m_Height; y++)
			for (int32_t x = 0; x < m_Width; x++)
			{
				const int32_t cell = y * m_Width + x;
				if (m_Clearance[size_t(cell)] <= 0.0f)
					continue;
				for (const int32_t step : { 0, 2, 4, 6 })
				{
					const int32_t nx = x + StepX[step];
					const int32_t ny = y + StepY[step];
					if (nx < 0 || ny < 0 || nx >= m_Width || ny >= m_Height)
						continue;
					const int32_t neighbor = ny * m_Width + nx;
					if (m_Clearance[size_t(neighbor)] <= 0.0f || CastSegment(level, CellCenter(cell), CellCenter(neighbor), Block::Movement) < 1.0f)
						continue;
					m_Edges[size_t(cell)] |= uint8_t(1u << step);
					m_Edges[size_t(neighbor)] |= uint8_t(1u << Opposite[step]);
				}
			}

		m_Exits.assign(m_Clearance.size(), 0);
		m_Links.clear();
		for (size_t i = 0; i < level.Portals.size(); i++)
		{
			const int32_t entry = CellAt(level.Portals[i].Position);
			const int32_t exit = CellAt(level.Portals[i].Destination);
			if (entry < 0 || exit < 0)
				continue;
			m_Links.push_back({ entry, exit, int32_t(i) });
			m_Exits[size_t(exit)] = 1;
		}
	}

	int32_t NavGrid::CellAt(Vec2 position) const
	{
		const int32_t x = int32_t(std::floor(position.x * float(CellsPerTile)));
		const int32_t y = int32_t(std::floor(position.y * float(CellsPerTile)));
		return x >= 0 && y >= 0 && x < m_Width && y < m_Height ? y * m_Width + x : -1;
	}

	bool FlowField::IsPassable(int32_t cell) const
	{
		return m_Grid->GetClearance(cell) >= m_Radius - PassTolerance;
	}

	int32_t FlowField::Neighbor(int32_t cell, int32_t step) const
	{
		return cell + StepY[step] * m_Grid->GetWidth() + StepX[step];
	}

	bool FlowField::CanStep(int32_t cell, int32_t step) const
	{
		if (!m_Grid->IsOpen(cell, step) || !IsPassable(Neighbor(cell, step)))
			return false;
		if (step < 4)
			return true;
		return IsPassable(cell + StepX[step]) && IsPassable(cell + StepY[step] * m_Grid->GetWidth());
	}

	void FlowField::Build(const Level& level, const NavGrid& grid, float radius, std::span<const Vec2> targets)
	{
		m_Grid = &grid;
		m_Radius = radius;
		m_Distance.assign(size_t(grid.GetWidth()) * size_t(grid.GetHeight()), Unreachable);
		m_Via.assign(m_Distance.size(), -1);
		for (std::vector<int32_t>& bucket : m_Buckets)
			bucket.clear();
		m_Pending = 0;

		for (const Vec2 target : targets)
		{
			const int32_t center = grid.CellAt(target);
			if (center >= 0 && !SeedAround(level, target, center))
				SeedNearest(center);
		}
		Sweep();
	}

	bool FlowField::Relax(int32_t cell, uint32_t cost, int32_t via)
	{
		if (cost >= m_Distance[size_t(cell)])
			return false;
		m_Distance[size_t(cell)] = cost;
		m_Via[size_t(cell)] = int16_t(via);
		m_Buckets[cost % m_Buckets.size()].push_back(cell);
		m_Pending++;
		return true;
	}

	bool FlowField::SeedAround(const Level& level, Vec2 target, int32_t center)
	{
		const int32_t width = m_Grid->GetWidth();
		const int32_t height = m_Grid->GetHeight();
		const int32_t reach = int32_t(std::ceil(TargetReach * float(NavGrid::CellsPerTile)));
		const int32_t cx = center % width;
		const int32_t cy = center / width;
		bool seeded = false;
		for (int32_t y = std::max(cy - reach, 0); y <= std::min(cy + reach, height - 1); y++)
			for (int32_t x = std::max(cx - reach, 0); x <= std::min(cx + reach, width - 1); x++)
			{
				const int32_t cell = y * width + x;
				const Vec2 point = m_Grid->CellCenter(cell);
				const float distance = Length(point - target);
				if (distance > TargetReach || !IsPassable(cell) || CastSegment(level, target, point, Block::Movement) < 1.0f)
					continue;
				Relax(cell, uint32_t(std::lround(distance * float(StraightCost) / NavGrid::CellSize)), -1);
				seeded = true;
			}
		return seeded;
	}

	void FlowField::SeedNearest(int32_t center)
	{
		const int32_t width = m_Grid->GetWidth();
		const int32_t height = m_Grid->GetHeight();
		const auto fits = [&](int32_t x, int32_t y) { return x >= 0 && y >= 0 && x < width && y < height && m_Grid->GetClearance(y * width + x) >= FitClearance; };
		std::vector<uint8_t> seen(m_Distance.size(), 0);
		std::vector<int32_t> frontier = { center };
		std::vector<int32_t> next;
		seen[size_t(center)] = 1;
		for (int32_t depth = 0; depth <= NearestReach && !frontier.empty(); depth++)
		{
			bool found = false;
			for (const int32_t cell : frontier)
				if (IsPassable(cell))
					found |= Relax(cell, 0, -1) || m_Distance[size_t(cell)] == 0;
			if (found)
				return;
			next.clear();
			for (const int32_t cell : frontier)
			{
				const int32_t x = cell % width;
				const int32_t y = cell / width;
				for (int32_t step = 0; step < 8; step++)
				{
					const int32_t nx = x + StepX[step];
					const int32_t ny = y + StepY[step];
					if (!fits(nx, ny) || seen[size_t(ny * width + nx)] || !m_Grid->IsOpen(cell, step))
						continue;
					if (step >= 4 && (!fits(nx, y) || !fits(x, ny)))
						continue;
					seen[size_t(ny * width + nx)] = 1;
					next.push_back(ny * width + nx);
				}
			}
			std::swap(frontier, next);
		}
	}

	void FlowField::Sweep()
	{
		for (uint32_t distance = 0; m_Pending > 0; distance++)
		{
			std::vector<int32_t>& bucket = m_Buckets[distance % m_Buckets.size()];
			while (!bucket.empty())
			{
				const int32_t cell = bucket.back();
				bucket.pop_back();
				m_Pending--;
				if (m_Distance[size_t(cell)] != distance)
					continue;
				if (m_Grid->IsExit(cell))
					for (const NavGrid::Link& link : m_Grid->GetLinks())
						if (link.Exit == cell && IsPassable(link.Entry))
							Relax(link.Entry, distance + PortalCost, link.Portal);
				for (int32_t step = 0; step < 8; step++)
					if (CanStep(cell, step))
						Relax(Neighbor(cell, step), distance + (step < 4 ? StraightCost : DiagonalCost), -1);
			}
		}
	}

	FlowField::Step FlowField::GetStep(Vec2 position, float bodyRadius) const
	{
		if (!m_Grid || m_Distance.empty())
			return {};
		const int32_t cell = m_Grid->CellAt(position);
		if (cell < 0)
			return {};
		uint32_t best = m_Distance[size_t(cell)];
		if (best == 0)
			return {};
		if (best == Unreachable)
			return Escape(cell, position, bodyRadius);
		if (m_Via[size_t(cell)] >= 0)
			return { {}, m_Via[size_t(cell)] };

		int32_t chosen = -1;
		for (int32_t step = 0; step < 8; step++)
		{
			if (!CanStep(cell, step))
				continue;
			const int32_t next = Neighbor(cell, step);
			if (m_Distance[size_t(next)] < best)
			{
				best = m_Distance[size_t(next)];
				chosen = next;
			}
		}
		if (chosen < 0)
			return {};
		return { Normalize(m_Grid->CellCenter(chosen) - position), -1 };
	}

	FlowField::Step FlowField::Escape(int32_t start, Vec2 position, float bodyRadius) const
	{
		const int32_t width = m_Grid->GetWidth();
		const int32_t originX = start % width - EscapeReach;
		const int32_t originY = start / width - EscapeReach;
		const auto fits = [&](int32_t x, int32_t y)
		{
			return x >= 0 && y >= 0 && x < width && y < m_Grid->GetHeight() && m_Grid->GetClearance(y * width + x) >= bodyRadius - PassTolerance;
		};
		const auto toCell = [&](int32_t local) { return (originY + local / EscapeSpan) * width + originX + local % EscapeSpan; };

		std::array<int16_t, EscapeSpan * EscapeSpan> from;
		std::array<int16_t, EscapeSpan * EscapeSpan> queue;
		from.fill(-1);
		const int16_t root = int16_t(EscapeReach * EscapeSpan + EscapeReach);
		from[size_t(root)] = root;
		queue[0] = root;
		for (size_t head = 0, tail = 1; head < tail; head++)
		{
			const int16_t local = queue[head];
			const int32_t cell = toCell(local);
			if (m_Distance[size_t(cell)] != Unreachable)
			{
				int16_t first = local;
				while (from[size_t(first)] != root)
					first = from[size_t(first)];
				return { Normalize(m_Grid->CellCenter(toCell(first)) - position), -1 };
			}
			const int32_t lx = local % EscapeSpan;
			const int32_t ly = local / EscapeSpan;
			for (int32_t step = 0; step < 8; step++)
			{
				const int32_t nx = lx + StepX[step];
				const int32_t ny = ly + StepY[step];
				if (nx < 0 || ny < 0 || nx >= EscapeSpan || ny >= EscapeSpan)
					continue;
				const int16_t next = int16_t(ny * EscapeSpan + nx);
				const int32_t x = originX + nx;
				const int32_t y = originY + ny;
				if (from[size_t(next)] >= 0 || !m_Grid->IsOpen(cell, step) || !fits(x, y))
					continue;
				if (step >= 4 && (!fits(x, originY + ly) || !fits(originX + lx, y)))
					continue;
				from[size_t(next)] = local;
				queue[tail++] = next;
			}
		}
		return {};
	}

	uint32_t FlowField::GetDistance(Vec2 position) const
	{
		if (!m_Grid || m_Distance.empty())
			return Unreachable;
		const int32_t cell = m_Grid->CellAt(position);
		return cell >= 0 ? m_Distance[size_t(cell)] : Unreachable;
	}

	float FlowField::GetTravelDistance(Vec2 position) const
	{
		const uint32_t distance = GetDistance(position);
		return distance == Unreachable ? std::numeric_limits<float>::infinity() : float(distance) * NavGrid::CellSize / float(StraightCost);
	}
}
