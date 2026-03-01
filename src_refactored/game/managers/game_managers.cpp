// =============================================================================
// GameManagers - Full implementation ported from gamemanagers.h
// =============================================================================
// Each Get hook: return-address whitelist → anticheat module range check →
//                critical section → allocate → init → IPC notify
// Each Destroy hook: double-check → critical section → CallVFunc(1) → null out
// =============================================================================
#include "pch.hpp"
#include "game/managers/game_managers.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "engine/pattern_scanner.hpp"
#include "utils/logger.hpp"
#include "platform/ipc.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::game {

// ── Return-address helper (compiler-agnostic) ─────────────────────────────────
#if defined(__clang__) && defined(_MSC_VER)
    #define MG_RETURN_ADDRESS() reinterpret_cast<u32>(__builtin_return_address(0))
#elif defined(_MSC_VER)
    #define MG_RETURN_ADDRESS() reinterpret_cast<u32>(_ReturnAddress())
#elif defined(__clang__) || defined(__GNUC__)
    #define MG_RETURN_ADDRESS() reinterpret_cast<u32>(__builtin_return_address(0))
#endif

namespace {

// ── Hook callbacks ────────────────────────────────────────────────────────────

u32* __cdecl hkGetCRoom()
{
	auto ra = MG_RETURN_ADDRESS();
	return managerGetHook(ra, mg::ctx().gameManagers().room(),
						  MG_CONST(addr::anticheat::game_managers::room::Init),
						  addr::anticheat::game_managers::room::AllocSize);
}

void __cdecl hkDestroyCRoom()
{
	managerDestroyHook(mg::ctx().gameManagers().room());
}

u32* __cdecl hkGetCUnitContainer()
{
    auto ra = MG_RETURN_ADDRESS();
    return managerGetHook(ra, mg::ctx().gameManagers().unitContainer(),
                          MG_CONST(addr::anticheat::game_managers::unit_container::Init),
                          addr::anticheat::game_managers::unit_container::AllocSize);
}

void __cdecl hkDestroyCUnitContainer()
{
    managerDestroyHook(mg::ctx().gameManagers().unitContainer());
}

u32* __cdecl hkGetCUnitMgr()
{
    auto ra = MG_RETURN_ADDRESS();
    return managerGetHook(ra, mg::ctx().gameManagers().unitMgr(),
                          MG_CONST(addr::anticheat::game_managers::unit_mgr::Init),
                          addr::anticheat::game_managers::unit_mgr::AllocSize);
}

void __cdecl hkDestroyCUnitMgr()
{
    managerDestroyHook(mg::ctx().gameManagers().unitMgr());
}

// NOTE: GetCNetMgr is __stdcall in original, all others are __cdecl
u32* __stdcall hkGetCNetMgr()
{
    auto ra = MG_RETURN_ADDRESS();
    return managerGetHook(ra, mg::ctx().gameManagers().netMgr(),
                          MG_CONST(addr::anticheat::game_managers::net_mgr::Init),
                          addr::anticheat::game_managers::net_mgr::AllocSize);
}

void __cdecl hkDestroyCNetMgr()
{
    managerDestroyHook(mg::ctx().gameManagers().netMgr());
}

u32* __cdecl hkGetCDynamics()
{
    auto ra = MG_RETURN_ADDRESS();
    return managerGetHook(ra, mg::ctx().gameManagers().dynamics(),
                          MG_CONST(addr::anticheat::game_managers::dynamics::Init),
                          addr::anticheat::game_managers::dynamics::AllocSize);
}

void __cdecl hkDestroyCDynamics()
{
    managerDestroyHook(mg::ctx().gameManagers().dynamics());
}

} // anonymous namespace

// ── GameManagerHooks class ────────────────────────────────────────────────────

GameManagerHooks::GameManagerHooks(MegaGuardContext& ctx) : ctx_(ctx) {
    auto base = ctx_.moduleBase();
    room_.init(base, "CRoom");
    unitContainer_.init(base, "CUnitContainer");
    unitMgr_.init(base, "CUnitMgr");
    netMgr_.init(base, "CNetMgr");
    dynamics_.init(base, "CDynamics");
}

GameManagerHooks::~GameManagerHooks() {
    uninstallAll();
    room_.shutdown();
    unitContainer_.shutdown();
    unitMgr_.shutdown();
    netMgr_.shutdown();
    dynamics_.shutdown();
}

VoidResult GameManagerHooks::install() {
    auto& registry = ctx_.hookRegistry();

    // Room
    registry.registerDetour(HookId::GetCRoom)
        .create(MG_CONST(addr::anticheat::game_managers::room::Get), hkGetCRoom);
    registry.registerDetour(HookId::DestroyCRoom)
        .create(MG_CONST(addr::anticheat::game_managers::room::Destroy), hkDestroyCRoom);

    // UnitContainer
    registry.registerDetour(HookId::GetCUnitContainer)
        .create(MG_CONST(addr::anticheat::game_managers::unit_container::Get), hkGetCUnitContainer);
    registry.registerDetour(HookId::DestroyCUnitContainer)
        .create(MG_CONST(addr::anticheat::game_managers::unit_container::Destroy), hkDestroyCUnitContainer);

    // UnitMgr
    registry.registerDetour(HookId::GetCUnitMgr)
        .create(MG_CONST(addr::anticheat::game_managers::unit_mgr::Get), hkGetCUnitMgr);
    registry.registerDetour(HookId::DestroyCUnitMgr)
        .create(MG_CONST(addr::anticheat::game_managers::unit_mgr::Destroy), hkDestroyCUnitMgr);

    // NetMgr
    registry.registerDetour(HookId::GetCNetMgr)
        .create(MG_CONST(addr::anticheat::game_managers::net_mgr::Get), hkGetCNetMgr);
    registry.registerDetour(HookId::DestroyCNetMgr)
        .create(MG_CONST(addr::anticheat::game_managers::net_mgr::Destroy), hkDestroyCNetMgr);

    // Dynamics
    registry.registerDetour(HookId::GetCDynamics)
        .create(MG_CONST(addr::anticheat::game_managers::dynamics::Get), hkGetCDynamics);
    registry.registerDetour(HookId::DestroyCDynamics)
        .create(MG_CONST(addr::anticheat::game_managers::dynamics::Destroy), hkDestroyCDynamics);

    return VoidResult::ok();
}

void GameManagerHooks::buildWhitelists() {
    auto& scanner = ctx_.patternScanner();
    auto base = addr::globals::g_GameModuleBase;
    auto size = addr::globals::g_GameModuleSize;

    scanner.addReturnAddresses(base, size,
        MG_CONST(addr::anticheat::game_managers::room::Get), room_.whitelistReturnAddrs);
    scanner.addReturnAddresses(base, size,
        MG_CONST(addr::anticheat::game_managers::unit_container::Get), unitContainer_.whitelistReturnAddrs);
    scanner.addReturnAddresses(base, size,
        MG_CONST(addr::anticheat::game_managers::unit_mgr::Get), unitMgr_.whitelistReturnAddrs);
    scanner.addReturnAddresses(base, size,
        MG_CONST(addr::anticheat::game_managers::net_mgr::Get), netMgr_.whitelistReturnAddrs);
    scanner.addReturnAddresses(base, size,
        MG_CONST(addr::anticheat::game_managers::dynamics::Get), dynamics_.whitelistReturnAddrs);
}

void GameManagerHooks::uninstallAll() {
    // No static pointers to clear — callbacks reach state via mg::ctx()
}

} // namespace mg::game
