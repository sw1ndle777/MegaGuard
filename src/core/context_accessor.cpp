// =============================================================================
// Context Accessor — Implementation
// =============================================================================
#include "pch.hpp"
#include "core/context_accessor.hpp"
#include "core/context.hpp"

namespace mg {

namespace {

// VirtualAlloc'd block — NOT in the DLL's .data section
// [0] = context pointer XOR'd with key
// [1] = XOR key (derived from runtime entropy)
struct ContextBlock {
    uptr encrypted;
    uptr key;
};

// This single pointer lives in .data but points to a VirtualAlloc'd block.
// The actual context pointer is never stored in plain form.
ContextBlock* s_block = nullptr;

} // anonymous namespace

void initContextAccessor(MegaGuardContext* ctx) {
    s_block = reinterpret_cast<ContextBlock*>(
        VirtualAlloc(nullptr, sizeof(ContextBlock),
                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

    // Generate key from runtime entropy
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    s_block->key = static_cast<uptr>(counter.LowPart ^ GetTickCount() ^ 0x5A3C7E9D);
    s_block->encrypted = reinterpret_cast<uptr>(ctx) ^ s_block->key;
}

MegaGuardContext& ctx() {
    return *reinterpret_cast<MegaGuardContext*>(s_block->encrypted ^ s_block->key);
}

void destroyContextAccessor() {
    if (s_block) {
        s_block->encrypted = 0;
        s_block->key = 0;
        VirtualFree(s_block, 0, MEM_RELEASE);
        s_block = nullptr;
    }
}

} // namespace mg
