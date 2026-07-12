// =============================================================================
// PcBang - PC room dialog initialization hooks
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::modding {

class PcBang {
public:
    explicit PcBang(::mg::MegaGuardContext& ctx);
    ~PcBang();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
    bool firstTimeLobby_ = true;
};

} // namespace mg::modding
