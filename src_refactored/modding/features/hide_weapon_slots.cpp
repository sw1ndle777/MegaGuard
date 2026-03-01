// =============================================================================
// HideWeaponSlots - Implementation
// =============================================================================
#include "pch.hpp"
#include "modding/features/hide_weapon_slots.hpp"
#include "core/context.hpp"
#include "game/addresses.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::modding {

using namespace mg::game;

void __cdecl hideWeaponSlotsCallback(mg::RegisterState regs) {
    u32 edx_value = *reinterpret_cast<u32*>(regs.ebp - 0x584);
    u32 eax_value = *reinterpret_cast<u32*>(regs.ebp - 0x8AC);
    u32 calculated_offset = edx_value * 0x4C;
    *reinterpret_cast<u8*>(calculated_offset + eax_value + 0x38) = 1;
}

HideWeaponSlots::HideWeaponSlots(MegaGuardContext& ctx) : ctx_(ctx) {}
HideWeaponSlots::~HideWeaponSlots() = default;

VoidResult HideWeaponSlots::install() {
    return midHook_.create(
        MG_CONST(addr::features::HideWeaponSlot),
        hideWeaponSlotsCallback);
}

} // namespace mg::modding
