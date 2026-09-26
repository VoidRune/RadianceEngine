#include "Random.h"

namespace Rdn
{
	namespace
	{
		struct Pcg32
		{
			static constexpr uint64_t Multiplier = 6364136223846793005ULL;
			static constexpr uint64_t Increment = 0xda3e39cb94b95bdbULL;
			uint64_t State = 0x853c49e6748fea9bULL;

			uint32_t Next()
			{
				const uint64_t old = State;
				State = old * Multiplier + Increment;
				const uint32_t xorShifted = uint32_t(((old >> 18) ^ old) >> 27);
				const uint32_t rotation = uint32_t(old >> 59);
				return (xorShifted >> rotation) | (xorShifted << ((32 - rotation) & 31));
			}
		};

		thread_local Pcg32 s_Generator;
	}

	void Random::SetSeed(uint64_t seed)
	{
		s_Generator.State = 0;
		s_Generator.Next();
		s_Generator.State += seed;
		s_Generator.Next();
	}

	uint32_t Random::UInt()
	{
		return s_Generator.Next();
	}

	int32_t Random::Range(int32_t min, int32_t max)
	{
		const uint64_t span = uint64_t(int64_t(max) - int64_t(min)) + 1;
		return int32_t(int64_t(min) + int64_t((uint64_t(UInt()) * span) >> 32));
	}

	bool Random::Chance(float probability)
	{
		return Float() < probability;
	}

	float Random::Float()
	{
		return float(UInt() >> 8) * 0x1.0p-24f;
	}

	float Random::Float(float max)
	{
		return Float() * max;
	}

	float Random::Float(float min, float max)
	{
		return min + Float() * (max - min);
	}
}
