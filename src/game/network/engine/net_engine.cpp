// =============================================================================
// Network engine swap — CNetEngine + per-frame pump  [see .hpp]
// =============================================================================
// Reversed 1:1 from MicroVolts.exe.i64:
//   CNetEngine::Initialize  0x00E12750 (SENSOR, CMD, size, NETWORK, a6)
//   CNetEngine::Release     0x00E12330
//   CNetEngine::Clear       0x00E12390 -> cmdQueue->Clear (q vtbl[7])
//   CNetEngine::RecvPump    0x00E12310 -> model->Select(0)        (model vtbl[4])
//   CNetEngine::GetCommand  0x00E12300 -> cmdQueue->Get           (q vtbl[4])
//   CNetEngine::SendPump    0x00E12320 -> model->Run              (model vtbl[5])
//   model factory           0x00E123B0 (SENSOR_TYPE, count) -> CEventSelectModel/CPoll/CEpoll
//   game drain+route        0x00AB42E0 -> cmdQueue.Get -> PacketsCallbackDistribution
//
// CNetEngine layout: +0x04 cmdQueue, +0x08 sensor(model), +0x0C bInitialized.
// SENSOR_TYPE: 1/default -> CEventSelectModel (0x318), 2 -> CPollModel (0x10),
// 3 -> CEpollModel (0x10). COMMAND_TYPE != 0 -> Extend queue (else Native).
// =============================================================================
#include "pch.hpp"
#include "game/network/engine/net_engine.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "utils/call_helper.hpp"

namespace mg::game::net {
namespace {

namespace en = addr::net::engine;

// ── model / queue vtable wrappers ─────────────────────────────────────────────
inline void* hModel(CNetEngine* e) { return e->m_pkSensor; }

inline int hModelSelect(void* model, DWORD timeout) {
    auto** vt = *reinterpret_cast<void***>(model);
    return reinterpret_cast<int(__thiscall*)(void*, DWORD)>(vt[4])(model, timeout);
}
inline int hModelRun(void* model) {
    auto** vt = *reinterpret_cast<void***>(model);
    return reinterpret_cast<int(__thiscall*)(void*)>(vt[5])(model);
}
inline void hQueueClear(ICommandQueue* q) {
    auto** vt = *reinterpret_cast<void***>(q);
    reinterpret_cast<void(__thiscall*)(ICommandQueue*)>(vt[7])(q);
}
inline CExtendCommand* hQueueGet(ICommandQueue* q) {
    auto** vt = *reinterpret_cast<void***>(q);
    return reinterpret_cast<CExtendCommand*(__thiscall*)(ICommandQueue*)>(vt[4])(q);
}
inline bool hQueueIsEmpty(ICommandQueue* q) {
    auto** vt = *reinterpret_cast<void***>(q);
    return reinterpret_cast<int(__thiscall*)(ICommandQueue*)>(vt[2])(q) != 0;
}

// ── CNetEngine::RecvPump  (0x00E12310, vtbl[4]) ───────────────────────────────
// Per-frame recv poll: drive the model's WSAEventSelect wait loop (timeout 0).
int __fastcall hkRecvPump(CNetEngine* self, u32 /*edx*/, DWORD timeout) {
    return hModelSelect(hModel(self), timeout); // game passes 0
}

// ── CNetEngine::SendPump  (0x00E12320, vtbl[6]) ───────────────────────────────
// Per-frame send/keepalive pump: model->Run -> CDispatcherArray::Run -> Process.
int __fastcall hkSendPump(CNetEngine* self, u32 /*edx*/) {
    return hModelRun(hModel(self));
}

// ── CNetEngine::GetCommand  (0x00E12300, vtbl[5]) ─────────────────────────────
CExtendCommand* __fastcall hkGetCommand(CNetEngine* self, u32 /*edx*/) {
    return hQueueGet(self->m_pkCommandQueue);
}

// ── CNetEngine::Clear  (0x00E12390, vtbl[3]) ──────────────────────────────────
void __fastcall hkClear(CNetEngine* self, u32 /*edx*/) {
    if (self->m_pkCommandQueue)
        hQueueClear(self->m_pkCommandQueue);
}

// ── model factory  (sub_E123B0) ───────────────────────────────────────────────
// Replaces the model: type 2 -> CPollModel, 3 -> CEpollModel, else
// CEventSelectModel; then model->Init(count, cmdQueue) (vtbl[1]).
bool hkBuildModel(CNetEngine* self, int sensorType, int count) {
    // tear down any existing model
    if (self->m_pkSensor) {
        auto** vt = *reinterpret_cast<void***>(self->m_pkSensor);
        reinterpret_cast<void(__thiscall*)(void*)>(vt[2])(self->m_pkSensor);
        reinterpret_cast<void(__thiscall*)(void*, int)>(vt[0])(self->m_pkSensor, 1);
        self->m_pkSensor = nullptr;
    }

    void* model = nullptr;
    if (sensorType == 2 || sensorType == 3) {
        // CPollModel / CEpollModel are 0x10 placeholders (not used on Windows).
        model = mg::call<void*(__cdecl*)(u32)>(addr::msvc::operator_new, 0x10u);
        // [orig sets the matching vftable + zeroes 3 fields; deferred to vtable swap]
    } else {
        model = mg::call<void*(__cdecl*)(u32)>(addr::msvc::operator_new, addr::net::model::AllocSize);
        if (model)
            model = mg::call<void*(__thiscall*)(void*)>(addr::net::model::Ctor, model);
    }
    self->m_pkSensor = model;
    if (!model) {
        // [orig: diag log] "netmodel is null"
        return false;
    }
    // model->Init(count, cmdQueue)  — vtbl[1]
    auto** vt = *reinterpret_cast<void***>(model);
    reinterpret_cast<void(__thiscall*)(void*, int, void*)>(vt[1])(model, count, self->m_pkCommandQueue);
    return true;
}

// ── CNetEngine::Initialize  (0x00E12750, vtbl[1]) ─────────────────────────────
// Builds the command queue + net model and arms the sensor. Returns 1 on success.
char __fastcall hkInitialize(CNetEngine* self, u32 /*edx*/,
                             int sensorType, int commandType, int size, int networkType, int a6) {
    if (self->m_bInitialized) {
        // [orig: diag log] "engine was Initialize()" + Release()
        auto** vt = *reinterpret_cast<void***>(self);
        reinterpret_cast<void(__thiscall*)(CNetEngine*)>(vt[2])(self); // Release
    }
    if (!mg::call<int(__cdecl*)()>(addr::net::factory::Get) /*g_kConnectorFactory*/) {
        // (orig checks factory+32; simplified guard — see note) [diag log]
    }
    if (!size) {
        // [orig: diag log] "size is 0"
        return 0;
    }

    WSADATA wsa{};
    WSAStartup(0x0202u, &wsa);

    // command queue: Extend (CMD != 0) or Native
    void* qraw = mg::call<void*(__cdecl*)(u32)>(addr::msvc::operator_new, addr::net::command_queue::CmdQueueAlloc);
    void* queue = nullptr;
    if (commandType) {
        queue = qraw ? mg::call<void*(__thiscall*)(void*)>(addr::net::command_queue::ExCtor, qraw) : nullptr;
    } else {
        queue = qraw ? mg::call<void*(__thiscall*)(void*)>(addr::net::command_queue::NaCtor, qraw) : nullptr;
    }
    self->m_pkCommandQueue = static_cast<ICommandQueue*>(queue);

    if (!hkBuildModel(self, sensorType, size)) {
        // [orig: diag log] "net model create failed" + free queue + WSACleanup
        if (self->m_pkCommandQueue) {
            auto** qvt = *reinterpret_cast<void***>(self->m_pkCommandQueue);
            reinterpret_cast<void(__thiscall*)(void*, int)>(qvt[0])(self->m_pkCommandQueue, 1);
            self->m_pkCommandQueue = nullptr;
        }
        WSACleanup();
        return 0;
    }

    // arm the sensor: SetNetType (vtbl[6]) then vtbl[7](a6).
    void* model = self->m_pkSensor;
    auto** vt = *reinterpret_cast<void***>(model);
    constexpr int kNetworkServerBegin = 0; // NETWORK_SERVER_BEGIN
    constexpr int kNetworkUdpServer   = 1; // NETWORK_UDP_SERVER (enum-relative)
    int netArg = (networkType == kNetworkServerBegin) ? 0
               : (networkType == kNetworkUdpServer)   ? 1
               : networkType;
    reinterpret_cast<void(__thiscall*)(void*, int)>(vt[6])(model, netArg); // SetNetType
    reinterpret_cast<void(__thiscall*)(void*, int)>(vt[7])(model, a6);
    self->m_bInitialized = 1;
    return 1;
}

// ── game-layer drain + route  (sub_AB42E0 mirror) ─────────────────────────────
// Per-frame: pop every queued command and dispatch it to the protocol handlers
// via PacketsCallbackDistribution, freeing each command afterward. This is the
// piece that turns received bytes into handler calls — it lives in the game layer,
// NOT in CDispatcher.
void hkDrainAndRoute(CNetEngine* engine) {
    ICommandQueue* q = engine->m_pkCommandQueue;
    if (!q || hQueueIsEmpty(q))
        return;

    while (CExtendCommand* cmd = hQueueGet(q)) {
        mg::call<char(__stdcall*)(void*)>(addr::net::netmgr::PacketsCallbackDistribution, &cmd->m_kPacket);
        // free the command (scalar deleting dtor, vtbl[0]).
        auto** vt = *reinterpret_cast<void***>(cmd);
        reinterpret_cast<void(__thiscall*)(void*, int)>(vt[0])(cmd, 1);
    }
}

} // namespace

// ── Detour install (inert — every .create is commented out) ───────────────────
void InstallNetEngine() {
    // auto& reg = mg::ctx().hookRegistry();
    // reg.registerDetour(HookId::Engine_Initialize).create(MG_CONST(addr::net::engine::Initialize), hkInitialize);
    // reg.registerDetour(HookId::Engine_RecvPump  ).create(MG_CONST(addr::net::engine::RecvPump),   hkRecvPump);
    // reg.registerDetour(HookId::Engine_SendPump  ).create(MG_CONST(addr::net::engine::SendPump),   hkSendPump);
    // reg.registerDetour(HookId::Engine_GetCommand).create(MG_CONST(addr::net::engine::GetCommand), hkGetCommand);
    // reg.registerDetour(HookId::Engine_Clear     ).create(MG_CONST(addr::net::engine::Clear),      hkClear);
    // reg.registerDetour(HookId::Engine_DrainRoute).create(MG_CONST(addr::net::netmgr::DrainRoute), hkDrainAndRoute);
    //
    // Release (0x00E12330) + ctor (0x00E12730) are construction plumbing; deferred
    // to the engine-wiring step. The factory guard in Initialize is simplified —
    // the original tests g_kConnectorFactory+0x20 (see addr::net::factory).
    (void)hkInitialize; (void)hkRecvPump; (void)hkSendPump; (void)hkGetCommand;
    (void)hkClear; (void)hkDrainAndRoute;
}

} // namespace mg::game::net
