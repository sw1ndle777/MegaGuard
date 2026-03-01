// =============================================================================
// CustomTickrate - Frame time / rotation / delay / distance patches
// =============================================================================
// Patches ~90 hardcoded float constants throughout the game binary to enable
// a custom tick rate. All patches are applied as SwapAddressPatch or PatchBytes.
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::modding {

class CustomTickrate {
public:
    explicit CustomTickrate(::mg::MegaGuardContext& ctx);
    ~CustomTickrate();

    VoidResult patch();
    void unpatch();

private:
    ::mg::MegaGuardContext& ctx_;
    bool patched_ = false;
};

} // namespace mg::modding
