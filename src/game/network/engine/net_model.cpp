// =============================================================================
// Network engine swap — CEventSelectModel (ISensor)  [see .hpp]
// =============================================================================
// Reversed 1:1 from MicroVolts.exe.i64:
//   CEventSelectModel::Init             0x00E17C40 (count, cmdQueue) -> new CDispatcherArray
//   CEventSelectModel::Select           0x00E17EC0 (DWORD timeout)   — WSAWaitForMultipleEvents
//   CEventSelectModel::Run              0x00E17C30 -> CDispatcherArray::Run
//   CEventSelectModel::SetNetType       0x00E1B8F0 (type<2) -> array SetMainConnAll
//   CEventSelectModel::RegisterSocket   0x00E183E0 (connId, SOCKET)
//   CEventSelectModel::UnregisterSocket 0x00E18580 (idx, SOCKET)
//   CEventSelectModel::Reset            0x00E17E40 (WSACloseEvent all + memset)
//
// The recv "wait loop" (Select) is NOT on a thread — the engine's recv pump calls
// it with timeout≈0 each frame. Per signaled socket: WSAEnumNetworkEvents -> look
// up the owning dispatcher (array vtbl[9]) -> handle FD_CLOSE, else FD_READ by
// calling the dispatcher's OnRead (which pulls the connector's recv/frame parser).
//
// FD set used at registration is FD_READ|FD_ACCEPT|FD_CLOSE (== 41), matching the
// original WSAEventSelect call. Verbose LogFile() blocks are not reproduced.
// =============================================================================
#include "pch.hpp"
#include "game/network/engine/net_model.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "utils/call_helper.hpp"

namespace mg::game::net {
namespace {

namespace md = addr::net::model;

// FD_*_BIT indices into WSANETWORKEVENTS::iErrorCode (winsock2).
constexpr int kFdReadBit  = 0; // FD_READ_BIT
constexpr int kFdCloseBit = 5; // FD_CLOSE_BIT
constexpr long kFdRegisterMask = FD_READ | FD_ACCEPT | FD_CLOSE; // 41

// ── array helpers ─────────────────────────────────────────────────────────────
inline CDispatcher* hArrayGetDispatcher(CDispatcherArray* arr, int connectorId) {
    return reinterpret_cast<CDispatcher*(__thiscall*)(CDispatcherArray*, int)>(
        arr->_vptr->GetDispatcher)(arr, connectorId);
}
inline int hDispatcherOnRead(CDispatcher* d) {
    return reinterpret_cast<int(__thiscall*)(CDispatcher*)>(d->_vptr->OnRead)(d);
}

// ── CEventSelectModel::Select  (0x00E17EC0) ───────────────────────────────────
// The recv wait loop. Returns -2 if nothing is registered, else 0. Walks from the
// first signaled event to the end, servicing each readable/closing socket once.
int __fastcall hkModelSelect(CEventSelectModel* self, u32 /*edx*/, DWORD timeout) {
    const u32 count = self->m_uCount;
    if (!count)
        return -2;

    DWORD idx = WSAWaitForMultipleEvents(count, self->m_aEvents, FALSE, timeout, FALSE);
    if (idx >= count)                       // WSA_WAIT_TIMEOUT / out of range
        return 0;

    for (; idx < count; ++idx) {
        const DWORD r = WSAWaitForMultipleEvents(1, &self->m_aEvents[idx], FALSE, 0, FALSE);
        if (r == WSA_WAIT_FAILED || r == WSA_WAIT_TIMEOUT)
            continue;

        WSANETWORKEVENTS ev{};
        WSAEnumNetworkEvents(self->m_aSlots[idx].m_skSocket, self->m_aEvents[idx], &ev);

        CDispatcher* d = hArrayGetDispatcher(self->m_pkArray, self->m_aSlots[idx].m_iConnectorId);
        if (!d) {
            // [orig: diag log] "dispatcher is null"
            continue;
        }

        if (ev.lNetworkEvents & FD_CLOSE) {
            // Drain anything still buffered, then drop. (Both branches just log.)
            if (ev.iErrorCode[kFdCloseBit] || hDispatcherOnRead(d)) {
                // [orig: diag log] "CLOSE: [err][socket]"
            } else {
                // [orig: diag log] "CLOSED: [err][socket]"
            }
            continue;
        }

        // FD_READ path: OnRead returns true on success; a read error with the
        // FD_READ error code set is logged, otherwise we just move on.
        if (hDispatcherOnRead(d) || !ev.iErrorCode[kFdReadBit])
            continue;
        // [orig: diag log] "read failed: [socket]"
    }
    return 0;
}

// ── CEventSelectModel::Run  (0x00E17C30) ──────────────────────────────────────
// vtbl[5]: delegate the per-frame send/keepalive pump to the dispatcher array.
int __fastcall hkModelRun(CEventSelectModel* self, u32 /*edx*/) {
    return reinterpret_cast<int(__thiscall*)(CDispatcherArray*, int)>(
        self->m_pkArray->_vptr->Run)(self->m_pkArray, 0);
}

// ── CEventSelectModel::SetNetType  (0x00E1B8F0) ───────────────────────────────
// vtbl[6]: type<2 only; forwards to CDispatcherArray::SetMainConnAll(type).
char __fastcall hkModelSetNetType(CEventSelectModel* self, u32 /*edx*/, int type) {
    if (type >= 2)
        return 0;
    return reinterpret_cast<char(__thiscall*)(CDispatcherArray*, int)>(
        self->m_pkArray->_vptr->SetMainConnAll)(self->m_pkArray, type);
}

// ── CEventSelectModel::RegisterSocket  (0x00E183E0) ───────────────────────────
// vtbl[8]: append (socket, connectorId) to the table + an FD_READ|ACCEPT|CLOSE
// WSAEventSelect event. Rejects INVALID_SOCKET and overflow past 64 connections.
char __fastcall hkModelRegisterSocket(CEventSelectModel* self, u32 /*edx*/, int connectorId, SOCKET s) {
    if (s == INVALID_SOCKET) {
        // [orig: diag log] "Register EventSelect: error"
        return 0;
    }
    if (self->m_uCount >= md::MaxConns) {
        // [orig: diag log] "Register EventSelect: over flow connection"
        return 0;
    }
    const u32 i = self->m_uCount;
    self->m_aSlots[i].m_skSocket     = s;
    self->m_aSlots[i].m_iConnectorId = connectorId;
    self->m_aEvents[i] = WSACreateEvent();
    WSAEventSelect(s, self->m_aEvents[i], kFdRegisterMask);
    ++self->m_uCount;
    return 1;
}

// ── CEventSelectModel::UnregisterSocket  (0x00E18580) ─────────────────────────
// vtbl[9]: find the slot whose socket == s, close its event, compact the table.
char __fastcall hkModelUnregisterSocket(CEventSelectModel* self, u32 /*edx*/, int /*idxHint*/, SOCKET s) {
    if (s == INVALID_SOCKET) {
        // [orig: diag log] "Unregister EventSelect: error"
        return 0;
    }
    if (!self->m_uCount)
        return 0;

    u32 i = 0;
    while (self->m_aSlots[i].m_skSocket != s) {
        if (++i >= self->m_uCount)
            return 0;
    }

    WSACloseEvent(self->m_aEvents[i]);
    const u32 newCount = --self->m_uCount;
    if (i < newCount) {
        memmove(&self->m_aSlots[i], &self->m_aSlots[i + 1],
                sizeof(CEventSelectModelSlot) * (newCount - i));
        memmove(&self->m_aEvents[i], &self->m_aEvents[i + 1],
                sizeof(HANDLE) * (newCount - i));
        self->m_aSlots[newCount].m_skSocket = INVALID_SOCKET;
    }
    return 1;
}

// ── CEventSelectModel::Reset  (0x00E17E40) ────────────────────────────────────
// vtbl[3]: close all events, blank the (socket,connector) table (0xFF) and the
// event handles (0). Calls the base reset (sub_E1B4C0) afterwards.
int __fastcall hkModelReset(CEventSelectModel* self, u32 /*edx*/) {
    if (self->m_uCount) {
        for (u32 i = 0; i < self->m_uCount; ++i)
            if (self->m_aSlots[i].m_skSocket != INVALID_SOCKET)
                WSACloseEvent(self->m_aEvents[i]);
        self->m_uCount = 0;
        nocrtMemset(self->m_aSlots, 0xFF, sizeof(self->m_aSlots));   // 0x200
        nocrtMemset(self->m_aEvents, 0,   sizeof(self->m_aEvents));  // 0x100
    }
    return mg::call<int(__thiscall*)(CEventSelectModel*)>(0x00E1B4C0, self); // base reset
}

// ── CEventSelectModel::Init  (0x00E17C40) ─────────────────────────────────────
// vtbl[1]: build the CDispatcherArray (NiSPJob) and init it with (cmdQueue, model,
// count). Bounds: not already inited, 1 <= count <= 64.
char __fastcall hkModelInit(CEventSelectModel* self, u32 /*edx*/, int count, void* cmdQueue) {
    if (self->m_bInited || static_cast<unsigned>(count - 1) > 0x3F)
        return 0;

    auto* arr = static_cast<CDispatcherArray*>(
        mg::call<void*(__cdecl*)(u32)>(addr::msvc::operator_new, addr::net::dispatcher::array::AllocSize));
    if (arr)
        mg::call<void*(__thiscall*)(void*)>(addr::net::dispatcher::array::Ctor, arr);
    self->m_pkArray = arr;
    if (!arr) {
        // [orig: diag log] "dispatcher list is null: memory is not enough"
        return 0;
    }

    // arr->Init(cmdQueue, model, count)  — vtbl[1]
    const bool ok = reinterpret_cast<char(__thiscall*)(CDispatcherArray*, void*, void*, int)>(
        arr->_vptr->Init)(arr, cmdQueue, self, count) != 0;
    if (ok) {
        // *(model+0x14) = count (configured max); +0x0C = inited
        *reinterpret_cast<u32*>(reinterpret_cast<char*>(self) + 0x14) = static_cast<u32>(count);
        self->m_bInited = 1;
        return 1;
    }

    // [orig: diag log] "dispatcher list is not initialize" + tear down
    if (self->m_pkArray) {
        auto** vt = *reinterpret_cast<void***>(self->m_pkArray);
        reinterpret_cast<void(__thiscall*)(void*)>(vt[2])(self->m_pkArray);
        reinterpret_cast<void(__thiscall*)(void*, int)>(vt[0])(self->m_pkArray, 1);
        self->m_pkArray = nullptr;
    }
    return 0;
}

} // namespace

// ── Detour install (inert — every .create is commented out) ───────────────────
void InstallNetModelEngine() {
    // auto& reg = mg::ctx().hookRegistry();
    // reg.registerDetour(HookId::Model_Init            ).create(MG_CONST(addr::net::model::Init),             hkModelInit);
    // reg.registerDetour(HookId::Model_Select          ).create(MG_CONST(addr::net::model::Select),           hkModelSelect);
    // reg.registerDetour(HookId::Model_Run             ).create(MG_CONST(addr::net::model::Run),              hkModelRun);
    // reg.registerDetour(HookId::Model_SetNetType      ).create(MG_CONST(addr::net::model::SetNetType),       hkModelSetNetType);
    // reg.registerDetour(HookId::Model_RegisterSocket  ).create(MG_CONST(addr::net::model::RegisterSocket),   hkModelRegisterSocket);
    // reg.registerDetour(HookId::Model_UnregisterSocket).create(MG_CONST(addr::net::model::UnregisterSocket), hkModelUnregisterSocket);
    // reg.registerDetour(HookId::Model_Reset           ).create(MG_CONST(addr::net::model::Reset),            hkModelReset);
    //
    // ctor (0x00E17BF0) sets the vftable + blanks the tables; deferred to the
    // vtable-swap step. The base-reset leaf (sub_E1B4C0) is called directly above.
    (void)0;
}

} // namespace mg::game::net
