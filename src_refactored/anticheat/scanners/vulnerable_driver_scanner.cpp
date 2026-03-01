#include "pch.hpp"
#include "anticheat/scanners/vulnerable_driver_scanner.hpp"

namespace mg {

void VulnerableDriverScanner::scan(std::atomic<u32>& flags, Logger& log) {
    const char* kVuln[] = {
        "dbutil_2_3.sys", "rtcore64.sys", "gdrv.sys", "cpuz141.sys",
        "elrawdisk.sys", "iqvw64e.sys", "msio64.sys", "winio64.sys",
        "phymemx64.sys", "aswarpot.sys", "ene.sys", "gmerdrv.sys",
        "asrdrv106.sys", "BS_HWMIO64_W10.sys", "BS_I2cIo.sys",
        "kdmapper.sys", "DBUtilDrv2.sys",
    };

    LPVOID drivers[1024]{};
    DWORD cbNeeded = 0;
    if (!EnumDeviceDrivers(drivers, sizeof(drivers), &cbNeeded)) return;

    u32 cnt = cbNeeded / sizeof(LPVOID);
    for (u32 i = 0; i < cnt; ++i) {
        char drvName[MAX_PATH]{};
        if (!GetDeviceDriverBaseNameA(drivers[i], drvName, sizeof(drvName))) continue;
        for (auto* v : kVuln) {
            if (_stricmp(drvName, v) == 0)
                reportFlag(flags, log, DetectionFlag::kVulnerableDriver, MG_STR("DriverScan"),
                    fmt::format("Vulnerable: {} @ 0x{:X}", drvName, reinterpret_cast<uptr>(drivers[i])),
                    static_cast<u32>(reinterpret_cast<uptr>(drivers[i])));
        }
    }
}

} // namespace mg
