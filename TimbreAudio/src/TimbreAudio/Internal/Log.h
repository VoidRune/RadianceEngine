#pragma once
#include "TimbreAudio/Types.h"
#include <format>
#include <string_view>
#include <utility>

namespace Timbre::Internal
{
	// The callback is process wide. Returns a token for ResetLogCallback().
	uint64_t SetLogCallback(LogCallback callback);
	// Goes back to printing, unless another callback was set since the one for this token.
	void ResetLogCallback(uint64_t token);
	void WriteLog(LogLevel level, std::string_view message);

	template<typename... Args>
	void Log(LogLevel level, std::format_string<Args...> format, Args&&... args)
	{
		WriteLog(level, std::format(format, std::forward<Args>(args)...));
	}
}

#define TIMBRE_LOG_INFO(...) ::Timbre::Internal::Log(::Timbre::LogLevel::Info, __VA_ARGS__)
#define TIMBRE_LOG_WARNING(...) ::Timbre::Internal::Log(::Timbre::LogLevel::Warning, __VA_ARGS__)
#define TIMBRE_LOG_ERROR(...) ::Timbre::Internal::Log(::Timbre::LogLevel::Error, __VA_ARGS__)
