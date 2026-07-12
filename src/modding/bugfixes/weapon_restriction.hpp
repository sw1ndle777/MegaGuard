// =============================================================================
// WeaponRestriction - Room dialog weapon restriction bugfix
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::modding {

class WeaponRestriction {
public:
    explicit WeaponRestriction(::mg::MegaGuardContext& ctx);
    ~WeaponRestriction();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::modding
