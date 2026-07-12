#include "pch.hpp"
#include "anticheat/scanners/integrity_scanner.hpp"

namespace mg {

void IntegrityScanner::init() {
    auto [gb, gs] = findSection(game::addr::globals::g_GameModuleBase, MG_STR(".text"));
    if (gb && gs) { gameBase_ = gb; gameSize_ = gs; gameHash_ = fnv1a(reinterpret_cast<const u8*>(gb), gs); }

    auto [ab, as_] = findSection(game::addr::globals::g_AntiCheatModuleBase, MG_STR(".text"));
    if (ab && as_) { acBase_ = ab; acSize_ = as_; acHash_ = fnv1a(reinterpret_cast<const u8*>(ab), as_); }

    ready_ = (gameHash_ != 0 || acHash_ != 0);
}

void IntegrityScanner::scan(std::atomic<u32>& flags, Logger& log) {
    if (!ready_) return;

    if (gameHash_) {
        u32 cur = fnv1a(reinterpret_cast<const u8*>(gameBase_), gameSize_);
        if (cur != gameHash_)
            reportFlag(flags, log, DetectionFlag::kIntegrityViolation, MG_STR("Integrity"),
                fmt::format(MG_STR_CONST("Game .text mismatch: exp=0x{:08X} got=0x{:08X}"), gameHash_, cur), cur);
    }
    if (acHash_) {
        u32 cur = fnv1a(reinterpret_cast<const u8*>(acBase_), acSize_);
        if (cur != acHash_)
            reportFlag(flags, log, DetectionFlag::kIntegrityViolation, MG_STR("Integrity"),
                fmt::format(MG_STR_CONST("AC .text mismatch: exp=0x{:08X} got=0x{:08X}"), acHash_, cur), cur);
    }
}

} // namespace mg
