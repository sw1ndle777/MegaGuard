#include "pch.hpp"
#include "anticheat/scanners/string_scanner.hpp"

namespace mg {

void StringScanner::scan(std::atomic<u32>& flags, Logger& log) {
    CritLock lk(cs_);
    if (strings_.empty()) return;

    uptr base = game::addr::globals::g_GameModuleBase;
    if (!base) return;

    auto [rdataAddr, rdataSize] = findSection(base, MG_STR(".rdata"));
    if (!rdataAddr || !rdataSize) return;

    auto* ptr = reinterpret_cast<const u8*>(rdataAddr);

    for (auto& str : strings_) {
        auto wstr = toWide(str);
        u32 len = static_cast<u32>(str.size());

        for (u32 i = 0; i + len < rdataSize; ++i) {
            if (memcmp(ptr + i, str.c_str(), len) == 0 ||
                (i + wstr.size() * sizeof(wchar_t) < rdataSize &&
                 memcmp(ptr + i, wstr.c_str(), wstr.size() * sizeof(wchar_t)) == 0))
            {
                reportFlag(flags, log, DetectionFlag::kBlacklistedString, MG_STR("StringScan"),
                    fmt::format("Found '{}' at 0x{:X}", str, rdataAddr + i),
                    static_cast<u32>(rdataAddr + i));
                break;
            }
        }
    }
}

} // namespace mg
