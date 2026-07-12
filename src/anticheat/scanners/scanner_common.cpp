#include "pch.hpp"
#include "anticheat/scanners/scanner_common.hpp"
#include "anticheat/detection_report.hpp"
#include "game/network/heartbeat_handler.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"

#pragma comment(lib, "crypt32.lib")

namespace mg {

namespace {

std::wstring bytesToUpperHex(const BYTE* data, DWORD size) {
    static constexpr wchar_t kHex[] = L"0123456789ABCDEF";

    std::wstring out;
    out.resize(static_cast<std::size_t>(size) * 2);

    for (DWORD i = 0; i < size; ++i) {
        out[static_cast<std::size_t>(i) * 2]     = kHex[(data[i] >> 4) & 0x0F];
        out[static_cast<std::size_t>(i) * 2 + 1] = kHex[data[i] & 0x0F];
    }

    return out;
}

bool hasSysExtension(const std::wstring& path) {
    if (path.size() < 4)
        return false;

    const wchar_t* ext = path.c_str() + (path.size() - 4);
    return _wcsicmp(ext, L".sys") == 0;
}

bool verifyTrustFilePolicy(NtApi& api, const std::wstring& path, const GUID& policyGuid) {
    WINTRUST_FILE_INFO fi{};
    fi.cbStruct = sizeof(fi);
    fi.pcwszFilePath = path.c_str();

    WINTRUST_DATA wd{};
    wd.cbStruct = sizeof(wd);
    wd.dwUIChoice = WTD_UI_NONE;
    wd.fdwRevocationChecks = WTD_REVOKE_NONE;
    wd.dwUnionChoice = WTD_CHOICE_FILE;
    wd.pFile = &fi;
    wd.dwStateAction = WTD_STATEACTION_VERIFY;
    wd.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;

    GUID guid = policyGuid;
    const LONG st = api.WinVerifyTrust(NULL, &guid, &wd);
    wd.dwStateAction = WTD_STATEACTION_CLOSE;
    api.WinVerifyTrust(NULL, &guid, &wd);
    return st == ERROR_SUCCESS;
}

bool verifyTrustCatalogPolicy(NtApi& api,
                              const std::wstring& path,
                              HANDLE hFile,
                              const BYTE* hash,
                              DWORD cbH,
                              const wchar_t* catalogPath,
                              const wchar_t* memberTag,
                              const GUID& policyGuid) {
    WINTRUST_CATALOG_INFO wci{};
    wci.cbStruct = sizeof(wci);
    wci.pcwszCatalogFilePath = catalogPath;
    wci.pcwszMemberTag = memberTag;
    wci.pcwszMemberFilePath = path.c_str();
    wci.hMemberFile = hFile;
    wci.pbCalculatedFileHash = const_cast<BYTE*>(hash);
    wci.cbCalculatedFileHash = cbH;

    WINTRUST_DATA wd{};
    wd.cbStruct = sizeof(wd);
    wd.dwUIChoice = WTD_UI_NONE;
    wd.fdwRevocationChecks = WTD_REVOKE_NONE;
    wd.dwUnionChoice = WTD_CHOICE_CATALOG;
    wd.dwStateAction = WTD_STATEACTION_VERIFY;
    wd.pCatalog = &wci;
    wd.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;

    GUID guid = policyGuid;
    const LONG st = api.WinVerifyTrust(NULL, &guid, &wd);
    wd.dwStateAction = WTD_STATEACTION_CLOSE;
    api.WinVerifyTrust(NULL, &guid, &wd);
    return st == ERROR_SUCCESS;
}

std::size_t cstrLength(const char* value) {
    std::size_t len = 0;
    while (value && value[len] != '\0')
        ++len;
    return len;
}

std::string trimCopy(const std::string& value) {
    std::size_t first = 0;
    while (first < value.size() && static_cast<unsigned char>(value[first]) <= ' ')
        ++first;

    std::size_t last = value.size();
    while (last > first && static_cast<unsigned char>(value[last - 1]) <= ' ')
        --last;

    return value.substr(first, last - first);
}

std::string leafName(const std::string& value) {
    const std::size_t pos = value.find_last_of("\\/");
    if (pos == std::string::npos)
        return trimCopy(value);

    return trimCopy(value.substr(pos + 1));
}

std::string extractBetween(const std::string& detail, const char* prefix, const char* suffix) {
    const std::size_t prefixLen = cstrLength(prefix);
    if (detail.compare(0, prefixLen, prefix) != 0)
        return {};

    const std::size_t start = prefixLen;
    const std::size_t end = detail.find(suffix, start);
    if (end == std::string::npos || end <= start)
        return {};

    return trimCopy(detail.substr(start, end - start));
}

std::string extractQuotedValue(const std::string& detail, const char* prefix) {
    const std::size_t prefixLen = cstrLength(prefix);
    if (detail.compare(0, prefixLen, prefix) != 0)
        return {};

    const std::size_t start = prefixLen;
    const std::size_t end = detail.find('\'', start);
    if (end == std::string::npos || end <= start)
        return {};

    return trimCopy(detail.substr(start, end - start));
}

std::string extractParenthesizedValue(const std::string& detail) {
    const std::size_t start = detail.find('(');
    if (start == std::string::npos)
        return {};

    const std::size_t end = detail.find(')', start + 1);
    if (end == std::string::npos || end <= start + 1)
        return {};

    return trimCopy(detail.substr(start + 1, end - start - 1));
}

std::string extractDetectionName(DetectionFlag flag, const std::string& detail) {
    if (detail.empty())
        return {};

    switch (flag) {
    case DetectionFlag::kBlacklistedModule:
    case DetectionFlag::kDllInjection:
        return extractBetween(detail, "", " @ 0x");

    case DetectionFlag::kUnsignedModule:
        return extractBetween(detail, "Unsigned: ", " @ 0x");

    case DetectionFlag::kBlacklistedString:
        return extractQuotedValue(detail, "Found '");

    case DetectionFlag::kBlacklistedSignature:
        return extractQuotedValue(detail, "Pattern '");

    case DetectionFlag::kVulnerableDriver:
        return extractBetween(detail, "Blacklisted driver: ", " @ 0x");

    case DetectionFlag::kUnsignedDriver:
        return extractBetween(detail, "Unsigned driver: ", " @ 0x");

    case DetectionFlag::kProxyDll:
        return extractBetween(detail, "", " loaded from game dir");

    case DetectionFlag::kDangerousHandle:
        return leafName(extractParenthesizedValue(detail));

    default:
        return {};
    }
}

} // anonymous namespace

// =============================================================================
// NtApi::load
// =============================================================================
bool NtApi::load() {
    HMODULE ntdll = GetModuleHandleA(MG_STR("ntdll.dll"));
    if (!ntdll) return false;

    NtQuerySystemInformation  = reinterpret_cast<NtQuerySystemInformation_t>(GetProcAddress(ntdll, MG_STR("NtQuerySystemInformation")));
    NtQueryInformationProcess = reinterpret_cast<NtQueryInformationProcess_t>(GetProcAddress(ntdll, MG_STR("NtQueryInformationProcess")));
    NtQueryInformationThread  = reinterpret_cast<NtQueryInformationThread_t>(GetProcAddress(ntdll, MG_STR("NtQueryInformationThread")));
    NtDuplicateObject         = reinterpret_cast<NtDuplicateObject_t>(GetProcAddress(ntdll, MG_STR("NtDuplicateObject")));
    NtClose                   = reinterpret_cast<NtClose_t>(GetProcAddress(ntdll, MG_STR("NtClose")));
    NtCreateTransaction       = reinterpret_cast<NtCreateTransaction_t>(GetProcAddress(ntdll, MG_STR("NtCreateTransaction")));
    NtCreateSection           = reinterpret_cast<NtCreateSection_t>(GetProcAddress(ntdll, MG_STR("NtCreateSection")));
    NtMapViewOfSection        = reinterpret_cast<NtMapViewOfSection_t>(GetProcAddress(ntdll, MG_STR("NtMapViewOfSection")));
    NtUnmapViewOfSection      = reinterpret_cast<NtUnmapViewOfSection_t>(GetProcAddress(ntdll, MG_STR("NtUnmapViewOfSection")));

    HMODULE k32 = GetModuleHandleA(MG_STR("kernel32.dll"));
    if (k32)
        CreateFileTransactedA = reinterpret_cast<CreateFileTransactedA_t>(GetProcAddress(k32, MG_STR("CreateFileTransactedA")));

    HMODULE wt = LoadLibraryA(MG_STR("wintrust.dll"));
    if (wt) {
        WinVerifyTrust                      = reinterpret_cast<WinVerifyTrust_t>(GetProcAddress(wt, MG_STR("WinVerifyTrust")));
        CryptCATAdminAcquireContext          = reinterpret_cast<CryptCATAdminAcquireCtx_t>(GetProcAddress(wt, MG_STR("CryptCATAdminAcquireContext")));
        CryptCATAdminCalcHashFromFileHandle  = reinterpret_cast<CryptCATAdminCalcHash_t>(GetProcAddress(wt, MG_STR("CryptCATAdminCalcHashFromFileHandle")));
        CryptCATAdminEnumCatalogFromHash     = reinterpret_cast<CryptCATAdminEnumCat_t>(GetProcAddress(wt, MG_STR("CryptCATAdminEnumCatalogFromHash")));
        CryptCATCatalogInfoFromContext       = reinterpret_cast<CryptCATCatalogInfo_t>(GetProcAddress(wt, MG_STR("CryptCATCatalogInfoFromContext")));
        CryptCATAdminReleaseCatalogContext   = reinterpret_cast<CryptCATAdminRelCat_t>(GetProcAddress(wt, MG_STR("CryptCATAdminReleaseCatalogContext")));
        CryptCATAdminReleaseContext          = reinterpret_cast<CryptCATAdminRelCtx_t>(GetProcAddress(wt, MG_STR("CryptCATAdminReleaseContext")));
    }

    return NtQuerySystemInformation && NtQueryInformationProcess && NtDuplicateObject && NtClose;
}

// =============================================================================
// Helpers
// =============================================================================
std::vector<ModuleInfo> getLoadedModules() {
    std::vector<ModuleInfo> result;
    HMODULE hMods[1024]{};
    DWORD cbNeeded = 0;
    HANDLE hSelf = GetCurrentProcess();

    if (!EnumProcessModulesEx(hSelf, hMods, sizeof(hMods), &cbNeeded, LIST_MODULES_ALL))
        return result;

    u32 count = cbNeeded / sizeof(HMODULE);
    result.reserve(count);

    for (u32 i = 0; i < count; ++i) {
        MODULEINFO mi{};
        if (!GetModuleInformation(hSelf, hMods[i], &mi, sizeof(mi))) continue;

        wchar_t modPath[MAX_PATH]{};
        GetModuleFileNameExW(hSelf, hMods[i], modPath, MAX_PATH);

        wchar_t baseName[MAX_PATH]{};
        GetModuleBaseNameW(hSelf, hMods[i], baseName, MAX_PATH);

        result.push_back({reinterpret_cast<uptr>(mi.lpBaseOfDll), mi.SizeOfImage, baseName, modPath});
    }
    return result;
}

bool isAddressInModules(const std::vector<ModuleInfo>& modules, uptr addr) {
    for (auto& m : modules)
        if (addr >= m.base && addr < m.base + m.size) return true;
    return false;
}

bool isInsideAcModule(uptr addr) {
    uptr acBase = game::addr::globals::g_AntiCheatModuleBase;
    u32  acSize = game::addr::globals::g_AntiCheatModuleSize;
    return acBase && addr >= acBase && addr < acBase + acSize;
}

bool isPEHeader(const u8* buf) {
    if (buf[0] != 'M' || buf[1] != 'Z') return false;
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(buf);
    if (dos->e_lfanew <= 0 || dos->e_lfanew >= MG_INT(0x400)) return false;
    return *reinterpret_cast<const DWORD*>(buf + dos->e_lfanew) == IMAGE_NT_SIGNATURE;
}

u32 fnv1a(const u8* data, u32 len) {
    u32 hash = MG_INT(0x811C9DC5u);
    for (u32 i = 0; i < len; ++i) { hash ^= data[i]; hash *= MG_INT(0x01000193u); }
    return hash;
}

std::pair<uptr, u32> findSection(uptr moduleBase, const char* sectionName) {
    if (!moduleBase) return {0, 0};
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(moduleBase);
    auto* nt  = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<u8*>(moduleBase) + dos->e_lfanew);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (strncmp(reinterpret_cast<const char*>(sec[i].Name), sectionName, 8) == 0)
            return {moduleBase + sec[i].VirtualAddress, sec[i].Misc.VirtualSize};
    }
    return {0, 0};
}

std::wstring toWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), static_cast<int>(s.size()), ws.data(), len);
    return ws;
}

std::string toNarrow(const std::wstring& ws) {
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_ACP, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_ACP, 0, ws.c_str(), static_cast<int>(ws.size()), s.data(), len, nullptr, nullptr);
    return s;
}

void reportFlag(std::atomic<u32>& flags, Logger& log, DetectionFlag flag, const char* scanner, const std::string& detail, u32 extra) {
    flags.fetch_or(static_cast<u32>(flag), std::memory_order_relaxed);
    //if (detail.empty())
    //log.info("[DETECT] {} triggered", scanner);
    //else
    //    log.info("[DETECT] {} triggered: {}", scanner, detail);

    const std::string detectionName = extractDetectionName(flag, detail);

    // Queue the event for next heartbeat response, preserving only compact
    // identifier-like strings that the server can store separately.
    mg::ctx().heartbeatHandler().detectionReport().report(
        flag,
        extra,
        detectionName.empty() ? nullptr : detectionName.c_str());
}

bool isModuleSigned(NtApi& api, const std::wstring& path) {
    if (!api.WinVerifyTrust) return true;

    const bool isDriverImage = hasSysExtension(path);
    if (verifyTrustFilePolicy(api, path, WINTRUST_ACTION_GENERIC_VERIFY_V2))
        return true;

    if (isDriverImage && verifyTrustFilePolicy(api, path, DRIVER_ACTION_VERIFY))
        return true;

    // Windows catalog verification
    if (!api.CryptCATAdminAcquireContext ||
        !api.CryptCATAdminCalcHashFromFileHandle ||
        !api.CryptCATAdminEnumCatalogFromHash ||
        !api.CryptCATCatalogInfoFromContext ||
        !api.CryptCATAdminReleaseCatalogContext ||
        !api.CryptCATAdminReleaseContext)
        return false;

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    HCATADMIN hCA = nullptr;
    if (!api.CryptCATAdminAcquireContext(&hCA, nullptr, 0)) {
        CloseHandle(hFile);
        return false;
    }

    BYTE hash[100]{};
    DWORD cbH = sizeof(hash);
    if (!api.CryptCATAdminCalcHashFromFileHandle(hFile, &cbH, hash, 0)) {
        api.CryptCATAdminReleaseContext(hCA, 0);
        CloseHandle(hFile);
        return false;
    }

    HCATINFO hCI = api.CryptCATAdminEnumCatalogFromHash(hCA, hash, cbH, 0, nullptr);
    bool ok = false;
    if (hCI) {
        CATALOG_INFO ci{};
        ci.cbStruct = sizeof(ci);
        if (api.CryptCATCatalogInfoFromContext(hCI, &ci, 0)) {
            const std::wstring memberTag = bytesToUpperHex(hash, cbH);

            ok = verifyTrustCatalogPolicy(
                api,
                path,
                hFile,
                hash,
                cbH,
                ci.wszCatalogFile,
                memberTag.c_str(),
                WINTRUST_ACTION_GENERIC_VERIFY_V2);

            if (!ok && isDriverImage) {
                ok = verifyTrustCatalogPolicy(
                    api,
                    path,
                    hFile,
                    hash,
                    cbH,
                    ci.wszCatalogFile,
                    memberTag.c_str(),
                    DRIVER_ACTION_VERIFY);
            }
        }
        api.CryptCATAdminReleaseCatalogContext(hCA, hCI, 0);
    }
    api.CryptCATAdminReleaseContext(hCA, 0);
    CloseHandle(hFile);
    return ok;
}

} // namespace mg
