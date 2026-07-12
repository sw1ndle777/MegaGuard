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
inline constexpr u32 kDetectionDetailSize = 64;

#pragma pack(push, 1)
struct DetectionEvent {
    u32  flag;                              // which DetectionFlag was triggered
    u32  timestamp;                         // GetTickCount() at time of detection
    u32  extra;                             // scanner-specific context (address, hash, etc.)
    char detail[kDetectionDetailSize];      // compact string context (module / string / driver name)
};
#pragma pack(pop)

static_assert(sizeof(DetectionEvent) == sizeof(u32) * 3 + kDetectionDetailSize,
              "DetectionEvent size mismatch");

inline constexpr u32 kMaxQueuedDetectionEvents = 128;
inline constexpr u32 kRecentDetectionHistory = 512;

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

    // Queue a detection flag + optional numeric/text context (thread-safe).
    // Sets the flag bit AND pushes an event into the bounded FIFO queue.
    void report(DetectionFlag flag, u32 extra = 0, const char* detail = nullptr) {
        DetectionEvent ev{};
        ev.flag      = static_cast<u32>(flag);
        ev.timestamp = GetTickCount();
        ev.extra     = extra;
        copyDetail(ev.detail, detail);

        EnterCriticalSection(&cs_);
        if (isDuplicateLocked(ev)) {
            LeaveCriticalSection(&cs_);
            return;
        }

        flags_.fetch_or(static_cast<u32>(flag), std::memory_order_release);

        if (count_ < kMaxQueuedDetectionEvents) {
            events_[count_++] = ev;
        } else {
            for (u32 i = 1; i < kMaxQueuedDetectionEvents; ++i)
                events_[i - 1] = events_[i];
            events_[kMaxQueuedDetectionEvents - 1] = ev;
        }
        rememberLocked(ev);
        LeaveCriticalSection(&cs_);
    }

    // Atomically harvest all pending flags and clear the accumulator.
    u32 drainFlags() {
        return flags_.exchange(0, std::memory_order_acq_rel);
    }

    // Copy queued events into `out` (up to `maxOut`) and keep any remainder
    // queued for the next heartbeat bulk.
    // Returns the number of events copied.
    u32 drainEvents(DetectionEvent* out, u32 maxOut) {
        EnterCriticalSection(&cs_);
        u32 n = (count_ < maxOut) ? count_ : maxOut;
        for (u32 i = 0; i < n; ++i)
            out[i] = events_[i];
        for (u32 i = n; i < count_; ++i)
            events_[i - n] = events_[i];
        count_ -= n;
        LeaveCriticalSection(&cs_);
        return n;
    }

    // Non-destructive peek at current accumulated flags.
    u32 peekFlags() const {
        return flags_.load(std::memory_order_acquire);
    }

private:
    static bool sameDetail(const char* lhs, const char* rhs) {
        for (u32 i = 0; i < kDetectionDetailSize; ++i) {
            if (lhs[i] != rhs[i])
                return false;

            if (lhs[i] == '\0')
                return true;
        }

        return true;
    }

    static bool sameEvent(const DetectionEvent& lhs, const DetectionEvent& rhs) {
        return lhs.flag == rhs.flag &&
               lhs.extra == rhs.extra &&
               sameDetail(lhs.detail, rhs.detail);
    }

    bool isDuplicateLocked(const DetectionEvent& ev) const {
        for (u32 i = 0; i < count_; ++i) {
            if (sameEvent(events_[i], ev))
                return true;
        }

        for (u32 i = 0; i < recentCount_; ++i) {
            if (sameEvent(recent_[i], ev))
                return true;
        }

        return false;
    }

    void rememberLocked(const DetectionEvent& ev) {
        if (recentCount_ < kRecentDetectionHistory) {
            recent_[recentCount_++] = ev;
            return;
        }

        for (u32 i = 1; i < kRecentDetectionHistory; ++i)
            recent_[i - 1] = recent_[i];

        recent_[kRecentDetectionHistory - 1] = ev;
    }

    static void copyDetail(char* dst, const char* src) {
        for (u32 i = 0; i < kDetectionDetailSize; ++i)
            dst[i] = '\0';

        if (!src)
            return;

        for (u32 i = 0; i + 1 < kDetectionDetailSize && src[i] != '\0'; ++i)
            dst[i] = src[i];
    }

    CRITICAL_SECTION cs_;
    DetectionEvent   events_[kMaxQueuedDetectionEvents]{};
    DetectionEvent   recent_[kRecentDetectionHistory]{};
    u32              count_ = 0;
    u32              recentCount_ = 0;
    std::atomic<u32> flags_{0};
};

} // namespace mg
