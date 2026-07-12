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
#include "utils/logger.hpp"

namespace mg::modding {

using namespace mg::game;

namespace tick = addr::features::tickrate;

CustomTickrate::CustomTickrate(MegaGuardContext& ctx) : ctx_(ctx) {}
CustomTickrate::~CustomTickrate() { if (patched_) unpatch(); }

VoidResult CustomTickrate::patch() {
    auto& registry = ctx_.hookRegistry();

    // ── Frame time (ALL 3 sites — old patched Frametime1/3/4) ──────────────────
    for (u32 i = 0; i < tick::kNumFrameTimes; ++i)
        registry.registerSwapPatch(HookId::TickFrametime_Base + i).patch(
            tick::FrameTimes[i], &tick::kFrameTimeValue);

    // ── Rotation damping (ALL 3 sites; default 0.1 -> 1.0) ─────────────────────
    // The refactor only patched RotDamp1, leaving 2 of 3 sites under-damped at the
    // vanilla 0.1 — a prime cause of the residual rotation snap.
    for (u32 i = 0; i < tick::kNumRotationDampings; ++i)
        registry.registerSwapPatch(HookId::TickRotDamp_Base + i).patch(
            tick::RotationDampings[i], &tick::kRotationDampingValue);

    // ── Min rotation speed (ALL 6 sites) ───────────────────────────────────────
    for (u32 i = 0; i < tick::kNumMinRotationSpeeds; ++i)
        registry.registerSwapPatch(HookId::TickMinRot_Base + i).patch(
            tick::MinRotationSpeeds[i], &tick::kMinRotationSpeedValue);

    // ── Max rotation speed (ALL 6 sites) ───────────────────────────────────────
    for (u32 i = 0; i < tick::kNumMaxRotationSpeeds; ++i)
        registry.registerSwapPatch(HookId::TickMaxRot_Base + i).patch(
            tick::MaxRotationSpeeds[i], &tick::kMaxRotationSpeedValue);

    // ── Rotation threshold (ALL 6 sites) + limit ───────────────────────────────
    for (u32 i = 0; i < tick::kNumRotationThresholds; ++i)
        registry.registerSwapPatch(HookId::TickRotThresh_Base + i).patch(
            tick::RotationThresholds[i], &tick::kRotationThresholdValue);
    registry.registerSwapPatch(HookId::TickRotThreshLimit).patch(
        tick::RotThresholdLimit, &tick::kRotationThresholdValue);

    // ── Delay requests (21 addresses) ─────────────────────────────────────────
    // These rewrite the movement-send interval constant (vanilla 0.08 = ~12.5/s send)
    // to 1/128. If the logged count below is not 21/21 at runtime, the 128-tick send is
    // NOT live (stale DLL / wrong client build) and movement + rifle full-auto will run
    // at vanilla ~10-tick — which is exactly the "10 tickrate" symptom.
    u32 reqApplied = 0;
    for (u32 i = 0; i < tick::kNumDelayRequests; ++i) {
        if (registry.registerSwapPatch(HookId::TickDelayReq_Base + i).patch(
                tick::DelayRequests[i], &tick::kDelayRequestValue))
            ++reqApplied;
    }
    // ctx_.logger().info("[CustomTickrate] 128-tick send patches applied: {}/{}",
    //     reqApplied, tick::kNumDelayRequests);
    (void)reqApplied;

    // ── Delay animations (18 addresses) ───────────────────────────────────────
    for (u32 i = 0; i < tick::kNumDelayAnimations; ++i) {
        registry.registerSwapPatch(HookId::TickDelayAnim_Base + i).patch(
            tick::DelayAnimations[i], &tick::kDelayAnimationValue);
    }

    // ── Minimum distances (7 addresses) ───────────────────────────────────────
    // Old code split these: MinDistance1 = 10.0 (minimum_distance), MinDistance2..7 =
    // 0.0099 (minimum_distance2 — a near-zero gate). The refactor forced all 7 to 10.0,
    // which gates out small position deltas -> stutter/snap on fine movement.
    for (u32 i = 0; i < tick::kNumMinDistances; ++i) {
        const double* value = (i == 0) ? &tick::kMinDistanceValue : &tick::kMinDistanceValue2;
        registry.registerSwapPatch(HookId::TickMinDist_Base + i).patch(
            tick::MinDistances[i], value);
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