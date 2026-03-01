#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class ManualMapScanner final : public IScanner {
public:
    const char* name() const override { return "ManualMap"; }
    u32 intervalSeconds() const override { return MG_INT(15); }
    void scan(std::atomic<u32>& flags, Logger& log) override;

private:
    std::string disassemble(const u8* data, u32 len, uptr addr, u32 maxInsns);
};

} // namespace mg
