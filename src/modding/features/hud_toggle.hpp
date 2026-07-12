// =============================================================================
// HudToggle - Legacy CTRL+U HUD visibility toggle
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::modding {

class HudToggle {
public:
    explicit HudToggle(::mg::MegaGuardContext& ctx);
    ~HudToggle();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::modding
