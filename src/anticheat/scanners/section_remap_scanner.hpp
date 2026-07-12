#pragma once
#include "anticheat/scanners/scanner_common.hpp"

namespace mg {

class GuardRegions;

// =============================================================================
// SectionRemapScanner — Detects dual-mapped memory (NtMapViewOfSection bypass)
// =============================================================================
// Attackers can map the same physical pages at two virtual addresses via
// NtCreateSection + NtMapViewOfSection.  The "shadow" mapping uses PAGE_READWRITE
// while the original stays PAGE_NOACCESS / PAGE_EXECUTE_READ, allowing reads and
// writes that never trigger the GuardRegions exception handler.
//
// Detection strategy:
//   1. Walk the VA space looking for MEM_MAPPED regions with R/W that overlap
//      the physical backing of our guarded pages.
//   2. For each candidate, compare content against the known XOR-encrypted
//      snapshot: if the shadow view shows *decrypted* content while the guard
//      page is in kEncrypted state, it's a confirmed dual-map bypass.
//   3. NtQueryVirtualMemory(MemoryMappedFilenameInformation) to distinguish
//      file-backed sections from pagefile-backed (suspicious).
// =============================================================================
class SectionRemapScanner final : public IScanner {
    NtApi& api_;
public:
    explicit SectionRemapScanner(NtApi& api) : api_(api) {}
    const char* name() const override { return "SectionRemap"; }
    u32 intervalSeconds() const override { return MG_INT(10); }
    void scan(std::atomic<u32>& flags, Logger& log) override;
};

} // namespace mg
