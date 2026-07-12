// =============================================================================
// XorStr - Compile-time string encryption wrapper
// =============================================================================
// Thin wrapper around CloakWork's CW_XOR or a fallback no-op.
// Include in headers if needed — this is lightweight.
// =============================================================================
#pragma once

#include "core/config.hpp"

#if __has_include("utils/cloakwork.h") && !MG_PROFILE_DEV
    // In release, use CloakWork's string encryption
    #include "utils/cloakwork.h"
    #define MG_XOR(str) CW_XOR(str)
#else
    // In dev or without CloakWork, pass through
    #define MG_XOR(str) (str)
#endif
