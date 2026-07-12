#include "pch.hpp"
#include "anticheat/guard_regions.hpp"
#include "anticheat/manual_seh.hpp"
#include "anticheat/detection_engine.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "core/version_generated.hpp"
#include "utils/logger.hpp"
#include "engine/hook_engine.hpp"
#include "game/addresses.hpp"
#include "platform/syscall_cloner.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg {

inline constexpr u32 fnv1a(const char* s, u32 h = 0x811C9DC5u) {
    return (*s == '\0') ? h : fnv1a(s + 1, (h ^ static_cast<u32>(*s)) * 0x01000193u);
}
inline constexpr u32 kVersionSalt = fnv1a(MG_VERSION_FULL);

#pragma optimize("", off)
#pragma comment(linker, "/SECTION:.mg,ER")
#pragma code_seg(push, ".mg")

using KiUserExceptionDispatcher_t = void(NTAPI*)(PEXCEPTION_RECORD, PCONTEXT);
using NtContinue_t = NTSTATUS(NTAPI*)(PCONTEXT, BOOLEAN);

using NtProtectVirtualMemory_t = NTSTATUS(NTAPI*)(
    HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);

using NtDelayExecution_t = NTSTATUS(NTAPI*)(BOOLEAN, PLARGE_INTEGER);
using NtFlushInstructionCache_t = NTSTATUS(NTAPI*)(HANDLE, PVOID, SIZE_T);

inline KiUserExceptionDispatcher_t g_OriginalKiDispatcher = nullptr;
inline NtContinue_t                g_NtContinue           = nullptr;
inline NtProtectVirtualMemory_t    g_NtProtect            = nullptr;
inline NtDelayExecution_t          g_NtDelayExecution     = nullptr;
inline NtFlushInstructionCache_t   g_NtFlushICache        = nullptr;
inline u64                         g_CooldownTicks        = 0;
inline u64                         g_TscTicksPerMs        = 1;

inline void acquireSpinLock(volatile LONG* lock) {
    while (InterlockedCompareExchange(lock, 1, 0) != 0)
        YieldProcessor();
}

inline void releaseSpinLock(volatile LONG* lock) {
    InterlockedExchange(lock, 0);
}

inline void sleepMs(u32 ms) {
    LARGE_INTEGER interval;
    interval.QuadPart = -static_cast<LONGLONG>(ms) * 10000;
    g_NtDelayExecution(FALSE, &interval);
}

inline NTSTATUS ntProtect(uptr pageAddr, ULONG newProt, PULONG oldProt) {
    PVOID base = reinterpret_cast<PVOID>(pageAddr);
    SIZE_T regionSize = MG_CONST(0x1000);
    return g_NtProtect(reinterpret_cast<HANDLE>(-1), &base, &regionSize,
                       newProt, oldProt);
}

inline void flushICache(uptr pageAddr) {
    g_NtFlushICache(reinterpret_cast<HANDLE>(-1),
                    reinterpret_cast<PVOID>(pageAddr), MG_CONST(0x1000));
}

bool GuardRegions::sectionNameEquals(const char* a, const char* b) {
    for (int i = 0; i < 8; ++i) {
        if (a[i] != b[i]) return false;
        if (a[i] == '\0' && b[i] == '\0') return true;
    }
    return true;
}

PIMAGE_SECTION_HEADER GuardRegions::getSectionByName(HMODULE hMod, const char* name) {
    auto moduleBase = reinterpret_cast<uptr>(hMod);
    auto dosHeader  = reinterpret_cast<PIMAGE_DOS_HEADER>(moduleBase);
    auto ntHeaders  = reinterpret_cast<PIMAGE_NT_HEADERS>(moduleBase + dosHeader->e_lfanew);
    auto section    = IMAGE_FIRST_SECTION(ntHeaders);

    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i, ++section) {
        if (sectionNameEquals(reinterpret_cast<const char*>(section->Name), name))
            return section;
    }
    return nullptr;
}

void GuardRegions::xorEncryptPage(uptr pageAddr, u32 key) {
    auto* ptr = reinterpret_cast<u32*>(pageAddr);
    for (std::size_t i = 0; i < 0x1000 / sizeof(u32); ++i)
        ptr[i] ^= key;
}

bool GuardRegions::isCallerWhitelisted(uptr eip) const {
    for (int i = 0; i < whitelistCount_; ++i) {
        if (eip >= whitelistedRanges_[i].base && eip < whitelistedRanges_[i].end)
            return true;
    }
    return false;
}

void GuardRegions::addWhitelistRange(uptr base, uptr size) {
    if (whitelistCount_ < kMaxWhitelistRanges)
        whitelistedRanges_[whitelistCount_++] = { base, base + size };
}

void GuardRegions::addExcludedRange(uptr base, uptr size) {
    if (excludedCount_ < kMaxWhitelistRanges)
        excludedRanges_[excludedCount_++] = { base, base + size };
}

bool GuardRegions::isPageExcluded(uptr pageAddr) const {
    for (int i = 0; i < excludedCount_; ++i) {
        if (pageAddr >= excludedRanges_[i].base && pageAddr < excludedRanges_[i].end)
            return true;
    }
    return false;
}

BOOLEAN GuardRegions::dispatchException(
    PEXCEPTION_RECORD exRecord, PCONTEXT ctxRecord)
{
    if (exRecord->ExceptionCode != static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION)) {
        auto* manualSeh = ManualSEH::instance();
        if (manualSeh && manualSeh->handleException(ctxRecord, exRecord))
            return TRUE;
        return FALSE;
    }

    if (exRecord->NumberParameters < 2)
        return FALSE;

    uptr faultAddr = exRecord->ExceptionInformation[1];
    uptr pageAddr  = faultAddr & ~static_cast<uptr>(0xFFF);

    // execution fault: EIP is on the faulting page (instruction fetch)
    // always allow — game must be able to run its own code
    // data-access fault: EIP is elsewhere, gate by whitelist
    //uptr eipPage = ctxRecord->Eip & ~static_cast<uptr>(0xFFF);
    //if (eipPage != pageAddr && !isCallerWhitelisted(ctxRecord->Eip))
    //    return FALSE;

	// brief global lock — map lookup only
	acquireSpinLock(&mapLock_);
	auto it = pageInfo_.find(pageAddr);
	if (it == pageInfo_.end()) {
		releaseSpinLock(&mapLock_);
		return FALSE;
	}
	auto* psd = &it->second;
	releaseSpinLock(&mapLock_);

	// per-page lock — XOR + protection changes
	acquireSpinLock(&psd->lock);

	if (psd->state == PageState::kDecrypted) {
		releaseSpinLock(&psd->lock);
		return (exRecord->ExceptionInformation[0] != 1) ? TRUE : FALSE;
	}

	if (psd->state != PageState::kEncrypted) {
		releaseSpinLock(&psd->lock);
		return FALSE;
	}

	ULONG tmpProt;
    /*
	NTSTATUS status = ntProtect(pageAddr, PAGE_READWRITE, &tmpProt);
	if (status < 0) {
		releaseSpinLock(&psd->lock);
		return FALSE;
	}*/
	//xorEncryptPage(pageAddr, psd->xorKey);
	ntProtect(pageAddr, psd->oldProtect, &tmpProt);
	flushICache(pageAddr);

	psd->lastAccessTick = __rdtsc();
	++psd->accessCount;
	psd->state = PageState::kDecrypted;
	releaseSpinLock(&psd->lock);

	acquireSpinLock(&queueLock_);
	pendingPages_.push_back(pageAddr);
	releaseSpinLock(&queueLock_);

	++dbgDecryptCount_;
	return TRUE;
}

void NTAPI HandleGuardException(PEXCEPTION_RECORD exRecord, PCONTEXT ctxRecord)
{
    auto* self = GuardRegions::instance();
    if (!self) return;
    if (self->dispatchException(exRecord, ctxRecord))
        g_NtContinue(ctxRecord, FALSE);
}

__declspec(naked) void __cdecl KiDispatcherThunk()
{
    __asm {
        mov eax, [esp+4]
        mov ecx, [esp]
        push eax
        push ecx
        call HandleGuardException
        jmp dword ptr [g_OriginalKiDispatcher]
    }
}

void GuardRegions::encryptSection(const char* sectionName) {
    HMODULE hMod = GetModuleHandleW(nullptr);
    auto* section = getSectionByName(hMod, sectionName);
    if (!section) {
		return;
    }

    auto moduleBase = reinterpret_cast<uptr>(hMod);

    DWORD prot = PAGE_READONLY;
    DWORD chars = section->Characteristics;
    bool exec  = (chars & IMAGE_SCN_MEM_EXECUTE) != 0;
    bool write = (chars & IMAGE_SCN_MEM_WRITE) != 0;
    if (exec && write)      prot = PAGE_EXECUTE_READWRITE;
    else if (exec)          prot = PAGE_EXECUTE_READ;
    else if (write)         prot = PAGE_READWRITE;

    registerPages(moduleBase + section->VirtualAddress, section->Misc.VirtualSize, prot);
}

void GuardRegions::registerPages(uptr base, std::size_t size, DWORD originalProtect) {
    uptr alignedBase = base & ~static_cast<uptr>(0xFFF);
    uptr alignedEnd  = (base + size + 0xFFF) & ~static_cast<uptr>(0xFFF);
    const std::size_t pageCount = (alignedEnd - alignedBase) / 0x1000;
    u64 now = __rdtsc();

    acquireSpinLock(&mapLock_);
    pageInfo_.reserve(pageInfo_.size() + pageCount);
    releaseSpinLock(&mapLock_);

    acquireSpinLock(&queueLock_);
    pendingPages_.reserve(pendingPages_.size() + pageCount);
    releaseSpinLock(&queueLock_);

    for (std::size_t i = 0; i < pageCount; ++i) {
        uptr pageAddr = alignedBase + i * 0x1000;

        //if (isPageExcluded(pageAddr))
        //    continue;

        u32 xorKey = static_cast<u32>(__rdtsc() ^ pageAddr ^ (i * 0x9E3779B9) ^ kVersionSalt);
        if (xorKey == 0) xorKey = 0xDEADBEEF;

        // insert into map under global lock — NO ntProtect here
        acquireSpinLock(&mapLock_);
        auto& entry = pageInfo_[pageAddr];
        entry = { 0, PageState::kDecrypted, originalProtect, xorKey, now, 0 };
        auto* psd = &entry;
        releaseSpinLock(&mapLock_);

        // encrypt under per-page lock only — global lock NOT held
        acquireSpinLock(&psd->lock);
        ULONG tmpProt;
        /*
        if (ntProtect(pageAddr, PAGE_READWRITE, &tmpProt) < 0) {
            releaseSpinLock(&psd->lock);
            continue;
        }
        xorEncryptPage(pageAddr, xorKey);
        */
        ntProtect(pageAddr, PAGE_NOACCESS, &tmpProt);
        psd->state = PageState::kEncrypted;
        releaseSpinLock(&psd->lock);
    }

    //ctx_.logger().info("[GuardRegions] registered {} pages at 0x{:08X} (size=0x{:X} prot=0x{:X}) total={}",
    //    pageCount, alignedBase, alignedEnd - alignedBase, originalProtect, pageInfo_.size());
}

DWORD WINAPI GuardRegions::reencryptThreadProc(LPVOID param) {
    auto* self = static_cast<GuardRegions*>(param);

    std::vector<uptr> localBatch;
    localBatch.reserve(MG_CONST(4096));

    //self->ctx_.logger().info("[GuardRegions] worker started, cooldownTicks={} ticksPerMs={}",
    //    g_CooldownTicks, g_TscTicksPerMs);

	while (self->running_) {
		sleepMs(static_cast<u32>(MG_CONST(1000)));

		acquireSpinLock(&self->queueLock_);
		localBatch.swap(self->pendingPages_);
		releaseSpinLock(&self->queueLock_);

		if (localBatch.empty())
			continue;

		u64 now = __rdtsc();

		for (uptr pageAddr : localBatch) {
			// brief global lock — map lookup only
			acquireSpinLock(&self->mapLock_);
			auto it = self->pageInfo_.find(pageAddr);
			if (it == self->pageInfo_.end()) {
				releaseSpinLock(&self->mapLock_);
				continue;
			}
			auto* psd = &it->second;
			releaseSpinLock(&self->mapLock_);

			// per-page lock — state check + XOR
			acquireSpinLock(&psd->lock);

			if (psd->state != PageState::kDecrypted) {
				releaseSpinLock(&psd->lock);
				continue;
			}

			if (now - psd->lastAccessTick < g_CooldownTicks) {
				releaseSpinLock(&psd->lock);
				acquireSpinLock(&self->queueLock_);
				self->pendingPages_.push_back(pageAddr);
				releaseSpinLock(&self->queueLock_);
				continue;
			}

			u32 access = psd->accessCount;
			u64 decryptedMs = (now - psd->lastAccessTick) / g_TscTicksPerMs;

			ULONG tmpProt;
            /*
			NTSTATUS status = ntProtect(pageAddr, PAGE_READWRITE, &tmpProt);
			if (status < 0) {
				releaseSpinLock(&psd->lock);
				continue;
			}
			xorEncryptPage(pageAddr, psd->xorKey);
            */
			ntProtect(pageAddr, PAGE_NOACCESS, &tmpProt);
			psd->state = PageState::kEncrypted;

			releaseSpinLock(&psd->lock);

			//self->ctx_.logger().info("[GuardRegions] 0x{:08X} re-locked | {}ms | #{}",
			//	pageAddr, decryptedMs, access);
			++self->dbgEncryptCount_;
		}
		localBatch.clear();
	}
    return 0;
}

void GuardRegions::startReencryptWorker() {
    if (workerThread_) return;
    running_ = true;
    workerThread_ = CreateThread(nullptr, 0, reencryptThreadProc, this, 0, nullptr);
    if (workerThread_)
        ctx_.detectionEngine().trackThread(workerThread_, MG_STR("GuardRegionsWorker"));
}

#pragma code_seg(pop)
#pragma optimize("", on)

GuardRegions::GuardRegions(MegaGuardContext& ctx)
    : ctx_(ctx)
{
    pendingPages_.reserve(MG_CONST(4096));
}

GuardRegions::~GuardRegions() {
    shutdown();
}

GuardRegions* GuardRegions::instance() {
    return s_instance;
}

VoidResult GuardRegions::initialize() {
    //ctx_.logger().info("[GuardRegions] initialize start");

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
        return VoidResult::err(ErrorCode::kModuleNotFound);

    auto& cloner = ctx_.syscallCloner();

    g_NtContinue = reinterpret_cast<NtContinue_t>(
        cloner.cloneSyscallByName<void*>("NtContinue"));
    if (!g_NtContinue)
        return VoidResult::err(ErrorCode::kProcNotFound);

    g_NtProtect = reinterpret_cast<NtProtectVirtualMemory_t>(
        cloner.cloneSyscallByName<void*>("NtProtectVirtualMemory"));
    if (!g_NtProtect)
        return VoidResult::err(ErrorCode::kProcNotFound);

    g_NtDelayExecution = reinterpret_cast<NtDelayExecution_t>(
        cloner.cloneSyscallByName<void*>("NtDelayExecution"));
    if (!g_NtDelayExecution)
        return VoidResult::err(ErrorCode::kProcNotFound);

    g_NtFlushICache = reinterpret_cast<NtFlushInstructionCache_t>(
        cloner.cloneSyscallByName<void*>("NtFlushInstructionCache"));
    if (!g_NtFlushICache)
        return VoidResult::err(ErrorCode::kProcNotFound);

    u64 tscStart = __rdtsc();
    sleepMs(static_cast<u32>(MG_CONST(16)));
    u64 tscEnd = __rdtsc();
    g_TscTicksPerMs = (tscEnd - tscStart) / MG_CONST(16);
    if (g_TscTicksPerMs == 0) g_TscTicksPerMs = 1;
    g_CooldownTicks = g_TscTicksPerMs * MG_CONST(1000);

    auto kiDispatcherAddr = reinterpret_cast<uptr>(
        GetProcAddress(ntdll, "KiUserExceptionDispatcher"));
    if (!kiDispatcherAddr)
        return VoidResult::err(ErrorCode::kProcNotFound);
    kiDispatcherHook_ = std::make_unique<DetourHook>();
    if (!kiDispatcherHook_->create(kiDispatcherAddr, KiDispatcherThunk)) {
        kiDispatcherHook_.reset();
        return VoidResult::err(ErrorCode::kHookFailed);
    }

    g_OriginalKiDispatcher = kiDispatcherHook_->getOriginal<KiUserExceptionDispatcher_t>();

    s_instance = this;
    return VoidResult::ok();
}

void GuardRegions::shutdown() {
    running_ = false;

    if (workerThread_) {
        WaitForSingleObject(workerThread_, 2000);
        CloseHandle(workerThread_);
        workerThread_ = nullptr;
    }
    u32 restored = 0;
    acquireSpinLock(&mapLock_);
    for (auto& [pageAddr, psd] : pageInfo_) {
        if (psd.state == PageState::kEncrypted) {
            ULONG tmpProt;
            //ntProtect(pageAddr, PAGE_READWRITE, &tmpProt);
            //xorEncryptPage(pageAddr, psd.xorKey);
            ntProtect(pageAddr, psd.oldProtect, &tmpProt);
            //flushICache(pageAddr);
            psd.state = PageState::kDecrypted;
            ++restored;
        }
    }
    releaseSpinLock(&mapLock_);

    // remove hook BEFORE clearing map — any late fault still finds
    // a kDecrypted entry and returns harmlessly instead of crashing
    if (kiDispatcherHook_) {
        kiDispatcherHook_->remove();
        kiDispatcherHook_.reset();
    }

    // hook is gone, no more exceptions — safe to destroy
    pageInfo_.clear();
    pendingPages_.clear();

    g_OriginalKiDispatcher = nullptr;
    g_NtContinue = nullptr;
    g_NtProtect = nullptr;
    g_NtDelayExecution = nullptr;
    g_NtFlushICache = nullptr;

    if (s_instance == this)
        s_instance = nullptr;
}

} // namespace mg
