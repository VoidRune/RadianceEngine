#pragma once
#include <cstdint>
#include <limits>

class Random
{
public:
	static void SetSeed(uint32_t seed);

	/* Returns int32_t in range [min, max] */
	static int32_t Range(int32_t min, int32_t max);

	/* returns true/false based on chance  */
	static bool Chance(float chance);

	/* Returns float value in range[0, 1] */
	static float Float();

	/* Returns float value in range[0, max] */
	static float Float(float max);

	/* Returns float value in range[min, max] */
	static float Float(float min, float max);

};