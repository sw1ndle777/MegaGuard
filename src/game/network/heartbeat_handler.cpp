// =============================================================================
// HeartbeatHandler — Challenge-response + detection reporting
// =============================================================================
// File integrity is computed here but sent during MainAuthorize.
// Heartbeat carries: challenge answer + detection bits + queued events.
// =============================================================================
#include "pch.hpp"
#include "game/network/heartbeat_handler.hpp"
#include "game/network/secure_channel.hpp"
#include "anticheat/detection_report.hpp"
#include "anticheat/detection_engine.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "platform/memory.hpp"
#include "utils/logger.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::game {

// =============================================================================
// File integrity — BLAKE2b-256 per-file hashing
// =============================================================================
namespace {

constexpr u32 kHashBufSize = 8192;

bool hashFileInto(u8 out[32], const char* path)
{
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        nocrtMemset(out, 0, 32);
        return false;
    }

    crypto_blake2b_ctx ctx;
    crypto_blake2b_init(&ctx, 32);

    u8 buf[kHashBufSize];
    DWORD bytesRead = 0;
    while (ReadFile(hFile, buf, kHashBufSize, &bytesRead, nullptr) && bytesRead > 0)
        crypto_blake2b_update(&ctx, buf, bytesRead);

    CloseHandle(hFile);
    crypto_blake2b_final(&ctx, out);
    return true;
}

void buildPath(char* dst, const char* base, const char* file)
{
    nocrtStrcpy(dst, base);
    nocrtStrcat(dst, "\\");
    nocrtStrcat(dst, file);
}

// ── Release directory file list (31 files) ──────────────────────────────────
const char* const kReleaseFiles[] = {
    "bdvid32.dll",
    "cudart32_30_9.dll",
    "D3DX10d_39.dll",
    "d3dx10_37.dll",
    "d3dx10_39.dll",
    "d3dx10_40.dll",
    "d3dx10_41.dll",
    "d3dx10_42.dll",
    "D3dx9d_39.dll",
    "d3dx9_31.dll",
    "d3dx9_33.dll",
    "D3DX9_37.dll",
    "d3dx9_38.dll",
    "D3DX9_39.dll",
    "D3DX9_40.dll",
    "D3DX9_41.dll",
    "D3DX9_42.dll",
    "gdiplus.dll",
    "megaguard.dll",
    "MegaVolts.exe",
    "Microsoft.VC90.CRT.manifest",
    "mss32.dll",
    "msvcm90.dll",
    "msvcp90.dll",
    "msvcr90.dll",
    "NxCharacter.dll",
    "PhysXCooking.dll",
    "PhysXCore.dll",
    "PhysXDevice.dll",
    "PhysXLoader.dll",
    "steam_api.dll",
};
constexpr u32 kReleaseFileCount = sizeof(kReleaseFiles) / sizeof(kReleaseFiles[0]);

void getBasePath(char* basePath)
{
    GetModuleFileNameA(nullptr, basePath, MAX_PATH);

    char* lastSlash = nullptr;
    for (char* p = basePath; *p; ++p)
        if (*p == '\\' || *p == '/') lastSlash = p;
    if (lastSlash) *lastSlash = '\0';

    lastSlash = nullptr;
    for (char* p = basePath; *p; ++p)
        if (*p == '\\' || *p == '/') lastSlash = p;
    if (lastSlash) *lastSlash = '\0';
}

// ── Detection bitfield with noise ───────────────────────────────────────────

void buildDetectionBits(u8 out[kDetectionBitsSize],
                        u32 detectionFlags,
                        u64 challengeId,
                        const u8 sessionKey[kKeySize])
{
    u8 noiseKey[kDetectionBitsSize];
    {
        u8 input[sizeof(u64) + kKeySize];
        nocrtMemcpy(input, &challengeId, sizeof(u64));
        nocrtMemcpy(input + sizeof(u64), sessionKey, kKeySize);

        crypto_blake2b_keyed(noiseKey, kDetectionBitsSize,
                             kNoiseSeed, sizeof(kNoiseSeed),
                             input, sizeof(input));
        crypto_wipe(input, sizeof(input));
    }

    nocrtMemcpy(out, noiseKey, kDetectionBitsSize);

    for (u32 i = 0; i < kDetectionFlagCount; ++i)
    {
        u32 bitPos = kDetectionBitMap[i];
        u32 byteIdx = bitPos / 8;
        u8  mask = static_cast<u8>(1u << (bitPos % 8));

        out[byteIdx] &= ~mask;
        if (detectionFlags & (1u << i))
            out[byteIdx] |= mask;
    }
}

// ── Wire format for heartbeat messages (matches server HeartbeatWireData) ───

template<typename T>
struct HeartbeatWireData {
    u8 nonce[kNonceSize];
    u8 mac[kMacSize];
    u8 ciphertext[sizeof(T)];
};

// ── Send helpers ────────────────────────────────────────────────────────────

using tGetNetMgr = CNetMgr* (__cdecl*)();

// ── Heartbeat hook callback ────────────────────────────────────────────────

void __cdecl hkHeartbeat(u32 data)
{
    auto header = *reinterpret_cast<SCommandHeader*>(data);

    // ── Secure heartbeat challenge (extra == 1) ─────────────────────────
    if (header.extra == 1)
    {
        auto& channel = SecureChannel::instance();
        if (!channel.isEstablished())
        {
            MG_LOG(mg::ctx().logger(),
                "[Heartbeat] Challenge received but channel not established");
            return;
        }

        auto* wire = reinterpret_cast<HeartbeatWireData<HeartbeatChallenge>*>(
            data + sizeof(SCommandHeader));

        HeartbeatChallenge challenge{};
        if (crypto_aead_unlock(
                reinterpret_cast<u8*>(&challenge), wire->mac,
                channel.sessionKey(), wire->nonce,
                nullptr, 0,
                wire->ciphertext, sizeof(HeartbeatChallenge)) != 0)
        {
            MG_LOG(mg::ctx().logger(), "[Heartbeat] Failed to decrypt challenge");
            return;
        }

        //MG_LOG(mg::ctx().logger(),
        //    "[Heartbeat] Challenge received: id=0x{:016X}", challenge.challenge_id);

        // ── Build response ──────────────────────────────────────────────

        HeartbeatResponse response{};
        response.challenge_id = challenge.challenge_id;

        // 1. Solve challenge
        channel.solveChallenge(challenge.challenge_data,
                               sizeof(challenge.challenge_data),
                               response.challenge_answer);

        // 2. Detection flags — drain from DetectionReport
        auto& hbHandler = mg::ctx().heartbeatHandler();
        u32 flags = hbHandler.detectionReport().drainFlags();

        // Also merge scanner flags if detection engine is active
        // flags |= mg::ctx().detectionEngine().exchangeFlags();

        buildDetectionBits(response.detection_bits, flags,
                           challenge.challenge_id, channel.sessionKey());

        // 3. Queued detection events — drain into response
        response.event_count = static_cast<u8>(
            hbHandler.detectionReport().drainEvents(
                response.events, kMaxHeartbeatEventsPerPacket));

        // ── Encrypt and send response ───────────────────────────────────

        struct {
            SCommandHeader header;
            HeartbeatWireData<HeartbeatResponse> wire;
        } pkt{};
        pkt.header.order  = 81;
        pkt.header.extra  = 38;
        pkt.header.option = 2;

        // Generate unique nonce from challenge_id + entropy + session key
        {
            u8 nonceInput[sizeof(u64) + sizeof(u64) + kKeySize];
            nocrtMemcpy(nonceInput, &response.challenge_id, sizeof(u64));
            u64 tsc = __rdtsc();
            nocrtMemcpy(nonceInput + sizeof(u64), &tsc, sizeof(u64));
            nocrtMemcpy(nonceInput + sizeof(u64) * 2, channel.sessionKey(), kKeySize);
            u8 hash[32];
            crypto_blake2b(hash, 32, nonceInput, sizeof(nonceInput));
            nocrtMemcpy(pkt.wire.nonce, hash, kNonceSize);
            crypto_wipe(nonceInput, sizeof(nonceInput));
            crypto_wipe(hash, sizeof(hash));
        }

        crypto_aead_lock(pkt.wire.ciphertext, pkt.wire.mac,
                         channel.sessionKey(), pkt.wire.nonce,
                         nullptr, 0,
                         reinterpret_cast<const u8*>(&response),
                         sizeof(HeartbeatResponse));

        auto GetNetMgr = reinterpret_cast<tGetNetMgr>(
            MG_CONST(addr::anticheat::game_managers::net_mgr::Get));
        auto* netMgr = GetNetMgr();

        if (netMgr && netMgr->CMainConnectorTcp)
        {
            netMgr->CMainConnectorTcp->_vptr_CConnector->Send(
                netMgr->CMainConnectorTcp,
                reinterpret_cast<char*>(&pkt),
                static_cast<int>(sizeof(HeartbeatWireData<HeartbeatResponse>)), 0, 0);

            // MG_LOG(mg::ctx().logger(),
            //     "[Heartbeat] Response sent: challenge=0x{:016X} events={}",
            //     challenge.challenge_id, response.event_count);
        }

        crypto_wipe(&response, sizeof(response));
        crypto_wipe(&challenge, sizeof(challenge));
    }
}

} // anonymous namespace

// =============================================================================
// File integrity — public, called from AuthorizeHandler
// =============================================================================
// Computes individual BLAKE2b-256 hash of each game file.
// Index 0: Launcher.exe, 1: map.dat, 2: cgd.dip, 3..33: Release files
// =============================================================================
void computeFileIntegrity(FileIntegrityPayload& out)
{
    char basePath[MAX_PATH]{};
    getBasePath(basePath);
    char path[MAX_PATH];

    u32 idx = 0;

    // [0] Launcher.exe
    /*
    buildPath(path, basePath, MG_STR("Launcher.exe"));
    hashFileInto(out.hashes[idx++], path);
    */

    // [0] Data/map.dat
    {
        char dataDir[MAX_PATH];
        buildPath(dataDir, basePath, MG_STR("Data"));

        buildPath(path, dataDir, MG_STR("map.dat"));
        hashFileInto(out.hashes[idx++], path);

        // [1] Data/cgd.dip
        buildPath(path, dataDir, MG_STR("cgd.dip"));
        hashFileInto(out.hashes[idx++], path);
    }

    // [2..32] Release files (31 files)
    {
        char releaseDir[MAX_PATH];
        buildPath(releaseDir, basePath, MG_STR("Release"));

        for (u32 i = 0; i < kReleaseFileCount; ++i)
        {
            buildPath(path, releaseDir, kReleaseFiles[i]);
            hashFileInto(out.hashes[idx++], path);
        }
    }
}

// =============================================================================
// HeartbeatHandler::Impl
// =============================================================================
struct HeartbeatHandler::Impl {
    DetectionReport report;
};

// ── HeartbeatHandler class ───────────────────────────────────────────────────

HeartbeatHandler::HeartbeatHandler(MegaGuardContext& ctx)
    : ctx_(ctx)
    , impl_(nullptr)
{
    void* mem = VirtualAlloc(nullptr, sizeof(Impl), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (mem) impl_ = new (mem) Impl();
}

HeartbeatHandler::~HeartbeatHandler()
{
    if (impl_) {
        impl_->~Impl();
        VirtualFree(impl_, 0, MEM_RELEASE);
        impl_ = nullptr;
    }
}

VoidResult HeartbeatHandler::install() {
    auto& registry = ctx_.hookRegistry();
    registry.registerDetour(HookId::Heartbeat)
        .create(MG_CONST(addr::anticheat::heartbeat::Init), hkHeartbeat);


    return VoidResult::ok();
}

DetectionReport& HeartbeatHandler::detectionReport() {
    return impl_->report;
}

// =============================================================================
// Legacy heartbeat crypto (kept for backwards compatibility)
// =============================================================================

u64 generateKey(const u8* buffer, std::size_t size)
{
    u64 key = 0x123456789ABCDEF0ULL;
    std::size_t i = 0;
    while (i < size)
    {
        key ^= buffer[i];
        key = (key << 7) | (key >> (64 - 7));
        key += (static_cast<u64>(buffer[i]) << (i % 8));
        key = ~((key << 11) | (key >> (64 - 11)));
        key ^= 0x5A5A5A5A5A5A5A5AULL;
        key = (key << 3) | (key >> (64 - 3));
        key += buffer[i];
        key ^= buffer[i] ^ (key >> (i % 64));
        ++i;
    }
    return key;
}

u64 encryptKey(u64 key)
{
    u64 result = 0;
    std::size_t i = 0;
    while (i < 8) {
        u8 byte = static_cast<u8>(key & 0xFF);
        byte ^= 0xAA;
        byte = (byte << 4) | (byte >> 4);
        byte ^= 0x55;
        result |= (static_cast<u64>(byte) << (i * 8));
        key = (key >> 8);
        ++i;
    }
    return result;
}

u64 decryptKey(u64 key)
{
    u64 result = 0;
    std::size_t i = 0;
    while (i < 8) {
        u8 byte = static_cast<u8>((key >> (i * 8)) & 0xFF);
        byte ^= 0x55;
        byte = (byte >> 4) | (byte << 4);
        byte ^= 0xAA;
        result |= (static_cast<u64>(byte) << (i * 8));
        ++i;
    }
    return result;
}

void xorEncrypt(u8* buffer, std::size_t size, u64 key)
{
    std::size_t i = 0;
    while (i < size)
    {
        buffer[i] ^= static_cast<u8>(key);
        buffer[i] = ~buffer[i];
        buffer[i] = (buffer[i] << 3) | (buffer[i] >> (8 - 3));
        buffer[i] ^= static_cast<u8>((key >> 4) & 0xFF);
        buffer[i] = (buffer[i] & static_cast<u8>(key)) | (~buffer[i] & ~static_cast<u8>(key));
        buffer[i] = (buffer[i] << 2) | (buffer[i] >> (8 - 2));
        buffer[i] ^= static_cast<u8>((key >> 8) & 0xFF);
        buffer[i] = ((buffer[i] & 0xF0) >> 4) | ((buffer[i] & 0x0F) << 4);
        buffer[i] = ((buffer[i] & 0xCC) >> 2) | ((buffer[i] & 0x33) << 2);
        i++;
    }
}

void xorDecrypt(u8* buffer, std::size_t size, u64 key)
{
    std::size_t i = 0;
    while (i < size)
    {
        buffer[i] = ((buffer[i] & 0xCC) >> 2) | ((buffer[i] & 0x33) << 2);
        buffer[i] = ((buffer[i] & 0xF0) >> 4) | ((buffer[i] & 0x0F) << 4);
        buffer[i] ^= static_cast<u8>((key >> 8) & 0xFF);
        buffer[i] = (buffer[i] >> 2) | (buffer[i] << (8 - 2));
        buffer[i] = (buffer[i] & static_cast<u8>(key)) | (~buffer[i] & ~static_cast<u8>(key));
        buffer[i] ^= static_cast<u8>((key >> 4) & 0xFF);
        buffer[i] = (buffer[i] >> 3) | (buffer[i] << (8 - 3));
        buffer[i] = ~buffer[i];
        buffer[i] ^= static_cast<u8>(key);
        i++;
    }
}

} // namespace mg::game
