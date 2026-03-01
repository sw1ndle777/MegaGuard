// =============================================================================
// CustomPacketDispatcher - Intercepts game packet distribution for custom IDs
// =============================================================================
// Hooks PacketsCallbackDistribution to route unregistered packet IDs to
// custom handlers before the game's callbackList lookup.
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::game {

using PacketHandler = char(__cdecl*)(u16*);

class CustomPacketDispatcher {
public:
    explicit CustomPacketDispatcher(::mg::MegaGuardContext& ctx);
    ~CustomPacketDispatcher();

    VoidResult install();

    void registerHandler(u32 packetId, PacketHandler handler);

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::game
