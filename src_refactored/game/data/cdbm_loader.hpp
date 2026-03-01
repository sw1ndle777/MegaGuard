// =============================================================================
// CDBMLoader - Custom CDBM file loading
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::game {

class CDBMLoader {
public:
    explicit CDBMLoader(::mg::MegaGuardContext& ctx);
    ~CDBMLoader();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::game
