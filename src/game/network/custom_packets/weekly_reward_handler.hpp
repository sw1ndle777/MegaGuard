#pragma once

#include "core/types.hpp"

namespace mg::game {

class CustomPacketDispatcher;

class WeeklyRewardHandler {
public:
    explicit WeeklyRewardHandler(::mg::MegaGuardContext& ctx, CustomPacketDispatcher& dispatcher);
    ~WeeklyRewardHandler();

    VoidResult install();
    // Registers the dialog object so the scene loader can bind XML controls to it.
    // Call during Agora menu init (before the scene XML loads).
    void showOnLobbyEntry();
    // If the server pushed weekly data this session, auto-show the dialog once.
    // Call on first lobby entry (Agora menu construct).
    void showIfPendingOnLobby();

private:
    ::mg::MegaGuardContext& ctx_;
    CustomPacketDispatcher& dispatcher_;
};

} // namespace mg::game
