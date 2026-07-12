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

    // Set to 1 to disable pointer encryption, integrity checks, vtable
    // checks, and return-address whitelisting in manager hooks.
    // Stores/returns raw pointers — same behavior as the original game.
    #define MG_DISABLE_MANAGER_ENCRYPTION 0

#if !MG_DISABLE_MANAGER_ENCRYPTION

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

#endif // !MG_DISABLE_MANAGER_ENCRYPTION

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
#if !MG_DISABLE_MANAGER_ENCRYPTION
            ptrEncryption = std::make_unique<::mg::PointerEncryption>(moduleBase);
#endif
            initialized = true;
        }
    }

    void ManagerState::shutdown() {
        if (initialized) {
            encrypted = nullptr;
            integrityTag = 0;
            accessNonce = 0;
            vtableSnapshot = 0;
#if !MG_DISABLE_MANAGER_ENCRYPTION
            ptrEncryption.reset();
#endif
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
        u32* realPtr = nullptr;

        EnterCriticalSection(&state.critSection);

        if (!state.encrypted) {
            auto* mem = static_cast<u32*>(::operator new(allocSize, std::nothrow));
            if (!mem) {
                LeaveCriticalSection(&state.critSection);
                return nullptr;
            }

            realPtr = mg::call<u32 * (__thiscall*)(void*)>(initAddr, mem);

            //context.logger().info("[Manager:{}] INIT alloc={} initAddr=0x{:08X} -> ptr=0x{:08X}",
            //    state.name, allocSize, static_cast<u32>(initAddr), reinterpret_cast<u32>(realPtr));
#if MG_DISABLE_MANAGER_ENCRYPTION
            state.encrypted = realPtr;
#else
            state.vtableSnapshot = *realPtr;

            u32  nonce = freshNonce();
            uptr coreEnc = state.ptrEncryption->process(reinterpret_cast<uptr>(realPtr));
            uptr stored = coreEnc ^ nonce;

            state.encrypted = reinterpret_cast<u32*>(stored);
            state.accessNonce = nonce;
            state.integrityTag = computeIntegrityTag(
                stored, nonce, reinterpret_cast<uptr>(state.ptrEncryption.get()));
#endif

            context.ipcClient().setManagerReady(1, 4);
        }
        else {
#if MG_DISABLE_MANAGER_ENCRYPTION
            realPtr = state.encrypted;
#else
            uptr stored = reinterpret_cast<uptr>(state.encrypted);
            u32  expectedTag = computeIntegrityTag(
                stored, state.accessNonce,
                reinterpret_cast<uptr>(state.ptrEncryption.get()));

            if (expectedTag != state.integrityTag) {
                context.heartbeatHandler().detectionReport().report(
                    DetectionFlag::kIntegrityViolation, static_cast<u32>(stored));
                LeaveCriticalSection(&state.critSection);
                return nullptr;
            }

            realPtr = state.ptrEncryption->decrypt<u32*>(stored ^ state.accessNonce);

            u32  newNonce = freshNonce();
            uptr coreEnc = stored ^ state.accessNonce;
            uptr newStored = coreEnc ^ newNonce;

            state.encrypted = reinterpret_cast<u32*>(newStored);
            state.accessNonce = newNonce;
            state.integrityTag = computeIntegrityTag(
                newStored, newNonce,
                reinterpret_cast<uptr>(state.ptrEncryption.get()));

            u32 currentVtable = *realPtr;
            if (currentVtable != state.vtableSnapshot) {
                context.heartbeatHandler().detectionReport().report(
                    DetectionFlag::kHookIntegrity, currentVtable);
                realPtr = nullptr;
            }
#endif
        }

        LeaveCriticalSection(&state.critSection);

        if (!realPtr) return nullptr;

#if !MG_DISABLE_MANAGER_ENCRYPTION
        if (state.whitelistReturnAddrs.contains(returnAddress))
            return realPtr;

        auto acBase = addr::globals::g_AntiCheatModuleBase;
        auto acSize = addr::globals::g_AntiCheatModuleSize;
        if (returnAddress >= acBase && returnAddress < acBase + acSize)
            return realPtr;

        context.heartbeatHandler().detectionReport().report(
            DetectionFlag::kHoneypotTriggered, returnAddress);
        return nullptr;
#else
        return realPtr;
#endif
    }

    // ── Destroy logic ────────────────────────────────────────────────────────────

    void managerDestroyHook(ManagerState& state) {
        if (!state.encrypted) return;

        auto& context = mg::ctx();
        EnterCriticalSection(&state.critSection);
        if (state.encrypted) {
#if MG_DISABLE_MANAGER_ENCRYPTION
            u32* realPtr = state.encrypted;
            context.logger().info("[Manager:{}] DESTROY ptr=0x{:08X}",
                state.name, reinterpret_cast<u32>(realPtr));
            mg::callVFunc<void>(realPtr, 0, 0);
            ::operator delete(realPtr);
#else
            uptr stored = reinterpret_cast<uptr>(state.encrypted);
            u32  expectedTag = computeIntegrityTag(
                stored, state.accessNonce,
                reinterpret_cast<uptr>(state.ptrEncryption.get()));

            if (expectedTag == state.integrityTag) {
                u32* realPtr = state.ptrEncryption->decrypt<u32*>(
                    stored ^ state.accessNonce);
                mg::callVFunc<void>(realPtr, 0, 0);
                ::operator delete(realPtr);
            }
            else {
                mg::ctx().heartbeatHandler().detectionReport().report(
                    DetectionFlag::kIntegrityViolation, static_cast<u32>(stored));
            }
#endif

            state.encrypted = nullptr;
            state.accessNonce = 0;
            state.integrityTag = 0;
            state.vtableSnapshot = 0;
        }
        LeaveCriticalSection(&state.critSection);
    }

    // ── Internal decrypt (trusted callers inside the DLL) ────────────────────────

    u32* ManagerState::decryptInternal() {
        if (!encrypted) return nullptr;

        EnterCriticalSection(&critSection);
        u32* realPtr = nullptr;

        if (encrypted) {
#if MG_DISABLE_MANAGER_ENCRYPTION
            realPtr = encrypted;
#else
            uptr stored = reinterpret_cast<uptr>(encrypted);
            u32  expectedTag = computeIntegrityTag(
                stored, accessNonce,
                reinterpret_cast<uptr>(ptrEncryption.get()));

            if (expectedTag == integrityTag) {
                realPtr = ptrEncryption->decrypt<u32*>(stored ^ accessNonce);

                u32  newNonce = freshNonce();
                uptr coreEnc = stored ^ accessNonce;
                uptr newStored = coreEnc ^ newNonce;

                encrypted = reinterpret_cast<u32*>(newStored);
                accessNonce = newNonce;
                integrityTag = computeIntegrityTag(
                    newStored, newNonce,
                    reinterpret_cast<uptr>(ptrEncryption.get()));

                u32 currentVtable = *realPtr;
                if (currentVtable != vtableSnapshot) {
                    mg::ctx().heartbeatHandler().detectionReport().report(
                        DetectionFlag::kHookIntegrity, currentVtable);
                    realPtr = nullptr;
                }
            }
            else {
                mg::ctx().heartbeatHandler().detectionReport().report(
                    DetectionFlag::kIntegrityViolation, static_cast<u32>(stored));
            }
#endif
        }

        LeaveCriticalSection(&critSection);
        return realPtr;
    }

} // namespace mg::game