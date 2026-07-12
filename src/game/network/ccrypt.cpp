// =============================================================================
// CCrypt - RC5/RC6 cipher engine implementation
// =============================================================================
#include "pch.hpp"
#include "game/network/ccrypt.hpp"
#include "platform/memory.hpp"

#include <bit>
#include <type_traits>

#pragma warning(disable: 28020)

namespace mg::crypto {

// ── Rotation helpers ─────────────────────────────────────────────────────────

template <class T>
[[nodiscard]] constexpr T rotl(T x, int s) noexcept
    requires (std::is_unsigned_v<T>)
{
    return std::rotl(x, s);
}

template <class T>
[[nodiscard]] constexpr T rotr(T x, int s) noexcept
    requires (std::is_unsigned_v<T>)
{
    return std::rotr(x, s);
}

// ── Cipher constants ─────────────────────────────────────────────────────────

inline constexpr u32 kRC5Tables  = 26;
inline constexpr u32 kRC5Round   = 12;
inline constexpr u32 kRC6Tables  = 84;
inline constexpr u32 kRC6LGW     = 5;
inline constexpr u32 kRC6R22     = 82;
inline constexpr u32 kRC6R23     = 83;
inline constexpr u32 kRC6Round   = 40;
inline constexpr u16 kP16        = 20835;         // 0x5163
inline constexpr u32 kP32        = 0xB7E15163;
inline constexpr u16 kQ16        = 31161;         // 0x79B9
inline constexpr u32 kQ32        = 0x9E3779B9;
inline constexpr size_t kRC5KeySize = 16;
inline constexpr size_t kRC6KeySize = 32;

// ── Compile-time keys (matching addresses.hpp rc5::K / rc6::K) ───────────────

consteval std::array<u8, kRC5KeySize> rc5Key() noexcept
{
    return { 0xc0, 0x58, 0x1e, 0x07, 0xd9, 0x39, 0x43, 0x12,
             0x31, 0xd0, 0xce, 0x21, 0xdd, 0xaf, 0x90, 0xad };
}

consteval std::array<u8, kRC6KeySize> rc6Key() noexcept
{
    return { 0x55, 0x35, 0x34, 0xb1, 0x9f, 0x85, 0x46, 0x23,
             0x46, 0x08, 0xb4, 0x75, 0xc3, 0xd4, 0x9e, 0x9c,
             0x66, 0x0d, 0xab, 0x76, 0x74, 0xe7, 0x74, 0xf1,
             0x35, 0x4b, 0x53, 0xc7, 0x4d, 0xe6, 0x69, 0xfe };
}

// ═════════════════════════════════════════════════════════════════════════════
//  CCrypt — construction & public API
// ═════════════════════════════════════════════════════════════════════════════

CCrypt::CCrypt(CryptType eType, i32 key)
{
    if (!changeType(eType, key)) [[unlikely]]
    {
        // An invalid cipher type slipped through: fall back to a known-good
        // cipher so the object stays in a defined state instead of invoking UB
        // via std::unreachable(), which would let the optimizer elide the range
        // check in changeType() and read kVtabs out of bounds.
        (void)changeType(CryptType::CRYPT_RC5, key);
    }
}

void CCrypt::setup(i32 key) noexcept
{
    m_iSerialKey = key;
    [[assume(m_pfnSetup != nullptr)]];
    (this->*m_pfnSetup)();
}

std::expected<void, CCrypt::ErrorType> CCrypt::changeType(CryptType eType, i32 key)
{
    constexpr VTab kVtabs[] = {
        /* CRYPT_NONE       */ { &CCrypt::rc5Setup,  &CCrypt::rc5Encrypt,  &CCrypt::rc5Decrypt },
        /* CRYPT_RC5        */ { &CCrypt::rc5Setup,  &CCrypt::rc5Encrypt,  &CCrypt::rc5Decrypt },
        /* CRYPT_RC5_SERIAL */ { &CCrypt::rc5Setup,  &CCrypt::rc5Encrypt,  &CCrypt::rc5Decrypt },
        /* CRYPT_RC6        */ { &CCrypt::rc6Setup,  &CCrypt::rc6Encrypt,  &CCrypt::rc6Decrypt },
        /* CRYPT_RC6_SERIAL */ { &CCrypt::rc6Setup,  &CCrypt::rc6Encrypt,  &CCrypt::rc6Decrypt },
    };

    const auto idx = static_cast<u32>(eType);
    if (idx >= std::size(kVtabs))
        return std::unexpected(ErrorType::UNK_CHANGE_TYPE);

    const auto& vt = kVtabs[idx];
    m_pfnSetup   = vt.setup;
    m_pfnEncrypt = vt.enc;
    m_pfnDecrypt = vt.dec;
    if (!m_pfnSetup || !m_pfnEncrypt || !m_pfnDecrypt)
        [[unlikely]] return std::unexpected(ErrorType::INVALID_FUNCTIONS);

    setup(key);
    return {};
}

// ═════════════════════════════════════════════════════════════════════════════
//  RC5
// ═════════════════════════════════════════════════════════════════════════════

void CCrypt::rc5KeySetup(const u8* K) noexcept
{
    constexpr u32 U = 4, ROT = 3, MIX = 3 * kRC5Tables;
    std::array<u32, U> L{};
    auto keyBytes = std::bit_cast<std::array<u8, 4>>(m_iSerialKey);

    for (int j = 15, idx = 0; j >= 0; --j, idx = (idx + 1) % 4)
        L[j / U] = (L[j / U] << 8) + static_cast<u32>(K[j] + static_cast<i8>(keyBytes[idx]));

    m_usKeyRC5[0] = m_uiKeyRC5[0] = kP16;
    for (u32 j = 1; j < kRC5Tables; ++j)
        m_usKeyRC5[j] = m_uiKeyRC5[j] = m_uiKeyRC5[j - 1] + kQ16;

    u32 A = 0, B = 0, j = 0, k = 0;
    for (u32 l = 0; l < MIX; ++l)
    {
        A = m_uiKeyRC5[j] = rotl<u32>(m_uiKeyRC5[j] + A + B, ROT);
        m_usKeyRC5[j] = static_cast<u16>(A);
        B = L[k] = rotl<u32>(L[k] + A + B, A + B);
        j = (j + 1) % (2 * 13);
        k = (k + 1) % U;
    }
}

void CCrypt::rc5Setup() noexcept
{
    constexpr auto key = rc5Key();
    rc5KeySetup(key.data());
}

// ── RC5 decrypt ──────────────────────────────────────────────────────────────

bool CCrypt::rc5Decrypt32(const u8* in, u8* out, i32 len) noexcept
{
    if (in != out) nocrtMemcpy(out, in, len);
    i32 rest = len;
    int i = 0;
    auto key = kRC5Tables;
    while (rest >= 4)
    {
        u16 A = static_cast<u16>(*reinterpret_cast<const u16*>(in + i * 2) ^ m_iSerialKey);
        u16 B = static_cast<u16>(*reinterpret_cast<const u16*>(in + (i + 1) * 2) ^ m_iSerialKey);
        for (u32 k = 0; k < kRC5Round; k++)
        {
            B = rotr<u16>(B - m_usKeyRC5[--key], A & 0xF) ^ A;
            A = rotr<u16>(A - m_usKeyRC5[--key], B & 0xF) ^ B;
        }
        *reinterpret_cast<u16*>(out + (i + 1) * 2) = B - m_usKeyRC5[--key];
        *reinterpret_cast<u16*>(out + i * 2)       = A - m_usKeyRC5[--key];
        i += 2;
        rest -= 4;
    }
    return true;
}

bool CCrypt::rc5Decrypt64(const u8* in, u8* out, i32 len) noexcept
{
    i32 rest = len;
    int i = 0;
    while (rest >= 8)
    {
        auto A = *reinterpret_cast<const u32*>(in + i * 4) ^ m_iSerialKey;
        auto B = *reinterpret_cast<const u32*>(in + (i + 1) * 4) ^ m_iSerialKey;
        for (int j = static_cast<int>(kRC5Round); j > 0; j--)
        {
            B = rotr<u32>(B - m_uiKeyRC5[2 * j + 1], A) ^ A;
            A = rotr<u32>(A - m_uiKeyRC5[2 * j], B) ^ B;
        }
        *reinterpret_cast<u32*>(out + i * 4)       = A - m_uiKeyRC5[0];
        *reinterpret_cast<u32*>(out + (i + 1) * 4) = B - m_uiKeyRC5[1];
        i += 2;
        rest -= 8;
    }
    return rest == 0 || rc5Decrypt32(in + (len - rest), out + (len - rest), rest);
}

bool CCrypt::rc5Decrypt(const u8* in, u8* out, i32 len) noexcept
{
    return len < 8 ? rc5Decrypt32(in, out, len) : rc5Decrypt64(in, out, len);
}

// ── RC5 encrypt ──────────────────────────────────────────────────────────────

bool CCrypt::rc5Encrypt32(const u8* in, u8* out, i32 len) noexcept
{
    if (in != out) nocrtMemcpy(out, in, len);
    u16 key = 0;
    i32 rest = len;
    int i = 0;
    while (rest >= 4)
    {
        auto A = *reinterpret_cast<const u16*>(in + i * 2) + m_usKeyRC5[key++];
        auto B = *reinterpret_cast<const u16*>(in + (i + 1) * 2) + m_usKeyRC5[key++];
        for (u32 round = 0; round < kRC5Round; ++round)
        {
            // Round keys from the 16-bit schedule to match the original RC5_Encrypt32
            // exactly. m_usKeyRC5[j] == (u16)m_uiKeyRC5[j], so output is identical.
            A = rotl<u16>(A ^ B, B & 0x0F) + m_usKeyRC5[key++];
            B = rotl<u16>(B ^ A, A & 0x0F) + m_usKeyRC5[key++];
        }
        *reinterpret_cast<u16*>(out + i * 2)       = A ^ static_cast<u16>(m_iSerialKey);
        *reinterpret_cast<u16*>(out + (i + 1) * 2) = B ^ static_cast<u16>(m_iSerialKey);
        i += 2;
        rest -= 4;
    }
    return true;
}

bool CCrypt::rc5Encrypt64(const u8* in, u8* out, i32 len) noexcept
{
    i32 rest = len;
    int i = 0;
    while (rest >= 8)
    {
        auto A = *reinterpret_cast<const u32*>(in + i * 4) + m_uiKeyRC5[0];
        auto B = *reinterpret_cast<const u32*>(in + (i + 1) * 4) + m_uiKeyRC5[1];
        for (int j = 1; j <= static_cast<int>(kRC5Round); j++)
        {
            A = rotl<u32>(A ^ B, B) + m_uiKeyRC5[2 * j];
            B = rotl<u32>(B ^ A, A) + m_uiKeyRC5[2 * j + 1];
        }
        *reinterpret_cast<u32*>(out + i * 4)       = A ^ static_cast<u32>(m_iSerialKey);
        *reinterpret_cast<u32*>(out + (i + 1) * 4) = B ^ static_cast<u32>(m_iSerialKey);
        i += 2;
        rest -= 8;
    }
    return rest == 0 || rc5Encrypt32(in + (len - rest), out + (len - rest), rest);
}

bool CCrypt::rc5Encrypt(const u8* in, u8* out, i32 len) noexcept
{
    return len < 8 ? rc5Encrypt32(in, out, len) : rc5Encrypt64(in, out, len);
}

// ═════════════════════════════════════════════════════════════════════════════
//  RC6
// ═════════════════════════════════════════════════════════════════════════════

void CCrypt::rc6KeySetup(const u8* inKey, u16 keyLen) noexcept
{
    constexpr u32 U = 4, ROT = 3;
    const u32 c   = (keyLen + U - 1) / U;
    const u32 MIX = 3 * (c > kRC6Tables ? c : kRC6Tables);

    std::array<u32, 8> L{};
    auto keyBytes = std::bit_cast<std::array<u8, 4>>(m_iSerialKey);
    for (i32 j = static_cast<i32>(keyLen) - 1, idx = 0; j >= 0; j--, idx = (idx + 1) % 4)
        L[j / U] = (L[j / U] << 8) + static_cast<u32>(inKey[j] + static_cast<i8>(keyBytes[idx]));

    m_uiKeyRC6[0] = kP32;
    for (u32 j = 1; j < kRC6Tables; j++)
        m_uiKeyRC6[j] = m_uiKeyRC6[j - 1] + kQ32;

    u32 A = 0, B = 0, j = 0, k = 0;
    for (u32 s = 1; s <= MIX; s++)
    {
        A = m_uiKeyRC6[j] = rotl<u32>(m_uiKeyRC6[j] + A + B, ROT);
        B = L[k]          = rotl<u32>(L[k] + A + B, A + B);
        j = (j + 1) % kRC6Tables;
        k = (k + 1) % c;
    }
}

void CCrypt::rc6Setup() noexcept
{
    constexpr auto key = rc6Key();
    rc6KeySetup(key.data(), 32);
}

// ── RC6 decrypt ──────────────────────────────────────────────────────────────

bool CCrypt::rc6Decrypt(const u8* in, u8* out, i32 len) noexcept
{
    i32 rest = len;
    int i = 0;
    while (rest >= 16)
    {
        auto A = (*reinterpret_cast<const u32*>(in + i * 4)       ^ m_iSerialKey) - m_uiKeyRC6[kRC6R22];
        auto B =  *reinterpret_cast<const u32*>(in + (i + 1) * 4) ^ m_iSerialKey;
        auto C = (*reinterpret_cast<const u32*>(in + (i + 2) * 4) ^ m_iSerialKey) - m_uiKeyRC6[kRC6R23];
        auto D =  *reinterpret_cast<const u32*>(in + (i + 3) * 4) ^ m_iSerialKey;
        for (u32 k = kRC6Round; k > 0; k--)
        {
            auto tmp = D; D = C; C = B; B = A; A = tmp;
            auto u = rotl<u32>(D * (2 * D + 1), kRC6LGW);
            auto t = rotl<u32>(B * (2 * B + 1), kRC6LGW);
            C = rotr<u32>(C - m_uiKeyRC6[2 * k + 1], t) ^ u;
            A = rotr<u32>(A - m_uiKeyRC6[2 * k],     u) ^ t;
        }
        *reinterpret_cast<u32*>(out + i * 4)       = A;
        *reinterpret_cast<u32*>(out + (i + 1) * 4) = B - m_uiKeyRC6[0];
        *reinterpret_cast<u32*>(out + (i + 2) * 4) = C;
        *reinterpret_cast<u32*>(out + (i + 3) * 4) = D - m_uiKeyRC6[1];
        i += 4;
        rest -= 16;
    }
    return rest == 0 || (rc5Setup(), rc5Decrypt(in + (len - rest), out + (len - rest), rest));
}

// ── RC6 encrypt ──────────────────────────────────────────────────────────────

bool CCrypt::rc6Encrypt(const u8* in, u8* out, i32 len) noexcept
{
    i32 rest = len;
    int i = 0;
    while (rest >= 16)
    {
        auto A = *reinterpret_cast<const u32*>(in + i * 4);
        auto B = *reinterpret_cast<const u32*>(in + (i + 1) * 4) + m_uiKeyRC6[0];
        auto C = *reinterpret_cast<const u32*>(in + (i + 2) * 4);
        auto D = *reinterpret_cast<const u32*>(in + (i + 3) * 4) + m_uiKeyRC6[1];
        for (u32 k = 1; k <= kRC6Round; k++)
        {
            auto t = rotl<u32>(B * (2 * B + 1), kRC6LGW);
            auto u = rotl<u32>(D * (2 * D + 1), kRC6LGW);
            A = rotl<u32>(A ^ t, u) + m_uiKeyRC6[2 * k];
            C = rotl<u32>(C ^ u, t) + m_uiKeyRC6[2 * k + 1];
            auto tmp = A; A = B; B = C; C = D; D = tmp;
        }
        *reinterpret_cast<u32*>(out + i * 4)       = (A + m_uiKeyRC6[kRC6R22]) ^ static_cast<u32>(m_iSerialKey);
        *reinterpret_cast<u32*>(out + (i + 1) * 4) = B ^ static_cast<u32>(m_iSerialKey);
        *reinterpret_cast<u32*>(out + (i + 2) * 4) = (C + m_uiKeyRC6[kRC6R23]) ^ static_cast<u32>(m_iSerialKey);
        *reinterpret_cast<u32*>(out + (i + 3) * 4) = D ^ static_cast<u32>(m_iSerialKey);
        i += 4;
        rest -= 16;
    }
    return rest == 0 || (rc5Setup(), rc5Encrypt(in + (len - rest), out + (len - rest), rest));
}

// ═════════════════════════════════════════════════════════════════════════════
//  Hex utilities
// ═════════════════════════════════════════════════════════════════════════════

u8 hexCharToValue(char c) noexcept
{
    if (c >= '0' && c <= '9') return static_cast<u8>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<u8>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<u8>(c - 'A' + 10);
    return 0;
}

void hexToBytes(const char* hexStr, char* output, size_t byteCount) noexcept
{
    for (size_t i = 0; i < byteCount; i++)
    {
        u8 high = hexCharToValue(hexStr[i * 2]);
        u8 low  = hexCharToValue(hexStr[i * 2 + 1]);
        output[i] = static_cast<char>((high << 4) | low);
    }
}

} // namespace mg::crypto
