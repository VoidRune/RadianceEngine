#pragma once
#include "Timer.h"

namespace Rdn
{
	class FrameLimiter
	{
	public:
		FrameLimiter();
		~FrameLimiter();
		FrameLimiter(const FrameLimiter&) = delete;
		FrameLimiter& operator=(const FrameLimiter&) = delete;

		// Sleeps until the next frame is due and returns the seconds slept.
		double Wait(double framesPerSecond);

	private:
		void SleepFor(double seconds);

		Timer m_Timer;
		double m_Next = 0.0;
		double m_Overshoot = 0.001;
		void* m_WaitableTimer = nullptr;
	};
}
