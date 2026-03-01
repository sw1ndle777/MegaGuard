// =============================================================================
// HookRegistry - Implementation
// =============================================================================
#include "pch.hpp"
#include "engine/hook_registry.hpp"
#include <minhook/minhook.h>

namespace mg {

HookRegistry::HookRegistry() = default;

HookRegistry::~HookRegistry() {
    removeAll();
    uninitializeMinHook();
}

VoidResult HookRegistry::initializeMinHook() {
    if (minHookInitialized_) return VoidResult::ok();
    auto status = MH_Initialize();
    if (status != MH_OK) return VoidResult::err(ErrorCode::kInitFailed);
    minHookInitialized_ = true;
    return VoidResult::ok();
}

void HookRegistry::uninitializeMinHook() {
    if (!minHookInitialized_) return;
    MH_Uninitialize();
    minHookInitialized_ = false;
}

DetourHook& HookRegistry::registerDetour(u32 id) {
    auto [it, inserted] = detours_.emplace(id, std::make_unique<DetourHook>());
    return *it->second;
}

SwapAddressPatch& HookRegistry::registerSwapPatch(u32 id) {
    auto [it, inserted] = swapPatches_.emplace(id, std::make_unique<SwapAddressPatch>());
    return *it->second;
}

PatchBytes& HookRegistry::registerPatchBytes(u32 id) {
    auto [it, inserted] = patchBytes_.emplace(id, std::make_unique<PatchBytes>());
    return *it->second;
}

DetourHook* HookRegistry::findDetour(u32 id) {
    auto it = detours_.find(id);
    return (it != detours_.end()) ? it->second.get() : nullptr;
}

SwapAddressPatch* HookRegistry::findSwapPatch(u32 id) {
    auto it = swapPatches_.find(id);
    return (it != swapPatches_.end()) ? it->second.get() : nullptr;
}

PatchBytes* HookRegistry::findPatchBytes(u32 id) {
    auto it = patchBytes_.find(id);
    return (it != patchBytes_.end()) ? it->second.get() : nullptr;
}

void HookRegistry::removeAll() {
    // Remove in reverse order of typical creation
    for (auto& [id, patch] : patchBytes_) {
        patch->unpatch();
    }
    patchBytes_.clear();

    for (auto& [id, patch] : swapPatches_) {
        patch->unpatch();
    }
    swapPatches_.clear();

    for (auto& [id, hook] : detours_) {
        hook->remove();
    }
    detours_.clear();
}

} // namespace mg
