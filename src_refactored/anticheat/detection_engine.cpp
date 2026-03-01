// =============================================================================
// DetectionEngine — Orchestration only (scanners in anticheat/scanners/)
// =============================================================================
#include "pch.hpp"
#include "anticheat/detection_engine.hpp"
#include "core/context.hpp"
#include "utils/logger.hpp"
#include "utils/cloakwork_isolation.hpp"

// All scanner modules
#include "anticheat/scanners/scanner_common.hpp"
#include "anticheat/scanners/anti_debug_scanner.hpp"
#include "anticheat/scanners/inline_hook_scanner.hpp"
#include "anticheat/scanners/iat_hook_scanner.hpp"
#include "anticheat/scanners/integrity_scanner.hpp"
#include "anticheat/scanners/injection_scanner.hpp"
#include "anticheat/scanners/manual_map_scanner.hpp"
#include "anticheat/scanners/anonymous_thread_scanner.hpp"
#include "anticheat/scanners/proxy_dll_scanner.hpp"
#include "anticheat/scanners/hook_integrity_scanner.hpp"
#include "anticheat/scanners/peb_module_scanner.hpp"
#include "anticheat/scanners/unsigned_module_scanner.hpp"
#include "anticheat/scanners/handle_watcher_scanner.hpp"
#include "anticheat/scanners/vulnerable_driver_scanner.hpp"
#include "anticheat/scanners/string_scanner.hpp"
#include "anticheat/scanners/signature_scanner.hpp"
#include "anticheat/scanners/section_remap_scanner.hpp"
#include "anticheat/scanners/ntapi_monitor_scanner.hpp"
#include "anticheat/memory_fog.hpp"

#pragma comment(lib, "crypt32.lib")

namespace mg {

// Thread entry point (free function, not static member — safe for mapped DLL)
inline DWORD WINAPI scannerThreadProc(LPVOID param);

// =============================================================================
// DetectionEngine::Impl
// =============================================================================
struct DetectionEngine::Impl {
    MegaGuardContext& ctx;
    Logger&           log;
    NtApi             api;

    HANDLE shutdownEvent = nullptr;
    HANDLE scannerThread = nullptr;
    std::atomic<u32> flags{0};

    // Thread heartbeat
    std::vector<TrackedThread> trackedThreads;
    CritSection threadCs;

    // Shared config (scanners hold references)
    std::vector<std::string>  blacklistedModules;
    std::vector<std::string>  blacklistedStrings;
    std::vector<BytePattern>  blacklistedSigs;
    std::vector<HookBaseline> hookBaselines;
    CritSection configCs;

    // Memory fog (own module)
    std::unique_ptr<MemoryFog> fog;

    // Owned scanner modules
    std::vector<std::unique_ptr<IScanner>> scanners;

    // Per-scanner performance tracking (parallel to scanners vector)
    std::vector<ScannerStats> scannerStats;
    LARGE_INTEGER qpcFreq{};

    u32 tickCount = 0;

    explicit Impl(MegaGuardContext& c, Logger& l) : ctx(c), log(l) {}

    // ── Build all scanner modules ─────────────────────────────────────────
    void buildScanners() {
        scanners.push_back(std::make_unique<AntiDebugScanner>(api));
        scanners.push_back(std::make_unique<InlineHookScanner>());
        scanners.push_back(std::make_unique<IATHookScanner>());
        scanners.push_back(std::make_unique<IntegrityScanner>());
        scanners.push_back(std::make_unique<InjectionScanner>(api, blacklistedModules, configCs));
        scanners.push_back(std::make_unique<ManualMapScanner>());
        scanners.push_back(std::make_unique<AnonymousThreadScanner>(api));
        scanners.push_back(std::make_unique<ProxyDllScanner>());
        scanners.push_back(std::make_unique<HookIntegrityScanner>(hookBaselines, configCs));
        scanners.push_back(std::make_unique<PebModuleScanner>(blacklistedModules, configCs));
        scanners.push_back(std::make_unique<UnsignedModuleScanner>(api));
        scanners.push_back(std::make_unique<HandleWatcherScanner>(api));
        scanners.push_back(std::make_unique<VulnerableDriverScanner>());
        scanners.push_back(std::make_unique<StringScanner>(blacklistedStrings, configCs));
        scanners.push_back(std::make_unique<SignatureScanner>(blacklistedSigs, configCs));
        scanners.push_back(std::make_unique<SectionRemapScanner>(api));
        scanners.push_back(std::make_unique<NtApiMonitor>(ctx));
    }

    // ── Heartbeat check
    void checkHeartbeats() {
        CritLock lk(threadCs);
        for (auto& t : trackedThreads) {
            if (!t.handle) continue;
            DWORD ec = 0;
            if (GetExitCodeThread(t.handle, &ec) && ec != STILL_ACTIVE) {
                log.error("[DE] HEARTBEAT: '{}' terminated (exit={})", t.name, ec);
                t.handle = nullptr;
                continue;
            }
            u32 cur = t.heartbeat.load(std::memory_order_relaxed);
            if (cur == t.lastSeen && t.lastSeen != 0)
                log.error("[DE] HEARTBEAT: '{}' stalled (counter={})", t.name, cur);
            t.lastSeen = cur;
        }
    }
};

// ── Scanner thread entry (free function) ──────────────────────────────────
inline DWORD WINAPI scannerThreadProc(LPVOID param) {
    auto* self = static_cast<DetectionEngine::Impl*>(param);
    self->log.info("[DE] Scanner thread started (TID={})", GetCurrentThreadId());

    while (true) {
        DWORD wait = WaitForSingleObject(self->shutdownEvent, MG_INT(1000u));
        if (wait == WAIT_OBJECT_0) break;

        self->tickCount++;
        u32 tick = self->tickCount;

        self->checkHeartbeats();

        for (size_t i = 0; i < self->scanners.size(); ++i) {
            auto& scanner = self->scanners[i];
            u32 interval = scanner->effectiveInterval();
            if (interval > 0 && (tick % interval) == 0) {
                LARGE_INTEGER t0, t1;
                QueryPerformanceCounter(&t0);
                scanner->scan(self->flags, self->log);
                QueryPerformanceCounter(&t1);

                if (i < self->scannerStats.size() && self->qpcFreq.QuadPart > 0) {
                    u64 elapsed = static_cast<u64>((t1.QuadPart - t0.QuadPart) * 1000000 / self->qpcFreq.QuadPart);
                    auto& st = self->scannerStats[i];
                    st.lastUs = elapsed;
                    if (elapsed > st.peakUs) st.peakUs = elapsed;
                    st.totalUs += elapsed;
                    st.runCount++;
                    st.interval = interval;
                }
            }
        }

        // Periodic performance stats dump
        if (tick > 0 && (tick % MG_INT(60u)) == 0) {
            self->log.info("[DE] ===== Scanner Performance Stats =====");
            for (auto& s : self->scannerStats) {
                u64 avg = s.runCount > 0 ? (s.totalUs / s.runCount) : 0;
                self->log.info("[DE]   {:<24} interval={}s  runs={}  last={}us  avg={}us  peak={}us",
                    s.name, s.interval, s.runCount, s.lastUs, avg, s.peakUs);
            }
            self->log.info("[DE] ========================================");
        }
    }

    self->log.info("[DE] Scanner thread exiting");
    return 0;
}

// =============================================================================
// DetectionEngine public API
// =============================================================================
DetectionEngine::DetectionEngine(MegaGuardContext& ctx)
    : impl_(new Impl(ctx, ctx.logger())) {}

DetectionEngine::~DetectionEngine() {
    shutdown();
    delete impl_;
    impl_ = nullptr;
}

VoidResult DetectionEngine::initialize() {
    auto& log = impl_->log;

    if (!impl_->api.load()) {
        log.error("[DE] Failed to resolve NT APIs");
        return VoidResult::err(ErrorCode::kProcNotFound);
    }
    log.info("[DE] NT APIs resolved");

    impl_->buildScanners();
    log.info("[DE] {} scanners registered", impl_->scanners.size());

    // Initialize QPC frequency for timing
    QueryPerformanceFrequency(&impl_->qpcFreq);

    // Allocate per-scanner stats
    impl_->scannerStats.resize(impl_->scanners.size());
    for (size_t i = 0; i < impl_->scanners.size(); ++i) {
        impl_->scannerStats[i].name     = impl_->scanners[i]->name();
        impl_->scannerStats[i].interval = impl_->scanners[i]->effectiveInterval();
    }

    for (auto& s : impl_->scanners) {
        s->init();
        log.info("[DE] Scanner '{}' initialized (interval={}s)", s->name(), s->effectiveInterval());
    }

    //impl_->fog = std::make_unique<MemoryFog>(impl_->api, log);
    //impl_->fog->activate();

    impl_->shutdownEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!impl_->shutdownEvent) {
        log.error("[DE] CreateEvent failed");
        return VoidResult::err(ErrorCode::kInitFailed);
    }

    impl_->scannerThread = CreateThread(nullptr, 0, scannerThreadProc, impl_, 0, nullptr);
    if (!impl_->scannerThread) {
        log.error("[DE] CreateThread failed");
        CloseHandle(impl_->shutdownEvent);
        impl_->shutdownEvent = nullptr;
        return VoidResult::err(ErrorCode::kInitFailed);
    }

    log.info("[DE] DetectionEngine initialized");
    return VoidResult::ok();
}

void DetectionEngine::shutdown() {
    if (!impl_) return;

    if (impl_->shutdownEvent) {
        SetEvent(impl_->shutdownEvent);
        if (impl_->scannerThread) {
            WaitForSingleObject(impl_->scannerThread, MG_INT(5000u));
            CloseHandle(impl_->scannerThread);
            impl_->scannerThread = nullptr;
        }
        CloseHandle(impl_->shutdownEvent);
        impl_->shutdownEvent = nullptr;
    }

    impl_->fog.reset();

    impl_->scanners.clear();
    impl_->log.info("[DE] DetectionEngine shutdown");
}

void DetectionEngine::trackThread(void* handle, const char* name) {
    CritLock lk(impl_->threadCs);
    impl_->trackedThreads.emplace_back(static_cast<HANDLE>(handle), name);
    impl_->log.info("[DE] Tracking thread '{}' (handle=0x{:X})", name, reinterpret_cast<uptr>(handle));
}

void DetectionEngine::addBlacklistedModule(const std::string& name) {
    CritLock lk(impl_->configCs);
    impl_->blacklistedModules.push_back(name);
}

void DetectionEngine::addBlacklistedString(const std::string& str) {
    CritLock lk(impl_->configCs);
    impl_->blacklistedStrings.push_back(str);
}

void DetectionEngine::addBlacklistedSignature(BytePattern bp) {
    CritLock lk(impl_->configCs);
    impl_->blacklistedSigs.push_back(std::move(bp));
}

void DetectionEngine::addHookBaseline(uptr address, u32 size) {
    CritLock lk(impl_->configCs);
    u32 h = fnv1a(reinterpret_cast<const u8*>(address), size);
    impl_->hookBaselines.push_back({address, size, h});
    impl_->log.info("[DE] Hook baseline: 0x{:X} size={} hash=0x{:08X}", address, size, h);
}

u32 DetectionEngine::detectionFlags() const {
    return impl_->flags.load(std::memory_order_relaxed);
}

bool DetectionEngine::hasDetection() const {
    return detectionFlags() != 0;
}

u32 DetectionEngine::exchangeFlags() {
    return impl_->flags.exchange(0, std::memory_order_acq_rel);
}

void DetectionEngine::setScannerInterval(const char* scannerName, u32 seconds) {
    for (size_t i = 0; i < impl_->scanners.size(); ++i) {
        if (_stricmp(impl_->scanners[i]->name(), scannerName) == 0) {
            impl_->scanners[i]->intervalOverride = seconds;
            if (i < impl_->scannerStats.size())
                impl_->scannerStats[i].interval = impl_->scanners[i]->effectiveInterval();
            impl_->log.info("[DE] Scanner '{}' interval set to {}s", scannerName, seconds);
            return;
        }
    }
    impl_->log.error("[DE] Scanner '{}' not found", scannerName);
}

std::vector<ScannerStats> DetectionEngine::getScannerStats() const {
    return impl_->scannerStats;
}

void DetectionEngine::dumpScannerStats() {
    impl_->log.info("[DE] ===== Scanner Performance Stats =====");
    for (auto& s : impl_->scannerStats) {
        u64 avgUs = s.runCount > 0 ? (s.totalUs / s.runCount) : 0;
        impl_->log.info("[DE]   {:<24} interval={}s  runs={}  last={}us  avg={}us  peak={}us  total={}us",
            s.name, s.interval, s.runCount, s.lastUs, avgUs, s.peakUs, s.totalUs);
    }
    impl_->log.info("[DE] ========================================");
}

} // namespace mg
