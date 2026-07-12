#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class AnonymousThreadScanner final : public IScanner {
    NtApi& api_;
public:
    explicit AnonymousThreadScanner(NtApi& api) : api_(api) {}
    const char* name() const override { return "ThreadScan"; }
    u32 intervalSeconds() const override { return MG_INT(10); }
    void scan(std::atomic<u32>& flags, Logger& log) override;
};

} // namespace mg
