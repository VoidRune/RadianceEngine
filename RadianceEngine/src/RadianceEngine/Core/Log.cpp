#include "Log.h"

#include <chrono>
#include <iostream>
#include <fstream>
#include <mutex>

namespace Rdn
{
    std::ofstream logFile;
    std::mutex logMutex;

    void SetLogFile(const std::string& filename)
    {
        logFile.open(filename, std::ios::app);
        if (!logFile)
        {
            std::cerr << "[ERROR] Failed to open log file: " << filename << std::endl;
        }
    }

    std::string GetTimeStamp()
    {
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        tm localTime{};
        localtime_s(&localTime, &timeT);

        char buffer[20];
        strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", &localTime);
        return std::string(buffer);
    }

    void LogMessageInternal(const std::string& level, const std::string& message)
    {
        std::lock_guard<std::mutex> lock(logMutex);

        std::string timestamp = GetTimeStamp();

        std::cout << "[" << timestamp << "] " << level << ": " << message << std::endl;
        if (logFile.is_open())
        {
            logFile << "[" << timestamp << "] " << level << ": " << message << std::endl;
        }
    }
}