// =============================================================================
// Network engine swap — CDispatcher / CDispatcherArray  [see .hpp]
// =============================================================================
// Reversed 1:1 from MicroVolts.exe.i64:
//   CDispatcher::Init             0x00E1C100   (model, cmdQueue)
//   CDispatcher::SetState         0x00E1D5D0   (mark socket netstate + Disconnect)
//   CDispatcher::OnRead           0x00E1D320   (-> mainConnector->Read)
//   CDispatcher::Process          0x00E1D660   (-> socket->SendQueue + connector->KeepAlive)
//   CDispatcher::SetMainConnector 0x00E1D890   (via g_kConnectorFactory)
//   CDispatcher::SetSubConnectors 0x00E1D9E0   (new[] via g_kConnectorFactory)
//   CDispatcherArray::Init        0x00E1C190   (create N CDispatcher)
//   CDispatcherArray::Run         0x00E1C760   (run all dispatchers' Process)
//   CDispatcherArray::SetMainConnAll 0x00E1C5A0
//   CDispatcherArray::SetSubConnAll  0x00E1C680
//
// Vtable slots used through the connector / socket / dispatcher (verified):
//   socket  vtbl[7]  (+0x1C) = SendQueue   (CRawSocket)
//   conn    vtbl[5]  (+0x14) = KeepAlive
//   conn    vtbl[11] (+0x2C) = Read
//   conn    vtbl[14] (+0x38) = Disconnect(socketHandle, flag)  [subclass slot]
//   conn    vtbl[1]  (+0x04) = Init(model, cmdQueue)
//   socket->m_iNetworkState lives at socket+0x7C.
//
// Connector setup links (SetMainConnector / SetSubConnectors), verified offsets:
//   conn+0x28 (m_pkDispatcher) = dispatcher ; conn+0x40 (m_iRelay) = index ;
//   conn+0x34/0x36 (m_iSessionId/2) = (u16)index.
//
// The per-dispatcher object pools, connector factory (sub_DFA720) and the
// std::deque/new[] book-keeping are called into the client, matching the
// leaf-helper convention used across the engine reimplementation.
// =============================================================================
#include "pch.hpp"
#include "game/network/engine/dispatcher.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "utils/call_helper.hpp"

namespace mg::game::net {
namespace {

namespace dsp = addr::net::dispatcher;

// ── Vtable-slot wrappers ──────────────────────────────────────────────────────
inline char hSocketSendQueue(CTcpSocket* s) {
    return reinterpret_cast<char(__thiscall*)(CTcpSocket*)>(s->_vptr_CRawSocket->SendQueue)(s);
}
inline bool hConnKeepAlive(CConnector* c) {
    return c->_vptr_CConnector->KeepAlive(c);
}
inline char hConnRead(CConnector* c) {
    return c->_vptr_CConnector->Read(c);
}
// conn vtbl[14] (+0x38) — subclass Disconnect(socketHandle, flag).
inline void hConnDisconnect(CConnector* c, int sockHandle, int flag) {
    auto** vt = *reinterpret_cast<void***>(c);
    reinterpret_cast<void(__thiscall*)(CConnector*, int, int)>(vt[14])(c, sockHandle, flag);
}
// conn vtbl[1] (+0x04) — CConnector::Init(cmdQueue, model). Verified arg order.
inline void hConnInit(CConnector* c, void* cmdQueue, void* model) {
    auto** vt = *reinterpret_cast<void***>(c);
    reinterpret_cast<void(__thiscall*)(CConnector*, void*, void*)>(vt[1])(c, cmdQueue, model);
}
// g_kConnectorFactory.Create(type) — sub_51D900 -> factory, sub_DFA720(factory,type).
inline CConnector* hFactoryCreate(int type) {
    void* f = mg::call<void*(__cdecl*)()>(addr::net::factory::Get);
    return mg::call<CConnector*(__cdecl*)(void*, int)>(addr::net::factory::Create, f, type);
}
inline bool hFactoryRegistered() {
    void* f = mg::call<void*(__cdecl*)()>(addr::net::factory::Get);
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(f) + addr::net::factory::Registered) != nullptr;
}
// scalar deleting dtor: vtbl[0](obj, 1).
inline void hDeleteVirtual(void* obj) {
    if (!obj) return;
    auto** vt = *reinterpret_cast<void***>(obj);
    reinterpret_cast<void(__thiscall*)(void*)>(vt[2])(obj);     // [orig calls dtor (vtbl+8) first]
    reinterpret_cast<void(__thiscall*)(void*, int)>(vt[0])(obj, 1);
}

// ── Mark a connector down: socket netstate + subclass Disconnect ──────────────
// Mirrors the inlined fail-path the original uses for sub-connectors (and the
// body of CDispatcher::SetState for the main connector).
inline void markDown(CConnector* c, int state) {
    CTcpSocket* s = c->m_pkSocket;
    if (!s->m_iNetworkState) s->m_iNetworkState = static_cast<DWORD>(state);
    hConnDisconnect(c, static_cast<int>(s->m_skSocket), 1);
}

// ── CDispatcher::Init  (0x00E1C100) ───────────────────────────────────────────
// Original arg order is (cmdQueue, model); stored as this[1]=cmdQueue, this[2]=model.
char __fastcall hkDispInit(CDispatcher* self, u32 /*edx*/, void* cmdQueue, void* model) {
    self->m_pkCommandQueue = static_cast<ICommandQueue*>(cmdQueue);
    self->m_pkModel        = model;
    return 1;
}

// ── CDispatcher::SetState  (0x00E1D5D0) ───────────────────────────────────────
// vtbl[3]: mark the main connector and every sub-connector down with `state`.
char __fastcall hkDispSetState(CDispatcher* self, u32 /*edx*/, int state) {
    if (self->m_pkMainConn)
        markDown(self->m_pkMainConn, state);
    for (u32 i = 0; i < self->m_uSubCount; ++i)
        if (self->m_pkSubConns[i])
            markDown(self->m_pkSubConns[i], state);
    return 1;
}

// ── CDispatcher::OnRead  (0x00E1D320) ─────────────────────────────────────────
// vtbl[4]: drain the main connector's socket (recv -> frame parse -> cmdQueue.Put).
int __fastcall hkDispOnRead(CDispatcher* self, u32 /*edx*/) {
    return hConnRead(self->m_pkMainConn);
}

// ── CDispatcher::Process  (0x00E1D660) ────────────────────────────────────────
// vtbl[5]: the per-frame send/keepalive pump. Main connector failures escalate via
// SetState (which marks ALL connectors down — original behaviour); sub-connector
// failures are marked down individually. Gated on the main socket being valid.
char __fastcall hkDispProcess(CDispatcher* self, u32 /*edx*/) {
    CConnector* main = self->m_pkMainConn;
    CTcpSocket* mainSock = main->m_pkSocket;
    if (mainSock->m_skSocket == INVALID_SOCKET)
        return 0;

    if (!hSocketSendQueue(mainSock)) {                  // flush failed -> hard down
        hkDispSetState(self, 0, 3);
        return 0;
    }
    if (!hConnKeepAlive(main))                            // keepalive lapsed
        hkDispSetState(self, 0, 6);

    for (u32 i = 0; i < self->m_uSubCount; ++i) {
        if (mainSock->m_skSocket == INVALID_SOCKET)      // orig re-checks the MAIN socket
            continue;
        CConnector* sub = self->m_pkSubConns[i];
        CTcpSocket* subSock = sub->m_pkSocket;
        if (!hSocketSendQueue(subSock))
            markDown(sub, 3);
        else if (!hConnKeepAlive(sub))
            markDown(sub, 6);
    }
    return 1;
}

// ── CDispatcher connector linkage (shared by SetMain / SetSub) ─────────────────
inline void linkConnector(CDispatcher* self, CConnector* c) {
    hConnInit(c, self->m_pkCommandQueue, self->m_pkModel);
    c->m_pkDispatcher = self;
    c->m_iRelay       = self->m_iIndex;
    c->m_iSessionId   = static_cast<u16>(self->m_iIndex);
    c->m_iSessionId2  = static_cast<u16>(self->m_iIndex);
}

// ── CDispatcher::SetMainConnector  (0x00E1D890) ───────────────────────────────
// vtbl[6]: replace the main connector with a fresh one of the given factory type.
void __fastcall hkDispSetMainConnector(CDispatcher* self, u32 /*edx*/, int type) {
    if (!hFactoryRegistered()) {
        // [orig: diag log] "connector is not register in g_kConnectorFactory"
        return;
    }
    if (self->m_pkMainConn) {
        hDeleteVirtual(self->m_pkMainConn);
        self->m_pkMainConn = nullptr;
    }
    CConnector* c = hFactoryCreate(type);
    self->m_pkMainConn = c;
    linkConnector(self, c);
}

// ── CDispatcher::SetSubConnectors  (0x00E1D9E0) ───────────────────────────────
// vtbl[7]: (re)allocate `count` sub-connectors (factory type 1 each).
void __fastcall hkDispSetSubConnectors(CDispatcher* self, u32 /*edx*/, int count) {
    if (!hFactoryRegistered()) {
        // [orig: diag log] "connector is not register in g_kConnectorFactory"
        return;
    }
    if (self->m_pkSubConns) {                            // tear down the old array
        for (u32 i = 0; i < self->m_uSubCount; ++i) {
            CConnector* c = self->m_pkSubConns[i];
            if (c) {
                if (c->m_pkSocket && c->m_pkSocket->m_iNetworkState /*connected*/)
                    hSocketSendQueue(c->m_pkSocket); // [orig flushes the down-going link]
                hDeleteVirtual(c);
                self->m_pkSubConns[i] = nullptr;
            }
        }
        mg::call<void(__cdecl*)(void*)>(addr::msvc::operator_delete_array, self->m_pkSubConns);
        self->m_pkSubConns = nullptr;
    }
    if (!count)
        return;

    self->m_uSubCount = static_cast<u32>(count);
    self->m_pkSubConns = static_cast<CConnector**>(
        mg::call<void*(__cdecl*)(u32)>(addr::msvc::operator_new_array, static_cast<u32>(4 * count)));
    for (int i = 0; i < count; ++i) {
        CConnector* c = hFactoryCreate(1);
        self->m_pkSubConns[i] = c;
        hConnInit(c, self->m_pkCommandQueue, self->m_pkModel);
        c->m_pkDispatcher = self;
        c->m_iRelay       = self->m_iIndex;
        c->m_iSessionId   = static_cast<u16>(self->m_iIndex);
        c->m_iSessionId2  = static_cast<u16>(self->m_iIndex);
    }
}

// ── CDispatcherArray::Run  (0x00E1C760) ───────────────────────────────────────
// vtbl[5]: idx==0 -> run Process on every dispatcher (the per-frame send pump);
// idx!=0 -> run a contiguous sub-range (used for fan-out). Routed through each
// dispatcher's vtable so installed detours still apply.
void __fastcall hkArrayRun(CDispatcherArray* self, u32 /*edx*/, int idx) {
    auto runProcess = [](CDispatcher* d) {
        reinterpret_cast<char(__thiscall*)(CDispatcher*)>(d->_vptr->Process)(d);
    };
    if (idx == 0) {
        for (u32 i = 0; i < self->m_uCount; ++i)
            if (self->m_pkList[i]) runProcess(self->m_pkList[i]);
            // else [orig: diag log] "dispatcher is null"
    } else {
        // Sub-range form indexes a secondary count/list pair stored per-slot in the
        // 0x190 array region; preserved verbatim from the original. VERIFY-vs-IDA.
        auto* base = reinterpret_cast<u32*>(self);
        const u32 start = base[/*this+4*idx+12*/ idx + 3];
        const u32 end   = base[/*this+4*idx+16*/ idx + 4];
        for (u32 i = start; i < end; ++i)
            if (self->m_pkList[i]) runProcess(self->m_pkList[i]);
    }
}

// ── CDispatcherArray::SetMainConnAll  (0x00E1C5A0) ────────────────────────────
char __fastcall hkArraySetMainAll(CDispatcherArray* self, u32 /*edx*/, int type) {
    for (u32 i = 0; i < self->m_uCount; ++i)
        if (self->m_pkList[i])
            reinterpret_cast<void(__thiscall*)(CDispatcher*, int)>(
                self->m_pkList[i]->_vptr->SetMainConnector)(self->m_pkList[i], type);
        // else [orig: diag log] "dispatcher is null"
    return 1;
}

// ── CDispatcherArray::SetSubConnAll  (0x00E1C680) ─────────────────────────────
char __fastcall hkArraySetSubAll(CDispatcherArray* self, u32 /*edx*/, int count) {
    for (u32 i = 0; i < self->m_uCount; ++i)
        if (self->m_pkList[i])
            reinterpret_cast<void(__thiscall*)(CDispatcher*, int)>(
                self->m_pkList[i]->_vptr->SetSubConnectors)(self->m_pkList[i], count);
    return 1;
}

} // namespace

// ── Detour install (inert — every .create is commented out) ───────────────────
void InstallDispatcherEngine() {
    // auto& reg = mg::ctx().hookRegistry();
    // reg.registerDetour(HookId::Disp_Init             ).create(MG_CONST(addr::net::dispatcher::Init),             hkDispInit);
    // reg.registerDetour(HookId::Disp_SetState         ).create(MG_CONST(addr::net::dispatcher::SetState),         hkDispSetState);
    // reg.registerDetour(HookId::Disp_OnRead           ).create(MG_CONST(addr::net::dispatcher::OnRead),           hkDispOnRead);
    // reg.registerDetour(HookId::Disp_Process          ).create(MG_CONST(addr::net::dispatcher::Process),          hkDispProcess);
    // reg.registerDetour(HookId::Disp_SetMainConnector ).create(MG_CONST(addr::net::dispatcher::SetMainConnector), hkDispSetMainConnector);
    // reg.registerDetour(HookId::Disp_SetSubConnectors ).create(MG_CONST(addr::net::dispatcher::SetSubConnectors), hkDispSetSubConnectors);
    // reg.registerDetour(HookId::DispArr_Run           ).create(MG_CONST(addr::net::dispatcher::array::Run),            hkArrayRun);
    // reg.registerDetour(HookId::DispArr_SetMainConnAll).create(MG_CONST(addr::net::dispatcher::array::SetMainConnAll), hkArraySetMainAll);
    // reg.registerDetour(HookId::DispArr_SetSubConnAll ).create(MG_CONST(addr::net::dispatcher::array::SetSubConnAll),  hkArraySetSubAll);
    //
    // CDispatcherArray::Init (0x00E1C190) constructs the N CDispatcher objects and
    // is construction plumbing — deferred to the engine-wiring step. delete[] is
    // shown via addr::msvc::operator_new as a placeholder; wire the real
    // operator delete[] (0x00F1996C) when the vtable swap goes live.
    (void)0;
}

} // namespace mg::game::net
