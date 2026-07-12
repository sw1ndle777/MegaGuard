// =============================================================================
// ConnectHandler - Network connect/ack hooks
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::game {

class DlgInputPopup {
public:
    explicit DlgInputPopup(::mg::MegaGuardContext& ctx);
    ~DlgInputPopup();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::game
