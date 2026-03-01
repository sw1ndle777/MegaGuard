#include "pch.hpp"
#include "anticheat/scanners/unsigned_module_scanner.hpp"

namespace mg {

void UnsignedModuleScanner::scan(std::atomic<u32>& flags, Logger& log) {
    if (!api_.WinVerifyTrust) return;

    wchar_t sysDir[MAX_PATH]{}, winDir[MAX_PATH]{};
    GetSystemDirectoryW(sysDir, MAX_PATH);
    GetWindowsDirectoryW(winDir, MAX_PATH);

    for (auto& m : getLoadedModules()) {
        if (m.path.empty()) continue;
        if (_wcsnicmp(m.path.c_str(), sysDir, wcslen(sysDir)) == 0) continue;
        if (_wcsnicmp(m.path.c_str(), winDir, wcslen(winDir)) == 0) continue;

        if (!isModuleSigned(api_, m.path))
            reportFlag(flags, log, DetectionFlag::kUnsignedModule, MG_STR("SignatureCheck"),
                fmt::format("Unsigned: {} @ 0x{:X}", toNarrow(m.name), m.base),
                static_cast<u32>(m.base));
    }
}

} // namespace mg
