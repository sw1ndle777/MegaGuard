// =============================================================================
// DisconnectHandler - Order 73 disconnect packet hook
// =============================================================================
#include "pch.hpp"
#include "game/network/disconnect_handler.hpp"

#include "core/context.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "game/game_engine_log.hpp"
#include "game/structures.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::game {

namespace {

using tGetNetMgr = CNetMgr* (__cdecl*)();
using tAuthorizeDisconnectAll = void(__thiscall*)(CNetMgr*, const char*, u32, u32);
using tDisconnectOfflinePrepare = void(__thiscall*)(CNetMgr*, int);
using tDisconnectOfflineFinalize = void(__stdcall*)();
using tStateMgrSetTransition = void(__thiscall*)(uptr, GameState);

char __cdecl hkDisconnectPacket(CTcpPacket* data)
{
    auto GetNetMgr = reinterpret_cast<tGetNetMgr>(
        MG_CONST(addr::anticheat::game_managers::net_mgr::Get));
    auto AuthorizeDisconnectAll = reinterpret_cast<tAuthorizeDisconnectAll>(
        MG_CONST(addr::ui::AuthorizeDisconnectAll));
    auto DisconnectOfflinePrepare = reinterpret_cast<tDisconnectOfflinePrepare>(
        MG_CONST(addr::ui::DisconnectOfflinePrepare));
    auto CloseAllMessages = reinterpret_cast<tCloseAllMessages>(
        MG_CONST(addr::ui::CloseAllMessages));
    auto DisconnectOfflineFinalize = reinterpret_cast<tDisconnectOfflineFinalize>(
        MG_CONST(addr::ui::DisconnectOfflineFinalize));
    auto StateMgrSetTransition = reinterpret_cast<tStateMgrSetTransition>(
        MG_CONST(addr::ui::state_mgr::SetTransition));
    auto* UIMsgBox = *reinterpret_cast<CUIMsgBox**>(
        MG_CONST(addr::ui::ui_msgbox::Get));

    const char* idCpp = MG_STR("c:\\buildproject\\frameworks\\game\\net\\ID.cpp");

    switch (data->m_kCommand.m_tHeader.extra)
    {
    case 42:
        GameEngineLog::errLine(idCpp, 130, MG_STR("ID_QUIT: BLOCK"));
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("ID_QUIT_BLOCK"), 0, 41);
        break;

    case 47:
        GameEngineLog::errLine(idCpp, 157, MG_STR("ID_QUIT: OFFLINE"));
        DisconnectOfflinePrepare(GetNetMgr(), 41);
        CloseAllMessages(UIMsgBox);
        UIMsgBox->_vptr_CUIMsgBox->CreateBox(UIMsgBox, MG_STR("ID_QUIT_OFFLINE"), 0, 0, 1, false);
        DisconnectOfflineFinalize();
        return 1;

    case 27:
        GameEngineLog::errLine(idCpp, 171, MG_STR("ID_QUIT: CLOSE"));
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("ID_QUIT_CLOSE"), 13, 41);
        break;

    case 4:
        GameEngineLog::errLine(idCpp, 181, MG_STR("ID_QUIT: DATA_ERROR"));
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("ID_QUIT_DATAERROR"), 13, 41);
        break;

    case 5:
        GameEngineLog::errLine(idCpp, 191, MG_STR("ID_QUIT: BUSY"));
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("ID_QUIT_BUSY"), 13, 41);
        break;

    case 35:
        GameEngineLog::errLine(idCpp, 201, MG_STR("ID_QUIT: DENY"));
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("ID_QUIT_DENY"), 0, 41);
        *reinterpret_cast<u8*>(reinterpret_cast<uptr>(UIMsgBox) + 0xAE) = 0;
        break;

    default:
        GameEngineLog::errLineFmt(idCpp, 212, MG_STR("ID_QUIT : FAIL(%u)"), data->m_kCommand.m_tHeader.extra);
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("ID_QUIT_LOCK"), 7, 41);
        break;
    }

    StateMgrSetTransition(MG_CONST(addr::ui::state_mgr::Instance), GAME_STATE_LOGIN);
    return 1;
}

} // anonymous namespace

DisconnectHandler::DisconnectHandler(MegaGuardContext& ctx) : ctx_(ctx) {}
DisconnectHandler::~DisconnectHandler() = default;

VoidResult DisconnectHandler::install() {
    auto& registry = ctx_.hookRegistry();
    registry.registerDetour(HookId::DisconnectPacket)
        .create(MG_CONST(addr::anticheat::ack_handlers::Disconnect), hkDisconnectPacket);
    return VoidResult::ok();
}

} // namespace mg::game
