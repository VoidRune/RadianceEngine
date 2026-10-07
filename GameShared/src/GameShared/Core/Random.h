#pragma once
#include <cstdint>

namespace Game
{
	// PCG32. Integer only, so the same seed generates the same sequence on every machine.
	class Random
	{
	public:
		explicit Random(uint64_t seed = 0x853c49e6748fea9bULL) { Seed(seed); }

		void Seed(uint64_t seed)
		{
			m_State = 0;
			Next();
			m_State += seed;
			Next();
		}

		uint32_t Next()
		{
			const uint64_t old = m_State;
			m_State = old * 6364136223846793005ULL + 1442695040888963407ULL;
			const uint32_t shifted = uint32_t(((old >> 18u) ^ old) >> 27u);
			const uint32_t rotation = uint32_t(old >> 59u);
			return (shifted >> rotation) | (shifted << ((32u - rotation) & 31u));
		}

		uint32_t Below(uint32_t count) { return uint32_t((uint64_t(Next()) * count) >> 32); }
		int32_t Range(int32_t min, int32_t max) { return min + int32_t(Below(uint32_t(max - min + 1))); }
		float Float() { return float(Next() >> 8) * (1.0f / 16777216.0f); }

	private:
		uint64_t m_State = 0;
	};
}
