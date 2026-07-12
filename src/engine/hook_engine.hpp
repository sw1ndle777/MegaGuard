// =============================================================================
// DetourHook - MinHook wrapper with RAII
// =============================================================================
// Single-instance hook. Owns the hook lifecycle.
// No global state. Stored in HookRegistry which is owned by Context.
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg {

class DetourHook {
public:
    DetourHook() = default;
    ~DetourHook();

    DetourHook(const DetourHook&) = delete;
    DetourHook& operator=(const DetourHook&) = delete;
    DetourHook(DetourHook&& other) noexcept;
    DetourHook& operator=(DetourHook&& other) noexcept;

    // Create and enable a hook at the given address.
    template <typename Fn>
    bool create(uptr targetAddress, Fn detourFn) {
        return createInternal(
            reinterpret_cast<void*>(targetAddress),
            reinterpret_cast<void*>(detourFn));
    }

    // Get pointer to the original function (trampoline).
    template <typename Fn>
    Fn getOriginal() const { return static_cast<Fn>(originalFn_); }

    bool enable();
    bool disable();
    bool remove();
    bool isHooked() const { return isHooked_; }

private:
    bool createInternal(void* target, void* detour);

    bool isHooked_ = false;
    void* baseFn_ = nullptr;
    void* detourFn_ = nullptr;
    void* originalFn_ = nullptr;
};

// =============================================================================
// SwapAddressPatch - Replace a pointer/address at a target location
// =============================================================================
class SwapAddressPatch {
public:
    SwapAddressPatch() = default;
    ~SwapAddressPatch();

    SwapAddressPatch(const SwapAddressPatch&) = delete;
    SwapAddressPatch& operator=(const SwapAddressPatch&) = delete;

    template <typename T>
    bool patch(uptr targetAddress, T* newValue, u32 startOffset = 2) {
        return patchInternal(targetAddress, reinterpret_cast<uptr>(newValue), startOffset);
    }

    bool unpatch();
    bool isPatched() const { return targetAddress_ != nullptr; }

private:
    bool patchInternal(uptr targetAddress, uptr newValueAddress, u32 startOffset);

    void* targetAddress_ = nullptr;
    u32 startOffset_ = 2;
    u8 originalBytes_[8]{};
    std::size_t originalSize_ = 0;
};

// =============================================================================
// PatchBytes - Raw byte patching with backup/restore
// =============================================================================
class PatchBytes {
public:
    PatchBytes() = default;
    ~PatchBytes();

    PatchBytes(const PatchBytes&) = delete;
    PatchBytes& operator=(const PatchBytes&) = delete;

    bool patch(uptr targetAddress, const void* newBytes, std::size_t size, u32 startOffset = 0);
    bool unpatch();
    bool isPatched() const { return targetAddress_ != nullptr; }

private:
    void* targetAddress_ = nullptr;
    u8 originalBytes_[256]{};
    std::size_t patchSize_ = 0;
};

} // namespace mg
