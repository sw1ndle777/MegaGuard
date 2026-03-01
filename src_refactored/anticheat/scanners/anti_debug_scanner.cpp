#include "pch.hpp"
#include "anticheat/scanners/anti_debug_scanner.hpp"

namespace mg {

void AntiDebugScanner::scan(std::atomic<u32>& flags, Logger& log) {
    if (IsDebuggerPresent())
        reportFlag(flags, log, DetectionFlag::kDebuggerPresent, MG_STR("AntiDebug"), MG_STR("IsDebuggerPresent"), 1);

    auto* peb = reinterpret_cast<PPEB>(__readfsdword(MG_INT(0x30)));
    if (peb && peb->BeingDebugged)
        reportFlag(flags, log, DetectionFlag::kPebDebugFlag, MG_STR("AntiDebug"), MG_STR("PEB.BeingDebugged"), 1);

    u32 ntgf = *reinterpret_cast<u32*>(reinterpret_cast<u8*>(peb) + MG_INT(0x68));
    if (ntgf & MG_INT(0x70))
        reportFlag(flags, log, DetectionFlag::kPebDebugFlag, MG_STR("AntiDebug"), fmt::format("NtGlobalFlag=0x{:X}", ntgf), ntgf);

    if (api_.NtQueryInformationProcess) {
        DWORD_PTR port = 0;
        if (NT_SUCCESS(api_.NtQueryInformationProcess(GetCurrentProcess(), MG_INT(7), &port, sizeof(port), nullptr)) && port)
            reportFlag(flags, log, DetectionFlag::kDebugPort, MG_STR("DebugPort"), fmt::format("port=0x{:X}", port), static_cast<u32>(port));

        HANDLE obj = nullptr;
        if (NT_SUCCESS(api_.NtQueryInformationProcess(GetCurrentProcess(), MG_INT(0x1E), &obj, sizeof(obj), nullptr)) && obj) {
            reportFlag(flags, log, DetectionFlag::kDebugPort, MG_STR("DebugObject"), "", static_cast<u32>(reinterpret_cast<uptr>(obj)));
            if (api_.NtClose) api_.NtClose(obj);
        }
    }

    u64 t0 = __rdtsc();
    volatile u32 dummy = 0;
    for (int i = 0; i < MG_INT(100); ++i) dummy += i;
    u64 elapsed = __rdtsc() - t0;
    if (elapsed > MG_INT(100000u))
        reportFlag(flags, log, DetectionFlag::kTimingAnomaly, MG_STR("Timing"), fmt::format("elapsed={} cycles", elapsed), static_cast<u32>(elapsed));
}

} // namespace mg
