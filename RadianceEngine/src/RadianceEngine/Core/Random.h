#pragma once
#include <cstdint>

namespace Rdn
{
	class Random
	{
	public:
		static void SetSeed(uint64_t seed);

		static uint32_t UInt();
		static int32_t Range(int32_t min, int32_t max);
		static bool Chance(float probability);

		static float Float();
		static float Float(float max);
		static float Float(float min, float max);
	};
}
