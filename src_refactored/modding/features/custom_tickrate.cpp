// =============================================================================
// CustomTickrate - Implementation
// =============================================================================
#include "pch.hpp"
#include "modding/features/custom_tickrate.hpp"
#include "core/context.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::modding {

using namespace mg::game;

namespace tick = addr::features::tickrate;

CustomTickrate::CustomTickrate(MegaGuardContext& ctx) : ctx_(ctx) {}
CustomTickrate::~CustomTickrate() { if (patched_) unpatch(); }

VoidResult CustomTickrate::patch() {
    auto& registry = ctx_.hookRegistry();

    // ── Frame time ────────────────────────────────────────────────────────────
    registry.registerSwapPatch(HookId::TickFrametime).patch(tick::FrameTime, &tick::kFrameTimeValue);

    // ── Rotation damping ────────────────────────────────────────────────────────
    registry.registerSwapPatch(HookId::TickRotDamp).patch(tick::RotationDamping, &tick::kRotationDampingValue);

    // ── Min rotation speed ──────────────────────────────────────────────────────
    registry.registerSwapPatch(HookId::TickMinRot).patch(tick::MinRotationSpeed, &tick::kMinRotationSpeedValue);

    // ── Max rotation speed ──────────────────────────────────────────────────────
    registry.registerSwapPatch(HookId::TickMaxRot).patch(tick::MaxRotationSpeed, &tick::kMaxRotationSpeedValue);

    // ── Rotation threshold ──────────────────────────────────────────────────────
    registry.registerSwapPatch(HookId::TickRotThresh).patch(tick::RotationThreshold, &tick::kRotationThresholdValue);

    // ── Delay requests (21 addresses) ─────────────────────────────────────────
    for (u32 i = 0; i < tick::kNumDelayRequests; ++i) {
        registry.registerSwapPatch(HookId::TickDelayReq_Base + i).patch(
            tick::DelayRequests[i], &tick::kDelayRequestValue);
    }

    // ── Delay animations (18 addresses) ───────────────────────────────────────
    for (u32 i = 0; i < tick::kNumDelayAnimations; ++i) {
        registry.registerSwapPatch(HookId::TickDelayAnim_Base + i).patch(
            tick::DelayAnimations[i], &tick::kDelayAnimationValue);
    }

    // ── Minimum distances (7 addresses) ───────────────────────────────────────
    for (u32 i = 0; i < tick::kNumMinDistances; ++i) {
        registry.registerSwapPatch(HookId::TickMinDist_Base + i).patch(
            tick::MinDistances[i], &tick::kMinDistanceValue);
    }

    // ── Rotation byte patches (3) ─────────────────────────────────────────────
    for (u32 i = 0; i < tick::kNumRotationBytePatches; ++i) {
        registry.registerPatchBytes(HookId::TickRotByte_Base + i).patch(
            tick::RotationByteAddresses[i],
            &tick::kRotationByteValue, sizeof(tick::kRotationByteValue));
    }

    patched_ = true;
    return VoidResult::ok();
}

void CustomTickrate::unpatch() {
    // SwapAddressPatch and PatchBytes have automatic restore via unregister
    patched_ = false;
}

} // namespace mg::modding
