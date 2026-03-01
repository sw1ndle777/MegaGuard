#include "pch.hpp"
#include "anticheat/scanners/iat_hook_scanner.hpp"

namespace mg {

void IATHookScanner::scan(std::atomic<u32>& flags, Logger& log) {
    auto modules = getLoadedModules();
    if (modules.empty()) return;

    HMODULE gameMod = reinterpret_cast<HMODULE>(game::addr::globals::g_GameModuleBase);
    if (!gameMod) return;

    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(gameMod);
    if (IsBadReadPtr(dosHeader, sizeof(IMAGE_DOS_HEADER))) return;
    auto* ntHeader = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<u8*>(gameMod) + dosHeader->e_lfanew);
    if (IsBadReadPtr(ntHeader, sizeof(IMAGE_NT_HEADERS))) return;
    auto& impDir = ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (impDir.Size == 0) return;

    auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(reinterpret_cast<u8*>(gameMod) + impDir.VirtualAddress);

    while (!IsBadReadPtr(desc, sizeof(IMAGE_IMPORT_DESCRIPTOR)) &&
           (desc->OriginalFirstThunk || desc->FirstThunk))
    {
        const char* dllName = "unknown";
        if (desc->Name) {
            auto* namePtr = reinterpret_cast<const char*>(reinterpret_cast<u8*>(gameMod) + desc->Name);
            if (!IsBadReadPtr(namePtr, 1))
                dllName = namePtr;
        }

        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(reinterpret_cast<u8*>(gameMod) + desc->FirstThunk);
        auto* ont = desc->OriginalFirstThunk
            ? reinterpret_cast<IMAGE_THUNK_DATA*>(reinterpret_cast<u8*>(gameMod) + desc->OriginalFirstThunk)
            : nullptr;

        u32 idx = 0;
        while (!IsBadReadPtr(iat, sizeof(IMAGE_THUNK_DATA)) && iat->u1.Function) {
            uptr funcPtr = static_cast<uptr>(iat->u1.Function);

            if (!isAddressInModules(modules, funcPtr) && !isInsideAcModule(funcPtr)) {
                const char* fnName = "(ordinal)";
                if (ont && !IsBadReadPtr(&ont[idx], sizeof(IMAGE_THUNK_DATA)) &&
                    ont[idx].u1.AddressOfData && !(ont[idx].u1.Ordinal & IMAGE_ORDINAL_FLAG32))
                {
                    auto* hint = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                        reinterpret_cast<u8*>(gameMod) + ont[idx].u1.AddressOfData);
                    if (!IsBadReadPtr(hint, sizeof(IMAGE_IMPORT_BY_NAME)))
                        fnName = reinterpret_cast<const char*>(hint->Name);
                }
                reportFlag(flags, log, DetectionFlag::kIatHook, MG_STR("IATHook"),
                    fmt::format("{}!{} -> 0x{:X}", dllName, fnName, funcPtr),
                    static_cast<u32>(funcPtr));
            }
            ++iat;
            ++idx;
        }
        ++desc;
    }
}

} // namespace mg
