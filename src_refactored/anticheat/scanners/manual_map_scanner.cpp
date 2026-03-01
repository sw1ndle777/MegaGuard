#include "pch.hpp"
#include "anticheat/scanners/manual_map_scanner.hpp"

namespace mg {

void ManualMapScanner::scan(std::atomic<u32>& flags, Logger& log) {
    auto modules = getLoadedModules();

    MEMORY_BASIC_INFORMATION mbi{};
    uptr cur = 0;
    constexpr uptr kLimit = 0x7FFFFFFF;

    while (cur < kLimit && VirtualQuery(reinterpret_cast<LPCVOID>(cur), &mbi, sizeof(mbi)) == sizeof(mbi)) {
        if (mbi.State != MEM_COMMIT || !(mbi.Protect & (PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_WRITECOPY | PAGE_EXECUTE))) {
            cur += mbi.RegionSize;
            continue;
        }

        uptr regionAddr = reinterpret_cast<uptr>(mbi.BaseAddress);
        if (isAddressInModules(modules, regionAddr) || isInsideAcModule(regionAddr)) {
            cur += mbi.RegionSize;
            continue;
        }

        auto* buf = reinterpret_cast<const u8*>(mbi.BaseAddress);

        if (isPEHeader(buf)) {
            auto dis = disassemble(buf, MG_INT(512u), regionAddr, MG_INT(5u));
            reportFlag(flags, log, DetectionFlag::kManualMap, MG_STR("ManualMap"),
                fmt::format("PE header at 0x{:X} size={}: {}", regionAddr, mbi.RegionSize, dis),
                static_cast<u32>(regionAddr));
        } else if (mbi.RegionSize >= MG_CONST(0x10000)) {
            PSAPI_WORKING_SET_EX_INFORMATION ws{};
            ws.VirtualAddress = mbi.BaseAddress;
            if (QueryWorkingSetEx(GetCurrentProcess(), &ws, sizeof(ws)) &&
                ws.VirtualAttributes.Valid && !ws.VirtualAttributes.Shared)
            {
                uptr textAddr = regionAddr + MG_CONST(0x1000);

                MEMORY_BASIC_INFORMATION textMbi{};
                if (VirtualQuery(reinterpret_cast<LPCVOID>(textAddr), &textMbi, sizeof(textMbi)) == sizeof(textMbi) &&
                    textMbi.State == MEM_COMMIT && !isInsideAcModule(textAddr))
                {
                    auto* textBuf = reinterpret_cast<const u8*>(textAddr);
                    bool hasCode = false;
                    for (u32 i = 0; i + 3 < MG_INT(128u); ++i) {
                        if (textBuf[i] && textBuf[i+1] && textBuf[i+2] && textBuf[i+3]) { hasCode = true; break; }
                    }
                    if (hasCode) {
                        auto dis = disassemble(textBuf, MG_INT(128u), textAddr, MG_INT(5u));
                        reportFlag(flags, log, DetectionFlag::kManualMap, MG_STR("ManualMap"),
                            fmt::format("Erased-header suspect at 0x{:X}: {}", textAddr, dis),
                            static_cast<u32>(textAddr));
                    }
                }
            }
        }
        cur += mbi.RegionSize;
    }
}

std::string ManualMapScanner::disassemble(const u8* data, u32 len, uptr addr, u32 maxInsns) {
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_COMPAT_32, ZYDIS_STACK_WIDTH_32);
    ZydisFormatter formatter;
    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

    ZydisDecodedInstruction insn;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

    std::string result;
    u32 off = 0, cnt = 0;
    while (off < len && cnt < maxInsns) {
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, data + off, len - off, &insn, operands)))
            break;
        char buf[128]{};
        ZydisFormatterFormatInstruction(&formatter, &insn, operands,
            insn.operand_count_visible, buf, sizeof(buf), addr + off, nullptr);
        if (!result.empty()) result += "; ";
        result += buf;
        off += insn.length;
        ++cnt;
    }
    return result;
}

} // namespace mg
