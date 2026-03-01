#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class UnsignedModuleScanner final : public IScanner {
    NtApi& api_;
public:
    explicit UnsignedModuleScanner(NtApi& api) : api_(api) {}
    const char* name() const override { return "SignatureCheck"; }
    u32 intervalSeconds() const override { return MG_INT(60); }
    void scan(std::atomic<u32>& flags, Logger& log) override;
};

} // namespace mg
