// =============================================================================
// Network engine swap — CTcpConnector ctor + Main/Cast subclasses  [1:1]
// =============================================================================
// Reversed 1:1 from MicroVolts.exe.i64:
//   CTcpConnector::CTcpConnector     0x00E13160  (base: alloc socket + recv buffer)
//   CMainConnectorTcp::CMainConnectorTcp 0x00AB87B0
//   CCastConnectorTcp::CCastConnectorTcp 0x00AB8720
//   CTcpConnector base disconnect    0x00E12BA0  (sub_E12BA0 — socket teardown)
//
// The base connector ctor allocates a CTcpSocket (0xA8) and a 0xC0000 (768KB)
// recv stream buffer, points the vtable at CConnectorTcp, and Clear()s. Each
// subclass ctor chains the base, swaps to its own vtable, clears m_bIsConnected
// and stamps socket->dword0074 = 2 / dword0078 = 0.
//
// The CMainConnectorTcp::Disconnect override (0xAB0760) is overwhelmingly game/UI
// policy (server-disconnect message boxes, CStateMgr transitions, nation checks);
// its net core is the base teardown sub_E12BA0 below. We DO NOT reimplement the
// override — when the swap goes live the Main vtable keeps slot 14 pointed at the
// original 0xAB0760, so the disconnect UX is preserved unchanged.
//
// base disconnect semantics (sub_E12BA0): reject mismatched/closed socket; if the
// caller asked to keep-on-pending (a3) and there is still in-flight data
// (HIDWORD(m_ullKeepAliveTick) != 0, i.e. connector+0x30 — the same in-flight slot
// the command queue increments on Put), mark state 10 ("pending") and defer; else
// Clear() the connector. Returns 1 when actually torn down.
//
// Install hooks are written but left COMMENTED OUT (see InstallConnectorSubclasses).
// =============================================================================
#include "pch.hpp"
#include "game/network/engine/connector_subclass.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "utils/call_helper.hpp"

namespace mg::game::net {
namespace {

namespace co = addr::net::connector;

// ── leaf helpers (construction plumbing called into the client) ───────────────
inline void hConnectorBaseInit(CTcpConnector* c) {
    mg::call<void*(__thiscall*)(void*)>(co::BaseCtor, c);            // CConnector::CConnector
}
inline void hStreamBufferCtor(CStreamBuffer* sb) {
    mg::call<void*(__thiscall*)(void*)>(addr::net::helpers::StreamBufferCtor, sb);
}
inline void hStreamBufferSetBuffer(CStreamBuffer* sb, void* buf, u32 size) {
    mg::call<void(__thiscall*)(void*, void*, u32)>(addr::net::helpers::StreamBufferSetBuffer, sb, buf, size);
}
inline CTcpSocket* hNewTcpSocket() {
    void* raw = mg::call<void*(__cdecl*)(u32)>(addr::msvc::operator_new, co::TcpSocketSize);
    return raw ? mg::call<CTcpSocket*(__thiscall*)(void*)>(addr::net::tcp_socket::Ctor, raw) : nullptr;
}
inline void hConnectorClear(CTcpConnector* c) {
    mg::call<void(__thiscall*)(void*)>(co::Clear, c);               // CTcpConnector::Clear
}

// ── CTcpConnector::CTcpConnector  (0x00E13160) ────────────────────────────────
// Base connector ctor: build the socket + recv stream buffer, set the base vtable,
// Clear(). (Field zeroing is done by CConnector::CConnector / Clear.)
CTcpConnector* __fastcall hkConnectorCtor(CTcpConnector* self, u32 /*edx*/) {
    hConnectorBaseInit(self);
    self->_vptr_CConnector = reinterpret_cast<CTcpConnector_vtbl*>(co::ConnectorTcpVft);
    hStreamBufferCtor(&self->m_kStreamBuffer);
    self->m_acRecvBuffer = nullptr;

    self->m_pkSocket = hNewTcpSocket();

    auto* buf = static_cast<char*>(
        mg::call<void*(__cdecl*)(u32)>(addr::msvc::operator_new_array, co::RecvBufSize));
    self->m_acRecvBuffer = buf;
    if (buf)
        hStreamBufferSetBuffer(&self->m_kStreamBuffer, buf, co::RecvBufSize);

    hConnectorClear(self);
    return self;
}

// ── CMainConnectorTcp / CCastConnectorTcp ctors  (0xAB87B0 / 0xAB8720) ─────────
inline void subclassCtorCommon(CTcpConnector* self, uptr vft) {
    hkConnectorCtor(self, 0);                       // chain the base ctor
    self->_vptr_CConnector = reinterpret_cast<CTcpConnector_vtbl*>(vft);
    self->m_bIsConnected = false;
    self->m_pkSocket->dword0074 = 2;
    self->m_pkSocket->dword0078 = 0;
}

CTcpConnector* __fastcall hkMainCtor(CTcpConnector* self, u32 /*edx*/) {
    subclassCtorCommon(self, addr::net::netmgr::MainVft);
    return self;
}
CTcpConnector* __fastcall hkCastCtor(CTcpConnector* self, u32 /*edx*/) {
    subclassCtorCommon(self, addr::net::netmgr::CastVft);
    return self;
}

// ── CTcpConnector base disconnect  (sub_E12BA0) ───────────────────────────────
// Socket teardown shared by the subclass Disconnect overrides. `sockHandle` must
// match the live socket. `keepOnPending` defers the teardown while data is still
// in flight (the connector+0x30 / keepalive-high counter is non-zero).
char __fastcall hkBaseDisconnect(CTcpConnector* self, u32 /*edx*/, int sockHandle, char keepOnPending) {
    if (sockHandle == -1)
        return 0;
    CTcpSocket* sock = self->m_pkSocket;
    if (static_cast<int>(sock->m_skSocket) != sockHandle)
        return 0;

    const u32 inFlightHigh = *(reinterpret_cast<u32*>(&self->m_ullKeepAliveTick) + 1); // HIDWORD
    if (keepOnPending && inFlightHigh) {
        if (!sock->m_iNetworkState)
            sock->m_iNetworkState = 10;             // pending disconnect
        return 0;
    }
    if (sock->m_iNetworkState == 10)
        sock->m_iNetworkState = 1;

    self->_vptr_CConnector->Clear(self);
    return 1;
}

} // namespace

// ── Detour install (inert — every .create is commented out) ───────────────────
void InstallConnectorSubclasses() {
    // auto& reg = mg::ctx().hookRegistry();
    // reg.registerDetour(HookId::Connector_Ctor    ).create(MG_CONST(addr::net::connector::Ctor),           hkConnectorCtor);
    // reg.registerDetour(HookId::MainConnector_Ctor).create(MG_CONST(addr::net::netmgr::MainCtor),           hkMainCtor);
    // reg.registerDetour(HookId::CastConnector_Ctor).create(MG_CONST(addr::net::netmgr::CastCtor),           hkCastCtor);
    // reg.registerDetour(HookId::Connector_BaseDisc).create(MG_CONST(addr::net::connector::BaseDisconnect),  hkBaseDisconnect);
    //
    // The CMainConnectorTcp::Disconnect override (0xAB0760, conn vtbl[14]) stays
    // original — it is disconnect UX, not net-engine logic. The Front connector
    // uses the plain CConnectorTcp vtable (built by the factory in CreateConnector),
    // so it needs no dedicated subclass ctor here.
    (void)hkConnectorCtor; (void)hkMainCtor; (void)hkCastCtor; (void)hkBaseDisconnect;
}

} // namespace mg::game::net
