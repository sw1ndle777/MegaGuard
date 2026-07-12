#include "pch.hpp"
#include "anticheat/scanners/signature_scanner.hpp"

namespace mg {

void SignatureScanner::scan(std::atomic<u32>& flags, Logger& log) {
    CritLock lk(cs_);
    if (sigs_.empty()) return;

    wchar_t sysDir[MAX_PATH]{}, winDir[MAX_PATH]{};
    GetSystemDirectoryW(sysDir, MAX_PATH);
    GetWindowsDirectoryW(winDir, MAX_PATH);

    for (auto& sig : sigs_) {
        u32 patLen = static_cast<u32>(sig.pattern.size());
        if (!patLen) continue;

        const bool useMask = !sig.mask.empty();
        if (useMask && sig.mask.size() != sig.pattern.size())
            continue;

        const u8* pat = sig.pattern.data();
        bool found = false;

        for (const auto& module : getLoadedModules()) {
            if (!module.path.empty()) {
                if (_wcsnicmp(module.path.c_str(), sysDir, wcslen(sysDir)) == 0) continue;
                if (_wcsnicmp(module.path.c_str(), winDir, wcslen(winDir)) == 0) continue;
            }

            auto [textAddr, textSize] = findSection(module.base, MG_STR(".text"));
            if (!textAddr || patLen > textSize) continue;

            auto* textPtr = reinterpret_cast<const u8*>(textAddr);

            for (u32 i = 0; i <= textSize - patLen; ++i) {
                bool match = true;
                for (u32 j = 0; j < patLen; ++j) {
                    if (useMask && sig.mask[j] == 0)
                        continue;
                    if (textPtr[i + j] != pat[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    reportFlag(flags, log, DetectionFlag::kBlacklistedSignature, MG_STR("SigScan"),
                        fmt::format(MG_STR_CONST("Pattern '{}' in {} at 0x{:X}"), sig.name, toNarrow(module.name), textAddr + i),
                        static_cast<u32>(textAddr + i));
                    found = true;
                    break;
                }
            }

            if (found)
                break;
        }
    }
}

} // namespace mg
