// =============================================================================
// PcBang - Full implementation ported from pcbang.h
// =============================================================================
// Common_Agora_Init_DLG: calls original, then CFactoryGet → GetDlgById(101075)
//   → CExPICBaseAlloc → AssignDlgInfo("E_DLG_AGORA_MENU_PIC_PCROOM_ICON")
// Common_Agora_Construct_DLG: calls original, then GetDlgInfo for IDs
//   101075/101076/101077 → writes to instance offsets 2248/2252/2256
//   → InitPcBangInfo on first lobby
// =============================================================================
#include "pch.hpp"
#include "modding/features/pcbang.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"
#include "game/network/custom_packets/weekly_reward_handler.hpp"
#include "game/network/custom_packets/trade_handler.hpp"

namespace mg::modding {

using namespace mg::game;

namespace dlg = addr::features::dlg;

namespace {

// ── Per-module state ───────────────────────────────────────────────────────
// first_time_lobby must persist across frames on game thread
bool first_time_lobby = true;

// ── Common_Agora_Init_DLG hook ────────────────────────────────────────────

void __fastcall hkAgoraInitDlg(u32 instance, u32 edx)
{
    // Register weekly reward dialog BEFORE original — sub_598BD0 has already
    // registered all other dialogs, and scene XML hasn't loaded yet.
    // This lets the scene loader link XML elements to our dialog object.
    mg::ctx().weeklyRewardHandler().showOnLobbyEntry();
    // TRADE DISABLED (user request) — trade handler is not constructed in context.cpp,
    // so do NOT register E_DLG_TRADE here (would call a null handler).
    // mg::ctx().tradeHandler().registerDialogOnLobby();

    // Call original
    auto& registry = mg::ctx().hookRegistry();
    auto original = registry.findDetour(HookId::AgoraInitDlg)
        ->getOriginal<decltype(&hkAgoraInitDlg)>();
    original(instance, edx);

    // CFactoryGet → GetDlgById(101075) → CExPICBaseAlloc
    auto pcroom_1 = mg::call<u32(*)()>(MG_CONST(dlg::CFactoryGet));
    mg::call<void(__thiscall*)(u32, u32, void*)>(
        MG_CONST(dlg::GetDlgById),
        pcroom_1, 101075,
        reinterpret_cast<void*>(MG_CONST(dlg::CExPICBaseAlloc)));

    auto pcroom_2 = mg::call<u32(*)()>(MG_CONST(dlg::CFactoryGet));
    auto pcroom_3 = mg::call<u32(__thiscall*)(u32, u32)>(
        MG_CONST(dlg::GetDlgId),
        pcroom_2, 101075);

    mg::call<void(__thiscall*)(u32, const char*, u32)>(
        MG_CONST(dlg::AssignDlgInfo),
        instance,
        MG_STR("E_DLG_AGORA_MENU_PIC_PCROOM_ICON"),
        pcroom_3);

    /*
    // CFactoryGet → GetDlgById(101081) → CExPICBaseAlloc (weekly reward icon picture)
    auto wr_1 = mg::call<u32(*)()>(MG_CONST(dlg::CFactoryGet));
    mg::call<void(__thiscall*)(u32, u32, void*)>(
        MG_CONST(dlg::GetDlgById),
        wr_1, 101081,
        reinterpret_cast<void*>(MG_CONST(dlg::CExPICBaseAlloc)));

    auto wr_2 = mg::call<u32(*)()>(MG_CONST(dlg::CFactoryGet));
    auto wr_3 = mg::call<u32(__thiscall*)(u32, u32)>(
        MG_CONST(dlg::GetDlgId),
        wr_2, 101081);

    mg::call<void(__thiscall*)(u32, const char*, u32)>(
        MG_CONST(dlg::AssignDlgInfo),
        instance,
        MG_STR("E_DLG_AGORA_MENU_PIC_WEEKLY_REWARD"),
        wr_3);
    */
}

// ── Common_Agora_Construct_DLG hook ─────────────────────────────────────────

void __fastcall hkAgoraConstructDlg(u32 instance, u32 edx)
{
    // Call original
    auto& registry = mg::ctx().hookRegistry();
    auto original = registry.findDetour(HookId::AgoraConstructDlg)
        ->getOriginal<decltype(&hkAgoraConstructDlg)>();
    original(instance, edx);

    // GetDlgInfo for dialog ID 101075 → write to instance+2248
    u32 pcroom_id_1 = 101075;
    auto pcroom_result_1 = mg::call<u32(__thiscall*)(u32, u32*)>(
        MG_CONST(dlg::GetDlgInfo),
        (instance + 1712), &pcroom_id_1);
    mg::writeValue<uptr>(instance, 2248,
        mg::readValue<uptr>(pcroom_result_1, 0));

    // GetDlgInfo for dialog ID 101076 → write to instance+2252
    u32 pcroom_id_2 = 101076;
    auto pcroom_result_2 = mg::call<u32(__thiscall*)(u32, u32*)>(
        MG_CONST(dlg::GetDlgInfo),
        (instance + 1712), &pcroom_id_2);
    mg::writeValue<uptr>(instance, 2252,
        mg::readValue<uptr>(pcroom_result_2, 0));

    // GetDlgInfo for dialog ID 101077 → write to instance+2256
    u32 pcroom_id_3 = 101077;
    auto pcroom_result_3 = mg::call<u32(__thiscall*)(u32, u32*)>(
        MG_CONST(dlg::GetDlgInfo),
        (instance + 1712), &pcroom_id_3);
    mg::writeValue<uptr>(instance, 2256,
        mg::readValue<uptr>(pcroom_result_3, 0));

    // Init PC Bang info on first lobby entry
    if (first_time_lobby)
    {
        mg::call<void(__thiscall*)(u32)>(
            MG_CONST(addr::features::InitPcBangInfo), instance);
        first_time_lobby = false;
    }

    // Close the stray reward-menu dropdown on EVERY plaza/lobby construct (not just
    // the first). Its XML is created open-by-default; gating this behind first_time
    // meant a plaza entry after the first lobby left the "Monthly" dropdown visible.
    // Closing a not-open dialog is a safe no-op; the lobby reward button still opens it.
    mg::ctx().weeklyRewardHandler().showIfPendingOnLobby();

}

} // anonymous namespace

// ── PcBang class ─────────────────────────────────────────────────────────────

PcBang::PcBang(MegaGuardContext& ctx) : ctx_(ctx) {}
PcBang::~PcBang() = default;

VoidResult PcBang::install()
{
    auto& registry = ctx_.hookRegistry();
    registry.registerDetour(HookId::AgoraInitDlg)
        .create(MG_CONST(addr::features::CommonAgoraDlgInit), hkAgoraInitDlg);
    registry.registerDetour(HookId::AgoraConstructDlg)
        .create(MG_CONST(addr::features::CommonAgoraDlgConstruct), hkAgoraConstructDlg);
    return VoidResult::ok();
}

} // namespace mg::modding
