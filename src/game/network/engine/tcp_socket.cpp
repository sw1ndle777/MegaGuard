// =============================================================================
// Network engine swap — CTcpSocket layer (1:1 reimplementation)  [see .hpp]
// =============================================================================
// Reversed 1:1 from MicroVolts.exe.i64:
//   CreateSocket  0x00E192D0    Send       0x00E1A430
//   SendBlock     0x00E195E0    Read       0x00E1A7D0
//   SendTrust     0x00E196A0    ClearSendQueue 0x00E19270
//
// Control flow (socket states, WSA error classes, m_uliTick back-off math,
// m_iNetworkState transitions) is preserved exactly. The original's verbose
// diagnostic LogFile() blocks are NOT reproduced (they don't affect on-wire
// behavior); spots are marked `// [orig: diag log]`. Leaf helpers (CTick,
// CBlockQueue, CRawSocket::SetSocketOption) are still called into the client.
//
// VERIFY-vs-IDA notes:
//  * CBlockQueue::PutStream 3rd arg is passed BY VALUE (IDA shows (_DWORD*)Size,
//    i.e. the size value reinterpreted — treated here as u32 by value).
//  * CTick::GetMSec/GetTick taken as __thiscall(CTick*); GetInstance as __cdecl.
// =============================================================================
#include "pch.hpp"
#include "game/network/engine/tcp_socket.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "utils/call_helper.hpp"

namespace mg::game::net {
namespace {

// ── WSA error codes (numeric to match the original comparisons exactly) ───────
constexpr int kWSAEINTR        = 10004;
constexpr int kWSAEWOULDBLOCK  = 10035;
constexpr int kWSAENOBUFS      = 10055;
constexpr int kWSAETIMEDOUT    = 10060;
constexpr int kWSAEINVAL       = 10022;
constexpr int kWSAECONNABORTED = 10053;
constexpr int kWSAENETUNREACH  = 10051;
constexpr int kWSAEHOSTUNREACH = 10065;

// ── Leaf-helper call wrappers (call straight into the client) ─────────────────
inline void hRawSetSocketOption(CRawSocket* s, int a, int b) {
    mg::call<void(__thiscall*)(CRawSocket*, int, int)>(addr::net::helpers::RawSetSocketOption, s, a, b);
}
inline char hBlockQueuePutStream(CBlockQueue* q, void* src, u32 size) {
    return mg::call<char(__thiscall*)(CBlockQueue*, void*, u32)>(addr::net::helpers::BlockQueuePutStream, q, src, size);
}
inline void hBlockQueueClear(CBlockQueue* q) {
    mg::call<void(__thiscall*)(CBlockQueue*)>(addr::net::helpers::BlockQueueClear, q);
}
inline void* hTickInstance() {
    return mg::call<void*(__cdecl*)()>(addr::net::helpers::TickGetInstance);
}
inline u64 hTickMSec() {
    return mg::call<u64(__thiscall*)(void*)>(addr::net::helpers::TickGetMSec, hTickInstance());
}
inline u32 hTickTick() {
    return mg::call<u32(__thiscall*)(void*)>(addr::net::helpers::TickGetTick, hTickInstance());
}
inline void hRawSetLocalAddress(CRawSocket* s, char* addr_, int port) {
    mg::call<void(__thiscall*)(CRawSocket*, char*, int)>(addr::net::helpers::RawSetLocalAddress, s, addr_, port);
}
inline CTcpPacketBuffer* hBlockQueuePeek(CBlockQueue* q) {
    return mg::call<CTcpPacketBuffer*(__thiscall*)(CBlockQueue*)>(addr::net::helpers::BlockQueuePeek, q);
}
inline void hBlockQueuePop(CBlockQueue* q) {
    mg::call<void(__thiscall*)(CBlockQueue*)>(addr::net::helpers::BlockQueuePop, q);
}

// ── CTcpSocket::CreateSocket  (0x00E192D0) ────────────────────────────────────
int __fastcall hkCreateSocket(CTcpSocket* self, u32 /*edx*/) {
    if (self->m_skSocket != INVALID_SOCKET)
        return static_cast<int>(self->m_skSocket);

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0); // (2, 1, 0)
    if (s == INVALID_SOCKET)
        return -1;

    self->m_skSocket = s;
    hRawSetSocketOption(self, 4097, 0);  // 0x1001 (SO_SNDBUF)
    hRawSetSocketOption(self, 4098, 0);  // 0x1002 (SO_RCVBUF)

    if (self->m_skSocket != INVALID_SOCKET) {
        int one = 1;
        setsockopt(self->m_skSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&one), 4); // (6, 1)
    }
    u_long argp = 1;
    ioctlsocket(self->m_skSocket, FIONBIO, &argp); // 0x8004667E
    return static_cast<int>(self->m_skSocket);
}

// ── CTcpSocket::SendBlock  (0x00E195E0) ───────────────────────────────────────
// Thread-safe enqueue of a raw block into the send queue. Returns size on
// success, -2 on queue-full, or the (invalid) socket on a closed socket.
SOCKET __fastcall hkSendBlock(CTcpSocket* self, u32 /*edx*/, void* src, size_t size) {
    if (self->m_skSocket == INVALID_SOCKET)
        return self->m_skSocket;

    EnterCriticalSection(&self->m_kLocker.m_tMutex);
    const char ok = hBlockQueuePutStream(&self->m_kBlockQueue, src, static_cast<u32>(size));
    LeaveCriticalSection(&self->m_kLocker.m_tMutex);
    return ok ? static_cast<SOCKET>(size) : static_cast<SOCKET>(-2);
}

// ── CTcpSocket::ClearSendQueue  (0x00E19270) ──────────────────────────────────
void __fastcall hkClearSendQueue(CTcpSocket* self, u32 /*edx*/) {
    EnterCriticalSection(&self->m_kLocker.m_tMutex);
    hBlockQueueClear(&self->m_kBlockQueue);
    LeaveCriticalSection(&self->m_kLocker.m_tMutex);
}

// ── CTcpSocket::Send  (0x00E1A430) ────────────────────────────────────────────
// Single non-blocking send(). EWOULDBLOCK/EINTR/ENOBUFS -> defer (tick+2, ret 0);
// any other error -> drop connection. Recovers m_iNetworkState 101 -> 0.
int __fastcall hkSend(CTcpSocket* self, u32 /*edx*/, char* buf, int len) {
    if (self->m_skSocket == INVALID_SOCKET || !self->m_bConnected)
        return -1;

    if (len <= 0) {
        // [orig: diag log] "error: packet size is 0"
        return -1;
    }

    const int n = send(self->m_skSocket, buf, len, self->m_iSendFlags);
    if (n <= 0) {
        const int err = WSAGetLastError();
        if (err == kWSAEWOULDBLOCK || err == kWSAEINTR || err == kWSAENOBUFS) {
            self->m_uliTick = hTickMSec() + 2;
            return 0;
        }
        // [orig: diag log] "OnSend FAILED" (only when m_iNetworkState == 0)
        self->m_bConnected = false;
        return -1;
    }

    const u32 tick = hTickTick();
    const bool recovering = (self->m_iNetworkState == 101);
    self->m_TickMs = tick;
    if (recovering) {
        self->m_iNetworkState = 0;
        // [orig: diag log] "recovery [EHOSTUNREACH]"
    }
    return n;
}

// ── CTcpSocket::SendTrust  (0x00E196A0) ───────────────────────────────────────
// Blocking-ish reliable send: loops until `len` bytes are sent, tolerating
// EWOULDBLOCK/EINTR/ENOBUFS with Sleep(0) up to 3 spins per stall.
int __fastcall hkSendTrust(CTcpSocket* self, u32 /*edx*/, char* buf, int len) {
    if (self->m_skSocket == INVALID_SOCKET || !self->m_bConnected)
        return -1;

    if (len <= 0) {
        // [orig: diag log] "error: packet size is 0"
        self->m_bConnected = false;
        return -1;
    }

    int sent = send(self->m_skSocket, buf, len, self->m_iSendFlags);
    if (sent == len)
        return len;

    if (sent <= 0) {
        const int err = WSAGetLastError();
        if (err != kWSAEWOULDBLOCK && err != kWSAEINTR && err != kWSAENOBUFS) {
            self->m_bConnected = false;
            return -1;
        }
        sent = 0;
        Sleep(0);
    }

    int spins = 0;
    do {
        const int n = send(self->m_skSocket, &buf[sent], len - sent, self->m_iSendFlags);
        if (n > 0) {
            sent += n;
            if (sent >= len)
                return len;
        } else {
            const int err = WSAGetLastError();
            if (err != kWSAEWOULDBLOCK && err != kWSAEINTR && err != kWSAENOBUFS) {
                self->m_bConnected = false;
                return -1;
            }
            ++spins;
            Sleep(0);
        }
    } while (spins < 3);

    self->m_bConnected = false;
    return -1;
}

// ── CTcpSocket::Read  (0x00E1A7D0) ────────────────────────────────────────────
// Non-blocking recv(). Maps WSA error classes to back-off ticks (1/5/10ms) and
// returns 0 (try-again), or drops the connection on a hard error.
int __fastcall hkRead(CTcpSocket* self, u32 /*edx*/, char* buf, int len) {
    if (self->m_skSocket == INVALID_SOCKET)
        return static_cast<int>(self->m_skSocket);

    const int n = recv(self->m_skSocket, buf, len, self->m_iRecvFlags);
    if (n > 0) {
        const u32 tick = hTickTick();
        const bool recovering = (self->m_iNetworkState == 101);
        self->m_eNatType = static_cast<NatType>(tick); // [orig writes GetTick() into m_eNatType here]
        if (recovering) {
            self->m_iNetworkState = 0;
            // [orig: diag log] "recovery [EHOSTUNREACH]"
        }
        return n;
    }

    const int err = WSAGetLastError();
    if (err == kWSAEWOULDBLOCK || err == kWSAEINTR || err == kWSAENOBUFS) {
        self->m_uliTick = hTickMSec() + 1;
        return 0;
    }
    if (err == kWSAETIMEDOUT || err == kWSAEINVAL || err == kWSAECONNABORTED) {
        self->m_uliTick = hTickMSec() + 5;
        return 0;
    }
    if (err == kWSAENETUNREACH || err == kWSAEHOSTUNREACH) {
        if (!self->m_iNetworkState) {
            // [orig: diag log] "[EHOSTUNREACH] errno"
            self->m_iNetworkState = 101;
            self->m_uliTick = hTickMSec() + 10;
            return 0;
        }
        self->m_uliTick = hTickMSec() + 5;
        return 0;
    }

    // [orig: diag log] "OnRead FAILED" (only when m_iNetworkState == 0)
    self->m_bConnected = false;
    return -1;
}

// ── CTcpSocket::SendQueue  (0x00E19890) ───────────────────────────────────────
// The send pump: drains m_kBlockQueue. Each queued CTcpPacketBuffer holds
// [m_stFrontOffset, m_stSize) of m_acBuffer still to go out. A buffer is Pop()'d
// only once fully sent; a partial/blocked send advances m_stFrontOffset and
// shrinks m_stSize so the next pump resumes mid-buffer. Returns 1 = keep going
// (or just throttled), 0 = connection dropped.
char __fastcall hkSendQueue(CTcpSocket* self, u32 /*edx*/) {
    if (self->m_skSocket == INVALID_SOCKET || !self->m_bConnected)
        return 0;

    // Back-off gate: do nothing until the scheduled retry tick elapses.
    if (hTickMSec() < self->m_uliTick)
        return 1;

    EnterCriticalSection(&self->m_kLocker.m_tMutex);

    for (CTcpPacketBuffer* buf = hBlockQueuePeek(&self->m_kBlockQueue); buf;
         buf = hBlockQueuePeek(&self->m_kBlockQueue)) {
        char* data = buf->m_acBuffer + buf->m_stFrontOffset;
        const int len = static_cast<int>(buf->m_stSize);
        if (len <= 0) {
            hBlockQueuePop(&self->m_kBlockQueue);
            continue;
        }

        const int n = send(self->m_skSocket, data, len, self->m_iSendFlags);

        if (n == len) {                               // whole buffer flushed
            if (self->m_iNetworkState == 101) self->m_iNetworkState = 0; // [orig: recovery log]
            self->m_TickMs = hTickTick();
            hBlockQueuePop(&self->m_kBlockQueue);
            continue;
        }

        if (n <= 0) {                                 // nothing went out
            const int err = WSAGetLastError();
            if (err == kWSAEWOULDBLOCK || err == kWSAEINTR || err == kWSAENOBUFS) {
                self->m_uliTick = hTickMSec() + 2;
                LeaveCriticalSection(&self->m_kLocker.m_tMutex);
                return 1;
            }
            if (err == 109 || err == kWSAETIMEDOUT || err == kWSAECONNABORTED) {
                const u32 st = self->m_iNetworkState;
                if (st == 0 || st == 101) {
                    self->m_uliTick = hTickMSec() + 10;
                    LeaveCriticalSection(&self->m_kLocker.m_tMutex);
                    return 1;
                }
                self->m_bConnected = false;
                LeaveCriticalSection(&self->m_kLocker.m_tMutex);
                return 0;
            }
            if (err == kWSAENETUNREACH || err == kWSAEHOSTUNREACH) {
                const u32 st = self->m_iNetworkState;
                if (st == 0) {
                    // [orig: EHOSTUNREACH log]
                    self->m_iNetworkState = 101;
                    self->m_uliTick = hTickMSec() + 20;
                    LeaveCriticalSection(&self->m_kLocker.m_tMutex);
                    return 1;
                }
                if (st == 101) {
                    self->m_uliTick = hTickMSec() + 10;
                    LeaveCriticalSection(&self->m_kLocker.m_tMutex);
                    return 1;
                }
                self->m_bConnected = false;
                LeaveCriticalSection(&self->m_kLocker.m_tMutex);
                return 0;
            }
            // [orig: SendKeepedData FAILED log when m_iNetworkState == 0]
            self->m_bConnected = false;
            LeaveCriticalSection(&self->m_kLocker.m_tMutex);
            return 0;
        }

        // Partial send (0 < n < len): finish the remainder, persisting progress
        // into the buffer on any further block so the next pump can resume.
        if (self->m_iNetworkState == 101) self->m_iNetworkState = 0; // [orig: recovery log]
        self->m_TickMs = hTickTick();
        int sent = n;
        bool dropped = false;
        for (;;) {
            const int m = send(self->m_skSocket, data + sent, len - sent, self->m_iSendFlags);
            if (m > 0) {
                if (self->m_iNetworkState == 101) self->m_iNetworkState = 0; // [orig: recovery log]
                sent += m;
                self->m_TickMs = hTickTick();
                if (sent == len) {
                    hBlockQueuePop(&self->m_kBlockQueue);
                    break;                            // -> outer re-peek
                }
                continue;
            }

            const int err = WSAGetLastError();
            const auto persist = [&] {
                if (sent > 0) { buf->m_stFrontOffset += sent; buf->m_stSize = len - sent; }
            };
            if (err == kWSAEWOULDBLOCK || err == kWSAEINTR || err == kWSAENOBUFS) {
                persist();
                self->m_uliTick = hTickMSec() + 2;
                LeaveCriticalSection(&self->m_kLocker.m_tMutex);
                return 1;
            }
            if (err == 109 || err == kWSAETIMEDOUT || err == kWSAECONNABORTED) {
                const u32 st = self->m_iNetworkState;
                if (st != 0 && st != 101) { dropped = true; break; }
                persist();
                self->m_uliTick = hTickMSec() + 10;
                LeaveCriticalSection(&self->m_kLocker.m_tMutex);
                return 1;
            }
            if (err == kWSAENETUNREACH || err == kWSAEHOSTUNREACH) {
                const u32 st = self->m_iNetworkState;
                if (st == 0) {
                    // [orig: EHOSTUNREACH log]
                    self->m_iNetworkState = 101;
                    persist();
                    self->m_uliTick = hTickMSec() + 20;
                    LeaveCriticalSection(&self->m_kLocker.m_tMutex);
                    return 1;
                }
                if (st != 101) { dropped = true; break; }
                persist();
                self->m_uliTick = hTickMSec() + 10;
                LeaveCriticalSection(&self->m_kLocker.m_tMutex);
                return 1;
            }
            // [orig: SendKeepedData FAILED log when m_iNetworkState == 0]
            dropped = true;
            break;
        }
        if (dropped) {
            self->m_bConnected = false;
            LeaveCriticalSection(&self->m_kLocker.m_tMutex);
            return 0;
        }
        // partial buffer fully drained -> loop re-peeks the next one
    }

    LeaveCriticalSection(&self->m_kLocker.m_tMutex);
    return 1;
}

// ── CTcpSocket::Listen  (0x00E19370) ──────────────────────────────────────────
// Server-side bind+listen (backlog 1280). Uses the vtable Create/Disconnect so
// it still routes through any installed detours. Returns the socket or -1.
SOCKET __fastcall hkListen(CTcpSocket* self, u32 /*edx*/, char* addr_, int port) {
    if (port <= 0) {
        // [orig: diag log] "port is wrong"
        return static_cast<SOCKET>(-1);
    }

    auto vCreate = reinterpret_cast<int(__thiscall*)(CTcpSocket*)>(self->_vptr_CRawSocket->Create);
    if (!vCreate(self))
        return static_cast<SOCKET>(-1);

    hRawSetLocalAddress(self, addr_, port);
    hRawSetSocketOption(self, 4, 1); // 4 = SO_REUSEADDR

    if (bind(self->m_skSocket, reinterpret_cast<const sockaddr*>(&self->m_saLocalAddr_in), self->m_slSize) == 0) {
        if (listen(self->m_skSocket, 1280) == 0)
            return self->m_skSocket;
        // [orig: diag log] "listen() failed"
    } else {
        // [orig: diag log] "bind() failed"
    }

    auto vDisconnect = reinterpret_cast<void(__thiscall*)(CTcpSocket*)>(self->_vptr_CRawSocket->Disconnect);
    vDisconnect(self);
    return static_cast<SOCKET>(-1);
}

// ── CTcpSocket::Accept  (0x00E195C0) ──────────────────────────────────────────
SOCKET __fastcall hkAccept(CTcpSocket* self, u32 /*edx*/) {
    SOCKET r = accept(self->m_skSocket,
                      reinterpret_cast<sockaddr*>(&self->m_saRemoteAddr_in),
                      &self->m_slSize);
    if (!r)                       // [orig checks == 0 explicitly]
        return static_cast<SOCKET>(-1);
    return r;
}

} // namespace

// ── Detour install (intentionally inert — every .create is commented out) ─────
void InstallTcpSocketEngine() {
    // Swap-in is staged but DISABLED. Uncomment to route the client's CTcpSocket
    // through these reimplementations once the full TCP path is reversed.
    //
    // auto& reg = mg::ctx().hookRegistry();
    // reg.registerDetour(HookId::TcpSocket_CreateSocket  ).create(MG_CONST(addr::net::tcp_socket::CreateSocket),   hkCreateSocket);
    // reg.registerDetour(HookId::TcpSocket_SendBlock      ).create(MG_CONST(addr::net::tcp_socket::SendBlock),      hkSendBlock);
    // reg.registerDetour(HookId::TcpSocket_ClearSendQueue ).create(MG_CONST(addr::net::tcp_socket::ClearSendQueue), hkClearSendQueue);
    // reg.registerDetour(HookId::TcpSocket_Send           ).create(MG_CONST(addr::net::tcp_socket::Send),           hkSend);
    // reg.registerDetour(HookId::TcpSocket_SendTrust      ).create(MG_CONST(addr::net::tcp_socket::SendTrust),      hkSendTrust);
    // reg.registerDetour(HookId::TcpSocket_Read           ).create(MG_CONST(addr::net::tcp_socket::Read),           hkRead);
    // reg.registerDetour(HookId::TcpSocket_SendQueue      ).create(MG_CONST(addr::net::tcp_socket::SendQueue),      hkSendQueue);
    // reg.registerDetour(HookId::TcpSocket_Listen         ).create(MG_CONST(addr::net::tcp_socket::Listen),         hkListen);
    // reg.registerDetour(HookId::TcpSocket_Accept         ).create(MG_CONST(addr::net::tcp_socket::Accept),         hkAccept);
    //
    // ctor/dtor (0x00E19180 / 0x00E19200) are construction plumbing — only needed
    // when swapping the vtable wholesale; deferred to the engine-wiring step.
    (void)0;
}

} // namespace mg::game::net
