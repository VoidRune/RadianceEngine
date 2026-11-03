#pragma once
#include "GameShared/Core/Math.h"
#include <cstdint>
#include <vector>

namespace Game
{
	class TargetGrid
	{
	public:
		static constexpr float CellSize = 2.0f;

		void Begin(int32_t width, int32_t height);
		void Add(Vec2 low, Vec2 high);
		void Finish();
		void Gather(Vec2 low, Vec2 high, std::vector<uint32_t>& found) const;
		uint32_t GetCount() const { return uint32_t(m_Low.size()); }

	private:
		struct CellRange
		{
			int32_t MinX = 0;
			int32_t MinY = 0;
			int32_t MaxX = 0;
			int32_t MaxY = 0;
		};

		CellRange Cover(Vec2 low, Vec2 high) const;

		int32_t m_Columns = 1;
		int32_t m_Rows = 1;
		std::vector<Vec2> m_Low;
		std::vector<Vec2> m_High;
		std::vector<CellRange> m_Ranges;
		std::vector<uint32_t> m_CellStart;
		std::vector<uint32_t> m_CellEntries;
		std::vector<uint32_t> m_Cursor;
		mutable std::vector<uint32_t> m_Marks;
		mutable uint32_t m_Stamp = 0;
	};
}
