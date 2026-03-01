// =============================================================================
// DetectionReport — Thread-safe detection event queue + flag accumulator
// =============================================================================
// Any subsystem (scanner, game manager, heartbeat) calls report() to queue
// a detection event.  HeartbeatHandler calls drainEvents() once per heartbeat
// to harvest all accumulated events + flags, then everything is cleared.
//
// Two layers:
//   1. Flag accumulator (atomic u32) — fast bitfield for the obfuscated 128-bit
//      detection field.  Scanner flags from DetectionEngine merge into this too.
//   2. Event queue (CRITICAL_SECTION + ring buffer) — detailed per-event data
//      with type + timestamp + extra context, sent as an array in the response.
// =============================================================================
#pragma once

#include "core/types.hpp"
#include "anticheat/detection_engine.hpp"
#include <atomic>

namespace mg {

// ── Detection event (queued for next heartbeat) ─────────────────────────────
#pragma pack(push, 1)
struct DetectionEvent {
    u32 flag;       // which DetectionFlag was triggered
    u32 timestamp;  // GetTickCount() at time of detection
    u32 extra;      // scanner-specific context (address, hash, etc.)
};
#pragma pack(pop)

inline constexpr u32 kMaxQueuedEvents = 16;

class DetectionReport {
public:
    DetectionReport() {
        InitializeCriticalSection(&cs_);
    }

    ~DetectionReport() {
        DeleteCriticalSection(&cs_);
    }

    DetectionReport(const DetectionReport&) = delete;
    DetectionReport& operator=(const DetectionReport&) = delete;

    // Queue a detection flag + optional context (thread-safe).
    // Sets the flag bit AND pushes an event into the ring buffer.
    void report(DetectionFlag flag, u32 extra = 0) {
        flags_.fetch_or(static_cast<u32>(flag), std::memory_order_release);

        DetectionEvent ev{};
        ev.flag      = static_cast<u32>(flag);
        ev.timestamp = GetTickCount();
        ev.extra     = extra;

        EnterCriticalSection(&cs_);
        if (count_ < kMaxQueuedEvents) {
            events_[count_++] = ev;
        }
        LeaveCriticalSection(&cs_);
    }

    // Atomically harvest all pending flags and clear the accumulator.
    u32 drainFlags() {
        return flags_.exchange(0, std::memory_order_acq_rel);
    }

    // Copy all queued events into `out` (up to `maxOut`), clear the queue.
    // Returns the number of events copied.
    u32 drainEvents(DetectionEvent* out, u32 maxOut) {
        EnterCriticalSection(&cs_);
        u32 n = (count_ < maxOut) ? count_ : maxOut;
        for (u32 i = 0; i < n; ++i)
            out[i] = events_[i];
        count_ = 0;
        LeaveCriticalSection(&cs_);
        return n;
    }

    // Non-destructive peek at current accumulated flags.
    u32 peekFlags() const {
        return flags_.load(std::memory_order_acquire);
    }

private:
    CRITICAL_SECTION cs_;
    DetectionEvent   events_[kMaxQueuedEvents]{};
    u32              count_ = 0;
    std::atomic<u32> flags_{0};
};

} // namespace mg
