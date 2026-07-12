#include "pch.hpp"
#include <unordered_set>
#include "anticheat/scanners/handle_watcher_scanner.hpp"

namespace mg {

namespace {

bool hasDangerousProcessAccess(ACCESS_MASK access) {
    constexpr ACCESS_MASK kDangerousMask =
        PROCESS_CREATE_THREAD |
        PROCESS_VM_OPERATION |
        PROCESS_VM_READ |
        PROCESS_VM_WRITE |
        PROCESS_DUP_HANDLE |
        PROCESS_SET_INFORMATION |
        PROCESS_SET_QUOTA |
        PROCESS_SUSPEND_RESUME |
        PROCESS_TERMINATE |
        PROCESS_ALL_ACCESS;

    return (access & kDangerousMask) != 0;
}

std::wstring baseNameFromPath(const std::wstring& path) {
    const std::size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos)
        return path;

    return path.substr(pos + 1);
}

bool isSystemSessionProcess(DWORD pid) {
    if (pid <= 4) return true;

    DWORD sessionId = 0xFFFFFFFF;
    if (!ProcessIdToSessionId(pid, &sessionId))
        return true;

    if (sessionId != 0)
        return false;

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc || hProc == INVALID_HANDLE_VALUE)
        return true;

    wchar_t imgPath[MAX_PATH]{};
    DWORD len = MAX_PATH;
    bool result = false;

    if (QueryFullProcessImageNameW(hProc, 0, imgPath, &len) != FALSE) {
        wchar_t winDir[MAX_PATH]{};
        UINT winDirLen = GetWindowsDirectoryW(winDir, MAX_PATH);
        if (winDirLen > 0 && _wcsnicmp(imgPath, winDir, winDirLen) == 0)
            result = true;
    }
    else {
        result = true;
    }

    CloseHandle(hProc);
    return result;
}

std::wstring getProcessDisplayName(DWORD pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                if (pe.th32ProcessID == pid) {
                    std::wstring name = pe.szExeFile;
                    CloseHandle(snap);
                    return name;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }

    std::wstring fallback = L"pid_" + std::to_wstring(pid);
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc || hProc == INVALID_HANDLE_VALUE)
        return fallback;

    wchar_t imgName[MAX_PATH]{};
    DWORD len = MAX_PATH;
    if (QueryFullProcessImageNameW(hProc, 0, imgName, &len) != FALSE) {
        CloseHandle(hProc);
        return baseNameFromPath(imgName);
    }

    CloseHandle(hProc);
    return fallback;
}

bool tryCloseRemoteHandle(NtApi& api, DWORD ownerPid, USHORT handleValue) {
    HANDLE hProc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, ownerPid);
    if (!hProc || hProc == INVALID_HANDLE_VALUE)
        return false;

    HANDLE dup = nullptr;
    const NTSTATUS st = api.NtDuplicateObject(
        hProc,
        reinterpret_cast<HANDLE>(static_cast<uptr>(handleValue)),
        GetCurrentProcess(),
        &dup,
        0,
        FALSE,
        DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);

    if (dup)
        api.NtClose(dup);

    CloseHandle(hProc);
    return NT_SUCCESS(st);
}

bool referencesSelfViaDuplicate(NtApi& api, DWORD ownerPid, USHORT handleValue, DWORD myPid) {
    HANDLE hProc = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, ownerPid);
    if (!hProc || hProc == INVALID_HANDLE_VALUE)
        return false;

    HANDLE dup = nullptr;
    const NTSTATUS st = api.NtDuplicateObject(
        hProc,
        reinterpret_cast<HANDLE>(static_cast<uptr>(handleValue)),
        GetCurrentProcess(),
        &dup,
        PROCESS_QUERY_LIMITED_INFORMATION,
        FALSE,
        0);

    bool referencesSelf = false;
    if (NT_SUCCESS(st) && dup) {
        referencesSelf = (GetProcessId(dup) == myPid);
        api.NtClose(dup);
    }

    CloseHandle(hProc);
    return referencesSelf;
}

} // anonymous namespace

void HandleWatcherScanner::scan(std::atomic<u32>& flags, Logger& log) {
    if (!api_.NtQuerySystemInformation || !api_.NtDuplicateObject) return;

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

    std::unordered_set<DWORD> systemPids;
    std::unordered_set<DWORD> checkedPids;

    for (ULONG i = 0; i < info->HandleCount; ++i) {
        auto& e = info->Handles[i];
        if (e.ProcessId == myPid || !hasDangerousProcessAccess(e.GrantedAccess))
            continue;

        if (checkedPids.find(e.ProcessId) == checkedPids.end()) {
            checkedPids.insert(e.ProcessId);
            if (isSystemSessionProcess(e.ProcessId))
                systemPids.insert(e.ProcessId);
        }
        if (systemPids.count(e.ProcessId))
            continue;

        const bool referencesSelf =
            referencesSelfViaDuplicate(api_, e.ProcessId, e.Handle, myPid);
        if (!referencesSelf)
            continue;

        const std::wstring processName = getProcessDisplayName(e.ProcessId);
        const bool closed = tryCloseRemoteHandle(api_, e.ProcessId, e.Handle);

        reportFlag(flags, log, DetectionFlag::kDangerousHandle, MG_STR("HandleWatcher"),
            fmt::format(MG_STR_CONST("PID={} ({}) access=0x{:X}{}"),
                        e.ProcessId,
                        toNarrow(processName),
                        e.GrantedAccess,
                        closed ? MG_STR_CONST(" [closed]") : MG_STR_CONST("")),
            static_cast<u32>(e.ProcessId));
    }
}

} // namespace mg
