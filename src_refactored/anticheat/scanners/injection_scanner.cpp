#include "pch.hpp"
#include "anticheat/scanners/injection_scanner.hpp"

namespace mg {

void InjectionScanner::init() {
    auto mods = getLoadedModules();
    knownBases_.reserve(mods.size());
    for (auto& m : mods) knownBases_.push_back(m.base);
    std::sort(knownBases_.begin(), knownBases_.end());
}

void InjectionScanner::scan(std::atomic<u32>& flags, Logger& log) {
    auto mods = getLoadedModules();
    for (auto& m : mods) {
        if (std::binary_search(knownBases_.begin(), knownBases_.end(), m.base)) continue;

        auto nm = toNarrow(m.name);
        bool bl = false;
        {
            CritLock lk(*configCs_);
            for (auto& b : *blacklist_)
                if (_stricmp(nm.c_str(), b.c_str()) == 0) { bl = true; break; }
        }

        if (bl) {
            reportFlag(flags, log, DetectionFlag::kBlacklistedModule, MG_STR("ModuleScan"),
                fmt::format("{} @ 0x{:X}", nm, m.base), static_cast<u32>(m.base));
        } else if (!m.path.empty() && !isModuleSigned(api_, m.path)) {
            reportFlag(flags, log, DetectionFlag::kDllInjection, MG_STR("InjectionDetect"),
                fmt::format("{} @ 0x{:X} (unsigned)", nm, m.base), static_cast<u32>(m.base));
        }

        knownBases_.push_back(m.base);
        std::sort(knownBases_.begin(), knownBases_.end());
    }
}

} // namespace mg
