// =============================================================================
// MemoryFog — Anti-memory-scanning protection
// =============================================================================
// Creates massive reserved section views that force memory scanning tools
// (PE-sieve, Process Hacker, etc.) to iterate billions of page table entries,
// causing significant slowdowns.
//
// Technique: https://secret.club/2021/05/23/big-memory.html
//
// Architecture:
//   - Transaction-backed ghost file (never committed to disk)
//   - Section object backed by the transacted file
//   - NtMapViewOfSection with MEM_RESERVE to flood VA space
//   - Only 1 page (0x1000) physically committed per view
//   - Sequential hint addresses for contiguous reservations
//   - RAM-based cap to avoid excessive reservation
// =============================================================================
#pragma once

#include "core/types.hpp"
#include <vector>

namespace mg {

class Logger;
struct NtApi;

// ── Configuration ─────────────────────────────────────────────────────────────
struct MemFogConfig {
    u32 viewCount    = 5;     // number of section views to map
    u32 maxSizeShift = 28;    // max view size as 2^N (28 = 256 MB)
    u32 minSizeShift = 24;    // min view size as 2^N (24 = 16 MB)
    u32 ramMultiplier = 256;  // max total reservation = physical RAM * this
};

// ── Single mapped view ────────────────────────────────────────────────────────
struct MemFogMapping {
    void*  base = nullptr;
    size_t size = 0;
};

// ── MemoryFog engine ──────────────────────────────────────────────────────────
class MemoryFog {
public:
    explicit MemoryFog(NtApi& api, Logger& log);
    ~MemoryFog();

    MemoryFog(const MemoryFog&) = delete;
    MemoryFog& operator=(const MemoryFog&) = delete;

    // ── Lifecycle ──────────────────────────────────────────────────────────
    VoidResult activate(const MemFogConfig& cfg = {});
    void       deactivate();

    // ── Query ─────────────────────────────────────────────────────────────
    bool   isActive() const;
    size_t totalReserved() const;
    const std::vector<MemFogMapping>& mappings() const;
    bool   containsAddress(uptr addr) const;

private:
    NtApi&  api_;
    Logger& log_;
    void*   section_ = nullptr;
    std::vector<MemFogMapping> mappings_;
    size_t  totalReserved_ = 0;

    size_t safeMaxReservation(u32 multiplier) const;
};

} // namespace mg
