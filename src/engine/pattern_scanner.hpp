// =============================================================================
// PatternScanner - Signature-based code scanning
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg {

class PatternScanner {
public:
    explicit PatternScanner(uptr moduleBase);
    ~PatternScanner() = default;

    PatternScanner(const PatternScanner&) = delete;
    PatternScanner& operator=(const PatternScanner&) = delete;

    // Find a single pattern match. Returns 0 if not found.
    uptr findPattern(uptr start, u32 size, const char* pattern) const;

    // Find all matches
    std::vector<uptr> findPatterns(uptr start, u32 size, const char* pattern) const;

    // Find all CALL instructions targeting a specific address
    std::vector<uptr> findReturnAddresses(uptr start, u32 size, uptr targetAddress) const;

    // Build a set of return addresses (for whitelist validation)
    void addReturnAddresses(uptr start, u32 size, uptr targetAddress,
                            boost::unordered_flat_set<u32>& outSet) const;

    // Dereference a relative pointer
    uptr dereference(uptr address, unsigned int offset) const;

private:
    uptr moduleBase_;
};

} // namespace mg
