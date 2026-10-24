#include "FrameLimiter.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif

#include <algorithm>
#include <thread>

namespace Rdn
{

	FrameLimiter::FrameLimiter()
	{
#ifdef _WIN32
		m_WaitableTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
#endif
	}

	FrameLimiter::~FrameLimiter()
	{
#ifdef _WIN32
		if (m_WaitableTimer)
			CloseHandle(m_WaitableTimer);
#endif
	}

	double FrameLimiter::Wait(double framesPerSecond)
	{
		const double period = 1.0 / framesPerSecond;
		const double start = m_Timer.ElapsedSeconds();
		m_Next += period;
		if (m_Next < start - period)
			m_Next = start;
		if (m_Next <= start)
			return 0.0;
		const double margin = std::clamp(m_Overshoot + 0.0002, 0.0002, 0.002);
		if (m_Next - start > margin)
		{
			const double sleep = m_Next - start - margin;
			SleepFor(sleep);
			m_Overshoot = std::max(m_Timer.ElapsedSeconds() - start - sleep, m_Overshoot * 0.99);
		}
		double now = m_Timer.ElapsedSeconds();
		while (now < m_Next)
		{
			std::this_thread::yield();
			now = m_Timer.ElapsedSeconds();
		}
		return now - start;
	}

	void FrameLimiter::SleepFor(double seconds)
	{
#ifdef _WIN32
		if (m_WaitableTimer)
		{
			LARGE_INTEGER due;
			due.QuadPart = -LONGLONG(seconds * 1e7);
			if (SetWaitableTimerEx(m_WaitableTimer, &due, 0, nullptr, nullptr, nullptr, 0))
			{
				WaitForSingleObject(m_WaitableTimer, INFINITE);
				return;
			}
		}
#endif
		std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
	}
}
