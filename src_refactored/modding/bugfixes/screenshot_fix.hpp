// =============================================================================
// ScreenshotFix - D3D9/GDI screenshot capture bugfix
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::modding {

class ScreenshotFix {
public:
    explicit ScreenshotFix(::mg::MegaGuardContext& ctx);
    ~ScreenshotFix();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::modding
