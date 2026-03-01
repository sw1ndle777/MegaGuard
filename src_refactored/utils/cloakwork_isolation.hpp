// =============================================================================
// CloakWork Isolation Header
// =============================================================================
// Wraps CloakWork macros so the rest of the project uses MG_CONST / MG_STR.
// In dev builds these are pass-through; in release builds they expand to
// CW_CONST / CW_STR_LAYERED for compile-time obfuscation.
// =============================================================================
#pragma once

#include "core/config.hpp"

// Include the full CloakWork library ONLY in release builds.
// The file lives in src/utils/cloakwork.h — reached via the original src include path.
#if !MG_PROFILE_DEV
    #include "utils/cloakwork.h"
#endif

// ── Address / constant obfuscation ────────────────────────────────────────────
// Wrap every hardcoded address or sensitive constant with MG_CONST().
// In release builds this invokes CW_CONST for compile-time encryption.
#if MG_PROFILE_DEV
    #define MG_CONST(x) (static_cast<::mg::uptr>(x))
#else
    #define MG_CONST(x) (CW_CONST(static_cast<::mg::uptr>(x)))
#endif

// ── String obfuscation ───────────────────────────────────────────────────────
// Wrap every sensitive string literal with MG_STR().
// In release builds this invokes CW_STR_LAYERED for multi-layer encryption.
#if MG_PROFILE_DEV
    #define MG_STR(x) (x)
#else
    #define MG_STR(x) CW_STR_LAYERED(x)
#endif

// ── Import hiding ────────────────────────────────────────────────────────────
// Resolve APIs without touching the IAT.
#if MG_PROFILE_DEV
    #define MG_IMPORT(mod, func)      (&func)
    #define MG_GET_MODULE(name)       ((void*)GetModuleHandleA(name))
#else
    #define MG_IMPORT(mod, func)      CW_IMPORT(mod, func)
    #define MG_GET_MODULE(name)       CW_GET_MODULE(name)
#endif

// ── Arithmetic obfuscation (MBA) ─────────────────────────────────────────────
#if MG_PROFILE_DEV
    #define MG_ADD(a, b)     ((a) + (b))
    #define MG_SUB(a, b)     ((a) - (b))
    #define MG_XOR(a, b)     ((a) ^ (b))
    #define MG_AND(a, b)     ((a) & (b))
    #define MG_OR(a, b)      ((a) | (b))
    #define MG_NEG(a)        (-(a))
    #define MG_INT(x)        (x)
    #define MG_MBA(x)        (x)
#else
    #define MG_ADD(a, b)     CW_ADD(a, b)
    #define MG_SUB(a, b)     CW_SUB(a, b)
    #define MG_XOR(a, b)     CW_XOR(a, b)
    #define MG_AND(a, b)     CW_AND(a, b)
    #define MG_OR(a, b)      CW_OR(a, b)
    #define MG_NEG(a)        CW_NEG(a)
    #define MG_INT(x)        CW_INT(x)
    #define MG_MBA(x)        CW_MBA(x)
#endif

// ── Comparison obfuscation ───────────────────────────────────────────────────
#if MG_PROFILE_DEV
    #define MG_EQ(a, b)      ((a) == (b))
    #define MG_NE(a, b)      ((a) != (b))
    #define MG_LT(a, b)      ((a) < (b))
    #define MG_GT(a, b)      ((a) > (b))
    #define MG_LE(a, b)      ((a) <= (b))
    #define MG_GE(a, b)      ((a) >= (b))
#else
    #define MG_EQ(a, b)      CW_EQ(a, b)
    #define MG_NE(a, b)      CW_NE(a, b)
    #define MG_LT(a, b)      CW_LT(a, b)
    #define MG_GT(a, b)      CW_GT(a, b)
    #define MG_LE(a, b)      CW_LE(a, b)
    #define MG_GE(a, b)      CW_GE(a, b)
#endif

// ── Boolean obfuscation ──────────────────────────────────────────────────────
#if MG_PROFILE_DEV
    #define MG_TRUE          (true)
    #define MG_BOOL(x)       (x)
#else
    #define MG_TRUE          CW_TRUE
    #define MG_BOOL(x)       CW_BOOL(x)
#endif

// ── Control flow obfuscation ─────────────────────────────────────────────────
#if MG_PROFILE_DEV
    #define MG_IF(cond)      if (cond)
    #define MG_ELSE          else
    #define MG_JUNK()        ((void)0)
    #define MG_JUNK_FLOW()   ((void)0)
    #define MG_FLATTEN(func, ...) func(__VA_ARGS__)
    #define MG_PROTECT(ret_type, ...) [&]() -> ret_type { __VA_ARGS__ }()
    #define MG_PROTECT_VOID(...) do { __VA_ARGS__ } while(0)
#else
    #define MG_IF(cond)      CW_IF(cond)
    #define MG_ELSE          CW_ELSE
    #define MG_JUNK()        CW_JUNK()
    #define MG_JUNK_FLOW()   CW_JUNK_FLOW()
    #define MG_FLATTEN(func, ...) CW_FLATTEN(func, __VA_ARGS__)
    #define MG_PROTECT(ret_type, ...) CW_PROTECT(ret_type, __VA_ARGS__)
    #define MG_PROTECT_VOID(...) CW_PROTECT_VOID(__VA_ARGS__)
#endif

// ── Function-pointer obfuscation (XTEA + decoy arrays) ───────────────────────
#if MG_PROFILE_DEV
    #define MG_CALL(func)    (func)
#else
    #define MG_CALL(func)    CW_CALL(func)
#endif

// ── Data hiding (scattered / polymorphic values) ─────────────────────────────
#if MG_PROFILE_DEV
    #define MG_SCATTER(x)    (x)
    #define MG_POLY(x)       (x)
#else
    #define MG_SCATTER(x)    CW_SCATTER(x)
    #define MG_POLY(x)       CW_POLY(x)
#endif

// Convenience aliases for use inside .cpp TUs
namespace mg::cw {

#if !MG_PROFILE_DEV
    // String encryption (legacy — prefer MG_STR at call sites)
    template <typename T>
    MG_FORCEINLINE auto encrypt_string(T&& str) {
        return CW_STR(std::forward<T>(str));
    }

    // Runtime constant (legacy — prefer MG_CONST at call sites)
    template <auto V>
    MG_FORCEINLINE constexpr auto rt_const() {
        return CW_CONST(V);
    }
#else
    // Dev stubs — no encryption
    template <typename T>
    MG_FORCEINLINE auto encrypt_string(T&& str) {
        return std::forward<T>(str);
    }

    template <auto V>
    MG_FORCEINLINE constexpr auto rt_const() {
        return V;
    }
#endif

    // Anti-debug checks
    MG_FORCEINLINE void check_debug_port() {
#if !MG_PROFILE_DEV
        CW_CHECK_DEBUG_PORT();
#endif
    }

    MG_FORCEINLINE void hide_thread() {
#if !MG_PROFILE_DEV
        CW_HIDE_THREAD();
#endif
    }

    MG_FORCEINLINE void scrub_debug_imports() {
#if !MG_PROFILE_DEV
        CW_SCRUB_DEBUG_IMPORTS();
#endif
    }

    MG_FORCEINLINE void erase_pe_header() {
#if !MG_PROFILE_DEV
        CW_ERASE_PE_HEADER();
#endif
    }

} // namespace mg::cw
