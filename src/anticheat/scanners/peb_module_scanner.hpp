#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class PebModuleScanner final : public IScanner {
    std::vector<std::string>& blacklist_;
    CritSection& cs_;
public:
    PebModuleScanner(std::vector<std::string>& bl, CritSection& cs) : blacklist_(bl), cs_(cs) {}
    const char* name() const override { return "PEBScan"; }
    u32 intervalSeconds() const override { return MG_INT(30); }
    void scan(std::atomic<u32>& flags, Logger& log) override;
};

} // namespace mg
