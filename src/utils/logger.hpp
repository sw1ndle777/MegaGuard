// =============================================================================
// Logger - Async file + console logger
// =============================================================================
#pragma once

#include "core/types.hpp"
#include <string>

namespace mg {

class Logger {
public:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    VoidResult initialize(const std::string& logDir = "logs");
    void shutdown();

    // Aliases for common use
    VoidResult init(const std::string& path, bool /*async*/ = true) { return initialize(path); }
    void stop() { shutdown(); }

    template <typename... Args>
    void debug(const char* file, int line, const char* func, fmt::format_string<Args...> fmtStr, Args&&... args) {
        auto msg = fmt::format(fmtStr, std::forward<Args>(args)...);
        write(file, line, func, msg);
    }

    // Convenience: info/error/warn without source location (used by context)
    template <typename... Args>
    void info(fmt::format_string<Args...> fmtStr, Args&&... args) {
        auto msg = fmt::format(fmtStr, std::forward<Args>(args)...);
        write("", 0, "", msg);
    }
    template <typename... Args>
    void error(fmt::format_string<Args...> fmtStr, Args&&... args) {
        auto msg = fmt::format(fmtStr, std::forward<Args>(args)...);
        write("", 0, "", msg);
    }

    // Convenience macro-friendly overload
    void write(const char* file, int line, const char* func, const std::string& message);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

// Macro for source location capture
#define MG_LOG(logger, ...) \
    (logger).debug(__FILE__, __LINE__, __func__, __VA_ARGS__)

} // namespace mg
