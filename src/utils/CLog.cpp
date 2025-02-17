#include "..\pch.h"

namespace MegaGuard
{

    void CLog::Initialize(const std::string& Path, bool removeExisting)
    {

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
            throw std::runtime_error("Could not open file: " + Path);
        }

        logThread = std::make_unique<std::thread>(&CLog::ProcessQueue, this);
    }

    void CLog::Write(const std::string& Text)
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "[%d-%m-%Y %H:%M:%S]", std::localtime(&time));

        const std::string Output = fmt::format("{} {}", buffer, Text);

        //std::scoped_lock lock(WriteMutex);
        File << Output << std::endl;
    }

    void CLog::Add(const char* format, ...)
    {
        char buffer[8192] = { 0 };
        va_list arglist;

        va_start(arglist, format);
        vsprintf(buffer, format, arglist);
        va_end(arglist);

        Write(buffer);
    }

    void CLog::Info(const char* format, ...)
    {
        char buffer[8192] = { 0 };
        va_list arglist;

        va_start(arglist, format);
        vsprintf(buffer, format, arglist);
        va_end(arglist);

        Write(fmt::format("[INFO] {}", buffer));
    }

    void CLog::Warning(const char* format, ...)
    {
        char buffer[8192] = { 0 };
        va_list arglist;

        va_start(arglist, format);
        vsprintf(buffer, format, arglist);
        va_end(arglist);

        Write(fmt::format("[WARNING] {}", buffer));
    }

    void CLog::Error(const char* format, ...)
    {
        char buffer[8192] = { 0 };
        va_list arglist;

        va_start(arglist, format);
        vsprintf(buffer, format, arglist);
        va_end(arglist);

        Write(fmt::format("[ERROR] {}", buffer));
    }

    void CLog::Verbose(const char* format, ...)
    {
        char buffer[8192] = { 0 };
        va_list arglist;

        va_start(arglist, format);
        vsprintf(buffer, format, arglist);
        va_end(arglist);

        Write(fmt::format("[VERBOSE] {}", buffer));
    }

    std::unique_ptr<CLog> EventLog = std::make_unique<CLog>();
}