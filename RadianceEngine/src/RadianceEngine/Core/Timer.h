#pragma once
#include <chrono>

namespace Rdn
{
	class Timer
	{
	public:
		Timer();
		void Reset();
		double ElapsedSeconds() const;
		double ElapsedMilliseconds() const;
		double ElapsedMicroseconds() const;

	private:
		using Clock = std::chrono::steady_clock;
		Clock::time_point m_Start;
	};
}
