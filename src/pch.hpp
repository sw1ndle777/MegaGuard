// =============================================================================
// MegaGuard - Precompiled Header
// =============================================================================
// PCH POLICY:
//   - Only stable, heavy, rarely-changing headers belong here.
//   - NEVER include cloakwork.h here (compile-time randomization).
//   - NEVER include project headers that change frequently.
//   - Headers here are chosen because they are:
//       (a) expensive to parse (Windows SDK, STL containers)
//       (b) used in >50% of translation units
//       (c) stable (change < once/month)
// =============================================================================
#pragma once

// ── Windows SDK (stable, very expensive to parse) ──────────────────────────────
#define NOMINMAX 1
#include <windows.h>
#include <psapi.h>
#include <winternl.h>

// ── Platform intrinsics ────────────────────────────────────────────────────────
#if defined(_M_ARM64) || defined(__aarch64__) || defined(_M_ARM) || defined(__arm__)
    #include <arm_neon.h>
#elif defined(_M_X64) || defined(__amd64__) || defined(_M_IX86) || defined(__i386__)
    #include <immintrin.h>
#else
    #error Unsupported platform
#endif

// ── C++ Standard Library (stable, expensive, widely used) ──────────────────────
#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include <cstring>
#include <ctime>

#include <array>
#include <vector>
#include <span>
#include <string>
#include <string_view>

#include <memory>
#include <optional>
#include <utility>
#include <type_traits>
#include <functional>

#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <condition_variable>

#include <algorithm>
#include <chrono>
#include <random>
#include <queue>
#include <fstream>
#include <sstream>

// ── Third-party stable headers ─────────────────────────────────────────────────
#include <fmt/format.h>
#include <boost/unordered/unordered_flat_set.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <Zydis/Zydis.h>

#include "core/monocypher/monocypher.h"

// ── Build configuration ────────────────────────────────────────────────────────
#include "core/config.hpp"
