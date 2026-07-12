// =============================================================================
// PointerEncryption - Implementation
// =============================================================================
#include "pch.hpp"
#include "engine/pointer_encryption.hpp"
#include "core/version_generated.hpp"

namespace mg {

    // Compile-time FNV-1a for version salt — same approach as GuardRegions.
    // Ensures key schedule is unique per build.
    inline constexpr u32 fnv1a_pe(const char* s, u32 h = 0x811C9DC5u) {
        return (*s == '\0') ? h : fnv1a_pe(s + 1, (h ^ static_cast<u32>(*s)) * 0x01000193u);
    }
    inline constexpr u32 kPtrVersionSalt = fnv1a_pe(MG_VERSION_FULL);

    PointerEncryption::PointerEncryption(uptr moduleBase)
        : moduleBase_(moduleBase)
    {
    }

    void PointerEncryption::ensureInit() {
        if (initialized_) return;

#ifdef _WIN64
        pebAddress_ = __readgsqword(0x60);
#else
        pebAddress_ = __readfsdword(0x30);
#endif

        // Seed with multiple entropy sources: tick count, TSC, this pointer, PEB,
        // module base, and version salt — makes key schedule unique per instance,
        // per process, per build.
        u32 seed = static_cast<u32>(GetTickCount())
            ^ static_cast<u32>(__rdtsc())
            ^ static_cast<u32>(reinterpret_cast<uptr>(this))
            ^ static_cast<u32>(pebAddress_)
            ^ static_cast<u32>(moduleBase_)
            ^ kPtrVersionSalt;

        std::mt19937 engine(seed);
        std::uniform_int_distribution<int> dist(0xFFF, 0xFFFFFF);
        for (int i = 0; i < 12; i++) {
            randomKeys_[i] = dist(engine) ^ static_cast<int>(kPtrVersionSalt >> (i % 4));
        }

        initialized_ = true;
    }

    uptr PointerEncryption::process(uptr ptrValue) {
        ensureInit();

        if (moduleBase_ == 0) return ptrValue;

        uptr instanceAddr = reinterpret_cast<uptr>(this);
        uptr keyArray[18];

        keyArray[0] = 0xC7FA8EF6 ^ kPtrVersionSalt;
        keyArray[1] = instanceAddr;
        keyArray[2] = 0xA9F6EFC2 + static_cast<int>(__DATE__[0]) + static_cast<int>(__DATE__[1]) + static_cast<int>(__DATE__[2]);
        keyArray[3] = moduleBase_ ^ kPtrVersionSalt;
        keyArray[4] = 0x5A98F6C1;
        keyArray[5] = pebAddress_;
        keyArray[6] = 0x6C9F5E8A ^ (kPtrVersionSalt >> 8);
        keyArray[7] = 0xFB38B773 + static_cast<int>(__TIMESTAMP__[0]) + static_cast<int>(__TIMESTAMP__[1]) + static_cast<int>(__TIMESTAMP__[2]);
        keyArray[8] = moduleBase_ ^ pebAddress_;
        keyArray[9] = 0x5216D58A ^ kPtrVersionSalt;
        keyArray[10] = 0xECDF0931;
        keyArray[11] = 0x5DA5FCEF ^ (kPtrVersionSalt << 4);
        keyArray[12] = instanceAddr ^ moduleBase_;
        keyArray[13] = 0x95ACF30D + static_cast<int>(__TIMESTAMP__[11]) + static_cast<int>(__TIMESTAMP__[12]) + static_cast<int>(__TIMESTAMP__[14]);
        keyArray[14] = 0x2E2CDFE2;
        keyArray[15] = 0x824E7BD4 ^ kPtrVersionSalt;
        keyArray[16] = instanceAddr ^ pebAddress_;
        keyArray[17] = 0x4D703C6C + static_cast<int>(__TIMESTAMP__[15]) + static_cast<int>(__TIMESTAMP__[17]) + static_cast<int>(__TIMESTAMP__[18]);

        for (int i = 0; i < 18; i++) {
            if (i < 12) keyArray[i] ^= randomKeys_[i];

            keyArray[i] ^= (~(i % 2) ^ (i + 1) * (i + 1) >> 0xC) | 0xF;
            keyArray[i] += ((i + 1) * 0xC6A) ^ 0xCF6A9F;
            keyArray[i] <<= 0xC;
            keyArray[i] -= 0x7F9A ^ (~(i % 2) * (i + 1)) | 0xA;
            keyArray[i] *= ((i + i) * i) + 0x76A;
            keyArray[i] ^= _byteswap_ulong(static_cast<unsigned long>(0xC5A9E6A1));
            keyArray[i] >>= 0xC;
            keyArray[i] *= (~(i % 4) ^ (i + 0xC) * (i + 0xA) >> 0xF) | 0xA;
            keyArray[i] -= ((i * 4) * 0x5A8E) ^ 0xA5E89F;
            keyArray[i] <<= 0xA;
            keyArray[i] ^= 0xC63EA ^ (~(i % 3) * (i + 4)) | 0xC;
            keyArray[i] *= ((i + i) * i) + 0xCF;
            keyArray[i] += _byteswap_ulong(static_cast<unsigned long>(0x6F9A5EC1));
            keyArray[i] >>= 0xF;

            bool shouldDecrypt = ptrValue & 0x80000000000;
            if (shouldDecrypt) {
                ptrValue &= ~(0x80000000000ULL);
                ptrValue ^= keyArray[i];
            }
            else {
                ptrValue |= 0x80000000000ULL;
                ptrValue ^= keyArray[i];
            }
        }
        return ptrValue;
    }

} // namespace mg