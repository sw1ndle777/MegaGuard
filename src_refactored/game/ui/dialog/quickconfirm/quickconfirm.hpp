// =============================================================================
// DlgQuickConfirm - Spacebar/Enter quick confirm for dialog popups
// =============================================================================
// Swaps vtable entries on dialog classes to add keyboard confirm support.
// Each dialog (CDlgGachaResult, CDlgPackageItem, etc.) has its own handler.
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::game {

class DlgQuickConfirm {
public:
    explicit DlgQuickConfirm(::mg::MegaGuardContext& ctx);
    ~DlgQuickConfirm();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::game
