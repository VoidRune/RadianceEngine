#include "Random.h"


static uint32_t s_seed = 0;
void Random::SetSeed(uint32_t seed)
{
	s_seed = seed;
}

/* https://www.youtube.com/watch?v=ZZY9YE7rZJw */
static uint32_t Lehmer32()
{
	s_seed += 0xe120fc15;
	uint64_t tmp;
	tmp = (uint64_t)s_seed * 0x4a39b70d;
	uint32_t m1 = static_cast<uint32_t>((tmp >> 32) ^ tmp);
	tmp = (uint64_t)m1 * 0x12fad5c9;
	uint32_t m2 = static_cast<uint32_t>((tmp >> 32) ^ tmp);
	return m2;
}

int32_t Random::Range(int32_t min, int32_t max)
{
	return Lehmer32() % (max - min + 1) + min;
}

bool Random::Chance(float chance)
{
	return (Range(0, 100000) / 100000.0f) <= chance;
}

float Random::Float()
{
	return Lehmer32() / (float)std::numeric_limits<uint32_t>::max();
}

float Random::Float(float max)
{
	return Lehmer32() / (float)std::numeric_limits<uint32_t>::max() * max;
}

float Random::Float(float min, float max)
{
	return min + Lehmer32() / (float)std::numeric_limits<uint32_t>::max() * (max - min);
}