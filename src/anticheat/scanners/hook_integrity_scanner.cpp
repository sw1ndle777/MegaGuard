#include "pch.hpp"
#include "anticheat/scanners/hook_integrity_scanner.hpp"

namespace mg {

void HookIntegrityScanner::scan(std::atomic<u32>& flags, Logger& log) {
    CritLock lk(cs_);
    for (auto& b : baselines_) {
        u32 cur = fnv1a(reinterpret_cast<const u8*>(b.address), b.size);
        if (cur != b.hash)
            reportFlag(flags, log, DetectionFlag::kHookIntegrity, MG_STR("HookIntegrity"),
                fmt::format(MG_STR_CONST("0x{:X} modified: exp=0x{:08X} got=0x{:08X}"), b.address, b.hash, cur),
                static_cast<u32>(b.address));
    }
}

} // namespace mg
