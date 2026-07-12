// =============================================================================
// AuthorizeHandler - Full implementation ported from front_main_authorize.h
// =============================================================================
// MainAuthorize: builds MainAuthorizeReq (order=68, extra=37, option=2)
// RequestChannelInfo: sends FrontChannelInfoReq (order=23)
// FrontAuthorizeAck: parses auth response, extracts auth keys, calls channel info
// =============================================================================
#include "pch.hpp"
#include "game/network/authorize_handler.hpp"
#include "game/game_engine_log.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "platform/memory.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"
#include "utils/logger.hpp"
#include "game/network/secure_channel.hpp"
#include "game/network/heartbeat_handler.hpp"
#include "anticheat/manual_seh.hpp"
namespace mg::game {

namespace {

// ── Packet structures ────────────────────────────────────────────────────────

// MainAuthorizeReq is now SecurePacket<SecureAuthPayload> (from secure_channel.hpp)
// Wire: { header(4) | client_pubkey(32) | mac(16) | encrypted(1136) } = 1188 bytes total

struct FrontChannelInfoReq
{
    SCommandHeader m_tHeader;
};

// ── Type aliases ─────────────────────────────────────────────────────────────

using tGetNetMgr = CNetMgr* (__cdecl*)();
using tAuthorizeDisconnectAll = void(__thiscall*)(CNetMgr*, const char*, u32, u32);
using tGetDialogInfo = int* (__thiscall*)(CGUIManager*, const char*);
using tInitPopupTitles = void(__thiscall*)(int*, int);
using tIdkPopup = void(__thiscall*)(int*, int);
// ── RequestChannelInfo ───────────────────────────────────────────────────────

bool RequestChannelInfo()
{
    auto GetNetMgr = reinterpret_cast<tGetNetMgr>(
        MG_CONST(addr::anticheat::game_managers::net_mgr::Get));

    FrontChannelInfoReq channelInfoReq{};
    channelInfoReq.m_tHeader.order = 23;

    if (GetNetMgr()->CFrontConnectorTcp)
        return (GetNetMgr()->CFrontConnectorTcp->_vptr_CConnector->Send(
            GetNetMgr()->CFrontConnectorTcp,
            reinterpret_cast<char*>(&channelInfoReq), 0, 0, 0) >= 0);
    else
        return false;
}

// ── MainAuthorize hook ───────────────────────────────────────────────────────

void __cdecl hkMainAuthorize(u8 extra, int a2)
{
    auto GetNetMgr = reinterpret_cast<tGetNetMgr>(
        MG_CONST(addr::anticheat::game_managers::net_mgr::Get));

    auto& channel = SecureChannel::instance();
    if (!channel.isEstablished())
    {
        MG_LOG(mg::ctx().logger(),
            "[SecureChannel] MainAuthorize called but channel not established");
        return;
    }

    // Build plaintext payload
    SecureAuthPayload payload{};
    payload.auth_key   = GetNetMgr()->auth_key;
    payload.auth_key2  = GetNetMgr()->auth_key2;
    payload.server_id  = GetNetMgr()->serverinfo_sid;
    payload.netVersion1 = 3;
    payload.netVersion2 = 0;
    payload.netVersion3 = 1;
    payload.netVersion4 = 0;
    SecureChannel::collectHwid(payload.hwid);
    computeFileIntegrity(payload.integrity);

    // MG_LOG(mg::ctx().logger(), "[SecureChannel] File integrity computed for auth");

    // Build encrypted packet
    SecurePacket<SecureAuthPayload> pkt{};
    pkt.m_tHeader.order  = 68;
    pkt.m_tHeader.extra  = 37;
    pkt.m_tHeader.option = 2;

    if (!pkt.seal(channel, payload))
    {
        MG_LOG(mg::ctx().logger(), "[SecureChannel] Failed to seal auth packet");
        return;
    }

    // Send via main connector
    // data_size = everything after header = pubkey(32) + mac(16) + ciphertext(1136) = 1184
    auto netMgr = GetNetMgr();
    if (netMgr->CMainConnectorTcp)
    {
        netMgr->CMainConnectorTcp->_vptr_CConnector->Send(
            netMgr->CMainConnectorTcp,
            reinterpret_cast<char*>(&pkt),
            SecurePacket<SecureAuthPayload>::dataSize(), 0, 0);
    }
}

void ShowDlgInputPopup(CUIMsgBox* UIMsgBox)
{
    auto GUIManager = reinterpret_cast<CGUIManager*>(
        MG_CONST(addr::ui::gui_manager::Get));

    auto GetDialogInfo = reinterpret_cast<tGetDialogInfo>(
		MG_CONST(addr::ui::gui_manager::GetDialogInfo));

    auto InitInputTitles = reinterpret_cast<tInitPopupTitles>(
		MG_CONST(addr::ui::input_popup::InitTitles));

	auto IdkPopup = reinterpret_cast<tIdkPopup>(
        MG_CONST(addr::ui::input_popup::IdkPopup));

	auto dialog = GetDialogInfo(GUIManager, MG_STR("E_DLG_INPUT_POP"));
    if (dialog)
    {
		InitInputTitles(dialog, 6);
		UIMsgBox->_vptr_CUIMsgBox->CreateDialogByInfo(UIMsgBox, dialog);
        IdkPopup(dialog, 1);
    }
}

// ── FrontAuthorizeAck hook ───────────────────────────────────────────────────

void __cdecl hkFrontAuthorizeAck(CCommand* cmd)
{
    auto GetNetMgr = reinterpret_cast<tGetNetMgr>(
        MG_CONST(addr::anticheat::game_managers::net_mgr::Get));
    auto AuthorizeDisconnectAll = reinterpret_cast<tAuthorizeDisconnectAll>(
        MG_CONST(addr::ui::AuthorizeDisconnectAll));
    auto* UIMsgBox = *reinterpret_cast<CUIMsgBox**>(
        MG_CONST(addr::ui::ui_msgbox::Get));

    auto GameNew = reinterpret_cast<tGameNew>(MG_CONST(addr::msvc::operator_new));


    auto kLoginCpp = MG_STR("c:\\buildproject\\frameworks\\game\\net\\LOGIN.cpp");

    switch (cmd->m_tHeader.extra)
    {
    case 1:
    case 37:
        if (cmd->m_tHeader.extra == 1)
        {
            GetNetMgr()->auth_key = *reinterpret_cast<u32*>(cmd->m_acData);
            GetNetMgr()->auth_key2 = *reinterpret_cast<u32*>(&cmd->m_acData[4]);
            GetNetMgr()->grade = cmd->m_tHeader.option;
            mg::writeValue<u32>(MG_CONST(0x011DBAAC), 0, GetNetMgr()->grade < 7 ? 0 : 2);
        }

        if (GetNetMgr()->auth_key && GetNetMgr()->auth_key2)
        {
            if (!RequestChannelInfo())
            {
                GameEngineLog::errLine(kLoginCpp, 64,
                   MG_STR("LOGIN_AUTHORIZE: TIME_EXPIRE 1"));
                AuthorizeDisconnectAll(GetNetMgr(), MG_STR("LOGIN_AUTHORIZE_FAIL"), 5, 41);
            }
        }
        else
        {
            GameEngineLog::errLineFmt(kLoginCpp, 94, MG_STR("LOGIN_AUTHORIZE: FAIL: %u : %u"), GetNetMgr()->auth_key, GetNetMgr()->auth_key2);
            AuthorizeDisconnectAll(GetNetMgr(), MG_STR("LOGIN_AUTHORIZE_DONT_EXIST"), 6, 41);
        }
        break;

    case 5:
    {
        GameEngineLog::errLine(kLoginCpp, 125,
            MG_STR("LOGIN_AUTHORIZE: BUSY"));

        GetNetMgr()->auth_key = *reinterpret_cast<u32*>(cmd->m_acData);
        GetNetMgr()->auth_key2 = *reinterpret_cast<u32*>(&cmd->m_acData[4]);
        GetNetMgr()->grade = cmd->m_tHeader.option;
        mg::writeValue<u32>(MG_CONST(0x011DBAAC), 0, GetNetMgr()->grade < 7 ? 0 : 2);

        // Close all pending messages
        auto CloseAll = reinterpret_cast<tCloseAllMessages>(
            MG_CONST(addr::ui::CloseAllMessages));
        CloseAll(UIMsgBox);
        /*
        // Create CMsgDataLoginBanUser
        auto* msgData = GameNew(0x630);
        if (msgData)
        {
            auto Init = reinterpret_cast<tIMsgDataInit>(MG_CONST(addr::ui::IMsgDataInit));
            Init(msgData, 0, 0, 0, 0, 0, 0, 0);
            *reinterpret_cast<uptr*>(msgData) = MG_CONST(addr::ui::CMsgDataLoginBanUser_vftbl);
        }
		UIMsgBox->_vptr_CUIMsgBox->CreateBox(UIMsgBox, MG_STR("ID_AUTHORIZE_BUSY"), msgData, 0, 1, 0);
        */
        ShowDlgInputPopup(UIMsgBox);
        break;
    }
    case 6:
        GameEngineLog::errLine(kLoginCpp, 146,
            MG_STR("LOGIN_AUTHORIZE: 2FA FAIL"));
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("LOGIN_AUTHORIZE_2FA_FAIL"), 7, 41);
		break;

    case 7:
        GameEngineLog::errLine(kLoginCpp, 146,
            MG_STR("LOGIN_AUTHORIZE: EMAIL FAIL"));
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("LOGIN_AUTHORIZE_EMAIL_FAIL"), 7, 41);
        break;

    case 8:
        GameEngineLog::errLine(kLoginCpp, 146,
            MG_STR("LOGIN_AUTHORIZE: TOO MANY ATTEMPTS"));
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("LOGIN_AUTHORIZE_COOLDOWN_FAIL"), 7, 41);
        break;
    case 42:
        GameEngineLog::errLine(kLoginCpp, 146,
            MG_STR("LOGIN_AUTHORIZE: BLOCK"));
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("LOGIN_AUTHORIZE_BLOCK"), 7, 41);
        break;

    case 13:
        GameEngineLog::errLine(kLoginCpp, 176,
            MG_STR("LOGIN_AUTHORIZE: DONT_EXIST"));
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("LOGIN_AUTHORIZE_DONT_EXIST"), 8, 41);
        break;

    case 35:
        GameEngineLog::errLine(kLoginCpp, 205,
            MG_STR("LOGIN_AUTHORIZE: DENY"));
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("LOGIN_AUTHORIZE_DENY"), 10, 41);
        break;

    case 24:
        GameEngineLog::errLine(kLoginCpp, 235, MG_STR("LOGIN_AUTHORIZE: TIME_EXPIRE 2"));
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("LOGIN_AUTHORIZE_FAIL"), 5, 41);
        break;

    case 4:
        GameEngineLog::errLine(kLoginCpp, 263, MG_STR("LOGIN_AUTHORIZE: DATA_ERROR"));
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("LOGIN_AUTHORIZE_DATA_ERROR"), 9, 41);
        break;

    default:
        GameEngineLog::errLine(kLoginCpp, 291, MG_STR("LOGIN_AUTHORIZE: FAIL"));
        AuthorizeDisconnectAll(GetNetMgr(), MG_STR("LOGIN_AUTHORIZE_DONT_EXIST"), 6, 41);
        break;
    }
}

} // anonymous namespace

// ── AuthorizeHandler class ───────────────────────────────────────────────────

AuthorizeHandler::AuthorizeHandler(MegaGuardContext& ctx) : ctx_(ctx) {}
AuthorizeHandler::~AuthorizeHandler() = default;

VoidResult AuthorizeHandler::install() {
    auto& registry = ctx_.hookRegistry();
    registry.registerDetour(HookId::MainAuthorize)
        .create(MG_CONST(addr::anticheat::req_handlers::MainAuthorize), hkMainAuthorize);

    registry.registerDetour(HookId::FrontAuthorize)
		.create(MG_CONST(addr::anticheat::ack_handlers::FrontAuthorize), hkFrontAuthorizeAck);
    return VoidResult::ok();
}

} // namespace mg::game
