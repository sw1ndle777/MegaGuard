// =============================================================================
// Logger - Implementation with async queue
// =============================================================================
#include "pch.hpp"
#include "utils/logger.hpp"

#include <queue>
#include <thread>
#include <fstream>
#include <chrono>
#include <ctime>

namespace mg {

struct Logger::Impl {
    std::ofstream logFile;
    std::queue<std::string> messageQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::thread workerThread;
    std::atomic<bool> running{false};
    bool consoleEnabled = false;
    bool fileEnabled = true;

    void workerLoop() {
        while (running.load()) {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !messageQueue.empty() || !running.load();
            });

            while (!messageQueue.empty()) {
                auto msg = std::move(messageQueue.front());
                messageQueue.pop();
                lock.unlock();

                if (fileEnabled && logFile.is_open()) {
                    logFile << msg << "\n";
                    logFile.flush();
                }
#if MG_PROFILE_DEV
                if (consoleEnabled) {
                    OutputDebugStringA(msg.c_str());
                    OutputDebugStringA("\n");
                }
#endif
                lock.lock();
            }
        }
    }
};

Logger::Logger() = default;

Logger::~Logger() {
    shutdown();
}

VoidResult Logger::initialize(const std::string& logDir) {
    impl_ = static_cast<Impl*>(VirtualAlloc(nullptr, sizeof(Impl), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!impl_) return VoidResult::err(ErrorCode::kAllocationFail);
    new (impl_) Impl();

    CreateDirectoryA(logDir.c_str(), nullptr);

    // Generate filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf;
    localtime_s(&tm_buf, &t);

    char filename[128];
    snprintf(filename, sizeof(filename), "%s/megaguard_%04d%02d%02d_%02d%02d%02d.log",
             logDir.c_str(),
             tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
             tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

    impl_->logFile.open(filename, std::ios::out | std::ios::app);
    if (!impl_->logFile.is_open())
        return VoidResult::err(ErrorCode::kInitFailed);

#if MG_PROFILE_DEV
    impl_->consoleEnabled = true;
#endif

    impl_->running.store(true);
    impl_->workerThread = std::thread(&Impl::workerLoop, impl_);

    return VoidResult::ok();
}

void Logger::shutdown() {
    if (!impl_) return;

    impl_->running.store(false);
    impl_->queueCV.notify_all();
    if (impl_->workerThread.joinable())
        impl_->workerThread.join();

    impl_->logFile.close();
    impl_->~Impl();
    VirtualFree(impl_, 0, MEM_RELEASE);
    impl_ = nullptr;
}

void Logger::write(const char* file, int line, const char* func, const std::string& message) {
    if (!impl_ || !impl_->running.load()) return;

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf;
    localtime_s(&tm_buf, &t);

    char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "[%02d:%02d:%02d]",
             tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

    // Extract just the filename from path
    const char* shortFile = file;
    for (const char* p = file; *p; ++p) {
        if (*p == '\\' || *p == '/') shortFile = p + 1;
    }

    auto formatted = fmt::format("{} [{}:{}] {}", timestamp, shortFile, line, message);

    std::lock_guard<std::mutex> lock(impl_->queueMutex);
    impl_->messageQueue.push(std::move(formatted));
    impl_->queueCV.notify_one();
}

} // namespace mg
