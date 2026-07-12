#pragma once
#include <string>
#include <format>

namespace Rdn
{
    void SetLogFile(const std::string& filename);
    void LogMessageInternal(const std::string& level, const std::string& message);

    template<typename... Args>
    inline void LogMessage(const std::string& level, std::string_view format, Args&&... args)
    {
        std::string message = std::vformat(format, std::make_format_args(args...));
        LogMessageInternal(level, message);
    }

    template<typename... Args>
    inline void LogInfo(std::string_view format, Args&&... args)
    {
        LogMessage("INFO", format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    inline void LogWarning(std::string_view format, Args&&... args)
    {
        LogMessage("WARN", format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    inline void LogError(std::string_view format, Args&&... args)
    {
        LogMessage("ERROR", format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    inline void LogFatal(std::string_view format, Args&&... args)
    {
        LogMessage("FATAL", format, std::forward<Args>(args)...);
    }
}

#if defined(DEBUG) || defined(RELEASE)
#define RDN_LOG(...) Rdn::LogInfo(__VA_ARGS__)
#define RDN_LOG_WARNING(...) Rdn::LogWarning(__VA_ARGS__)
#define RDN_LOG_ERROR(...) Rdn::LogError(__VA_ARGS__)
#define RDN_LOG_FATAL(...) Rdn::LogFatal(__VA_ARGS__)
#else
#define RDN_LOG(...)
#define RDN_LOG_WARNING(...)
#define RDN_LOG_ERROR(...)
#define RDN_LOG_FATAL(...)
#endif