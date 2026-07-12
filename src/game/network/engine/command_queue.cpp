// =============================================================================
// Network engine swap — ICommandQueue (Extend + Native)  [see .hpp]
// =============================================================================
// Reversed 1:1 from MicroVolts.exe.i64:
//   CExCommandQueue::NonEmpty  0x00C4D660   (returns _Mysize)
//   CExCommandQueue::Get       0x00E119B0   (pop front; decrement connector in-flight)
//   CExCommandQueue::Put       0x00E11D30   (build CExtendCommand, decrypt, push_back)
//   CExCommandQueue::PutEx     0x00E11FB0   (Put filtered by regIdx & sessionId2)
//   CCommandQueue::Put         0x00E18EF0   (native variant — CBasicCommand)
//
// The embedded ring (queue + 0x1C) is a stock MSVC std::deque<Cmd*>. Its push /
// subscript are done by the client's own helpers (addr::net::helpers::Deque*),
// and the per-command storage comes from the client's object pools
// (Extend/Native PoolAlloc) — both called into directly, exactly like the
// CBlockQueue / CStreamBuffer leaf helpers in tcp_socket.cpp. We only reimplement
// the queue *logic* (validation, the decrypt-into-command, the trailer stamping,
// and the ring book-keeping in Get).
//
// VERIFY-vs-IDA:
//  * Connector in-flight counter is the raw dword at connector+0x30 (Put: ++,
//    Get: --). This overlaps the keepalive-tick region in the current CConnector
//    layout; kept as a raw offset to match the binary byte-for-byte.
//  * Decrypt is connector vtable slot 19 (offset 0x4C) — the received body is
//    decrypted straight into the command's packet area (cmd + 8).
// =============================================================================
#include "pch.hpp"
#include "game/network/engine/command_queue.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "utils/call_helper.hpp"

namespace mg::game::net {
namespace {

namespace cq = addr::net::command_queue;

// ── Leaf-helper call wrappers (call straight into the client) ─────────────────
inline u64 hTickMSec() {
    void* inst = mg::call<void*(__cdecl*)()>(addr::net::helpers::TickGetInstance);
    return mg::call<u64(__thiscall*)(void*)>(addr::net::helpers::TickGetMSec, inst);
}
// std::deque<Cmd*> base lives at (queue + 0x1C); element block size is 4.
inline char* hDequeBase(ICommandQueue* q) {
    return reinterpret_cast<char*>(q) + kCmdDequeBase;
}
// push_back: sub_E18DF0(deque, &cmd)  — __thiscall(deque, Cmd**)
inline void hDequePushBack(ICommandQueue* q, void* cmd) {
    void* slot = cmd;
    mg::call<void*(__thiscall*)(void*, void*)>(addr::net::helpers::DequePushBack, hDequeBase(q), &slot);
}
// front element at the given offset. sub_E1CC80 takes a {_Myproxy, idx} pair by
// pointer and returns the *address* of the slot; deref gives the Cmd*.
inline void* hDequeAt(ICommandQueue* q, u32 frontIdx) {
    void* args[2] = { q->m_pMyproxy, reinterpret_cast<void*>(static_cast<uptr>(frontIdx)) };
    uptr p = mg::call<uptr(__thiscall*)(void*)>(addr::net::helpers::DequeSubscript, args);
    return *reinterpret_cast<void**>(p);
}
inline void* hExtendAlloc() {
    void* raw = mg::call<void*(__cdecl*)(u32)>(addr::net::helpers::ExtendPoolAlloc, cq::ExtendCmdSize);
    return raw ? mg::call<void*(__thiscall*)(void*)>(addr::net::helpers::ExtendPoolInit, raw) : nullptr;
}
inline void* hNativeAlloc() {
    // Native pool returns raw storage; ctor (CTcpPacket + vtable) is set up here.
    return mg::call<void*(__cdecl*)(u32)>(addr::net::helpers::NativePoolAlloc, cq::BasicCmdSize);
}
// connector->Decrypt(cryptType, in, out, size)  — vtable slot 19 (offset 0x4C).
inline void hConnectorDecrypt(CConnector* conn, int cryptType, void* in, void* out, int size) {
    conn->_vptr_CConnector->Decrypt(conn, cryptType, in, out, size);
}

// In-flight command counter (raw dword at connector+0x30).
inline u32& hInFlight(CConnector* conn) {
    return *reinterpret_cast<u32*>(reinterpret_cast<char*>(conn) + 0x30);
}

// ── CExCommandQueue::NonEmpty  (0x00C4D660) ───────────────────────────────────
// vtbl[3]: returns _Mysize (0 == empty). The drain / Get gate on this.
int __fastcall hkExNonEmpty(ICommandQueue* self, u32 /*edx*/) {
    return static_cast<int>(self->m_uCount);
}

// ── CExtendCommand trailer stamping (shared by Put / PutEx) ────────────────────
inline void stampExtend(CExtendCommand* cmd, CConnector* conn, int size) {
    cmd->m_pkConnector  = conn;
    cmd->m_iSessionId2  = *reinterpret_cast<u16*>(reinterpret_cast<char*>(conn) + 54); // m_iSessionId2
    cmd->m_iRegisterIdx = *reinterpret_cast<u32*>(reinterpret_cast<char*>(conn) + 60); // m_iRegisterIndex
    cmd->m_iSize        = static_cast<u32>(size);
    cmd->m_ullTick      = hTickMSec();
    ++hInFlight(conn);
}

// ── CExCommandQueue::Put  (0x00E11D30) ────────────────────────────────────────
// vtbl[5]: allocate a CExtendCommand, decrypt the body into cmd+8, stamp the
// trailer, push_back. Returns 1 on success, 0 on bad size / pool exhaustion.
char __fastcall hkExPut(ICommandQueue* self, u32 /*edx*/, CConnector* conn,
                        void* body, int size, int cryptType) {
    EnterCriticalSection(&self->m_kLocker.m_tMutex);

    if (size <= 0 || static_cast<unsigned>(size) > 1432) {
        // [orig: diag log] "packet size error: too short or big"
        LeaveCriticalSection(&self->m_kLocker.m_tMutex);
        return 0;
    }

    auto* cmd = static_cast<CExtendCommand*>(hExtendAlloc());
    if (!cmd) {
        // [orig: diag log] "new ExtendCommand failed"
        LeaveCriticalSection(&self->m_kLocker.m_tMutex);
        return 0;
    }

    hConnectorDecrypt(conn, cryptType, body, &cmd->m_kPacket, size);
    stampExtend(cmd, conn, size);
    hDequePushBack(self, cmd);

    LeaveCriticalSection(&self->m_kLocker.m_tMutex);
    return 1;
}

// ── CExCommandQueue::PutEx  (0x00E11FB0) ──────────────────────────────────────
// vtbl[6]: as Put, but only enqueues when the command targets this connector's
// register index (a6) AND session id 2 (a7); otherwise the freshly-built command
// is dropped (returns 0). Size cap here is 0x598. Used for relay/registered fan-out.
char __fastcall hkExPutEx(ICommandQueue* self, u32 /*edx*/, CConnector* conn,
                          void* body, int size, int cryptType, int regIdx, int sid2) {
    EnterCriticalSection(&self->m_kLocker.m_tMutex);

    if (size <= 0 || static_cast<unsigned>(size) > 0x598) {
        // [orig: diag log] "packet size error: too short or big"
        LeaveCriticalSection(&self->m_kLocker.m_tMutex);
        return 0;
    }

    auto* cmd = static_cast<CExtendCommand*>(hExtendAlloc());
    if (!cmd) {
        // [orig: diag log] "new ExtendCommand failed"
        LeaveCriticalSection(&self->m_kLocker.m_tMutex);
        return 0;
    }

    hConnectorDecrypt(conn, cryptType, body, &cmd->m_kPacket, size);

    const u32 connReg  = *reinterpret_cast<u32*>(reinterpret_cast<char*>(conn) + 60);
    const u16 connSid2 = *reinterpret_cast<u16*>(reinterpret_cast<char*>(conn) + 54);
    if (regIdx == static_cast<int>(connReg) && sid2 == static_cast<int>(connSid2)) {
        cmd->m_iRegisterIdx = connReg;
        cmd->m_iSessionId2  = connSid2;
        cmd->m_pkConnector  = conn;
        cmd->m_iSize        = static_cast<u32>(size);
        cmd->m_ullTick      = hTickMSec();
        ++hInFlight(conn);
        hDequePushBack(self, cmd);
        LeaveCriticalSection(&self->m_kLocker.m_tMutex);
        return 1;
    }

    // Filter mismatch: command not enqueued (pool entry returns to the free list
    // on the next pool sweep, exactly as the original leaves it).
    LeaveCriticalSection(&self->m_kLocker.m_tMutex);
    return 0;
}

// ── CExCommandQueue::Get  (0x00E119B0) ────────────────────────────────────────
// vtbl[4]: pop the front command. Advances the deque ring (block size 4), tolerates
// (and logs) null slots by continuing, and decrements the originating connector's
// in-flight counter before returning. Returns 0 when empty.
CExtendCommand* __fastcall hkExGet(ICommandQueue* self, u32 /*edx*/) {
    // Fast gate via vtbl[3] NonEmpty (== _Mysize) before taking the lock.
    if (!reinterpret_cast<int(__thiscall*)(ICommandQueue*)>(self->_vptr->NonEmpty)(self))
        return nullptr;

    EnterCriticalSection(&self->m_kLocker.m_tMutex);

    if (self->m_uCount == 0) {
        LeaveCriticalSection(&self->m_kLocker.m_tMutex);
        return nullptr;
    }

    void* front = nullptr;
    for (;;) {
        front = hDequeAt(self, self->m_uFrontIdx);

        if (self->m_uCount) {                                  // advance the ring
            if (4u * self->m_uMapSize <= ++self->m_uFrontIdx)
                self->m_uFrontIdx = 0;
            if (--self->m_uCount == 0)
                self->m_uFrontIdx = 0;
        }

        if (front)
            break;
        // [orig: diag log] "CExtendCommand is null" — skip and keep draining.
        if (self->m_uCount == 0) {
            LeaveCriticalSection(&self->m_kLocker.m_tMutex);
            return nullptr;
        }
    }

    auto* cmd = static_cast<CExtendCommand*>(front);
    if (cmd->m_pkConnector) {
        u32& inflight = hInFlight(static_cast<CConnector*>(cmd->m_pkConnector));
        if (inflight) --inflight;
    }

    LeaveCriticalSection(&self->m_kLocker.m_tMutex);
    return cmd;
}

// ── CCommandQueue::Put  (0x00E18EF0) ──────────────────────────────────────────
// Native (CBasicCommand) variant: no trailer fields, decrypt into cmd+8, push.
char __fastcall hkNaPut(ICommandQueue* self, u32 /*edx*/, CConnector* conn,
                        void* body, int size, int cryptType) {
    EnterCriticalSection(&self->m_kLocker.m_tMutex);

    if (size <= 0 || static_cast<unsigned>(size) > 0x598) {
        // [orig: diag log] "packet size error: too short or big"
        LeaveCriticalSection(&self->m_kLocker.m_tMutex);
        return 0;
    }

    auto* cmd = static_cast<CBasicCommand*>(hNativeAlloc());
    if (!cmd) {
        LeaveCriticalSection(&self->m_kLocker.m_tMutex);
        return 0;
    }

    hConnectorDecrypt(conn, cryptType, body, &cmd->m_kPacket, size);
    hDequePushBack(self, cmd);

    LeaveCriticalSection(&self->m_kLocker.m_tMutex);
    return 1;
}

} // namespace

// ── Detour install (inert — every .create is commented out) ───────────────────
void InstallCommandQueueEngine() {
    // auto& reg = mg::ctx().hookRegistry();
    // reg.registerDetour(HookId::CmdQueue_ExNonEmpty).create(MG_CONST(addr::net::command_queue::ExNonEmpty), hkExNonEmpty);
    // reg.registerDetour(HookId::CmdQueue_ExGet     ).create(MG_CONST(addr::net::command_queue::ExGet),      hkExGet);
    // reg.registerDetour(HookId::CmdQueue_ExPut     ).create(MG_CONST(addr::net::command_queue::ExPut),      hkExPut);
    // reg.registerDetour(HookId::CmdQueue_ExPutEx   ).create(MG_CONST(addr::net::command_queue::ExPutEx),    hkExPutEx);
    // reg.registerDetour(HookId::CmdQueue_NaPut     ).create(MG_CONST(addr::net::command_queue::NaPut),      hkNaPut);
    //
    // ctors (ExCtor 0x00E11B60 / NaCtor 0x00E18AA0) warm the object pools and set
    // up the std::deque proxy; deferred to the vtable-swap step (they only matter
    // when constructing a queue from scratch instead of swapping methods).
    (void)0;
}

} // namespace mg::game::net
