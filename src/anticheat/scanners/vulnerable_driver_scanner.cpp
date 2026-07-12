#include "pch.hpp"
#include "anticheat/scanners/vulnerable_driver_scanner.hpp"

namespace mg {

namespace {

enum class RegistryFeatureState : u8 {
    kUnavailable = 0,
    kDisabled,
    kEnabled,
};

const char* const kBlacklistedDrivers[] = {
    MG_STR("ntguard.sys"),
    MG_STR("Gdrv.sys"),
    MG_STR("AsIO.sys"),
    MG_STR("AsUpIO.sys"),
    MG_STR("CPUID.sys"),
    MG_STR("ENE.sys"),
    MG_STR("iqvw64e.sys"),
    MG_STR("hxctl.sys"),
    MG_STR("kprocesshacker.sys"),
    MG_STR("kprocesshacker2.sys"),
    MG_STR("EIO64.sys"),
    MG_STR("IOMap64.sys"),
    MG_STR("ATSZIO64.sys"),
    MG_STR("atillk64.sys"),
    MG_STR("BS_Flash64.sys"),
    MG_STR("Capcom.sys"),
    MG_STR("cpuz141.sys"),
    MG_STR("WinRing0x64.sys"),
    MG_STR("FairplayKD.sys"),
    MG_STR("pgldqpoc.sys"),
    MG_STR("HwOs2Ec10x64.sys"),
    MG_STR("Phymemx64.sys"),
    MG_STR("Monitor_win10_x64.sys"),
    MG_STR("driver.sys"),
    MG_STR("lha.sys"),
    MG_STR("Mslo64.sys"),
    MG_STR("NTIOLib_x64.sys"),
    MG_STR("pcdsrvc_x64.pkms"),
    MG_STR("HWiNFO64A.sys"),
    MG_STR("rzpnk.sys"),
    MG_STR("magdrvamd64.sys"),
    MG_STR("speedfan.sys"),
    MG_STR("zam64.sys"),
    MG_STR("DBK64.sys"),
};

const char* const kWhitelistedUnsignedDrivers[] = {
    MG_STR("dump_diskdump.sys"),
    MG_STR("dump_storahci.sys"),
    MG_STR("dump_dumpfve.sys"),
};

RegistryFeatureState queryRegistryFeatureState(const char* subKey, const char* valueName) {
    HKEY hKey = nullptr;
    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return RegistryFeatureState::kUnavailable;

    const LONG result = RegQueryValueExA(hKey, valueName, nullptr, &type,
        reinterpret_cast<LPBYTE>(&value), &size);

    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS || type != REG_DWORD)
        return RegistryFeatureState::kUnavailable;

    return value != 0 ? RegistryFeatureState::kEnabled : RegistryFeatureState::kDisabled;
}

template <std::size_t N>
bool isDriverListed(const char* driverName, const char* const (&list)[N]) {
    if (!driverName || !*driverName) return false;

    for (const char* entry : list) {
        if (_stricmp(driverName, entry) == 0)
            return true;
    }

    return false;
}

bool isImplicitlyWhitelistedUnsignedDriver(const char* driverName) {
    if (!driverName || !*driverName)
        return false;

    if (isDriverListed(driverName, kWhitelistedUnsignedDrivers))
        return true;

    return _strnicmp(driverName, "dump_", 5) == 0;
}

bool isCurrentProcessWow64() {
    BOOL wow64 = FALSE;
    return IsWow64Process(GetCurrentProcess(), &wow64) != FALSE && wow64 == TRUE;
}

std::wstring normalizeDriverPath(const std::wstring& nativePath) {
    if (nativePath.empty()) return {};

    std::wstring normalized = nativePath;

    if (normalized.rfind(L"\\??\\", 0) == 0)
        normalized = normalized.substr(4);

    if (normalized.rfind(L"\\SystemRoot\\", 0) == 0) {
        wchar_t windowsDir[MAX_PATH]{};
        if (GetWindowsDirectoryW(windowsDir, MAX_PATH) != 0)
            normalized = std::wstring(windowsDir) + L"\\" + normalized.substr(12);
    }

    if (isCurrentProcessWow64()) {
        wchar_t windowsDir[MAX_PATH]{};
        if (GetWindowsDirectoryW(windowsDir, MAX_PATH) != 0) {
            const std::wstring system32Prefix = std::wstring(windowsDir) + L"\\System32\\";
            if (_wcsnicmp(normalized.c_str(), system32Prefix.c_str(), system32Prefix.size()) == 0)
                normalized = std::wstring(windowsDir) + L"\\Sysnative\\" + normalized.substr(system32Prefix.size());
        }
    }

    return normalized;
}

} // namespace

void VulnerableDriverScanner::scan(std::atomic<u32>& flags, Logger& log) {
    if (queryRegistryFeatureState(MG_STR("SYSTEM\\CurrentControlSet\\Control\\CI\\Config"),
            MG_STR("VulnerableDriverBlocklistEnable")) == RegistryFeatureState::kDisabled) {
        reportFlag(flags, log, DetectionFlag::kDriverBlocklistDisabled, MG_STR("DriverScan"),
            MG_STR("Microsoft vulnerable driver blocklist disabled"));
    }

    if (queryRegistryFeatureState(MG_STR("SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity"),
            MG_STR("Enabled")) == RegistryFeatureState::kDisabled) {
        reportFlag(flags, log, DetectionFlag::kHvciDisabled, MG_STR("DriverScan"),
            MG_STR("HVCI disabled"));
    }

    LPVOID drivers[1024]{};
    DWORD cbNeeded = 0;
    if (!EnumDeviceDrivers(drivers, sizeof(drivers), &cbNeeded)) return;

    u32 cnt = cbNeeded / sizeof(LPVOID);
    for (u32 i = 0; i < cnt; ++i) {
        char drvName[MAX_PATH]{};
        if (!GetDeviceDriverBaseNameA(drivers[i], drvName, sizeof(drvName)))
            continue;

        const uptr driverBase = reinterpret_cast<uptr>(drivers[i]);
        if (isDriverListed(drvName, kBlacklistedDrivers)) {
            reportFlag(flags, log, DetectionFlag::kVulnerableDriver, MG_STR("DriverScan"),
                fmt::format(MG_STR_CONST("Blacklisted driver: {} @ 0x{:X}"), drvName, driverBase),
                static_cast<u32>(driverBase));
        }

        if (!api_.WinVerifyTrust || isImplicitlyWhitelistedUnsignedDriver(drvName))
            continue;

        wchar_t driverPath[MAX_PATH]{};
        if (!GetDeviceDriverFileNameW(drivers[i], driverPath, MAX_PATH))
            continue;

        const std::wstring normalizedPath = normalizeDriverPath(driverPath);
        if (normalizedPath.empty())
            continue;

        if (!isModuleSigned(api_, normalizedPath)) {
            reportFlag(flags, log, DetectionFlag::kUnsignedDriver, MG_STR("DriverScan"),
                fmt::format(MG_STR_CONST("Unsigned driver: {} @ 0x{:X}"), drvName, driverBase),
                static_cast<u32>(driverBase));
        }
    }
}

} // namespace mg
