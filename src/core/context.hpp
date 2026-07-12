// =============================================================================
// MegaGuardContext - Central Ownership Root
// =============================================================================
// DESIGN:
//   - Owns ALL runtime state. No globals, no statics, no singletons.
//   - Constructed explicitly from the DLL entry point.
//   - Passed by reference (dependency injection) to every subsystem.
//   - Destruction order is deterministic (reverse of member declaration).
//   - Safe for manual-mapped DLLs (no CRT static init).
//
// PIMPL:
//   - Implementation hidden behind Impl struct to keep this header lean.
//   - Changes to subsystem internals don't trigger rebuild of includers.
// =============================================================================
#pragma once

#include "types.hpp"

// Forward declarations only — no heavy includes in public header
namespace mg {

class MemoryPool;
class ImportResolver;
class SyscallCloner;
class IpcClient;
class Logger;
class HookRegistry;
class PatternScanner;
class PointerEncryption;
class IntegrityEngine;
class IATScrubber;
class ManualSEH;
class GuardRegions;
class DetectionEngine;
class Splash;

namespace game {
    class CryptoEngine;
    class GameManagerHooks;
    class SecureChannel;
    class HeartbeatHandler;
    class WeeklyRewardHandler;
    class TradeHandler;
}

class MegaGuardContext {
public:
    // Construct with the anticheat module base address (from manual map loader).
    explicit MegaGuardContext(uptr moduleBase);
    ~MegaGuardContext();

    // Non-copyable, non-movable (ownership root)
    MegaGuardContext(const MegaGuardContext&) = delete;
    MegaGuardContext& operator=(const MegaGuardContext&) = delete;
    MegaGuardContext(MegaGuardContext&&) = delete;
    MegaGuardContext& operator=(MegaGuardContext&&) = delete;

    // ── Initialization sequence (called from entry point) ──────────────────
    VoidResult initialize();
    void shutdown();

    // ── Subsystem accessors (return references — caller never owns) ────────
    MemoryPool&           memoryPool();
    ImportResolver&       importResolver();
    SyscallCloner&        syscallCloner();
    IpcClient&            ipcClient();
    Logger&               logger();
    HookRegistry&         hookRegistry();
    PatternScanner&       patternScanner();
    PointerEncryption&    pointerEncryption();
    IntegrityEngine&      integrityEngine();
    IATScrubber&           iatScrubber();
    ManualSEH&            manualSEH();
    GuardRegions&         guardRegions();
    game::CryptoEngine&   cryptoEngine();
    DetectionEngine&      detectionEngine();
    Splash&               splashScreen();
    game::GameManagerHooks& gameManagers();
    game::SecureChannel&  secureChannel();
    game::HeartbeatHandler& heartbeatHandler();
    game::WeeklyRewardHandler& weeklyRewardHandler();
    game::TradeHandler&   tradeHandler();

    // ── Module base ────────────────────────────────────────────────────────
    uptr moduleBase() const;

private:
    // PIMPL — all subsystem storage lives here, defined in context.cpp.
    // This prevents rebuild cascades when subsystem internals change.
    struct Impl;
    Impl* impl_;  // Raw pointer: manually allocated via VirtualAlloc, no CRT.
    uptr moduleBase_;
};

} // namespace mg
