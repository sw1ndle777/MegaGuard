#pragma once

#include "core/types.hpp"

namespace mg::modding {

class EditBoxClipboard {
public:
    explicit EditBoxClipboard(::mg::MegaGuardContext& ctx);
    ~EditBoxClipboard();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::modding
