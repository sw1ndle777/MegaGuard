// =============================================================================
// SecureChannel — Implementation
// =============================================================================
// Phase 1: ConnectAck — verify Ed25519 signature, extract plaintext connect data
// Phase 2: Session AEAD using ephemeral X25519 key (forward secrecy)
// =============================================================================
#include "pch.hpp"
#include "game/network/secure_channel.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "platform/memory.hpp"
#include "platform/syscall_cloner.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::game {

// =============================================================================
// Compiled-in trust anchor
// =============================================================================
// Server Ed25519 verification key — the ONLY trust anchor.
// The server proves ownership by signing its ephemeral X25519 key + connect data.
// Replace with your actual server key.
// Generate with:  crypto_eddsa_key_pair(sk, pk, seed);
// =============================================================================


const u8 kServerSignPubkey[kKeySize] = {
        0x0e, 0x41, 0x3e, 0x0e, 0x94, 0x5f, 0x7d, 0x36,
        0x0c, 0x49, 0x04, 0x68, 0xd9, 0xe3, 0x18, 0x57,
        0x80, 0xdb, 0x6c, 0x48, 0xcc, 0x50, 0x53, 0xd0,
        0x6b, 0x48, 0x0b, 0x29, 0x78, 0x19, 0x26, 0x97
};

// =============================================================================
// Singleton
// =============================================================================
SecureChannel* s_instance = nullptr;

SecureChannel& SecureChannel::instance() {
    return *s_instance;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================
SecureChannel::SecureChannel() {
    nocrtMemset(clientSecret_, 0, sizeof(clientSecret_));
    nocrtMemset(clientPubkey_, 0, sizeof(clientPubkey_));
    nocrtMemset(serverPubkey_, 0, sizeof(serverPubkey_));
    nocrtMemset(sharedKey_, 0, sizeof(sharedKey_));
    nocrtMemset(nonce_, 0, sizeof(nonce_));
    s_instance = this;
}

SecureChannel::~SecureChannel() {
    crypto_wipe(clientSecret_, sizeof(clientSecret_));
    crypto_wipe(sharedKey_, sizeof(sharedKey_));
    crypto_wipe(nonce_, sizeof(nonce_));
    established_ = false;
    if (s_instance == this) s_instance = nullptr;
}

// =============================================================================
// Phase 1 — ConnectAck: verify Ed25519 signature, extract connect data
// =============================================================================
bool SecureChannel::onServerKeyReceived(const ServerKeyPayload& keys)
{
    established_ = false;
    crypto_wipe(clientSecret_, sizeof(clientSecret_));
    crypto_wipe(sharedKey_, sizeof(sharedKey_));

    // ── 1. Verify the server's sign pubkey matches compiled-in trust anchor ──
    if (crypto_verify32(keys.sign_pubkey, kServerSignPubkey) != 0)
        return false;

    // ── 2. Verify Ed25519 signature over (x25519_pk || connect_data) ──
    // These fields are contiguous in memory starting at &keys.x25519_pubkey
    if (crypto_eddsa_check(keys.signature, keys.sign_pubkey,
                           keys.x25519_pubkey, kSignedMsgSize) != 0)
        return false;

    // ── 3. Store server's X25519 public key for Phase 2 ──
    nocrtMemcpy(serverPubkey_, keys.x25519_pubkey, kKeySize);

    // ── 4. Generate ephemeral keypair for session (authorize etc.) ──
    generateEphemeralKeypair();

    // ── 5. Derive session symmetric key + nonce (for SecurePacket AEAD) ──
    deriveSessionKey(keys.x25519_pubkey);
    deriveSessionNonce(keys.x25519_pubkey);

    // ── 6. Wipe ephemeral secret — baked into sharedKey_ now ──
    crypto_wipe(clientSecret_, sizeof(clientSecret_));

    established_ = true;
    return true;
}

// =============================================================================
// BLAKE2b CSPRNG — adapted from server's SecureRandomBlake2b::Generator
// =============================================================================
namespace {

struct Blake2bRng {
    static constexpr u32 kDigestSize = 64;
    static constexpr int kRounds     = 10;

    i64 stateCounter;
    i64 seedCounter;
    i64 counter;
    u8  state[kDigestSize];
    u8  seed[kDigestSize];

    void uint64ToLE(u64 n, u8* bs) {
        for (int i = 0; i < 8; ++i)
            bs[i] = static_cast<u8>(n >> (i * 8));
    }

    void addCounter(i64 seedVal) {
        u8 bytes[8];
        uint64ToLE(static_cast<u64>(seedVal), bytes);
        crypto_blake2b_ctx ctx;
        crypto_blake2b_init(&ctx, kDigestSize);
        crypto_blake2b_update(&ctx, bytes, sizeof(bytes));
        crypto_blake2b_update(&ctx, seed, kDigestSize);
        crypto_blake2b_final(&ctx, seed);
    }

    i64 nextCounterValue() {
        return ++counter;
    }

    void addSeedMaterial(const u8* inSeed, u32 length) {
        crypto_blake2b_ctx ctx;
        crypto_blake2b_init(&ctx, kDigestSize);
        crypto_blake2b_update(&ctx, inSeed, length);
        crypto_blake2b_update(&ctx, seed, kDigestSize);
        crypto_blake2b_final(&ctx, seed);
    }

    void addSeedMaterial(i64 rSeed) {
        addCounter(rSeed);
        crypto_blake2b_ctx ctx;
        crypto_blake2b_init(&ctx, kDigestSize);
        crypto_blake2b_update(&ctx, seed, kDigestSize);
        crypto_blake2b_final(&ctx, seed);
    }

    void cycleSeed() {
        crypto_blake2b_ctx ctx;
        crypto_blake2b_init(&ctx, kDigestSize);
        crypto_blake2b_update(&ctx, seed, kDigestSize);
        addCounter(seedCounter++);
        crypto_blake2b_update(&ctx, seed, kDigestSize);
        crypto_blake2b_final(&ctx, seed);
    }

    void generateState() {
        addCounter(stateCounter++);
        crypto_blake2b_ctx ctx;
        crypto_blake2b_init(&ctx, kDigestSize);
        crypto_blake2b_update(&ctx, state, kDigestSize);
        crypto_blake2b_update(&ctx, seed, kDigestSize);
        crypto_blake2b_final(&ctx, state);
        if ((stateCounter % kRounds) == 0) cycleSeed();
    }

    void init() {
        stateCounter = 1;
        seedCounter  = 0;
        nocrtMemset(state, 0, kDigestSize);
        nocrtMemset(seed, 0, kDigestSize);

        LARGE_INTEGER qpc;
        QueryPerformanceCounter(&qpc);
        counter = qpc.QuadPart;

        seedCounter = 2;
        addSeedMaterial(nextCounterValue());

        u8 entropy[kDigestSize]{};
        u64 tsc = __rdtsc();
        auto ptr = reinterpret_cast<uptr>(entropy);
        for (u32 i = 0; i < kDigestSize; i += 8) {
            u64 val = (i % 16 == 0) ? tsc : static_cast<u64>(ptr + i);
            nocrtMemcpy(entropy + i, &val, 8);
        }
        addSeedMaterial(entropy, kDigestSize);

        // Mix in kernel entropy via cloned NtQuerySystemInformation
        using NtQSI_t = NTSTATUS(NTAPI*)(ULONG, PVOID, ULONG, PULONG);
        auto ntqsiName = MG_STR("NtQuerySystemInformation");
        auto NtQSI = reinterpret_cast<NtQSI_t>(
            mg::ctx().syscallCloner().cloneSyscallByName<void*>(ntqsiName));
        if (NtQSI) {
            u8 perfInfo[312]{};
            ULONG retLen = 0;
            NtQSI(2 /* SystemPerformanceInformation */, perfInfo,
                  sizeof(perfInfo), &retLen);
            addSeedMaterial(perfInfo, retLen > 0 ? retLen : sizeof(perfInfo));
            crypto_wipe(perfInfo, sizeof(perfInfo));
        }

        // Mix in process/thread context
        DWORD pid = GetCurrentProcessId();
        DWORD tid = GetCurrentThreadId();
        u8 ctxBuf[16]{};
        nocrtMemcpy(ctxBuf, &pid, 4);
        nocrtMemcpy(ctxBuf + 4, &tid, 4);
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        nocrtMemcpy(ctxBuf + 8, &ft, 8);
        addSeedMaterial(ctxBuf, sizeof(ctxBuf));

        crypto_wipe(entropy, sizeof(entropy));
    }

    void nextBytes(u8* bytes, u32 length) {
        if (seedCounter == 0) {
            addSeedMaterial(nextCounterValue());
            seedCounter = 1;
        }

        u32 stateOff = 0;
        generateState();

        for (u32 i = 0; i < length; ++i) {
            if (stateOff == kDigestSize) {
                generateState();
                stateOff = 0;
            }
            bytes[i] = state[stateOff++];
        }
    }

    void wipe() {
        crypto_wipe(state, kDigestSize);
        crypto_wipe(seed, kDigestSize);
    }
};

} // anonymous namespace

// =============================================================================
// Ephemeral keypair generation (for Phase 2)
// =============================================================================
void SecureChannel::generateEphemeralKeypair() {
    Blake2bRng rng;
    rng.init();
    rng.nextBytes(clientSecret_, kKeySize);
    rng.wipe();

    crypto_x25519_public_key(clientPubkey_, clientSecret_);
}

// =============================================================================
// Session key derivation (from ephemeral DH — forward secrecy)
// =============================================================================
void SecureChannel::deriveSessionKey(const u8 server_x25519[kKeySize]) {
    // Raw DH: X25519(client_ephemeral_secret, server_public)
    u8 rawShared[kKeySize];
    crypto_x25519(rawShared, clientSecret_, server_x25519);

    // Key derivation: BLAKE2b keyed hash
    const char* label = MG_STR("MegaGuard-AEAD-Key-v1");
    crypto_blake2b_keyed(sharedKey_, kKeySize,
                         rawShared, kKeySize,
                         reinterpret_cast<const u8*>(label), 21);

    crypto_wipe(rawShared, sizeof(rawShared));
}

void SecureChannel::deriveSessionNonce(const u8 server_x25519[kKeySize]) {
    // Nonce = BLAKE2b(client_ephemeral_pk || server_pk), first 24 bytes
    u8 nonceInput[kKeySize * 2];
    nocrtMemcpy(nonceInput, clientPubkey_, kKeySize);
    nocrtMemcpy(nonceInput + kKeySize, server_x25519, kKeySize);

    u8 fullHash[32];
    crypto_blake2b(fullHash, 32, nonceInput, sizeof(nonceInput));
    nocrtMemcpy(nonce_, fullHash, kNonceSize);

    crypto_wipe(nonceInput, sizeof(nonceInput));
    crypto_wipe(fullHash, sizeof(fullHash));
}

// =============================================================================
// Phase 2 — AEAD encrypt / decrypt (session key)
// =============================================================================
bool SecureChannel::encrypt(const u8* plain, u32 plain_size,
                            const u8* ad, u32 ad_size,
                            u8* cipher, u8 mac[kMacSize])
{
    if (!established_) return false;

    crypto_aead_lock(cipher, mac, sharedKey_, nonce_,
                     ad, ad_size,
                     plain, plain_size);
    return true;
}

bool SecureChannel::decrypt(const u8* cipher, u32 cipher_size,
                            const u8* ad, u32 ad_size,
                            const u8 mac[kMacSize], u8* plain)
{
    if (!established_) return false;

    return crypto_aead_unlock(plain, mac, sharedKey_, nonce_,
                              ad, ad_size,
                              cipher, cipher_size) == 0;
}

// =============================================================================
// Counter-based nonce derivation
// =============================================================================
// nonce = BLAKE2b(base_nonce || counter_LE || direction_byte)[0:24]
// direction: 0 = client→server, 1 = server→client
// =============================================================================
void SecureChannel::deriveCounterNonce(u32 counter, u8 direction, u8 out[kNonceSize])
{
    u8 input[kNonceSize + sizeof(u32) + 1];
    nocrtMemcpy(input, nonce_, kNonceSize);
    nocrtMemcpy(input + kNonceSize, &counter, sizeof(u32));
    input[kNonceSize + sizeof(u32)] = direction;

    u8 fullHash[32];
    crypto_blake2b(fullHash, 32, input, sizeof(input));
    nocrtMemcpy(out, fullHash, kNonceSize);

    crypto_wipe(input, sizeof(input));
    crypto_wipe(fullHash, sizeof(fullHash));
}

// =============================================================================
// Counter-based AEAD encrypt (client→server)
// =============================================================================
u32 SecureChannel::encryptMsg(const u8* plain, u32 plain_size,
                               const u8* ad, u32 ad_size,
                               u8* cipher, u8 mac[kMacSize])
{
    u32 counter = sendCounter_.fetch_add(1, std::memory_order_relaxed);

    u8 msgNonce[kNonceSize];
    deriveCounterNonce(counter, 0, msgNonce); // 0 = client→server

    crypto_aead_lock(cipher, mac, sharedKey_, msgNonce,
                     ad, ad_size,
                     plain, plain_size);

    crypto_wipe(msgNonce, sizeof(msgNonce));
    return counter;
}

// =============================================================================
// Counter-based AEAD decrypt (server→client)
// =============================================================================
bool SecureChannel::decryptMsg(u32 counter,
                                const u8* cipher, u32 cipher_size,
                                const u8* ad, u32 ad_size,
                                const u8 mac[kMacSize], u8* plain)
{
    if (!established_) return false;

    u8 msgNonce[kNonceSize];
    deriveCounterNonce(counter, 1, msgNonce); // 1 = server→client

    int result = crypto_aead_unlock(plain, mac, sharedKey_, msgNonce,
                                    ad, ad_size,
                                    cipher, cipher_size);

    crypto_wipe(msgNonce, sizeof(msgNonce));
    return result == 0;
}

// =============================================================================
// Challenge solver — BLAKE2b-keyed(sharedKey, challenge_data) → 32B answer
// =============================================================================
void SecureChannel::solveChallenge(const u8* challenge_data, u32 challenge_size,
                                    u8 answer[kKeySize])
{
    crypto_blake2b_keyed(answer, kKeySize,
                         sharedKey_, kKeySize,
                         challenge_data, challenge_size);
}

// =============================================================================
// HWID collection — BLAKE2b-256 of hardware fingerprint
// =============================================================================
void SecureChannel::collectHwid(u8 hwid[kHwidSize]) {
    crypto_blake2b_ctx ctx;
    crypto_blake2b_init(&ctx, kHwidSize);

    // 1. Machine GUID from registry
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      MG_STR("SOFTWARE\\Microsoft\\Cryptography"),
                      0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS)
    {
        char guid[128]{};
        DWORD guidSize = sizeof(guid);
        DWORD type = 0;
        if (RegQueryValueExA(hKey, MG_STR("MachineGuid"), nullptr,
                             &type, reinterpret_cast<LPBYTE>(guid),
                             &guidSize) == ERROR_SUCCESS)
        {
            crypto_blake2b_update(&ctx,
                reinterpret_cast<const u8*>(guid), guidSize);
        }
        RegCloseKey(hKey);
    }

    // 2. Volume serial number (system drive)
    char sysDir[MAX_PATH]{};
    GetSystemDirectoryA(sysDir, MAX_PATH);
    char rootPath[4] = { sysDir[0], ':', '\\', '\0' };
    DWORD volumeSerial = 0;
    GetVolumeInformationA(rootPath, nullptr, 0, &volumeSerial,
                          nullptr, nullptr, nullptr, 0);
    crypto_blake2b_update(&ctx,
        reinterpret_cast<const u8*>(&volumeSerial), sizeof(volumeSerial));

    // 3. Computer name
    char compName[MAX_COMPUTERNAME_LENGTH + 1]{};
    DWORD compSize = sizeof(compName);
    GetComputerNameA(compName, &compSize);
    crypto_blake2b_update(&ctx,
        reinterpret_cast<const u8*>(compName), compSize);

    // 4. CPUID brand string
    int cpuInfo[4]{};
    __cpuid(cpuInfo, 0x80000002);
    crypto_blake2b_update(&ctx,
        reinterpret_cast<const u8*>(cpuInfo), sizeof(cpuInfo));
    __cpuid(cpuInfo, 0x80000003);
    crypto_blake2b_update(&ctx,
        reinterpret_cast<const u8*>(cpuInfo), sizeof(cpuInfo));
    __cpuid(cpuInfo, 0x80000004);
    crypto_blake2b_update(&ctx,
        reinterpret_cast<const u8*>(cpuInfo), sizeof(cpuInfo));

    // 5. Processor count + page size
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    crypto_blake2b_update(&ctx,
        reinterpret_cast<const u8*>(&si.dwNumberOfProcessors),
        sizeof(si.dwNumberOfProcessors));
    crypto_blake2b_update(&ctx,
        reinterpret_cast<const u8*>(&si.dwPageSize),
        sizeof(si.dwPageSize));

    crypto_blake2b_final(&ctx, hwid);
}

} // namespace mg::game
