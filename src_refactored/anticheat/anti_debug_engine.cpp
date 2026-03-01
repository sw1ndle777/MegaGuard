// =============================================================================
// AntiDebugEngine - Implementation
// =============================================================================
#include "pch.hpp"
#include "anticheat/anti_debug_engine.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg {

AntiDebugEngine::AntiDebugEngine(MegaGuardContext& ctx) : ctx_(ctx) {}
AntiDebugEngine::~AntiDebugEngine() = default;

VoidResult AntiDebugEngine::initialize() {
    hideCurrentThread();
    return VoidResult::ok();
}

void AntiDebugEngine::hideCurrentThread() {
    mg::cw::hide_thread();
}

bool AntiDebugEngine::checkDebugPort() {
#if !MG_PROFILE_DEV
    mg::cw::check_debug_port();
#endif
    return false;
}

} // namespace mg
