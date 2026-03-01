#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class IntegrityScanner final : public IScanner {
    u32  gameHash_ = 0, acHash_ = 0;
    uptr gameBase_ = 0, acBase_ = 0;
    u32  gameSize_ = 0, acSize_ = 0;
    bool ready_ = false;

public:
    const char* name() const override { return "Integrity"; }
    u32 intervalSeconds() const override { return MG_INT(5); }
    void init() override;
    void scan(std::atomic<u32>& flags, Logger& log) override;
};

} // namespace mg
