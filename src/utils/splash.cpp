// =============================================================================
// Splash - Implementation
// =============================================================================
#include "pch.hpp"
#include "utils/splash.hpp"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

namespace mg {

Splash::Splash() = default;
Splash::~Splash() = default;

void Splash::start() {
    // GDI+ splash window init (placeholder — kept minimal for now)
    loadProgress_.store(0);
}

void Splash::updateProgress(int progress) {
    loadProgress_.store(progress);
}

void Splash::onManagerReady() {
    auto ready = managersReady_.fetch_add(1) + 1;
    if (ready >= kTotalManagers)
        allReady_.store(true);
    updateProgress(static_cast<int>((ready * 100) / kTotalManagers));
}

} // namespace mg
