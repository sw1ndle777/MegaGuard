// =============================================================================
// CCrypt - RC5/RC6 cipher engine (ported from NetEngine::CCrypt)
// =============================================================================
// Modern C++23 implementation. All state is per-instance, no static variables.
// Safe for manually-mapped DLLs with no CRT init.
//
// Lives in mg::crypto to avoid conflict with the game's POD
// mg::game::CCrypt struct in structures.hpp.
// =============================================================================
#pragma once

#include "core/types.hpp"
#include <array>
#include <expected>

namespace mg::crypto {

// ── CCrypt ──────────────────────────────────────────────────────────────────

class CCrypt {
public:
    enum class ErrorType  { UNK_CHANGE_TYPE, INVALID_FUNCTIONS };
    enum class CryptType : u32 { CRYPT_NONE, CRYPT_RC5, CRYPT_RC5_SERIAL, CRYPT_RC6, CRYPT_RC6_SERIAL };

    // ── Member-function-pointer types for dispatch ───────────────────────
    using SetupFn   = void  (CCrypt::*)();
    using EncryptFn = bool  (CCrypt::*)(const u8*, u8*, i32);
    using DecryptFn = bool  (CCrypt::*)(const u8*, u8*, i32);

    struct VTab { SetupFn setup; EncryptFn enc; DecryptFn dec; };

    // ── Construction ─────────────────────────────────────────────────────

    explicit CCrypt(CryptType eType, i32 key);
    ~CCrypt() = default;
    CCrypt(const CCrypt&)            = delete;
    CCrypt& operator=(const CCrypt&) = delete;

    // ── Public API ───────────────────────────────────────────────────────

    void setup(i32 key) noexcept;
    std::expected<void, ErrorType> changeType(CryptType eType, i32 key);

    template <typename TIn, typename TOut>
    bool encrypt(const TIn* pvIn, TOut* pvOut, i32 iLen) noexcept
    {
        [[assume(m_pfnEncrypt != nullptr)]];
        return (this->*m_pfnEncrypt)(
            reinterpret_cast<const u8*>(pvIn),
            reinterpret_cast<u8*>(pvOut), iLen);
    }

    template <typename TIn, typename TOut>
    bool decrypt(const TIn* pvIn, TOut* pvOut, i32 iLen) noexcept
    {
        [[assume(m_pfnDecrypt != nullptr)]];
        return (this->*m_pfnDecrypt)(
            reinterpret_cast<const u8*>(pvIn),
            reinterpret_cast<u8*>(pvOut), iLen);
    }

private:
    // ── RC5 ──────────────────────────────────────────────────────────────
    void rc5KeySetup(const u8* K) noexcept;
    void rc5Setup() noexcept;
    bool rc5Decrypt32(const u8* in, u8* out, i32 len) noexcept;
    bool rc5Decrypt64(const u8* in, u8* out, i32 len) noexcept;
    bool rc5Decrypt(const u8* in, u8* out, i32 len) noexcept;
    bool rc5Encrypt32(const u8* in, u8* out, i32 len) noexcept;
    bool rc5Encrypt64(const u8* in, u8* out, i32 len) noexcept;
    bool rc5Encrypt(const u8* in, u8* out, i32 len) noexcept;

    // ── RC6 ──────────────────────────────────────────────────────────────
    void rc6KeySetup(const u8* inKey, u16 keyLen) noexcept;
    void rc6Setup() noexcept;
    bool rc6Decrypt(const u8* in, u8* out, i32 len) noexcept;
    bool rc6Encrypt(const u8* in, u8* out, i32 len) noexcept;

    // ── Instance data ────────────────────────────────────────────────────

    SetupFn   m_pfnSetup   = nullptr;
    EncryptFn m_pfnEncrypt = nullptr;
    DecryptFn m_pfnDecrypt = nullptr;

    std::array<u16, 26> m_usKeyRC5{};
    std::array<u32, 26> m_uiKeyRC5{};
    std::array<u32, 84> m_uiKeyRC6{};
    i32                 m_iSerialKey = 0;
};

// ── Utility: hex string → byte array ─────────────────────────────────────────

u8   hexCharToValue(char c) noexcept;
void hexToBytes(const char* hexStr, char* output, size_t byteCount) noexcept;

} // namespace mg::crypto
