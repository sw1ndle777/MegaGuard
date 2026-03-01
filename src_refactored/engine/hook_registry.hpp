// =============================================================================
// HookRegistry - Central ownership of all hooks
// =============================================================================
// All DetourHook / SwapAddressPatch / PatchBytes instances live here.
// Provides named registration and bulk remove-all for shutdown.
// =============================================================================
#pragma once

#include "core/types.hpp"
#include "engine/hook_engine.hpp"

namespace mg {

class HookRegistry {
public:
    HookRegistry();
    ~HookRegistry();

    HookRegistry(const HookRegistry&) = delete;
    HookRegistry& operator=(const HookRegistry&) = delete;

    // ── MinHook lifecycle ──────────────────────────────────────────────────
    VoidResult initializeMinHook();
    void uninitializeMinHook();

    // ── Registration (keyed by compile-time u32 from HookId) ──────────────
    DetourHook& registerDetour(u32 id);
    SwapAddressPatch& registerSwapPatch(u32 id);
    PatchBytes& registerPatchBytes(u32 id);

    // ── Lookup ─────────────────────────────────────────────────────────────
    DetourHook* findDetour(u32 id);
    SwapAddressPatch* findSwapPatch(u32 id);
    PatchBytes* findPatchBytes(u32 id);

    // ── Bulk operations ────────────────────────────────────────────────────
    void removeAll();

private:
    boost::unordered_flat_map<u32, std::unique_ptr<DetourHook>> detours_;
    boost::unordered_flat_map<u32, std::unique_ptr<SwapAddressPatch>> swapPatches_;
    boost::unordered_flat_map<u32, std::unique_ptr<PatchBytes>> patchBytes_;
    bool minHookInitialized_ = false;
};

} // namespace mg
