// =============================================================================
// MemoryPool - Implementation
// =============================================================================
#include "pch.hpp"
#include "platform/memory_pool.hpp"

namespace mg {

MemoryPool::MemoryPool() = default;

MemoryPool::~MemoryPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto* ptr : allocations_) {
        VirtualFree(ptr, 0, MEM_RELEASE);
    }
    allocations_.clear();
}

void* MemoryPool::alloc(std::size_t size) {
    return allocWithProtection(size, PAGE_EXECUTE_READWRITE);
}

void* MemoryPool::allocWithProtection(std::size_t size, unsigned long protection) {
    void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, protection);
    if (ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        allocations_.insert(ptr);
    }
    return ptr;
}

void MemoryPool::free(void* ptr) {
    if (!ptr) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (allocations_.erase(ptr)) {
        VirtualFree(ptr, 0, MEM_RELEASE);
    }
}

} // namespace mg
