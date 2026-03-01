// =============================================================================
// GameManagers - Hook installer for all 5 game managers
// =============================================================================
// Registers Get/Destroy hooks for: Room, UnitContainer, UnitMgr, NetMgr, Dynamics
// =============================================================================
#pragma once

#include "core/types.hpp"

#include "game/managers/manager_base.hpp"

namespace mg::game {

class GameManagerHooks {
public:
    explicit GameManagerHooks(::mg::MegaGuardContext& ctx);
    ~GameManagerHooks();

    GameManagerHooks(const GameManagerHooks&) = delete;
    GameManagerHooks& operator=(const GameManagerHooks&) = delete;

    VoidResult install();
    void buildWhitelists();
    void uninstallAll();

    // Access manager state
    ManagerState& room()          { return room_; }
    ManagerState& unitContainer() { return unitContainer_; }
    ManagerState& unitMgr()       { return unitMgr_; }
    ManagerState& netMgr()        { return netMgr_; }
    ManagerState& dynamics()      { return dynamics_; }

private:
    ::mg::MegaGuardContext& ctx_;

    ManagerState room_;
    ManagerState unitContainer_;
    ManagerState unitMgr_;
    ManagerState netMgr_;
    ManagerState dynamics_;
};

} // namespace mg::game
