#include "pch.hpp"
#include "anticheat/scanners/injection_scanner.hpp"
#include "../../platform/memory.hpp"
namespace mg {

namespace {

struct TrustedLateModule {
    const wchar_t* relativePath;
    const char*    expectedHashHex;
};

TrustedLateModule kTrustedLateModules[] = {
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

bool isTrustedLateLoadedModule(const std::wstring& gameRoot, const std::wstring& modulePath) {
    if (modulePath.empty())
        return false;

    std::wstring normalizedPath = modulePath;
    normalizeSlashes(normalizedPath);
    const std::wstring relativePath = getRelativeGamePath(gameRoot, modulePath);

    const TrustedLateModule* trusted = nullptr;
    for (const auto& entry : kTrustedLateModules) {
        if (_wcsicmp(relativePath.c_str(), entry.relativePath) == 0 ||
            pathEndsWith(normalizedPath, entry.relativePath)) {
            trusted = &entry;
            break;
        }
    }

    if (!trusted)
        return false;

    u8 expectedHash[32]{};
    u8 currentHash[32]{};
    if (!parseHashHex(trusted->expectedHashHex, expectedHash) ||
        !hashFileBlake2b(modulePath, currentHash)) {
        return false;
    }

    return sameHash(expectedHash, currentHash);
}

} // namespace

void InjectionScanner::init() {
    auto mods = getLoadedModules();
    knownBases_.reserve(mods.size());
    for (auto& m : mods) knownBases_.push_back(m.base);
    std::sort(knownBases_.begin(), knownBases_.end());
}

void InjectionScanner::scan(std::atomic<u32>& flags, Logger& log) {
    auto mods = getLoadedModules();
    const std::wstring gameRoot = getGameRootPath();
    for (auto& m : mods) {
        if (std::binary_search(knownBases_.begin(), knownBases_.end(), m.base)) continue;

        if (isTrustedLateLoadedModule(gameRoot, m.path)) {
            knownBases_.push_back(m.base);
            std::sort(knownBases_.begin(), knownBases_.end());
            continue;
        }

        auto nm = toNarrow(m.name);
        bool bl = false;
        {
            CritLock lk(*configCs_);
            for (auto& b : *blacklist_)
                if (_stricmp(nm.c_str(), b.c_str()) == 0) { bl = true; break; }
        }

        if (bl) {
            reportFlag(flags, log, DetectionFlag::kBlacklistedModule, MG_STR("ModuleScan"),
                fmt::format(MG_STR_CONST("{} @ 0x{:X}"), nm, m.base), static_cast<u32>(m.base));
        } else if (!m.path.empty() && !isModuleSigned(api_, m.path)) {
            reportFlag(flags, log, DetectionFlag::kDllInjection, MG_STR("InjectionDetect"),
                fmt::format(MG_STR_CONST("{} @ 0x{:X} (unsigned)"), nm, m.base), static_cast<u32>(m.base));
        }

        knownBases_.push_back(m.base);
        std::sort(knownBases_.begin(), knownBases_.end());
    }
}

} // namespace mg
