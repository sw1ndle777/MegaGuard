#include "pch.hpp"
#include "anticheat/scanners/signature_scanner.hpp"

namespace mg {

void SignatureScanner::scan(std::atomic<u32>& flags, Logger& log) {
    CritLock lk(cs_);
    if (sigs_.empty()) return;

    auto [textAddr, textSize] = findSection(game::addr::globals::g_GameModuleBase, MG_STR(".text"));
    if (!textAddr || !textSize) return;

    auto* textPtr = reinterpret_cast<const u8*>(textAddr);

    for (auto& sig : sigs_) {
        u32 patLen = static_cast<u32>(sig.pattern.size());
        if (!patLen || patLen > textSize) continue;
        const u8* pat = sig.pattern.data();

        for (u32 i = 0; i <= textSize - patLen; ++i) {
            bool match = true;
            for (u32 j = 0; j < patLen; ++j) {
                if (pat[j] != MG_INT(0xFFu) && textPtr[i + j] != pat[j]) { match = false; break; }
            }
            if (match) {
                reportFlag(flags, log, DetectionFlag::kBlacklistedSignature, MG_STR("SigScan"),
                    fmt::format("Pattern '{}' at 0x{:X}", sig.name, textAddr + i),
                    static_cast<u32>(textAddr + i));
                break;
            }
        }
    }
}

} // namespace mg
