// =============================================================================
// NationIndex - Placeholder (patching handled by strip_client)
// =============================================================================
#include "pch.hpp"
#include "modding/features/nation_index.hpp"
#include "core/context.hpp"

namespace mg::modding {

NationIndex::NationIndex(MegaGuardContext& ctx) : ctx_(ctx) {}
NationIndex::~NationIndex() = default;

VoidResult NationIndex::install()
{
    return VoidResult::ok();
}

} // namespace mg::modding
