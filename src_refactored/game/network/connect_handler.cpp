// =============================================================================
// ConnectHandler - Full implementation ported from network_connect_ack.h
// =============================================================================
// LoginAuthorizeReq: validates username/password, builds FrontLoginAuthorizeReq
// NetworkConnectAck: handles extra==54 (cast), extra==34 (front), extra==0 (main)
//   - main: sets session IDs, serial key, connects front socket,
//           zeroes out 5 game players' room info, runs InitContainerClass
// =============================================================================
#include "pch.hpp"
#include "game/network/connect_handler.hpp"
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

namespace mg::game {

namespace {

// ── Helpers ──────────────────────────────────────────────────────────────────

int nocrt_iscntrl(int c)
{
    if (c <= 0x1f) return 1; // NUL to US
    if (c == 0x7f) return 1; // DEL
    return 0;
}

// ── Packet structures ────────────────────────────────────────────────────────

struct FrontLoginAuthorizeReq
{
    SCommandHeader m_tHeader;
    u32   cryptoKey;
    char  password[40];
    u32   serverTime;
    char  username[68];
};

struct FrontEngineConnectAck
{
    i32  cryptoKey;
    u32  serverTime;
};

// MainEngineConnectAck is now ServerKeyPayload (from secure_channel.hpp)
// The cryptoKey + uniqueId are AEAD-encrypted inside it — decrypted by SecureChannel

struct CastEngineConnectAck
{
    i32 cryptoKey;
};

// ── Type aliases for game function pointers ──────────────────────────────────

using tGetNetMgr         = CNetMgr*       (__cdecl*)();
using tGetUnitContainer  = CUnitContainer* (__cdecl*)();
using tInitContainerClass = void(__thiscall*)(CUnitContainer*, int);

// ── LoginAuthorizeReq ────────────────────────────────────────────────────────

bool LoginAuthorizeReq()
{
    auto GetNetMgr = reinterpret_cast<tGetNetMgr>(
        MG_CONST(addr::anticheat::game_managers::net_mgr::Get));
    auto* UIMsgBox = *reinterpret_cast<CUIMsgBox**>(
        MG_CONST(addr::ui::ui_msgbox::Get));

    auto loginUsername = GetNetMgr()->loginUsername;
    auto loginPassword = GetNetMgr()->loginPassword;
    int loginUsernameLength = 0, loginPasswordLength = 0;

    // Validate username
    for (int i = 0; i < 67 && loginUsername[i] && loginUsername[i] != ' '; i++)
    {
        if (nocrt_iscntrl(loginUsername[i]))
        {
            UIMsgBox->_vptr_CUIMsgBox->CreateBox(UIMsgBox,
                MG_STR("LOGIN_NULL_ID"), 0, 0, 1, 0);
            return false;
        }
        loginUsernameLength++;
    }

    // Validate password
    for (int i = 0; i < 39 && loginPassword[i] && loginPassword[i] != ' '; i++)
    {
        if (nocrt_iscntrl(loginPassword[i]))
        {
            UIMsgBox->_vptr_CUIMsgBox->CreateBox(UIMsgBox,
                MG_STR("ROOM_CONFIG_WRONGPASSWORD"), 0, 0, 1, 0);
            return false;
        }
        loginPasswordLength++;
    }

    loginUsername[loginUsernameLength] = 0;
    loginPassword[loginPasswordLength] = 0;

    if (loginUsernameLength < 1)
    {
        UIMsgBox->_vptr_CUIMsgBox->CreateBox(UIMsgBox,
            MG_STR("LOGIN_ID_SHORTLEN"), 0, 0, 1, 0);
        return false;
    }
    if (loginPasswordLength < 1)
    {
        UIMsgBox->_vptr_CUIMsgBox->CreateBox(UIMsgBox,
            MG_STR("LOGIN_PASSWD_SHORTLEN"), 0, 0, 1, 0);
        return false;
    }

    FrontLoginAuthorizeReq loginReq{};
    loginReq.m_tHeader.order = 22;
    loginReq.m_tHeader.extra = 52;
    loginReq.cryptoKey = GetNetMgr()->CFrontConnectorTcp->m_iSerialKey;
    nocrtStrcpy(loginReq.username, loginUsername);
    nocrtStrcpy(loginReq.password, loginPassword);

    // Pad remaining username bytes with random data
    if (loginUsernameLength + 1 < 68)
        for (int i = loginUsernameLength + 1; i < 67; i++)
            loginReq.username[i] = static_cast<char>((static_cast<double>(rand()) * 254.0 / 32768.0) + 1);

    // Pad remaining password bytes with random data
    if (loginPasswordLength + 1 < 40)
        for (int i = loginPasswordLength + 1; i < 39; i++)
            loginReq.password[i] = static_cast<char>((static_cast<double>(rand()) * 254.0 / 32768.0) + 1);

    loginReq.serverTime = GetNetMgr()->server_time;

    auto ConnectorTcp = GetNetMgr()->CFrontConnectorTcp;
    if (ConnectorTcp)
    {
        if (ConnectorTcp->_vptr_CConnector->Send(ConnectorTcp, reinterpret_cast<char*>(&loginReq), 116, 0, 4) < 0)
            return false;
        else
            return true;
    }
    return false;
}

// ── NetworkConnectAck ────────────────────────────────────────────────────────

void __cdecl hkNetworkConnectAck(CCommand* cmd)
{
    auto GetNetMgr = reinterpret_cast<tGetNetMgr>(
        MG_CONST(addr::anticheat::game_managers::net_mgr::Get));
    auto GetUnitContainer = reinterpret_cast<tGetUnitContainer>(
        MG_CONST(addr::anticheat::game_managers::unit_container::Get));

    if (cmd->m_tHeader.extra)
    {
        if (cmd->m_tHeader.extra == 54)
        {
            // Cast engine connect
            auto engineConnect = reinterpret_cast<CastEngineConnectAck*>(cmd->m_acData);
            GetNetMgr()->CCastConnectorTcp->m_iRegisterIndex = -2;
        }
        else if (cmd->m_tHeader.extra == 34)
        {
            // Front engine connect
            auto engineConnect = reinterpret_cast<FrontEngineConnectAck*>(cmd->m_acData);
            GetNetMgr()->server_time = engineConnect->serverTime;
            GetNetMgr()->CFrontConnectorTcp->m_iSerialKey = engineConnect->cryptoKey;

            if (LoginAuthorizeReq())
            {
                nocrtMemset(GetNetMgr()->loginPassword, 0, 40);
                nocrtMemset(GetNetMgr()->loginKey, 0, 500);
            }
        }
    }
    else
    {
        // Main engine connect (extra == 0)
        // The entire m_acData is a ServerKeyPayload: signed {x25519_pk, connect_data}
        auto keys = reinterpret_cast<const ServerKeyPayload*>(cmd->m_acData);

        // Verify Ed25519 signature and init secure channel (ephemeral keypair + session keys)
        auto& channel = SecureChannel::instance();
        if (!channel.onServerKeyReceived(*keys))
        {
            MG_LOG(mg::ctx().logger(),
                "[SecureChannel] Server key verification FAILED — signature invalid");
            return;
        }

        // Connect data is plaintext inside the signed payload
        auto& connectData = keys->connect_data;

        GetNetMgr()->CMainConnectorTcp->m_iSessionId2 = connectData.uniqueId.session;
        GetNetMgr()->CMainConnectorTcp->m_iSessionId  = connectData.uniqueId.session;
        GetNetMgr()->CMainConnectorTcp->m_iSerialKey   = connectData.cryptoKey;

        if (GetNetMgr()->CFrontConnectorTcp)
        {
            if (!GetNetMgr()->CFrontConnectorTcp->m_pkSocket->m_iNetworkState)
                GetNetMgr()->CFrontConnectorTcp->m_pkSocket->m_iNetworkState = 1;

            GetNetMgr()->CFrontConnectorTcp->_vptr_CConnector->sub_AB0760(
                GetNetMgr()->CFrontConnectorTcp,
                GetNetMgr()->CFrontConnectorTcp->m_pkSocket->m_skSocket, true);
            GetNetMgr()->byte103D = 1;
        }

        GetNetMgr()->CMainConnectorTcp->m_iRegisterIndex = cmd->m_tHeader.option; // server_id
        GetNetMgr()->CExConnectorUdp->m_iRegisterIndex   = cmd->m_tHeader.option;
        GetNetMgr()->CExConnectorUdp->m_iSessionId2      = connectData.uniqueId.session;

        auto my_unique_id = UniqueId(connectData.uniqueId.session, cmd->m_tHeader.option);

        // Zero out all 5 game players' room info
        for (int i = 0; i < 5; i++)
        {
            auto m_pGamePlayer = GetUnitContainer()->m_pGamePlayer[i];
            auto roomInfo = m_pGamePlayer->m_pExPlayer->m_kRoomInfo;
            roomInfo->uniqueId = my_unique_id;

            // Zero out the room state block (0xBD0 to 0xC4C range)
            // These are the dword0bd0..dword0c4c fields from the original
            u8* zeroStart = reinterpret_cast<u8*>(roomInfo) + 0x0BD0;
            nocrtMemset(zeroStart, 0, 0x80); // 0xBD0 to 0xC50

            // Additional specific fields
            u8* fieldBase = reinterpret_cast<u8*>(roomInfo);
            *reinterpret_cast<u32*>(fieldBase + 0x0B94) = 0;
            *reinterpret_cast<u32*>(fieldBase + 0x0B98) = 0;
            *reinterpret_cast<u32*>(fieldBase + 0x0B9C) = 0;
            *reinterpret_cast<u32*>(fieldBase + 0x0BA0) = 0;
            *reinterpret_cast<u32*>(fieldBase + 0x0CA4) = 0;
            *reinterpret_cast<u32*>(fieldBase + 0x0B88) = 0;
            *reinterpret_cast<u32*>(fieldBase + 0x0B8C) = 0;

            auto& dword0c14 = *reinterpret_cast<u32*>(fieldBase + 0x0C14);
            dword0c14 &= 1u;

            auto& dword0bd0 = *reinterpret_cast<u32*>(fieldBase + 0x0BD0);
            auto& dword0bd4 = *reinterpret_cast<u32*>(fieldBase + 0x0BD4);

            if (static_cast<i32>(dword0bd0) >= 0)
            {
                dword0c14 &= ~1u;
                dword0bd4 = (0 << 14) | (dword0bd4 & 0x3FFF);
            }
            else
            {
                dword0c14 |= 1u;
                dword0bd4 = (0xFFFFC000 * 0) | (dword0bd4 & 0x3FFF);
            }

            *reinterpret_cast<u32*>(fieldBase + 0x0CA4) = 0;
            *reinterpret_cast<u32*>(fieldBase + 0x0B90) = 0;
            *reinterpret_cast<u32*>(fieldBase + 0x1110) = 0;
        }

        // Initialize container class if player 5 doesn't exist
        auto InitContainerClass = reinterpret_cast<tInitContainerClass>(MG_CONST(0x009115C0));
        if (!GetUnitContainer()->m_pGamePlayer[5])
            InitContainerClass(GetUnitContainer(), 0);

        GetNetMgr()->CMainConnectorTcp->m_bIsConnected = 1;
        GetNetMgr()->lastConnectTick = 0;
    }
}

} // anonymous namespace

// ── ConnectHandler class ─────────────────────────────────────────────────────

ConnectHandler::ConnectHandler(MegaGuardContext& ctx) : ctx_(ctx) {}
ConnectHandler::~ConnectHandler() = default;

VoidResult ConnectHandler::install() {
    auto& registry = ctx_.hookRegistry();
    registry.registerDetour(HookId::NetworkInitCrypto)
        .create(MG_CONST(addr::anticheat::ack_handlers::NetworkInitCrypto), hkNetworkConnectAck);
    return VoidResult::ok();
}

} // namespace mg::game
