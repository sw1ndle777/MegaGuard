#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class InlineHookScanner final : public IScanner {
public:
    const char* name() const override { return "InlineHook"; }
    u32 intervalSeconds() const override { return MG_INT(5); }
    void scan(std::atomic<u32>& flags, Logger& log) override;
};

} // namespace mg
