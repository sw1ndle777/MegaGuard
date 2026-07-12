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

struct CTcpPacket {
    STcpPacketHeader m_tHeader;
    CCommand         m_kCommand;
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

// ── Command queue (ICommandQueue / CExCommandQueue / CCommandQueue) ────────────
// libnetengine/src/ExCommandQueue.cpp. The embedded ring at +28 is an MSVC
// std::deque<Cmd*>; only the fields the engine reads are named — the actual ring
// push/subscript is done by leaf helpers (addr::net::helpers::Deque*).

struct ICommandQueue_vtbl {
    void* Dtor;        // [0]  scalar deleting dtor
    void* sub1;        // [1]
    void* IsEmpty;     // [2]  used by drain: !IsEmpty() -> drain
    void* NonEmpty;    // [3]  C4D660 -> count (this+56)
    void* Get;         // [4]  E119B0 -> Cmd* (pop front) / 0
    void* Put;         // [5]  E11D30 (connector, sid2, body, size, cryptType)
    void* PutEx;       // [6]  E11FB0 (filtered by regIdx & sid2)
    void* Clear;       // [7]
};

// The ring at +0x1C is a stock MSVC std::deque<Cmd*> (_Container_base_secure +
// _Map/_Mapsize/_Myoff/_Mysize). The deque base passed to the leaf helpers is
// (queue + 0x1C); element block size is 4. Push/subscript are done by the helpers.
struct ICommandQueue {
    ICommandQueue_vtbl* _vptr;        // +0x00
    CLocker             m_kLocker;    // +0x04  CRITICAL_SECTION (24B) -> +0x04..+0x1C
    void*               m_pMyproxy;   // +0x1C  (this[7])  std::deque _Myproxy
    u32                 dword20;      // +0x20  (this[8])
    u32                 dword24;      // +0x24  (this[9])
    u32                 dword28;      // +0x28  (this[10])
    void*               m_pMap;       // +0x2C  (this[11]) _Map (Cmd**)
    u32                 m_uMapSize;   // +0x30  (this[12]) _Mapsize (blocks)
    u32                 m_uFrontIdx;  // +0x34  (this[13]) _Myoff
    u32                 m_uCount;     // +0x38  (this[14]) _Mysize == NonEmpty() return
};
constexpr u32 kCmdDequeBase = 0x1C; // (queue + 0x1C) = std::deque base for leaf helpers

#pragma pack(push, 1)
// CExtendCommand (0x5B8). m_kPacket is the decrypted body; PacketsCallbackDistribution
// reads (cmd + 8). The trailer fields at +1440 are stamped by CExCommandQueue::Put.
struct CExtendCommand {
    void*      _vptr;          // +0x000
    void*      m_NextOfPool;   // +0x004
    CTcpPacket m_kPacket;      // +0x008  (1432B) -> ends at +1440
    void*      m_pkConnector;  // +1440
    u32        m_iSessionId2;  // +1444
    u32        m_iRegisterIdx; // +1448
    u32        m_iSize;        // +1452
    u64        m_ullTick;      // +1456 -> ends +1464 (0x5B8)
};
// CBasicCommand (0x5A0) = header + packet only.
struct CBasicCommand {
    void*      _vptr;          // +0x000
    void*      m_NextOfPool;   // +0x004
    CTcpPacket m_kPacket;      // +0x008  -> ends +1440 (0x5A0)
};
#pragma pack(pop)

// ── CDispatcher / CDispatcherArray (libnetengine Dispatcher.cpp) ───────────────

struct CDispatcher_vtbl {
    void* Dtor;             // [0] E1C120
    void* Init;             // [1] E1C100 (model, cmdQueue)
    void* Cleanup;          // [2] E1D4F0
    void* SetState;         // [3] E1D5D0 (int)
    void* OnRead;           // [4] E1D320 -> mainConnector->Read (conn vtbl[11])
    void* Process;          // [5] E1D660 -> socket->SendQueue + connector->KeepAlive
    void* SetMainConnector; // [6] E1D890 (int type)
    void* SetSubConnectors; // [7] E1D9E0 (int count)
};

// NB: Init arg order is (cmdQueue, model) — verified via CConnector::Init(o,p) and
// the Init chain — so +0x04 is the queue and +0x08 is the model (not the reverse).
struct CDispatcher {
    CDispatcher_vtbl* _vptr;          // +0x00
    ICommandQueue*    m_pkCommandQueue;// +0x04  (this[1])
    void*             m_pkModel;      // +0x08  (this[2])
    CConnector*       m_pkMainConn;   // +0x0C  (this[3])
    int               m_iIndex;       // +0x10  (this[4])
    CConnector**      m_pkSubConns;   // +0x14  (this[5])  new[]
    u32               m_uSubCount;    // +0x18  (this[6])
};

struct CDispatcherArray_vtbl {
    void* Dtor;           // [0] E1CC60
    void* Init;           // [1] E1C190 (model, cmdQueue, count<=64)
    void* sub2;           // [2]
    void* sub3;           // [3]
    void* sub4;           // [4]
    void* Run;            // [5] E1C760 (idx 0 = run all -> CDispatcher::Process)
    void* SetMainConnAll; // [6] E1C5A0
    void* SetSubConnAll;  // [7] E1C680
    void* sub8;           // [8]
    void* GetDispatcher;  // [9] (+36) used by model Select
};

struct CDispatcherArray {
    CDispatcherArray_vtbl* _vptr;          // +0x00
    ICommandQueue*         m_pkCommandQueue;// +0x04 (Init stores the queue here; model
                                           //        flows to each dispatcher, not stored)
    u32                    m_uCount;       // +0x08
    CDispatcher**          m_pkList;       // +0x0C  new[]
    char                   m_aArray[0x190];// +0x10
};

// ── CEventSelectModel (ISensor, type1/default net model) ───────────────────────
// 0x318. Socket slots are (SOCKET, connectorId) pairs at +0x18 (8B stride, memset
// 0xFF). Parallel HANDLE event[] at +0x218. The wait loop is Select (vtbl[4]).

struct CEventSelectModel_vtbl {
    void* Dtor;             // [0]  E18770
    void* Init;             // [1]  E17C40 (count, cmdQueue)
    void* sub2;             // [2]  E17E10
    void* Reset;            // [3]  E17E40 (WSACloseEvent all)
    void* Select;           // [4]  E17EC0 (DWORD timeout) — WSAWaitForMultipleEvents
    void* Run;              // [5]  E17C30 -> CDispatcherArray::Run
    void* SetNetType;       // [6]  E1B8F0
    void* sub7;             // [7]  E1B910
    void* RegisterSocket;   // [8]  E183E0 (connectorId, SOCKET)
    void* UnregisterSocket; // [9]  E18580
    void* sub10;            // [10] E122C0 (stub, returns -1)
    void* sub11;            // [11] E1B610
    void* GetDispatcherBy;  // [12] E1B630
    void* SetMainConnector; // [13] E1B4E0
    void* RegisterConn23;   // [14] E1B9A0 (udp/server types)
    void* RegisterConn01;   // [15] E1BD60 (tcp front/main)
};

#pragma pack(push, 1)
struct CEventSelectModelSlot { // (SOCKET, connectorId) pair, +0x18
    SOCKET m_skSocket;     // +0
    int    m_iConnectorId; // +4
};
struct CEventSelectModel {
    CEventSelectModel_vtbl* _vptr;        // +0x000
    CDispatcherArray*       m_pkArray;    // +0x004
    u32                     m_uCount;     // +0x008  registered socket/event count
    u8                      m_bInited;    // +0x00C
    u8                      _pad00D[0xB]; // +0x00D
    CEventSelectModelSlot   m_aSlots[64]; // +0x018  (512B)
    HANDLE                  m_aEvents[64];// +0x218  (256B) -> ends +0x318
};
#pragma pack(pop)

// ── CNetEngine (libnetengine NetEngine.cpp) ────────────────────────────────────

struct CNetEngine_vtbl {
    void* Dtor;       // [0] E12730
    void* Initialize; // [1] E12750
    void* Release;    // [2] E12330
    void* Clear;      // [3] E12390
    void* RecvPump;   // [4] E12310 -> model->Select(0)
    void* GetCommand; // [5] E12300 -> cmdQueue->Get
    void* SendPump;   // [6] E12320 -> model->Run -> array Run -> Process
};

// 0x1C total. The internal-thread slots (+0x10..+0x18) are the libnetengine
// CWaitEventThread / CSenderThread fields — left 0 in this client, which drives
// recv from an EXTERNAL NiThread ("NetengineWait", a CWaitProcedure) instead.
struct CNetEngine {
    CNetEngine_vtbl* _vptr;               // +0x00
    ICommandQueue*   m_pkCommandQueue;    // +0x04
    void*            m_pkSensor;          // +0x08  the net model
    u8               m_bInitialized;      // +0x0C
    u8               _pad0D[3];           // +0x0D
    void*            m_pkWaitEventThread; // +0x10  (unused, 0)
    void*            m_ppkSenderThread;   // +0x14  (unused, 0)
    u32              m_uiMaxSenderThread; // +0x18  (unused, 0)
};                                        // +0x1C

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
    char     _pad[0xB7c];          // 0x0018
	uint8_t m_uiMeleeKills;        // 0x0B94
	uint8_t m_uiRifleKills;        // 0x0B95
	uint8_t m_uiShotgunKills;      // 0x0B96
	uint8_t m_uiSniperKills;       // 0x0B97
    uint8_t m_uiGatlingKills;      // 0x0B98
	uint8_t m_uiBazookaKills;      // 0x0B99
	uint8_t m_uiGrenadeKills;      // 0x0B9A
	uint8_t m_uiTempKillstreak;    // 0x0B9B
	uint8_t m_uiTotalKills; 	   // 0x0B9C
	uint8_t m_uiDeathCount;        // 0x0B9D
	uint8_t m_uiHeadshots;         // 0x0B9E
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
