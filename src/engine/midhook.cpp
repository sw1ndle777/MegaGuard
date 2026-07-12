// =============================================================================
// MidHook - Implementation
// =============================================================================
#include "pch.hpp"
#include "engine/midhook.hpp"
#include "engine/ldasm.h"
#include "platform/memory.hpp"

namespace mg {

MidHook::~MidHook() {
    remove();
}

VoidResult MidHook::create(uptr address, MidHookCallback callback,
                            bool withOrigCode, std::size_t jmpSize) {
    jmpSize_ = jmpSize;
    if (!address) return VoidResult::err(ErrorCode::kInvalidAddress);

    MEMORY_BASIC_INFORMATION meminfo{};
    auto r = VirtualQuery(reinterpret_cast<LPCVOID>(address), &meminfo, sizeof(meminfo));
    if (r == 0) return VoidResult::err(ErrorCode::kInvalidAddress);

    callback_ = callback;
    address_ = address;

    buffer_ = static_cast<u8*>(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!buffer_) return VoidResult::err(ErrorCode::kAllocationFail);

    origCode_ = buffer_ + 0x100;
    newCode_ = buffer_ + 0x200;

    copyOldCode(reinterpret_cast<u8*>(address));

    Assembler assembler(asmjit::Arch::kX86);
    generateThunk(assembler, withOrigCode);

    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address_), origCodeSize_, PAGE_EXECUTE_READWRITE, &oldProtect);

    // Write JMP from original to our thunk
    auto setJump = [this](uptr src, u8* dst) {
        *reinterpret_cast<u8*>(src) = 0xE9;
        *reinterpret_cast<i32*>(src + 1) = static_cast<i32>(reinterpret_cast<uptr>(dst) - src - 5);
    };
    setJump(address_, newCode_);

    // NOP remaining bytes
    for (std::size_t i = jmpSize_; i < origCodeSize_; i++) {
        *reinterpret_cast<u8*>(address_ + i) = 0x90;
    }

    VirtualProtect(reinterpret_cast<LPVOID>(address_), origCodeSize_, oldProtect, &oldProtect);
    return VoidResult::ok();
}

VoidResult MidHook::remove() {
    if (!address_ || !buffer_) return VoidResult::ok();

    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(address_), origCodeSize_, PAGE_EXECUTE_READWRITE, &oldProtect);

    for (std::size_t i = 0; i < origCodeSize_; i++) {
        *reinterpret_cast<u8*>(address_ + i) = origCode_[i];
    }

    VirtualProtect(reinterpret_cast<LPVOID>(address_), origCodeSize_, oldProtect, &oldProtect);
    VirtualFree(buffer_, 0, MEM_RELEASE);

    buffer_ = nullptr;
    origCode_ = nullptr;
    newCode_ = nullptr;
    address_ = 0;
    return VoidResult::ok();
}

void MidHook::generateThunk(Assembler& assembler, bool withOrigCode) {
    auto* a = assembler.assembler();

    // Save all GP registers + eflags onto the stack.
    // This creates the RegisterState struct layout at [ESP]:
    //   pushad → eax,ecx,edx,ebx,esp,ebp,esi,edi  (32 bytes)
    //   pushfd → eflags                             (4 bytes)
    // After 'call', [ESP+4] = eflags = start of RegisterState,
    // which is exactly where __cdecl reads the first by-value argument.
    a->pushad();
    a->pushfd();

    // Indirect call — position-independent (the thunk is copied to a VirtualAlloc'd buffer,
    // so call-relative would target the wrong address).
    a->lea(asmjit::x86::eax, asmjit::x86::dword_ptr(reinterpret_cast<uptr>(callback_)));
    a->call(asmjit::x86::eax);

    // Restore registers
    a->popfd();
    a->popad();

    if (withOrigCode) {
        // Execute original code
        for (std::size_t i = 0; i < origCodeSize_; i++) {
            a->embed(&origCode_[i], 1);
        }
    }

    // Jump back to original code after hook
    auto returnAddr = address_ + origCodeSize_;
    a->push(asmjit::imm(static_cast<u32>(returnAddr)));
    a->ret();

    auto& code = assembler.assembler()->code()->sectionById(0)->buffer();
    nocrtMemcpy(newCode_, code.data(), code.size());
}

void MidHook::copyOldCode(u8* ptr) {
    origCodeSize_ = 0;

    struct _ldasm_data ld;
    while (origCodeSize_ < jmpSize_) {
        unsigned int instrLen = ldasm(ptr + origCodeSize_, &ld, 0);
        if (instrLen == 0) break;
        origCodeSize_ += instrLen;
    }

    nocrtMemcpy(origCode_, ptr, origCodeSize_);
}

} // namespace mg
