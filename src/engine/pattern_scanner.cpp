// =============================================================================
// PatternScanner - Implementation
// =============================================================================
#include "pch.hpp"
#include "engine/pattern_scanner.hpp"

namespace mg {

#define IN_RANGE(x, a, b) ((x) >= (a) && (x) <= (b))
#define GET_BITS(x) (IN_RANGE(((x) & (~0x20)), 'A', 'F') ? (((x) & (~0x20)) - 'A' + 0xA) : (IN_RANGE((x), '0', '9') ? (x) - '0' : 0))
#define GET_BYTE(x) (GET_BITS((x)[0]) << 4 | GET_BITS((x)[1]))

PatternScanner::PatternScanner(uptr moduleBase)
    : moduleBase_(moduleBase)
{}

uptr PatternScanner::findPattern(uptr start, u32 size, const char* pattern) const {
    uptr end = start + static_cast<uptr>(size);
    uptr match = 0;
    const char* current = pattern;

    for (uptr pCur = start; pCur < end; pCur++) {
        if (!*current) return match;

        if (*reinterpret_cast<const u8*>(current) == '?' ||
            *reinterpret_cast<const u8*>(pCur) == GET_BYTE(current)) {
            if (!match) match = pCur;
            if (!current[2]) return match;

            if (*reinterpret_cast<const u16*>(current) == '??' ||
                *reinterpret_cast<const u8*>(current) != '?') {
                current += 3;
            } else {
                current += 2;
            }
        } else {
            current = pattern;
            match = 0;
        }
    }
    return 0;
}

std::vector<uptr> PatternScanner::findPatterns(uptr start, u32 size, const char* pattern) const {
    uptr end = start + static_cast<uptr>(size);
    uptr match = 0;
    std::vector<uptr> matches;
    const char* current = pattern;

    for (uptr pCur = start; pCur < end; pCur++) {
        if (!*current) matches.push_back(match);

        if (*reinterpret_cast<const u8*>(current) == '?' ||
            *reinterpret_cast<const u8*>(pCur) == GET_BYTE(current)) {
            if (!match) match = pCur;
            if (!current[2]) matches.push_back(match);

            if (*reinterpret_cast<const u16*>(current) == '??' ||
                *reinterpret_cast<const u8*>(current) != '?') {
                current += 3;
            } else {
                current += 2;
            }
        } else {
            current = pattern;
            match = 0;
        }
    }
    return matches;
}

std::vector<uptr> PatternScanner::findReturnAddresses(uptr start, u32 size, uptr targetAddress) const {
    std::vector<uptr> xrefs;
    uptr end = start + static_cast<uptr>(size);

    for (uptr pCur = start; pCur < end; pCur++) {
        if (*reinterpret_cast<u8*>(pCur) == 0xE8) {
            auto relativeAddress = *reinterpret_cast<i32*>(pCur + 1);
            uptr callAddress = pCur + 5 + relativeAddress;
            if (callAddress == targetAddress) {
                xrefs.push_back(pCur + 5);
            }
        }
    }
    return xrefs;
}

void PatternScanner::addReturnAddresses(uptr start, u32 size, uptr targetAddress,
                                         boost::unordered_flat_set<u32>& outSet) const {
    uptr end = start + static_cast<uptr>(size);

    for (uptr pCur = start; pCur < end; pCur++) {
        if (*reinterpret_cast<u8*>(pCur) == 0xE8) {
            auto relativeAddress = *reinterpret_cast<i32*>(pCur + 1);
            uptr callAddress = pCur + 5 + relativeAddress;
            if (callAddress == targetAddress) {
                outSet.emplace(static_cast<u32>(pCur + 5));
            }
        }
    }
}

uptr PatternScanner::dereference(uptr address, unsigned int offset) const {
    if (address == 0) return 0;
    if constexpr (sizeof(uptr) == 8) {
        return address + static_cast<int>(
            (*reinterpret_cast<int*>(address + offset) + offset) + sizeof(int));
    }
    return static_cast<uptr>(*reinterpret_cast<unsigned long*>(address + offset));
}

#undef IN_RANGE
#undef GET_BITS
#undef GET_BYTE

} // namespace mg
