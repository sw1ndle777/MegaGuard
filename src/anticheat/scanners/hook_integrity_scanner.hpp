#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class HookIntegrityScanner final : public IScanner {
    std::vector<HookBaseline>& baselines_;
    CritSection& cs_;
public:
    HookIntegrityScanner(std::vector<HookBaseline>& bl, CritSection& cs) : baselines_(bl), cs_(cs) {}
    const char* name() const override { return "HookIntegrity"; }
    u32 intervalSeconds() const override { return MG_INT(10); }
    void scan(std::atomic<u32>& flags, Logger& log) override;
};

} // namespace mg
