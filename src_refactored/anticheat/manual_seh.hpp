// =============================================================================
// ManualSEH - Structured Exception Handling for manually-mapped DLLs
// =============================================================================
// Provides __TRY / __EXCEPT macros that work without OS-registered SEH data.
// Uses a VEH-free approach: the exception handler is invoked from the
// KiUserExceptionDispatcher hook (see GuardRegions).
//
// Usage:
//   MG_TRY {
//       // dangerous code
//   } MG_EXCEPT {
//       // handler
//   }
// =============================================================================
#pragma once

#include "core/types.hpp"
#include <Windows.h>

namespace mg {

// ── Forward ────────────────────────────────────────────────────────────────────
class ManualSEH;

// ── Per-entry context snapshot ─────────────────────────────────────────────────
DECLSPEC_ALIGN(16) struct ManualSEHEntry {
    CONTEXT  savedContext;
    BOOLEAN  active;
    HANDLE   threadId;
};

// ── Per-thread exception record (for GetCode / GetExceptionRecord) ─────────
DECLSPEC_ALIGN(16) struct ManualSEHRecord {
    CONTEXT          contextRecord;
    EXCEPTION_RECORD exceptionRecord;
    HANDLE           threadId;
};

// ── ManualSEH engine ──────────────────────────────────────────────────────────
class ManualSEH {
public:
    // #define avoids template-deduction issues with CloakWork obfuscated
    // comparisons (MG_LT/MG_GT) and is safe for manual-mapped DLLs.
#define MG_MANUAL_SEH_MAX_ENTRIES  static_cast<u32>(64)

    ManualSEH() = default;
    ~ManualSEH();

    ManualSEH(const ManualSEH&) = delete;
    ManualSEH& operator=(const ManualSEH&) = delete;

    /// Allocate internal buffers. Call once at startup.
    [[nodiscard]] VoidResult initialize();

    /// Free internal buffers.
    void shutdown();

    /// Core exception handler — called from KiUserExceptionDispatcher hook.
    /// Returns TRUE if the exception was inside an MG_TRY region and was unwound.
    BOOLEAN handleException(PCONTEXT contextRecord, PEXCEPTION_RECORD exceptionRecord);

    /// Retrieve the last exception record for the calling thread.
    PEXCEPTION_RECORD getExceptionRecord() const;

    /// Retrieve the last context record for the calling thread.
    PCONTEXT getContextRecord() const;

    /// Retrieve the last exception code for the calling thread.
    u32 getExceptionCode() const;

    /// Push a context snapshot for the calling thread. Called from MG_TRY.
    BOOLEAN pushEntry(PCONTEXT contextRecord, HANDLE threadId);

    /// Pop the latest entry for the calling thread. Called on MG_TRY exit.
    BOOLEAN popEntry(HANDLE threadId);

    /// Singleton access (set during initialize, used by asm trampoline).
    static ManualSEH* instance();

private:
    ManualSEHEntry*  findCurrentEntry(HANDLE threadId) const;
    ManualSEHRecord* findCurrentRecord(HANDLE threadId) const;
    BOOLEAN          pushRecord(PCONTEXT ctx, PEXCEPTION_RECORD exc, HANDLE threadId);

    ManualSEHEntry*  entries_  = nullptr;
    ManualSEHRecord* records_  = nullptr;
    BOOLEAN          pushLock_ = FALSE;
    BOOLEAN          recordLock_ = FALSE;

    static ManualSEH* s_instance;
};

} // namespace mg

// ── Macros ─────────────────────────────────────────────────────────────────────
// These mirror the original __TRY / __EXCEPT but use the mg::ManualSEH engine.

EXTERN_C {
    DECLSPEC_NOINLINE HANDLE  ManualSehCurrentThread(VOID);
    DECLSPEC_NOINLINE BOOLEAN ManualSehPushEntry(PCONTEXT ContextRecord, HANDLE ThreadId);
    UINT_PTR                  __MSEH_ENTER_TRY(VOID);
}

#define __MSEH_EXIT_TRY() \
    do { if (auto* _seh = mg::ManualSEH::instance()) _seh->popEntry(ManualSehCurrentThread()); } while(0)

#define MG_TRY    if (__MSEH_ENTER_TRY()) {
#define MG_EXCEPT __MSEH_EXIT_TRY(); } else
