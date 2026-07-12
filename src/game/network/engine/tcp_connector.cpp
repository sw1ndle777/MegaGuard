// =============================================================================
// Network engine swap — CConnector / CTcpConnector send path  [see .hpp]
// =============================================================================
// Reversed 1:1 from MicroVolts.exe.i64:
//   Send        0x00E13430   (dispatch: arg8==1 -> SendObsolete, else SendSafty)
//   SendSafty   0x00E12C20   (build header, vtable Encrypt, RC5 header, SendBlock)
//
// Wire framing (per SendSafty): a packet is [ STcpPacketHeader(4) ][ body ]
//   * body = vtable Encrypt(connector, a4, cmd, body, a3+4) over the CCommand
//     (SCommandHeader(4) + data(a3)), i.e. a3+4 bytes.
//   * header is a 32-bit bitfield built with raw integer ops (kept verbatim):
//       hdr  = 16 * ((a4 << 25) | (sessionId & 0x3FFF));
//       hdr ^= (hdr ^ ((a3 + 8) << 18)) & 0x1FFC0000;   // splice in total size
//     then RC5-encrypted (4 bytes) when m_bHeaderCrypt.
//   * total sent = a3 + 8 (header 4 + encrypted command a3+4); cap a3+4 <= 1432.
//
// Notes: header crypt uses mg::crypto::CCrypt(CRYPT_RC5, 0) — the 1:1 port of the
// client's CCrypt RC5 path (matches CryptType::CRYPT_RC5 == 1). The CTcpPacket
// stack object the original constructs is pure scratch (only a3+4 body bytes are
// ever sent), so it's modeled as a flat buffer; its ctor is intentionally omitted.
// =============================================================================
#include "pch.hpp"
#include "game/network/engine/tcp_connector.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "game/network/ccrypt.hpp"
#include "utils/call_helper.hpp"

namespace mg::game::net {
namespace {

// ── Leaf-helper call wrappers ─────────────────────────────────────────────────
inline u32 hTickTick() {
    void* inst = mg::call<void*(__cdecl*)()>(addr::net::helpers::TickGetInstance);
    return mg::call<u32(__thiscall*)(void*)>(addr::net::helpers::TickGetTick, inst);
}
inline size_t hStreamBufferMove(CStreamBuffer* sb, int consumed) {
    return mg::call<size_t(__thiscall*)(CStreamBuffer*, int)>(addr::net::helpers::StreamBufferMove, sb, consumed);
}
inline void hStreamBufferClear(CStreamBuffer* sb) {
    mg::call<void(__thiscall*)(CStreamBuffer*)>(addr::net::helpers::StreamBufferClear, sb);
}

// On-the-wire scratch: [hdr (4 bytes)][body]. Encrypt writes into `body`;
// SendBlock sends from `&hdr` for (a3 + 8) bytes.
#pragma pack(push, 1)
struct SendBuf {
    u32  hdr;
    char body[1432]; // max a3+4 (SendSafty caps a3+4 <= 1432)
};
#pragma pack(pop)

// ── CTcpConnector::SendSafty  (0x00E12C20) ────────────────────────────────────
int __fastcall hkSendSafty(CTcpConnector* self, u32 /*edx*/, CCommand* cmd, int a3, int a4) {
    EnterCriticalSection(&self->m_kLocker.m_tMutex);

    const int total = a3 + 4; // CCommand size to encrypt (SCommandHeader + data)
    if (static_cast<unsigned>(total) > 1432) {
        // [orig: diag log] "SendSafty() is failed: data is too big size"
        LeaveCriticalSection(&self->m_kLocker.m_tMutex);
        return 0;
    }

    // socket->dword0070 low byte gates whether the link is ready to enqueue.
    if (!static_cast<u8>(self->m_pkSocket->dword0070)) {
        LeaveCriticalSection(&self->m_kLocker.m_tMutex);
        return -2;
    }

    const u16 sid = self->m_iSessionId;

    SendBuf buf;
    buf.hdr = 16u * ((static_cast<u32>(a4) << 25) | (sid & 0x3FFFu));

    // Encrypt the command into the body (vtable -> routes through any hook).
    self->_vptr_CConnector->Encrypt(self, a4, cmd, buf.body, total);

    // Splice the total wire size (a3 + 8) into the header bitfield (bits 18..28).
    buf.hdr ^= (buf.hdr ^ (static_cast<u32>(a3 + 8) << 18)) & 0x1FFC0000u;

    if (self->m_bHeaderCrypt) {
        mg::crypto::CCrypt crypt(mg::crypto::CCrypt::CryptType::CRYPT_RC5, 0);
        crypt.encrypt(reinterpret_cast<u8*>(&buf.hdr), reinterpret_cast<u8*>(&buf.hdr), 4);
    }

    const int r = reinterpret_cast<int(__thiscall*)(CTcpSocket*, void*, size_t)>(
        self->m_pkSocket->_vptr_CRawSocket->SendBlock)(self->m_pkSocket, &buf, static_cast<size_t>(a3 + 8));

    LeaveCriticalSection(&self->m_kLocker.m_tMutex);
    return r;
}

// ── CTcpConnector::Send  (0x00E13430) ─────────────────────────────────────────
// Thin dispatcher: arg8==1 selects the "obsolete" (unframed/relay) path via the
// vtable; otherwise the safe framed path above.
int __fastcall hkConnectorSend(CTcpConnector* self, u32 /*edx*/, void* cmd, int a3, int arg8, int a4) {
    if (arg8 == 1)
        return self->_vptr_CConnector->SendObsolete(self, reinterpret_cast<CCommand*>(cmd), a3, a4);
    return hkSendSafty(self, 0, reinterpret_cast<CCommand*>(cmd), a3, a4);
}

// ── CConnector::Read  (0x00E13470) ────────────────────────────────────────────
// Recv pump + frame parser. Reassembles the stream in m_kStreamBuffer:
//   * each packet starts with a 4-byte header (RC5-decrypted in place when
//     m_bHeaderCrypt); its wire size is bits 18..28 -> (hdr >> 18) & 0x7FF.
//   * size == 4  -> header-only/keepalive (consume 4, continue).
//   * size >= 8  -> validate via vtable CheckRecvHeader, and if the whole packet
//     is buffered, hand it to the vtable Queuing (sub_E13940) which enqueues the
//     command for the dispatcher thread to route. Otherwise re-encrypt the header
//     and refill from the socket.
//   * consumed bytes are compacted with CStreamBuffer::Move; the buffer is topped
//     up via the socket's (possibly hooked) Read. Updates socket->m_eNatType with
//     the last-activity tick. Returns 1 = ok/again, 0 = error (sets m_iNetworkState).
char __fastcall hkConnectorRead(CTcpConnector* self, u32 /*edx*/) {
    if (!static_cast<u8>(self->m_pkSocket->dword0070))
        return 0;

    int n4       = static_cast<int>(self->m_kStreamBuffer.m_stSize);
    int consumed = 0;
    int iRet     = 0;

    if (n4 < 4)
        goto REFILL;

    for (;;) {
        // ── inner: drain complete packets already in the buffer ──
        for (;;) {
            char* p = self->m_kStreamBuffer.m_acBuffer + consumed;

            if (self->m_bHeaderCrypt) {
                mg::crypto::CCrypt c(mg::crypto::CCrypt::CryptType::CRYPT_RC5, 0);
                c.decrypt(reinterpret_cast<u8*>(p), reinterpret_cast<u8*>(p), 4);
            }

            const u32 size = (*reinterpret_cast<u32*>(p) >> 18) & 0x7FFu;

            if (size >= 8) {
                if (!reinterpret_cast<u8(__thiscall*)(CTcpConnector*, void*)>(
                        self->_vptr_CConnector->CheckRecvHeader)(self, p)) {
                    self->m_pkSocket->m_iNetworkState = 7;
                    // [orig: diag log] "packet is invalid"
                    return 0;
                }
                if (static_cast<int>(size) <= n4) {              // full packet -> enqueue
                    if (!reinterpret_cast<u8(__thiscall*)(CTcpConnector*, void*)>(
                            self->_vptr_CConnector->sub_E13940)(self, p)) {
                        // [orig: diag log] "packet queue is full"
                        return 0;
                    }
                    consumed += static_cast<int>(size);
                    n4       -= static_cast<int>(size);
                    if (n4 < 4) goto REFILL;
                    continue;
                }
                // partial packet -> restore header bytes, then refill
                if (self->m_bHeaderCrypt) {
                    mg::crypto::CCrypt c(mg::crypto::CCrypt::CryptType::CRYPT_RC5, 0);
                    c.encrypt(reinterpret_cast<u8*>(p), reinterpret_cast<u8*>(p), 4);
                }
                goto REFILL;
            }

            if (size != 4) {                                     // invalid size field
                self->m_pkSocket->m_iNetworkState = 7;
                // [orig: diag log] "packet header is invalid"
                return 0;
            }
            consumed += 4;                                       // header-only / keepalive
            n4       -= 4;
            if (n4 < 4) goto REFILL;
        }

    REFILL:
        if (consumed > 0) {
            n4 = static_cast<int>(hStreamBufferMove(&self->m_kStreamBuffer, consumed));
            consumed = 0;
        }
        if (self->m_kStreamBuffer.m_stMax != self->m_kStreamBuffer.m_stSize) {
            iRet = reinterpret_cast<int(__thiscall*)(CTcpSocket*, char*, int)>(
                self->m_pkSocket->_vptr_CRawSocket->Read)(
                    self->m_pkSocket,
                    self->m_kStreamBuffer.m_acBuffer + self->m_kStreamBuffer.m_stSize,
                    static_cast<int>(self->m_kStreamBuffer.m_stMax - self->m_kStreamBuffer.m_stSize));
            if (iRet <= 0)
                break;                                           // no data / error
            self->m_kStreamBuffer.m_stSize += static_cast<size_t>(iRet);
            n4 = static_cast<int>(self->m_kStreamBuffer.m_stSize);
            self->m_pkSocket->m_eNatType = static_cast<NatType>(hTickTick()); // last-activity tick
        }
        if (n4 < 4)
            return 1;
        // else: re-enter the inner parse loop
    }

    if (iRet != -1)
        return 1;
    if (!self->m_pkSocket->m_iNetworkState)
        self->m_pkSocket->m_iNetworkState = 2;
    return 0;
}

// ── Crypt method dispatch shared by Encrypt/Decrypt ───────────────────────────
// method (the CRYPT_TYPE passed in, == SendSafty's a4):
//   1 CRYPT_RC5  -> RC5, key 0           3 CRYPT_RC6        -> RC6, key 0
//   2 CRYPT_RC5_SERIAL -> RC5, serialKey 4 CRYPT_RC6_SERIAL -> RC6, serialKey
//   0/other (CRYPT_NONE) -> plain copy (mg CCrypt would RC5 it, so guard here).
inline void connectorCryptCopy(const void* in, void* out, int size) {
    if (in != out && size > 0) {
        auto* d = static_cast<u8*>(out);
        auto* s = static_cast<const u8*>(in);
        for (int i = 0; i < size; ++i) d[i] = s[i];
    }
}

// ── CConnector::Encrypt  (0x00E10FE0) ─────────────────────────────────────────
void __fastcall hkConnectorEncrypt(CConnector* self, u32 /*edx*/, u32 method, void* in, void* out, int size) {
    if (method < 1 || method > 4) { connectorCryptCopy(in, out, size); return; }
    const i32 key = (method == 2 || method == 4) ? self->m_iSerialKey : 0;
    mg::crypto::CCrypt c(static_cast<mg::crypto::CCrypt::CryptType>(method), key);
    c.encrypt(static_cast<u8*>(in), static_cast<u8*>(out), size);
}

// ── CConnector::Decrypt  (0x00E11170) ─────────────────────────────────────────
void __fastcall hkConnectorDecrypt(CConnector* self, u32 /*edx*/, u32 method, void* in, void* out, int size) {
    if (method < 1 || method > 4) { connectorCryptCopy(in, out, size); return; }
    const i32 key = (method == 2 || method == 4) ? self->m_iSerialKey : 0;
    mg::crypto::CCrypt c(static_cast<mg::crypto::CCrypt::CryptType>(method), key);
    c.decrypt(static_cast<u8*>(in), static_cast<u8*>(out), size);
}

// ── CTcpConnector::SetConnected  (0x00E13240) ─────────────────────────────────
// Post-connect reset: clears the recv stream buffer, re-arms the socket as
// non-blocking (FIONBIO), zeroes the keepalive-tick high dword then stamps both
// the socket activity tick and the keepalive low dword with the current tick.
u64 __fastcall hkSetConnected(CTcpConnector* self, u32 /*edx*/) {
    hStreamBufferClear(&self->m_kStreamBuffer);
    CTcpSocket* sock = self->m_pkSocket;

    // orig: *(this + 0x30) = 0  -> high dword of m_ullKeepAliveTick
    *(reinterpret_cast<u32*>(&self->m_ullKeepAliveTick) + 1) = 0;

    if (sock->m_skSocket == INVALID_SOCKET) {
        // [orig: diag log] "INVALID_SOCKET"
    } else {
        u_long argp = 1;
        ioctlsocket(sock->m_skSocket, FIONBIO, &argp); // orig literal -2147195266 == 0x8004667E
    }

    sock->m_eNatType = static_cast<NatType>(hTickTick()); // socket+0x5C activity tick

    const u32 t = hTickTick();
    *reinterpret_cast<u32*>(&self->m_ullKeepAliveTick) = t; // orig: *(this + 0x2C) = result (low dword)
    return t;
}

// ── CConnector::Queuing  (sub_E13940) ─────────────────────────────────────────
// Bridge from the recv parser (Read) into the command queue. `a2` points at the
// raw packet (its first 4 bytes are the wire STcpPacketHeader):
//   size      = ((hdr >> 18) & 0x7FF) - 4   // CCommand bytes (SCommandHeader+data)
//   cryptType = hdr >> 29                    // top 3 bits == send-side a4 (crypt method)
// Stamps the socket recv-activity tick, then hands the command body
// (a2->m_acData, i.e. packet+4) to ICommandQueue::Put (vtable index 5). The
// queue is drained + routed by the dispatcher thread (see dispatch.*).
int __fastcall hkQueuing(CConnector* self, u32 /*edx*/, CCommand* a2) {
    const u32 hdr      = *reinterpret_cast<u32*>(&a2->m_tHeader);
    const int size     = static_cast<int>((hdr >> 18) & 0x7FFu) - 4;
    const int cryptType = static_cast<int>(hdr >> 29);

    // socket recv-activity tick (orig writes GetTick into socket+0x5C). VERIFY-vs-IDA:
    // offset derived as CTcpSocket+0x5C (== m_eNatType slot, reused as a tick).
    self->m_pkSocket->m_eNatType = static_cast<NatType>(hTickTick());

    void** qVtbl = *reinterpret_cast<void***>(self->m_pkCommandQueue);
    return reinterpret_cast<int(__thiscall*)(void*, CConnector*, char*, int, int)>(qVtbl[5])(
        self->m_pkCommandQueue, self, a2->m_acData, size, cryptType);
}

} // namespace

// ── Detour install (inert — every .create is commented out) ───────────────────
void InstallTcpConnectorEngine() {
    // auto& reg = mg::ctx().hookRegistry();
    // reg.registerDetour(HookId::Connector_Send    ).create(MG_CONST(addr::net::connector::Send),     hkConnectorSend);
    // reg.registerDetour(HookId::Connector_SendSafty).create(MG_CONST(addr::net::connector::SendSafty), hkSendSafty);
    // reg.registerDetour(HookId::Connector_Read    ).create(MG_CONST(addr::net::connector::Read),     hkConnectorRead);
    // reg.registerDetour(HookId::Connector_Encrypt ).create(MG_CONST(addr::net::connector::Encrypt),  hkConnectorEncrypt);
    // reg.registerDetour(HookId::Connector_Decrypt ).create(MG_CONST(addr::net::connector::Decrypt),  hkConnectorDecrypt);
    // reg.registerDetour(HookId::Connector_SetConnected).create(MG_CONST(addr::net::connector::SetConnected), hkSetConnected);
    //
    // Still to reverse for the full connector layer: SendObsolete (0x00E12ED0),
    // KeepAlive (0x00E13990), CheckRecvHeader, queue resets.
    // Connect (0x00E13340) is deferred to the dispatch pass — it drives the
    // ISensor (vtable[8]=register / [9]=unregister) and the connector Disconnect
    // vtable slot, which must be mapped precisely there (not guessed).
    // Queuing (sub_E13940, called by Read) feeds the command queue; the queue
    // drain + CDispatcher routing to handlers is the dispatch task (#5).
    (void)0;
}

} // namespace mg::game::net
