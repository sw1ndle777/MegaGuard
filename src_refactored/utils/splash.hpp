// =============================================================================
// Splash - Loading splash screen (GDI+ based)
// =============================================================================
#pragma once

#include "core/types.hpp"
#include <atomic>

namespace mg {

class Splash {
public:
    Splash();
    ~Splash();

    Splash(const Splash&) = delete;
    Splash& operator=(const Splash&) = delete;

    void start();
    void updateProgress(int progress);
    void onManagerReady();
    bool allManagersReady() const { return allReady_.load(); }

    enum { kTotalManagers = 4 };

private:
    std::atomic<int> managersReady_{0};
    std::atomic<bool> allReady_{false};
    std::atomic<int> loadProgress_{0};
};

} // namespace mg
