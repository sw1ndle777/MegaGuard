// =============================================================================
// WeaponDrop - restore weapon-drop-on-death + diagnostic  (1.0.3)
// =============================================================================
// In 0.6.7, every death handler called StartDropWeapon directly; in 1.0.3 it was
// refactored into CCh_BasicDropper and is invoked on death via the dropper's
// vtable[1] (== 0x00884A10) from the death-effect Start (e.g. sub_80A840).
//
// The call IS reached on death; the gun's physics-throw inside StartDropWeapon is
// gated on the equipped weapon owning a PhysX prop (weapon + 0x264). This hook
// logs the whole chain on each death so we can see which link is null, then apply
// the targeted fix.
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::modding {

class WeaponDrop {
public:
    explicit WeaponDrop(::mg::MegaGuardContext& ctx);
    ~WeaponDrop();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::modding
