// =============================================================================
// Network engine swap — Connect path, recv thread, CNetMgr wiring  [1:1]
// =============================================================================
// The glue tier of the engine reimplementation, reversed from MicroVolts.exe.i64:
//   CTcpConnector::Connect       0x00E13340  (unregister/connect/register socket)
//   CNetEngine::CreateConnector  0x00E125F0  (-> model vtbl[14]/[15])
//   CNetEngine::RegisterConnector 0x00E12560 (-> model vtbl[13] SetMainConnector)
//   CWaitProcedure::Run          0x00AB8950  (recv thread loop)
//   CNetMgr::NetFrameTick        0x00AB40A0  (per-frame send + drain pump) [mirror]
//
// Threading model (corrected): RECV for netEngine1 (main+cast) runs on a Gamebryo
// NiThread named "NetengineWait" whose CWaitProcedure::Run loops
//   while (engine) { engine->RecvPump(10ms); Sleep(50ms); }
// driving the WSAEventSelect wait loop (CEventSelectModel::Select). SEND/keepalive
// and the command drain run per-frame from NetFrameTick. The front/auth engine
// (netEngine2) has no thread — it is pumped entirely per-frame.
//
// Install hooks are written but left COMMENTED OUT (see InstallNetWiring).
// =============================================================================
#include "pch.hpp"
#include "game/network/engine/net_wiring.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "utils/call_helper.hpp"

namespace mg::game::net {
namespace {

// ── ISensor (net model) vtable slots used here ────────────────────────────────
inline char hModelRegisterSocket(void* model, int relay, SOCKET s) {
    auto** vt = *reinterpret_cast<void***>(model);
    return reinterpret_cast<char(__thiscall*)(void*, int, SOCKET)>(vt[8])(model, relay, s);
}
inline void hModelUnregisterSocket(void* model, int relay, SOCKET s) {
    auto** vt = *reinterpret_cast<void***>(model);
    reinterpret_cast<void(__thiscall*)(void*, int, SOCKET)>(vt[9])(model, relay, s);
}

// conn vtbl[14] (+0x38) — subclass Disconnect(socketHandle, flag).
inline void hConnDisconnect(CConnector* c, int sockHandle, int flag) {
    auto** vt = *reinterpret_cast<void***>(c);
    reinterpret_cast<void(__thiscall*)(CConnector*, int, int)>(vt[14])(c, sockHandle, flag);
}

// ── CTcpConnector::Connect  (0x00E13340) ──────────────────────────────────────
// If already registered, pull the old socket from the model first; connect the
// raw socket; on success re-arm (SetConnected) and re-register with the model so
// the recv wait loop watches it. On failure, mark the socket down + Disconnect.
char __fastcall hkConnect(CTcpConnector* self, u32 /*edx*/, const char* addr_, int port) {
    if (port <= 0)
        return 0;

    EnterCriticalSection(&self->m_kLocker.m_tMutex);

    if (self->m_bRegister) {
        hModelUnregisterSocket(self->m_pkSensor, self->m_iRelay, self->m_pkSocket->m_skSocket);
        self->m_bRegister = false;
    }

    auto vConnect = reinterpret_cast<int(__thiscall*)(CTcpSocket*, const char*, int)>(
        self->m_pkSocket->_vptr_CRawSocket->Connect);
    if (!vConnect(self->m_pkSocket, addr_, port)) {
        LeaveCriticalSection(&self->m_kLocker.m_tMutex);
        return 0;
    }

    self->m_pkSocket->m_iNetworkState = 0;
    self->_vptr_CConnector->SetConnected(self);
    reinterpret_cast<u8&>(self->m_pkSocket->dword0070) = 1;       // link ready to enqueue

    const char ok = hModelRegisterSocket(self->m_pkSensor, self->m_iRelay, self->m_pkSocket->m_skSocket);
    if (ok) {
        self->m_bRegister = true;
        LeaveCriticalSection(&self->m_kLocker.m_tMutex);
        return 1;
    }

    LeaveCriticalSection(&self->m_kLocker.m_tMutex);
    CTcpSocket* sock = self->m_pkSocket;
    if (!sock->m_iNetworkState) sock->m_iNetworkState = 8;
    const SOCKET h = sock->m_skSocket;
    if (!sock->m_iNetworkState) sock->m_iNetworkState = 1;        // (orig double-check; moot)
    hConnDisconnect(self, static_cast<int>(h), 0);
    return 0;
}

// ── CNetEngine::RegisterConnector  (sub_E12560) ───────────────────────────────
// Bind an externally-constructed connector as a dispatcher's main connector
// (model vtbl[13] SetMainConnector). Used for the Main/Cast connectors.
char __fastcall hkRegisterConnector(CNetEngine* self, u32 /*edx*/, CConnector* conn) {
    if (!self->m_bInitialized) {
        // [orig: diag log] "don't initialized on engine"
        return 0;
    }
    auto** vt = *reinterpret_cast<void***>(self->m_pkSensor);
    return reinterpret_cast<char(__thiscall*)(void*, CConnector*)>(vt[13])(self->m_pkSensor, conn);
}

// ── CNetEngine::CreateConnector  (sub_E125F0) ─────────────────────────────────
// Factory-create + register a connector of the given type (model vtbl[14] for
// type>=2, else vtbl[15]). Used for the Front/auth connector. NB: the original
// calls these sensor slots __stdcall (no `this`) — preserved verbatim.
int __fastcall hkCreateConnector(CNetEngine* self, u32 /*edx*/, const char* addr_, int port, int type) {
    if (!self->m_bInitialized) {
        // [orig: diag log] "don't initialized on engine"
        return 0;
    }
    auto** vt = *reinterpret_cast<void***>(self->m_pkSensor);
    const int slot = (type >= 2) ? 14 : 15;
    return reinterpret_cast<int(__stdcall*)(const char*, int, int)>(vt[slot])(addr_, port, type);
}

// ── CWaitProcedure (NetengineWait recv thread) ────────────────────────────────
// Layout: +0 vftable, +4 CNetEngine* (netEngine1), +8 timeout (ms).
struct CWaitProcedure {
    void*       _vptr;     // +0
    CNetEngine* m_pkEngine;// +4
    u32         m_uTimeout;// +8
};

// CWaitProcedure::Run  (0x00AB8950) — the recv thread body. Drives the engine's
// recv pump (-> model Select(timeout)) until the engine pointer is cleared, with
// a 50ms cadence between polls.
int __fastcall hkWaitProcRun(CWaitProcedure* self, u32 /*edx*/) {
    while (self->m_pkEngine) {
        auto** vt = *reinterpret_cast<void***>(self->m_pkEngine);
        reinterpret_cast<void(__thiscall*)(CNetEngine*, DWORD)>(vt[4])(
            self->m_pkEngine, self->m_uTimeout);                  // RecvPump(timeout)
        Sleep(addr::net::netmgr::WaitProcSleep);                  // 50ms
    }
    return 0;
}

// ── CNetMgr field accessors (offsets verified from sub_AB3450 disasm) ─────────
inline CNetEngine* hEngine(void* netMgr, u32 off) {
    return reinterpret_cast<CNetEngine*>(reinterpret_cast<char*>(netMgr) + off);
}
inline u8& hByte(void* netMgr, u32 off) {
    return *(reinterpret_cast<u8*>(netMgr) + off);
}
inline CTcpConnector*& hConnSlot(void* netMgr, u32 off) {
    return *reinterpret_cast<CTcpConnector**>(reinterpret_cast<char*>(netMgr) + off);
}

// ── CNetMgr::NetFrameTick pump mirror  (subset of sub_AB40A0) ──────────────────
// The net-relevant slice of the per-frame tick: send/keepalive pump, optional
// recv poll (standalone mode), drain+route. The scene/room updates the original
// interleaves (sub_AB43C0 / sub_AB41A0 / sub_AB5220) are game logic and left to
// the client. `engine` here is CNetMgr+4044 (netEngine1).
void hkNetFrameTickPump(CNetEngine* engine, bool recvGate) {
    auto** vt = *reinterpret_cast<void***>(engine);
    auto sendPump = [&] { reinterpret_cast<int(__thiscall*)(CNetEngine*)>(vt[6])(engine); };
    auto recvPump = [&] { reinterpret_cast<int(__thiscall*)(CNetEngine*, DWORD)>(vt[4])(engine, 0); };

    sendPump();                       // engine vtbl[6] (twice across the frame)
    if (recvGate)                     // CNetMgr+4201: standalone (no NetengineWait thread)
        recvPump();

    // drain + route: cmdQueue.Get -> PacketsCallbackDistribution -> free
    if (ICommandQueue* q = engine->m_pkCommandQueue) {
        auto** qvt = *reinterpret_cast<void***>(q);
        const bool empty = reinterpret_cast<int(__thiscall*)(ICommandQueue*)>(qvt[2])(q) != 0;
        if (!empty) {
            while (auto* cmd = reinterpret_cast<CExtendCommand*(__thiscall*)(ICommandQueue*)>(qvt[4])(q)) {
                mg::call<char(__stdcall*)(void*)>(
                    addr::net::netmgr::PacketsCallbackDistribution, &cmd->m_kPacket);
                auto** cvt = *reinterpret_cast<void***>(cmd);
                reinterpret_cast<void(__thiscall*)(void*, int)>(cvt[0])(cmd, 1);
            }
        }
    }

    sendPump();                       // trailing send pump (orig calls vtbl[6] again)
}

// ── Front engine (netEngine2) per-frame pump  (sub_AB3EF0) ─────────────────────
// The front/auth link has no NetengineWait thread, so recv is pumped here too.
// Gated on byte103D; order is send -> recv -> drain (no trailing send).
void hkFrontEnginePump(void* netMgr) {
    if (!*(reinterpret_cast<u8*>(netMgr) + addr::net::netmgr::FrontPumpGate))
        return;
    CNetEngine* engine = hEngine(netMgr, addr::net::netmgr::Engine2Offset);
    auto** vt = *reinterpret_cast<void***>(engine);
    reinterpret_cast<int(__thiscall*)(CNetEngine*)>(vt[6])(engine);          // SendPump
    reinterpret_cast<int(__thiscall*)(CNetEngine*, DWORD)>(vt[4])(engine, 0);// RecvPump

    if (ICommandQueue* q = engine->m_pkCommandQueue) {
        auto** qvt = *reinterpret_cast<void***>(q);
        if (!(reinterpret_cast<int(__thiscall*)(ICommandQueue*)>(qvt[2])(q) != 0)) {
            while (auto* cmd = reinterpret_cast<CExtendCommand*(__thiscall*)(ICommandQueue*)>(qvt[4])(q)) {
                mg::call<char(__stdcall*)(void*)>(
                    addr::net::netmgr::PacketsCallbackDistribution, &cmd->m_kPacket);
                auto** cvt = *reinterpret_cast<void***>(cmd);
                reinterpret_cast<void(__thiscall*)(void*, int)>(cvt[0])(cmd, 1);
            }
        }
    }
}

// ── CNetMgr::InitializeEngines mirror  (sub_AB3140) ───────────────────────────
// Initializes BOTH embedded engines (vtbl[1] Initialize). Order matters: auth/front
// (netEngine2) first, then main/cast (netEngine1). On success sets byte103C — the
// wire gate that CreateConnectors checks. Args verified from the disasm:
//   auth: (sensor=1 EventSelect, cmd=0 Native,  size=1  dispatcher, net=2)
//   main: (sensor=1 EventSelect, cmd=1 Extend,  size=53 dispatchers, net=1)
char hkNetMgrInitializeEngines(void* netMgr) {
    namespace nm = addr::net::netmgr;
    mg::call<void(__cdecl*)()>(nm::InitPreStep);            // unknown_libname_349 (pre-init)

    auto initEngine = [](CNetEngine* e, int sensor, int cmd, int size, int net, int a6) -> bool {
        auto** vt = *reinterpret_cast<void***>(e);
        return reinterpret_cast<char(__thiscall*)(CNetEngine*, int, int, int, int, int)>(
                   vt[1])(e, sensor, cmd, size, net, a6) != 0;
    };

    CNetEngine* eng2 = hEngine(netMgr, nm::Engine2Offset);  // auth/front
    if (!initEngine(eng2, nm::AuthInitSensor, nm::AuthInitCmd, nm::AuthInitSize, nm::AuthInitNet, nm::AuthInitA6)) {
        // [orig: diag log] "Auth Engine Initialize Failed" (NetMgr.cpp:100)
        return 0;
    }
    CNetEngine* eng1 = hEngine(netMgr, nm::Engine1Offset);  // main/cast
    if (!initEngine(eng1, nm::MainInitSensor, nm::MainInitCmd, nm::MainInitSize, nm::MainInitNet, nm::MainInitA6)) {
        // [orig: diag log] "Net Engine Initialize Failed" (NetMgr.cpp:97)
        return 0;
    }
    hByte(netMgr, nm::WireGateByte) = 1;
    return 1;
}

// ── CNetMgr::CreateConnectors mirror  (sub_AB3450) ────────────────────────────
// Builds Front/Main/Cast connectors across the two embedded engines and spawns
// the NetengineWait recv thread. Engines are assumed already Initialize()'d (the
// Register/Create calls early-out if not). Offsets verified from the disasm.
char hkNetMgrCreateConnectors(void* netMgr) {
    namespace nm = addr::net::netmgr;
    if (!hByte(netMgr, nm::WireGateByte))
        return 0;

    // Per-frame tick fn ptr (skipped in true standalone/dedicated builds).
    if (!*reinterpret_cast<u8*>(0x11C8C81 /*bRunStandAlone*/))
        *reinterpret_cast<uptr*>(reinterpret_cast<char*>(netMgr) + nm::TickFnPtr) = nm::NetFrameTick;

    if (*reinterpret_cast<void**>(reinterpret_cast<char*>(netMgr) + nm::UdpConnOff))
        return 1;                                   // already wired

    // probe + log the local address (sockaddr at +0x1030)
    mg::call<void(__cdecl*)(void*)>(nm::LocalAddrProbe,
                                    reinterpret_cast<char*>(netMgr) + nm::LocalAddrOff);

    // ── Front / auth connector (netEngine2, factory type 0) ──
    CNetEngine* eng2 = hEngine(netMgr, nm::Engine2Offset);
    CTcpConnector* front = reinterpret_cast<CTcpConnector*>(
        mg::call<int(__thiscall*)(CNetEngine*, const char*, int, int)>(
            addr::net::engine::CreateConnector, eng2, "0.0.0.0", 0, 0));
    hConnSlot(netMgr, nm::FrontConnOff) = front;
    if (front) {
        const int basePort = *reinterpret_cast<int*>(nm::FrontBasePort);
        for (int i = 0; i < 10; ++i) {
            if (mg::call<int(__thiscall*)(void*, const char*, int)>(
                    nm::RawSocketBind, front->m_pkSocket, "0.0.0.0", i + basePort) != -1)
                break;
        }
    }
    if (!front || !ntohs(front->m_pkSocket->m_saLocalAddr_in.sin_port)) {
        // [orig: diag log] "Create Failed: Auth"
        return 0;
    }

    CNetEngine* eng1 = hEngine(netMgr, nm::Engine1Offset);

    // ── Main connector (CMainConnectorTcp, registered on netEngine1) ──
    auto* mainRaw = mg::call<void*(__cdecl*)(u32)>(addr::msvc::operator_new, nm::ConnectorAlloc);
    CTcpConnector* mainConn = mainRaw
        ? mg::call<CTcpConnector*(__thiscall*)(void*)>(nm::MainCtor, mainRaw) : nullptr;
    hConnSlot(netMgr, nm::MainConnOff) = mainConn;
    reinterpret_cast<int(__thiscall*)(CTcpSocket*)>(mainConn->m_pkSocket->_vptr_CRawSocket->Create)(mainConn->m_pkSocket);
    if (!mg::call<char(__thiscall*)(CNetEngine*, CConnector*)>(
            addr::net::engine::RegisterConnector, eng1, mainConn)) {
        // [orig: diag log] "Create Failed: Main"
        return 0;
    }
    mainConn->m_bHeaderCrypt = true;

    // ── Cast connector (CCastConnectorTcp, registered on netEngine1) ──
    auto* castRaw = mg::call<void*(__cdecl*)(u32)>(addr::msvc::operator_new, nm::ConnectorAlloc);
    CTcpConnector* castConn = castRaw
        ? mg::call<CTcpConnector*(__thiscall*)(void*)>(nm::CastCtor, castRaw) : nullptr;
    hConnSlot(netMgr, nm::CastConnOff) = castConn;
    reinterpret_cast<int(__thiscall*)(CTcpSocket*)>(castConn->m_pkSocket->_vptr_CRawSocket->Create)(castConn->m_pkSocket);
    if (!mg::call<char(__thiscall*)(CNetEngine*, CConnector*)>(
            addr::net::engine::RegisterConnector, eng1, castConn)) {
        // [orig: diag log] "Create Failed: Cast"
        return 0;
    }

    // ── NetengineWait recv thread for netEngine1 (threaded mode only) ──
    if (!hByte(netMgr, nm::RecvGateByte)) {           // byte1069 == 0
        auto* procRaw = mg::call<void*(__cdecl*)(u32, int)>(nm::NiMemObjectNew, 12u, 0);
        CWaitProcedure* proc = procRaw
            ? reinterpret_cast<CWaitProcedure*>(
                  mg::call<void*(__thiscall*)(void*, void*, int)>(
                      nm::WaitProcCtor, procRaw, eng1, nm::WaitProcTimeout))
            : nullptr;
        // store proc at CNetMgr+0x1010 (orig), create the NiThread, name it, prioritize.
        *reinterpret_cast<void**>(reinterpret_cast<char*>(netMgr) + 0x1010) = proc;
        if (proc) {
            void* thread = mg::call<void*(__cdecl*)(void*, u32)>(nm::NiThreadCreate, proc, 0xFFFFFFFFu);
            *reinterpret_cast<void**>(reinterpret_cast<char*>(netMgr) + 0x1014) = thread;
            if (thread) {
                mg::call<void(__thiscall*)(void*, const char*)>(nm::NiThreadSetName, thread, "NetengineWait");
                mg::call<char(__thiscall*)(void*, int)>(nm::NiThreadSetPriority, thread, 6);
                // [orig also pins SystemSetAffinity — tuning, omitted here]
            }
            // else [orig: diag log] "wait thread create failed"
        }
        // else [orig: diag log] "wait procedure new failed"
    }
    return 1;
}

} // namespace

// ── Detour install (inert — every .create is commented out) ───────────────────
void InstallNetWiring() {
    // auto& reg = mg::ctx().hookRegistry();
    // reg.registerDetour(HookId::Connector_Connect   ).create(MG_CONST(addr::net::connector::Connect),         hkConnect);
    // reg.registerDetour(HookId::Engine_RegConnector  ).create(MG_CONST(addr::net::engine::RegisterConnector),  hkRegisterConnector);
    // reg.registerDetour(HookId::Engine_CreateConnector).create(MG_CONST(addr::net::engine::CreateConnector),   hkCreateConnector);
    // reg.registerDetour(HookId::WaitProc_Run         ).create(MG_CONST(addr::net::netmgr::WaitProcRun),        hkWaitProcRun);
    // reg.registerDetour(HookId::NetMgr_InitEngines   ).create(MG_CONST(addr::net::netmgr::InitEngines),        hkNetMgrInitializeEngines);
    // reg.registerDetour(HookId::NetMgr_CreateConns   ).create(MG_CONST(addr::net::netmgr::Wiring),             hkNetMgrCreateConnectors);
    //
    // Full bring-up order (all reimplemented here): CNetMgr ctor (AB2E60) constructs
    // both engines; CNetMgr::InitializeEngines (AB3140, called from Game::Init
    // 0x506DF0) -> hkNetMgrInitializeEngines; then per-login CreateConnectors
    // (AB3450) -> hkNetMgrCreateConnectors builds connectors + the NetengineWait
    // thread. The SystemSetAffinity pin is omitted as tuning. This whole tier turns
    // live only once the vtable swap is enabled.
    (void)hkConnect; (void)hkRegisterConnector; (void)hkCreateConnector;
    (void)hkWaitProcRun; (void)hkNetFrameTickPump; (void)hkFrontEnginePump;
    (void)hkNetMgrInitializeEngines; (void)hkNetMgrCreateConnectors;
}

} // namespace mg::game::net
