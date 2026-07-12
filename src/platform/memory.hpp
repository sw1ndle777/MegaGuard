// =============================================================================
// Platform memory helpers (nocrt-safe)
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg {

// ── CRT-free memory operations ─────────────────────────────────────────────────
// These are safe for manual-mapped DLLs with no CRT.

MG_FORCEINLINE void* nocrtMemset(void* dst, int value, std::size_t count) {
    auto* p = static_cast<unsigned char*>(dst);
    while (count--) *(p++) = static_cast<unsigned char>(value);
    return dst;
}

MG_FORCEINLINE void* nocrtMemcpy(void* dst, const void* src, std::size_t count) {
    if (count % sizeof(unsigned long) == 0) {
        count /= sizeof(unsigned long);
        auto* d = static_cast<unsigned long*>(dst);
        const auto* s = static_cast<const unsigned long*>(src);
        while (count--) *(d++) = *(s++);
    } else {
        auto* d = static_cast<unsigned char*>(dst);
        const auto* s = static_cast<const unsigned char*>(src);
        while (count--) *(d++) = *(s++);
    }
    return dst;
}

MG_FORCEINLINE std::size_t nocrtStrlen(const char* s) {
    const char* p = s;
    while (*p != '\0') p++;
    return static_cast<std::size_t>(p - s);
}

MG_FORCEINLINE char* nocrtStrcpy(char* dst, const char* src) {
    char* d = dst;
    while (*src != '\0') {
        *d = *src;
        src++;
        d++;
    }
    *d = '\0';
    return dst;
}

MG_FORCEINLINE int nocrtMemcmp(const void* a, const void* b, std::size_t count) {
    const auto* pa = static_cast<const unsigned char*>(a);
    const auto* pb = static_cast<const unsigned char*>(b);
    while (count--) {
        if (*pa != *pb) return (*pa < *pb) ? -1 : 1;
        pa++;
        pb++;
    }
    return 0;
}

MG_FORCEINLINE char* nocrtStrcat(char* dst, const char* src) {
    char* d = dst;
    while (*d != '\0') d++;
    while (*src != '\0') {
        *d = *src;
        src++;
        d++;
    }
    *d = '\0';
    return dst;
}

} // namespace mg
