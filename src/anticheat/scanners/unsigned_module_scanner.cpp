#include "pch.hpp"
#include "anticheat/scanners/unsigned_module_scanner.hpp"
#include "../../platform/memory.hpp"
namespace mg {

namespace {

struct TrustedUnsignedModule {
    const wchar_t* relativePath;
    const char*    expectedHashHex = nullptr;
    u8             baselineHash[32]{};
    bool           baselineReady = false;
    bool           hashLogged = false;
};

TrustedUnsignedModule kTrustedUnsignedModules[] = {
    {MG_WSTR(L"Release\\mss32.dll"), MG_STR("5765553068F423FC0AD1BD1E556F347A75754E94EA0B5526ED6B6D9BFB2C8B67")},
    {MG_WSTR(L"mss32.dll"), MG_STR("5765553068F423FC0AD1BD1E556F347A75754E94EA0B5526ED6B6D9BFB2C8B67")},
    {MG_WSTR(L"Release\\bdvid32.dll"), MG_STR("9091476FDC332450247D262CC0D339C7281BAB220AB709FD3A157C37E9FD5C43")},
    {MG_WSTR(L"bdvid32.dll"), MG_STR("9091476FDC332450247D262CC0D339C7281BAB220AB709FD3A157C37E9FD5C43")},
    {MG_WSTR(L"Data\\miles\\mssmp3.asi"), MG_STR("DF2432DAE4246BE0A8702E32ACB7A53598ED73945F2CA0BE9B143224C03182F0")},
    {MG_WSTR(L"Data\\miles\\mssvoice.asi"), MG_STR("D74A5E59357FCF75F2A9B573E57B1EF2397E64934E4EA642AFE8258284FA7349")},
    {MG_WSTR(L"Data\\miles\\mssdolby.flt"), MG_STR("11771130CF5084D49B174C881A880C65CE9C758A3C3488BF46E4FB984DCF425E")},
    {MG_WSTR(L"Data\\miles\\mssds3d.flt"), MG_STR("E5F3931209859D5B49B30B1D13F4645B01D2892AB9D40C8CA01BA5D9985174CC")},
    {MG_WSTR(L"Data\\miles\\mssdsp.flt"), MG_STR("DBF0F941CCD5C568790FE0C3D3B2299ED35AD6E122CF34F66F9CC06FDC4C3C1E")},
    {MG_WSTR(L"Data\\miles\\msseax.flt"), MG_STR("872FA247446ECBD1310A345A459F3C6003CD8C6151244247F566B4612DE87C71")},
    {MG_WSTR(L"Data\\miles\\msssrs.flt"), MG_STR("134D754108101722573EAA99DAAB092E288FDBAF54764544D41505184D085BFF")},
};

CritSection gLoggedChecksumPathsLock;
std::vector<std::wstring> gLoggedChecksumPaths;

void normalizeSlashes(std::wstring& value) {
    for (wchar_t& ch : value) {
        if (ch == L'/')
            ch = L'\\';
    }
}

bool pathStartsWith(const std::wstring& value, const std::wstring& prefix) {
    return value.size() >= prefix.size() &&
           _wcsnicmp(value.c_str(), prefix.c_str(), prefix.size()) == 0;
}

bool pathEndsWith(const std::wstring& value, const wchar_t* suffix) {
    const std::size_t suffixLen = wcslen(suffix);
    return value.size() >= suffixLen &&
           _wcsicmp(value.c_str() + (value.size() - suffixLen), suffix) == 0;
}

std::wstring getRelativeGamePath(const std::wstring& gameRoot, const std::wstring& modulePath) {
    std::wstring normalizedPath = modulePath;
    normalizeSlashes(normalizedPath);

    if (pathStartsWith(normalizedPath, gameRoot) && normalizedPath.size() > gameRoot.size() + 1) {
        std::wstring relative = normalizedPath.substr(gameRoot.size() + 1);
        normalizeSlashes(relative);
        return relative;
    }

    return normalizedPath;
}

bool hashFileBlake2b(const std::wstring& path, u8 out[32]);

u8 hexNibble(char ch) {
    if (ch >= '0' && ch <= '9')
        return static_cast<u8>(ch - '0');
    if (ch >= 'A' && ch <= 'F')
        return static_cast<u8>(10 + ch - 'A');
    if (ch >= 'a' && ch <= 'f')
        return static_cast<u8>(10 + ch - 'a');
    return MG_INT(0xFFu);
}

bool parseHashHex(const char* hex, u8 out[32]) {
    if (!hex || strlen(hex) != 64)
        return false;

    for (u32 i = 0; i < 32; ++i) {
        const u8 hi = hexNibble(hex[i * 2]);
        const u8 lo = hexNibble(hex[i * 2 + 1]);
        if (hi == MG_INT(0xFFu) || lo == MG_INT(0xFFu))
            return false;
        out[i] = static_cast<u8>((hi << 4) | lo);
    }

    return true;
}

void ensureTrustedBaselineLoaded(TrustedUnsignedModule& trusted) {
    if (trusted.baselineReady || !trusted.expectedHashHex)
        return;

    u8 parsed[32]{};
    if (!parseHashHex(trusted.expectedHashHex, parsed))
        return;

    nocrtMemcpy(trusted.baselineHash, parsed, sizeof(parsed));
    trusted.baselineReady = true;
}

std::string hashToHex(const u8 hash[32]) {
    static constexpr char kHex[] = "0123456789ABCDEF";

    std::string out;
    out.resize(64);
    for (u32 i = 0; i < 32; ++i) {
        out[static_cast<std::size_t>(i) * 2]     = kHex[(hash[i] >> 4) & 0x0F];
        out[static_cast<std::size_t>(i) * 2 + 1] = kHex[hash[i] & 0x0F];
    }

    return out;
}

void logTrustedChecksum(Logger& log, TrustedUnsignedModule& trusted, const u8 hash[32]) {
    if (trusted.hashLogged)
        return;

    trusted.hashLogged = true;
    log.info("[UnsignedModule] Whitelist checksum candidate '{}' = {}",
             toNarrow(std::wstring(trusted.relativePath)),
             hashToHex(hash));
}

bool tryMarkChecksumLogged(const std::wstring& displayPath) {
    CritLock lock(gLoggedChecksumPathsLock);
    for (const auto& loggedPath : gLoggedChecksumPaths) {
        if (_wcsicmp(loggedPath.c_str(), displayPath.c_str()) == 0)
            return false;
    }

    gLoggedChecksumPaths.push_back(displayPath);
    return true;
}

void rollbackChecksumLogged(const std::wstring& displayPath) {
    CritLock lock(gLoggedChecksumPathsLock);
    for (auto it = gLoggedChecksumPaths.begin(); it != gLoggedChecksumPaths.end(); ++it) {
        if (_wcsicmp(it->c_str(), displayPath.c_str()) == 0) {
            gLoggedChecksumPaths.erase(it);
            return;
        }
    }
}

void logModuleChecksumCandidate(Logger& log,
                                const std::wstring& displayPath,
                                const std::wstring& modulePath) {
    std::wstring normalizedDisplayPath = displayPath.empty() ? modulePath : displayPath;
    normalizeSlashes(normalizedDisplayPath);

    if (!tryMarkChecksumLogged(normalizedDisplayPath))
        return;

    u8 hash[32]{};
    if (!hashFileBlake2b(modulePath, hash)) {
        rollbackChecksumLogged(normalizedDisplayPath);
        return;
    }

    //log.info("[UnsignedModule] Whitelist checksum candidate '{}' = {}",
    //         toNarrow(normalizedDisplayPath),
    //         hashToHex(hash));
}

bool hashFileBlake2b(const std::wstring& path, u8 out[32]) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        nocrtMemset(out, 0, 32);
        return false;
    }

    crypto_blake2b_ctx ctx;
    crypto_blake2b_init(&ctx, 32);

    constexpr DWORD kBufSize = 4096;
    u8 buf[kBufSize];
    DWORD bytesRead = 0;
    while (ReadFile(hFile, buf, kBufSize, &bytesRead, nullptr) && bytesRead > 0)
        crypto_blake2b_update(&ctx, buf, bytesRead);

    CloseHandle(hFile);
    crypto_blake2b_final(&ctx, out);
    return true;
}

bool sameHash(const u8 lhs[32], const u8 rhs[32]) {
    for (u32 i = 0; i < 32; ++i) {
        if (lhs[i] != rhs[i])
            return false;
    }

    return true;
}

std::wstring getGameRootPath() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);

    for (int pass = 0; pass < 2; ++pass) {
        wchar_t* lastSlash = wcsrchr(path, L'\\');
        if (!lastSlash)
            break;
        *lastSlash = L'\0';
    }

    return std::wstring(path);
}

TrustedUnsignedModule* findTrustedUnsignedModule(const std::wstring& gameRoot,
                                                 const std::wstring& modulePath) {
    std::wstring normalizedPath = modulePath;
    normalizeSlashes(normalizedPath);
    const std::wstring relative = getRelativeGamePath(gameRoot, modulePath);

    for (auto& entry : kTrustedUnsignedModules) {
        if ((!relative.empty() && _wcsicmp(relative.c_str(), entry.relativePath) == 0) ||
            pathEndsWith(normalizedPath, entry.relativePath))
            return &entry;
    }

    return nullptr;
}

bool isTrustedUnsignedGameModule(const std::wstring& gameRoot,
                                 const std::wstring& modulePath,
                                 Logger& log) {
    TrustedUnsignedModule* trusted = findTrustedUnsignedModule(gameRoot, modulePath);
    if (!trusted)
        return false;

    ensureTrustedBaselineLoaded(*trusted);

    u8 currentHash[32]{};
    if (!hashFileBlake2b(modulePath, currentHash))
        return false;

    if (!trusted->baselineReady) {
        nocrtMemcpy(trusted->baselineHash, currentHash, sizeof(currentHash));
        trusted->baselineReady = true;
        //logTrustedChecksum(log, *trusted, currentHash);
        return true;
    }

    //logTrustedChecksum(log, *trusted, trusted->baselineHash);
    return sameHash(trusted->baselineHash, currentHash);
}

} // anonymous namespace

void UnsignedModuleScanner::scan(std::atomic<u32>& flags, Logger& log) {
    if (!api_.WinVerifyTrust) return;

    wchar_t sysDir[MAX_PATH]{}, winDir[MAX_PATH]{};
    GetSystemDirectoryW(sysDir, MAX_PATH);
    GetWindowsDirectoryW(winDir, MAX_PATH);
    const std::wstring gameRoot = getGameRootPath();

    for (auto& m : getLoadedModules()) {
        if (m.path.empty()) continue;
        if (_wcsnicmp(m.path.c_str(), sysDir, wcslen(sysDir)) == 0) continue;
        if (_wcsnicmp(m.path.c_str(), winDir, wcslen(winDir)) == 0) continue;

        const std::wstring relativePath = getRelativeGamePath(gameRoot, m.path);

        if (isTrustedUnsignedGameModule(gameRoot, m.path, log))
            continue;

        if (!isModuleSigned(api_, m.path)) {
            //logModuleChecksumCandidate(log, relativePath, m.path);
            reportFlag(flags, log, DetectionFlag::kUnsignedModule, MG_STR("SignatureCheck"),
                fmt::format(MG_STR_CONST("Unsigned: {} @ 0x{:X}"), toNarrow(m.name), m.base),
                static_cast<u32>(m.base));
        }
    }
}

} // namespace mg
