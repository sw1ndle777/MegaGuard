// =============================================================================
// SpectatePov - Full implementation ported from spectate_pov.h (~895 lines)
// =============================================================================
// Complete TPS camera override with:
//  - Crouch detection (anim IDs 9,10,11,12,24)
//  - Bone position resolution (Human/Zombie/ZombieKing by bone type)
//  - Yaw/pitch calculation from player model rotation + room info
//  - NiMatrix3 rotation matrix computation
//  - Camera offset via sub_465A40
//  - Collision check via sub_5797B0
//  - NiAVObject transform update
//  - FPS RenderText overlay
//  Two major branches: WATCH mode (spectating) vs GAME mode (normal play)
// =============================================================================
#include "pch.hpp"
#include "modding/features/spectate_pov.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"

#include <cmath>

namespace mg::modding {

using namespace mg::game;

namespace {

// ── Normalize helpers ─────────────────────────────────────────────────────

void normalizeRadians(float* radian)
{
    *radian = std::fmod(*radian, TWO_PI);
    if (*radian < 0.0f) *radian += TWO_PI;
}

void clamp_yaw(float* yaw)
{
    while (*yaw < 0.0f)    *yaw += TWO_PI;
    while (*yaw > TWO_PI)  *yaw -= TWO_PI;
}

// ── Engine function pointer types ─────────────────────────────────────────────

using Sub57B550Func = void(__thiscall*)(CCameraTPS*);
using Sub579690Func = void(__thiscall*)(CCameraTPS*, float*, float*, float);
using Sub5778F0Func = void(__thiscall*)(CCameraTPS*, float);
using Sub577D90Func = void(__thiscall*)(CCameraTPS*);
using Sub4585C0Func = void(__cdecl*)(float*);
using Sub578030Func = void(__thiscall*)(CCameraTPS*);
using Sub577EE0Func = void(__thiscall*)(CCameraTPS*);
using NiMatrix3FromEulerAnglesZYXFunc = void(__thiscall*)(float*, float, float, float);
using Sub465A40Func = float* (__thiscall*)(float*, float*, float*);
using Sub5797B0Func = void(__thiscall*)(CCameraTPS*, float*, float*, float*);
using NiAVObjectUpdateFunc = void(__thiscall*)(NiAVObject*, float, char);
using Sub4586C0Func = float(__cdecl*)(float, float);
using Sub56F5C0Func = void(__thiscall*)(CCameraTPS*, float);
using Sub430760Func = int(__thiscall*)(char*, u32, u32, u32, const char*, u32);

// ── Resolve engine functions via MG_CONST ─────────────────────────────────────

Sub57B550Func                  sub_57B550;
Sub579690Func                  sub_579690;
Sub5778F0Func                  sub_5778F0;
Sub577D90Func                  sub_577D90;
Sub4585C0Func                  NormalizeRadian;
Sub578030Func                  sub_578030;
Sub577EE0Func                  sub_577EE0;
NiMatrix3FromEulerAnglesZYXFunc NiMatrix3FromEulerAnglesZYX;
Sub465A40Func                  sub_465A40;
Sub5797B0Func                  sub_5797B0;
NiAVObjectUpdateFunc           NiAVObjectUpdate;
Sub4586C0Func                  sub_4586C0;
Sub56F5C0Func                  sub_56F5C0;
Sub430760Func                  sub_430760;

// ── Crosshair system (driven manually in observer/WATCH mode) ─────────────────
// The crosshair manager (CCrosshairs) lives at CWorld+0x258 and is normally
// driven only for the LOCAL player — the engine hard-gates show/update to the
// local unit (sub_8E2300). In observer mode the spectated player is
// cam->CGamePlayer, so we replicate the engine's "select crosshair for equipped
// weapon" logic (sub_BA1550) against the spectated unit and drive the manager
// each frame. Crosshairs are screen-centered and selected by weapon type / zoom,
// so selecting + showing + updating is enough to render the right reticle.

using CWorldGetInstanceFunc = uptr (__cdecl*)();
using CrosshairSelectFunc   = void (__thiscall*)(void* mgr, int weaponTypeIndex);
using CrosshairSetVisFunc   = void (__fastcall*)(void* mgr, int edx, u8 visible);
using CrosshairUpdateFunc   = void (__thiscall*)(void* mgr, int a2);
using SlotGetItemFunc       = void* (__thiscall*)(void* slot);
using RtDynamicCastFunc     = void* (__cdecl*)(void* ptr, int vfDelta,
                                               void* srcType, void* dstType, int isRef);
// Crosshair spread / on-target (mirror the engine's per-weapon HUD update for the spectated unit)
using CrosshairSetSpreadFunc = void (__thiscall*)(void* mgr, float gap);          // sub_58D440 / sub_58D480
using CrosshairSetStateFunc  = void (__thiscall*)(void* mgr, int state, u8 onTgt);// sub_58D790
using WeaponIsOnTargetFunc   = int  (__thiscall*)(void* weapon);                  // sub_B573E0
using WeaponGetAttrFunc      = double (__thiscall*)(void* weapon, int attrId);    // weapon vtbl[45]
using CrosshairPointVisFunc  = void (__fastcall*)(void* pointElem, void* edx, u8 show); // sub_58E170
using LocalCrosshairRefreshFunc = void (__cdecl*)();                              // sub_BA1550 (RefreshCrosshairForEquippedWeapon)

CWorldGetInstanceFunc CWorldGetInstance;
CrosshairSelectFunc   CrosshairSelect;
CrosshairSetVisFunc   CrosshairSetVisible;
CrosshairUpdateFunc   CrosshairUpdateAll;
RtDynamicCastFunc     RtDynamicCast;
CrosshairSetSpreadFunc CrosshairSetSpread1;  // sub_58D440 -> active reticle vtbl+24 (outer/primary gap)
CrosshairSetSpreadFunc CrosshairSetSpread2;  // sub_58D480 -> active reticle vtbl+32 (inner/secondary gap)
CrosshairSetStateFunc  CrosshairSetState;    // sub_58D790 -> active reticle vtbl+60 (state + on-target)
WeaponIsOnTargetFunc   WeaponIsOnTarget;     // sub_B573E0 -> (weapon+0x1B0 ^ key) > 0
CrosshairPointVisFunc  CrosshairPointSetVisible; // sub_58E170 -> show/hide the redpoint center dot
LocalCrosshairRefreshFunc LocalCrosshairRefresh;  // sub_BA1550 -> re-show + re-select local reticle (what a weapon swap calls)

// Offsets / vtable indices recovered from the current client (see memory note).
constexpr uptr kCWorldCrosshairMgrOffset = 0x258;  // CWorld + 0x258  -> CCrosshairs*
constexpr uptr kExPlayerEquipSlotOffset  = 0x28;   // CExPlayer + 0x28 -> equip slot
constexpr u32  kWeaponCategoryOffset     = 0x28C;  // CWeapon + 0x28C  -> category (0..6)
constexpr int  kSlotGetItemVtblIndex     = 9;      // slot->vtbl[9]()  -> CItem*
constexpr uptr kModelLeanCur             = 0x444;  // CPlayerModelProperty + 0x444 -> A/D body lean
constexpr uptr kModelAimComp             = 0x44C;  // CPlayerModelProperty + 0x44C -> aim-yaw lean compensation
constexpr int  kWeaponAttrVtblIndex      = 45;     // weapon->vtbl[45](id) -> attribute (25/26 = spread)
constexpr int  kWeaponSpreadAttrOuter    = 25;     // attr id used by sub_58D440 path
constexpr int  kWeaponSpreadAttrInner    = 26;     // attr id used by sub_58D480 path
constexpr uptr kSpreadScaleConst         = 0x00FEB190; // dbl_FEB190 (~0.01) spread unit scale
constexpr uptr kCrosshairRedpointOffset  = 0x38;       // CCrosshairs + 0x38 -> CCrosshairPoint (center dot)
constexpr uptr kCrosshairVisibleFlagOffset = 0x4;      // CCrosshairs + 0x4  -> u8 visible flag (set by sub_58D560)
constexpr u32  kRoomStatusNormal         = 0x0B;       // STATUS_NORMAL (matches movement_protocol.cpp)

// ── Per-frame state (NOT in data section — stored via context accessor) ──────
// These are camera state that must persist across frames but are confined
// to this TU and accessed only from the hook callback on the game thread.
u32  old_move_state_anim = 0;
bool reset_crouch = false;

// ── RenderText ────────────────────────────────────────────────────────────────

int RenderText(u32 x, u32 y, Color color, const char* text, u32 len)
{
    if (mg::readValue<u8>(addr::features::hud_toggle::ShowHud) == 0)
    {
        return 0;
    }

    return sub_430760(reinterpret_cast<char*>(MG_CONST(0x011C9C58)), x, y, color.toARGB(), text, len);
}

// ── Spectator crosshair ───────────────────────────────────────────────────────

// Maps a CWeapon category (CWeapon+0x28C) to a crosshair array index,
// mirroring the engine's own switch in sub_BA1550. Returns -1 for "no change".
int crosshairIndexForCategory(int category)
{
    switch (category)
    {
        case 0: return 0; // melee / sword
        case 1: return 1; // nailgun
        case 2: return 2; // shotgun
        case 3: return 4; // sniper (un-zoomed)
        case 4: return 3; // gatling
        case 5: return 9; // launcher
        case 6: return 8; // grenade
        default: return -1;
    }
}

// A spectated player is a valid live crosshair target only while their room
// status is NORMAL and they still have HP. When the LOCAL player dies, the engine
// leaves cam->CGamePlayer pointing at the (now-dead) unit until the death-cam
// re-targets the killer; if that re-target never happens (e.g. a dropped/late
// server death event) we must NOT keep forcing the reticle visible on a corpse —
// that is the "crosshair stuck centered while dead" symptom. HP is XOR-obfuscated
// exactly like the engine's own alive test (see movement_protocol.cpp):
// alive == (int)(HP2 ^ HP1) > 0.
bool spectatedTargetIsAlive(CGamePlayerLocal* player)
{
    if (!player || !player->CExPlayer || !player->CExPlayer->room_info)
        return false;
    auto* ri = player->CExPlayer->room_info;
    if ((ri->RoomStatus & 0xF) != kRoomStatusNormal)
        return false;
    return static_cast<int>(ri->HP2 ^ ri->HP1) > 0;
}

// Repair a wrongly-hidden LOCAL crosshair during normal play. The crosshair's
// single global visible flag (CCrosshairs+4) is shared by ~40 engine sites
// (every weapon + every UI dialog). One known hole: respawning while the ESC
// menu is open leaves the flag at 0 and nothing re-shows it — the player has no
// reticle until they swap weapons (which calls sub_BA1550). The same hole would
// also leave the crosshair hidden after our own death-hide guard above. This
// runs only in live GAME play with an alive local player and ONLY when the flag
// is actually 0, so it normally never fires — when it does, it re-runs the
// engine's own refresh, exactly like a weapon swap. Self-limiting: SetVisible
// flips the flag to 1, so the next frame skips the call.
void repairLocalCrosshairIfHidden(CCameraTPS* cam)
{
    if (mg::readValue<u8>(addr::features::hud_toggle::ShowHud) == 0)
        return;
    if (!spectatedTargetIsAlive(cam->CGamePlayer))   // GAME mode: cam->CGamePlayer == local unit
        return;

    uptr world = CWorldGetInstance();
    if (!world)
        return;
    void* mgr = *reinterpret_cast<void**>(world + kCWorldCrosshairMgrOffset);
    if (!mgr)
        return;

    if (*(reinterpret_cast<u8*>(mgr) + kCrosshairVisibleFlagOffset) != 0)
        return;   // already visible — nothing to repair

    LocalCrosshairRefresh();   // sub_BA1550: SetVisible(1) + reselect reticle for the equipped weapon
}

// Drive the crosshair manager for the spectated unit while observing. The engine
// only does this for the local player, so we reproduce the equipped-weapon ->
// crosshair selection (sub_BA1550) using cam->CGamePlayer instead of the local
// unit, then show + update the manager every frame.
void driveSpectatorCrosshair(CCameraTPS* cam)
{
    if (mg::readValue<u8>(addr::features::hud_toggle::ShowHud) == 0)
        return;

    uptr world = CWorldGetInstance();
    if (!world)
        return;

    void* mgr = *reinterpret_cast<void**>(world + kCWorldCrosshairMgrOffset);
    if (!mgr)
        return;

    // Dead / invalid spectate target: hide the reticle instead of leaving it
    // frozen on-screen (degrades gracefully if the death-cam never re-targets).
    auto player = cam->CGamePlayer;
    if (!spectatedTargetIsAlive(player))
    {
        CrosshairSetVisible(mgr, 0, 0);
        return;
    }

    // equip slot = *(CExPlayer + 0x28)
    void* slot = *reinterpret_cast<void**>(
        reinterpret_cast<u8*>(player->CExPlayer) + kExPlayerEquipSlotOffset);
    if (!slot)
        return;

    // item = slot->vtbl[9]()
    auto slotVtbl = *reinterpret_cast<void***>(slot);
    auto getItem  = reinterpret_cast<SlotGetItemFunc>(slotVtbl[kSlotGetItemVtblIndex]);
    void* item = getItem(slot);
    if (!item)
        return;

    // weapon = dynamic_cast<CWeapon*>(item)  (CItem -> CWeapon)
    void* weapon = RtDynamicCast(
        item, 0,
        reinterpret_cast<void*>(MG_CONST(0x011962E8)),   // CItem RTTI type descriptor
        reinterpret_cast<void*>(MG_CONST(0x0119FE24)),   // CWeapon RTTI type descriptor
        0);
    if (!weapon)
        return;

    int category = *reinterpret_cast<int*>(reinterpret_cast<u8*>(weapon) + kWeaponCategoryOffset);
    int index = crosshairIndexForCategory(category);
    if (index < 0)
        return;

    // NOTE: sniper (category 3) intentionally stays on the unzoomed reticle (index 4, which
    // carries the center dot). Zoom stage / scope / FOV are NOT replicated here because the
    // remote player's zoom state isn't networked yet — it will be driven explicitly once the
    // movement handler carries crosshair/zoom/FOV state.

    CrosshairSelect(mgr, index);
    CrosshairSetVisible(mgr, 0, 1);   // sets mgr+4 (in a battle), enabling the gap setters below

    // ── Continuous spread (gap), mirroring the engine's own per-weapon HUD update so the
    // observer reticle matches the firing player. CRITICAL: each weapon drives ONLY the gap
    // dimension(s) it actually uses (verified in IDA) — pushing an extra one writes a bogus
    // value that collapses the reticle:
    //   rifle    (cat1,idx1): sub_58D440(25) + sub_58D480(26)   <- the ONLY one using dim 26
    //   shotgun  (cat2,idx2): sub_58D440(25)            (sub_B44890)
    //   gatling  (cat4,idx3): sub_58D440(25)            (sub_B291F0)
    //   launcher (cat5,idx9): sub_58D440(25)            (sub_B3E6A0)
    //   melee(0)/sniper(3)/grenade(6): no continuous gap
    // The old code pushed sub_58D480 for EVERY weapon -> the "shotgun gap too closed" and the
    // hidden reticle dot. The gap is set BEFORE UpdateAll (which applies it via the reticle's
    // per-frame vtbl+8).
    const bool usesOuterGap = (category == 1 || category == 2 || category == 4 || category == 5);
    const bool usesInnerGap = (category == 1);
    {
        auto weaponVtbl = *reinterpret_cast<void***>(weapon);
        auto getAttr    = reinterpret_cast<WeaponGetAttrFunc>(weaponVtbl[kWeaponAttrVtblIndex]);
        const double scale = mg::readValue<double>(MG_CONST(kSpreadScaleConst));
        if (usesOuterGap)
            CrosshairSetSpread1(mgr, static_cast<float>(getAttr(weapon, kWeaponSpreadAttrOuter) * scale));
        if (usesInnerGap)
            CrosshairSetSpread2(mgr, static_cast<float>(getAttr(weapon, kWeaponSpreadAttrInner) * scale));
    }

    CrosshairUpdateAll(mgr, 0);   // advance/apply each reticle's per-frame animation (uses the gap set above)

    // ── On-target state + center dot — must run AFTER UpdateAll, whose per-entity vtbl+60 pass
    // resets the on-target flag (a3) to 0; running these last makes them stick.
    {
        const u8 onTarget = (WeaponIsOnTarget(weapon) > 0) ? 1 : 0;

        // rifle/shotgun/launcher push the on-target state (engine: sub_58D790(mgr,0,onTarget)).
        if (category == 1 || category == 2 || category == 5)
            CrosshairSetState(mgr, 0, onTarget);

        // Center dot (redpoint, CCrosshairPoint @ mgr+0x38). The engine shows it for indices
        // 1/3/9 (rifle/gatling/launcher) — drive it explicitly so it's reliable in spectate.
        // The sniper (index 4) has no engine dot, so show it on-target (agreed behavior). Other
        // weapons carry their dot INSIDE the reticle nif (which renders fine now that we no
        // longer collapse the reticle), so we leave the redpoint hidden for them.
        void* redpoint = *reinterpret_cast<void**>(
            reinterpret_cast<u8*>(mgr) + kCrosshairRedpointOffset);
        if (redpoint)
        {
            if (category == 1 || category == 4 || category == 5)   // idx 1/3/9 -> always shown
                CrosshairPointSetVisible(redpoint, mgr, 1);
            else if (category == 3)                                 // sniper -> on-target
                CrosshairPointSetVisible(redpoint, mgr, onTarget);
        }
    }
}

// ── Main spectate hook ───────────────────────────────────────────────────────

void __fastcall hkSpectate(CCameraTPS* cam, u32 /*edx*/, float idk)
{
    // FPS overlay
    auto fps = reinterpret_cast<u32*>(MG_CONST(0x11D81F0));
    auto my_fps = fps ? *fps : 0u;
    u32 y_pos = 10;

    char fpsBuf[32];
    int fpsLen = sprintf_s(fpsBuf, "FPS: %u", my_fps);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fpsBuf, fpsLen);
    y_pos += 20;

    // ── Only handle GAME or WATCH control modes ──────────────────────────
    if (cam->m_eControlMode == ControlMode_GAME ||
        cam->m_eControlMode == ControlMode_WATCH)
    {
        // ═══════════════════════════════════════════════════════════════════
        // BRANCH 1: WATCH + WATCH_CAMERA_VIEW (spectating another player)
        // ═══════════════════════════════════════════════════════════════════
        if (cam->m_eControlMode == ControlMode_WATCH &&
            cam->m_eViewMode == ViewMode_WATCH_CAMERA_VIEW)
        {
            sub_5778F0(cam, idk);

            // Show the spectated player's weapon crosshair (incl. sniper zoom).
            driveSpectatorCrosshair(cam);

            if (cam->CGamePlayer->CPlayerModelProperty)
            {
                float tmp2PrevBonePos[3] = {0, 0, 0};

                if (cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode)
                {
                    cam->m_vBonePos[0] = cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[0];
                    cam->m_vBonePos[1] = cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[1];
                    cam->m_vBonePos[2] = cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[2];
                }

                cam->m_vBonePos[2] += cam->CGamePlayer->CPlayerModelProperty->idk_c8;
                tmp2PrevBonePos[0] = cam->m_vBonePos[0];
                tmp2PrevBonePos[1] = cam->m_vBonePos[1];
                tmp2PrevBonePos[2] = cam->m_vBonePos[2];

                // Crouch detection for spectated player
                if (old_move_state_anim != cam->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id)
                {
                    auto anim = cam->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id;
                    if (anim == kNormalCrouchAnimId || anim == kRightCrouchAnimId ||
                        anim == kLeftCrouchAnimId || anim == kForwardCrouchAnimId ||
                        anim == kBackwardCrouchAnimId)
                    {
                        cam->IsCrouch = 1;
                        cam->IsCrouchAnimActive = 1;
                        cam->FinalCurrHeight = 30.0f;
                        reset_crouch = true;
                    }
                    else if (old_move_state_anim == kNormalCrouchAnimId ||
                             old_move_state_anim == kRightCrouchAnimId ||
                             old_move_state_anim == kLeftCrouchAnimId ||
                             old_move_state_anim == kForwardCrouchAnimId ||
                             old_move_state_anim == kBackwardCrouchAnimId)
                    {
                        cam->IsCrouch = 0;
                        cam->IsCrouchAnimActive = 1;
                        cam->FinalCurrHeight = 90.0f;
                    }
                    old_move_state_anim = cam->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id;
                }

                sub_577D90(cam); // crouch animation processing
                cam->m_vBonePos[2] += cam->CurrHeight;
                tmp2PrevBonePos[2] += cam->CurrHeight;

                // Calculate yaw and pitch from player model
                auto my_yaw = cam->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Yaw
                            + cam->CGamePlayer->CPlayerModelProperty->Rotation;
                auto my_pitch = cam->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Pitch
                              + cam->CGamePlayer->CPlayerModelProperty->Pitch
                              + cam->CGamePlayer->CPlayerModelProperty->idk_c4
                              + cam->CGamePlayer->CPlayerModelProperty->idk_ac;

                NormalizeRadian(&my_yaw);
                //clamp_yaw(&my_yaw);
                NormalizeRadian(&my_pitch);

                sub_578030(cam);
                sub_577EE0(cam);

                float v3 = 0.0f;
                if (my_pitch < (PI + HALF_PI))
                    v3 = -my_pitch * PITCH_50;
                else
                    v3 = (TWO_PI - my_pitch) * PITCH_50;
                cam->ViewOffsetByPitch = v3;

                // Normalize forward vector
                auto matrix = cam->Matrix;
                auto forward_x = &matrix->matrix_13;
                auto forward_y = &matrix->matrix_23;
                auto forward_z = &matrix->matrix_33;
                *forward_z = 0.0f;

                float length = sqrt((*forward_x) * (*forward_x) + (*forward_y) * (*forward_y) + (*forward_z) * (*forward_z));
                if (length > *reinterpret_cast<double*>(MG_CONST(0x00FEB350)))
                {
                    float invLength = 1.0f / length;
                    *forward_x *= invLength;
                    *forward_y *= invLength;
                    *forward_z *= invLength;
                }
                else
                {
                    *forward_x = 0.0f;
                    *forward_y = 0.0f;
                    *forward_z = 0.0f;
                }

                float v40[9] = {0};
                float v49[9] = {0};
                float v50[9] = {0};

                auto matrix2 = cam->Matrix;
                auto right_x = matrix2->matrix_11 * cam->float114;
                auto right_y = matrix2->matrix_21 * cam->float114;
                auto right_z = matrix2->matrix_31 * cam->float114;

                v40[6] = right_x;
                v40[7] = right_y;
                v40[8] = right_z;
                auto v22 = right_x + cam->m_vBonePos[0];
                auto v23 = right_y + cam->m_vBonePos[1];
                auto v24 = right_z + cam->m_vBonePos[2];
                v40[3] = v22;
                v40[4] = v23;
                v40[5] = v24;
                tmp2PrevBonePos[0] = v22;
                tmp2PrevBonePos[1] = v23;
                tmp2PrevBonePos[2] = v24;

                NiMatrix3FromEulerAnglesZYX(v49, my_yaw, my_pitch, 0.0f);
                v40[0] = -cam->idkk[0];
                v40[1] = 0.0f;
                v40[2] = 0.0f;

                float v39[3] = {0};
                auto v18 = sub_465A40(v49, v39, v40);
                cam->WorldPos[0] = tmp2PrevBonePos[0] + v18[0];
                cam->WorldPos[1] = tmp2PrevBonePos[1] + v18[1];
                cam->WorldPos[2] = tmp2PrevBonePos[2] + v18[2];

                float previous_world_pos[3] = {cam->WorldPos[0], cam->WorldPos[1], cam->WorldPos[2]};

                if (my_pitch < (PI + HALF_PI))
                    my_pitch = my_pitch * static_cast<float>(DIFFERENCE_IDK) + my_pitch;
                else
                    my_pitch = (TWO_PI - my_pitch) * static_cast<float>(DIFFERENCE_IDK) + my_pitch;

                NiMatrix3FromEulerAnglesZYX(v50, my_yaw, my_pitch, 0.0f);

                if (cam->byte148)
                    sub_5797B0(cam, cam->m_vBonePos, tmp2PrevBonePos, previous_world_pos);

                sub_579690(cam, v50, previous_world_pos, 0.0f);
                cam->float74 = previous_world_pos[0];
                cam->float78 = previous_world_pos[1];
                cam->float7C = previous_world_pos[2];
                cam->NiAVObject->m_localTranslate[0] = previous_world_pos[0];
                cam->NiAVObject->m_localTranslate[1] = previous_world_pos[1];
                cam->NiAVObject->m_localTranslate[2] = previous_world_pos[2];
                cam->NiAVObject->m_localRotate[0] = v50[0];
                cam->NiAVObject->m_localRotate[1] = v50[1];
                cam->NiAVObject->m_localRotate[2] = v50[2];
                cam->NiAVObject->m_localRotate[3] = v50[3];
                cam->NiAVObject->m_localRotate[4] = v50[4];
                cam->NiAVObject->m_localRotate[5] = v50[5];
                cam->NiAVObject->m_localRotate[6] = v50[6];
                cam->NiAVObject->m_localRotate[7] = v50[7];
                cam->NiAVObject->m_localRotate[8] = v50[8];

                NiAVObjectUpdate(cam->NiAVObject, 0.0f, 1);
                sub_57B550(cam);
                cam->float4 = sub_4586C0(my_yaw, cam->RotationX);
                cam->float8 = sub_4586C0(my_pitch, cam->RotationY);
                cam->RotationX = my_yaw;
                cam->RotationY = my_pitch;
            }
        }
        // ═══════════════════════════════════════════════════════════════════
        // BRANCH 2: Normal play mode (ControlMode_GAME or other WATCH modes)
        // ═══════════════════════════════════════════════════════════════════
        else
        {
            // Live first-person/third-person play: make sure a respawn (esp. with the
            // ESC menu open) didn't leave the local crosshair stuck hidden.
            if (cam->m_eControlMode == ControlMode_GAME)
                repairLocalCrosshairIfHidden(cam);

            auto dword_11DE160 = reinterpret_cast<DWORD*>(MG_CONST(0x011DE160));
            auto flt_11DE13C   = reinterpret_cast<float*>(MG_CONST(0x011DE13C));

            if ((*dword_11DE160 & 1) == 0)
                *dword_11DE160 |= 1u;

            flt_11DE13C[0] = 1.0f; flt_11DE13C[1] = 0.0f; flt_11DE13C[2] = 0.0f;
            flt_11DE13C[3] = 0.0f; flt_11DE13C[4] = 1.0f; flt_11DE13C[5] = 0.0f;
            flt_11DE13C[6] = 0.0f; flt_11DE13C[7] = 0.0f; flt_11DE13C[8] = 1.0f;

            if (cam->byte149)
            {
                // Direct coordinate follow mode
                auto my_coords = cam->CGamePlayer->CExPlayer->MovementInfo->coords;
                float idk2[9] = {0};
                sub_579690(cam, idk2, my_coords, 0.0f);
                cam->NiAVObject->m_localTranslate[0] = my_coords[0];
                cam->NiAVObject->m_localTranslate[1] = my_coords[1];
                cam->NiAVObject->m_localTranslate[2] = my_coords[2];
                cam->NiAVObject->m_localRotate[0] = idk2[0];
                cam->NiAVObject->m_localRotate[1] = idk2[1];
                cam->NiAVObject->m_localRotate[2] = idk2[2];
                cam->NiAVObject->m_localRotate[3] = idk2[3];
                cam->NiAVObject->m_localRotate[4] = idk2[4];
                cam->NiAVObject->m_localRotate[5] = idk2[5];
                cam->NiAVObject->m_localRotate[6] = idk2[6];
                cam->NiAVObject->m_localRotate[7] = idk2[7];
                cam->NiAVObject->m_localRotate[8] = idk2[8];
                NiAVObjectUpdate(cam->NiAVObject, 0.0f, 1);
                sub_57B550(cam);
            }
            else
            {
                sub_5778F0(cam, idk);

                if (cam->CGamePlayer->CPlayerModelProperty)
                {
                    float tmp2PrevBonePos[3] = {0, 0, 0};

                    // ── First person: resolve head bone ──────────────────
                    if (cam->m_eViewMode == ViewMode_FirstPerson)
                    {
                        auto character_bone_type = cam->CGamePlayer->CPlayerModelProperty->character_bone_type;
                        float bone_pos[3] = {0, 0, 0};

                        if (character_bone_type == 0 && cam->CGamePlayer->CPlayerModelProperty->HumanBones)
                        {
                            bone_pos[0] = cam->CGamePlayer->CPlayerModelProperty->HumanBones->position[0];
                            bone_pos[1] = cam->CGamePlayer->CPlayerModelProperty->HumanBones->position[1];
                            bone_pos[2] = cam->CGamePlayer->CPlayerModelProperty->HumanBones->position[2];
                        }
                        else if (character_bone_type == 1 && cam->CGamePlayer->CPlayerModelProperty->ZombieBones)
                        {
                            bone_pos[0] = cam->CGamePlayer->CPlayerModelProperty->ZombieBones->position[0];
                            bone_pos[1] = cam->CGamePlayer->CPlayerModelProperty->ZombieBones->position[1];
                            bone_pos[2] = cam->CGamePlayer->CPlayerModelProperty->ZombieBones->position[2];
                        }
                        else if (character_bone_type == 2 && cam->CGamePlayer->CPlayerModelProperty->ZombieKingBones)
                        {
                            bone_pos[0] = cam->CGamePlayer->CPlayerModelProperty->ZombieKingBones->position[0];
                            bone_pos[1] = cam->CGamePlayer->CPlayerModelProperty->ZombieKingBones->position[1];
                            bone_pos[2] = cam->CGamePlayer->CPlayerModelProperty->ZombieKingBones->position[2];
                        }
                        else
                        {
                            auto bone_info = reinterpret_cast<float*>(MG_CONST(0x011ED050));
                            bone_pos[0] = bone_info[0];
                            bone_pos[1] = bone_info[1];
                            bone_pos[2] = bone_info[2];
                        }

                        cam->m_vBonePos[0] = bone_pos[0];
                        cam->m_vBonePos[1] = bone_pos[1];
                        cam->m_vBonePos[2] = bone_pos[2];
                        tmp2PrevBonePos[0] = cam->m_vBonePos[0];
                        tmp2PrevBonePos[1] = cam->m_vBonePos[1];
                        tmp2PrevBonePos[2] = cam->m_vBonePos[2];
                    }
                    // ── Third person: use root node + offset ─────────────
                    else
                    {
                        if (cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode)
                        {
                            cam->m_vBonePos[0] = cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[0];
                            cam->m_vBonePos[1] = cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[1];
                            cam->m_vBonePos[2] = cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[2];
                        }

                        cam->m_vBonePos[2] += cam->CGamePlayer->CPlayerModelProperty->idk_c8;
                        tmp2PrevBonePos[0] = cam->m_vBonePos[0];
                        tmp2PrevBonePos[1] = cam->m_vBonePos[1];
                        tmp2PrevBonePos[2] = cam->m_vBonePos[2];
                    }

                    // Reset crouch when transitioning from watch to game
                    if (reset_crouch)
                    {
                        cam->IsCrouch = 0;
                        cam->IsCrouchAnimActive = 0;
                        cam->FinalCurrHeight = 90.0f;
                        cam->CurrHeight = 90.0f;
                        reset_crouch = false;
                    }

                    sub_577D90(cam);
                    cam->m_vBonePos[2] += cam->CurrHeight;
                    tmp2PrevBonePos[2] += cam->CurrHeight;

                    auto my_yaw = cam->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Yaw
                                + cam->CGamePlayer->CPlayerModelProperty->Rotation;
                    auto my_pitch = cam->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Pitch
                                  + cam->CGamePlayer->CPlayerModelProperty->Pitch
                                  + cam->CGamePlayer->CPlayerModelProperty->idk_c4
                                  + cam->CGamePlayer->CPlayerModelProperty->idk_ac;

                    NormalizeRadian(&my_yaw);
                    NormalizeRadian(&my_pitch);

                    if (cam->m_eControlMode == ControlMode_WATCH)
                    {
                        my_yaw = 0.0f;
                        my_pitch = 0.0f;
                    }
                    if (cam->m_eViewMode == ViewMode_SELF_CAMERA_VIEW)
                        my_pitch = 0.0f;

                    sub_578030(cam);
                    sub_577EE0(cam);

                    float v3 = 0.0f;
                    if (my_pitch < (PI + HALF_PI))
                        v3 = -my_pitch * PITCH_50;
                    else
                        v3 = (TWO_PI - my_pitch) * PITCH_50;
                    cam->ViewOffsetByPitch = v3;

                    auto matrix = cam->Matrix;
                    auto forward_x = &matrix->matrix_13;
                    auto forward_y = &matrix->matrix_23;
                    auto forward_z = &matrix->matrix_33;
                    *forward_z = 0.0f;

                    float length = sqrt((*forward_x) * (*forward_x) + (*forward_y) * (*forward_y) + (*forward_z) * (*forward_z));
                    if (length > *reinterpret_cast<double*>(MG_CONST(0x00FEB350)))
                    {
                        float invLength = 1.0f / length;
                        *forward_x *= invLength;
                        *forward_y *= invLength;
                        *forward_z *= invLength;
                    }
                    else
                    {
                        *forward_x = 0.0f;
                        *forward_y = 0.0f;
                        *forward_z = 0.0f;
                    }

                    float v40[9] = {0};
                    float v49[9] = {0};
                    float v50[9] = {0};

                    if (cam->m_eViewMode != ViewMode_ZoomIn || cam->m_eViewMode != ViewMode_DoubleZoomIn)
                    {
                        auto matrix2 = cam->Matrix;
                        auto v25 = cam->float114;
                        auto v26 = matrix2->matrix_11 * v25;
                        auto v27 = matrix2->matrix_21 * v25;
                        auto v28 = matrix2->matrix_31 * v25;
                        v40[6] = v26;
                        v40[7] = v27;
                        v40[8] = v28;
                        auto v22 = v26 + cam->m_vBonePos[0];
                        auto v23 = v27 + cam->m_vBonePos[1];
                        auto v24 = v28 + cam->m_vBonePos[2];
                        v40[3] = v22;
                        v40[4] = v23;
                        v40[5] = v24;
                        tmp2PrevBonePos[0] = v22;
                        tmp2PrevBonePos[1] = v23;
                        tmp2PrevBonePos[2] = v24;
                    }

                    auto v14 = my_pitch + cam->CameraPitch;
                    auto v13 = my_yaw + cam->CameraYaw;
                    NiMatrix3FromEulerAnglesZYX(v49, v13, v14, 0.0f);
                    v40[0] = -cam->idkk[15 * cam->m_eViewMode];
                    v40[1] = 0.0f;
                    v40[2] = 0.0f;

                    float v39[3] = {0};
                    auto v18 = sub_465A40(v49, v39, v40);
                    cam->WorldPos[0] = tmp2PrevBonePos[0] + v18[0];
                    cam->WorldPos[1] = tmp2PrevBonePos[1] + v18[1];
                    cam->WorldPos[2] = tmp2PrevBonePos[2] + v18[2];

                    float previous_world_pos[3] = {cam->WorldPos[0], cam->WorldPos[1], cam->WorldPos[2]};

                    if (my_pitch < (PI + HALF_PI))
                        my_pitch = my_pitch * static_cast<float>(DIFFERENCE_IDK) + my_pitch;
                    else
                        my_pitch = (TWO_PI - my_pitch) * static_cast<float>(DIFFERENCE_IDK) + my_pitch;

                    // Compute final rotation matrix based on control mode
                    if (cam->m_eControlMode == ControlMode_GAME)
                    {
                        if (cam->m_eViewMode == ViewMode_WATCH_CAMERA_VIEW ||
                            cam->m_eViewMode == ViewMode_SELF_CAMERA_VIEW)
                        {
                            auto v11 = my_pitch + cam->float90 + cam->CameraPitch;
                            auto v10 = my_yaw + cam->float8C + cam->CameraYaw;
                            NiMatrix3FromEulerAnglesZYX(v50, v10, v11, cam->float30);
                        }
                        else
                        {
                            auto v9 = my_pitch + cam->CameraPitch;
                            auto v8 = my_yaw + cam->CameraYaw;
                            NiMatrix3FromEulerAnglesZYX(v50, v8, v9, cam->float30);
                        }
                    }
                    else
                    {
                        auto v7 = my_pitch + cam->float90 + cam->CameraPitch;
                        auto v6 = my_yaw + cam->float8C + cam->CameraYaw;
                        NiMatrix3FromEulerAnglesZYX(v50, v6, v7, cam->float30);
                    }

                    if (cam->byte148)
                        sub_5797B0(cam, cam->m_vBonePos, tmp2PrevBonePos, previous_world_pos);

                    sub_579690(cam, v50, previous_world_pos, 0.0f);
                    cam->float74 = previous_world_pos[0];
                    cam->float78 = previous_world_pos[1];
                    cam->float7C = previous_world_pos[2];
                    cam->NiAVObject->m_localTranslate[0] = previous_world_pos[0];
                    cam->NiAVObject->m_localTranslate[1] = previous_world_pos[1];
                    cam->NiAVObject->m_localTranslate[2] = previous_world_pos[2];
                    cam->NiAVObject->m_localRotate[0] = v50[0];
                    cam->NiAVObject->m_localRotate[1] = v50[1];
                    cam->NiAVObject->m_localRotate[2] = v50[2];
                    cam->NiAVObject->m_localRotate[3] = v50[3];
                    cam->NiAVObject->m_localRotate[4] = v50[4];
                    cam->NiAVObject->m_localRotate[5] = v50[5];
                    cam->NiAVObject->m_localRotate[6] = v50[6];
                    cam->NiAVObject->m_localRotate[7] = v50[7];
                    cam->NiAVObject->m_localRotate[8] = v50[8];

                    NiAVObjectUpdate(cam->NiAVObject, 0.0f, 1);
                    sub_57B550(cam);
                    cam->float4 = sub_4586C0(my_yaw, cam->RotationX);
                    cam->float8 = sub_4586C0(my_pitch, cam->RotationY);
                    cam->RotationX = my_yaw;
                    cam->RotationY = my_pitch;
                }
            }
        }
    }
    // ═══════════════════════════════════════════════════════════════════════
    // BRANCH 3: Other control modes (FREE, TARGET, UNKNOWN)
    // ═══════════════════════════════════════════════════════════════════════
    else
    {
        sub_56F5C0(cam, idk);
        sub_57B550(cam);
    }
    /*
    char buf[512];
    int len = 0;

    len = sprintf_s(buf, sizeof(buf), "CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c8: %f",
        cam->CGamePlayer->CPlayerModelProperty->idk_c8);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "CCameraTPS->CurrHeight: %f", cam->CurrHeight);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "CCameraTPS->FinalCurrHeight: %f", cam->FinalCurrHeight);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "CCameraTPS->IsCrouch: %d", cam->IsCrouch);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "CCameraTPS->IsCrouchAnimActive: %d", cam->IsCrouchAnimActive);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "CCameraTPS->IsCrouchIdk1: %d", cam->IsCrouchIdk1);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "CCameraTPS->IsCrouchIdk2: %d", cam->IsCrouchIdk2);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "MoveState anim_id: %d",
        cam->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "ActionState anim_id: %d",
        cam->CGamePlayer->CPlayerActionStateMgr->PlayerActionState->anim_id);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "ViewMode: %d", cam->m_eViewMode);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "idkk[15 * ViewMode]: %f",
        cam->idkk[15 * cam->m_eViewMode]);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "float90: %f", cam->float90);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "float8C: %f", cam->float8C);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "float4: %f", cam->float4);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "float8: %f", cam->float8);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "float114: %f", cam->float114);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "RotationX: %f", cam->RotationX);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "RotationY: %f", cam->RotationY);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "CameraPitch: %f", cam->CameraPitch);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "CameraYaw: %f", cam->CameraYaw);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "float30: %f", cam->float30);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "Room Yaw: %f",
        cam->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Yaw);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "Model Rotation: %f",
        cam->CGamePlayer->CPlayerModelProperty->Rotation);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    float my_yaw =
        cam->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Yaw +
        cam->CGamePlayer->CPlayerModelProperty->Rotation;
    normalizeRadians(&my_yaw);

    len = sprintf_s(buf, sizeof(buf), "final my_yaw: %f", my_yaw);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "Room Pitch: %f",
        cam->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Pitch);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "Model Pitch: %f",
        cam->CGamePlayer->CPlayerModelProperty->Pitch);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "idk_c4: %f",
        cam->CGamePlayer->CPlayerModelProperty->idk_c4);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "idk_ac: %f",
        cam->CGamePlayer->CPlayerModelProperty->idk_ac);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    float my_pitch =
        cam->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Pitch +
        cam->CGamePlayer->CPlayerModelProperty->Pitch +
        cam->CGamePlayer->CPlayerModelProperty->idk_c4 +
        cam->CGamePlayer->CPlayerModelProperty->idk_ac;
    normalizeRadians(&my_pitch);

    len = sprintf_s(buf, sizeof(buf), "final my_pitch: %f", my_pitch);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "idk_b0: %f", cam->CGamePlayer->CPlayerModelProperty->idk_b0);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "idk_b4: %f", cam->CGamePlayer->CPlayerModelProperty->idk_b4);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "idk_b8: %f", cam->CGamePlayer->CPlayerModelProperty->idk_b8);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "idk_bc: %f", cam->CGamePlayer->CPlayerModelProperty->idk_bc);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "idk_c0: %f", cam->CGamePlayer->CPlayerModelProperty->idk_c0);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "idk_c4: %f", cam->CGamePlayer->CPlayerModelProperty->idk_c4);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;

    len = sprintf_s(buf, sizeof(buf), "idk_c8: %f", cam->CGamePlayer->CPlayerModelProperty->idk_c8);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), buf, len);
    y_pos += 20;
    */

}
} // anonymous namespace
// ── SpectatePov class ─────────────────────────────────────────────────────────

SpectatePov::SpectatePov(MegaGuardContext& ctx) : ctx_(ctx) {}
SpectatePov::~SpectatePov() = default;

VoidResult SpectatePov::install()
{
    // Resolve all engine function pointers via MG_CONST
    sub_57B550                  = reinterpret_cast<Sub57B550Func>(MG_CONST(0x0057B550));
    sub_579690                  = reinterpret_cast<Sub579690Func>(MG_CONST(0x00579690));
    sub_5778F0                  = reinterpret_cast<Sub5778F0Func>(MG_CONST(0x005778F0));
    sub_577D90                  = reinterpret_cast<Sub577D90Func>(MG_CONST(0x00577D90));
    NormalizeRadian             = reinterpret_cast<Sub4585C0Func>(MG_CONST(0x004585C0));
    sub_578030                  = reinterpret_cast<Sub578030Func>(MG_CONST(0x00578030));
    sub_577EE0                  = reinterpret_cast<Sub577EE0Func>(MG_CONST(0x00577EE0));
    NiMatrix3FromEulerAnglesZYX = reinterpret_cast<NiMatrix3FromEulerAnglesZYXFunc>(MG_CONST(0x00CCA8E0));
    sub_465A40                  = reinterpret_cast<Sub465A40Func>(MG_CONST(0x00465A40));
    sub_5797B0                  = reinterpret_cast<Sub5797B0Func>(MG_CONST(0x005797B0));
    NiAVObjectUpdate            = reinterpret_cast<NiAVObjectUpdateFunc>(MG_CONST(0x00CC7660));
    sub_4586C0                  = reinterpret_cast<Sub4586C0Func>(MG_CONST(0x004586C0));
    sub_56F5C0                  = reinterpret_cast<Sub56F5C0Func>(MG_CONST(0x0056F5C0));
    sub_430760                  = reinterpret_cast<Sub430760Func>(MG_CONST(0x00430760));

    // Crosshair manager + helpers (for observer-mode crosshairs)
    CWorldGetInstance           = reinterpret_cast<CWorldGetInstanceFunc>(MG_CONST(0x00563420));
    CrosshairSelect             = reinterpret_cast<CrosshairSelectFunc>(MG_CONST(0x0058D5F0));
    CrosshairSetVisible         = reinterpret_cast<CrosshairSetVisFunc>(MG_CONST(0x0058D560));
    CrosshairUpdateAll          = reinterpret_cast<CrosshairUpdateFunc>(MG_CONST(0x0058D6D0));
    RtDynamicCast               = reinterpret_cast<RtDynamicCastFunc>(MG_CONST(0x00F19972));
    CrosshairSetSpread1         = reinterpret_cast<CrosshairSetSpreadFunc>(MG_CONST(0x0058D440));
    CrosshairSetSpread2         = reinterpret_cast<CrosshairSetSpreadFunc>(MG_CONST(0x0058D480));
    CrosshairSetState           = reinterpret_cast<CrosshairSetStateFunc>(MG_CONST(0x0058D790));
    WeaponIsOnTarget            = reinterpret_cast<WeaponIsOnTargetFunc>(MG_CONST(0x00B573E0));
    CrosshairPointSetVisible    = reinterpret_cast<CrosshairPointVisFunc>(MG_CONST(0x0058E170));
    LocalCrosshairRefresh       = reinterpret_cast<LocalCrosshairRefreshFunc>(MG_CONST(0x00BA1550));

    auto& registry = ctx_.hookRegistry();
    registry.registerDetour(HookId::SpectateCamera)
        .create(MG_CONST(addr::features::SpectateObserver), hkSpectate);

    return VoidResult::ok();
}

} // namespace mg::modding
