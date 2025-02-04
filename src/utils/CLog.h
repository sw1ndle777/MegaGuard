#pragma once

namespace MegaGuard
{
    class CLog
    {
    public:
        void Initialize(const std::string& Path, bool removeExisting = false);
        void Write(const std::string& Text);
        void Add(const char* format, ...);

        void Info(const char* format, ...);
        void Warning(const char* format, ...);
        void Error(const char* format, ...);
        void Verbose(const char* format, ...);

        std::string extractFunctionName(const std::string& str)
        {
            std::string_view result = str;

            result = result.substr(0, result.find("::<"));
            result = result.substr(0, result.find('('));
            result = result.substr(result.find_last_of(' ') + 1);

            return std::string(result);
        }
        std::string extractFileName(const std::string& path)
        {
            const auto& pos = path.find_last_of("/\\");
            return (pos != std::string::npos) ? path.substr(pos + 1) : path;
        }
        void Stop()
        {
            stopLogging = true;
            cv.notify_all();
            if (logThread && logThread->joinable())
            {
                logThread->join();
            }
        }
        ~CLog()
        {
            Stop();
            if (File.is_open())  File.close();
        }
        template <typename... Args>
        void Debug(std::source_location source_location, std::string_view format, Args&&... args)
        {
            const auto file_name = extractFileName(source_location.file_name());
            std::string function_name = extractFunctionName(source_location.function_name());

            std::string source_debug_info = std::format("({}:{}) {}() ", file_name, source_location.line(), function_name);
            std::string formattedMessage = std::vformat(format, std::make_format_args(args...));

            {
                std::unique_lock<std::mutex> lock(queueMutex);
                logQueue.push(source_debug_info + formattedMessage);
            }
            cv.notify_one();
        }

        void ProcessQueue()
        {
            while (!stopLogging)
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                cv.wait(lock, [this] { return !logQueue.empty() || stopLogging; });

                while (!logQueue.empty())
                {
                    auto logEntry = logQueue.front();
                    logQueue.pop();
                    lock.unlock();
                #if DEBUG_FILE_LOG == 1
                    Write(logEntry);
                #endif
                #if DEBUG_CONSOLE_LOG == 1
                    std::printf("%s\n", logEntry.c_str());
                #endif

                    lock.lock();
                }
            }
        }
    private:

        std::queue<std::string> logQueue;
        std::mutex queueMutex;
        std::condition_variable cv;
        std::optional<std::jthread> logThread;
        std::atomic<bool> stopLogging;

        std::mutex WriteMutex;
        std::ofstream File;
    };

    extern std::unique_ptr<CLog> EventLog;
}

//#endif