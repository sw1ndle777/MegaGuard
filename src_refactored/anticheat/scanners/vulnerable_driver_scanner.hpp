#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class VulnerableDriverScanner final : public IScanner {
public:
    const char* name() const override { return "DriverScan"; }
    u32 intervalSeconds() const override { return MG_INT(60); }
    void scan(std::atomic<u32>& flags, Logger& log) override;
};

} // namespace mg
