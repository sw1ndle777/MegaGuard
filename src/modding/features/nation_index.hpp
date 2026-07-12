// =============================================================================
// NationIndex - Custom nation index + window title hooks
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::modding {

class NationIndex {
public:
    explicit NationIndex(::mg::MegaGuardContext& ctx);
    ~NationIndex();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::modding
