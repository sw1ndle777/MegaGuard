// =============================================================================
// SyscallCloner - Clone NT syscalls from disk ntdll
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg {

class MemoryPool;
class ImportResolver;

class SyscallCloner {
public:
    SyscallCloner(MemoryPool& pool, ImportResolver& resolver);
    ~SyscallCloner();

    SyscallCloner(const SyscallCloner&) = delete;
    SyscallCloner& operator=(const SyscallCloner&) = delete;

    VoidResult initialize();

    // Clone a syscall function from clean ntdll on disk.
    // Returns function pointer to cloned stub in RWX memory.
    template <typename T>
    T cloneSyscall(uptr ntdllBase, uptr functionOffset);

    // Clone by name lookup
    template <typename T>
    T cloneSyscallByName(const char* funcName);

private:
    MemoryPool& pool_;
    ImportResolver& resolver_;

    // Cached raw ntdll data from disk
    std::vector<u8> ntdllDiskData_;
    uptr ntdllBaseInMemory_ = 0;
    bool initialized_ = false;

    uptr cloneInternal(uptr base, uptr offset);
    VoidResult loadNtdllFromDisk();
};

} // namespace mg
