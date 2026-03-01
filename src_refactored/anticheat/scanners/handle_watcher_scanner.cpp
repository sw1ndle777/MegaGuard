#include "pch.hpp"
#include "anticheat/scanners/handle_watcher_scanner.hpp"

namespace mg {

void HandleWatcherScanner::scan(std::atomic<u32>& flags, Logger& log) {
    if (!api_.NtQuerySystemInformation || !api_.NtDuplicateObject) return;

    constexpr ACCESS_MASK kDangerous =
        PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_ALL_ACCESS |
        PROCESS_VM_OPERATION | PROCESS_DUP_HANDLE |
        PROCESS_SET_INFORMATION | PROCESS_SUSPEND_RESUME;

    DWORD myPid = GetCurrentProcessId();
    ULONG bufSize = MG_INT(0x40000u);
    auto buffer = std::make_unique<u8[]>(bufSize);

    NTSTATUS st;
    while ((st = api_.NtQuerySystemInformation(MG_INT(16u), buffer.get(), bufSize, nullptr)) == static_cast<NTSTATUS>(0xC0000004L)) {
        bufSize *= 2;
        if (bufSize > MG_INT(64u * 1024 * 1024)) return;
        buffer = std::make_unique<u8[]>(bufSize);
    }
    if (!NT_SUCCESS(st)) return;

    auto* info = reinterpret_cast<SystemHandleInformation*>(buffer.get());
    for (ULONG i = 0; i < info->HandleCount; ++i) {
        auto& e = info->Handles[i];
        if (e.ProcessId == myPid || !(e.GrantedAccess & kDangerous)) continue;

        HANDLE hProc = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.ProcessId);
        if (!hProc || hProc == INVALID_HANDLE_VALUE) continue;

        HANDLE dup = nullptr;
        st = api_.NtDuplicateObject(hProc, reinterpret_cast<HANDLE>(static_cast<uptr>(e.Handle)),
            GetCurrentProcess(), &dup, PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 0);

        if (NT_SUCCESS(st) && dup) {
            if (GetProcessId(dup) == myPid) {
                HANDLE closeH = nullptr;
                st = api_.NtDuplicateObject(hProc, reinterpret_cast<HANDLE>(static_cast<uptr>(e.Handle)),
                    GetCurrentProcess(), &closeH, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
                if (NT_SUCCESS(st) && closeH) {
                    api_.NtClose(closeH);
                    wchar_t imgName[MAX_PATH]{};
                    DWORD nl = MAX_PATH;
                    QueryFullProcessImageNameW(hProc, 0, imgName, &nl);
                    reportFlag(flags, log, DetectionFlag::kDangerousHandle, MG_STR("HandleWatcher"),
                        fmt::format("Closed PID={} ({}) access=0x{:X}", e.ProcessId, toNarrow(imgName), e.GrantedAccess),
                        static_cast<u32>(e.ProcessId));
                }
            } else {
                api_.NtClose(dup);
            }
        }
        CloseHandle(hProc);
    }
}

} // namespace mg
