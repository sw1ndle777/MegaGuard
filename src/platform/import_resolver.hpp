// =============================================================================
// ImportResolver - Hash-based API resolution
// =============================================================================
// Resolves Windows API functions by hash instead of import table entries.
// No global state. Instance owned by MegaGuardContext.
// CloakWork macros (CW_GET_MODULE, CW_GET_PROC) used only in .cpp.
// =============================================================================
#pragma once

#include "core/types.hpp"
#include <mutex>

namespace mg {

class MemoryPool;

class ImportResolver {
public:
    explicit ImportResolver(MemoryPool& pool);
    ~ImportResolver();

    ImportResolver(const ImportResolver&) = delete;
    ImportResolver& operator=(const ImportResolver&) = delete;

    VoidResult initialize();

    // Resolve a function by module name hash + function name hash.
    // Returns raw pointer (caller casts to function typedef).
    void* resolve(u32 moduleHash, u32 procHash);

    // Resolve by string (convenience — uses runtime hashing)
    void* resolveByName(const char* moduleName, const char* procName);

    // Get a cached module handle
    void* getModuleHandle(u32 moduleHash);

private:
    MemoryPool& pool_;
    std::mutex mutex_;

    struct CachedResolve {
        u32 moduleHash;
        u32 procHash;
        void* address;
    };
    std::vector<CachedResolve> cache_;
};

} // namespace mg
