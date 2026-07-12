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
#include "engine/hook_id.hpp"
#include "engine/pattern_scanner.hpp"
#include "engine/pointer_encryption.hpp"
#include "anticheat/integrity_engine.hpp"
#include "anticheat/iat_scrubber.hpp"
#include "anticheat/manual_seh.hpp"
#include "anticheat/guard_regions.hpp"
#include "anticheat/detection_engine.hpp"
#include "game/network/crypto_engine.hpp"
#include "game/network/secure_channel.hpp"
#include "game/network/authorize_handler.hpp"
#include "game/network/connect_handler.hpp"
#include "game/network/disconnect_handler.hpp"
#include "game/network/heartbeat_handler.hpp"
#include "game/managers/game_managers.hpp"
#include "game/data/cdbm_loader.hpp"
#include "modding/features/custom_tickrate.hpp"
#include "modding/features/hide_weapon_slots.hpp"
#include "modding/features/hud_toggle.hpp"
#include "modding/features/zoom_sensitivity.hpp"
#include "modding/features/nation_index.hpp"
#include "modding/features/pcbang.hpp"
#include "modding/features/resolutions.hpp"
#include "modding/features/spectate_pov.hpp"
#include "modding/features/weapon_drop.hpp"
#include "modding/features/editbox_clipboard.hpp"
#include "game/network/movement/movement_protocol.hpp"
#include "modding/bugfixes/screenshot_fix.hpp"
#include "modding/bugfixes/weapon_restriction.hpp"
#include "modding/bugfixes/window_close_fix.hpp"
#include "game/ui/dialog/popups/input_pop.hpp"
#include "game/ui/dialog/rightclick/rightclick.hpp"
#include "game/ui/dialog/quickconfirm/quickconfirm.hpp"
#include "game/network/custom_packets/custom_packet_dispatcher.hpp"
#include "game/network/custom_packets/gacha_pity_handler.hpp"
#include "game/network/custom_packets/trade_handler.hpp"
#include "game/network/custom_packets/weekly_reward_handler.hpp"
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
    std::unique_ptr<IntegrityEngine>       integrityEngine;
    std::unique_ptr<IATScrubber>           iatScrubber;
    std::unique_ptr<ManualSEH>             manualSeh;
    //std::unique_ptr<GuardRegions>           guardRegions;
    std::unique_ptr<DetectionEngine>        detectionEngine;
    std::unique_ptr<game::CryptoEngine>    cryptoEngine;
    std::unique_ptr<game::SecureChannel>    secureChannel;
    std::unique_ptr<game::AuthorizeHandler>  authorizeHandler;
    std::unique_ptr<game::ConnectHandler>    connectHandler;
    std::unique_ptr<game::DisconnectHandler> disconnectHandler;
    std::unique_ptr<game::HeartbeatHandler>  heartbeatHandler;
    std::unique_ptr<game::GameManagerHooks>  gameManagers;
    std::unique_ptr<game::CDBMLoader>        cdbmLoader;
    std::unique_ptr<modding::CustomTickrate>     customTickrate;
    std::unique_ptr<modding::HideWeaponSlots>    hideWeaponSlots;
    std::unique_ptr<modding::HudToggle>          hudToggle;
    std::unique_ptr<modding::ZoomSensitivity>    zoomSensitivity;
    std::unique_ptr<modding::NationIndex>        nationIndex;
    std::unique_ptr<modding::PcBang>             pcBang;
    std::unique_ptr<modding::CustomResolutions>  customResolutions;
    std::unique_ptr<modding::SpectatePov>        spectatePov;
    std::unique_ptr<modding::WeaponDrop>         weaponDrop;
    std::unique_ptr<game::MovementProtocol>      movementProtocol;
    std::unique_ptr<modding::EditBoxClipboard>    editBoxClipboard;
    std::unique_ptr<modding::ScreenshotFix>      screenshotFix;
    std::unique_ptr<modding::WeaponRestriction>  weaponRestriction;
    std::unique_ptr<modding::WindowCloseFix>     windowCloseFix;
	std::unique_ptr<game::DlgInputPopup>         inputPopups;
	std::unique_ptr<game::DlgRightClick>         rightClick;
	std::unique_ptr<game::DlgQuickConfirm>       quickConfirm;
	std::unique_ptr<game::CustomPacketDispatcher> packetDispatcher;
	std::unique_ptr<game::GachaPityHandler>       gachaPityHandler;
	std::unique_ptr<game::WeeklyRewardHandler>    weeklyRewardHandler;
	std::unique_ptr<game::TradeHandler>           tradeHandler;
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
    //impl_->logger.info("[Init] Logger ready, PID={}", GetCurrentProcessId());

    // 2. IPC
    //impl_->logger.info("[Init] IPC open...");
    impl_->ipcClient.open();
    impl_->ipcClient.setProgress(10);

    // 3. Splash
    //impl_->logger.info("[Init] Splash start...");
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
    //impl_->logger.info("[Init] SyscallCloner...");
    auto syscallResult = impl_->syscallCloner.initialize();
    if (syscallResult.isErr()) {
        //impl_->logger.error("SyscallCloner init failed");
    }
    
    impl_->ipcClient.setProgress(30);

    // 6. Reserved progress step (anti-debugging is scanner-driven)
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
    /*
    impl_->manualSeh = std::make_unique<ManualSEH>();
    {
        auto sehResult = impl_->manualSeh->initialize();
        if (sehResult.isErr()) {
            impl_->logger.error("ManualSEH init failed");
            return VoidResult::err(sehResult.error());
        }
    }
    */
    
    impl_->ipcClient.setProgress(65);

    // ── Initialize MinHook BEFORE any hooks are created ──────────────────
    auto mhResult = impl_->hookRegistry.initializeMinHook();
    if (mhResult.isErr()) {
        return VoidResult::err(mhResult.error());
    }

    // 10. GuardRegions (KiUserExceptionDispatcher hook for page guards)
    //     Must come after MinHook init — it creates a detour on ntdll
    
    //impl_->logger.info("[Init] GuardRegions...");
    /*
    impl_->guardRegions = std::make_unique<GuardRegions>(*this);
    {
        auto grResult = impl_->guardRegions->initialize();
        if (grResult.isErr()) {
            //impl_->logger.error("GuardRegions init failed");
            // Non-fatal: continue without guard regions
        }
    }
    */
    
    

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

    impl_->hudToggle = std::make_unique<modding::HudToggle>(*this);
    impl_->hudToggle->install();

    impl_->spectatePov = std::make_unique<modding::SpectatePov>(*this);
    impl_->spectatePov->install();

    // Weapon-drop-on-death diagnostic/fix (logs the drop chain on each death).
    // DISABLED: diagnostic confirmed the drop no-ops because the weapon model's
    // NiPhysXProp (model+0x264) is null at drop time — a 1.0.3 runtime prop-attach
    // gap, not fixable from this hook. Re-enable only to re-run the chain logger.
    // impl_->weaponDrop = std::make_unique<modding::WeaponDrop>(*this);
    // impl_->weaponDrop->install();

    // ZoomSensitivity: per-zoom mouse-sensitivity hooks + Options-menu slider/dialog
    // hooks (the sliders bind to the restored SCENE_COMMON.xml backup).
    impl_->zoomSensitivity = std::make_unique<modding::ZoomSensitivity>(*this);
    impl_->zoomSensitivity->install();

    //impl_->movementProtocol = std::make_unique<game::MovementProtocol>(*this);
    //impl_->movementProtocol->install();

    impl_->pcBang = std::make_unique<modding::PcBang>(*this);
    impl_->pcBang->install();

    //impl_->editBoxClipboard = std::make_unique<modding::EditBoxClipboard>(*this);
    //impl_->editBoxClipboard->install();

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
    

	// Monthly-only build: the handler MUST still be constructed — pcbang.cpp::hkAgoraInitDlg
	// calls weeklyRewardHandler().showOnLobbyEntry() to register the Monthly reward-menu
	// dropdown. (Weekly/play-time/battle-pass stay disabled via #if 0 inside the handler.)
	// Leaving this commented made weeklyRewardHandler() deref a null unique_ptr -> crash on
	// interface load (showOnLobbyEntry -> ctx_.logger() reading 0x0).
	impl_->weeklyRewardHandler = std::make_unique<game::WeeklyRewardHandler>(*this, *impl_->packetDispatcher);
	impl_->weeklyRewardHandler->install();

	// TRADE DISABLED (user request — to be fixed later). Construction + install + the
	// lobby dialog registration in pcbang.cpp::hkAgoraInitDlg are all commented out so
	// no trade packets/hooks/dialog run.
	// impl_->tradeHandler = std::make_unique<game::TradeHandler>(*this, *impl_->packetDispatcher);
	// impl_->tradeHandler->install();


	impl_->ipcClient.setProgress(80);

    // ── Install BugFixes hooks ──────────────────────────────────────────

    
    impl_->weaponRestriction = std::make_unique<modding::WeaponRestriction>(*this);
    impl_->weaponRestriction->install();

    
    impl_->screenshotFix = std::make_unique<modding::ScreenshotFix>(*this);
    impl_->screenshotFix->install();
    

    impl_->hookRegistry.registerPatchBytes(HookId::DisconnectReloginState)
        .patch(MG_CONST(game::addr::bugfixes::DisconnectReloginState),
            "\x6A\x03", 2);
            

    impl_->hookRegistry.registerPatchBytes(HookId::CpuidInitStub)
        .patch(MG_CONST(game::addr::bugfixes::CpuidInit),
            "\xB8\x01\x00\x00\x00\xC3", 6);

    // ── Grade threshold patches (raise cmp 7 → 12 so grades 7-9 play normally) ─
    /*
    {
        const u8 threshold = static_cast<u8>(MG_CONST(game::addr::grade_threshold::kNewThreshold));
        impl_->hookRegistry.registerPatchBytes(HookId::GradeRoomCreate)
            .patch(MG_CONST(game::addr::grade_threshold::RoomCreate_Cmp), &threshold, 1);
        impl_->hookRegistry.registerPatchBytes(HookId::GradePartyCreate)
            .patch(MG_CONST(game::addr::grade_threshold::PartyCreate_Cmp), &threshold, 1);
        impl_->hookRegistry.registerPatchBytes(HookId::GradeAloneMode)
            .patch(MG_CONST(game::addr::grade_threshold::AloneMode_Cmp), &threshold, 1);
        impl_->hookRegistry.registerPatchBytes(HookId::GradeMatchGmBypass1)
            .patch(MG_CONST(game::addr::grade_threshold::MatchGmBypass1_Cmp), &threshold, 1);
        impl_->hookRegistry.registerPatchBytes(HookId::GradeMatchGmBypass2)
            .patch(MG_CONST(game::addr::grade_threshold::MatchGmBypass2_Cmp), &threshold, 1);
        impl_->hookRegistry.registerPatchBytes(HookId::GradeMatchValidation)
            .patch(MG_CONST(game::addr::grade_threshold::MatchValidation_Cmp), &threshold, 1);
    }
    */

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

    impl_->disconnectHandler = std::make_unique<game::DisconnectHandler>(*this);
    impl_->disconnectHandler->install();

    impl_->heartbeatHandler = std::make_unique<game::HeartbeatHandler>(*this);
    impl_->heartbeatHandler->install();

    

    //impl_->cdbmLoader = std::make_unique<game::CDBMLoader>(*this);
    // CDBM load hook is commented out in original — skip install for now

    impl_->ipcClient.setProgress(95);

    // ── Apply all queued hooks atomically (MinHook suspends threads internally) ──
    auto applyResult = impl_->hookRegistry.applyQueuedHooks();
    if (applyResult.isErr()) {
        impl_->logger.error("[Init] MH_ApplyQueued failed");
        return VoidResult::err(applyResult.error());
    }

    // 11. Detection engine (start scanner thread after all hooks are in place)
    impl_->detectionEngine = std::make_unique<DetectionEngine>(*this);
    impl_->detectionEngine->addBlacklistedSignature(BytePattern{
        {
            0x48, 0x8D, 0x64, 0x24, 0x28, 0xC3, 0x00,
            0x48, 0x8D, 0x64, 0x24, 0xD8, 0xC6, 0x05,
        },
        {},
        MG_STR("CheatEngine")
    });
    impl_->detectionEngine->addBlacklistedSignature(BytePattern{
        {
            0xCC, 0xCC, 0xCC, 0xCC, 0x48, 0x89, 0x54, 0x24,
            0x10, 0x4C, 0x89, 0x44, 0x24, 0x18, 0x4C, 0x89,
            0x4C, 0x24, 0x20, 0x48, 0x83, 0xEC, 0x28, 0x4C,
            0x8B, 0xC2, 0x4C, 0x8D, 0x4C, 0x24, 0x40, 0xBA,
            0x04, 0x01, 0x00, 0x00, 0xFF, 0x15, 0xAA, 0x20,
            0x00, 0x00, 0x48, 0x83, 0xC4, 0x28, 0xC3,
        },
        {
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 0, 1,
            1, 1, 1, 1, 1, 1, 1,
        },
        MG_STR("x64dbg")
    });
    impl_->detectionEngine->addBlacklistedSignature(BytePattern{
        {
            0x4C, 0x8B, 0x14, 0x24, 0x4C, 0x8B, 0x5C, 0x24,
            0x08, 0x48, 0x83, 0xC4, 0x10, 0xC3, 0xCC, 0xCC,
            0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74,
            0x24, 0x18, 0x55, 0x57, 0x41, 0x56, 0x48, 0x8B,
            0xEC, 0x48, 0x83, 0xEC, 0x10, 0x33, 0xC0, 0x33,
            0xC9, 0x0F, 0xA2, 0x44, 0x8B, 0xC1, 0x44, 0x8B,
            0xD2, 0x41, 0x81, 0xF2, 0x69, 0x6E, 0x65, 0x49,
            0x41, 0x81, 0xF0, 0x6E, 0x74, 0x65, 0x6C, 0x44,
            0x8B, 0xCB, 0x44, 0x8B, 0xF0, 0x33, 0xC9, 0xB8,
            0x01, 0x00, 0x00, 0x00, 0x0F, 0xA2, 0x45, 0x0B,
            0xD0, 0x89, 0x45, 0xF0, 0x41, 0x81, 0xF1, 0x47,
            0x65, 0x6E, 0x75, 0x89, 0x5D, 0xF4, 0x45, 0x0B,
            0xD1, 0x89, 0x4D, 0xF8, 0x8B, 0xF9, 0x89, 0x55,
            0xFC, 0x75, 0x5B, 0x48, 0x83, 0x0D, 0xE5, 0xFC,
            0x00, 0x00, 0xFF, 0x25, 0xF0, 0x3F, 0xFF, 0x0F,
        },
        {},
        MG_STR("DbgX.Shell.exe")
    });
    {
        auto deResult = impl_->detectionEngine->initialize();
        if (deResult.isErr()) {
            impl_->logger.error("DetectionEngine init failed");
            // Non-fatal: continue without detection engine
        }
    }
    

    impl_->ipcClient.setProgress(98);
    impl_->ipcClient.signalAllReady();

    return VoidResult::ok();
}

void MegaGuardContext::shutdown() {
    if (!impl_) return;
    if (impl_->detectionEngine) impl_->detectionEngine->shutdown();
    //if (impl_->guardRegions) impl_->guardRegions->shutdown();
    if (impl_->manualSeh)    impl_->manualSeh->shutdown();
    impl_->hookRegistry.removeAll();
    impl_->ipcClient.close();
    //impl_->logger.info("MegaGuard shutdown complete");
    //impl_->logger.stop();
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
IntegrityEngine&      MegaGuardContext::integrityEngine()      { return *impl_->integrityEngine; }
IATScrubber&          MegaGuardContext::iatScrubber()          { return *impl_->iatScrubber; }
ManualSEH&            MegaGuardContext::manualSEH()            { return *impl_->manualSeh; }
//GuardRegions&         MegaGuardContext::guardRegions()          { return *impl_->guardRegions; }
game::CryptoEngine&   MegaGuardContext::cryptoEngine()         { return *impl_->cryptoEngine; }
DetectionEngine&      MegaGuardContext::detectionEngine()       { return *impl_->detectionEngine; }
Splash&               MegaGuardContext::splashScreen()         { return impl_->splashScreen; }
game::GameManagerHooks& MegaGuardContext::gameManagers()       { return *impl_->gameManagers; }
game::SecureChannel&  MegaGuardContext::secureChannel()        { return *impl_->secureChannel; }
game::HeartbeatHandler& MegaGuardContext::heartbeatHandler()   { return *impl_->heartbeatHandler; }
game::WeeklyRewardHandler& MegaGuardContext::weeklyRewardHandler() { return *impl_->weeklyRewardHandler; }
game::TradeHandler&   MegaGuardContext::tradeHandler()         { return *impl_->tradeHandler; }
uptr                  MegaGuardContext::moduleBase() const     { return moduleBase_; }

} // namespace mg
