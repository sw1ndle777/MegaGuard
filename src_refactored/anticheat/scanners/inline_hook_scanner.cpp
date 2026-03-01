#include "pch.hpp"
#include "anticheat/scanners/inline_hook_scanner.hpp"

namespace mg {

void InlineHookScanner::scan(std::atomic<u32>& flags, Logger& log) {
    struct Target { const char* mod; const char* fn; };
    const Target targets[] = {
        // ntdll — core NT syscall wrappers
        { MG_STR("ntdll.dll"), MG_STR("NtQuerySystemInformation") },
        { MG_STR("ntdll.dll"), MG_STR("NtQueryInformationProcess") },
        { MG_STR("ntdll.dll"), MG_STR("NtReadVirtualMemory") },
        { MG_STR("ntdll.dll"), MG_STR("NtWriteVirtualMemory") },
        { MG_STR("ntdll.dll"), MG_STR("NtProtectVirtualMemory") },
        { MG_STR("ntdll.dll"), MG_STR("NtOpenProcess") },
        { MG_STR("ntdll.dll"), MG_STR("NtCreateThreadEx") },
        { MG_STR("ntdll.dll"), MG_STR("NtAllocateVirtualMemory") },
        { MG_STR("ntdll.dll"), MG_STR("NtFreeVirtualMemory") },
        { MG_STR("ntdll.dll"), MG_STR("NtSetInformationThread") },
        { MG_STR("ntdll.dll"), MG_STR("NtSuspendThread") },
        { MG_STR("ntdll.dll"), MG_STR("NtResumeThread") },
        { MG_STR("ntdll.dll"), MG_STR("NtGetContextThread") },
        { MG_STR("ntdll.dll"), MG_STR("NtSetContextThread") },
        { MG_STR("ntdll.dll"), MG_STR("LdrLoadDll") },
        // kernel32
        { MG_STR("kernel32.dll"), MG_STR("VirtualProtect") },
        { MG_STR("kernel32.dll"), MG_STR("VirtualProtectEx") },
        { MG_STR("kernel32.dll"), MG_STR("VirtualAllocEx") },
        { MG_STR("kernel32.dll"), MG_STR("VirtualFreeEx") },
        { MG_STR("kernel32.dll"), MG_STR("ReadProcessMemory") },
        { MG_STR("kernel32.dll"), MG_STR("WriteProcessMemory") },
        { MG_STR("kernel32.dll"), MG_STR("CreateRemoteThread") },
        { MG_STR("kernel32.dll"), MG_STR("LoadLibraryA") },
        { MG_STR("kernel32.dll"), MG_STR("LoadLibraryW") },
        { MG_STR("kernel32.dll"), MG_STR("GetProcAddress") },
        { MG_STR("kernel32.dll"), MG_STR("VirtualQuery") },
        { MG_STR("kernel32.dll"), MG_STR("VirtualQueryEx") },
        // ws2_32 — winsock hooks
        { MG_STR("ws2_32.dll"), MG_STR("send") },
        { MG_STR("ws2_32.dll"), MG_STR("recv") },
        { MG_STR("ws2_32.dll"), MG_STR("WSASend") },
        { MG_STR("ws2_32.dll"), MG_STR("WSARecv") },
        { MG_STR("ws2_32.dll"), MG_STR("connect") },
        { MG_STR("ws2_32.dll"), MG_STR("WSAConnect") },
        { MG_STR("ws2_32.dll"), MG_STR("sendto") },
        { MG_STR("ws2_32.dll"), MG_STR("recvfrom") },
        // user32
        { MG_STR("user32.dll"), MG_STR("SetWindowsHookExW") },
        { MG_STR("user32.dll"), MG_STR("GetAsyncKeyState") },
        { MG_STR("user32.dll"), MG_STR("SendMessageW") },
        { MG_STR("user32.dll"), MG_STR("PostMessageW") },
    };

    for (auto& t : targets) {
        HMODULE hMod = GetModuleHandleA(t.mod);
        if (!hMod) continue;
        auto* addr = reinterpret_cast<u8*>(GetProcAddress(hMod, t.fn));
        if (!addr) continue;

        bool hooked = false;
        if (addr[0] == 0xE9) hooked = true;
        else if (addr[0] == 0xFF && addr[1] == 0x25) hooked = true;
        else if (addr[0] == 0x68 && addr[5] == 0xC3) hooked = true;
        else if (addr[0] == 0xEB) hooked = true;

        if (hooked)
            reportFlag(flags, log, DetectionFlag::kInlineHook, MG_STR("InlineHook"),
                fmt::format("{}!{} @ 0x{:X}", t.mod, t.fn, reinterpret_cast<uptr>(addr)),
                static_cast<u32>(reinterpret_cast<uptr>(addr)));
    }
}

} // namespace mg
