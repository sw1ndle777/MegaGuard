// =============================================================================
// call_helper - Typed function pointer call & read/write helpers
// =============================================================================
// Replaces the old _call<>, _rv<>, _wv<> free functions.
// Header-only, no state, safe for mapped DLLs.
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg {

// Call a function at runtime address with typed signature
template <typename FuncPtr, typename... Args>
MG_FORCEINLINE auto call(uptr address, Args... args) {
    return reinterpret_cast<FuncPtr>(address)(std::forward<Args>(args)...);
}

// Read a value at base + offset
template <typename T>
MG_FORCEINLINE T readValue(uptr base, std::ptrdiff_t offset = 0) {
    return *reinterpret_cast<T*>(base + offset);
}

// Write a value at base + offset
template <typename T>
MG_FORCEINLINE void writeValue(uptr base, std::ptrdiff_t offset, T value) {
    *reinterpret_cast<T*>(base + offset) = value;
}

// Call a virtual function by index
template <typename ReturnType = void, typename... Args>
MG_FORCEINLINE ReturnType callVFunc(void* instance, int index, Args... args) {
    using Fn = ReturnType(__thiscall*)(void*, Args...);
    auto vtable = *reinterpret_cast<uptr**>(instance);
    return reinterpret_cast<Fn>(vtable[index])(instance, args...);
}

} // namespace mg
