// =============================================================================
// GachaPityHandler - Implementation
// =============================================================================
// 3 UI hooks (back/next/select) → read gachapon_id at this+0x818 (2072)
// → check GAME_STATE_GACHAPON → send order=95 packet to CMainConnectorTcp
// Response handler: update m_uiLuckyPoints on all players, refresh UI
// =============================================================================
#include "pch.hpp"
#include "game/network/custom_packets/gacha_pity_handler.hpp"
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

// ── Packet structures ────────────────────────────────────────────────────────

#pragma pack(push, 1)
struct GachaPityReq {
    SCommandHeader m_tHeader;
    u32            gacha_id;
};
#pragma pack(pop)

// ── Type aliases ─────────────────────────────────────────────────────────────

using tGetNetMgr        = CNetMgr* (__cdecl*)();
using tGetUnitContainer = CUnitContainer* (__cdecl*)();
using tGetDialogInfo    = int* (__thiscall*)(CGUIManager*, const char*);
using tUpdateGachaLucky = void(__thiscall*)(int*);

// ── Lucky Points MidHook Callback ────────────────────────────────────────────
// Hooks: mov eax,[ebp-58h]; mov [eax+0Ch],edx
// At hook point: edx = CDB lucky points, [ebp-58h] = gacha handler object
// object+04 = gachapon_type (1=RT, 2=MP), object+0C = m_uiLuckyPoints
// Replaces edx with current + 20 (RT) or current + 10 (MP)

void __cdecl gachaLuckyPointsCallback(mg::RegisterState regs) {
    u32 objPtr = *reinterpret_cast<u32*>(regs.ebp - 0x58);
    if (!objPtr) return;
    
    u32 gachaType     = *reinterpret_cast<u32*>(objPtr + 0x04);
    u32 currentPoints = *reinterpret_cast<u32*>(objPtr + 0x0C);

    u32 newPoints;
    if (gachaType == 1) {         // RT gachapon
        newPoints = currentPoints + 20;
    } else if (gachaType == 2) {  // MP gachapon
        newPoints = currentPoints + 10;
    } else {
        return; // leave edx as-is for unknown types
    }

    //// Write to the saved EDX on the stack so popad restores our value
    //*const_cast<volatile u32*>(&regs.edx) = 0;
    
}

// ── State ────────────────────────────────────────────────────────────────────

inline u32 g_lastGachaId = 0;

// ── Helpers ──────────────────────────────────────────────────────────────────

inline void SendGachaPityRequest(u32 _this)
{
    // Check game state
    auto currState = *reinterpret_cast<u32*>(
        MG_CONST(addr::ui::state_mgr::CurrGameState));
    if (currState != GAME_STATE_GACHAPON)
        return;

	// Read gachapon_id from this + 0x818
	u32 gachaId = *reinterpret_cast<u32*>(_this + 0x818);
	if (gachaId == g_lastGachaId)
		return;
	g_lastGachaId = gachaId;

	auto& logger = mg::ctx().logger();
	//logger.info("GachaPity: sending request for gacha_id = {}", gachaId);
	auto GetNetMgr = reinterpret_cast<tGetNetMgr>(
        MG_CONST(addr::anticheat::game_managers::net_mgr::Get));

    GachaPityReq req{};
    req.m_tHeader.order = 95;
    req.gacha_id = gachaId;

    auto netMgr = GetNetMgr();
    if (netMgr && netMgr->CMainConnectorTcp)
    {
        netMgr->CMainConnectorTcp->_vptr_CConnector->Send(
            netMgr->CMainConnectorTcp,
            reinterpret_cast<char*>(&req), sizeof(req), 0, 0);
    }
}

// ── UI Hooks ─────────────────────────────────────────────────────────────────

void __fastcall hkGachaBack(u32 _this, u32 /*edx*/)
{
    auto& registry = mg::ctx().hookRegistry();
    auto original = registry.findDetour(HookId::GachaBack)
        ->getOriginal<decltype(&hkGachaBack)>();
    original(_this, 0);

    SendGachaPityRequest(_this);
}

void __fastcall hkGachaNext(u32 _this, u32 /*edx*/)
{
    auto& registry = mg::ctx().hookRegistry();
    auto original = registry.findDetour(HookId::GachaNext)
        ->getOriginal<decltype(&hkGachaNext)>();
    original(_this, 0);

    SendGachaPityRequest(_this);
}

void __fastcall hkGachaSelect(u32 _this, u32 /*edx*/)
{
    auto& registry = mg::ctx().hookRegistry();
    auto original = registry.findDetour(HookId::GachaSelect)
        ->getOriginal<decltype(&hkGachaSelect)>();
    original(_this, 0);

    SendGachaPityRequest(_this);
}

// ── Response Handler ─────────────────────────────────────────────────────────
// Packet layout: [SCommandHeader(4)] [u32 lucky_points(4)]
// Total 8 bytes, lucky_points at byte offset 4

char __cdecl handleGachaPityResponse(u16* a1)
{
    auto& logger = mg::ctx().logger();

    // Extract lucky points from response (offset 4 bytes past header)
    u32 luckyPoints = *reinterpret_cast<u32*>(reinterpret_cast<u8*>(a1) + 4);

    //logger.info("GachaPity: received lucky points = {}", luckyPoints);

    // Update all 5 game players
    auto GetUnitContainer = reinterpret_cast<tGetUnitContainer>(
        MG_CONST(addr::anticheat::game_managers::unit_container::Get));

    auto unitContainer = GetUnitContainer();
    if (unitContainer)
    {
        for (int i = 0; i < 5; i++)
        {
            auto player = unitContainer->m_pGamePlayer[i];
            if (player && player->m_pExPlayer && player->m_pExPlayer->m_kGachaponHandler)
            {
                player->m_pExPlayer->m_kGachaponHandler->m_uiLuckyPoints = luckyPoints;
            }
        }
    }

    // Refresh gacha dialog UI
    
    auto GUIManager = reinterpret_cast<CGUIManager*>(
        MG_CONST(addr::ui::gui_manager::Get));

    if (GUIManager)
    {
        auto GetDialogInfo = reinterpret_cast<tGetDialogInfo>(
            MG_CONST(addr::ui::gui_manager::GetDialogInfo));

        auto dialog = GetDialogInfo(GUIManager, MG_STR("E_DLG_GACHA"));
        if (dialog)
        {
            auto UpdateGachaLucky = reinterpret_cast<tUpdateGachaLucky>(
                MG_CONST(addr::ui::gacha::UpdateGachaLucky));
            UpdateGachaLucky(dialog);
        }
    }
    

    return 1;
}

} // anonymous namespace

// ── Public API ───────────────────────────────────────────────────────────────

GachaPityHandler::GachaPityHandler(::mg::MegaGuardContext& ctx, CustomPacketDispatcher& dispatcher)
    : ctx_(ctx)
    , dispatcher_(dispatcher)
{
}

GachaPityHandler::~GachaPityHandler() = default;

VoidResult GachaPityHandler::install()
{
    auto& registry = ctx_.hookRegistry();
    auto& logger   = ctx_.logger();

    // Register response handler for packet ID 95
    dispatcher_.registerHandler(95, &handleGachaPityResponse);

    // Hook gacha back button
    registry.registerDetour(HookId::GachaBack)
        .create(MG_CONST(addr::ui::gacha::GachaBack), &hkGachaBack);

    // Hook gacha next button
    registry.registerDetour(HookId::GachaNext)
        .create(MG_CONST(addr::ui::gacha::GachaNext), &hkGachaNext);

    // Hook gacha select / currency swap
    registry.registerDetour(HookId::GachaSelect)
        .create(MG_CONST(addr::ui::gacha::GachaSelect), &hkGachaSelect);

    // MidHook: override lucky points increment per gachapon type
    //luckyPointsHook_.create(
    //    MG_CONST(addr::ui::gacha::GachaLuckyPointsPatch),
    //    gachaLuckyPointsCallback, false);

    //logger.info("GachaPity: installed (3 UI hooks + packet handler + lucky points midhook)");
    return VoidResult::ok();
}

} // namespace mg::game
