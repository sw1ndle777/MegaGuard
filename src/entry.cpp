// =============================================================================
// entry.cpp - DLL entry point for MegaGuard anticheat
// =============================================================================
// Mapped-DLL-safe patterns:
//   - mg::initContextAccessor() instead of static g_ctx
//   - VirtualAlloc'd MegaGuardContext
//   - KiUserExceptionDispatcher hook (GuardRegions) for SEH4 + page guards
//   - ManualSEH for MG_TRY/MG_EXCEPT in manually mapped code
//   - NationIndex ROM detection at startup
//   - IPC client for loader progress reporting
//   - All addresses via MG_CONST, all strings via MG_STR
//   - Import hiding via MG_IMPORT, obfuscated comparisons / arithmetic
// =============================================================================
#include "pch.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "game/managers/game_managers.hpp"
#include "anticheat/manual_seh.hpp"
#include "anticheat/guard_regions.hpp"
#include "anticheat/detection_engine.hpp"
#include "platform/ipc.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"

// ── Local debug mode ─────────────────────────────────────────────────────────
// Set to 1: rename this DLL to megaguard.dll, drop it next to the game exe.
// Game loads it directly via LoadLibraryA — no bootstrapper, no manual map,
// no VMProtect. Crash produces a full minidump with symbols you can open in
// Visual Studio or WinDbg. Build as Release with PDB.
#ifndef MG_LOCAL_DEBUG
    #define MG_LOCAL_DEBUG 0
#endif

#include <Windows.h>
#include <Psapi.h>

#if MG_LOCAL_DEBUG
#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp.lib")
#endif

#define DISCORD_DISABLE_IO_THREAD

#include <discord_rpc.h>
#include <discord_register.h>
#pragma comment(lib, "Psapi.lib")

namespace {

// ── Crash handler: VEH-based (runs before VMProtect's handler) ──────────────

static PVOID g_CrashVehHandle = nullptr;
static volatile LONG g_CrashLogged = 0;

static bool IsFatalException(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case 0xC0000374: // STATUS_HEAP_CORRUPTION
        return true;
    default:
        return false;
    }
}

#if MG_LOCAL_DEBUG

LONG CALLBACK MegaGuardCrashFilter(PEXCEPTION_POINTERS ep) {
    if (!IsFatalException(ep->ExceptionRecord->ExceptionCode))
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedCompareExchange(&g_CrashLogged, 1, 0) != 0)
        return EXCEPTION_CONTINUE_SEARCH;
    CreateDirectoryA("MegaGuard", nullptr);

    HANDLE hFile = CreateFileA("MegaGuard\\crash.dmp", GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;

        // Full memory: captures the manually-mapped module's code + all data/heap so the
        // dump is self-contained (the mapped payload isn't a real module on disk, so a
        // normal stack-only dump can't be symbolized/disassembled afterwards).
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
            static_cast<MINIDUMP_TYPE>(MiniDumpWithFullMemory |
                                       MiniDumpWithFullMemoryInfo |
                                       MiniDumpWithHandleData |
                                       MiniDumpWithThreadInfo |
                                       MiniDumpWithUnloadedModules),
            &mei, nullptr, nullptr);
        CloseHandle(hFile);
    }

    // Also write a quick text summary next to the dump
    hFile = CreateFileA("MegaGuard\\crash.log", GENERIC_WRITE, 0, nullptr,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        char buf[512];
        int len = wsprintfA(buf,
            "ExceptionCode: 0x%08X\r\n"
            "EIP: 0x%08X\r\n"
            "MegaGuard Base: 0x%08X  Size: 0x%08X\r\n"
            "Open crash.dmp in Visual Studio with the PDB next to it.\r\n",
            ep->ExceptionRecord->ExceptionCode,
            ep->ContextRecord->Eip,
            (DWORD)mg::game::addr::globals::g_AntiCheatModuleBase,
            (DWORD)mg::game::addr::globals::g_AntiCheatModuleSize);
        DWORD written;
        WriteFile(hFile, buf, len, &written, nullptr);
        CloseHandle(hFile);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

#else // Production: text log with stack scan

static const char* IdentifyModule(DWORD addr, DWORD& outOffset) {
    auto acBase = mg::game::addr::globals::g_AntiCheatModuleBase;
    auto acSize = mg::game::addr::globals::g_AntiCheatModuleSize;
    if (acBase && addr >= acBase && addr < acBase + acSize) {
        outOffset = addr - static_cast<DWORD>(acBase);
        return "MegaGuard";
    }

    auto gameBase = mg::game::addr::globals::g_GameModuleBase;
    auto gameSize = mg::game::addr::globals::g_GameModuleSize;
    if (gameBase && addr >= gameBase && addr < gameBase + gameSize) {
        outOffset = addr - static_cast<DWORD>(gameBase);
        return "Game";
    }

    HMODULE hMod = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)addr, &hMod);
    if (hMod) {
        outOffset = addr - (DWORD)hMod;
        static char modName[MAX_PATH];
        GetModuleFileNameA(hMod, modName, MAX_PATH);
        char* slash = modName;
        for (char* p = modName; *p; ++p)
            if (*p == '\\' || *p == '/') slash = p + 1;
        return slash;
    }

    outOffset = addr;
    return "Unknown";
}

LONG CALLBACK MegaGuardCrashFilter(PEXCEPTION_POINTERS ep) {
    if (!IsFatalException(ep->ExceptionRecord->ExceptionCode))
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedCompareExchange(&g_CrashLogged, 1, 0) != 0)
        return EXCEPTION_CONTINUE_SEARCH;

    CreateDirectoryA("MegaGuard", nullptr);

    HANDLE hFile = CreateFileA("MegaGuard\\crash.log", GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return EXCEPTION_CONTINUE_SEARCH;

    char buf[4096];
    int len = 0;

    len += wsprintfA(buf + len,
        "=== MegaGuard Crash Report ===\r\n"
        "ExceptionCode: 0x%08X\r\n"
        "ExceptionAddr: 0x%08X\r\n"
        "EIP: 0x%08X\r\n"
        "EAX: 0x%08X  ECX: 0x%08X  EDX: 0x%08X\r\n"
        "EBX: 0x%08X  ESP: 0x%08X  EBP: 0x%08X\r\n"
        "ESI: 0x%08X  EDI: 0x%08X\r\n",
        ep->ExceptionRecord->ExceptionCode,
        (DWORD)ep->ExceptionRecord->ExceptionAddress,
        ep->ContextRecord->Eip,
        ep->ContextRecord->Eax, ep->ContextRecord->Ecx, ep->ContextRecord->Edx,
        ep->ContextRecord->Ebx, ep->ContextRecord->Esp, ep->ContextRecord->Ebp,
        ep->ContextRecord->Esi, ep->ContextRecord->Edi);

    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        ep->ExceptionRecord->NumberParameters >= 2) {
        len += wsprintfA(buf + len,
            "AccessType: %s\r\nAccessAddr: 0x%08X\r\n",
            ep->ExceptionRecord->ExceptionInformation[0] ? "WRITE" : "READ",
            (DWORD)ep->ExceptionRecord->ExceptionInformation[1]);
    }

    DWORD faultOffset = 0;
    const char* faultMod = IdentifyModule(ep->ContextRecord->Eip, faultOffset);
    len += wsprintfA(buf + len,
        "FaultModule: %s\r\nFaultOffset: 0x%08X\r\n",
        faultMod, faultOffset);

    len += wsprintfA(buf + len,
        "\r\n=== Module Info ===\r\n"
        "MegaGuard Base: 0x%08X  Size: 0x%08X\r\n"
        "Game Base:      0x%08X  Size: 0x%08X\r\n",
        (DWORD)mg::game::addr::globals::g_AntiCheatModuleBase,
        (DWORD)mg::game::addr::globals::g_AntiCheatModuleSize,
        (DWORD)mg::game::addr::globals::g_GameModuleBase,
        (DWORD)mg::game::addr::globals::g_GameModuleSize);

    len += wsprintfA(buf + len, "\r\n=== Stack Scan ===\r\n");

    DWORD esp = ep->ContextRecord->Esp;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<void*>(esp), &mbi, sizeof(mbi)))
        mbi.RegionSize = 0x10000;

    DWORD stackTop = esp + static_cast<DWORD>(mbi.RegionSize);
    DWORD stackLimit = (stackTop < esp + 0x10000) ? stackTop : esp + 0x10000;
    int entries = 0;

    for (DWORD addr = esp; addr < stackLimit - 3 && entries < 48 && len < 3600; addr += 4) {
        DWORD val = 0;
        __try {
            val = *reinterpret_cast<DWORD*>(addr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            break;
        }

        if (val < 0x10000) continue;

        DWORD frameOffset = 0;
        const char* frameMod = IdentifyModule(val, frameOffset);
        if (frameMod[0] == 'U') continue;

        len += wsprintfA(buf + len, "  ESP+0x%04X: 0x%08X  %s+0x%08X\r\n",
            addr - esp, val, frameMod, frameOffset);
        ++entries;
    }

    DWORD written;
    WriteFile(hFile, buf, len, &written, nullptr);
    CloseHandle(hFile);

    return EXCEPTION_CONTINUE_SEARCH;
}

#endif // MG_LOCAL_DEBUG

} // anonymous namespace

namespace mg::entry {

using namespace mg::game;

namespace {

// ── Mapped region info passed by manual-map loader ───────────────────────────
struct MAPPED_REGION_INFO {
	void* base;
	DWORD size;
};

inline HANDLE g_HiddenWorkerThread = nullptr;
inline std::atomic<bool> g_HiddenWorkerRunning{ false };

using tGetUnitContainer = CUnitContainer * (__cdecl*)();
using tGetCRoom = void* (__cdecl*)();

constexpr DWORD kDiscordUpdateIntervalMs = 1000;
constexpr std::size_t kNicknameMaxLength = 16;

enum Characters : i32 {
	Naomi = 0,
	Kai = 1,
	Pandora = 2,
	CHIP = 3,
	Knox = 4,
};

struct CharacterAsset {
	const char* imageKey;
	const char* imageText;
};

struct DiscordPresenceState {
	u32 currentGameState = GAME_STATE_NONE;
	bool inMatch = false;
	int64_t sessionStartTimestamp = 0;
	int64_t matchStartTimestamp = 0;
};

enum MatchModeIndex : u8 {
	TeamDeathMatch = 0,
	FreeForAll = 1,
	ItemMatch = 2,
	CaptureTheBattery = 3,
	CloseCombat = 4,
	Elimination = 5,
	SuperItemMatch = 6,
	ZombieMode = 7,
	ArmsRace = 8,
	Scrimmage = 9,
	BombBattle = 10,
	BossBattle = 11,
	Unknown12 = 12,
	ClanCaptureTheBattery = 13,
	ClanElimination = 14,
	ClanTeamDeathMatch = 15,
	ClanBombBattle = 16,
	ClanRandom = 17,
};

const char* kMatchModeNames[] = {
	MG_STR("Team Death Match"),
	MG_STR("Free For All"),
	MG_STR("Item Match"),
	MG_STR("Capture The Battery"),
	MG_STR("Close Combat"),
	MG_STR("Elimination"),
	MG_STR("Super Item Match"),
	MG_STR("Zombie Mode"),
	MG_STR("Arms Race"),
	MG_STR("Scrimmage"),
	MG_STR("Bomb Battle"),
	MG_STR("Boss Battle"),
	MG_STR("Unknown 12"),
	MG_STR("Clan Capture The Battery"),
	MG_STR("Clan Elimination"),
	MG_STR("Clan Team Death Match"),
	MG_STR("Clan Bomb Battle"),
	MG_STR("Clan Random"),
};

void CopyGameString(char* dst, std::size_t dstSize, const char* src, std::size_t srcMaxLen) {
	if (!dst || dstSize == 0)
		return;

	nocrtMemset(dst, 0, dstSize);
	if (!src)
		return;

	std::size_t i = 0;
	for (; i + 1 < dstSize && i < srcMaxLen && src[i] != '\0'; ++i)
		dst[i] = src[i];

	dst[i] = '\0';
}

u32 GetCurrentGameState() {
	return mg::readValue<u32>(MG_CONST(addr::ui::state_mgr::CurrGameState), 0);
}

bool IsMatchState(u32 gameState) {
	return gameState == GAME_STATE_MOD_PLAYING;
}

const char* GetPresenceStateText(u32 gameState) {
	if (gameState == GAME_STATE_MOD_PLAYING)
		return MG_STR("Playing");

	if (gameState <= GAME_STATE_SELECT_SERVER)
		return MG_STR("Loading");

	return MG_STR("Idle");
}

const char* GetMatchModeName(i32 modIndex) {
	if (modIndex < 0 || modIndex >= static_cast<i32>(sizeof(kMatchModeNames) / sizeof(kMatchModeNames[0])))
		return MG_STR("Unknown Mode");

	return kMatchModeNames[modIndex];
}

CRoom* GetCurrentRoom() {
	auto GetCRoom = reinterpret_cast<tGetCRoom>(
		MG_CONST(addr::anticheat::game_managers::room::Get));
	if (!GetCRoom)
		return nullptr;

	return reinterpret_cast<CRoom*>(GetCRoom());
}

bool TryGetCurrentMatchModeText(char* dst, std::size_t dstSize) {
	if (!dst || dstSize == 0)
		return false;

	nocrtMemset(dst, 0, dstSize);

	auto* room = GetCurrentRoom();
	if (!room || !room->m_kCurrentMod)
		return false;

	const i32 modIndex = mg::callVFunc<i32>(room->m_kCurrentMod, 11);
	CopyGameString(dst, dstSize, GetMatchModeName(modIndex), 31);
	return dst[0] != '\0';
}

CharacterAsset GetCharacterAsset(i32 characterType) {
	switch (characterType) {
	case Naomi:
		return { MG_STR("naomi"), MG_STR("Naomi") };
	case Kai:
		return { MG_STR("kai"), MG_STR("Kai") };
	case Pandora:
		return { MG_STR("pandora"), MG_STR("Pandora") };
	case CHIP:
		return { MG_STR("chip"), MG_STR("CHIP") };
	case Knox:
		return { MG_STR("knox"), MG_STR("Knox") };
	default:
		return { MG_STR("logo"), MG_STR("MegaVolts Online") };
	}
}

CExPlayer* GetLocalExPlayer() {
	auto GetUnitContainer = reinterpret_cast<tGetUnitContainer>(
		MG_CONST(addr::anticheat::game_managers::unit_container::Get));
	if (!GetUnitContainer)
		return nullptr;

	auto* unitContainer = GetUnitContainer();
	if (!unitContainer)
		return nullptr;

	auto* gamePlayer = unitContainer->m_pGamePlayer[5];
	if (!gamePlayer)
		return nullptr;

	return gamePlayer->m_pExPlayer;
}

void UpdateDiscordPresence(DiscordPresenceState& presenceState) {
	const u32 gameState = GetCurrentGameState();
	const bool inMatch = IsMatchState(gameState);
	const auto now = static_cast<int64_t>(std::time(nullptr));

	if (presenceState.sessionStartTimestamp == 0)
		presenceState.sessionStartTimestamp = now;
	if (presenceState.matchStartTimestamp == 0)
		presenceState.matchStartTimestamp = now;

	if (gameState != presenceState.currentGameState) {
		if (inMatch && !presenceState.inMatch)
			presenceState.matchStartTimestamp = now;

		presenceState.currentGameState = gameState;
		presenceState.inMatch = inMatch;
	}

	char nicknameBuffer[32] = {};
	char stateBuffer[64] = {};
	char modeBuffer[40] = {};
	const char* detailsText = MG_STR("");
	const char* stateText = GetPresenceStateText(gameState);
	CharacterAsset imageAsset{ MG_STR("logo"), MG_STR("MegaVolts Online") };

	if (gameState > GAME_STATE_SELECT_SERVER) {
		auto* exPlayer = GetLocalExPlayer();
		if (exPlayer) {
			CopyGameString(nicknameBuffer, sizeof(nicknameBuffer), exPlayer->nickname, kNicknameMaxLength);
			if (nicknameBuffer[0] != '\0')
				detailsText = nicknameBuffer;

			if (inMatch && exPlayer->m_kRoomInfo) {
				const auto characterType = static_cast<i32>((exPlayer->m_kRoomInfo->m_iCharacterInfo >> 7) & 0x1F);
				imageAsset = GetCharacterAsset(characterType);
			}
		}
	}

	if (inMatch && TryGetCurrentMatchModeText(modeBuffer, sizeof(modeBuffer))) {
		nocrtStrcpy(stateBuffer, MG_STR("Playing "));
		nocrtStrcat(stateBuffer, modeBuffer);
		stateText = stateBuffer;
	}

	DiscordRichPresence presence{};
	DiscordButton buttons[] = {
		{ MG_STR("Discord"), MG_STR("https://discord.gg/megavolts") },
		{ MG_STR("Website"), MG_STR("https://megavolts.online") },
	};
	presence.state = stateText;
	presence.details = detailsText;
	presence.startTimestamp = inMatch
		? presenceState.matchStartTimestamp
		: presenceState.sessionStartTimestamp;
	presence.largeImageKey = imageAsset.imageKey;
	presence.largeImageText = imageAsset.imageText;
	presence.buttons = buttons;
	presence.instance = 0;

	Discord_UpdatePresence(&presence);
}

void RunDiscordPresenceLoop() {
	DiscordEventHandlers handlers{};
	Discord_Initialize(MG_STR("1416530750144122970"), &handlers, 0, nullptr);

	DiscordPresenceState presenceState{};
	presenceState.sessionStartTimestamp = static_cast<int64_t>(std::time(nullptr));
	presenceState.matchStartTimestamp = presenceState.sessionStartTimestamp;
	presenceState.currentGameState = GetCurrentGameState();
	presenceState.inMatch = IsMatchState(presenceState.currentGameState);
	if (presenceState.inMatch)
		presenceState.matchStartTimestamp = presenceState.sessionStartTimestamp;

	while (g_HiddenWorkerRunning.load(std::memory_order_acquire)) {
		Discord_UpdateConnection();
		Discord_RunCallbacks();
		UpdateDiscordPresence(presenceState);
		Sleep(kDiscordUpdateIntervalMs);
	}

	Discord_ClearPresence();
	Discord_Shutdown();
}



void StopHiddenWorker() {
	g_HiddenWorkerRunning.store(false, std::memory_order_release);

	if (!g_HiddenWorkerThread)
		return;

	WaitForSingleObject(g_HiddenWorkerThread, 2000);
	CloseHandle(g_HiddenWorkerThread);
	g_HiddenWorkerThread = nullptr;
}

// ── Hidden thread routine ────────────────────────────────────────────────────
// Runs return-address whitelist building + MG_TRY smoke test on background thread.

DWORD WINAPI HiddenThreadRoutine(LPVOID) {
	// Whitelists are already built during GameManagerHooks::install() before
	// hooks go live.  Calling buildWhitelists() again here races with hook
	// callbacks that read the set concurrently (data race → crash).

	// Encrypt executable sections in the game EXE only (skip third-party DLLs
	// like audio middleware — encrypting them causes resource-loading hangs).
	//context.guardRegions().encryptSection(".text");
	//context.guardRegions().startReencryptWorker();

	RunDiscordPresenceLoop();

	return 0;
}

// ── Early patches ────────────────────────────────────────────────────────────

void ApplyEarlyPatches() {
	mg::writeValue<u32>(MG_CONST(addr::anticheat::option_ddd::TutorialSkip), 0, 2);
}

} // anonymous namespace

} // namespace mg::entry

// ── DllMain ──────────────────────────────────────────────────────────────────

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	using namespace mg;
	using namespace mg::entry;
	using namespace mg::game;

	if (dwReason == DLL_PROCESS_ATTACH)
	{
		g_CrashVehHandle = AddVectoredExceptionHandler(0, MegaGuardCrashFilter);

		HMODULE hCheck = nullptr;
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCSTR>(hModule), &hCheck) && hCheck == hModule) {
			DisableThreadLibraryCalls(hModule);
		}

		// ── Determine module region ──────────────────────────────────────
		if (lpReserved)
		{
			auto* regionInfo = reinterpret_cast<MAPPED_REGION_INFO*>(lpReserved);
			addr::globals::g_AntiCheatModuleBase = reinterpret_cast<uptr>(regionInfo->base);
			addr::globals::g_AntiCheatModuleSize = regionInfo->size;
		}
		else
		{
			auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(hModule);
			auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>((BYTE*)hModule + dos->e_lfanew);
			addr::globals::g_AntiCheatModuleBase = reinterpret_cast<uptr>(hModule);
			addr::globals::g_AntiCheatModuleSize = nt->OptionalHeader.SizeOfImage;
		}

		// ── Ensure our mapped region is executable ───────────────────────
		// BlackBone manual mapping may leave sections as PAGE_READWRITE.
		// MinHook's IsExecutableAddress checks both target AND detour, so
		// our hook functions must be in executable memory.
		if (addr::globals::g_AntiCheatModuleBase && addr::globals::g_AntiCheatModuleSize) {
			DWORD oldProt;
			VirtualProtect(
				reinterpret_cast<void*>(addr::globals::g_AntiCheatModuleBase),
				addr::globals::g_AntiCheatModuleSize,
				PAGE_EXECUTE_READWRITE,
				&oldProt);
		}

		// ── Resolve game module base ─────────────────────────────────────
		HMODULE gameModule = GetModuleHandleW(nullptr);

		MODULEINFO gameModInfo = {};

		if (GetModuleInformation(
			GetCurrentProcess(), gameModule, &gameModInfo, sizeof(gameModInfo)))
		{
			addr::globals::g_GameModuleBase = reinterpret_cast<uptr>(gameModule);
			addr::globals::g_GameModuleSize = gameModInfo.SizeOfImage;
		}


		// ── Allocate context via VirtualAlloc (mapped-DLL safe) ──────────
		auto* rawCtx = static_cast<MegaGuardContext*>(
			VirtualAlloc(nullptr, sizeof(MegaGuardContext), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
			);
		if (!rawCtx)
			return FALSE;

		new (rawCtx) MegaGuardContext(addr::globals::g_AntiCheatModuleBase);
		mg::initContextAccessor(rawCtx);
		//OutputDebugStringA("[MegaGuard] Context allocated, accessor init\n");

		ApplyEarlyPatches();


		// ── Initialize context subsystems + install all hooks ────────────
		// This now also installs ManualSEH + GuardRegions (KiUserExceptionDispatcher hook)
		auto result = mg::ctx().initialize();
		if (!result)
			return FALSE;
		//OutputDebugStringA("[MegaGuard] initialize() complete\n");

		/*
		DWORD_PTR processAffinityMask = 0;
		DWORD_PTR systemAffinityMask = 0;

		if (GetProcessAffinityMask(GetCurrentProcess(), &processAffinityMask, &systemAffinityMask))
		{
			processAffinityMask &= ~(1ull << 1);
			SetProcessAffinityMask(GetCurrentProcess(), processAffinityMask);
		}
		*/


#if MG_LOCAL_DEBUG
		// Local debug: we ARE the bootstrapper — signal the game to continue
		{
			HANDLE hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, "Global\\megaguardInitialize");
			if (hEvent) { SetEvent(hEvent); CloseHandle(hEvent); }
		}
#endif

		// ── Start background thread ──────────────────────────────────────
		g_HiddenWorkerRunning.store(true, std::memory_order_release);
		g_HiddenWorkerThread = CreateThread(nullptr, 0, HiddenThreadRoutine, nullptr, 0, nullptr);
		HANDLE hWorker = g_HiddenWorkerThread;
		if (!hWorker)
			g_HiddenWorkerRunning.store(false, std::memory_order_release);

		// Register threads with detection engine for heartbeat monitoring
		if (hWorker) {
			mg::ctx().detectionEngine().trackThread(hWorker, MG_STR("HiddenWorker"));
		}

		return TRUE;
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		// ── Cleanup ──────────────────────────────────────────────────────
		StopHiddenWorker();

		auto& context = mg::ctx();
		context.ipcClient().close();
		context.shutdown();

		mg::destroyContextAccessor();

		if (g_CrashVehHandle) {
			RemoveVectoredExceptionHandler(g_CrashVehHandle);
			g_CrashVehHandle = nullptr;
		}
	}
	return FALSE;
}
