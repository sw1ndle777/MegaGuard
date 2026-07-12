// =============================================================================
// DisconnectHandler - Order 73 disconnect packet hook
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::game {

class DisconnectHandler {
public:
    explicit DisconnectHandler(::mg::MegaGuardContext& ctx);
    ~DisconnectHandler();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::game
