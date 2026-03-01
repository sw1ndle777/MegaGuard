// =============================================================================
// Game Structures - POD types for the target game client
// =============================================================================
// Pure data definitions. No logic, no globals, no includes beyond types.
// These match the game's binary layout exactly — DO NOT reorder fields.
// =============================================================================
#pragma once

#include "core/types.hpp"
#include "platform/memory.hpp"

namespace mg::game {

    enum GameState : u32 {
        GAME_STATE_NONE = 0u,
        GAME_STATE_NONE2 = 1u,
        GAME_STATE_INTRO = 2u,
        GAME_STATE_LOGIN = 3u,
        GAME_STATE_SELECT_SERVER = 4u,
        GAME_STATE_ROOM_JOIN = 5u,
        GAME_STATE_CHANNEL_JOIN = 6u,
        GAME_STATE_CLAN_PARTY_JOIN = 7u,
        GAME_STATE_ROOM_CREATE = 8u,
        GAME_STATE_CLAN_PARYT_CREATE = 9u,
        GAME_STATE_PARTY_MATCH_JOIN = 10u,
        GAME_STATE_PARTH_MATCH_CREATE = 11u,
        GAME_STATE_PARTY_MATCH_WAIT_ROOM = 12u,
        GAME_STATE_PARTY_MATCH_PLAY_WAIT_ROOM = 13u,
        GAME_STATE_FOLLOW_SERVER = 14u,
        GAME_STATE_AVATAR_SELECT = 15u,
        GAME_STATE_AVATAR_CREATE = 16u,
        GAME_STATE_PLAYERID_CREATE = 17u,
        GAME_STATE_UNKNOWN_18 = 18u,
        GAME_STATE_VENDOR = 19u,
        GAME_STATE_INVENTORY = 20u,
        GAME_STATE_GACHAPON = 21u,
        GAME_STATE_PLAYER_CREATE = 22u,
        GAME_STATE_TUTORIAL = 23u,
        GAME_STATE_TUTORIAL_LOAD = 24u,
        GAME_STATE_AGORA_LOAD = 25u,
        GAME_STATE_ROOM = 26u,
        GAME_STATE_PARTY = 27u,
        GAME_STATE_CLANROOM = 28u,
        GAME_STATE_MOD_LOADING = 29u,
        GAME_STATE_MOD_LOADED = 30u,
        GAME_STATE_MOD_PLAYING = 31u,
        GAME_STATE_TUTORIAL_PLAYING = 32u,
        GAME_STATE_AGORA_PLAYING = 33u,
        GAME_STATE_MOD_STOP = 34u,
        GAME_STATE_MOD_END = 35u,
    };


using tGameNew = void* (__cdecl*)(size_t);

// ── Old game string type ──────────────────────────────────────────────────────

class OldString {
public:
    u32 pad;
    union {
        char* str;
        char shortstr[16];
    } bx;
    std::size_t size;
    std::size_t capacity;

    OldString& operator=(const char* newStr) {
        std::size_t newLength = nocrtStrlen(newStr);
        if (newLength <= 15) {
            nocrtMemcpy(bx.shortstr, newStr, newLength + 1);
            size = newLength;
            capacity = 15;
        } else {
            if (newLength <= capacity)
                nocrtMemcpy(bx.str, newStr, newLength + 1);
            else {
                delete[] bx.str;
                bx.str = new char[newLength + 1];
                nocrtStrcpy(bx.str, newStr);
                capacity = newLength;
            }
            size = newLength;
        }
        return *this;
    }
};

// ── Packet/Command Headers ────────────────────────────────────────────────────

#pragma pack(push, 1)

union STcpPacketHeader {
    struct {
        u32 bogus     : 4;
        u32 sessionId : 14;
        u32 size      : 11;
        u32 crypt     : 3;
    };
    u32 data{ 0 };

    STcpPacketHeader() { nocrtMemset(this, 0, sizeof(STcpPacketHeader)); }
    explicit STcpPacketHeader(u32 d) { nocrtMemset(this, 0, sizeof(STcpPacketHeader)); data = d; }
};

union SCommandHeader {
    struct {
        u32 bogus   : 4;
        u32 mission : 2;
        u32 order   : 10;
        u32 extra   : 8;
        u32 option  : 8;
    };
    u32 data;

    SCommandHeader() {
        nocrtMemset(this, 0, sizeof(SCommandHeader));
        this->bogus = ((int)((double)rand() * 254.0f / 32768.0f) + 1) & 0xF;
    }
    explicit SCommandHeader(u32 d) { nocrtMemset(this, 0, sizeof(SCommandHeader)); data = d; }
};

union UniqueId {
    struct {
        u32 session : 16;
        u32 server  : 15;
        u32 unknown : 1;
    };
    u32 data{ 0 };

    UniqueId() { nocrtMemset(this, 0, sizeof(UniqueId)); }
    explicit UniqueId(u32 d) { nocrtMemset(this, 0, sizeof(UniqueId)); data = d; }
    UniqueId(u16 sess, u16 srv) {
        nocrtMemset(this, 0, sizeof(UniqueId));
        session = sess;
        server = srv & 0x7FFF;
        unknown = 0;
    }
};

#pragma pack(pop)

// ── NAT Types ─────────────────────────────────────────────────────────────────

enum NatType : i32 {
    kNatNone           = 0,
    kNatFirewall       = 1,
    kNatFullCone       = 2,
    kNatRestricted     = 3,
    kNatPortRestricted = 4,
    kNatSymmetric      = 5,
    kNatMax            = 6,
};

// ── Stream & Sync primitives ──────────────────────────────────────────────────

struct CStreamBuffer {
    char*       m_acBuffer;
    std::size_t m_stMax;
    std::size_t m_stSize;
};

struct CMutex {
    CRITICAL_SECTION m_tMutex;
};

struct CLocker : CMutex {};

// ── Socket layer ──────────────────────────────────────────────────────────────

struct CRawSocket_vtbl {
    void* DestructDelete;
    void* Create;
    void* Disconnect;
    void* Connect;
    void* Read;
    void* Send;
    void* SendTrust;
    void* SendQueue;
    void* ClearSendQueue;
    void* sub_E0D4F0;
    void* sub_E13BB0;
    void* SendBlock;
};

struct CRawSocket {
    CRawSocket_vtbl* _vptr_CRawSocket;
    char          pad[4];
    sockaddr_in   m_saLocalAddr_in;
    sockaddr_in   m_saPublicAddr_in;
    sockaddr_in   m_saRemoteAddr_in;
    int           m_slSize;
    SOCKET        m_skSocket;
    CLocker       m_kLocker;
    DWORD         m_TickMs;
    NatType       m_eNatType;
    u64           m_uliTick;
    bool          m_bConnected;
    bool          m_bIdk1;
    bool          m_bIdk2;
    bool          m_bIdk3;
    DWORD         dword006c;
    DWORD         dword0070;
    DWORD         dword0074;
    DWORD         dword0078;
    DWORD         m_iNetworkState;
};

struct CAllocator_CTcpPacketBuffer {
    void*  _vptr;
    struct CTcpPacketBuffer* m_NextOfPool;
};

struct CTcpPacketBuffer : CAllocator_CTcpPacketBuffer {
    char          m_acBuffer[1440];
    std::size_t   m_stFrontOffset;
    std::size_t   m_stRearOffset;
    std::size_t   m_stSize;
};

struct std_deque_CTcpPacketBuffer {
    CTcpPacketBuffer** _map;
    std::size_t        _size;
    std::size_t        _capacity;
};

struct CBlockQueue {
    std_deque_CTcpPacketBuffer m_kDeque;
};

struct CTcpSocket : CRawSocket {
    int         m_iRecvFlags;
    int         m_iSendFlags;
    CBlockQueue m_kBlockQueue;
};

// ── Command ───────────────────────────────────────────────────────────────────

struct CCommand {
    SCommandHeader m_tHeader;
    char           m_acData[1424];
};

// ── Connector vtable ──────────────────────────────────────────────────────────

struct CTcpConnector;
struct CTcpConnector_vtbl {
    void* sub_AB8800;
    void* sub_E12AE0;
    void* sub_B27A50;
    int   (__thiscall* Send)(CTcpConnector* instance, char* CCommand, int data_size, int arg8, int arg5);
    int   (__thiscall* SendObsolete)(CTcpConnector* instance, CCommand* cmd, int a3, int a4);
    bool  (__thiscall* KeepAlive)(CTcpConnector* instance);
    void* sub_5229C0;
    char  (__thiscall* ResetRecvQueue)(CTcpConnector* instance);
    void* sub2_B27A50;
    void  (__thiscall* Clear)(CTcpConnector* instance);
    void* sub_522A00;
    char  (__thiscall* Read)(CTcpConnector* instance);
    void* sub_55B900;
    char  (__thiscall* Connect)(CTcpConnector* instance, const char* szAddr, int iPort);
    char  (__thiscall* sub_AB0760)(CTcpConnector* instance, int a2, bool a3);
    u64   (__thiscall* SetConnected)(CTcpConnector* instance);
    void* sub_E11300;
    void* sub_E114F0;
    void  (__thiscall* Encrypt)(CTcpConnector* instance, int a2, void* pvIn, void* pvOut, int size);
    void  (__thiscall* Decrypt)(CTcpConnector* instance, int a2, void* pvIn, void* pvOut, int size);
    void* sub_E13940;
    bool  (__stdcall* CheckRecvHeader)(STcpPacketHeader* a1);
};

// ── Connectors ────────────────────────────────────────────────────────────────

#pragma pack(push, 1)
struct CConnector {
    CTcpConnector_vtbl* _vptr_CConnector;  // 0x0000
    CLocker     m_kLocker;                 // 0x0004
    CTcpSocket* m_pkSocket;                // 0x001C
    void*       m_pkCommandQueue;          // 0x0020
    void*       m_pkSensor;                // 0x0024
    void*       m_pkDispatcher;            // 0x0028
    u64         m_ullKeepAliveTick;        // 0x002C
    u16         m_iSessionId;              // 0x0034
    u16         m_iSessionId2;             // 0x0036
    i32         m_iSerialKey;              // 0x0038
    int         m_iRegisterIndex;          // 0x003C
    int         m_iRelay;                  // 0x0040
    bool        m_bRegister;               // 0x0044
    bool        m_bHeaderCrypt;            // 0x0045
    bool        m_bIdk1;                   // 0x0046
    bool        m_bIdk2;                   // 0x0047
    bool        m_bIdk3;                   // 0x0048
    bool        m_bIdk4;                   // 0x0049
    bool        m_bIdk5;                   // 0x004A
    bool        m_bIdk6;                   // 0x004B
};
#pragma pack(pop)

struct CTcpConnector : CConnector {
    CStreamBuffer m_kStreamBuffer;  // 0x004C
    char*         m_acRecvBuffer;   // 0x0058
    bool          m_bIsConnected;   // 0x005C
};

struct CUdpConnector : CConnector {
    int  m_iSymmetricPort;
    int  m_iHandshakeStep;
    bool m_bSendHole;
};

// ── Net Manager ───────────────────────────────────────────────────────────────

struct CNetMgr {
    void*          _vptr_CNetMgr;          // 0x0000
    char           pad[0x1014];            // 0x0004
    CTcpConnector* CFrontConnectorTcp;     // 0x1018
    CTcpConnector* CMainConnectorTcp;      // 0x101C
    CTcpConnector* CCastConnectorTcp;      // 0x1020
    CUdpConnector* CExConnectorUdp;        // 0x1024
    DWORD          dword1028;              // 0x1028
    DWORD          dword102C;
    DWORD          dword1030;
    DWORD          dword1034;
    DWORD          dword1038;
    u8             byte103C;
    u8             byte103D;
    u8             byte103E;
    u8             byte103F;
    DWORD          dword1040;
    DWORD          dword1044;
    DWORD          lastConnectTick;        // 0x1048
    DWORD          auth_key2;              // 0x104C
    DWORD          serverinfo_sid;         // 0x1050
    DWORD          dword1054;
    DWORD          server_time;            // 0x1058
    DWORD          auth_key;               // 0x105C
    DWORD          dword1060;
    DWORD          dword1064;
    u8             byte1068;
    u8             byte1069;
    u8             byte106A;
    u8             byte106B;
    DWORD          dword106C;
    DWORD          dword1070;
    DWORD          grade;                  // 0x1074
    char           loginUsername[68];       // 0x1078
    DWORD          dword10bc;
    char           loginPassword[40];      // 0x10C0
    DWORD          dword10e8;
    char           loginKey[500];          // 0x10EC
    DWORD          dword12E0;
    char           char12E4[508];
    DWORD          dword14E0;
    DWORD          dword14E4;
    DWORD          dword14E8;
    DWORD          dword14EC;
    DWORD          ChatChannelId;          // 0x14F0
    DWORD          dword14F4;
    u8             auth_token[32];         // 0x14F8
};

// ── UI ────────────────────────────────────────────────────────────────────────

struct CUIMsgBox;
struct CUIMsgBox_vtbl {
    void* sub_5C4AC0;//0x00
    void* sub_E608F0;//0x04
    void* sub_5C72A0;//0x08
    bool(__thiscall* CreateDialogByInfo)(CUIMsgBox* instance, int* info);//0x0C sub_E62230
	void(__thiscall* CreateDialog)(CUIMsgBox* instance, const char* msg, int msgData, u8 idk, u8 idk2, bool bUseMsgData);//0x10
	void* sub_E625A0;//0x14
    void* sub_5C7360;//0x18
    void* sub_E62B70;//0x1C
    void* sub_5C73B0;//0x20
    void* sub_E61480;//0x24
    void* sub_E62FE0;//0x28
	void(__thiscall* CreateBox)(CUIMsgBox* instance, const char* msg, void* msgData, u8 idk, u8 idk2, bool bUseMsgData);//0x2C
};

struct CUIMsgBox {
    CUIMsgBox_vtbl* _vptr_CUIMsgBox;
};

// ── IMsgData (base for message dialog data objects) ───────────────────────────
// Total size: 0x630 (1584 bytes) for CMsgDataLoginBanUser
// Layout: vftable + u32 + 3x wchar_t[260] + 3x u32

struct IMsgData {
    void* _vptr;         // 0x0000
    u32   field_04;      // 0x0004
    wchar_t msg1[260];   // 0x0008  (520 bytes)
    wchar_t msg2[260];   // 0x0210  (520 bytes)
    wchar_t msg3[260];   // 0x0418  (520 bytes)
    u32   field_620;     // 0x0620
    u32   field_624;     // 0x0624
    u32   field_628;     // 0x0628
    u32   field_62C;     // 0x062C
};

// IMsgData::Init function type
using tIMsgDataInit = void(__thiscall*)(void*, int, int, int, int,
                                         const wchar_t*, const wchar_t*, const wchar_t*);

// sub_E62E70: pushes CMsgCloseAll into CUIMsgBox message queue
using tCloseAllMessages = void(__thiscall*)(CUIMsgBox*);

struct CGUIManager;
struct CGUIManager_vtbl {

};
struct CUIMsgAdapter
{

};
struct CGUIManager {
    CGUIManager_vtbl* _vptr_CGUIManager;
	void* CFreeTypeManager; // 0x0004
	uint32_t dword0008; // 0x0008
	void* func000C; // 0x000C
	void* UI_State; // 0x0010
	void* UI_State2; // 0x0014
	uint32_t dword0018; // 0x0018
    CUIMsgAdapter* CUIMsgAdapter; // 0x001C
	void* CResManager; // 0x0020
	CUIMsgBox* UIMsgBox; // 0x0024
};

// ── Player / Unit system ──────────────────────────────────────────────────────

struct CExPlayerRoomInfo {
    UniqueId uniqueId;             // 0x0000
    DWORD    m_iCharacterInfo;     // 0x0004
    DWORD    m_iStatus;            // 0x0008
    DWORD    m_iHealth;            // 0x000C
    DWORD    m_iNotHealth;         // 0x0010
    DWORD    m_iHealthXor;         // 0x0014
    char     _pad[0xB70];          // 0x0018
    // Fields at 0x0B88+ are padding DWORDs — omitted for brevity
    // Access via offset reads if needed
};

struct CNetIO {};
struct CExBehaviour {};
struct CExEffectBehaviour {};
struct CExSkillBehaviour {};
struct CExRoomHandler {};
struct CExClanHandler {};
struct CExPartyHandler {};
struct CExEquipment {};
struct CExInventory {};
struct CExKnapsack {};
struct CExDioramaBox {};
struct CExWeaponBox {};
struct CExMessageBox {};
struct CExGiftBox {};
struct CExFriendBox {};
struct CExBlacklistBox {};
struct CExUnknown1 {};
struct CExUnknown2 {};
struct CExGachaponHandler {
	void* _vptr_CExGachaponHandler; // 0x0000
	char _pad[0x8]; // 0x0004
	u32 m_uiLuckyPoints; // 0x000C
};

struct CExPlayer {
    void*              _vptr_CExPlayer; // 0x0000
    CExPlayerRoomInfo* m_kRoomInfo;     // 0x0004
	DWORD dword0008;                    // 0x0008
	CNetIO* m_kNetIO;   		  // 0x000C
	CExBehaviour* m_kBehaviour; // 0x0010
	CExEffectBehaviour* m_kEffectBehaviour; // 0x0014
	CExSkillBehaviour* m_kSkillBehaviour; // 0x0018
    CExRoomHandler* m_kRoomHandler; // 0x001C
    CExClanHandler* m_kClanHandler; // 0x0020
    CExPartyHandler* m_kPartyHandler; // 0x0024
    CExEquipment* m_kEquipment; // 0x0028
    CExInventory* m_kInventory; // 0x002C
    CExKnapsack* m_kKnapsack; // 0x0030
    CExDioramaBox* m_kDioramaBox; // 0x0034
    CExWeaponBox* m_kWeaponBox; // 0x0038
    CExMessageBox* m_kMessageBox; // 0x003C
    CExGiftBox* m_kGiftBox; // 0x0040
    CExFriendBox* m_kFriendBox; // 0x0044
    CExBlacklistBox* m_kBlacklistBox; // 0x0048
    CExUnknown1* m_kUnknown1; // 0x004C
    CExUnknown2* m_kUnknown2; // 0x0050
    CExGachaponHandler* m_kGachaponHandler; // 0x0054
    struct CGamePlayer* m_kGamePlayer; // 0x0058
    uint32_t m_uiSpawnTick; // 0x005C
    uint32_t m_uiLastStateTick; // 0x0060
    char nickname[16]; // 0x0064
};
struct CExModTDM {};
struct CExModFFA {};
struct CExModITM {};
struct CExModCTF {};
struct CExModCTM {};
struct CExModSAB {};
struct CExModCIM {};
struct CExModZSM {};
struct CExModGRM {};
struct CExModMOC {};
struct CExModBMB {};
struct CExModPVE {};
struct CExModTUT {};
struct CExModClanCTF {};
struct CExModClanSAB {};
struct CExModClanTDM {};
struct CExModClanBMB {};
struct CExRoomUnknown1 {};
struct CExRoomUnknown2 {};
struct CExEnvEntityList {};
struct CExNpcEntityList {};
struct CExAmmoObjectList {};
struct CExModVariant {
};
struct CRoom
{
	int(__stdcall** _vptr_CRoom)(); // 0x0000
	CExModTDM* m_kModTDM; // 0x0004
	CExModFFA* m_kModFFA; // 0x0008
	CExModITM* m_kModITM; // 0x000C
	CExModCTF* m_kModCTF; // 0x0010
	CExModCTM* m_kModCTM; // 0x0014
	CExModSAB* m_kModSAB; // 0x0018
	CExModCIM* m_kModCIM; // 0x001C
	CExModZSM* m_kModZSM; // 0x0020
	CExModGRM* m_kModGRM; // 0x0024
	CExModMOC* m_kModMOC; // 0x0028
	CExModBMB* m_kModBMB; // 0x002C
	CExModPVE* m_kModPVE; // 0x0030
	CExModTUT* m_kModTUT; // 0x0034
	CExModClanCTF* m_kModClanCTF; // 0x0038
	CExModClanSAB* m_kModClanSAB; // 0x003C
	CExModClanTDM* m_kModClanTDM; // 0x0040
	CExModClanBMB* m_kModClanBMB; // 0x0044
	CExPlayer* m_kPlayer[26]; // 0x0048
	CExRoomUnknown1* m_kUnknown1; // 0x00B0
	CExRoomUnknown2* m_kUnknown2; // 0x00B4
    CExEnvEntityList* m_kEnvEntityList; // 0x00B8
    CExNpcEntityList* m_kNpcEntityList; // 0x00BC
    CExAmmoObjectList* m_kAmmoObjectList; // 0x00C0
    CExModVariant* m_kCurrentMod; // 0x00C4
	int32_t m_iPlayerCount; // 0x00C8
	int32_t m_iObserverCount; // 0x00CC
	char _pad00D0[0x10]; // 0x00D0
	int32_t m_iMaxPlayers; // 0x00E0
	uint8_t m_bRoomSettingIdk1; // 0x00E4
	uint8_t m_bRoomSettingIdk2; // 0x00E5
	uint8_t m_bIntrudersCanJoin; // 0x00E6
	uint8_t m_bObserversAllowed; // 0x00E7
	int32_t m_iTeamBalance; // 0x00E8
	int32_t m_iWeaponRestriction; // 0x00EC
	char m_szRoomTitle[30]; // 0x00F0
	char _pad010E[0xA]; //0x010E
	char m_szRoomPassword[8]; // 0x0118
};


struct CGamePlayer {
    void*      _vptr_CGamePlayer;   // 0x0000
    char       pad[0x108];          // 0x0004
    CExPlayer* m_pExPlayer;         // 0x010C
};

struct CUnitContainer {
    char         pad[0x3C];           // 0x0000
    CGamePlayer* m_pGamePlayer[13];   // 0x003C
    DWORD        dword0070;           // 0x0070
};

// ── CCrypt (encryption engine) ────────────────────────────────────────────────

struct CCrypt;
using PVF_SETUP     = void(__thiscall*)(CCrypt*);
using PBF_ENCRYPT   = void(__thiscall*)(CCrypt*, void*, void*, int);
using PBF_DECRYPT   = void(__thiscall*)(CCrypt*, void*, void*, int);

struct CCrypt {
    void*       _vptr_CCrypt;
    PVF_SETUP   m_pvfSetup;
    PBF_ENCRYPT m_pbfEncrypt;
    PBF_DECRYPT m_pbfDecrypt;
    u16         m_usKey_RC5[26];
    u32         m_uiKey_RC5[26];
    u32         m_uiKey_RC6[84];
    i32         m_iSerialKey;
};

// ── NT function typedefs ──────────────────────────────────────────────────────

using NtProtectVirtualMemory_t = NTSTATUS(NTAPI*)(
    HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize,
    ULONG NewProtect, PULONG OldProtect);

using NtQueryVirtualMemory_t = NTSTATUS(NTAPI*)(
    HANDLE ProcessHandle, PVOID BaseAddress, INT MemoryInformationClass,
    PVOID MemoryInformation, SIZE_T MemoryInformationLength, PSIZE_T ReturnLength);

} // namespace mg::game
