// =============================================================================
// WindowCloseFix - Taskbar close (WM_CLOSE) bugfix
// =============================================================================
// The game's WndProc swallows WM_CLOSE when its UI subsystem is active,
// preventing the window from closing via taskbar right-click → Close.
// This detour intercepts WM_CLOSE before the original handler.
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::modding {

class WindowCloseFix {
public:
    explicit WindowCloseFix(::mg::MegaGuardContext& ctx);
    ~WindowCloseFix();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::modding
