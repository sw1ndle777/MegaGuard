#include "pch.hpp"
#include "anticheat/scanners/anonymous_thread_scanner.hpp"

namespace mg {

void AnonymousThreadScanner::scan(std::atomic<u32>& flags, Logger& log) {
    if (!api_.NtQueryInformationThread) return;

    DWORD pid = GetCurrentProcessId();
    auto modules = getLoadedModules();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    THREADENTRY32 te{};
    te.dwSize = sizeof(te);

    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID != pid) continue;

            HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
            if (!hThread) continue;

            uptr startAddr = 0;
            api_.NtQueryInformationThread(hThread, MG_INT(9u), &startAddr, sizeof(startAddr), nullptr);
            CloseHandle(hThread);

            if (startAddr && !isAddressInModules(modules, startAddr) && !isInsideAcModule(startAddr))
                reportFlag(flags, log, DetectionFlag::kAnonymousThread, MG_STR("ThreadScan"),
                    fmt::format("TID={} start=0x{:X}", te.th32ThreadID, startAddr),
                    static_cast<u32>(startAddr));
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
}

} // namespace mg
