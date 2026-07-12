// =============================================================================
// Scanner Common — Shared types, NT API, helpers for all scanner modules
// =============================================================================
#pragma once

#include "anticheat/detection_engine.hpp"
#include "utils/cloakwork_isolation.hpp"
#include "utils/logger.hpp"
#include "game/addresses.hpp"

#include <TlHelp32.h>
#include <Softpub.h>
#include <mscat.h>
#include <wintrust.h>

namespace mg {

// =============================================================================
// CRITICAL_SECTION RAII wrapper (no CRT dependency unlike std::mutex)
// =============================================================================
struct CritSection {
    CRITICAL_SECTION cs;

    CritSection()  { InitializeCriticalSection(&cs); }
    ~CritSection() { DeleteCriticalSection(&cs); }

    CritSection(const CritSection&) = delete;
    CritSection& operator=(const CritSection&) = delete;

    void lock()   { EnterCriticalSection(&cs); }
    void unlock() { LeaveCriticalSection(&cs); }
};

struct CritLock {
    CritSection& cs_;
    explicit CritLock(CritSection& cs) : cs_(cs) { cs_.lock(); }
    ~CritLock() { cs_.unlock(); }
    CritLock(const CritLock&) = delete;
    CritLock& operator=(const CritLock&) = delete;
};

// =============================================================================
// NT API typedefs — resolved once, shared by all scanners
// =============================================================================
struct NtApi {
    using NtQuerySystemInformation_t  = NTSTATUS(NTAPI*)(ULONG, PVOID, ULONG, PULONG);
    using NtQueryInformationProcess_t = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    using NtQueryInformationThread_t  = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    using NtDuplicateObject_t         = NTSTATUS(NTAPI*)(HANDLE, HANDLE, HANDLE, PHANDLE, ACCESS_MASK, ULONG, ULONG);
    using NtClose_t                   = NTSTATUS(NTAPI*)(HANDLE);
    using NtCreateTransaction_t       = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, LPGUID, HANDLE, ULONG, ULONG, ULONG, PLARGE_INTEGER, PUNICODE_STRING);
    using NtCreateSection_t           = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PLARGE_INTEGER, ULONG, ULONG, HANDLE);
    using NtMapViewOfSection_t        = NTSTATUS(NTAPI*)(HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T, PLARGE_INTEGER, PSIZE_T, ULONG, ULONG, ULONG);
    using NtUnmapViewOfSection_t      = NTSTATUS(NTAPI*)(HANDLE, PVOID);
    using CreateFileTransactedA_t     = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE, HANDLE, PUSHORT, PVOID);
    using WinVerifyTrust_t            = LONG(WINAPI*)(HWND, GUID*, LPVOID);
    using CryptCATAdminAcquireCtx_t   = BOOL(WINAPI*)(HCATADMIN*, const GUID*, DWORD);
    using CryptCATAdminCalcHash_t     = BOOL(WINAPI*)(HANDLE, DWORD*, BYTE*, DWORD);
    using CryptCATAdminEnumCat_t      = HCATINFO(WINAPI*)(HCATADMIN, BYTE*, DWORD, DWORD, HCATINFO*);
    using CryptCATCatalogInfo_t       = BOOL(WINAPI*)(HCATINFO, CATALOG_INFO*, DWORD);
    using CryptCATAdminRelCat_t       = BOOL(WINAPI*)(HCATADMIN, HCATINFO, DWORD);
    using CryptCATAdminRelCtx_t       = BOOL(WINAPI*)(HCATADMIN, DWORD);

    NtQuerySystemInformation_t  NtQuerySystemInformation  = nullptr;
    NtQueryInformationProcess_t NtQueryInformationProcess = nullptr;
    NtQueryInformationThread_t  NtQueryInformationThread  = nullptr;
    NtDuplicateObject_t         NtDuplicateObject         = nullptr;
    NtClose_t                   NtClose                   = nullptr;
    NtCreateTransaction_t       NtCreateTransaction       = nullptr;
    NtCreateSection_t           NtCreateSection           = nullptr;
    NtMapViewOfSection_t        NtMapViewOfSection        = nullptr;
    NtUnmapViewOfSection_t      NtUnmapViewOfSection      = nullptr;
    CreateFileTransactedA_t     CreateFileTransactedA     = nullptr;
    WinVerifyTrust_t            WinVerifyTrust            = nullptr;
    CryptCATAdminAcquireCtx_t   CryptCATAdminAcquireContext       = nullptr;
    CryptCATAdminCalcHash_t     CryptCATAdminCalcHashFromFileHandle = nullptr;
    CryptCATAdminEnumCat_t      CryptCATAdminEnumCatalogFromHash  = nullptr;
    CryptCATCatalogInfo_t       CryptCATCatalogInfoFromContext    = nullptr;
    CryptCATAdminRelCat_t       CryptCATAdminReleaseCatalogContext = nullptr;
    CryptCATAdminRelCtx_t       CryptCATAdminReleaseContext       = nullptr;

    bool load();
};

// NtQuerySystemInformation undocumented structs
struct SystemHandleEntry {
    ULONG       ProcessId;
    BYTE        ObjectTypeNumber;
    BYTE        Flags;
    USHORT      Handle;
    PVOID       Object;
    ACCESS_MASK GrantedAccess;
};

struct SystemHandleInformation {
    ULONG             HandleCount;
    SystemHandleEntry Handles[1];
};

// =============================================================================
// Shared data structures
// =============================================================================
struct ModuleInfo {
    uptr         base;
    u32          size;
    std::wstring name;
    std::wstring path;
};

struct HookBaseline {
    uptr address;
    u32  size;
    u32  hash;
};

struct TrackedThread {
    HANDLE      handle = nullptr;
    const char* name   = "";
    std::atomic<u32> heartbeat{0};
    u32         lastSeen = 0;

    TrackedThread() = default;
    TrackedThread(HANDLE h, const char* n) : handle(h), name(n) {}
    TrackedThread(TrackedThread&& o) noexcept
        : handle(o.handle), name(o.name)
        , heartbeat(o.heartbeat.load(std::memory_order_relaxed))
        , lastSeen(o.lastSeen) { o.handle = nullptr; }
    TrackedThread& operator=(TrackedThread&& o) noexcept {
        handle = o.handle; name = o.name;
        heartbeat.store(o.heartbeat.load(std::memory_order_relaxed), std::memory_order_relaxed);
        lastSeen = o.lastSeen; o.handle = nullptr; return *this;
    }
    TrackedThread(const TrackedThread&) = delete;
    TrackedThread& operator=(const TrackedThread&) = delete;
};

// =============================================================================
// Helper functions (defined in scanner_common.cpp — no statics)
// =============================================================================
std::vector<ModuleInfo> getLoadedModules();
bool isAddressInModules(const std::vector<ModuleInfo>& modules, uptr addr);
bool isInsideAcModule(uptr addr);
bool isPEHeader(const u8* buf);
u32  fnv1a(const u8* data, u32 len);
std::pair<uptr, u32> findSection(uptr moduleBase, const char* sectionName);
std::wstring toWide(const std::string& s);
std::string  toNarrow(const std::wstring& ws);
bool isModuleSigned(NtApi& api, const std::wstring& path);
void reportFlag(std::atomic<u32>& flags, Logger& log, DetectionFlag flag, const char* scanner, const std::string& detail = "", u32 extra = 0);

} // namespace mg
