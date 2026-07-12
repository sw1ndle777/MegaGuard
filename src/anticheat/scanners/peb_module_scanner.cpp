#include "pch.hpp"
#include "anticheat/scanners/peb_module_scanner.hpp"

namespace mg {

void PebModuleScanner::scan(std::atomic<u32>& flags, Logger& log) {
    auto modules = getLoadedModules();
    CritLock lk(cs_);
    for (auto& m : modules) {
        auto nm = toNarrow(m.name);
        for (auto& bl : blacklist_) {
            if (_stricmp(nm.c_str(), bl.c_str()) == 0)
                reportFlag(flags, log, DetectionFlag::kBlacklistedModule, MG_STR("PEBScan"),
                    fmt::format(MG_STR_CONST("{} @ 0x{:X}"), nm, m.base), static_cast<u32>(m.base));
        }
    }
}

} // namespace mg
