// =============================================================================
// IntegrityEngine - Implementation
// =============================================================================
#include "pch.hpp"
#include "anticheat/integrity_engine.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg {

IntegrityEngine::IntegrityEngine(MegaGuardContext& ctx) : ctx_(ctx) {}
IntegrityEngine::~IntegrityEngine() = default;

VoidResult IntegrityEngine::initialize(uptr moduleBase, u32 moduleSize) {
    moduleBase_ = moduleBase;
    moduleSize_ = moduleSize;

    // Compute initial hash of .text section
    if (moduleBase_ == 0 || moduleSize_ == 0)
        return VoidResult::err(ErrorCode::kInvalidAddress);

    // FNV-1a hash of the code section
    u32 hash = 0x811C9DC5u;
    auto* ptr = reinterpret_cast<const u8*>(moduleBase_);
    for (u32 i = 0; i < moduleSize_; ++i) {
        hash ^= ptr[i];
        hash *= 0x01000193u;
    }
    originalHash_ = hash;
    initialized_ = true;

    return VoidResult::ok();
}

bool IntegrityEngine::verifyIntegrity() {
    if (!initialized_) return true; // Not yet set up

    u32 hash = 0x811C9DC5u;
    auto* ptr = reinterpret_cast<const u8*>(moduleBase_);
    for (u32 i = 0; i < moduleSize_; ++i) {
        hash ^= ptr[i];
        hash *= 0x01000193u;
    }

    return hash == originalHash_;
}

} // namespace mg
