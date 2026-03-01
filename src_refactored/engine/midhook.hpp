// =============================================================================
// MidHook - Mid-function hooking engine
// =============================================================================
#pragma once

#include "core/types.hpp"
#include "engine/assembler.hpp"

namespace mg {

struct RegisterState {
    u32 eflags;
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp;
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;
};

using MidHookCallback = void(__cdecl*)(RegisterState);

class MidHook {
public:
    MidHook() = default;
    ~MidHook();

    MidHook(const MidHook&) = delete;
    MidHook& operator=(const MidHook&) = delete;

    VoidResult create(uptr address, MidHookCallback callback,
                      bool withOrigCode = true, std::size_t jmpSize = 5);
    VoidResult remove();

private:
    void generateThunk(Assembler& assembler, bool withOrigCode);
    void copyOldCode(u8* ptr);

    MidHookCallback callback_ = nullptr;
    uptr address_ = 0;
    std::size_t jmpSize_ = 5;
    std::size_t origCodeSize_ = 0;
    u8* buffer_ = nullptr;
    u8* origCode_ = nullptr;
    u8* newCode_ = nullptr;
};

} // namespace mg
