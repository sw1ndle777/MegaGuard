#include "pch.hpp"
#include "anticheat/scanners/ntapi_monitor_scanner.hpp"
#include "core/context.hpp"
#include "engine/hook_engine.hpp"

namespace mg {

// =============================================================================
// NT API typedefs for hooked functions
// =============================================================================
using NtMapViewOfSection_t = NTSTATUS(NTAPI*)(
    HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T, PLARGE_INTEGER, PSIZE_T,
    ULONG, ULONG, ULONG);

using NtCreateSection_t = NTSTATUS(NTAPI*)(
    PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PLARGE_INTEGER, ULONG, ULONG, HANDLE);

using NtAllocateVirtualMemory_t = NTSTATUS(NTAPI*)(
    HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);

using NtProtectVirtualMemory_t = NTSTATUS(NTAPI*)(
    HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);

using NtCreateThreadEx_t = NTSTATUS(NTAPI*)(
    PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, HANDLE, PVOID, PVOID,
    ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID);

using NtWriteVirtualMemory_t = NTSTATUS(NTAPI*)(
    HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);

using NtResumeThread_t = NTSTATUS(NTAPI*)(HANDLE, PULONG);
using NtSuspendThread_t = NTSTATUS(NTAPI*)(HANDLE, PULONG);
using NtSetContextThread_t = NTSTATUS(NTAPI*)(HANDLE, PCONTEXT);

// =============================================================================
// Original function pointers (set by DetourHook::getOriginal)
// =============================================================================
static NtMapViewOfSection_t       g_OrigMapView      = nullptr;
static NtCreateSection_t          g_OrigCreateSection = nullptr;
static NtAllocateVirtualMemory_t  g_OrigAllocVM       = nullptr;
static NtProtectVirtualMemory_t   g_OrigProtectVM     = nullptr;
static NtCreateThreadEx_t         g_OrigCreateThread   = nullptr;
static NtWriteVirtualMemory_t     g_OrigWriteVM       = nullptr;
static NtResumeThread_t           g_OrigResumeThread   = nullptr;
static NtSuspendThread_t          g_OrigSuspendThread  = nullptr;
static NtSetContextThread_t       g_OrigSetContext     = nullptr;

// =============================================================================
// Spin lock helpers (same as guard_regions.cpp)
// =============================================================================
static inline void spinLock(volatile LONG* lock) {
    while (InterlockedCompareExchange(lock, 1, 0) != 0)
        YieldProcessor();
}
static inline void spinUnlock(volatile LONG* lock) {
    InterlockedExchange(lock, 0);
}

// =============================================================================
// Event push — called from hot-path hook trampolines
// =============================================================================
void NtApiMonitor::pushEvent(DetectionFlag flag, u32 extra, const char* fmt, ...) {
    LONG idx = InterlockedIncrement(&s_eventCount) - 1;
    if (idx >= static_cast<LONG>(kMaxPendingEvents)) {
        InterlockedDecrement(&s_eventCount);
        return; // ring buffer full, drop
    }
    spinLock(&s_eventLock);
    s_events[idx].flag = flag;
    s_events[idx].extra = extra;
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_events[idx].detail, sizeof(s_events[idx].detail), fmt, args);
    va_end(args);
    spinUnlock(&s_eventLock);
}

// =============================================================================
// Hook trampolines
// =============================================================================

static NTSTATUS NTAPI HookedNtMapViewOfSection(
    HANDLE SectionHandle, HANDLE ProcessHandle, PVOID* BaseAddress,
    ULONG_PTR ZeroBits, SIZE_T CommitSize, PLARGE_INTEGER SectionOffset,
    PSIZE_T ViewSize, ULONG InheritDisposition, ULONG AllocationType, ULONG Win32Protect)
{
    NTSTATUS status = g_OrigMapView(SectionHandle, ProcessHandle, BaseAddress,
        ZeroBits, CommitSize, SectionOffset, ViewSize, InheritDisposition,
        AllocationType, Win32Protect);

    if (NT_SUCCESS(status)) {
        NtApiMonitor::onMapViewOfSection(SectionHandle, ProcessHandle,
                                         BaseAddress, Win32Protect);
    }
    return status;
}

static NTSTATUS NTAPI HookedNtCreateSection(
    PHANDLE SectionHandle, ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes, PLARGE_INTEGER MaximumSize,
    ULONG SectionPageProtection, ULONG AllocationAttributes, HANDLE FileHandle)
{
    NTSTATUS status = g_OrigCreateSection(SectionHandle, DesiredAccess,
        ObjectAttributes, MaximumSize, SectionPageProtection,
        AllocationAttributes, FileHandle);

    if (NT_SUCCESS(status)) {
        NtApiMonitor::onCreateSection(SectionHandle, DesiredAccess,
                                       SectionPageProtection, AllocationAttributes);
    }
    return status;
}

static NTSTATUS NTAPI HookedNtAllocateVirtualMemory(
    HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits,
    PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect)
{
    NTSTATUS status = g_OrigAllocVM(ProcessHandle, BaseAddress, ZeroBits,
        RegionSize, AllocationType, Protect);

    if (NT_SUCCESS(status)) {
        NtApiMonitor::onAllocateVirtualMemory(ProcessHandle, BaseAddress,
                                               RegionSize, Protect);
    }
    return status;
}

static NTSTATUS NTAPI HookedNtProtectVirtualMemory(
    HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize,
    ULONG NewProtect, PULONG OldProtect)
{
    NTSTATUS status = g_OrigProtectVM(ProcessHandle, BaseAddress, RegionSize,
        NewProtect, OldProtect);

    if (NT_SUCCESS(status)) {
        NtApiMonitor::onProtectVirtualMemory(ProcessHandle, BaseAddress,
                                              RegionSize, NewProtect);
    }
    return status;
}

static NTSTATUS NTAPI HookedNtCreateThreadEx(
    PHANDLE ThreadHandle, ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes, HANDLE ProcessHandle,
    PVOID StartRoutine, PVOID Argument, ULONG CreateFlags,
    SIZE_T ZeroBits, SIZE_T StackSize, SIZE_T MaxStackSize, PVOID AttributeList)
{
    NTSTATUS status = g_OrigCreateThread(ThreadHandle, DesiredAccess,
        ObjectAttributes, ProcessHandle, StartRoutine, Argument, CreateFlags,
        ZeroBits, StackSize, MaxStackSize, AttributeList);

    if (NT_SUCCESS(status)) {
        NtApiMonitor::onCreateThreadEx(ThreadHandle, StartRoutine);
    }
    return status;
}

static NTSTATUS NTAPI HookedNtWriteVirtualMemory(
    HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer,
    SIZE_T NumberOfBytesToWrite, PSIZE_T NumberOfBytesWritten)
{
    NTSTATUS status = g_OrigWriteVM(ProcessHandle, BaseAddress, Buffer,
        NumberOfBytesToWrite, NumberOfBytesWritten);

    if (NT_SUCCESS(status)) {
        NtApiMonitor::onWriteVirtualMemory(ProcessHandle, BaseAddress,
                                            NumberOfBytesToWrite);
    }
    return status;
}

static NTSTATUS NTAPI HookedNtResumeThread(HANDLE ThreadHandle, PULONG PreviousSuspendCount) {
    NTSTATUS status = g_OrigResumeThread(ThreadHandle, PreviousSuspendCount);
    if (NT_SUCCESS(status))
        NtApiMonitor::onResumeThread(ThreadHandle);
    return status;
}

static NTSTATUS NTAPI HookedNtSuspendThread(HANDLE ThreadHandle, PULONG PreviousSuspendCount) {
    NTSTATUS status = g_OrigSuspendThread(ThreadHandle, PreviousSuspendCount);
    if (NT_SUCCESS(status))
        NtApiMonitor::onSuspendThread(ThreadHandle);
    return status;
}

static NTSTATUS NTAPI HookedNtSetContextThread(HANDLE ThreadHandle, PCONTEXT Context) {
    NtApiMonitor::onSetContextThread(ThreadHandle);
    return g_OrigSetContext(ThreadHandle, Context);
}

// =============================================================================
// Callback implementations — fast classification, no allocations
// =============================================================================
void NtApiMonitor::onMapViewOfSection(HANDLE section, HANDLE process,
                                       PVOID* baseAddr, ULONG protect)
{
    // Only care about mappings into our own process
    HANDLE self = GetCurrentProcess();
    if (process != self && GetProcessId(process) != GetCurrentProcessId())
        return;

    uptr addr = baseAddr ? reinterpret_cast<uptr>(*baseAddr) : 0;

    // Flag R/W or RWX mapped views (potential dual-map for guard bypass)
    if (protect == PAGE_READWRITE || protect == PAGE_EXECUTE_READWRITE) {
        pushEvent(DetectionFlag::kMappedImage, static_cast<u32>(addr),
            "NtMapViewOfSection R/W at 0x%08X protect=0x%X", addr, protect);
    }

    // Flag executable mappings (code injection)
    if (protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                   PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
    {
        if (!isInsideAcModule(addr)) {
            pushEvent(DetectionFlag::kDllInjection, static_cast<u32>(addr),
                "NtMapViewOfSection EXEC at 0x%08X protect=0x%X", addr, protect);
        }
    }
}

void NtApiMonitor::onCreateSection(PHANDLE sectionHandle, ACCESS_MASK access,
                                    ULONG protect, ULONG allocationAttributes)
{
    // Pagefile-backed executable sections are suspicious
    bool isCommit = (allocationAttributes & SEC_COMMIT) != 0;
    bool isExec = (protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                              PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;

    if (isCommit && isExec) {
        pushEvent(DetectionFlag::kManualMap, protect,
            "NtCreateSection SEC_COMMIT+EXEC protect=0x%X attr=0x%X",
            protect, allocationAttributes);
    }
}

void NtApiMonitor::onAllocateVirtualMemory(HANDLE process, PVOID* baseAddr,
                                            SIZE_T* regionSize, ULONG protect)
{
    // RWX allocations are almost always shellcode staging
    if (protect == PAGE_EXECUTE_READWRITE) {
        uptr addr = baseAddr ? reinterpret_cast<uptr>(*baseAddr) : 0;
        SIZE_T size = regionSize ? *regionSize : 0;
        if (!isInsideAcModule(addr)) {
            pushEvent(DetectionFlag::kManualMap, static_cast<u32>(addr),
                "NtAllocateVirtualMemory RWX at 0x%08X size=0x%X", addr, size);
        }
    }
}

void NtApiMonitor::onProtectVirtualMemory(HANDLE process, PVOID* baseAddr,
                                           SIZE_T* regionSize, ULONG newProtect)
{
    // Watch for W->X transitions (write then execute pattern)
    if (newProtect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                      PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
    {
        uptr addr = baseAddr ? reinterpret_cast<uptr>(*baseAddr) : 0;
        if (!isInsideAcModule(addr)) {
            // Only flag if it's not our own AC module or known game regions
            HANDLE self = GetCurrentProcess();
            if (process == self || GetProcessId(process) == GetCurrentProcessId()) {
                pushEvent(DetectionFlag::kInlineHook, static_cast<u32>(addr),
                    "NtProtectVirtualMemory ->EXEC at 0x%08X prot=0x%X", addr, newProtect);
            }
        }
    }
}

void NtApiMonitor::onCreateThreadEx(PHANDLE threadHandle, PVOID startAddress) {
    uptr startAddr = reinterpret_cast<uptr>(startAddress);
    auto modules = getLoadedModules();

    if (!isAddressInModules(modules, startAddr) && !isInsideAcModule(startAddr)) {
        pushEvent(DetectionFlag::kAnonymousThread, static_cast<u32>(startAddr),
            "NtCreateThreadEx start=0x%08X (not in any module)", startAddr);
    }
}

void NtApiMonitor::onWriteVirtualMemory(HANDLE process, PVOID baseAddr, SIZE_T size) {
    // Self-writes to executable regions are suspicious
    HANDLE self = GetCurrentProcess();
    if (process == self || GetProcessId(process) == GetCurrentProcessId()) {
        uptr addr = reinterpret_cast<uptr>(baseAddr);
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(baseAddr, &mbi, sizeof(mbi)) == sizeof(mbi)) {
            if (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                               PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
            {
                if (!isInsideAcModule(addr)) {
                    pushEvent(DetectionFlag::kIntegrityViolation, static_cast<u32>(addr),
                        "NtWriteVirtualMemory to EXEC region 0x%08X size=0x%X",
                        addr, size);
                }
            }
        }
    }
}

void NtApiMonitor::onResumeThread(HANDLE thread) {
    DWORD tid = GetThreadId(thread);
    if (tid != 0 && tid != GetCurrentThreadId()) {
        pushEvent(DetectionFlag::kAnonymousThread, tid,
            "NtResumeThread TID=%u", tid);
    }
}

void NtApiMonitor::onSuspendThread(HANDLE thread) {
    DWORD tid = GetThreadId(thread);
    if (tid != 0 && tid != GetCurrentThreadId()) {
        pushEvent(DetectionFlag::kAnonymousThread, tid,
            "NtSuspendThread TID=%u", tid);
    }
}

void NtApiMonitor::onSetContextThread(HANDLE thread) {
    DWORD tid = GetThreadId(thread);
    pushEvent(DetectionFlag::kAnonymousThread, tid,
        "NtSetContextThread TID=%u (EIP hijack?)", tid);
}

// =============================================================================
// NtApiMonitor lifecycle
// =============================================================================
NtApiMonitor::NtApiMonitor(MegaGuardContext& ctx) : ctx_(ctx) {}

NtApiMonitor::~NtApiMonitor() {
    removeHooks();
    if (s_instance == this) s_instance = nullptr;
}

void NtApiMonitor::init() {
    s_instance = this;
    installHooks();
}

bool NtApiMonitor::installHooks() {
    HMODULE ntdll = GetModuleHandleA(MG_STR("ntdll.dll"));
    if (!ntdll) return false;

    auto resolve = [&](const char* name) -> uptr {
        auto* p = GetProcAddress(ntdll, name);
        return p ? reinterpret_cast<uptr>(p) : 0;
    };

    auto tryHook = [&](std::unique_ptr<DetourHook>& hook, const char* name, auto detourFn) -> bool {
        uptr addr = resolve(name);
        if (!addr) return false;
        hook = std::make_unique<DetourHook>();
        if (!hook->create(addr, detourFn)) {
            hook.reset();
            return false;
        }
        return true;
    };

    bool ok = true;

    if (tryHook(hookMapView_, MG_STR("NtMapViewOfSection"), HookedNtMapViewOfSection))
        g_OrigMapView = hookMapView_->getOriginal<NtMapViewOfSection_t>();
    else ok = false;

    if (tryHook(hookCreateSection_, MG_STR("NtCreateSection"), HookedNtCreateSection))
        g_OrigCreateSection = hookCreateSection_->getOriginal<NtCreateSection_t>();
    else ok = false;

    if (tryHook(hookAllocVM_, MG_STR("ZwAllocateVirtualMemory"), HookedNtAllocateVirtualMemory))
        g_OrigAllocVM = hookAllocVM_->getOriginal<NtAllocateVirtualMemory_t>();
    else ok = false;

    if (tryHook(hookProtectVM_, MG_STR("NtProtectVirtualMemory"), HookedNtProtectVirtualMemory))
        g_OrigProtectVM = hookProtectVM_->getOriginal<NtProtectVirtualMemory_t>();
    else ok = false;

    if (tryHook(hookCreateThread_, MG_STR("NtCreateThreadEx"), HookedNtCreateThreadEx))
        g_OrigCreateThread = hookCreateThread_->getOriginal<NtCreateThreadEx_t>();

    if (tryHook(hookWriteVM_, MG_STR("NtWriteVirtualMemory"), HookedNtWriteVirtualMemory))
        g_OrigWriteVM = hookWriteVM_->getOriginal<NtWriteVirtualMemory_t>();

    if (tryHook(hookResumeThread_, MG_STR("NtResumeThread"), HookedNtResumeThread))
        g_OrigResumeThread = hookResumeThread_->getOriginal<NtResumeThread_t>();

    if (tryHook(hookSuspendThread_, MG_STR("NtSuspendThread"), HookedNtSuspendThread))
        g_OrigSuspendThread = hookSuspendThread_->getOriginal<NtSuspendThread_t>();

    if (tryHook(hookSetContext_, MG_STR("NtSetContextThread"), HookedNtSetContextThread))
        g_OrigSetContext = hookSetContext_->getOriginal<NtSetContextThread_t>();

    return ok;
}

void NtApiMonitor::removeHooks() {
    auto unhook = [](std::unique_ptr<DetourHook>& h) {
        if (h) { h->remove(); h.reset(); }
    };
    unhook(hookMapView_);
    unhook(hookCreateSection_);
    unhook(hookAllocVM_);
    unhook(hookProtectVM_);
    unhook(hookCreateThread_);
    unhook(hookWriteVM_);
    unhook(hookResumeThread_);
    unhook(hookSuspendThread_);
    unhook(hookSetContext_);

    g_OrigMapView = nullptr;
    g_OrigCreateSection = nullptr;
    g_OrigAllocVM = nullptr;
    g_OrigProtectVM = nullptr;
    g_OrigCreateThread = nullptr;
    g_OrigWriteVM = nullptr;
    g_OrigResumeThread = nullptr;
    g_OrigSuspendThread = nullptr;
    g_OrigSetContext = nullptr;
}

void NtApiMonitor::scan(std::atomic<u32>& flags, Logger& log) {
    // Drain pending events from hook callbacks and report them
    LONG count = InterlockedExchange(&s_eventCount, 0);
    if (count <= 0) return;
    if (count > static_cast<LONG>(kMaxPendingEvents))
        count = static_cast<LONG>(kMaxPendingEvents);

    spinLock(&s_eventLock);
    for (LONG i = 0; i < count; ++i) {
        reportFlag(flags, log, s_events[i].flag, MG_STR("NtApiMonitor"),
            std::string(s_events[i].detail), s_events[i].extra);
    }
    spinUnlock(&s_eventLock);
}

} // namespace mg
