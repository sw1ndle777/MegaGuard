#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class StringScanner final : public IScanner {
    std::vector<std::string>& strings_;
    CritSection& cs_;
public:
    StringScanner(std::vector<std::string>& strs, CritSection& cs) : strings_(strs), cs_(cs) {}
    const char* name() const override { return "StringScan"; }
    u32 intervalSeconds() const override { return MG_INT(30); }
    void scan(std::atomic<u32>& flags, Logger& log) override;
};

} // namespace mg
