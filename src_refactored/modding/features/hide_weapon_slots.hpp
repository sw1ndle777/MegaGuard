// =============================================================================
// HideWeaponSlots - MidHook callback for weapon slot visibility
// =============================================================================
#pragma once

#include "core/types.hpp"
#include "engine/midhook.hpp"

namespace mg::modding {

class HideWeaponSlots {
public:
    explicit HideWeaponSlots(::mg::MegaGuardContext& ctx);
    ~HideWeaponSlots();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
    MidHook midHook_;
};

// Midhook callback — forces weapon slot visibility byte to 1
void __cdecl hideWeaponSlotsCallback(mg::RegisterState regs);

} // namespace mg::modding
