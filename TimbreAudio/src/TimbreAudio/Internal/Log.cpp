#include "Log.h"
#include <cstdio>
#include <memory>
#include <mutex>

namespace Timbre::Internal
{
	namespace
	{
		std::mutex s_Mutex;
		// Shared so it can be called without holding the lock while another thread replaces it.
		std::shared_ptr<const LogCallback> s_Callback;
		uint64_t s_Token = 0;

		const char* LevelName(LogLevel level)
		{
			switch (level)
			{
			case LogLevel::Info: return "INFO";
			case LogLevel::Warning: return "WARN";
			case LogLevel::Error: return "ERROR";
			}
			return "?";
		}
	}

	uint64_t SetLogCallback(LogCallback callback)
	{
		std::scoped_lock lock(s_Mutex);
		s_Callback = callback ? std::make_shared<const LogCallback>(std::move(callback)) : nullptr;
		return ++s_Token;
	}

	void ResetLogCallback(uint64_t token)
	{
		std::scoped_lock lock(s_Mutex);
		if (token == s_Token)
			s_Callback.reset();
	}

	void WriteLog(LogLevel level, std::string_view message)
	{
		std::shared_ptr<const LogCallback> callback;
		{
			std::scoped_lock lock(s_Mutex);
			callback = s_Callback;
		}
		if (callback)
		{
			(*callback)(level, message);
			return;
		}
		std::scoped_lock lock(s_Mutex);
		std::printf("[Timbre] %s: %.*s\n", LevelName(level), int(message.size()), message.data());
		std::fflush(stdout);
	}
}
