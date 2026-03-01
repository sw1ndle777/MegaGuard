// =============================================================================
// ConnectHandler - Network connect/ack hooks
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::game {

class DlgRightClick {
public:
    explicit DlgRightClick(::mg::MegaGuardContext& ctx);
    ~DlgRightClick();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::game
