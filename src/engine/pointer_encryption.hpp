// =============================================================================
// PointerEncryption - Runtime pointer obfuscation
// =============================================================================
// No static state. Instance owned by MegaGuardContext.
// Encrypts/decrypts pointers using PEB + module base + random keys.
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg {

    class PointerEncryption {
    public:
        explicit PointerEncryption(uptr moduleBase);
        ~PointerEncryption() = default;

        PointerEncryption(const PointerEncryption&) = delete;
        PointerEncryption& operator=(const PointerEncryption&) = delete;

        // Encrypt/decrypt a raw pointer value
        template <typename T>
        MG_FORCEINLINE T encrypt(void* rawPtr) {
            return reinterpret_cast<T>(process(reinterpret_cast<uptr>(rawPtr)));
        }

        template <typename T>
        MG_FORCEINLINE T decrypt(uptr encryptedPtr) {
            return reinterpret_cast<T>(process(encryptedPtr));
        }

        // Process (encrypt if decrypted, decrypt if encrypted — toggle operation)
        uptr process(uptr ptrValue);

    private:
        void ensureInit();

        uptr moduleBase_ = 0;
        uptr pebAddress_ = 0;
        int randomKeys_[12] = {};
        bool initialized_ = false;
    };

} // namespace mg