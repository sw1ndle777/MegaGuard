// =============================================================================
// MegaGuard - Build Configuration & Feature Toggles
// =============================================================================
// Two profiles:
//   DEV/FAST:    #define MG_PROFILE_DEV 1     (disables heavy obfuscation)
//   RELEASE:     #define MG_PROFILE_DEV 0     (full hardening)
//
// Switch profiles via compiler define: /DMG_PROFILE_DEV=1
// =============================================================================
#pragma once

// ── Profile selection ──────────────────────────────────────────────────────────
#ifndef MG_PROFILE_DEV
    #ifdef _DEBUG
        #define MG_PROFILE_DEV 1
    #else
        #define MG_PROFILE_DEV 0
    #endif
#endif

// ── Logging ────────────────────────────────────────────────────────────────────
#ifndef MG_ENABLE_CONSOLE_LOG
    #define MG_ENABLE_CONSOLE_LOG 0
#endif
#ifndef MG_ENABLE_FILE_LOG
    #if MG_PROFILE_DEV
        #define MG_ENABLE_FILE_LOG 1
    #else
        #define MG_ENABLE_FILE_LOG 1
    #endif
#endif
#define MG_LOG_ENABLED (MG_ENABLE_CONSOLE_LOG || MG_ENABLE_FILE_LOG)

// ── CloakWork feature toggles ──────────────────────────────────────────────────
// Dev profile disables expensive obfuscation for fast iteration.
// Release profile enables everything.
#if MG_PROFILE_DEV
    #define CW_ENABLE_METAMORPHIC           0  // Very slow compile-time codegen
    #define CW_ENABLE_CONTROL_FLOW          0  // Heavy CFG obfuscation
    #define CW_ENABLE_DATA_HIDING           0  // Scattered/polymorphic values
    #define CW_ENABLE_STRING_ENCRYPTION     1  // Keep strings encrypted even in dev
    #define CW_ENABLE_VALUE_OBFUSCATION     1
    #define CW_ENABLE_ANTI_DEBUG            1
    #define CW_ENABLE_FUNCTION_OBFUSCATION  0
    #define CW_ENABLE_IMPORT_HIDING         1
    #define CW_ENABLE_SYSCALLS              1
    #define CW_ENABLE_ANTI_VM               0
    #define CW_ENABLE_INTEGRITY_CHECKS      0
    #define CW_ANTI_DEBUG_RESPONSE          0  // Don't crash in dev
#else
    #define CW_ENABLE_METAMORPHIC           1
    #define CW_ENABLE_CONTROL_FLOW          1
    #define CW_ENABLE_DATA_HIDING           1
    #define CW_ENABLE_STRING_ENCRYPTION     1
    #define CW_ENABLE_VALUE_OBFUSCATION     1
    #define CW_ENABLE_ANTI_DEBUG            1
    #define CW_ENABLE_FUNCTION_OBFUSCATION  1
    #define CW_ENABLE_IMPORT_HIDING         1
    #define CW_ENABLE_SYSCALLS              1
    #define CW_ENABLE_ANTI_VM               1
    #define CW_ENABLE_INTEGRITY_CHECKS      1
    #define CW_ANTI_DEBUG_RESPONSE          1  // Crash on debugger
#endif

// ── Version info (populated by prebuild script) ────────────────────────────────
// If prebuild_manifest.py ran, core/version_generated.hpp will define these.
// Otherwise, fall back to defaults.
#if __has_include("core/version_generated.hpp")
    #include "core/version_generated.hpp"
#endif
#ifndef MG_VERSION_MAJOR
    #define MG_VERSION_MAJOR 1
#endif
#ifndef MG_VERSION_MINOR
    #define MG_VERSION_MINOR 0
#endif
#ifndef MG_VERSION_PATCH
    #define MG_VERSION_PATCH 0
#endif
#ifndef MG_VERSION_BUILD
    #define MG_VERSION_BUILD "dev"
#endif
#ifndef MG_VERSION_DATEBUILD
    #define MG_VERSION_DATEBUILD "unknown"
#endif
#ifndef MG_VERSION_FULL
    #define MG_VERSION_FULL "1.0.0+unknown"
#endif

// ── Compiler helpers ───────────────────────────────────────────────────────────
#ifdef _MSC_VER
    #define MG_FORCEINLINE __forceinline
    #define MG_NOINLINE    __declspec(noinline)
#elif defined(__clang__) || defined(__GNUC__)
    #define MG_FORCEINLINE __attribute__((always_inline)) inline
    #define MG_NOINLINE    __attribute__((noinline))
#else
    #define MG_FORCEINLINE inline
    #define MG_NOINLINE
#endif

#define MG_UNUSED(x) ((void)(x))
