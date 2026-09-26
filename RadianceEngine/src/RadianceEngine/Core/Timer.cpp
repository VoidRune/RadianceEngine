#include "Timer.h"

namespace Rdn
{
	Timer::Timer()
		: m_Start(Clock::now())
	{
	}

	void Timer::Reset()
	{
		m_Start = Clock::now();
	}

	double Timer::ElapsedSeconds() const
	{
		return std::chrono::duration<double>(Clock::now() - m_Start).count();
	}

	double Timer::ElapsedMilliseconds() const
	{
		return std::chrono::duration<double, std::milli>(Clock::now() - m_Start).count();
	}

	double Timer::ElapsedMicroseconds() const
	{
		return std::chrono::duration<double, std::micro>(Clock::now() - m_Start).count();
	}
}
