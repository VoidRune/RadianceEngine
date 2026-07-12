#pragma once

namespace Rdn
{
	class Timer
	{
	public:
		Timer();
		void reset_time();
		/* Time elapsed since creation or reset_time in seconds */
		double elapsed_sec();
		/* Time elapsed since creation or reset_time in miliseconds */
		double elapsed_mili();
		/* Time elapsed since creation or reset_time in microseconds */
		double elapsed_micro();

	};
}