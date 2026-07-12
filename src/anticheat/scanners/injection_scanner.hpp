#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class InjectionScanner final : public IScanner {
    std::vector<uptr> knownBases_;
    std::vector<std::string>* blacklist_;
    CritSection* configCs_;
    NtApi& api_;

public:
    explicit InjectionScanner(NtApi& api, std::vector<std::string>& bl, CritSection& cs)
        : api_(api), blacklist_(&bl), configCs_(&cs) {}

    const char* name() const override { return "Injection"; }
    u32 intervalSeconds() const override { return MG_INT(10); }
    void init() override;
    void scan(std::atomic<u32>& flags, Logger& log) override;
};

} // namespace mg
