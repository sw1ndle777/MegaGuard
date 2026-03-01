// =============================================================================
// MegaGuardContext - Implementation
// =============================================================================
// CloakWork isolated here: cloakwork.h included ONLY in this .cpp.
// Changes to subsystem internals rebuild only this TU.
// =============================================================================
#include "pch.hpp"
#include "core/context.hpp"

// ── Subsystem headers (included only here, never in public headers) ────────────
#include "platform/memory_pool.hpp"
#include "platform/import_resolver.hpp"
#include "platform/syscall_cloner.hpp"
#include "platform/ipc.hpp"
#include "engine/hook_registry.hpp"
#include "engine/pattern_scanner.hpp"
#include "engine/pointer_encryption.hpp"
#include "anticheat/anti_debug_engine.hpp"
#include "anticheat/integrity_engine.hpp"
#include "anticheat/iat_scrubber.hpp"
#include "anticheat/manual_seh.hpp"
#include "anticheat/guard_regions.hpp"
#include "anticheat/detection_engine.hpp"
#include "game/network/crypto_engine.hpp"
#include "game/network/secure_channel.hpp"
#include "game/network/authorize_handler.hpp"
#include "game/network/connect_handler.hpp"
#include "game/network/heartbeat_handler.hpp"
#include "game/managers/game_managers.hpp"
#include "game/data/cdbm_loader.hpp"
#include "modding/features/custom_tickrate.hpp"
#include "modding/features/hide_weapon_slots.hpp"
#include "modding/features/nation_index.hpp"
#include "modding/features/pcbang.hpp"
#include "modding/features/resolutions.hpp"
#include "modding/features/spectate_pov.hpp"
#include "modding/bugfixes/screenshot_fix.hpp"
#include "modding/bugfixes/weapon_restriction.hpp"
#include "game/ui/dialog/popups/input_pop.hpp"
#include "game/ui/dialog/rightclick/rightclick.hpp"
#include "game/ui/dialog/quickconfirm/quickconfirm.hpp"
#include "game/network/custom_packets/custom_packet_dispatcher.hpp"
#include "game/network/custom_packets/gacha_pity_handler.hpp"
#include "utils/logger.hpp"
#include "utils/splash.hpp"

// CloakWork — ONLY in .cpp files, NEVER in public headers.
#include "utils/cloakwork_isolation.hpp"

// Needed for g_AntiCheatModuleSize in IntegrityEngine init
#include "game/addresses.hpp"

namespace mg {

// ── PIMPL definition ───────────────────────────────────────────────────────────
struct MegaGuardContext::Impl {
    // Construction order = initialization order.
    // Destruction order = reverse (C++ guarantees this for members).
    MemoryPool           memoryPool;
    ImportResolver       importResolver;
    SyscallCloner        syscallCloner;
    IpcClient            ipcClient;
    Logger               logger;
    HookRegistry         hookRegistry;
    PatternScanner       patternScanner;
    PointerEncryption    pointerEncryption;
    std::unique_ptr<AntiDebugEngine>       antiDebugEngine;
    std::unique_ptr<IntegrityEngine>       integrityEngine;
    std::unique_ptr<IATScrubber>           iatScrubber;
    std::unique_ptr<ManualSEH>             manualSeh;
    std::unique_ptr<GuardRegions>           guardRegions;
    std::unique_ptr<DetectionEngine>        detectionEngine;
    std::unique_ptr<game::CryptoEngine>    cryptoEngine;
    std::unique_ptr<game::SecureChannel>    secureChannel;
    std::unique_ptr<game::AuthorizeHandler>  authorizeHandler;
    std::unique_ptr<game::ConnectHandler>    connectHandler;
    std::unique_ptr<game::HeartbeatHandler>  heartbeatHandler;
    std::unique_ptr<game::GameManagerHooks>  gameManagers;
    std::unique_ptr<game::CDBMLoader>        cdbmLoader;
    std::unique_ptr<modding::CustomTickrate>     customTickrate;
    std::unique_ptr<modding::HideWeaponSlots>    hideWeaponSlots;
    std::unique_ptr<modding::NationIndex>        nationIndex;
    std::unique_ptr<modding::PcBang>             pcBang;
    std::unique_ptr<modding::CustomResolutions>  customResolutions;
    std::unique_ptr<modding::SpectatePov>        spectatePov;
    std::unique_ptr<modding::ScreenshotFix>      screenshotFix;
    std::unique_ptr<modding::WeaponRestriction>  weaponRestriction;
	std::unique_ptr<game::DlgInputPopup>         inputPopups;
	std::unique_ptr<game::DlgRightClick>         rightClick;
	std::unique_ptr<game::DlgQuickConfirm>       quickConfirm;
	std::unique_ptr<game::CustomPacketDispatcher> packetDispatcher;
	std::unique_ptr<game::GachaPityHandler>       gachaPityHandler;
	Splash               splashScreen;

    explicit Impl(uptr moduleBase)
        : memoryPool()
        , importResolver(memoryPool)
        , syscallCloner(memoryPool, importResolver)
        , ipcClient()
        , logger()
        , hookRegistry()
        , patternScanner(moduleBase)
        , pointerEncryption(moduleBase)
        , splashScreen()
    {}
};

// ── Constructor/Destructor ─────────────────────────────────────────────────────
MegaGuardContext::MegaGuardContext(uptr moduleBase)
    : moduleBase_(moduleBase)
    , impl_(nullptr)
{
    // Allocate Impl via VirtualAlloc — no CRT heap dependency.
    void* mem = VirtualAlloc(nullptr, sizeof(Impl), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (mem) {
        impl_ = new (mem) Impl(moduleBase);
    }
}

MegaGuardContext::~MegaGuardContext() {
    shutdown();
    if (impl_) {
        impl_->~Impl();
        VirtualFree(impl_, 0, MEM_RELEASE);
        impl_ = nullptr;
    }
}

VoidResult MegaGuardContext::initialize() {
    if (!impl_) return VoidResult::err(ErrorCode::kAllocationFail);

    // 1. Logger
    impl_->logger.init("MegaGuard\\", true);
    impl_->logger.info("MegaGuard v{}.{}.{} initializing...",
        MG_VERSION_MAJOR, MG_VERSION_MINOR, MG_VERSION_PATCH);

    // 2. IPC
    impl_->ipcClient.open();
    impl_->ipcClient.setProgress(10);

    // 3. Splash
    impl_->splashScreen.start();

    // 4. Import resolver bootstrap
    /*
    auto resolverResult = impl_->importResolver.initialize();
    if (resolverResult.isErr()) {
        impl_->logger.error("ImportResolver init failed: {}", static_cast<u32>(resolverResult.error()));
        return VoidResult::err(resolverResult.error());
    }
    */
    impl_->ipcClient.setProgress(20);

    // 5. Syscall cloner
    
    auto syscallResult = impl_->syscallCloner.initialize();
    if (syscallResult.isErr()) {
        impl_->logger.error("SyscallCloner init failed");
    }
    
    impl_->ipcClient.setProgress(30);

    // 6. Anti-debug engine
    /*
    impl_->antiDebugEngine = std::make_unique<AntiDebugEngine>(*this);
    impl_->antiDebugEngine->initialize();
    */
    impl_->ipcClient.setProgress(40);

    // 7. Integrity engine (pass actual module size)
    /*
    impl_->integrityEngine = std::make_unique<IntegrityEngine>(*this);
    {
        auto moduleSize = game::addr::globals::g_AntiCheatModuleSize;
        impl_->integrityEngine->initialize(moduleBase_, moduleSize);
    }
    */
    impl_->ipcClient.setProgress(50);

    // 8. IAT scrubber
    //impl_->iatScrubber = std::make_unique<IATScrubber>(*this);
    //impl_->iatScrubber->scrubDebugImports();
    impl_->ipcClient.setProgress(60);
    
    // 9. ManualSEH (must be initialized before GuardRegions)
    impl_->manualSeh = std::make_unique<ManualSEH>();
    {
        auto sehResult = impl_->manualSeh->initialize();
        if (sehResult.isErr()) {
            impl_->logger.error("ManualSEH init failed");
            return VoidResult::err(sehResult.error());
        }
    }
    
    impl_->ipcClient.setProgress(65);

    impl_->logger.info("Core initialization complete");

    // ── Initialize MinHook BEFORE any hooks are created ──────────────────
    auto mhResult = impl_->hookRegistry.initializeMinHook();
    if (mhResult.isErr()) {
        impl_->logger.error("MinHook init failed");
        return VoidResult::err(mhResult.error());
    }

    // 10. GuardRegions (KiUserExceptionDispatcher hook for page guards)
    //     Must come after MinHook init — it creates a detour on ntdll
    
    impl_->guardRegions = std::make_unique<GuardRegions>(*this);
    {
        auto grResult = impl_->guardRegions->initialize();
        if (grResult.isErr()) {
            impl_->logger.error("GuardRegions init failed");
            // Non-fatal: continue without guard regions
        }
    }
    
    
    

    // ── Install Features hooks ──────────────────────────────────────────
    impl_->ipcClient.setProgress(74);

    
    impl_->customResolutions = std::make_unique<modding::CustomResolutions>(*this);
    impl_->customResolutions->patch();

    impl_->hideWeaponSlots = std::make_unique<modding::HideWeaponSlots>(*this);
    impl_->hideWeaponSlots->install();

    impl_->customTickrate = std::make_unique<modding::CustomTickrate>(*this);
    impl_->customTickrate->patch();

    
    impl_->nationIndex = std::make_unique<modding::NationIndex>(*this);
    impl_->nationIndex->install();
    
    impl_->spectatePov = std::make_unique<modding::SpectatePov>(*this);
    impl_->spectatePov->install();

    impl_->pcBang = std::make_unique<modding::PcBang>(*this);
    impl_->pcBang->install();

    impl_->inputPopups = std::make_unique<game::DlgInputPopup>(*this);
	impl_->inputPopups->install();

	impl_->rightClick = std::make_unique<game::DlgRightClick>(*this);
	impl_->rightClick->install();

	impl_->quickConfirm = std::make_unique<game::DlgQuickConfirm>(*this);
	impl_->quickConfirm->install();

	impl_->packetDispatcher = std::make_unique<game::CustomPacketDispatcher>(*this);
	impl_->packetDispatcher->install();

	impl_->gachaPityHandler = std::make_unique<game::GachaPityHandler>(*this, *impl_->packetDispatcher);
	impl_->gachaPityHandler->install();


	impl_->ipcClient.setProgress(80);

    // ── Install BugFixes hooks ──────────────────────────────────────────

    impl_->weaponRestriction = std::make_unique<modding::WeaponRestriction>(*this);
    impl_->weaponRestriction->install();
   
    
    impl_->screenshotFix = std::make_unique<modding::ScreenshotFix>(*this);
    impl_->screenshotFix->install();
   
    impl_->ipcClient.setProgress(88);

    // ── Install AntiCheat hooks ─────────────────────────────────────────
    impl_->gameManagers = std::make_unique<game::GameManagerHooks>(*this);
    impl_->gameManagers->install();

    impl_->cryptoEngine = std::make_unique<game::CryptoEngine>(*this);
    impl_->cryptoEngine->install();

    impl_->secureChannel = std::make_unique<game::SecureChannel>();

    impl_->authorizeHandler = std::make_unique<game::AuthorizeHandler>(*this);
    impl_->authorizeHandler->install();

    impl_->connectHandler = std::make_unique<game::ConnectHandler>(*this);
    impl_->connectHandler->install();

    impl_->heartbeatHandler = std::make_unique<game::HeartbeatHandler>(*this);
    impl_->heartbeatHandler->install();

    impl_->cdbmLoader = std::make_unique<game::CDBMLoader>(*this);
    // CDBM load hook is commented out in original — skip install for now

    impl_->ipcClient.setProgress(95);

    // 11. Detection engine (start scanner thread after all hooks are in place)
    /*
    impl_->detectionEngine = std::make_unique<DetectionEngine>(*this);
    {
        auto deResult = impl_->detectionEngine->initialize();
        if (deResult.isErr()) {
            impl_->logger.error("DetectionEngine init failed");
            // Non-fatal: continue without detection engine
        }
    }
    */
    

    impl_->ipcClient.setProgress(98);

    return VoidResult::ok();
}

void MegaGuardContext::shutdown() {
    if (!impl_) return;
    if (impl_->detectionEngine) impl_->detectionEngine->shutdown();
    if (impl_->guardRegions) impl_->guardRegions->shutdown();
    if (impl_->manualSeh)    impl_->manualSeh->shutdown();
    impl_->hookRegistry.removeAll();
    impl_->ipcClient.close();
    impl_->logger.info("MegaGuard shutdown complete");
    impl_->logger.stop();
}

// ── Accessors ──────────────────────────────────────────────────────────────────
MemoryPool&           MegaGuardContext::memoryPool()           { return impl_->memoryPool; }
ImportResolver&       MegaGuardContext::importResolver()       { return impl_->importResolver; }
SyscallCloner&        MegaGuardContext::syscallCloner()        { return impl_->syscallCloner; }
IpcClient&            MegaGuardContext::ipcClient()            { return impl_->ipcClient; }
Logger&               MegaGuardContext::logger()               { return impl_->logger; }
HookRegistry&         MegaGuardContext::hookRegistry()         { return impl_->hookRegistry; }
PatternScanner&       MegaGuardContext::patternScanner()       { return impl_->patternScanner; }
PointerEncryption&    MegaGuardContext::pointerEncryption()    { return impl_->pointerEncryption; }
AntiDebugEngine&      MegaGuardContext::antiDebugEngine()      { return *impl_->antiDebugEngine; }
IntegrityEngine&      MegaGuardContext::integrityEngine()      { return *impl_->integrityEngine; }
IATScrubber&          MegaGuardContext::iatScrubber()          { return *impl_->iatScrubber; }
ManualSEH&            MegaGuardContext::manualSEH()            { return *impl_->manualSeh; }
GuardRegions&         MegaGuardContext::guardRegions()          { return *impl_->guardRegions; }
game::CryptoEngine&   MegaGuardContext::cryptoEngine()         { return *impl_->cryptoEngine; }
DetectionEngine&      MegaGuardContext::detectionEngine()       { return *impl_->detectionEngine; }
Splash&               MegaGuardContext::splashScreen()         { return impl_->splashScreen; }
game::GameManagerHooks& MegaGuardContext::gameManagers()       { return *impl_->gameManagers; }
game::SecureChannel&  MegaGuardContext::secureChannel()        { return *impl_->secureChannel; }
game::HeartbeatHandler& MegaGuardContext::heartbeatHandler()   { return *impl_->heartbeatHandler; }
uptr                  MegaGuardContext::moduleBase() const     { return moduleBase_; }

} // namespace mg
