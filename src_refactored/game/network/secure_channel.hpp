// =============================================================================
// SecureChannel — X25519 + Ed25519 + XChaCha20-Poly1305
// =============================================================================
// Full authenticated + signed + encrypted comms for anticheat <-> server.
//
// Protocol (two-phase handshake):
//
// Phase 1 — ConnectAck (Server → Client):
//   Server sends ServerKeyPayload:
//     { sign_pk[32], signature[64], x25519_pk[32], connect_data[8] }
//   - connect_data = plaintext {cryptoKey, uniqueId} (signed, not encrypted)
//   - signature  = Ed25519(sign_sk, x25519_pk || connect_data)
//   Client verifies Ed25519 signature against compiled-in trust anchor,
//   then extracts connect_data and stores server X25519 key.
//
// Phase 2 — MainAuthorize (Client → Server):
//   Client generates ephemeral X25519 keypair and sends SecurePacket:
//     { header[4], client_eph_pk[32], mac[16], encrypted[sizeof(T)] }
//   - session_key = BLAKE2b(X25519(client_eph_sk, server_x25519_pk), "...-AEAD-...")
//   - nonce       = BLAKE2b(client_eph_pk || server_x25519_pk), first 24 bytes
//   - header is AEAD additional data (authenticated, not encrypted)
//   Server derives same key via X25519(server_eph_sk, client_eph_pk).
//
// Wire helpers:
//   SecurePacket<T> — generic encrypt/decrypt wrapper for any payload struct
//   SecureChannel   — owns keys, state, HWID collection
// =============================================================================
#pragma once

#include "core/types.hpp"
#include "game/structures.hpp"
#include "platform/memory.hpp"

namespace mg::game {

// ── Crypto sizes ──────────────────────────────────────────────────────────────
inline constexpr u32 kKeySize    = 32;
inline constexpr u32 kNonceSize  = 24;
inline constexpr u32 kMacSize    = 16;
inline constexpr u32 kSigSize    = 64;
inline constexpr u32 kSignSkSize = 64; // Ed25519 expanded secret key
inline constexpr u32 kHwidSize   = 32; // BLAKE2b-256 of hardware fingerprint

// ── ConnectAck plaintext — signed inside ServerKeyPayload ─────────────────────
#pragma pack(push, 1)
struct ConnectAckData {
    i32      cryptoKey;     // RC5/RC6 crypto key
    UniqueId uniqueId;      // client session ID
};
#pragma pack(pop)
static_assert(sizeof(ConnectAckData) == 8, "ConnectAckData size mismatch");

inline constexpr u32 kConnectDataSize = sizeof(ConnectAckData); // 8

// ── Server hello (received in MainEngineConnectAck) ───────────────────────────
// Signed message = (x25519_pk || connect_data), contiguous in memory.
// =============================================================================
inline constexpr u32 kSignedMsgSize = kKeySize + kConnectDataSize; // 40

#pragma pack(push, 1)
struct ServerKeyPayload {
    u8             sign_pubkey[kKeySize];   // 32 — Ed25519 verify key (must match compiled-in)
    u8             signature[kSigSize];     // 64 — Ed25519(sign_sk, x25519_pk || connect_data)
    // ── signed message starts here ──
    u8             x25519_pubkey[kKeySize]; // 32 — Server ephemeral X25519 DH key
    ConnectAckData connect_data;            //  8 — plaintext {cryptoKey, uniqueId}
    // ── signed message ends here ──
};
#pragma pack(pop)
static_assert(sizeof(ServerKeyPayload) == 136, "ServerKeyPayload size mismatch");

// ── Auth payload (encrypted contents of MainAuthorize) ────────────────────────

// Per-file integrity: individual BLAKE2b-256 hash of each game file.
// Index order: [0] Launcher.exe, [1] map.dat, [2] cgd.dip, [3..33] Release files
// Server compares each slot to detect exactly which file was modified.
inline constexpr u32 kIntegrityFileCount = 34; // 1 launcher + 2 data + 31 release

#pragma pack(push, 1)
struct FileIntegrityPayload {
    u8 hashes[kIntegrityFileCount][32]; // 34 × 32 = 1088 bytes
};
static_assert(sizeof(FileIntegrityPayload) == 1088, "FileIntegrityPayload size mismatch");

struct SecureAuthPayload {
    u32 auth_key;
    u32 auth_key2;
    u32 server_id;
    u8  netVersion1;
    u8  netVersion2;
    u8  netVersion3;
    u8  netVersion4;
    u8  hwid[kHwidSize];
    FileIntegrityPayload integrity;
};
#pragma pack(pop)

// =============================================================================
// SecurePacket<T> — Generic encrypt/decrypt wrapper
// =============================================================================
// Wire layout:
//   [SCommandHeader] [client_pubkey:32] [mac:16] [ciphertext:sizeof(T)]
//
// Usage (encrypt — client side):
//   SecurePacket<SecureAuthPayload> pkt;
//   pkt.m_tHeader = ...;
//   SecureAuthPayload plain = { ... };
//   pkt.seal(channel, plain);           // fills pubkey + mac + ciphertext
//   Send(&pkt, pkt.dataSize(), ...);
//
// Usage (decrypt — server side):
//   SecureAuthPayload plain;
//   if (pkt.open(channel, plain)) { ... }
// =============================================================================
#pragma pack(push, 1)
template <typename T>
struct SecurePacket {
    SCommandHeader m_tHeader;              // plaintext, used as AEAD AD
    u8             client_pubkey[kKeySize]; // 32 — client ephemeral pk, server derives shared key
    u8             mac[kMacSize];          // 16 — AEAD auth tag
    u8             ciphertext[sizeof(T)];  // encrypted payload

    // Encrypt `plain` into this packet using the established channel.
    // Header must already be set before calling seal().
    bool seal(class SecureChannel& ch, const T& plain);

    // Decrypt ciphertext into `out` (server-side / test helper).
    bool open(class SecureChannel& ch, T& out) const;

    // Total data size AFTER header (what goes into Send's data_size param)
    static constexpr u32 dataSize() {
        return kKeySize + kMacSize + sizeof(T);
    }
};
#pragma pack(pop)

// =============================================================================
// SecureMsg<T> — Counter-based AEAD for ongoing messages (heartbeat etc.)
// =============================================================================
// Wire layout:
//   [SCommandHeader:4] [msg_counter:4] [mac:16] [ciphertext:sizeof(T)]
//
// Each message uses a unique nonce derived from the base session nonce,
// a monotonic counter, and a direction byte (0=client→server, 1=server→client).
// This prevents nonce reuse across the session.
// =============================================================================
#pragma pack(push, 1)
template <typename T>
struct SecureMsg {
    SCommandHeader m_tHeader;      // plaintext, used as AEAD AD
    u32            msg_counter;    // monotonic counter (part of wire)
    u8             mac[kMacSize];  // 16 — AEAD auth tag
    u8             ciphertext[sizeof(T)]; // encrypted payload

    bool seal(class SecureChannel& ch, const T& plain);
    bool open(class SecureChannel& ch, T& out) const;

    static constexpr u32 dataSize() {
        return sizeof(u32) + kMacSize + sizeof(T);
    }
};
#pragma pack(pop)

// =============================================================================
// SecureChannel — Key management + AEAD operations
// =============================================================================
class SecureChannel {
public:
    SecureChannel();
    ~SecureChannel();

    SecureChannel(const SecureChannel&)            = delete;
    SecureChannel& operator=(const SecureChannel&) = delete;

    // ── Phase 1: ConnectAck processing ────────────────────────────────────

    bool onServerKeyReceived(const ServerKeyPayload& keys);

    // ── Phase 2: AEAD primitives (called by SecurePacket — fixed nonce) ───

    bool encrypt(const u8* plain, u32 plain_size,
                 const u8* ad, u32 ad_size,
                 u8* cipher, u8 mac[kMacSize]);

    bool decrypt(const u8* cipher, u32 cipher_size,
                 const u8* ad, u32 ad_size,
                 const u8 mac[kMacSize], u8* plain);

    // ── Counter-based AEAD (for ongoing messages — heartbeat etc.) ────────

    // Encrypt with per-message counter nonce. Returns counter used.
    u32  encryptMsg(const u8* plain, u32 plain_size,
                    const u8* ad, u32 ad_size,
                    u8* cipher, u8 mac[kMacSize]);

    // Decrypt with counter from wire (server→client direction).
    bool decryptMsg(u32 counter,
                    const u8* cipher, u32 cipher_size,
                    const u8* ad, u32 ad_size,
                    const u8 mac[kMacSize], u8* plain);

    // ── Challenge solving ─────────────────────────────────────────────────

    // Compute BLAKE2b-keyed(sharedKey, challenge_data) → 32-byte answer
    void solveChallenge(const u8* challenge_data, u32 challenge_size,
                        u8 answer[kKeySize]);

    // ── Accessors ─────────────────────────────────────────────────────────
    const u8* clientPublicKey() const { return clientPubkey_; }
    const u8* serverPublicKey() const { return serverPubkey_; }
    const u8* sessionKey()     const { return sharedKey_; }
    bool      isEstablished()  const { return established_; }

    // ── HWID ──────────────────────────────────────────────────────────────
    static void collectHwid(u8 hwid[kHwidSize]);

    // ── Singleton ─────────────────────────────────────────────────────────
    static SecureChannel& instance();

private:
    void generateEphemeralKeypair();
    void deriveSessionKey(const u8 server_x25519[kKeySize]);
    void deriveSessionNonce(const u8 server_x25519[kKeySize]);
    void deriveCounterNonce(u32 counter, u8 direction, u8 out[kNonceSize]);

    u8   clientSecret_[kKeySize]{};
    u8   clientPubkey_[kKeySize]{};
    u8   serverPubkey_[kKeySize]{};
    u8   sharedKey_[kKeySize]{};
    u8   nonce_[kNonceSize]{};          // base nonce (used for counter derivation)
    bool established_ = false;
    std::atomic<u32> sendCounter_{0};   // monotonic send counter
};

// =============================================================================
// SecurePacket<T> template implementation
// =============================================================================
template <typename T>
bool SecurePacket<T>::seal(SecureChannel& ch, const T& plain) {
    if (!ch.isEstablished()) return false;

    // Copy client ephemeral pubkey into packet (server needs it for DH)
    nocrtMemcpy(client_pubkey, ch.clientPublicKey(), kKeySize);

    // Encrypt: AD = header (authenticated but plaintext)
    return ch.encrypt(
        reinterpret_cast<const u8*>(&plain), sizeof(T),
        nullptr, 0,
        ciphertext, mac);
}

template <typename T>
bool SecurePacket<T>::open(SecureChannel& ch, T& out) const {
    if (!ch.isEstablished()) return false;

    return ch.decrypt(
        ciphertext, sizeof(T),
        nullptr, 0,
        mac, reinterpret_cast<u8*>(&out));
}

// =============================================================================
// SecureMsg<T> template implementation
// =============================================================================
template <typename T>
bool SecureMsg<T>::seal(SecureChannel& ch, const T& plain) {
    if (!ch.isEstablished()) return false;

    msg_counter = ch.encryptMsg(
        reinterpret_cast<const u8*>(&plain), sizeof(T),
        reinterpret_cast<const u8*>(&m_tHeader), sizeof(SCommandHeader),
        ciphertext, mac);
    return true;
}

template <typename T>
bool SecureMsg<T>::open(SecureChannel& ch, T& out) const {
    if (!ch.isEstablished()) return false;

    return ch.decryptMsg(
        msg_counter,
        ciphertext, sizeof(T),
        reinterpret_cast<const u8*>(&m_tHeader), sizeof(SCommandHeader),
        mac, reinterpret_cast<u8*>(&out));
}

} // namespace mg::game
