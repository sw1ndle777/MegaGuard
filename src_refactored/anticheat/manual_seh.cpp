// =============================================================================
// ManualSEH - Implementation
// =============================================================================
#include "pch.hpp"
#include "anticheat/manual_seh.hpp"
#include "utils/cloakwork_isolation.hpp"

#ifndef ROUND_TO_PAGES
#define ROUND_TO_PAGES(_) (((_) + 0xFFF) & ~(0xFFF))
#endif

namespace mg {

ManualSEH* ManualSEH::s_instance = nullptr;

ManualSEH* ManualSEH::instance() {
    return s_instance;
}

ManualSEH::~ManualSEH() {
    shutdown();
}

VoidResult ManualSEH::initialize() {

    u32 entryLen = ROUND_TO_PAGES(MG_MANUAL_SEH_MAX_ENTRIES * sizeof(ManualSEHEntry));
    entries_ = static_cast<ManualSEHEntry*>(
        VirtualAlloc(nullptr, entryLen, MEM_COMMIT, PAGE_READWRITE));
    if (!entries_)
        return VoidResult::err(ErrorCode::kAllocationFail);
    RtlZeroMemory(entries_, entryLen);

    u32 recordLen = ROUND_TO_PAGES(MG_MANUAL_SEH_MAX_ENTRIES * sizeof(ManualSEHRecord));
    records_ = static_cast<ManualSEHRecord*>(
        VirtualAlloc(nullptr, recordLen, MEM_COMMIT, PAGE_READWRITE));
    if (!records_) {
        VirtualFree(entries_, 0, MEM_RELEASE);
        entries_ = nullptr;
        return VoidResult::err(ErrorCode::kAllocationFail);
    }
    RtlZeroMemory(records_, recordLen);

    s_instance = this;
    return VoidResult::ok();
}

void ManualSEH::shutdown() {
    if (entries_) {
        VirtualFree(entries_, 0, MEM_RELEASE);
        entries_ = nullptr;
    }
    if (records_) {
        VirtualFree(records_, 0, MEM_RELEASE);
        records_ = nullptr;
    }
    if (s_instance == this)
        s_instance = nullptr;
}

// ── Entry push / pop ──────────────────────────────────────────────────────────

BOOLEAN ManualSEH::pushEntry(PCONTEXT contextRecord, HANDLE threadId) {
    if (!entries_) return FALSE;

    while (_InterlockedExchange8(reinterpret_cast<CHAR*>(&pushLock_), TRUE) == TRUE)
        _mm_pause();

    BOOLEAN result = FALSE;
    for (u32 i = 0; MG_LT(i, MG_MANUAL_SEH_MAX_ENTRIES); ++i) {
        auto& e = entries_[i];
        if(!e.active) {
            RtlCopyMemory(&e.savedContext, contextRecord, sizeof(CONTEXT));
            e.active   = TRUE;
            e.threadId = threadId;
            result = TRUE;
            break;
        }
    }

    pushLock_ = FALSE;
    return result;
}

BOOLEAN ManualSEH::popEntry(HANDLE threadId) {
    if (!entries_) return FALSE;

    for (u32 i = MG_MANUAL_SEH_MAX_ENTRIES; MG_GT(i, 0u); --i) {
        auto& e = entries_[i - 1];
        if(e.active && e.threadId == threadId) {
            e.active = FALSE;
            return TRUE;
        }
    }
    return FALSE;
}

ManualSEHEntry* ManualSEH::findCurrentEntry(HANDLE threadId) const {
    if (!entries_) return nullptr;

    for (u32 i = MG_MANUAL_SEH_MAX_ENTRIES; MG_GT(i, 0u); --i) {
        auto& e = entries_[i - 1];
        if(e.active && e.threadId == threadId)
            return &e;
    }
    return nullptr;
}

// ── Record push / query ───────────────────────────────────────────────────────

BOOLEAN ManualSEH::pushRecord(PCONTEXT ctx, PEXCEPTION_RECORD exc, HANDLE threadId) {
    if (!records_) return FALSE;

    while (_InterlockedExchange8(reinterpret_cast<CHAR*>(&recordLock_), TRUE) == TRUE)
        _mm_pause();

    BOOLEAN result = FALSE;
    for (u32 i = 0; i < MG_MANUAL_SEH_MAX_ENTRIES; ++i) {
        auto& r = records_[i];
        if(r.threadId == threadId || r.threadId == static_cast<HANDLE>(nullptr)) {
            RtlCopyMemory(&r.contextRecord, ctx, sizeof(CONTEXT));
            RtlCopyMemory(&r.exceptionRecord, exc, sizeof(EXCEPTION_RECORD));
            r.threadId = threadId;
            result = TRUE;
            break;
        }
    }

    recordLock_ = FALSE;
    return result;
}

ManualSEHRecord* ManualSEH::findCurrentRecord(HANDLE threadId) const {
    if (!records_) return nullptr;

    for (u32 i = 0; i < MG_MANUAL_SEH_MAX_ENTRIES; ++i) {
        auto& r = records_[i];
        if(r.threadId == threadId)
            return &r;
    }
    return nullptr;
}

PCONTEXT ManualSEH::getContextRecord() const {
    auto* rec = findCurrentRecord(reinterpret_cast<HANDLE>(
        static_cast<uptr>(GetCurrentThreadId())));
    return rec ? &rec->contextRecord : nullptr;
}

PEXCEPTION_RECORD ManualSEH::getExceptionRecord() const {
    auto* rec = findCurrentRecord(reinterpret_cast<HANDLE>(
        static_cast<uptr>(GetCurrentThreadId())));
    return rec ? &rec->exceptionRecord : nullptr;
}

u32 ManualSEH::getExceptionCode() const {
    auto* rec = findCurrentRecord(reinterpret_cast<HANDLE>(
        static_cast<uptr>(GetCurrentThreadId())));
    return rec ? rec->exceptionRecord.ExceptionCode : 0;
}

// ── Core exception handler ────────────────────────────────────────────────────

BOOLEAN ManualSEH::handleException(PCONTEXT contextRecord, PEXCEPTION_RECORD exceptionRecord) {
    if (!entries_) return FALSE;

    auto threadId = reinterpret_cast<HANDLE>(
        static_cast<uptr>(GetCurrentThreadId()));
    auto* entry = findCurrentEntry(threadId);
    if (!entry) return FALSE;

    pushRecord(contextRecord, exceptionRecord, threadId);

    RtlCopyMemory(contextRecord, &entry->savedContext, sizeof(CONTEXT));
    popEntry(threadId);

#ifdef _WIN64
    contextRecord->Rax = FALSE;
#else
    contextRecord->Eax = FALSE;
#endif

    return TRUE;
}

} // namespace mg

// ── C linkage helpers (called from ASM trampoline) ────────────────────────────

DECLSPEC_NOINLINE
HANDLE ManualSehCurrentThread(VOID) {
    return reinterpret_cast<HANDLE>(
        static_cast<mg::uptr>(GetCurrentThreadId()));
}

DECLSPEC_NOINLINE
BOOLEAN ManualSehPushEntry(PCONTEXT ContextRecord, HANDLE ThreadId) {
    auto* seh = mg::ManualSEH::instance();
    if (!seh) return FALSE;
    return seh->pushEntry(ContextRecord, ThreadId);
}

// ── x86 inline ASM for __MSEH_ENTER_TRY ───────────────────────────────────────
#ifndef _WIN64
__declspec(naked)
DECLSPEC_NOINLINE
UINT_PTR __MSEH_ENTER_TRY(VOID)
{
    __asm
    {
        push    ebp
        mov     ebp, esp
        sub     esp, 0x2CC

        // CaptureContext 32-bit
        mov[esp + 0B0h], eax
        mov[esp + 0ACh], ecx
        mov[esp + 0A8h], edx
        mov[esp + 0A4h], ebx
        mov[esp + 0A0h], esi
        mov[esp + 09Ch], edi
        mov     word ptr[esp + 0BCh], cs
        mov     word ptr[esp + 098h], ds
        mov     word ptr[esp + 094h], es
        mov     word ptr[esp + 090h], fs
        mov     word ptr[esp + 08Ch], gs
        mov     word ptr[esp + 0C8h], ss
        pushf
        pop[esp + 0C0h]
        mov     eax, [ebp + 4]
        mov[esp + 0B8h], eax
        mov     eax, [ebp]
        mov[esp + 0B4h], eax
        lea     eax, [ebp + 8]
        mov[esp + 0C4h], eax
        mov     dword ptr[esp], 10007h

        call    ManualSehCurrentThread
        push    eax
        lea     eax, [esp + 4]
        push    eax
        xor     eax, eax
        call    ManualSehPushEntry
        add     esp, 8
        mov     esp, ebp
        pop     ebp
        ret
    }
}
#endif
