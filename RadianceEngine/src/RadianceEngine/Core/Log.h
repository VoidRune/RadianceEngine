#pragma once
#include <format>
#include <source_location>
#include <string>
#include <string_view>

namespace Rdn
{
    enum class LogLevel
    {
        Info,
        Warning,
        Error,
        Fatal,
    };

    void SetLogFile(const std::string& filename);
    void WriteLog(LogLevel level, const std::source_location& location, std::string_view message);

    template<typename... Args>
    void Log(LogLevel level, const std::source_location& location, std::format_string<Args...> format, Args&&... args)
    {
        WriteLog(level, location, std::format(format, std::forward<Args>(args)...));
    }
}

#if defined(DEBUG) || defined(RELEASE)
#define RDN_LOG(...) Rdn::Log(Rdn::LogLevel::Info, std::source_location::current(), __VA_ARGS__)
#define RDN_LOG_WARNING(...) Rdn::Log(Rdn::LogLevel::Warning, std::source_location::current(), __VA_ARGS__)
#define RDN_LOG_ERROR(...) Rdn::Log(Rdn::LogLevel::Error, std::source_location::current(), __VA_ARGS__)
#else
#define RDN_LOG(...) ((void)0)
#define RDN_LOG_WARNING(...) ((void)0)
#define RDN_LOG_ERROR(...) ((void)0)
#endif
#define RDN_LOG_FATAL(...) Rdn::Log(Rdn::LogLevel::Fatal, std::source_location::current(), __VA_ARGS__)
