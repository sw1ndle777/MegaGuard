#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class AntiDebugScanner final : public IScanner {
    NtApi& api_;
public:
    explicit AntiDebugScanner(NtApi& api) : api_(api) {}
    const char* name() const override { return "AntiDebug"; }
    u32 intervalSeconds() const override { return MG_INT(1); }
    void scan(std::atomic<u32>& flags, Logger& log) override;
};

} // namespace mg
