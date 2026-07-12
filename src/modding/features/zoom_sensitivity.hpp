// =============================================================================
// ZoomSensitivity - Per-zoom (rifle ADS / sniper scope) mouse sensitivity + accel
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg { class MegaGuardContext; }

namespace mg::modding {

class ZoomSensitivity {
public:
    explicit ZoomSensitivity(::mg::MegaGuardContext& ctx);
    ~ZoomSensitivity();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::modding
