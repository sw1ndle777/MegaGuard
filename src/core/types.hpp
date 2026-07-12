// =============================================================================
// MegaGuard - Core Types
// =============================================================================
// Fundamental types used across the entire codebase:
//   - Error enums (no exceptions)
//   - Result<T, E> (std::expected-style, C++23 polyfill)
//   - Common type aliases
// =============================================================================
#pragma once

#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

#include "core/fwd.hpp"

namespace mg {

// ── Error codes ────────────────────────────────────────────────────────────────
enum class ErrorCode : std::uint32_t {
    kSuccess         = 0,
    kInvalidAddress  = 1,
    kInvalidParam    = 2,
    kAllocationFail  = 3,
    kHookFailed      = 4,
    kHookNotFound    = 5,
    kProtectFailed   = 6,
    kPatternNotFound = 7,
    kModuleNotFound  = 8,
    kProcNotFound    = 9,
    kInitFailed      = 10,
    kAlreadyInit     = 11,
    kNotInitialized  = 12,
    kIpcFailed       = 13,
    kCryptoError     = 14,
    kFileIoError     = 15,
    kIntegrityFail   = 16,
    kDebugDetected   = 17,
    kUnknown         = 0xFFFF,
};

// ── Lightweight Result<T, E> ───────────────────────────────────────────────────
// No exceptions. Explicit error propagation. Works like std::expected.
template <typename T, typename E = ErrorCode>
class Result {
public:
    // Success construction
    Result(T value) : value_(std::move(value)), hasValue_(true) {}

    // Error construction
    struct ErrorTag {};
    Result(ErrorTag, E error) : error_(error), hasValue_(false) {}

    static Result ok(T value) { return Result(std::move(value)); }
    static Result err(E error) { return Result(ErrorTag{}, error); }

    [[nodiscard]] bool isOk() const { return hasValue_; }
    [[nodiscard]] bool isErr() const { return !hasValue_; }
    [[nodiscard]] explicit operator bool() const { return hasValue_; }

    [[nodiscard]] T& value() { return value_; }
    [[nodiscard]] const T& value() const { return value_; }
    [[nodiscard]] E error() const { return error_; }

    // Monadic: transform value if Ok, propagate error otherwise
    template <typename Fn>
    auto map(Fn&& fn) -> Result<decltype(fn(std::declval<T>())), E> {
        using U = decltype(fn(std::declval<T>()));
        if (hasValue_) return Result<U, E>::ok(fn(value_));
        return Result<U, E>::err(error_);
    }

    // Unwrap with default
    T valueOr(T fallback) const {
        return hasValue_ ? value_ : std::move(fallback);
    }

private:
    union {
        T value_;
        E error_;
    };
    bool hasValue_;
};

// Specialization for void success type
template <typename E>
class Result<void, E> {
public:
    Result() : hasValue_(true) {}

    struct ErrorTag {};
    Result(ErrorTag, E error) : error_(error), hasValue_(false) {}

    static Result ok() { return Result(); }
    static Result err(E error) { return Result(ErrorTag{}, error); }

    [[nodiscard]] bool isOk() const { return hasValue_; }
    [[nodiscard]] bool isErr() const { return !hasValue_; }
    [[nodiscard]] explicit operator bool() const { return hasValue_; }
    [[nodiscard]] E error() const { return error_; }

private:
    E error_{};
    bool hasValue_;
};

// ── Convenience aliases ────────────────────────────────────────────────────────
using VoidResult = Result<void, ErrorCode>;

// ── Common type aliases ────────────────────────────────────────────────────────
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using uptr = std::uintptr_t;

} // namespace mg
