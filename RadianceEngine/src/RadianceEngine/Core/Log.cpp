#include "Log.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>

namespace Rdn
{
    namespace
    {
        std::mutex s_Mutex;
        std::ofstream s_File;

        std::string_view LevelName(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Info: return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Fatal: return "FATAL";
            }
            return "?";
        }

        std::string_view LevelColor(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Warning: return "\x1b[33m";
            case LogLevel::Error: return "\x1b[31m";
            case LogLevel::Fatal: return "\x1b[97;41m";
            default: return {};
            }
        }

        bool EnableConsoleColors()
        {
            const HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD mode = 0;
            return GetConsoleMode(console, &mode) && SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }

        std::string FormatLine(LogLevel level, const std::source_location& location, std::string_view message)
        {
            const auto now = std::chrono::system_clock::now();
            const std::time_t time = std::chrono::system_clock::to_time_t(now);
            std::tm local{};
            localtime_s(&local, &time);
            const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

            std::string line = std::format("[{:02}-{:02}-{:04} {:02}:{:02}:{:02}.{:03}] {}: {}",
                local.tm_mday, local.tm_mon + 1, local.tm_year + 1900, local.tm_hour, local.tm_min, local.tm_sec, milliseconds,
                LevelName(level), message);
            if (level == LogLevel::Fatal)
            {
                std::string_view file = location.file_name();
                file.remove_prefix(file.find_last_of("/\\") + 1);
                line += std::format(" ({}:{})", file, location.line());
            }
            return line;
        }
    }

    void SetLogFile(const std::string& filename)
    {
        bool opened = false;
        {
            std::scoped_lock lock(s_Mutex);
            s_File.close();
            s_File.open(filename, std::ios::app);
            opened = s_File.is_open();
        }
        if (!opened)
        {
            RDN_LOG_ERROR("Failed to open log file {}", filename);
        }
    }

    void WriteLog(LogLevel level, const std::source_location& location, std::string_view message)
    {
        static const bool colors = EnableConsoleColors();
        const std::string line = FormatLine(level, location, message);
        const bool debugger = IsDebuggerPresent();
        {
            std::scoped_lock lock(s_Mutex);
            const std::string_view color = colors ? LevelColor(level) : std::string_view();
            if (color.empty())
                std::cout << line << std::endl;
            else
                std::cout << color << line << "\x1b[0m" << std::endl;

            if (s_File.is_open())
                s_File << line << std::endl;
            if (debugger)
                OutputDebugStringA((line + '\n').c_str());
        }

        if (level == LogLevel::Fatal && debugger)
            __debugbreak();
    }
}
