// =============================================================================
// GachaPityHandler - Gacha lucky meter pity system
// =============================================================================
// Hooks 3 gacha UI functions (back/next/select) to send pity request packets
// to the main server. Handles response packet ID 95 to update lucky points
// on all game players and refresh the gacha dialog UI.
// =============================================================================
#pragma once

#include "core/types.hpp"
#include "engine/midhook.hpp"

namespace mg::game {

class CustomPacketDispatcher;

class GachaPityHandler {
public:
    explicit GachaPityHandler(::mg::MegaGuardContext& ctx, CustomPacketDispatcher& dispatcher);
    ~GachaPityHandler();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
    CustomPacketDispatcher& dispatcher_;
    MidHook luckyPointsHook_;
};

} // namespace mg::game
