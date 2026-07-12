#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class MegaGuardContext;
class DetourHook;

// =============================================================================
// NtApiMonitor — Hooks critical NT APIs for real-time cheat detection
// =============================================================================
// Modeled after Hyperion's approach: hook the NT-layer functions that attackers
// must call to inject, remap, or manipulate memory/threads.
//
// Hooked APIs and what they catch:
//   NtMapViewOfSection     — dual-map bypass, section injection
//   NtCreateSection        — section object creation for remapping
//   NtAllocateVirtualMemory — RWX allocations, shellcode staging
//   NtProtectVirtualMemory — flipping pages to executable post-write
//   NtCreateThreadEx       — remote/anonymous thread creation
//   NtWriteVirtualMemory   — cross-process or self-write
//   NtResumeThread         — thread hijacking / injection completion
//   NtSuspendThread        — thread freezing for context manipulation
//   NtSetContextThread     — EIP/RIP hijacking
//
// The monitor does NOT block calls — it logs + sets detection flags.
// Blocking is left to a policy layer so legitimate game code isn't broken.
// =============================================================================
class NtApiMonitor final : public IScanner {
public:
    explicit NtApiMonitor(MegaGuardContext& ctx);
    ~NtApiMonitor();

    const char* name() const override { return "NtApiMonitor"; }
    u32 intervalSeconds() const override { return MG_INT(5); }
    void init() override;
    void scan(std::atomic<u32>& flags, Logger& log) override;

    // Called from hook trampolines — must be fast, no allocations
    static void onMapViewOfSection(HANDLE section, HANDLE process,
                                   PVOID* baseAddr, ULONG protect);
    static void onCreateSection(PHANDLE sectionHandle, ACCESS_MASK access,
                                ULONG protect, ULONG allocationAttributes);
    static void onAllocateVirtualMemory(HANDLE process, PVOID* baseAddr,
                                        SIZE_T* regionSize, ULONG protect);
    static void onProtectVirtualMemory(HANDLE process, PVOID* baseAddr,
                                       SIZE_T* regionSize, ULONG newProtect);
    static void onCreateThreadEx(PHANDLE threadHandle, PVOID startAddress);
    static void onWriteVirtualMemory(HANDLE process, PVOID baseAddr, SIZE_T size);
    static void onResumeThread(HANDLE thread);
    static void onSuspendThread(HANDLE thread);
    static void onSetContextThread(HANDLE thread);

private:
    bool installHooks();
    void removeHooks();

    MegaGuardContext& ctx_;

    std::unique_ptr<DetourHook> hookMapView_;
    std::unique_ptr<DetourHook> hookCreateSection_;
    std::unique_ptr<DetourHook> hookAllocVM_;
    std::unique_ptr<DetourHook> hookProtectVM_;
    std::unique_ptr<DetourHook> hookCreateThread_;
    std::unique_ptr<DetourHook> hookWriteVM_;
    std::unique_ptr<DetourHook> hookResumeThread_;
    std::unique_ptr<DetourHook> hookSuspendThread_;
    std::unique_ptr<DetourHook> hookSetContext_;

    // Pending events — accumulated between scan() calls
    struct PendingEvent {
        DetectionFlag flag;
        u32 extra;
        char detail[256];
    };
    static constexpr u32 kMaxPendingEvents = 64;

    static inline volatile LONG              s_eventLock = 0;
    static inline PendingEvent               s_events[kMaxPendingEvents]{};
    static inline volatile LONG              s_eventCount = 0;
    static inline NtApiMonitor*              s_instance = nullptr;

    static void pushEvent(DetectionFlag flag, u32 extra, const char* fmt, ...);
};

} // namespace mg
