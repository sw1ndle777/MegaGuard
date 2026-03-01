#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class HandleWatcherScanner final : public IScanner {
    NtApi& api_;
public:
    explicit HandleWatcherScanner(NtApi& api) : api_(api) {}
    const char* name() const override { return "HandleWatcher"; }
    u32 intervalSeconds() const override { return MG_INT(15); }
    void scan(std::atomic<u32>& flags, Logger& log) override;
};

} // namespace mg
