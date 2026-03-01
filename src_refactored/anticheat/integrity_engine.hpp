// =============================================================================
// IntegrityEngine - Code integrity verification
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg {

class MegaGuardContext;

class IntegrityEngine {
public:
    explicit IntegrityEngine(MegaGuardContext& ctx);
    ~IntegrityEngine();

    IntegrityEngine(const IntegrityEngine&) = delete;
    IntegrityEngine& operator=(const IntegrityEngine&) = delete;

    VoidResult initialize(uptr moduleBase, u32 moduleSize);
    bool verifyIntegrity();

private:
    MegaGuardContext& ctx_;
    uptr moduleBase_ = 0;
    u32  moduleSize_ = 0;
    u32  originalHash_ = 0;
    bool initialized_ = false;
};

} // namespace mg
