#include "TargetGrid.h"
#include <algorithm>

namespace Game
{
	void TargetGrid::Begin(int32_t width, int32_t height)
	{
		m_Columns = std::max(1, int32_t(std::ceil(float(width) / CellSize)));
		m_Rows = std::max(1, int32_t(std::ceil(float(height) / CellSize)));
		m_Low.clear();
		m_High.clear();
		m_Ranges.clear();
	}

	TargetGrid::CellRange TargetGrid::Cover(Vec2 low, Vec2 high) const
	{
		const auto cell = [](float value, int32_t count) { return std::clamp(int32_t(std::floor(value / CellSize)), 0, count - 1); };
		return { cell(low.x, m_Columns), cell(low.y, m_Rows), cell(high.x, m_Columns), cell(high.y, m_Rows) };
	}

	void TargetGrid::Add(Vec2 low, Vec2 high)
	{
		m_Low.push_back(low);
		m_High.push_back(high);
		m_Ranges.push_back(Cover(low, high));
	}

	void TargetGrid::Finish()
	{
		const size_t cells = size_t(m_Columns) * size_t(m_Rows);
		m_CellStart.assign(cells + 1, 0);
		for (const CellRange& range : m_Ranges)
			for (int32_t y = range.MinY; y <= range.MaxY; y++)
				for (int32_t x = range.MinX; x <= range.MaxX; x++)
					m_CellStart[size_t(y) * size_t(m_Columns) + size_t(x) + 1]++;
		for (size_t i = 1; i <= cells; i++)
			m_CellStart[i] += m_CellStart[i - 1];
		m_CellEntries.resize(m_CellStart[cells]);
		m_Cursor.assign(m_CellStart.begin(), m_CellStart.end() - 1);
		for (uint32_t entry = 0; entry < uint32_t(m_Ranges.size()); entry++)
		{
			const CellRange& range = m_Ranges[entry];
			for (int32_t y = range.MinY; y <= range.MaxY; y++)
				for (int32_t x = range.MinX; x <= range.MaxX; x++)
					m_CellEntries[m_Cursor[size_t(y) * size_t(m_Columns) + size_t(x)]++] = entry;
		}
		m_Marks.assign(m_Low.size(), 0);
		m_Stamp = 0;
	}

	void TargetGrid::Gather(Vec2 low, Vec2 high, std::vector<uint32_t>& found) const
	{
		found.clear();
		if (m_Low.empty())
			return;
		if (++m_Stamp == 0)
		{
			std::fill(m_Marks.begin(), m_Marks.end(), 0u);
			m_Stamp = 1;
		}
		const CellRange range = Cover(low, high);
		for (int32_t y = range.MinY; y <= range.MaxY; y++)
			for (int32_t x = range.MinX; x <= range.MaxX; x++)
			{
				const size_t cell = size_t(y) * size_t(m_Columns) + size_t(x);
				for (uint32_t i = m_CellStart[cell]; i < m_CellStart[cell + 1]; i++)
				{
					const uint32_t entry = m_CellEntries[i];
					if (m_Marks[entry] == m_Stamp)
						continue;
					m_Marks[entry] = m_Stamp;
					if (m_Low[entry].x <= high.x && m_High[entry].x >= low.x && m_Low[entry].y <= high.y && m_High[entry].y >= low.y)
						found.push_back(entry);
				}
			}
		std::sort(found.begin(), found.end());
	}
}
