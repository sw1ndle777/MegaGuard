// =============================================================================
// AuthorizeHandler - Main authorization flow
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::game {

class AuthorizeHandler {
public:
    explicit AuthorizeHandler(::mg::MegaGuardContext& ctx);
    ~AuthorizeHandler();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::game
