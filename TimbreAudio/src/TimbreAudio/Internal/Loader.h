#pragma once
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace Timbre::Internal
{
	// Runs loading jobs on background threads.
	class Loader
	{
	public:
		explicit Loader(uint32_t threads);
		~Loader();
		Loader(const Loader&) = delete;
		Loader& operator=(const Loader&) = delete;

		void Enqueue(std::function<void()> job);

	private:
		void Run();

		std::mutex m_Mutex;
		std::condition_variable m_Condition;
		std::deque<std::function<void()>> m_Jobs;
		bool m_Stopping = false;
		std::vector<std::thread> m_Threads;
	};
}
