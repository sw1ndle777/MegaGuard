// =============================================================================
// HeartbeatHandler — Challenge-response + detection flags + event reporting
// =============================================================================
// Protocol:
//   Server → Client:  SecureMsg<HeartbeatChallenge>  (encrypted, counter-nonce)
//   Client → Server:  SecureMsg<HeartbeatResponse>   (encrypted, counter-nonce)
//
// Response contents:
//   1. Challenge answer   — BLAKE2b-keyed(sessionKey, challenge_data)
//   2. Detection bitfield — 128-bit obfuscated field with noise bits
//   3. Queued events      — detailed per-detection data since last heartbeat
//
// File integrity is sent once during MainAuthorize (see SecureAuthPayload).
// =============================================================================
#pragma once

#include "core/types.hpp"
#include "game/structures.hpp"
#include "anticheat/detection_report.hpp"

namespace mg::game {

// Forward declaration (defined in secure_channel.hpp)
struct FileIntegrityPayload;

// ── Detection bitfield constants ─────────────────────────────────────────────
inline constexpr u32 kDetectionBitsSize = 16; // 128 bits
inline constexpr u32 kHeartbeatPacketLimit = 1432;
inline constexpr u32 kHeartbeatEncryptedOverhead = 24 + 16; // nonce + MAC
inline constexpr u32 kHeartbeatTransportOverhead =
    sizeof(STcpPacketHeader) + sizeof(SCommandHeader) + kHeartbeatEncryptedOverhead;
inline constexpr u32 kHeartbeatFixedResponseBytes =
    sizeof(u64) + 32 + kDetectionBitsSize + sizeof(u8) + 3;
inline constexpr i32 kHeartbeatEventBudget =
    static_cast<i32>(kHeartbeatPacketLimit) -
    static_cast<i32>(kHeartbeatTransportOverhead) -
    static_cast<i32>(kHeartbeatFixedResponseBytes);

static_assert(kHeartbeatEventBudget > 0,
              "Heartbeat packet overhead exceeds the 1432-byte transport budget");

inline constexpr u32 kMaxHeartbeatEventsPerPacket =
    static_cast<u32>(kHeartbeatEventBudget / static_cast<i32>(sizeof(DetectionEvent)));

static_assert(kMaxHeartbeatEventsPerPacket > 0,
              "DetectionEvent is too large for the heartbeat packet budget");

// Mapping of DetectionFlag bits to non-sequential positions in 128-bit field.
// Gaps are filled with noise bits — if a cheater zeros the whole field, the
// noise won't match and the server will know.
inline constexpr u8 kDetectionBitMap[] = {
    3, 7, 11, 15, 22, 28, 34, 41, 47, 53, 60,
    67, 73, 79, 85, 91, 97, 103, 109, 115, 121, 126, 123, 125
};

inline constexpr u32 kDetectionFlagCount = sizeof(kDetectionBitMap) / sizeof(kDetectionBitMap[0]);

// Seed for deterministic noise generation (server has the same seed)
inline constexpr u8 kNoiseSeed[16] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
    0x13, 0x37, 0x42, 0x69, 0xAA, 0x55, 0xF0, 0x0D
};

// ── Wire structures ─────────────────────────────────────────────────────────
#pragma pack(push, 1)

struct HeartbeatChallenge {
    u64 challenge_id;         //  8 — unique per heartbeat
    u8  challenge_data[32];   // 32 — random data to solve
};

struct HeartbeatResponse {
    u64              challenge_id;                            //   8
    u8               challenge_answer[32];                    //  32
    u8               detection_bits[kDetectionBitsSize];      //  16
    u8               event_count;                             //   1
    u8               _pad[3];                                 //   3
    DetectionEvent   events[kMaxHeartbeatEventsPerPacket];    // bounded by 1432-byte encrypted packet limit
};

#pragma pack(pop)

static_assert(
    sizeof(STcpPacketHeader) + sizeof(SCommandHeader) +
    kHeartbeatEncryptedOverhead + sizeof(HeartbeatResponse) <= kHeartbeatPacketLimit,
    "Heartbeat response exceeds the 1432-byte encrypted transport limit");

// ── File integrity computation (called from AuthorizeHandler) ───────────────

void computeFileIntegrity(FileIntegrityPayload& out);

// ── HeartbeatHandler class ──────────────────────────────────────────────────

class HeartbeatHandler {
public:
    explicit HeartbeatHandler(::mg::MegaGuardContext& ctx);
    ~HeartbeatHandler();

    VoidResult install();

    // Access the shared detection report queue.
    // Any subsystem can call:
    //   ctx.heartbeatHandler().detectionReport().report(flag, extra, detail);
    ::mg::DetectionReport& detectionReport();

private:
    ::mg::MegaGuardContext& ctx_;
    struct Impl;
    Impl* impl_;
};

// ── Legacy heartbeat crypto (kept for backwards compat) ─────────────────────
u64 generateKey(const u8* buffer, std::size_t size);
u64 encryptKey(u64 key);
u64 decryptKey(u64 key);
void xorEncrypt(u8* buffer, std::size_t size, u64 key);
void xorDecrypt(u8* buffer, std::size_t size, u64 key);

} // namespace mg::game
