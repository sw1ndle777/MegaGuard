#include "..\pch.h"

namespace MegaGuard
{
    std::unique_ptr<CLog> EventLog;
    void CLog::Initialize(const std::string& Path, bool removeExisting)
    {
#if DEBUG_CONSOLE_LOG == 1 || DEBUG_FILE_LOG == 1
        std::string logDirectory = GetParentDirectory(Path);

        if (!DirectoryExists(logDirectory))
        {
            if (!CreateDirectoryRecursive(logDirectory))
            {
                std::cerr << "Failed to create directory: " << logDirectory << std::endl;
                return;
            }
        }

        if (removeExisting)
        {
            if (FileExists(Path))
                remove(Path.c_str());
        }

        File.open(Path, std::ofstream::out | std::ofstream::trunc);

        if (!File.is_open())
        {
            //throw std::runtime_error("Could not open file: " + Path);
        }

        logThread = std::make_unique<std::thread>(&CLog::ProcessQueue, this);
#endif
    }

    void CLog::Write(const std::string& Text)
    {
#if DEBUG_CONSOLE_LOG == 1 || DEBUG_FILE_LOG == 1
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "[%d-%m-%Y %H:%M:%S]", std::localtime(&time));

        const std::string Output = fmt::format("{} {}", buffer, Text);

        //std::scoped_lock lock(WriteMutex);
        File << Output << std::endl;
#endif
    }
}