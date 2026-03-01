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
#include "game/managers/game_managers.hpp"
#include "anticheat/manual_seh.hpp"
#include "anticheat/guard_regions.hpp"
#include "anticheat/detection_engine.hpp"
#include "platform/ipc.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"

#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")

namespace mg::entry {

using namespace mg::game;

namespace {

// ── Mapped region info passed by manual-map loader ───────────────────────────
struct MAPPED_REGION_INFO {
	void* base;
	DWORD size;
};

// ── Hidden thread routine ────────────────────────────────────────────────────
// Runs return-address whitelist building + MG_TRY smoke test on background thread.

DWORD WINAPI HiddenThreadRoutine(LPVOID) {
	auto& context = mg::ctx();

	// Build return-address whitelists for game manager hooks
	context.gameManagers().buildWhitelists();

	// Encrypt executable sections in the game EXE only (skip third-party DLLs
	// like audio middleware — encrypting them causes resource-loading hangs).
	context.guardRegions().encryptSection(".text");
	context.guardRegions().startReencryptWorker();


	return 0;
}

// ── NationIndex detection ────────────────────────────────────────────────────
// Reads NationIndex from game memory. If == 20, set ROM flag and write 0.
// Otherwise set non-ROM and write 3.

void DetectNationIndex() {
	auto nationAddr = MG_CONST(addr::features::NationIndex);
	auto nationIndex = mg::readValue<u32>(nationAddr, 0);

	if (nationIndex == 20) {
		addr::config::NationIndexIsRom = true;
		mg::writeValue<u32>(nationAddr, 0, 0u);
	} else {
		addr::config::NationIndexIsRom = false;
		mg::writeValue<u32>(nationAddr, 0, 3u);
	}

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
		DisableThreadLibraryCalls(hModule);

		// ── Determine module region ──────────────────────────────────────
		if (lpReserved)
		{
			// Manual mapped — loader provides region info
			auto* regionInfo = reinterpret_cast<MAPPED_REGION_INFO*>(lpReserved);
			addr::globals::g_AntiCheatModuleBase = reinterpret_cast<uptr>(regionInfo->base);
			addr::globals::g_AntiCheatModuleSize = regionInfo->size;
		}
		else
		{
			// Normal LoadLibrary
			MODULEINFO modInfo = {};
			if (GetModuleInformation(
				GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo)))
			{
				addr::globals::g_AntiCheatModuleBase = reinterpret_cast<uptr>(hModule);
				addr::globals::g_AntiCheatModuleSize = modInfo.SizeOfImage;
			}
			else
			{
				auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
				auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
					reinterpret_cast<BYTE*>(hModule) + dosHeader->e_lfanew);
				addr::globals::g_AntiCheatModuleBase = reinterpret_cast<uptr>(hModule);
				addr::globals::g_AntiCheatModuleSize = ntHeaders->OptionalHeader.SizeOfImage;
			}
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


		// ── NationIndex detection (BEFORE context init, matching original) ───
		DetectNationIndex();

		// ── Initialize context subsystems + install all hooks ────────────
		// This now also installs ManualSEH + GuardRegions (KiUserExceptionDispatcher hook)
		auto result = mg::ctx().initialize();
		if (!result)
			return FALSE;


		// ── Start background thread ──────────────────────────────────────
		HANDLE hWorker = CreateThread(nullptr, 0, HiddenThreadRoutine, nullptr, 0, nullptr);

		// Register threads with detection engine for heartbeat monitoring
		if (hWorker) {
			//mg::ctx().detectionEngine().trackThread(hWorker, "HiddenWorker");
		}

		return TRUE;
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		// ── Cleanup ──────────────────────────────────────────────────────
		auto& context = mg::ctx();
		context.ipcClient().close();
		context.shutdown();

		// Destroy context accessor (clears encrypted pointer)
		mg::destroyContextAccessor();
	}
	return FALSE;
}
