// =============================================================================
// GameManagerBase - Implementation
// =============================================================================
#include "pch.hpp"
#include "game/managers/manager_base.hpp"
#include "engine/pointer_encryption.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "game/addresses.hpp"
#include "game/network/heartbeat_handler.hpp"
#include "anticheat/detection_engine.hpp"
#include "anticheat/detection_report.hpp"
#include "utils/logger.hpp"
#include "platform/ipc.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::game {

// ── Integrity tag (murmur-inspired mix of ciphertext + nonce + instance) ──────

MG_FORCEINLINE u32 computeIntegrityTag(
    uptr encVal, u32 nonce, uptr instanceAddr)
{
    u32 h = 0x6A09E667u;
    h ^= static_cast<u32>(encVal);        h *= 0x9E3779B1u; h = _rotl(h, 13);
    h ^= nonce;                           h *= 0x85EBCA77u; h = _rotl(h, 17);
    h ^= static_cast<u32>(instanceAddr);  h *= 0xC2B2AE3Du; h = _rotl(h, 11);
    h ^= h >> 16;
    return h;
}

MG_FORCEINLINE u32 freshNonce() {
    u32 n = static_cast<u32>(__rdtsc()) ^ GetTickCount();
    return n ? n : 0xDEADBEEFu;
}

// ── ManagerState special members (need complete PointerEncryption type) ───────

ManagerState::ManagerState() = default;
ManagerState::~ManagerState() = default;
ManagerState::ManagerState(ManagerState&&) noexcept = default;
ManagerState& ManagerState::operator=(ManagerState&&) noexcept = default;

// ── ManagerState lifetime ────────────────────────────────────────────────────

void ManagerState::init(uptr moduleBase, const char* debugName) {
    if (!initialized) {
        name = debugName;
        InitializeCriticalSection(&critSection);
        ptrEncryption = std::make_unique<::mg::PointerEncryption>(moduleBase);
        initialized = true;
    }
}

void ManagerState::shutdown() {
    if (initialized) {
        encrypted      = nullptr;
        integrityTag   = 0;
        accessNonce    = 0;
        vtableSnapshot = 0;
        ptrEncryption.reset();
        DeleteCriticalSection(&critSection);
        initialized = false;
    }
}

// ── Get logic ────────────────────────────────────────────────────────────────

u32* managerGetHook(
    u32 returnAddress,
    ManagerState& state,
    uptr initAddr,
    u32 allocSize)
{
    auto& context = mg::ctx();
    auto& log     = context.logger();
    u32*  realPtr = nullptr;

    EnterCriticalSection(&state.critSection);

    if (!state.encrypted) {
        // First-time initialization
        auto* mem = new (std::nothrow) u32[allocSize];
        if (!mem) {
            log.error("[MGR:{}] ALLOC_FAIL size={}", state.name, allocSize);
            LeaveCriticalSection(&state.critSection);
            return nullptr;
        }

        realPtr = mg::call<u32*(__thiscall*)(void*)>(initAddr, mem);

        state.vtableSnapshot = *realPtr;

        u32  nonce   = freshNonce();
        uptr coreEnc = state.ptrEncryption->process(reinterpret_cast<uptr>(realPtr));
        uptr stored  = coreEnc ^ nonce;

        state.encrypted    = reinterpret_cast<u32*>(stored);
        state.accessNonce  = nonce;
        state.integrityTag = computeIntegrityTag(
            stored, nonce, reinterpret_cast<uptr>(state.ptrEncryption.get()));

        log.info("[MGR:{}] INIT ptr={:08X} vtbl={:08X} enc={:08X} nonce={:08X} tag={:08X}",
                 state.name, reinterpret_cast<uptr>(realPtr),
                 state.vtableSnapshot, stored, nonce, state.integrityTag);

        context.ipcClient().setManagerReady(1, 4);
    }
    else {
        // Verify integrity — detects external tampering with the stored pointer
        uptr stored      = reinterpret_cast<uptr>(state.encrypted);
        u32  expectedTag = computeIntegrityTag(
            stored, state.accessNonce,
            reinterpret_cast<uptr>(state.ptrEncryption.get()));

        if (expectedTag != state.integrityTag) {
            log.error("[MGR:{}] INTEGRITY_FAIL stored={:08X} nonce={:08X} expected={:08X} actual={:08X}",
                      state.name, stored, state.accessNonce, expectedTag, state.integrityTag);
            context.heartbeatHandler().detectionReport().report(
                DetectionFlag::kIntegrityViolation, static_cast<u32>(stored));
            LeaveCriticalSection(&state.critSection);
            return nullptr;
        }

        // Decrypt: undo nonce layer, then process() (self-inverse) recovers raw ptr
        realPtr = state.ptrEncryption->decrypt<u32*>(stored ^ state.accessNonce);

        // Rolling re-encrypt: fresh nonce changes the stored ciphertext every access
        u32  newNonce   = freshNonce();
        uptr coreEnc    = stored ^ state.accessNonce;
        uptr newStored  = coreEnc ^ newNonce;

        state.encrypted    = reinterpret_cast<u32*>(newStored);
        state.accessNonce  = newNonce;
        state.integrityTag = computeIntegrityTag(
            newStored, newNonce,
            reinterpret_cast<uptr>(state.ptrEncryption.get()));

        // Verify vtable hasn't been externally corrupted
        u32 currentVtable = *realPtr;
        if (currentVtable != state.vtableSnapshot) {
            log.error("[MGR:{}] VTABLE_TAMPER ptr={:08X} expected={:08X} actual={:08X}",
                      state.name, reinterpret_cast<uptr>(realPtr),
                      state.vtableSnapshot, currentVtable);
            context.heartbeatHandler().detectionReport().report(
                DetectionFlag::kHookIntegrity, currentVtable);
            realPtr = nullptr;
        }
    }

    LeaveCriticalSection(&state.critSection);

    if (!realPtr) return nullptr;

    // Return-address whitelisting (outside critical section)
    if (state.whitelistReturnAddrs.contains(returnAddress))
        return realPtr;

    auto acBase = addr::globals::g_AntiCheatModuleBase;
    auto acSize = addr::globals::g_AntiCheatModuleSize;
    if (returnAddress >= acBase && returnAddress < acBase + acSize)
        return realPtr;

    log.error("[MGR:{}] REJECTED ra={:08X} (acBase={:08X} acSize={:08X})",
              state.name, returnAddress, acBase, acSize);
    context.heartbeatHandler().detectionReport().report(
        DetectionFlag::kHoneypotTriggered, returnAddress);
    return nullptr;
}

// ── Destroy logic ────────────────────────────────────────────────────────────

void managerDestroyHook(ManagerState& state) {
    if (!state.encrypted) return;

    auto& log = mg::ctx().logger();

    EnterCriticalSection(&state.critSection);
    if (state.encrypted) {
        uptr stored      = reinterpret_cast<uptr>(state.encrypted);
        u32  expectedTag = computeIntegrityTag(
            stored, state.accessNonce,
            reinterpret_cast<uptr>(state.ptrEncryption.get()));

        if (expectedTag == state.integrityTag) {
            u32* realPtr = state.ptrEncryption->decrypt<u32*>(
                stored ^ state.accessNonce);
            log.info("[MGR:{}] DESTROY ptr={:08X}",
                     state.name, reinterpret_cast<uptr>(realPtr));
            mg::callVFunc<void>(realPtr, 1); // virtual destructor
        }
        else {
            log.error("[MGR:{}] DESTROY INTEGRITY_FAIL stored={:08X} nonce={:08X} expected={:08X} actual={:08X}",
                      state.name, stored, state.accessNonce, expectedTag, state.integrityTag);
            mg::ctx().heartbeatHandler().detectionReport().report(
                DetectionFlag::kIntegrityViolation, static_cast<u32>(stored));
        }

        state.encrypted      = nullptr;
        state.accessNonce    = 0;
        state.integrityTag   = 0;
        state.vtableSnapshot = 0;
    }
    LeaveCriticalSection(&state.critSection);
}

// ── Internal decrypt (trusted callers inside the DLL) ────────────────────────

u32* ManagerState::decryptInternal() {
    if (!encrypted) return nullptr;

    auto& log = mg::ctx().logger();

    EnterCriticalSection(&critSection);
    u32* realPtr = nullptr;

    if (encrypted) {
        uptr stored      = reinterpret_cast<uptr>(encrypted);
        u32  expectedTag = computeIntegrityTag(
            stored, accessNonce,
            reinterpret_cast<uptr>(ptrEncryption.get()));

        if (expectedTag == integrityTag) {
            realPtr = ptrEncryption->decrypt<u32*>(stored ^ accessNonce);

            // Rolling re-encrypt
            u32  newNonce  = freshNonce();
            uptr coreEnc   = stored ^ accessNonce;
            uptr newStored = coreEnc ^ newNonce;

            encrypted    = reinterpret_cast<u32*>(newStored);
            accessNonce  = newNonce;
            integrityTag = computeIntegrityTag(
                newStored, newNonce,
                reinterpret_cast<uptr>(ptrEncryption.get()));

            // Verify vtable integrity
            u32 currentVtable = *realPtr;
            if (currentVtable != vtableSnapshot) {
                log.error("[MGR:{}] INTERNAL VTABLE_TAMPER expected={:08X} actual={:08X}",
                          name, vtableSnapshot, currentVtable);
                mg::ctx().heartbeatHandler().detectionReport().report(
                    DetectionFlag::kHookIntegrity, currentVtable);
                realPtr = nullptr;
            }
        }
        else {
            log.error("[MGR:{}] INTERNAL INTEGRITY_FAIL stored={:08X} nonce={:08X} expected={:08X} actual={:08X}",
                      name, stored, accessNonce, expectedTag, integrityTag);
            mg::ctx().heartbeatHandler().detectionReport().report(
                DetectionFlag::kIntegrityViolation, static_cast<u32>(stored));
        }
    }

    LeaveCriticalSection(&critSection);
    return realPtr;
}

} // namespace mg::game
