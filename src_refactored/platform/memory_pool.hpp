// =============================================================================
// MemoryPool - RAII executable memory allocator
// =============================================================================
// No globals. Instance owned by MegaGuardContext.
// Thread-safe via internal mutex.
// =============================================================================
#pragma once

#include "core/types.hpp"
#include <mutex>

// Forward-declare Windows types to avoid windows.h in header
struct _SECURITY_ATTRIBUTES;

namespace mg {

class MemoryPool {
public:
    MemoryPool();
    ~MemoryPool();

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    // Allocate RWX memory. Returns nullptr on failure.
    void* alloc(std::size_t size);

    // Free previously allocated block. No-op if nullptr.
    void free(void* ptr);

    // Allocate with specific protection flags
    void* allocWithProtection(std::size_t size, unsigned long protection);

private:
    boost::unordered_flat_set<void*> allocations_;
    std::mutex mutex_;
};

} // namespace mg
