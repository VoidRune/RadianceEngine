#include "Loader.h"
#include <algorithm>

namespace Timbre::Internal
{
	Loader::Loader(uint32_t threads)
	{
		for (uint32_t i = 0; i < std::max(threads, 1u); i++)
			m_Threads.emplace_back([this] { Run(); });
	}

	Loader::~Loader()
	{
		{
			std::scoped_lock lock(m_Mutex);
			m_Stopping = true;
			m_Jobs.clear();
		}
		m_Condition.notify_all();
		for (std::thread& thread : m_Threads)
			thread.join();
	}

	void Loader::Enqueue(std::function<void()> job)
	{
		{
			std::scoped_lock lock(m_Mutex);
			m_Jobs.push_back(std::move(job));
		}
		m_Condition.notify_one();
	}

	void Loader::Run()
	{
		while (true)
		{
			std::function<void()> job;
			{
				std::unique_lock lock(m_Mutex);
				m_Condition.wait(lock, [this] { return m_Stopping || !m_Jobs.empty(); });
				if (m_Stopping)
					return;
				job = std::move(m_Jobs.front());
				m_Jobs.pop_front();
			}
			job();
		}
	}
}
