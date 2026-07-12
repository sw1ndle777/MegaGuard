#include "pch.hpp"
#include "anticheat/scanners/proxy_dll_scanner.hpp"

namespace mg {

void ProxyDllScanner::scan(std::atomic<u32>& flags, Logger& log) {
    const wchar_t* kTargets[] = {
        L"version.dll", L"winmm.dll", L"d3d9.dll", L"d3d11.dll",
        L"dxgi.dll", L"dinput8.dll", L"dsound.dll", L"xinput1_3.dll",
        L"winhttp.dll", L"wsock32.dll",
    };

    wchar_t sysDir[MAX_PATH]{};
    GetSystemDirectoryW(sysDir, MAX_PATH);

    wchar_t gameDir[MAX_PATH]{};
    GetModuleFileNameW(nullptr, gameDir, MAX_PATH);
    if (auto* s = wcsrchr(gameDir, L'\\')) *(s + 1) = L'\0';

    for (auto* target : kTargets) {
        std::wstring gamePath = std::wstring(gameDir) + target;
        DWORD attr = GetFileAttributesW(gamePath.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) continue;

        HMODULE hLoaded = GetModuleHandleW(target);
        if (!hLoaded) continue;

        wchar_t loadedPath[MAX_PATH]{};
        GetModuleFileNameW(hLoaded, loadedPath, MAX_PATH);

        if (_wcsnicmp(loadedPath, sysDir, wcslen(sysDir)) != 0)
            reportFlag(flags, log, DetectionFlag::kProxyDll, MG_STR("ProxyDLL"),
                fmt::format(MG_STR_CONST("{} loaded from game dir"), toNarrow(target)),
                static_cast<u32>(reinterpret_cast<uptr>(hLoaded)));
    }
}

} // namespace mg
