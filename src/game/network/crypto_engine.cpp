// =============================================================================
// CryptoEngine - Full implementation ported from ccrypt.h
// =============================================================================
// RC5/RC6 key setup, CCrypt_Encrypt passthrough, CCrypt_Decrypt with
// TCP header decryption on specific return address match.
// =============================================================================
#include "pch.hpp"
#include "game/network/crypto_engine.hpp"
#include "game/network/ccrypt.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"
#include "engine/hook_id.hpp"
#include "platform/memory.hpp"

namespace mg::game {

// ── Return-address helper ─────────────────────────────────────────────────────
#if defined(__clang__) && defined(_MSC_VER)
    #define MG_RETURN_ADDRESS() reinterpret_cast<u32>(__builtin_return_address(0))
#elif defined(_MSC_VER)
    #define MG_RETURN_ADDRESS() reinterpret_cast<u32>(_ReturnAddress())
#elif defined(__clang__) || defined(__GNUC__)
    #define MG_RETURN_ADDRESS() reinterpret_cast<u32>(__builtin_return_address(0))
#endif

namespace {

// ── RC5/RC6 key setup hooks ──────────────────────────────────────────────────

void __fastcall hkRC5KeySetup(CCrypt* instance, u32 /*edx*/)
{
    mg::call<void(__thiscall*)(CCrypt*, const u8*)>(
        MG_CONST(addr::anticheat::crypto::rc5::InternalKeySetup),
        instance,
        addr::anticheat::crypto::rc5::K);
}

void __fastcall hkRC6KeySetup(CCrypt* instance, u32 /*edx*/)
{
    mg::call<void(__thiscall*)(CCrypt*, const u8*, u16)>(
        MG_CONST(addr::anticheat::crypto::rc6::InternalKeySetup),
        instance,
        addr::anticheat::crypto::rc6::K,
        32);
}

// ── CCrypt Encrypt — direct passthrough to m_pbfEncrypt ──────────────────────

void __fastcall hkCCryptEncrypt(CCrypt* instance, u32 /*edx*/,
                                        void* pvIn, void* pvOut, i32 iLen)
{
    instance->m_pbfEncrypt(instance, pvIn, pvOut, iLen);
}

// ── CCrypt Decrypt — passthrough + TCP header decryption ─────────────────────

void __fastcall hkCCryptDecrypt(CCrypt* instance, u32 /*edx*/,
                                        void* pvIn, void* pvOut, i32 iLen)
{
    auto return_address = MG_RETURN_ADDRESS();

    instance->m_pbfDecrypt(instance, pvIn, pvOut, iLen);

    // When the decryption is called from the specific return address
    // and the data is exactly 4 bytes (TCP header), apply our custom
    // header decryption to undo the additional obfuscation layer
    if (return_address == MG_CONST(addr::anticheat::crypto::DecryptReturnAddr) && iLen == 4)
    {
        *reinterpret_cast<u32*>(pvOut) = decryptTcpHeader(
            *reinterpret_cast<u32*>(pvOut));
    }
}

} // anonymous namespace

// ── Password constants ───────────────────────────────────────────────────────

namespace {

constexpr const char* kArchiveloaderPwHex =
    "45F6256E282EB3505800B7B7405236BA6BDA673B112EE25760F16DA8ADA61610"
    "E21544E9CD01F7BFE9E98B0D2F9AC48DAA7C57D4FECC173ADA3EF2A1FEECEEA1";

constexpr const char* kCgdPw =
    "02239E046913704B4B17BCDF5CF95FFD4D0524A29A232AE2BAE827620B350E89"
    "FAA58BF3D20DDBB9574841426EDEA58DF342C43203CA83EEF602F9242B41DE93";

} // anonymous namespace

// ── CryptoEngine class ───────────────────────────────────────────────────────

CryptoEngine::CryptoEngine(MegaGuardContext& ctx) : ctx_(ctx) {}
CryptoEngine::~CryptoEngine() = default;

VoidResult CryptoEngine::install() {
    auto& registry = ctx_.hookRegistry();

    // Hook RC5 and RC6 key setup functions
    registry.registerDetour(HookId::RC5KeySetup)
        .create(MG_CONST(addr::anticheat::crypto::rc5::KeySetup), hkRC5KeySetup);
    registry.registerDetour(HookId::RC6KeySetup)
        .create(MG_CONST(addr::anticheat::crypto::rc6::KeySetup), hkRC6KeySetup);

    // Hook CCrypt::Encrypt and CCrypt::Decrypt vtable entries
    registry.registerDetour(HookId::CryptEncrypt)
        .create(MG_CONST(addr::anticheat::crypto::Encrypt), hkCCryptEncrypt);
    registry.registerDetour(HookId::CryptDecrypt)
        .create(MG_CONST(addr::anticheat::crypto::Decrypt), hkCCryptDecrypt);

    // ── Write CGD passwords to game memory ──────────────────────────────
    nocrtStrcpy(reinterpret_cast<char*>(MG_CONST(addr::anticheat::crypto::cgd::static_pw1)), kCgdPw);
    nocrtStrcpy(reinterpret_cast<char*>(MG_CONST(addr::anticheat::crypto::cgd::static_pw2)), kCgdPw);

    // ── Decrypt and write archiveloader password ────────────────────────
    {
        char archiveloaderPw[65] = { 0 };

        // Convert hex string to raw bytes
        crypto::hexToBytes(kArchiveloaderPwHex, archiveloaderPw, 64);

        // Decrypt using RC6 with key=0 (matches original InitAntiCheatHooks)
        crypto::CCrypt crypt(crypto::CCrypt::CryptType::CRYPT_RC6, 0);
        crypt.decrypt(archiveloaderPw, archiveloaderPw, 64);
        archiveloaderPw[64] = '\0';

        // Write decrypted password to game's archiveloader_pw OldString
        *reinterpret_cast<OldString*>(MG_CONST(addr::anticheat::crypto::archiveloader_pw)) = archiveloaderPw;

        // Zero the stack buffer
        nocrtMemset(archiveloaderPw, 0, 64);
    }

    return VoidResult::ok();
}

// ── TCP header obfuscation (matches original ccrypt.h exactly) ───────────────

u32 encryptTcpHeader(u32 header)
{
    header = ~header;                                                       // Bitwise NOT
    header = (header << 13) | (header >> (32 - 13));                        // ROT left 13
    header ^= 0xA5A5A5A5;                                                  // XOR constant
    header = ((header & 0xF0F0F0F0) >> 4) | ((header & 0x0F0F0F0F) << 4);  // Nibble swap
    header += 0xCAFEBABE;                                                   // Add for diffusion
    header = (header << 7) | (header >> (32 - 7));                          // ROT left 7
    header ^= (header >> 16);                                               // XOR mix upper/lower
    header = ((header & 0xAAAAAAAA) >> 1) | ((header & 0x55555555) << 1);   // Alternating bit swap
    return header;
}

u32 decryptTcpHeader(u32 encrypted)
{
    encrypted = ((encrypted & 0xAAAAAAAA) >> 1) | ((encrypted & 0x55555555) << 1);
    encrypted ^= (encrypted >> 16);
    encrypted = (encrypted >> 7) | (encrypted << (32 - 7));
    encrypted -= 0xCAFEBABE;
    encrypted = ((encrypted & 0xF0F0F0F0) >> 4) | ((encrypted & 0x0F0F0F0F) << 4);
    encrypted ^= 0xA5A5A5A5;
    encrypted = (encrypted >> 13) | (encrypted << (32 - 13));
    encrypted = ~encrypted;
    return encrypted;
}

} // namespace mg::game
