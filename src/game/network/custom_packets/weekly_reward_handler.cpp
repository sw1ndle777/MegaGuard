#include "pch.hpp"
#include "game/network/custom_packets/weekly_reward_handler.hpp"
#include "game/network/custom_packets/custom_packet_dispatcher.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "utils/call_helper.hpp"
#include "utils/logger.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::game {

namespace {

// ── Type aliases ─────────────────────────────────────────────────────────────
// The dialog is server-driven (data pushed via order 179); no client request is
// sent, so no net-manager send path is needed here.

using tGetDialogInfo = int* (__thiscall*)(CGUIManager*, const char*);

// ── Custom vtable for weekly reward dialog ──────────────────────────────────

constexpr int kVtableSize = 26;
uptr g_rewardMenuVtbl[kVtableSize] = {};   // E_DLG_REWARD_MENU custom vtable

// ── Monthly-only build ───────────────────────────────────────────────────────
// Weekly reward, Play Time reward and Battle Pass (MICROPASS) are DISABLED. Their
// implementations are kept below but excluded from compilation via `#if 0` guards
// so the features can be restored later. Only the Monthly event path is live; the
// lobby Reward dropdown (E_DLG_REWARD_MENU) keeps a single "Monthly" choice.
//
// Original base-class OnMessage typedef, kept for the (disabled) dialog hooks.
using tDlgOnMsg = u32(__fastcall*)(u32 _this, u32 edx, int msgType, int ctrlId, int a4);

void ShowMonthlyEventDialog();
void ShowRewardMenu();

// ── Dialog OnMessage (vtable[3]) — close on confirm button ─────────────────

// Display-only, matching native WeeklyLoginRewardEventDialog::OnMessage (sub_6B37B0).
// The real dialog sends NO claim packet — rewards are pushed by the server on login.
// Day buttons are informational; only the close button (121008) acts. Sending a
// claim request here (as the old build did) is what crashed on stray clicks.
#if 0  // ── DISABLED: weekly reward + play-time dialog hooks (monthly-only build) ──
u32 __fastcall hkWeeklyRewardOnMessage(u32 _this, u32 edx, int msgType, int buttonId, int a4)
{
    if (msgType == 257)
    {
        // Day buttons are display-only and the close button just hides the dialog.
        // Both are consumed here so we never re-enter the native claim path that
        // used to crash on stray clicks.
        if (buttonId >= 121001 && buttonId <= 121007)
        {
            mg::ctx().logger().info("[WeeklyReward] Day {} clicked (display-only)", buttonId - 121000);
            return 1;
        }
        if (buttonId == 121008) // close button
        {
            u32 msgBox = *reinterpret_cast<u32*>(MG_CONST(addr::ui::ui_msgbox::Get));
            if (msgBox)
                mg::call<char(__thiscall*)(u32, const char*)>(
                    MG_CONST(addr::ui::ui_msgbox::CloseDialog),
                    msgBox, MG_STR("E_DLG_WEEKLY_REWARD"));
            return 1;
        }
    }
    // Forward the slot-list (121033) notifications and all non-click messages
    // (mouse move/hover, etc.) to the base handler so the reward items stay
    // selectable/inspectable like the native monthly slots.
    if (g_weeklyRewardOrigOnMsg)
        return g_weeklyRewardOrigOnMsg(_this, edx, msgType, buttonId, a4);
    return 0;
}

// ── CExSLTMonthEvent offsets (MV) ─────────────────────────────────────────
// Derived from MV vtable[3] paint function at 0x7A5FA0.
// Vector of slot entries: begin at this+1984, end at this+1988.
// Each entry: DWORD[0]=claim status (0=unclaimed,1=claimed), DWORD[8]=item data ptr.
// Current day highlight: this+2088 (compared against slot index+1 for brightness).

constexpr u32 kSlotContainerOff = 1972;
constexpr u32 kSlotVecBeginOff  = 1984;
constexpr u32 kCurrentDayOff    = 2088;

// Replay a stamp control's stamp_rotate ScaleAppear, byte-for-byte as the native
// monthly OnShow (sub_6CE6A0): clear +1956, show (vtable+96), set appear param
// +1636=5, reset the control's animation state (sub_E8BA40), mark active +874=1.
// These are current-client offsets (the earlier +2136/+894 were ToyBattles values).
static void triggerStampAppear(u32 stamp)
{
    if (!stamp)
        return;
    u32 vtbl = mg::readValue<u32>(stamp, 0);
    auto showHideFn = reinterpret_cast<void(__thiscall*)(u32, int)>(
        mg::readValue<u32>(vtbl, 96));
    mg::writeValue<u8>(stamp, 1956, 0);
    showHideFn(stamp, 1);
    mg::writeValue<u32>(stamp, 1636, 5);
    mg::call<u32(__thiscall*)(u32)>(
        MG_CONST(addr::features::dlg::StampAppearReset), stamp);
    mg::writeValue<u8>(stamp, 874, 1);
}

// ── OnShow (vtable[8]) — populate slot list + stamps + text ─────────────────

void __fastcall hkWeeklyRewardOnShow(u32 _this, u32 edx)
{
    auto& logger = mg::ctx().logger();

    // Find SlotList control (SLT_WEEKLY, ID 121033)
    u32 slotId = 121033;
    auto slotResult = mg::call<u32(__thiscall*)(u32, u32*)>(
        MG_CONST(addr::features::dlg::GetDlgInfo),
        _this + 1712, &slotId);
    if (!slotResult)
    {
        logger.info("[WeeklyReward] SlotList 121033 not found");
        return;
    }
    u32 slotList = mg::readValue<u32>(slotResult, 0);
    if (!slotList)
        return;

    // Read state from reward data block at dialog+2080
    u16 claimState = mg::readValue<u16>(_this, 2080);
    u16 currentDay = mg::readValue<u16>(_this, 2082);
    logger.info("[WeeklyReward] claimState={}, currentDay={}", claimState, currentDay);

    // Set CExSLTMonthEvent's current day field for brightness highlighting
    mg::writeValue<u32>(slotList, kCurrentDayOff, static_cast<u32>(currentDay));

    // Read entry vector directly from CExSLTMonthEvent (offsets 1984/1988),
    // matching the paint function's access at sub_7A5FA0.
    u32 slotVtbl = mg::readValue<u32>(slotList, 0);
    constexpr u32 kCExSLTMonthEventVtbl = 0x1032FFC;
    if (slotVtbl != kCExSLTMonthEventVtbl)
    {
        mg::writeValue<u32>(slotList, 0, kCExSLTMonthEventVtbl);
        logger.info("[WeeklyReward] Patched SlotList vtable 0x{:X} -> 0x{:X}", slotVtbl, kCExSLTMonthEventVtbl);
    }

    u32 vecBegin = mg::readValue<u32>(slotList, kSlotVecBeginOff);
    u32 vecEnd   = mg::readValue<u32>(slotList, kSlotVecBeginOff + 4);
    int entryCount = static_cast<int>((vecEnd - vecBegin) / 4);
    logger.info("[WeeklyReward] SlotList at 0x{:X}, vecBegin=0x{:X}, {} entries",
        slotList, vecBegin, entryCount);

    // Look up item info and populate slot entries for CExSLTMonthEvent rendering
    u32 itemList = reinterpret_cast<u32(*)()>(
        MG_CONST(addr::features::item_info::ListGet))();

    int fillCount = (entryCount < 7) ? entryCount : 7;
    for (int i = 0; i < fillCount; ++i)
    {
        u32 entry = mg::readValue<u32>(vecBegin, i * 4);
        if (!entry)
            continue;

        u32 itemId = mg::readValue<u32>(_this, 2088 + i * 4);
        u32 itemInfo = 0;
        if (itemId && itemList)
        {
            itemInfo = mg::call<u32(__thiscall*)(u32, u32)>(
                MG_CONST(addr::features::item_info::ListFind),
                itemList, itemId);
        }

        // entry[0] = claim status: 0=unclaimed, 1=claimed
        // Days before currentDay are claimed
        u32 status = (currentDay > 0 && static_cast<u32>(i) < static_cast<u32>(currentDay - 1)) ? 1u : 0u;
        mg::writeValue<u32>(entry, 0, status);

        // entry[8] (byte offset 32) = pointer to item info (first DWORD = item ID)
        mg::writeValue<u32>(entry, 32, itemInfo);

        u32 firstDword = itemInfo ? mg::readValue<u32>(itemInfo, 0) : 0;
        logger.info("[WeeklyReward] entry[{}]: addr=0x{:X}, status={}, entry[8]=0x{:X}, *entry[8]=0x{:X}",
            i, entry, status, itemInfo, firstDword);
    }

    // Show/hide "Already received" text (STC_WEEKLY_TOP, ID 121043)
    {
        u32 topTextId = 121043;
        auto topResult = mg::call<u32(__thiscall*)(u32, u32*)>(
            MG_CONST(addr::features::dlg::GetDlgInfo),
            _this + 1712, &topTextId);
        if (topResult)
        {
            u32 topText = mg::readValue<u32>(topResult, 0);
            if (topText)
            {
                u32 vtbl = mg::readValue<u32>(topText, 0);
                auto showHideFn = reinterpret_cast<void(__thiscall*)(u32, int)>(
                    mg::readValue<u32>(vtbl, 96));
                showHideFn(topText, claimState == 0 ? 1 : 0);
            }
        }
    }

    // First: hide all stamps and reset animation state
    for (u32 i = 0; i < 7; ++i)
    {
        u32 stampId = 121025 + i;
        auto stampResult = mg::call<u32(__thiscall*)(u32, u32*)>(
            MG_CONST(addr::features::dlg::GetDlgInfo),
            _this + 1712, &stampId);
        if (!stampResult) continue;
        u32 stamp = mg::readValue<u32>(stampResult, 0);
        if (!stamp) continue;

        mg::writeValue<u8>(stamp, 2456, 0);
        u32 stampVtbl = mg::readValue<u32>(stamp, 0);
        auto showHideFn = reinterpret_cast<void(__thiscall*)(u32, int)>(
            mg::readValue<u32>(stampVtbl, 96));
        showHideFn(stamp, 0);
    }

    // Show stamps for claimed days (days before current) — static state
    if (currentDay > 1)
    {
        for (u32 k = 0; k < static_cast<u32>(currentDay - 1); ++k)
        {
            u32 stampId = 121025 + k;
            auto stampResult = mg::call<u32(__thiscall*)(u32, u32*)>(
                MG_CONST(addr::features::dlg::GetDlgInfo),
                _this + 1712, &stampId);
            if (!stampResult) continue;
            u32 stamp = mg::readValue<u32>(stampResult, 0);
            if (!stamp) continue;

            u32 stampVtbl = mg::readValue<u32>(stamp, 0);
            auto showHideFn = reinterpret_cast<void(__thiscall*)(u32, int)>(
                mg::readValue<u32>(stampVtbl, 96));
            showHideFn(stamp, 1);
        }
    }

    // Show current day stamp with ScaleAppear animation (stamp_rotate)
    if (currentDay >= 1 && currentDay <= 7)
    {
        u32 curStampId = 121024 + currentDay;
        auto curResult = mg::call<u32(__thiscall*)(u32, u32*)>(
            MG_CONST(addr::features::dlg::GetDlgInfo),
            _this + 1712, &curStampId);
        if (curResult)
        {
            u32 curStamp = mg::readValue<u32>(curResult, 0);
            triggerStampAppear(curStamp);
        }
    }

    // Set item name labels (STC_WEEKLY_ITEM_01-07, IDs 121036-121042)
    for (int i = 0; i < 7; ++i)
    {
        u32 nameId = 121036 + static_cast<u32>(i);
        auto nameResult = mg::call<u32(__thiscall*)(u32, u32*)>(
            MG_CONST(addr::features::dlg::GetDlgInfo),
            _this + 1712, &nameId);
        if (!nameResult)
            continue;
        u32 nameCtrl = mg::readValue<u32>(nameResult, 0);
        if (!nameCtrl)
            continue;

        // Get item info for this slot's item
        u32 itemId = mg::readValue<u32>(_this, 2088 + i * 4);
        if (!itemId || !itemList)
            continue;

        u32 info = mg::call<u32(__thiscall*)(u32, u32)>(
            MG_CONST(addr::features::item_info::ListFind),
            itemList, itemId);
        if (!info)
            continue;

        u32 namePtr = info + 120;
        u32 vtbl = mg::readValue<u32>(nameCtrl, 0);
        auto setTextFn = reinterpret_cast<void(__thiscall*)(u32, u32)>(
            mg::readValue<u32>(vtbl, 216));
        setTextFn(nameCtrl, namePtr);
    }

    logger.info("[WeeklyReward] currentDay={}, entryCount={}", currentDay, entryCount);
}

// ── E_DLG_PLAYTIME_REWARD OnShow (vtable[8]) ─────────────────────────────────
// Light the playtime stamps 128010..128012 for each reached stage (stage at
// dialog+2080[0]: 1=30min, 2=60min, 3=90min), mirroring native sub_6F79C0.
void __fastcall hkPlaytimeOnShow(u32 _this, u32 edx)
{
    u32 stage = mg::readValue<u32>(_this, 2080);
    if (stage > 3) stage = 3;

    // Populate the reward slot list (128014) with the 3 threshold items, which the
    // server pushed to _this+2088/+2092/+2096. Same CExSLTMonthEvent path as weekly.
    {
        u32 slotId = 128014;
        auto slotResult = mg::call<u32(__thiscall*)(u32, u32*)>(
            MG_CONST(addr::features::dlg::GetDlgInfo), _this + 1712, &slotId);
        if (slotResult)
        {
            u32 slotList = mg::readValue<u32>(slotResult, 0);
            if (slotList)
            {
                constexpr u32 kCExSLTMonthEventVtbl = 0x1032FFC;
                if (mg::readValue<u32>(slotList, 0) != kCExSLTMonthEventVtbl)
                    mg::writeValue<u32>(slotList, 0, kCExSLTMonthEventVtbl);

                u32 vecBegin = mg::readValue<u32>(slotList, kSlotVecBeginOff);
                u32 vecEnd   = mg::readValue<u32>(slotList, kSlotVecBeginOff + 4);
                int entryCount = static_cast<int>((vecEnd - vecBegin) / 4);

                u32 itemList = reinterpret_cast<u32(*)()>(
                    MG_CONST(addr::features::item_info::ListGet))();

                int fillCount = (entryCount < 3) ? entryCount : 3;
                for (int i = 0; i < fillCount; ++i)
                {
                    u32 entry = mg::readValue<u32>(vecBegin, i * 4);
                    if (!entry)
                        continue;
                    u32 itemId = mg::readValue<u32>(_this, 2088 + i * 4);
                    u32 itemInfo = (itemId && itemList)
                        ? mg::call<u32(__thiscall*)(u32, u32)>(
                              MG_CONST(addr::features::item_info::ListFind), itemList, itemId)
                        : 0;
                    mg::writeValue<u32>(entry, 0, (static_cast<u32>(i) < stage) ? 1u : 0u);
                    mg::writeValue<u32>(entry, 32, itemInfo);
                }
            }
        }
    }

    // Set the item-name labels (128015/128016/128017) from item info (info+120).
    {
        u32 itemList = reinterpret_cast<u32(*)()>(
            MG_CONST(addr::features::item_info::ListGet))();
        for (int i = 0; i < 3; ++i)
        {
            u32 nameId = 128015 + static_cast<u32>(i);
            auto nameResult = mg::call<u32(__thiscall*)(u32, u32*)>(
                MG_CONST(addr::features::dlg::GetDlgInfo), _this + 1712, &nameId);
            if (!nameResult)
                continue;
            u32 nameCtrl = mg::readValue<u32>(nameResult, 0);
            if (!nameCtrl)
                continue;
            u32 itemId = mg::readValue<u32>(_this, 2088 + i * 4);
            if (!itemId || !itemList)
                continue;
            u32 info = mg::call<u32(__thiscall*)(u32, u32)>(
                MG_CONST(addr::features::item_info::ListFind), itemList, itemId);
            if (!info)
                continue;
            u32 namePtr = info + 120;
            u32 vtbl = mg::readValue<u32>(nameCtrl, 0);
            auto setTextFn = reinterpret_cast<void(__thiscall*)(u32, u32)>(
                mg::readValue<u32>(vtbl, 216));
            setTextFn(nameCtrl, namePtr);
        }
    }

    auto showHideStamp = [&](u32 stampId, int show)
    {
        auto res = mg::call<u32(__thiscall*)(u32, u32*)>(
            MG_CONST(addr::features::dlg::GetDlgInfo), _this + 1712, &stampId);
        if (!res) return;
        u32 stamp = mg::readValue<u32>(res, 0);
        if (!stamp) return;
        u32 vtbl = mg::readValue<u32>(stamp, 0);
        auto fn = reinterpret_cast<void(__thiscall*)(u32, int)>(mg::readValue<u32>(vtbl, 96));
        fn(stamp, show);
    };

    for (u32 i = 0; i < 3; ++i)
        showHideStamp(128010 + i, 0);               // hide all 3 stamps
    for (u32 s = 0; s + 1 < stage; ++s)
        showHideStamp(128010 + s, 1);               // show earlier reached stages (no anim)
    if (stage >= 1 && stage <= 3)                   // animate the most-recent stage stamp
    {
        u32 curStampId = 128010 + (stage - 1);
        auto res = mg::call<u32(__thiscall*)(u32, u32*)>(
            MG_CONST(addr::features::dlg::GetDlgInfo), _this + 1712, &curStampId);
        if (res)
            triggerStampAppear(mg::readValue<u32>(res, 0));
    }
}

// ── E_DLG_PLAYTIME_REWARD OnMessage (vtable[3]) — close on OK button ─────────
u32 __fastcall hkPlaytimeOnMessage(u32 _this, u32 edx, int msgType, int buttonId, int a4)
{
    if (msgType == 257 && buttonId == 128001) // BTN_PLAYTIME_OK — consume (close)
    {
        u32 msgBox = *reinterpret_cast<u32*>(MG_CONST(addr::ui::ui_msgbox::Get));
        if (msgBox)
            mg::call<char(__thiscall*)(u32, const char*)>(
                MG_CONST(addr::ui::ui_msgbox::CloseDialog), msgBox, MG_STR("E_DLG_PLAYTIME_REWARD"));
        return 1;
    }
    // Forward slot-list (128014) clicks and mouse/hover messages to the base
    // handler so the play-time reward items are selectable/inspectable.
    if (g_playtimeOrigOnMsg)
        return g_playtimeOrigOnMsg(_this, edx, msgType, buttonId, a4);
    return 0;
}
#endif // DISABLED: weekly reward + play-time dialog hooks

// ── E_DLG_REWARD_MENU OnMessage (vtable[3]) — Monthly only ───────────────────
void __fastcall hkRewardMenuOnMessage(u32 _this, u32 edx, int msgType, int buttonId, int a4)
{
    if (msgType == 257)
    {
        switch (buttonId)
        {
        case 127001: ShowMonthlyEventDialog(); break; // Monthly
        // Monthly-only build: the other reward types are DISABLED. Their dialogs are
        // no longer registered, so these choices are gone from the XML dropdown too.
        // case 127002: ShowPlaytimeDialog();               break; // Play Time   — DISABLED
        // case 127003: ShowWeeklyRewardDialogServerData(); break; // Weekly/Daily — DISABLED
        // case 127005: ShowBattlePass();                   break; // Battle Pass  — DISABLED
        default: return;
        }
        // Close the dropdown after a choice so it doesn't linger over the lobby.
        u32 msgBox = *reinterpret_cast<u32*>(MG_CONST(addr::ui::ui_msgbox::Get));
        if (msgBox)
            mg::call<char(__thiscall*)(u32, const char*)>(
                MG_CONST(addr::ui::ui_msgbox::CloseDialog), msgBox, MG_STR("E_DLG_REWARD_MENU"));
    }
}

// ── Helpers ──────────────────────────────────────────────────────────────────

// Show the dialog using the data the server already pushed into dialog+2080.
// Mirrors native button 101030 (sub_67DB90): just ShowDialog, no request, no
// synthetic data. Safe to call when no data arrived (dialog simply shows empty).
#if 0  // ── DISABLED: weekly reward dialog (monthly-only build) ──
void ShowWeeklyRewardDialogServerData()
{
    auto& logger = mg::ctx().logger();

    auto GUIManager = reinterpret_cast<CGUIManager*>(
        MG_CONST(addr::ui::gui_manager::Get));
    if (!GUIManager)
        return;

    auto GetDialogInfo = reinterpret_cast<tGetDialogInfo>(
        MG_CONST(addr::ui::gui_manager::GetDialogInfo));
    auto dialog = GetDialogInfo(GUIManager, MG_STR("E_DLG_WEEKLY_REWARD"));
    if (!dialog)
    {
        logger.info("[WeeklyReward] ShowDialog: dialog not registered yet");
        return;
    }

    u32 msgBox = *reinterpret_cast<u32*>(MG_CONST(addr::ui::ui_msgbox::Get));
    if (!msgBox)
        return;

    auto visible = mg::call<bool(__thiscall*)(u32, const char*)>(
        MG_CONST(addr::ui::FindDialog), msgBox, MG_STR("E_DLG_WEEKLY_REWARD"));
    if (visible)
        return;

    u32 vtable = *reinterpret_cast<u32*>(msgBox);
    auto showFn = reinterpret_cast<char(__thiscall*)(u32, const char*, int)>(
        *reinterpret_cast<u32*>(vtable + 16));
    showFn(msgBox, MG_STR("E_DLG_WEEKLY_REWARD"), 0);
}
#endif // DISABLED: weekly reward dialog

// Open the (natively-supported) monthly event dialog. The current client already
// handles E_DLG_MONTHLY_EVENT and is populated by the server's monthly packet on
// login. Matches the native open path (sub_6CEB70): the dialog manager's
// vtable+8 ShowDialog(name) — NOT the vtable+16 variant (that one crashes here).
void ShowMonthlyEventDialog()
{
    u32 dlgMgr = *reinterpret_cast<u32*>(MG_CONST(addr::ui::ui_msgbox::Get));
    if (!dlgMgr)
        return;

    auto visible = mg::call<bool(__thiscall*)(u32, const char*)>(
        MG_CONST(addr::ui::FindDialog), dlgMgr, MG_STR("E_DLG_MONTHLY_EVENT"));
    if (visible)
        return;

    u32 vtable = *reinterpret_cast<u32*>(dlgMgr);
    auto showFn = reinterpret_cast<void(__thiscall*)(u32, const char*)>(
        *reinterpret_cast<u32*>(vtable + 8));
    showFn(dlgMgr, MG_STR("E_DLG_MONTHLY_EVENT"));
}

// Show one of our custom-registered dialogs by name (msgbox vtable+16, like weekly).
inline void ShowCustomDialog(const char* name)
{
    u32 msgBox = *reinterpret_cast<u32*>(MG_CONST(addr::ui::ui_msgbox::Get));
    if (!msgBox)
        return;
    auto visible = mg::call<bool(__thiscall*)(u32, const char*)>(
        MG_CONST(addr::ui::FindDialog), msgBox, name);
    if (visible)
        return;
    u32 vtable = *reinterpret_cast<u32*>(msgBox);
    auto showFn = reinterpret_cast<char(__thiscall*)(u32, const char*, int)>(
        *reinterpret_cast<u32*>(vtable + 16));
    showFn(msgBox, name, 0);
}

void ShowRewardMenu() { ShowCustomDialog(MG_STR("E_DLG_REWARD_MENU")); }

#if 0  // ── DISABLED: play-time reward dialog (monthly-only build) ──
void ShowPlaytimeDialog()
{
    // Apply the latest cached play-time state before showing (an end-match push may
    // have updated the stage while this dialog wasn't loaded).
    if (g_playtimeHave)
    {
        auto GUIManager = reinterpret_cast<CGUIManager*>(MG_CONST(addr::ui::gui_manager::Get));
        if (GUIManager)
        {
            auto GetDialogInfo = reinterpret_cast<tGetDialogInfo>(
                MG_CONST(addr::ui::gui_manager::GetDialogInfo));
            auto dialog = GetDialogInfo(GUIManager, MG_STR("E_DLG_PLAYTIME_REWARD"));
            if (dialog)
                nocrtMemcpy(reinterpret_cast<u8*>(dialog) + 2080, g_playtimeData, 20);
        }
    }
    ShowCustomDialog(MG_STR("E_DLG_PLAYTIME_REWARD"));
}

// ── Response Handler ─────────────────────────────────────────────────────────

char __cdecl handleWeeklyRewardResponse(u16* a1)
{
    auto GUIManager = reinterpret_cast<CGUIManager*>(
        MG_CONST(addr::ui::gui_manager::Get));

    if (GUIManager)
    {
        auto GetDialogInfo = reinterpret_cast<tGetDialogInfo>(
            MG_CONST(addr::ui::gui_manager::GetDialogInfo));

        auto dialog = GetDialogInfo(GUIManager, MG_STR("E_DLG_WEEKLY_REWARD"));
        if (dialog)
        {
            // Payload at a1+4 is MainWeeklyRewardAck (0x24): week,received,unknown,items[7].
            auto src = reinterpret_cast<u8*>(a1) + 4;
            auto dst = reinterpret_cast<u8*>(dialog) + 2080;
            nocrtMemcpy(dst, src, 0x24);
        }
    }

    // byte[3] (a1+3) = server "granted" flag: 1 = a new daily reward was just granted
    // this login, 0 = already claimed today / nothing new. Only auto-popup on a new
    // grant, so it doesn't pop every login once you've already received today's.
    u8 grantedFlag = reinterpret_cast<u8*>(a1)[3];
    g_weeklyRewardPending = (grantedFlag != 0);

    return 1;
}

// ── Play Time response (order 181) ───────────────────────────────────────────
// MainPlaytimeAck payload at a1+4: stage, daily_seconds, reserved (3 dwords).
// Sent on login AND after each match end; cache it so a push that arrives while
// the lobby dialog isn't loaded still shows up when the dialog next opens.
char __cdecl handlePlaytimeResponse(u16* a1)
{
    auto src = reinterpret_cast<u32*>(reinterpret_cast<u8*>(a1) + 4);
    for (int i = 0; i < 5; ++i)
        g_playtimeData[i] = src[i];
    g_playtimeHave = true;

    auto GUIManager = reinterpret_cast<CGUIManager*>(MG_CONST(addr::ui::gui_manager::Get));
    if (GUIManager)
    {
        auto GetDialogInfo = reinterpret_cast<tGetDialogInfo>(
            MG_CONST(addr::ui::gui_manager::GetDialogInfo));
        auto dialog = GetDialogInfo(GUIManager, MG_STR("E_DLG_PLAYTIME_REWARD"));
        if (dialog)
            nocrtMemcpy(reinterpret_cast<u8*>(dialog) + 2080, src, 20);
    }
    return 1;
}

// ── Battle Pass (MICROPASS) ──────────────────────────────────────────────────
// Server-driven snapshot (order 182): 100 levels of free+premium items, claim
// masks, xp curve, season + mission. The client paginates 10 levels/page locally.

struct BattlePassData
{
    u32 season{}, days_left{}, level{}, xp{}, xp_required{}, has_premium{}, reset_cost{}, reset_count{};
    u8  claimed_free[16]{};
    u8  claimed_premium[16]{};
    u32 free_items[100]{};
    u32 premium_items[100]{};
    char mission[256]{};
    u32 page{};
    bool have{};
};
BattlePassData g_battlePass{};
uptr g_battlePassVtbl[kVtableSize] = {};
tDlgOnMsg g_battlePassOrigOnMsg = nullptr;

void __fastcall hkBattlePassOnShow(u32 _this, u32 edx);

namespace bp {
    inline bool bit(const u8* m, u32 i) { return i < 100 && ((m[i >> 3] >> (i & 7)) & 1); }
    // u32 -> decimal, returns end pointer (no CRT).
    inline char* u32ToStr(u32 v, char* o) { char t[12]; int n = 0; if (!v) { *o++ = '0'; return o; } while (v) { t[n++] = char('0' + v % 10); v /= 10; } while (n) *o++ = t[--n]; return o; }
    inline char* append(const char* s, char* o) { while (*s) *o++ = *s++; return o; }

    inline void setText(u32 _this, u32 ctrlId, const char* text)
    {
        auto res = mg::call<u32(__thiscall*)(u32, u32*)>(
            MG_CONST(addr::features::dlg::GetDlgInfo), _this + 1712, &ctrlId);
        if (!res) return;
        u32 ctrl = mg::readValue<u32>(res, 0);
        if (!ctrl) return;
        u32 vtbl = mg::readValue<u32>(ctrl, 0);
        auto fn = reinterpret_cast<void(__thiscall*)(u32, const char*)>(mg::readValue<u32>(vtbl, 216));
        fn(ctrl, text);
    }

    // Populate one 10-wide CExSLTMonthEvent slot list from items[base..base+10).
    inline void fillSlots(u32 _this, u32 slotId, const u32* items10, const u8* claimedMask, u32 base)
    {
        u32 id = slotId;
        auto res = mg::call<u32(__thiscall*)(u32, u32*)>(
            MG_CONST(addr::features::dlg::GetDlgInfo), _this + 1712, &id);
        if (!res) return;
        u32 slotList = mg::readValue<u32>(res, 0);
        if (!slotList) return;
        constexpr u32 kVtbl = 0x1032FFC;
        if (mg::readValue<u32>(slotList, 0) != kVtbl) mg::writeValue<u32>(slotList, 0, kVtbl);
        u32 vecBegin = mg::readValue<u32>(slotList, kSlotVecBeginOff);
        u32 vecEnd   = mg::readValue<u32>(slotList, kSlotVecBeginOff + 4);
        int cnt = static_cast<int>((vecEnd - vecBegin) / 4);
        u32 itemList = reinterpret_cast<u32(*)()>(MG_CONST(addr::features::item_info::ListGet))();
        int fill = cnt < 10 ? cnt : 10;
        for (int i = 0; i < fill; ++i)
        {
            u32 entry = mg::readValue<u32>(vecBegin, i * 4);
            if (!entry) continue;
            u32 itemId = items10[i];
            u32 info = (itemId && itemList)
                ? mg::call<u32(__thiscall*)(u32, u32)>(MG_CONST(addr::features::item_info::ListFind), itemList, itemId)
                : 0;
            mg::writeValue<u32>(entry, 0, bit(claimedMask, base + static_cast<u32>(i)) ? 1u : 0u);
            mg::writeValue<u32>(entry, 32, info);
        }
    }
}

// Parse order-182 snapshot into g_battlePass (payload at a1+4, mirrors playtime).
char __cdecl handleBattlePassResponse(u16* a1)
{
    auto p = reinterpret_cast<u8*>(a1) + 4;
    auto rd32 = [&](u32 off) { return *reinterpret_cast<u32*>(p + off); };
    g_battlePass.season       = rd32(0);
    g_battlePass.days_left    = rd32(4);
    g_battlePass.level        = rd32(8);
    g_battlePass.xp           = rd32(12);
    g_battlePass.xp_required  = rd32(16);
    g_battlePass.has_premium  = rd32(20);
    g_battlePass.reset_cost   = rd32(24);
    g_battlePass.reset_count  = rd32(28);
    nocrtMemcpy(g_battlePass.claimed_free, p + 32, 16);
    nocrtMemcpy(g_battlePass.claimed_premium, p + 48, 16);
    nocrtMemcpy(g_battlePass.free_items, p + 64, 400);
    nocrtMemcpy(g_battlePass.premium_items, p + 464, 400);
    u16 mlen = *reinterpret_cast<u16*>(p + 864);
    if (mlen > 255) mlen = 255;
    nocrtMemcpy(g_battlePass.mission, p + 866, mlen);
    g_battlePass.mission[mlen] = 0;
    g_battlePass.have = true;

    // Keep the current page valid (level's page by default on first load).
    if (!g_battlePass.page && g_battlePass.level >= 1)
        g_battlePass.page = (g_battlePass.level - 1) / 10;
    return 1;
}

void __fastcall hkBattlePassOnShow(u32 _this, u32 edx)
{
    if (!g_battlePass.have) return;
    u32 page = g_battlePass.page > 9 ? 0 : g_battlePass.page;
    u32 base = page * 10;
    bp::fillSlots(_this, 129010, &g_battlePass.premium_items[base], g_battlePass.claimed_premium, base);
    bp::fillSlots(_this, 129011, &g_battlePass.free_items[base], g_battlePass.claimed_free, base);

    char buf[64];
    for (u32 i = 0; i < 10; ++i) { char* e = bp::u32ToStr(base + i + 1, buf); *e = 0; bp::setText(_this, 129020 + i, buf); }
    { char* e = bp::append("SEASON ENDS IN ", buf); e = bp::u32ToStr(g_battlePass.days_left, e); e = bp::append(" Days", e); *e = 0; bp::setText(_this, 129001, buf); }
    { char* e = bp::u32ToStr(g_battlePass.xp, buf); *e++ = '/'; e = bp::u32ToStr(g_battlePass.xp_required, e); *e = 0; bp::setText(_this, 129035, buf); }
    { char* e = bp::u32ToStr(g_battlePass.level, buf); *e = 0; bp::setText(_this, 129036, buf); }
    bp::setText(_this, 129030, g_battlePass.mission);
}

// TODO(outgoing-packet): the client->server send path (CTcpPacket + SendPacket via
// CNetMgr) still needs to be RE'd/wired. Until then claim/reset only log; the rest
// of the dialog (open, paginate, view server data) is fully functional.
void sendBattlePassRequest(u32 order, u32 mode, u32 level)
{
    mg::ctx().logger().info("[BattlePass] request order={} mode={} level={} (send not yet wired)", order, mode, level);
}

u32 __fastcall hkBattlePassOnMessage(u32 _this, u32 edx, int msgType, int ctrlId, int a4)
{
    if (msgType == 257)
    {
        switch (ctrlId)
        {
        case 129003: // close
        {
            u32 msgBox = *reinterpret_cast<u32*>(MG_CONST(addr::ui::ui_msgbox::Get));
            if (msgBox)
                mg::call<char(__thiscall*)(u32, const char*)>(
                    MG_CONST(addr::ui::ui_msgbox::CloseDialog), msgBox, MG_STR("E_DLG_BATTLEPASS"));
            return 1;
        }
        case 129004: // left arrow (client-side pagination)
            if (g_battlePass.page > 0) { --g_battlePass.page; hkBattlePassOnShow(_this, 0); }
            return 1;
        case 129005: // right arrow
            if (g_battlePass.page < 9) { ++g_battlePass.page; hkBattlePassOnShow(_this, 0); }
            return 1;
        case 129032: // CLAIM (current unlocked level)
            sendBattlePassRequest(183, 0, g_battlePass.level);
            return 1;
        case 129033: // CLAIM ALL
            sendBattlePassRequest(183, 1, 0);
            return 1;
        case 129031: // reset mission
            sendBattlePassRequest(184, 0, 0);
            return 1;
        case 129002: // info — no-op for now
            return 1;
        default:
            break;
        }
    }
    // Forward slot-list (129010/129011) selection + mouse to base for item inspect.
    if (g_battlePassOrigOnMsg)
        return g_battlePassOrigOnMsg(_this, edx, msgType, ctrlId, a4);
    return 0;
}

void ShowBattlePass() { ShowCustomDialog(MG_STR("E_DLG_BATTLEPASS")); }
#endif // DISABLED: play-time + weekly response handlers + Battle Pass

// ── Button Click Hook ────────────────────────────────────────────────────────

// Agora (lobby) menu button hook. Button 101085 is the custom weekly-reward open
// button we added to E_DLG_AGORA_MENU (the native 101030 is a battery Picture in
// this client, not a button). It only shows the dialog — no packet.
int __fastcall hkAgoraMenuOnMessage(u32 _this, u32 edx, int msgType, int buttonId, int a4)
{
    // Single lobby Reward button opens the 3-choice menu (Daily/Monthly/Play Time).
    if (msgType == 257 && (buttonId == 101085 || buttonId == 101086))
    {
        // mg::ctx().logger().info("[RewardMenu] Lobby reward button {} clicked", buttonId);
        // Toggle: if the dropdown is already open, close it instead of re-opening.
        u32 msgBox = *reinterpret_cast<u32*>(MG_CONST(addr::ui::ui_msgbox::Get));
        bool open = msgBox && mg::call<bool(__thiscall*)(u32, const char*)>(
            MG_CONST(addr::ui::FindDialog), msgBox, MG_STR("E_DLG_REWARD_MENU"));
        if (open)
            mg::call<char(__thiscall*)(u32, const char*)>(
                MG_CONST(addr::ui::ui_msgbox::CloseDialog), msgBox, MG_STR("E_DLG_REWARD_MENU"));
        else
            ShowRewardMenu();
        return 1;
    }

    auto& registry = mg::ctx().hookRegistry();
    auto original = registry.findDetour(HookId::WeeklyRewardButton)
        ->getOriginal<decltype(&hkAgoraMenuOnMessage)>();
    return original(_this, edx, msgType, buttonId, a4);
}

// ── Dialog Registration ──────────────────────────────────────────────────────

int __cdecl weeklyRewardDialogFactory()
{
    auto mem = reinterpret_cast<u8*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x850));
    if (!mem)
        return 0;
    auto dialogCtor = reinterpret_cast<void*(__thiscall*)(void*)>(
        MG_CONST(addr::ui::gui_manager::DialogBaseCtor));
    dialogCtor(mem);
    return reinterpret_cast<int>(mem);
}

#if 0  // ── DISABLED: weekly reward dialog registration (monthly-only build) ──
bool g_dialogRegistered = false;

void RegisterWeeklyRewardDialog()
{
    if (g_dialogRegistered)
        return;
    g_dialogRegistered = true;

    auto& logger = mg::ctx().logger();

    auto factory1 = mg::call<u32(*)()>(MG_CONST(addr::features::dlg::CFactoryGet));
    mg::call<void(__thiscall*)(u32, u32, void*)>(
        MG_CONST(addr::features::dlg::GetDlgById),
        factory1, 0x1D8A8,
        reinterpret_cast<void*>(&weeklyRewardDialogFactory));

    auto factory2 = mg::call<u32(*)()>(MG_CONST(addr::features::dlg::CFactoryGet));
    auto dialogObj = mg::call<u32(__thiscall*)(u32, u32)>(
        MG_CONST(addr::features::dlg::GetDlgId),
        factory2, 0x1D8A8);

    if (!dialogObj)
    {
        logger.info("[WeeklyReward] Factory failed to create dialog object");
        return;
    }

    uptr baseVtbl = *reinterpret_cast<uptr*>(static_cast<uptr>(dialogObj));
    for (int i = 0; i < kVtableSize; ++i)
        g_weeklyRewardVtbl[i] = mg::readValue<uptr>(baseVtbl, i * sizeof(uptr));
    g_weeklyRewardOrigOnMsg = reinterpret_cast<tDlgOnMsg>(g_weeklyRewardVtbl[3]);
    g_weeklyRewardVtbl[3] = reinterpret_cast<uptr>(&hkWeeklyRewardOnMessage);
    g_weeklyRewardVtbl[8] = reinterpret_cast<uptr>(&hkWeeklyRewardOnShow);
    *reinterpret_cast<uptr*>(static_cast<uptr>(dialogObj)) = reinterpret_cast<uptr>(g_weeklyRewardVtbl);

    auto manager = reinterpret_cast<void*>(
        MG_CONST(addr::ui::gui_manager::Get));
    auto registerDlg = reinterpret_cast<char(__thiscall*)(void*, const char*, u32)>(
        MG_CONST(addr::ui::gui_manager::RegisterDialog));
    registerDlg(manager, MG_STR("E_DLG_WEEKLY_REWARD"), dialogObj);

    logger.info("[WeeklyReward] Registered dialog object at 0x{:X} with custom vtable", static_cast<uptr>(dialogObj));
}
#endif // DISABLED: weekly reward dialog registration

// ── Reward Menu dialog registration ──────────────────────────────────────────
// Factory id == XML dialog id (weekly 0x1D8A8 = 121000): reward menu 0x1F018 =
// 127000, playtime 0x1F400 = 128000. Same pattern as RegisterWeeklyRewardDialog.

bool g_rewardMenuRegistered = false;
void RegisterRewardMenuDialog()
{
    if (g_rewardMenuRegistered)
        return;
    g_rewardMenuRegistered = true;
    auto& logger = mg::ctx().logger();

    auto factory1 = mg::call<u32(*)()>(MG_CONST(addr::features::dlg::CFactoryGet));
    mg::call<void(__thiscall*)(u32, u32, void*)>(
        MG_CONST(addr::features::dlg::GetDlgById),
        factory1, 0x1F018, reinterpret_cast<void*>(&weeklyRewardDialogFactory));

    auto factory2 = mg::call<u32(*)()>(MG_CONST(addr::features::dlg::CFactoryGet));
    auto dialogObj = mg::call<u32(__thiscall*)(u32, u32)>(
        MG_CONST(addr::features::dlg::GetDlgId), factory2, 0x1F018);
    if (!dialogObj) { logger.info("[RewardMenu] factory failed to create dialog"); return; }

    uptr baseVtbl = *reinterpret_cast<uptr*>(static_cast<uptr>(dialogObj));
    for (int i = 0; i < kVtableSize; ++i)
        g_rewardMenuVtbl[i] = mg::readValue<uptr>(baseVtbl, i * sizeof(uptr));
    g_rewardMenuVtbl[3] = reinterpret_cast<uptr>(&hkRewardMenuOnMessage);
    *reinterpret_cast<uptr*>(static_cast<uptr>(dialogObj)) = reinterpret_cast<uptr>(g_rewardMenuVtbl);

    auto manager = reinterpret_cast<void*>(MG_CONST(addr::ui::gui_manager::Get));
    auto registerDlg = reinterpret_cast<char(__thiscall*)(void*, const char*, u32)>(
        MG_CONST(addr::ui::gui_manager::RegisterDialog));
    registerDlg(manager, MG_STR("E_DLG_REWARD_MENU"), dialogObj);
    // logger.info("[RewardMenu] registered dialog at 0x{:X}", static_cast<uptr>(dialogObj));
}

#if 0  // ── DISABLED: play-time + battle-pass dialog registration (monthly-only build) ──
bool g_playtimeRegistered = false;
void RegisterPlaytimeDialog()
{
    if (g_playtimeRegistered)
        return;
    g_playtimeRegistered = true;
    auto& logger = mg::ctx().logger();

    auto factory1 = mg::call<u32(*)()>(MG_CONST(addr::features::dlg::CFactoryGet));
    mg::call<void(__thiscall*)(u32, u32, void*)>(
        MG_CONST(addr::features::dlg::GetDlgById),
        factory1, 0x1F400, reinterpret_cast<void*>(&weeklyRewardDialogFactory));

    auto factory2 = mg::call<u32(*)()>(MG_CONST(addr::features::dlg::CFactoryGet));
    auto dialogObj = mg::call<u32(__thiscall*)(u32, u32)>(
        MG_CONST(addr::features::dlg::GetDlgId), factory2, 0x1F400);
    if (!dialogObj) { logger.info("[Playtime] factory failed to create dialog"); return; }

    uptr baseVtbl = *reinterpret_cast<uptr*>(static_cast<uptr>(dialogObj));
    for (int i = 0; i < kVtableSize; ++i)
        g_playtimeVtbl[i] = mg::readValue<uptr>(baseVtbl, i * sizeof(uptr));
    g_playtimeOrigOnMsg = reinterpret_cast<tDlgOnMsg>(g_playtimeVtbl[3]);
    g_playtimeVtbl[3] = reinterpret_cast<uptr>(&hkPlaytimeOnMessage);
    g_playtimeVtbl[8] = reinterpret_cast<uptr>(&hkPlaytimeOnShow);
    *reinterpret_cast<uptr*>(static_cast<uptr>(dialogObj)) = reinterpret_cast<uptr>(g_playtimeVtbl);

    auto manager = reinterpret_cast<void*>(MG_CONST(addr::ui::gui_manager::Get));
    auto registerDlg = reinterpret_cast<char(__thiscall*)(void*, const char*, u32)>(
        MG_CONST(addr::ui::gui_manager::RegisterDialog));
    registerDlg(manager, MG_STR("E_DLG_PLAYTIME_REWARD"), dialogObj);
    logger.info("[Playtime] registered dialog at 0x{:X}", static_cast<uptr>(dialogObj));
}

bool g_battlePassRegistered = false;
void RegisterBattlePassDialog()
{
    if (g_battlePassRegistered)
        return;
    g_battlePassRegistered = true;
    auto& logger = mg::ctx().logger();

    auto factory1 = mg::call<u32(*)()>(MG_CONST(addr::features::dlg::CFactoryGet));
    mg::call<void(__thiscall*)(u32, u32, void*)>(
        MG_CONST(addr::features::dlg::GetDlgById),
        factory1, 0x1F848 /* 129000 */, reinterpret_cast<void*>(&weeklyRewardDialogFactory));

    auto factory2 = mg::call<u32(*)()>(MG_CONST(addr::features::dlg::CFactoryGet));
    auto dialogObj = mg::call<u32(__thiscall*)(u32, u32)>(
        MG_CONST(addr::features::dlg::GetDlgId), factory2, 0x1F848);
    if (!dialogObj) { logger.info("[BattlePass] factory failed to create dialog"); return; }

    uptr baseVtbl = *reinterpret_cast<uptr*>(static_cast<uptr>(dialogObj));
    for (int i = 0; i < kVtableSize; ++i)
        g_battlePassVtbl[i] = mg::readValue<uptr>(baseVtbl, i * sizeof(uptr));
    g_battlePassOrigOnMsg = reinterpret_cast<tDlgOnMsg>(g_battlePassVtbl[3]);
    g_battlePassVtbl[3] = reinterpret_cast<uptr>(&hkBattlePassOnMessage);
    g_battlePassVtbl[8] = reinterpret_cast<uptr>(&hkBattlePassOnShow);
    *reinterpret_cast<uptr*>(static_cast<uptr>(dialogObj)) = reinterpret_cast<uptr>(g_battlePassVtbl);

    auto manager = reinterpret_cast<void*>(MG_CONST(addr::ui::gui_manager::Get));
    auto registerDlg = reinterpret_cast<char(__thiscall*)(void*, const char*, u32)>(
        MG_CONST(addr::ui::gui_manager::RegisterDialog));
    registerDlg(manager, MG_STR("E_DLG_BATTLEPASS"), dialogObj);
    logger.info("[BattlePass] registered dialog at 0x{:X}", static_cast<uptr>(dialogObj));
}
#endif // DISABLED: play-time + battle-pass dialog registration

// ── SlotList Factory Registration ────────────────────────────────────────────
// Register SlotList 121033 with CExSLTMonthEvent factory (MV sub_5EF270) so
// the game creates the CExSLTMonthEvent subclass when parsing the XML.
// Mirrors MV sub_5A6380's pattern: DFA680(factory, slotId, factoryFn).

#if 0  // ── DISABLED: slot-list factory (only used by weekly/playtime/battlepass) ──
bool g_slotFactoryRegistered = false;

void RegisterSlotListFactory()
{
    if (g_slotFactoryRegistered)
        return;
    g_slotFactoryRegistered = true;

    auto& logger = mg::ctx().logger();

    auto factory = mg::call<u32(*)()>(MG_CONST(addr::features::dlg::CFactoryGet));
    mg::call<void(__thiscall*)(u32, u32, void*)>(
        MG_CONST(addr::features::dlg::GetDlgById),
        factory, 121033u,
        reinterpret_cast<void*>(MG_CONST(addr::features::dlg::CExSLTMonthEventFactory)));

    // Play-time reward slot (128014) — same CExSLTMonthEvent slot type as weekly.
    auto factoryPt = mg::call<u32(*)()>(MG_CONST(addr::features::dlg::CFactoryGet));
    mg::call<void(__thiscall*)(u32, u32, void*)>(
        MG_CONST(addr::features::dlg::GetDlgById),
        factoryPt, 128014u,
        reinterpret_cast<void*>(MG_CONST(addr::features::dlg::CExSLTMonthEventFactory)));

    // Battle Pass premium (129010) + free (129011) slot lists — same slot type.
    for (u32 bpSlot : { 129010u, 129011u })
    {
        auto factoryBp = mg::call<u32(*)()>(MG_CONST(addr::features::dlg::CFactoryGet));
        mg::call<void(__thiscall*)(u32, u32, void*)>(
            MG_CONST(addr::features::dlg::GetDlgById),
            factoryBp, bpSlot,
            reinterpret_cast<void*>(MG_CONST(addr::features::dlg::CExSLTMonthEventFactory)));
    }

    logger.info("[WeeklyReward] Registered SlotLists 121033 + 128014 + 129010/129011 with CExSLTMonthEvent factory");
}
#endif // DISABLED: slot-list factory

} // anonymous namespace

// ── Public API ───────────────────────────────────────────────────────────────

WeeklyRewardHandler::WeeklyRewardHandler(::mg::MegaGuardContext& ctx, CustomPacketDispatcher& dispatcher)
    : ctx_(ctx)
    , dispatcher_(dispatcher)
{
}

WeeklyRewardHandler::~WeeklyRewardHandler() = default;

void WeeklyRewardHandler::showOnLobbyEntry()
{
    // ctx_.logger().info("[RewardMenu] registering monthly reward menu dialog");
    // Monthly-only build: only the reward-menu dropdown is registered. The weekly,
    // play-time and battle-pass dialog registrations are DISABLED (see #if 0 blocks).
    RegisterRewardMenuDialog();
    // RegisterWeeklyRewardDialog();   // DISABLED
    // RegisterPlaytimeDialog();       // DISABLED
    // RegisterBattlePassDialog();     // DISABLED

    // Close any reward dialogs left open from the previous scene so they don't persist
    // / stack when (re-)entering the lobby or plaza (user report: popups stuck in plaza,
    // can't close). Runs on scene entry only; closing a not-open dialog is a safe no-op.
    if (u32 mb = *reinterpret_cast<u32*>(MG_CONST(addr::ui::ui_msgbox::Get)))
    {
        auto close = [&](const char* n) {
            mg::call<char(__thiscall*)(u32, const char*)>(
                MG_CONST(addr::ui::ui_msgbox::CloseDialog), mb, n);
        };
        close(MG_STR("E_DLG_REWARD_MENU"));
        close(MG_STR("E_DLG_MONTHLY_EVENT"));
    }
}

void WeeklyRewardHandler::showIfPendingOnLobby()
{
    // Monthly-only build: the weekly auto-popup is DISABLED. The native monthly event
    // dialog (E_DLG_MONTHLY_EVENT) is auto-shown by the client from the server's monthly
    // packet, so nothing needs to be SHOWN here.
    //
    // This runs post-construct on the first lobby/plaza entry — AFTER the scene XML has
    // actually created the reward-menu dropdown. showOnLobbyEntry()'s close() runs at agora
    // *init*, before that dialog exists, so on the very first plaza entry the dropdown ends
    // up open by default. Now that it exists, close it so it stays hidden until the lobby
    // reward button is clicked. (Only E_DLG_REWARD_MENU — leave the auto-shown monthly dialog.)
    // The plaza scene (SCENE_COMMON_AGORA) defines E_DLG_REWARD_MENU active="1" — the lone
    // active popup (all the other on-demand popups are active="0") — so the scene SHOWS it on
    // plaza entry. We can't flip it to active="0" (the menu's show path needs it active, which
    // breaks the Reward button). Instead, fetch the dialog object from the gui-manager registry
    // and close it directly via its base close vtable (offset 36 — the same close the game uses
    // for E_DLG_OPTION). This hides it no matter which active-list the scene put it in (the
    // msgbox CloseByName missed it because the scene shows it elsewhere than the button does).
    // The object stays registered, so the Reward button still re-opens it.
    const u32 dlg = mg::call<u32(__thiscall*)(void*, const char*)>(
        MG_CONST(addr::ui::gui_manager::GetDialogInfo),
        reinterpret_cast<void*>(MG_CONST(addr::ui::gui_manager::Get)),
        MG_STR("E_DLG_REWARD_MENU"));
    if (dlg)
    {
        const uptr vt = *reinterpret_cast<uptr*>(static_cast<uptr>(dlg));
        reinterpret_cast<void(__thiscall*)(u32, u32)>(*reinterpret_cast<uptr*>(vt + 36))(dlg, dlg);
    }
}

VoidResult WeeklyRewardHandler::install()
{
    auto& registry = ctx_.hookRegistry();

    // Monthly-only build: weekly (179), play-time (181) and battle-pass (182) packet
    // handlers are DISABLED — server data for those features is ignored.
    // dispatcher_.registerHandler(179, &handleWeeklyRewardResponse);   // DISABLED
    // dispatcher_.registerHandler(181, &handlePlaytimeResponse);       // DISABLED
    // dispatcher_.registerHandler(182, &handleBattlePassResponse);     // DISABLED
    (void)dispatcher_;

    // Lobby Reward button still opens the reward-menu dropdown (Monthly only).
    registry.registerDetour(HookId::WeeklyRewardButton)
        .create(MG_CONST(addr::features::AgoraMenuOnMessage), &hkAgoraMenuOnMessage);

    // RegisterSlotListFactory();   // DISABLED — slots only used by weekly/playtime/battlepass

    return VoidResult::ok();
}

} // namespace mg::game
