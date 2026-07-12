// =============================================================================
// DetectionEngine - Modular runtime anti-cheat scanner
// =============================================================================
// Architecture:
//   - IScanner: abstract interface for each scanner module
//   - Individual scanner classes: AntiDebugScanner, InlineHookScanner, etc.
//   - DetectionEngine: owns all scanners, runs a timer-based loop (no Sleep)
//   - Thread heartbeat monitoring for thread protection
//   - NT APIs resolved at runtime (no static imports)
//   - All in-process: direct memory reads (no ReadProcessMemory on self)
// =============================================================================
#pragma once

#include "core/types.hpp"
#include <string>
#include <vector>

namespace mg {

class MegaGuardContext;
class Logger;

// ── Detection flags (bitfield) ────────────────────────────────────────────────
enum class DetectionFlag : u32 {
    kNone                  = 0,
    kDebuggerPresent       = (1u << 0),
    kDebugPort             = (1u << 1),
    kTimingAnomaly         = (1u << 2),
    kPebDebugFlag          = (1u << 3),
    kInlineHook            = (1u << 4),
    kIatHook               = (1u << 5),
    kHoneypotTriggered     = (1u << 6),
    kIntegrityViolation    = (1u << 7),
    kDllInjection          = (1u << 8),
    kManualMap             = (1u << 9),
    kAnonymousThread       = (1u << 10),
    kProxyDll              = (1u << 11),
    kGlobalHookInjection   = (1u << 12),
    kMappedImage           = (1u << 13),
    kHookIntegrity         = (1u << 14),
    kBlacklistedModule     = (1u << 15),
    kUnsignedModule        = (1u << 16),
    kDangerousHandle       = (1u << 17),
    kVulnerableDriver      = (1u << 18),
    kBlacklistedString     = (1u << 19),
    kBlacklistedSignature  = (1u << 20),
    kUnsignedDriver        = (1u << 21),
    kDriverBlocklistDisabled = (1u << 22),
    kHvciDisabled          = (1u << 23),
};

// ── Byte signature with optional mask (0 = wildcard, 1 = exact compare) ──────
struct BytePattern {
    std::vector<u8> pattern;
    std::vector<u8> mask;
    std::string     name;
};

// ── Per-scanner performance stats ─────────────────────────────────────────────
struct ScannerStats {
    const char* name       = "";
    u32         interval   = 0;
    u64         lastUs     = 0;   // last scan duration in microseconds
    u64         peakUs     = 0;   // peak scan duration in microseconds
    u64         totalUs    = 0;   // cumulative scan time
    u32         runCount   = 0;   // number of times scan() was called
};

// ── IScanner: abstract interface every scanner module implements ───────────────
class IScanner {
public:
    virtual ~IScanner() = default;

    virtual const char* name() const = 0;
    virtual u32         intervalSeconds() const = 0;
    virtual void        scan(std::atomic<u32>& flags, Logger& log) = 0;

    // Optional: one-time init after NT APIs are resolved
    virtual void        init() {}

    // Configurable interval override (0 = use default from intervalSeconds())
    u32 intervalOverride = 0;

    // Effective interval: override if set, else default
    u32 effectiveInterval() const { return intervalOverride ? intervalOverride : intervalSeconds(); }
};

// ── DetectionEngine: owns scanners, timer loop, heartbeat monitor ─────────────
class DetectionEngine {
public:
    explicit DetectionEngine(MegaGuardContext& ctx);
    ~DetectionEngine();

    DetectionEngine(const DetectionEngine&) = delete;
    DetectionEngine& operator=(const DetectionEngine&) = delete;

    // ── Lifecycle ──────────────────────────────────────────────────────────
    VoidResult initialize();
    void       shutdown();

    // ── Thread registration for heartbeat monitoring ───────────────────────
    void trackThread(void* handle, const char* name);

    // ── Scanner configuration (call before or after initialize) ───────────
    void addBlacklistedModule(const std::string& name);
    void addBlacklistedString(const std::string& str);
    void addBlacklistedSignature(BytePattern bp);
    void addHookBaseline(uptr address, u32 size);

    // ── Scanner tuning ─────────────────────────────────────────────────────
    void setScannerInterval(const char* scannerName, u32 seconds);

    // ── Performance stats ─────────────────────────────────────────────────
    std::vector<ScannerStats> getScannerStats() const;
    void dumpScannerStats();

    // ── Query ─────────────────────────────────────────────────────────────
    u32  detectionFlags() const;
    bool hasDetection() const;

    // Atomically read and clear all accumulated detection flags.
    u32  exchangeFlags();

private:
    friend DWORD WINAPI scannerThreadProc(LPVOID);

    struct Impl;
    Impl* impl_;
};

} // namespace mg
