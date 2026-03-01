// =============================================================================
// WeaponRestriction - Room dialog weapon restriction bugfix
// =============================================================================
// Ported from weaponrestriction_roomsettings.h
// Three hooks fix weapon restriction UI in room create/settings/main dialogs.
// =============================================================================
#include "pch.hpp"
#include "modding/bugfixes/weapon_restriction.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::modding {

using namespace mg::game;

namespace {

// ── RoomCreateDialogHandler ────────────────────────────────────────────────

void __fastcall hkRoomCreateDialogHandler(
    u32 crtRoomDlgInstance, u32 edx, u32 a2, u32 dialog_id, u32 a4)
{
    auto& registry = mg::ctx().hookRegistry();
    auto original = registry.findDetour(HookId::WR_RoomCreate)
        ->getOriginal<decltype(&hkRoomCreateDialogHandler)>();

    if (a2 == 257)
    {
        switch (dialog_id)
        {
            case 107022:
            {
                auto mod_id = mg::readValue<u32>(
                    mg::readValue<uptr>(crtRoomDlgInstance, 2060), 4);

                if (mod_id != 4 && mod_id != 7 && mod_id != 8 && mod_id != 11)
                {
                    auto weapon_restriction_id = mg::readValue<u32>(
                        mg::readValue<uptr>(crtRoomDlgInstance, 2060), 36);

                    mg::call<void(__thiscall*)(u32, const char*, u32, u32)>(
                        MG_CONST(0x692430),
                        crtRoomDlgInstance,
                        MG_STR("E_DLG_Q_MODE_SEL"),
                        8,
                        weapon_restriction_id);
                }
                break;
            }
        }
    }
    original(crtRoomDlgInstance, edx, a2, dialog_id, a4);
}

// ── RoomSettingDialogHandler ─────────────────────────────────────────────────

void __fastcall hkRoomSettingDialogHandler(
    u32 roomSettingDlgInstance, u32 edx, u32 eventType, u32 dialogId, u32 a4)
{
    auto& registry = mg::ctx().hookRegistry();
    auto original = registry.findDetour(HookId::WR_RoomSetting)
        ->getOriginal<decltype(&hkRoomSettingDialogHandler)>();

    if (eventType == 257)
    {
        switch (dialogId)
        {
            case 103053:
            {
                auto mod_id = mg::readValue<u32>(roomSettingDlgInstance, 2064);

                if (mod_id != 4 && mod_id != 7 && mod_id != 8 && mod_id != 11)
                {
                    mg::call<void(__thiscall*)(u32, const char*, u32, u32)>(
                        MG_CONST(0x66DDB0),
                        roomSettingDlgInstance,
                        MG_STR("E_DLG_Q_MODE_SEL"),
                        8,
                        mg::readValue<u32>(roomSettingDlgInstance, 2060));
                }
                break;
            }
        }
    }
    original(roomSettingDlgInstance, edx, eventType, dialogId, a4);
}

// ── RoomMainDialogHandler ────────────────────────────────────────────────────

using tGetCRoom = u32*(__cdecl*)();

void __fastcall hkRoomMainDialogHandler(
    u32 roomSettingDlgInstance, u32 edx, u32 eventType, u32 dialogId, u32 a4)
{
    auto& registry = mg::ctx().hookRegistry();
    auto original = registry.findDetour(HookId::WR_RoomMain)
        ->getOriginal<decltype(&hkRoomMainDialogHandler)>();

    // Dereference the pointer at ui_msgbox::Get to get the actual CUIMsgBox*
    auto* UIMsgBox = *reinterpret_cast<CUIMsgBox**>(
        MG_CONST(addr::ui::ui_msgbox::Get));

    bool checkVendorLobby = UIMsgBox && mg::call<u32*(__thiscall*)(CUIMsgBox*, const char*)>(
        MG_CONST(addr::ui::FindDialog),
        UIMsgBox,
        MG_STR("E_DLG_VENDOR_LOBBY"));

    if (!checkVendorLobby && eventType == 257)
    {
        switch (dialogId)
        {
            case 101069:
            {
                // Call CRoom::Get() directly via original game address
                auto GetCRoom = reinterpret_cast<tGetCRoom>(
                    MG_CONST(addr::anticheat::game_managers::room::Get));
                auto* room = GetCRoom();
                if (!room) break;

                auto mod_id = mg::callVFunc<u32>(room, 15);

                if (mod_id != 4 && mod_id != 7 && mod_id != 8 && mod_id != 11)
                {
                    mg::call<void(__thiscall*)(u32, const char*, u32, u32)>(
                        MG_CONST(0x6609C0),
                        roomSettingDlgInstance,
                        MG_STR("E_DLG_Q_MODE_SEL"),
                        8,
                        mg::readValue<u32>(reinterpret_cast<u32>(room), 236));
                }
                break;
            }
        }
    }
    original(roomSettingDlgInstance, edx, eventType, dialogId, a4);
}

} // anonymous namespace

// ── WeaponRestriction class ──────────────────────────────────────────────────

WeaponRestriction::WeaponRestriction(MegaGuardContext& ctx) : ctx_(ctx) {}
WeaponRestriction::~WeaponRestriction() = default;

VoidResult WeaponRestriction::install()
{
    auto& registry = ctx_.hookRegistry();

    registry.registerDetour(HookId::WR_RoomCreate)
        .create(MG_CONST(addr::bugfixes::RoomCreateDialogHandler), hkRoomCreateDialogHandler);

    registry.registerDetour(HookId::WR_RoomSetting)
        .create(MG_CONST(addr::bugfixes::RoomSettingsDialogHandler), hkRoomSettingDialogHandler);

    registry.registerDetour(HookId::WR_RoomMain)
        .create(MG_CONST(addr::bugfixes::RoomMainDialogHandler), hkRoomMainDialogHandler);

    return VoidResult::ok();
}

} // namespace mg::modding
