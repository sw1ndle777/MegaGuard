#pragma once

namespace MegaGuard
{
    class CLog
    {
    public:
        void Initialize(const std::string& Path, bool removeExisting = false);
        void Write(const std::string& Text);

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
        void Debug(nostd::source_location source_location, std::string_view format, Args&&... args)
        {
#if DEBUG_CONSOLE_LOG == 1 || DEBUG_FILE_LOG == 1
            const auto file_name = extractFileName(source_location.file_name());
            std::string function_name = extractFunctionName(source_location.function_name());

            std::string source_debug_info = fmt::format("({}:{}) {}() ", file_name, source_location.line(), function_name);
            std::string formattedMessage = fmt::vformat(format, fmt::make_format_args(args...));

            {
                std::unique_lock<std::mutex> lock(queueMutex);
                logQueue.push(source_debug_info + formattedMessage);
            }
            cv.notify_one();
#endif
        }

        void ProcessQueue()
        {
#if DEBUG_CONSOLE_LOG == 1 || DEBUG_FILE_LOG == 1
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
                    fmt::print("{}\n", logEntry.c_str());
                #endif

                    lock.lock();
                }
            }
#endif
        }

        bool DirectoryExists(const std::string& path)
        {
            DWORD ftyp = GetFileAttributesA(path.c_str());
            return (ftyp != INVALID_FILE_ATTRIBUTES && (ftyp & FILE_ATTRIBUTE_DIRECTORY));
        }

        bool FileExists(const std::string& path)
        {
            std::ifstream file(path);
            return file.good();
        }

        std::string GetParentDirectory(const std::string& filepath)
        {
            size_t found = filepath.find_last_of("/\\");
            return (found != std::string::npos) ? filepath.substr(0, found) : "";
        }

        bool CreateDirectoryRecursive(const std::string& path)
        {

            if (path.empty())
                return false;

#if DEBUG_CONSOLE_LOG == 1 || DEBUG_FILE_LOG == 1

            std::string tempPath;
            std::istringstream pathStream(path);
            std::string segment;
            while (std::getline(pathStream, segment, '\\'))
            {
                if (!tempPath.empty())
                    tempPath += "\\";
                tempPath += segment;

                if (!DirectoryExists(tempPath))
                {
                    if (_mkdir(tempPath.c_str()) != 0 && errno != EEXIST)
                    {
                        std::cerr << "Failed to create directory: " << tempPath << " Error: " << strerror(errno) << std::endl;
                        return false;
                    }
                }
            }
#endif
            return true;
        }
    private:

        std::queue<std::string> logQueue;
        std::mutex queueMutex;
        std::condition_variable cv;
        std::unique_ptr<std::thread> logThread;
        std::atomic<bool> stopLogging;

        std::mutex WriteMutex;
        std::ofstream File;
    };

    extern std::unique_ptr<CLog> EventLog;
}

//#endif