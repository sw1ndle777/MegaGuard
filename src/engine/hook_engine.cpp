// =============================================================================
// DetourHook, SwapAddressPatch, PatchBytes - Implementation
// =============================================================================
#include "pch.hpp"
#include "engine/hook_engine.hpp"
#include "platform/memory.hpp"

// MinHook — stable dep, ok to include in this TU
#include <minhook/minhook.h>

namespace mg {

// ── DetourHook ─────────────────────────────────────────────────────────────────

DetourHook::~DetourHook() {
    remove();
}

DetourHook::DetourHook(DetourHook&& other) noexcept
    : isHooked_(other.isHooked_)
    , baseFn_(other.baseFn_)
    , detourFn_(other.detourFn_)
    , originalFn_(other.originalFn_)
{
    other.isHooked_ = false;
    other.baseFn_ = nullptr;
    other.detourFn_ = nullptr;
    other.originalFn_ = nullptr;
}

DetourHook& DetourHook::operator=(DetourHook&& other) noexcept {
    if (this != &other) {
        remove();
        isHooked_ = other.isHooked_;
        baseFn_ = other.baseFn_;
        detourFn_ = other.detourFn_;
        originalFn_ = other.originalFn_;
        other.isHooked_ = false;
        other.baseFn_ = nullptr;
        other.detourFn_ = nullptr;
        other.originalFn_ = nullptr;
    }
    return *this;
}

bool DetourHook::createInternal(void* target, void* detour) {
    if (!target || !detour)
    {
		MessageBoxA(nullptr, "Invalid target or detour function for hook creation.", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    baseFn_ = target;
    detourFn_ = detour;

    auto status = MH_CreateHook(baseFn_, detourFn_, &originalFn_);
    if (status != MH_OK)
    {
		MessageBoxA(nullptr, std::to_string(status).c_str(), "MH_CreateHook error", MB_OK | MB_ICONERROR);
        return false;
    }

    return enable();
}

bool DetourHook::enable() {
    if (!baseFn_ || isHooked_) return false;
    auto status = MH_QueueEnableHook(baseFn_);
    if (status != MH_OK)
    {
		MessageBoxA(nullptr, std::to_string(status).c_str(), "MH_QueueEnableHook error", MB_OK | MB_ICONERROR);
        return false;
    }
    isHooked_ = true;
    return true;
}

bool DetourHook::disable() {
    if (!isHooked_) return false;
    auto status = MH_DisableHook(baseFn_);
    if (status != MH_OK) return false;
    isHooked_ = false;
    return true;
}

bool DetourHook::remove() {
    if (!baseFn_) return true; // Nothing to remove
    disable();
    auto status = MH_RemoveHook(baseFn_);
    baseFn_ = nullptr;
    detourFn_ = nullptr;
    originalFn_ = nullptr;
    return status == MH_OK;
}

// ── SwapAddressPatch ───────────────────────────────────────────────────────────

SwapAddressPatch::~SwapAddressPatch() {
    unpatch();
}

bool SwapAddressPatch::patchInternal(uptr targetAddress, uptr newValueAddress, u32 startOffset) {
    startOffset_ = startOffset;
    targetAddress_ = reinterpret_cast<void*>(targetAddress);
    if (!targetAddress_) return false;

    DWORD oldProtect;
    if (!VirtualProtect(targetAddress_, startOffset + sizeof(uptr), PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    originalSize_ = sizeof(uptr);
    nocrtMemcpy(originalBytes_, targetAddress_, originalSize_);

    *reinterpret_cast<uptr*>(reinterpret_cast<uptr>(targetAddress_) + startOffset) = newValueAddress;

    VirtualProtect(targetAddress_, startOffset + sizeof(uptr), oldProtect, &oldProtect);
    return true;
}

bool SwapAddressPatch::unpatch() {
    if (!targetAddress_ || originalSize_ == 0) return false;

    DWORD oldProtect;
    if (!VirtualProtect(targetAddress_, startOffset_ + originalSize_, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    nocrtMemcpy(targetAddress_, originalBytes_, originalSize_);
    VirtualProtect(targetAddress_, startOffset_ + originalSize_, oldProtect, &oldProtect);

    targetAddress_ = nullptr;
    originalSize_ = 0;
    return true;
}

// ── PatchBytes ─────────────────────────────────────────────────────────────────

PatchBytes::~PatchBytes() {
    unpatch();
}

bool PatchBytes::patch(uptr targetAddress, const void* newBytes, std::size_t size, u32 startOffset) {
    if (!targetAddress || !newBytes || size == 0 || size > sizeof(originalBytes_)) return false;

    targetAddress_ = reinterpret_cast<void*>(targetAddress + startOffset);
    patchSize_ = size;

    DWORD oldProtect;
    if (!VirtualProtect(targetAddress_, size, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    nocrtMemcpy(originalBytes_, targetAddress_, size);
    nocrtMemcpy(targetAddress_, newBytes, size);

    VirtualProtect(targetAddress_, size, oldProtect, &oldProtect);
    return true;
}

bool PatchBytes::unpatch() {
    if (!targetAddress_ || patchSize_ == 0) return false;

    DWORD oldProtect;
    if (!VirtualProtect(targetAddress_, patchSize_, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    nocrtMemcpy(targetAddress_, originalBytes_, patchSize_);
    VirtualProtect(targetAddress_, patchSize_, oldProtect, &oldProtect);

    targetAddress_ = nullptr;
    patchSize_ = 0;
    return true;
}

} // namespace mg
