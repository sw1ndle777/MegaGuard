// =============================================================================
// WeaponDrop - restore weapon-drop-on-death + diagnostic  (1.0.3)  [see .hpp]
// =============================================================================
// Hooks StartDropWeapon (CCh_BasicDropper::vtbl[1] == 0x00884A10), reached on
// death from the death-effect Start (sub_80A840 etc.) via the dropper object.
// Logs the weapon chain the function itself gates on, then calls the original.
// Whichever link logs as 0 is the reason the gun doesn't drop:
//   dropper[1]      = character
//   char[67]        = CExPlayer            (char + 0x10C)
//   ex[10]          = equip slot           (CExPlayer + 0x28)
//   equip->vtbl[9]()= weapon model/item
//   model[153]      = weapon PhysX prop    (+0x264)  <- the throwable; if 0 -> no drop
//   model[152]      = weapon node          (+0x260)
//
// The chain walk is wrapped in SEH so a bad/unexpected pointer can only produce a
// "0" in the log line, never a crash.
// =============================================================================
#include "pch.hpp"
#include "modding/features/weapon_drop.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "game/game_engine_log.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::modding {

using namespace mg::game;

namespace {

// Read the dword at (p + dwIndex*4) as a pointer.
inline void* rd(void* p, int dwIndex)
{
    return p ? *reinterpret_cast<void**>(reinterpret_cast<u8*>(p) + dwIndex * 4) : nullptr;
}

// POD result — no C++ objects, so this function may use SEH (__try/__except).
struct DropChain { void* chr; void* ex; void* equip; void* model; void* node; void* prop; };

// SEH-guarded walk. Any access violation just leaves the offending field (and the
// rest) at 0 — the hook can never fault here.
__declspec(noinline) DropChain walkChain(void* dropper)
{
    DropChain c{};
    __try
    {
        c.chr   = rd(dropper, 1);    // dropper->m_pCharacter
        c.ex    = rd(c.chr, 67);     // char + 0x10C   (CExPlayer)
        c.equip = rd(c.ex, 10);      // CExPlayer + 0x28 (equip slot)
        if (c.equip)
        {
            void** vt = *reinterpret_cast<void***>(c.equip);
            c.model = reinterpret_cast<void* (__thiscall*)(void*)>(vt[9])(c.equip);  // equip->vtbl[9]()
        }
        c.node = rd(c.model, 152);   // weapon node      (+0x260)
        c.prop = rd(c.model, 153);   // weapon PhysX prop (+0x264)
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return c;
}

// StartDropWeapon — __thiscall(CCh_BasicDropper* dropper).
void __fastcall hkStartDropWeapon(void* self, u32 /*edx*/)
{
    auto& registry = mg::ctx().hookRegistry();
    auto  original = registry.findDetour(HookId::WeaponDrop)
        ->getOriginal<decltype(&hkStartDropWeapon)>();

    DropChain c = walkChain(self);
    GameEngineLog::err(
        "[WeaponDrop] dropper=%p chr=%p ex=%p equip=%p model=%p node=%p prop=%p",
        self, c.chr, c.ex, c.equip, c.model, c.node, c.prop);

    original(self, 0);
}

} // anonymous namespace

WeaponDrop::WeaponDrop(::mg::MegaGuardContext& ctx) : ctx_(ctx) {}
WeaponDrop::~WeaponDrop() = default;

VoidResult WeaponDrop::install()
{
    auto& registry = ctx_.hookRegistry();
    registry.registerDetour(HookId::WeaponDrop)
        .create(MG_CONST(addr::features::StartDropWeapon), hkStartDropWeapon);
    return VoidResult::ok();
}

} // namespace mg::modding
