#include "pch.hpp"
#include "anticheat/scanners/section_remap_scanner.hpp"
#include "anticheat/guard_regions.hpp"

namespace mg {

// NtQueryVirtualMemory information classes
constexpr ULONG MemoryBasicInformation      = 0;
constexpr ULONG MemoryMappedFilenameInfo     = 2;
constexpr ULONG MemorySectionName            = 2;

using NtQueryVirtualMemory_t = NTSTATUS(NTAPI*)(
    HANDLE, PVOID, ULONG, PVOID, SIZE_T, PSIZE_T);

void SectionRemapScanner::scan(std::atomic<u32>& flags, Logger& log) {
    auto* guard = GuardRegions::instance();
    if (!guard) return;

    // Resolve NtQueryVirtualMemory each scan (cheap, avoids stale pointer)
    HMODULE ntdll = GetModuleHandleA(MG_STR("ntdll.dll"));
    if (!ntdll) return;
    auto NtQueryVM = reinterpret_cast<NtQueryVirtualMemory_t>(
        GetProcAddress(ntdll, MG_STR("NtQueryVirtualMemory")));
    if (!NtQueryVM) return;

    MEMORY_BASIC_INFORMATION mbi{};
    uptr cur = 0;
    constexpr uptr kLimit = 0x7FFFFFFF;
    auto modules = getLoadedModules();

    while (cur < kLimit &&
           VirtualQuery(reinterpret_cast<LPCVOID>(cur), &mbi, sizeof(mbi)) == sizeof(mbi))
    {
        // We only care about committed, mapped regions with R/W access
        if (mbi.State == MEM_COMMIT &&
            mbi.Type == MEM_MAPPED &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)))
        {
            uptr regionBase = reinterpret_cast<uptr>(mbi.BaseAddress);
            SIZE_T regionSize = mbi.RegionSize;

            // Skip known modules and our own AC module
            if (!isAddressInModules(modules, regionBase) && !isInsideAcModule(regionBase)) {

                // Check if this is pagefile-backed (no file name) — suspicious
                wchar_t nameBuf[512]{};
                SIZE_T retLen = 0;
                NTSTATUS st = NtQueryVM(
                    GetCurrentProcess(),
                    reinterpret_cast<PVOID>(regionBase),
                    MemoryMappedFilenameInfo,
                    nameBuf, sizeof(nameBuf), &retLen);

                // STATUS_SUCCESS with a path = file-backed (likely legitimate)
                // Failure / empty = pagefile-backed section (suspicious for dual-map)
                bool isPagefileBacked = (st != 0) || (retLen == 0);

                if (isPagefileBacked && regionSize >= 0x1000) {
                    // Walk pages in this mapped region and compare against guarded pages
                    for (SIZE_T off = 0; off < regionSize; off += 0x1000) {
                        uptr shadowPage = regionBase + off;

                        // Read shadow content safely
                        u8 probe[16]{};
                        SIZE_T bytesRead = 0;
                        if (!ReadProcessMemory(GetCurrentProcess(),
                                reinterpret_cast<LPCVOID>(shadowPage),
                                probe, sizeof(probe), &bytesRead) || bytesRead < sizeof(probe))
                        {
                            continue;
                        }

                        // Check if any bytes are non-zero (non-empty mapping)
                        bool hasContent = false;
                        for (auto b : probe) { if (b != 0) { hasContent = true; break; } }

                        if (hasContent) {
                            reportFlag(flags, log, DetectionFlag::kMappedImage, MG_STR("SectionRemap"),
                                fmt::format("Pagefile-backed R/W mapped region at 0x{:08X} size=0x{:X} "
                                            "(potential dual-map bypass)", shadowPage, regionSize),
                                static_cast<u32>(shadowPage));
                            break; // one report per region is enough
                        }
                    }
                }
            }
        }
        cur += mbi.RegionSize;
        if (mbi.RegionSize == 0) break;
    }
}

} // namespace mg
