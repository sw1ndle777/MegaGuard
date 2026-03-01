// =============================================================================
// CryptoEngine - RC5/RC6 key setup hooks + TCP header encryption
// =============================================================================
#pragma once

#include "core/types.hpp"
#include "game/structures.hpp"

namespace mg::game {

class CryptoEngine {
public:
    explicit CryptoEngine(::mg::MegaGuardContext& ctx);
    ~CryptoEngine();

    CryptoEngine(const CryptoEngine&) = delete;
    CryptoEngine& operator=(const CryptoEngine&) = delete;

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

// TCP header obfuscation (free functions — no class state needed)
u32 encryptTcpHeader(u32 header);
u32 decryptTcpHeader(u32 encrypted);

} // namespace mg::game
