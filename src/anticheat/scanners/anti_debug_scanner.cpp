#include "pch.hpp"
#include "anticheat/scanners/anti_debug_scanner.hpp"

namespace mg {

namespace {

constexpr ULONG kProcessDebugPortClass = 0x7;
constexpr ULONG kProcessDebugObjectHandleClass = 0x1E;
constexpr ULONG kProcessDebugFlagsClass = 0x1F;
constexpr ULONG kSystemKernelDebuggerInformationClass = 35;

#ifdef _M_IX86
constexpr SIZE_T kPebNtGlobalFlagOffset = 0x68;
constexpr SIZE_T kPebProcessHeapOffset = 0x18;
constexpr SIZE_T kHeapFlagsOffset = 0x40;
constexpr SIZE_T kHeapForceFlagsOffset = 0x44;
#else
constexpr SIZE_T kPebNtGlobalFlagOffset = 0xBC;
constexpr SIZE_T kPebProcessHeapOffset = 0x30;
constexpr SIZE_T kHeapFlagsOffset = 0x70;
constexpr SIZE_T kHeapForceFlagsOffset = 0x74;
#endif

constexpr uptr kUserSharedDataAddress = static_cast<uptr>(0x7FFE0000ull);

struct SystemKernelDebuggerInformation {
    BOOLEAN debuggerEnabled;
    BOOLEAN debuggerNotPresent;
};

struct UserSharedDataDebuggerView {
    u8 reserved[0x2D4];
    BOOLEAN kdDebuggerEnabled;
};

PPEB currentPeb() {
#ifdef _M_IX86
    return reinterpret_cast<PPEB>(__readfsdword(0x30));
#else
    return reinterpret_cast<PPEB>(__readgsqword(0x60));
#endif
}

bool hasNtGlobalFlag(PPEB peb, u32& ntGlobalFlag) {
    if (!peb)
        return false;

    ntGlobalFlag = *reinterpret_cast<const ULONG*>(reinterpret_cast<const u8*>(peb) + kPebNtGlobalFlagOffset);
    return (ntGlobalFlag & 0x70u) != 0;
}

bool hasDebugHeapFlags(PPEB peb, u32& heapFlags, u32& heapForceFlags) {
    if (!peb)
        return false;

    auto processHeap = *reinterpret_cast<PVOID const*>(reinterpret_cast<const u8*>(peb) + kPebProcessHeapOffset);
    if (!processHeap)
        return false;

    auto heapBytes = reinterpret_cast<const u8*>(processHeap);
    heapFlags = *reinterpret_cast<const ULONG*>(heapBytes + kHeapFlagsOffset);
    heapForceFlags = *reinterpret_cast<const ULONG*>(heapBytes + kHeapForceFlagsOffset);

    return ((heapFlags & ~HEAP_GROWABLE) != 0) || (heapForceFlags != 0);
}

bool hasVehDebuggerModule() {
#ifdef _M_X64
    HMODULE vehModule = GetModuleHandleA(MG_STR("vehdebug-x86_64.dll"));
#else
    HMODULE vehModule = GetModuleHandleA(MG_STR("vehdebug-i386.dll"));
    if (!vehModule)
        vehModule = GetModuleHandleA(MG_STR("vehdebug.dll"));
#endif

    if (!vehModule)
        return false;

    return GetProcAddress(vehModule, MG_STR("InitializeVEH")) != nullptr;
}

bool hasKernelDebugger(NtApi& api) {
    if (!api.NtQuerySystemInformation)
        return false;

    SystemKernelDebuggerInformation info{};
    auto status = api.NtQuerySystemInformation(
        kSystemKernelDebuggerInformationClass,
        &info,
        static_cast<ULONG>(sizeof(info)),
        nullptr);

    return NT_SUCCESS(status) && (info.debuggerEnabled || !info.debuggerNotPresent);
}

bool hasSharedKernelDebuggerFlag() {
    auto sharedData = reinterpret_cast<const UserSharedDataDebuggerView*>(kUserSharedDataAddress);
    return sharedData != nullptr && sharedData->kdDebuggerEnabled != FALSE;
}

bool hasHardwareBreakpoints() {
    THREADENTRY32 threadEntry{};
    threadEntry.dwSize = sizeof(threadEntry);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    const DWORD currentProcessId = GetCurrentProcessId();
    const DWORD currentThreadId = GetCurrentThreadId();
    bool found = false;

    if (Thread32First(snapshot, &threadEntry)) {
        do {
            if (threadEntry.th32OwnerProcessID != currentProcessId || threadEntry.th32ThreadID == currentThreadId)
                continue;

            HANDLE threadHandle = OpenThread(
                THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
                FALSE,
                threadEntry.th32ThreadID);
            if (!threadHandle)
                continue;

            const DWORD suspendCount = SuspendThread(threadHandle);
            if (suspendCount == static_cast<DWORD>(-1)) {
                CloseHandle(threadHandle);
                continue;
            }

            CONTEXT context{};
            context.ContextFlags = CONTEXT_DEBUG_REGISTERS;

            if (GetThreadContext(threadHandle, &context)) {
                if (context.Dr0 || context.Dr1 || context.Dr2 || context.Dr3 || context.Dr6 || context.Dr7)
                    found = true;
            }

            ResumeThread(threadHandle);
            CloseHandle(threadHandle);

            if (found)
                break;
        } while (Thread32Next(snapshot, &threadEntry));
    }

    CloseHandle(snapshot);
    return found;
}

} // namespace

void AntiDebugScanner::scan(std::atomic<u32>& flags, Logger& log) {
    if (IsDebuggerPresent())
        reportFlag(flags, log, DetectionFlag::kDebuggerPresent, MG_STR("AntiDebug"), MG_STR("IsDebuggerPresent"), 1);

    BOOL remoteDebugger = FALSE;
    if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &remoteDebugger) && remoteDebugger)
        reportFlag(flags, log, DetectionFlag::kDebuggerPresent, MG_STR("AntiDebug"), MG_STR("CheckRemoteDebuggerPresent"), 1);

    /*
#if !MG_PROFILE_DEV
    __try {
        CloseHandle(reinterpret_cast<HANDLE>(1));
    }
    __except (GetExceptionCode() == EXCEPTION_INVALID_HANDLE ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        reportFlag(flags, log, DetectionFlag::kDebuggerPresent, MG_STR("AntiDebug"), MG_STR("CloseHandle invalid-handle exception"), 1);
    }
#endif
    */

    auto* peb = currentPeb();
    if (peb && peb->BeingDebugged)
        reportFlag(flags, log, DetectionFlag::kPebDebugFlag, MG_STR("AntiDebug"), MG_STR("PEB.BeingDebugged"), 1);

    u32 ntGlobalFlag = 0;
    if (hasNtGlobalFlag(peb, ntGlobalFlag))
        reportFlag(flags, log, DetectionFlag::kPebDebugFlag, MG_STR("AntiDebug"), fmt::format("NtGlobalFlag=0x{:X}", ntGlobalFlag), ntGlobalFlag);

    u32 heapFlags = 0;
    u32 heapForceFlags = 0;
    if (hasDebugHeapFlags(peb, heapFlags, heapForceFlags)) {
        reportFlag(
            flags,
            log,
            DetectionFlag::kPebDebugFlag,
            MG_STR("AntiDebug"),
            fmt::format(MG_STR_CONST("HeapFlags=0x{:X} HeapForceFlags=0x{:X}"), heapFlags, heapForceFlags),
            heapForceFlags ? heapForceFlags : heapFlags);
    }

    if (hasVehDebuggerModule())
        reportFlag(flags, log, DetectionFlag::kDebuggerPresent, MG_STR("AntiDebug"), MG_STR("VEH debugger module loaded"), 1);

    if (api_.NtQueryInformationProcess) {
        uptr port = 0;
        if (NT_SUCCESS(api_.NtQueryInformationProcess(GetCurrentProcess(), kProcessDebugPortClass, &port, static_cast<ULONG>(sizeof(port)), nullptr)) && port)
            reportFlag(flags, log, DetectionFlag::kDebugPort, MG_STR("AntiDebug"), fmt::format(MG_STR_CONST("ProcessDebugPort=0x{:X}"), port), static_cast<u32>(port));

        HANDLE obj = nullptr;
        if (NT_SUCCESS(api_.NtQueryInformationProcess(GetCurrentProcess(), kProcessDebugObjectHandleClass, &obj, static_cast<ULONG>(sizeof(obj)), nullptr)) && obj) {
            reportFlag(flags, log, DetectionFlag::kDebugPort, MG_STR("AntiDebug"), MG_STR("ProcessDebugObjectHandle"), static_cast<u32>(reinterpret_cast<uptr>(obj)));
            if (api_.NtClose) api_.NtClose(obj);
        }

        ULONG processDebugFlags = 1;
        if (NT_SUCCESS(api_.NtQueryInformationProcess(GetCurrentProcess(), kProcessDebugFlagsClass, &processDebugFlags, static_cast<ULONG>(sizeof(processDebugFlags)), nullptr)) && processDebugFlags == 0)
            reportFlag(flags, log, DetectionFlag::kDebugPort, MG_STR("AntiDebug"), MG_STR("ProcessDebugFlags=0"), processDebugFlags);
    }

    if (hasKernelDebugger(api_))
        reportFlag(flags, log, DetectionFlag::kDebuggerPresent, MG_STR("AntiDebug"), MG_STR("SystemKernelDebuggerInformation"), 1);

    if (hasSharedKernelDebuggerFlag())
        reportFlag(flags, log, DetectionFlag::kDebuggerPresent, MG_STR("AntiDebug"), MG_STR("KUSER_SHARED_DATA.KdDebuggerEnabled"), 1);

    if (hasHardwareBreakpoints())
        reportFlag(flags, log, DetectionFlag::kDebuggerPresent, MG_STR("AntiDebug"), MG_STR("Hardware debug registers set"), 1);
}

} // namespace mg
