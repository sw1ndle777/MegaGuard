// =============================================================================
// AntiDebugEngine - Debug detection and evasion
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg {

class MegaGuardContext;

class AntiDebugEngine {
public:
    explicit AntiDebugEngine(MegaGuardContext& ctx);
    ~AntiDebugEngine();

    AntiDebugEngine(const AntiDebugEngine&) = delete;
    AntiDebugEngine& operator=(const AntiDebugEngine&) = delete;

    VoidResult initialize();
    void hideCurrentThread();
    bool checkDebugPort();

private:
    MegaGuardContext& ctx_;
};

} // namespace mg
