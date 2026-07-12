// =============================================================================
// IATScrubber - Implementation
// =============================================================================
#include "pch.hpp"
#include "anticheat/iat_scrubber.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg {

IATScrubber::IATScrubber(MegaGuardContext& ctx) : ctx_(ctx) {}
IATScrubber::~IATScrubber() = default;

VoidResult IATScrubber::scrubDebugImports() {
#if !MG_PROFILE_DEV
    mg::cw::scrub_debug_imports();
#endif
    return VoidResult::ok();
}

} // namespace mg
