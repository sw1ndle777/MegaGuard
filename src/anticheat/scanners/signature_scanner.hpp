#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class SignatureScanner final : public IScanner {
    std::vector<BytePattern>& sigs_;
    CritSection& cs_;
public:
    SignatureScanner(std::vector<BytePattern>& sigs, CritSection& cs) : sigs_(sigs), cs_(cs) {}
    const char* name() const override { return "SigScan"; }
    u32 intervalSeconds() const override { return MG_INT(30); }
    void scan(std::atomic<u32>& flags, Logger& log) override;
};

} // namespace mg
