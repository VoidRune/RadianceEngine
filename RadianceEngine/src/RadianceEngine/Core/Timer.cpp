#include "Timer.h"
#include <chrono>

using namespace std::chrono;


namespace Rdn
{
	time_point<high_resolution_clock> timeStamp;

	Timer::Timer()
	{
		reset_time();
	}

	void Timer::reset_time()
	{
		timeStamp = high_resolution_clock::now();
	}

	double Timer::elapsed_sec()
	{
		return 0.000001 * (time_point_cast<microseconds>(high_resolution_clock::now()).time_since_epoch().count() - time_point_cast<microseconds>(timeStamp).time_since_epoch().count());
	}
	double Timer::elapsed_mili()
	{
		return 0.001 * (time_point_cast<microseconds>(high_resolution_clock::now()).time_since_epoch().count() - time_point_cast<microseconds>(timeStamp).time_since_epoch().count());
	}
	double Timer::elapsed_micro()
	{
		return (double)time_point_cast<microseconds>(high_resolution_clock::now()).time_since_epoch().count() - time_point_cast<microseconds>(timeStamp).time_since_epoch().count();
	}
}