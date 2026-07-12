// =============================================================================
// GameManagerBase - Template for hooked game singleton managers
// =============================================================================
// Each game manager (CRoom, CUnitContainer, CUnitMgr, CNetMgr, CDynamics)
// follows the same pattern: Get/Destroy hooks with return-address whitelisting,
// critical section protection, and pointer encryption.
//
// Each ManagerState owns its own PointerEncryption instance so every manager
// has a unique key schedule — compromising one does not reveal the others.
// =============================================================================
#pragma once

#include "core/types.hpp"
#include "utils/call_helper.hpp"

namespace mg {
    class Logger;
    class IpcClient;
    class MegaGuardContext;
    class PointerEncryption;
}

namespace mg::game {

    // Per-manager data: stores the encrypted pointer, whitelist, etc.
    // No plaintext pointer is ever stored — only a nonce-wrapped ciphertext plus an
    // integrity tag that detects external tampering.  The nonce rotates on every
    // access so the stored ciphertext changes continuously (rolling re-encryption).
    struct ManagerState {
        u32* encrypted = nullptr;
        u32                                 integrityTag = 0;
        u32                                 accessNonce = 0;
        u32                                 vtableSnapshot = 0;
        CRITICAL_SECTION                    critSection{};
        boost::unordered_flat_set<u32>      whitelistReturnAddrs;
        std::unique_ptr<::mg::PointerEncryption> ptrEncryption;
        const char* name = "unknown";
        bool                                initialized = false;

        ManagerState();
        ~ManagerState();
        ManagerState(ManagerState&&) noexcept;
        ManagerState& operator=(ManagerState&&) noexcept;

        // Defined in manager_base.cpp (unique_ptr needs full type for dtor).
        void init(uptr moduleBase, const char* debugName);
        void shutdown();

        // Internal-only decrypt (integrity + rolling, no return-address check).
        // Returns nullptr if not yet initialized or integrity check fails.
        u32* decryptInternal();
    };

    // ── Get / Destroy logic (implemented in manager_base.cpp) ─────────────────────

    u32* managerGetHook(
        u32 returnAddress,
        ManagerState& state,
        uptr initAddr,
        u32 allocSize);

    void managerDestroyHook(ManagerState& state);

} // namespace mg::game