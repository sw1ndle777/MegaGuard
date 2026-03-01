// =============================================================================
// ConnectHandler - Network connect/ack hooks
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::game {

class ConnectHandler {
public:
    explicit ConnectHandler(::mg::MegaGuardContext& ctx);
    ~ConnectHandler();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::game
