// =============================================================================
// MemoryFog — Implementation
// =============================================================================
#include "pch.hpp"
#include "anticheat/memory_fog.hpp"
#include "anticheat/scanners/scanner_common.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg {

MemoryFog::MemoryFog(NtApi& api, Logger& log)
    : api_(api), log_(log) {}

MemoryFog::~MemoryFog() {
    deactivate();
}

bool   MemoryFog::isActive() const       { return !mappings_.empty(); }
size_t MemoryFog::totalReserved() const   { return totalReserved_; }
const std::vector<MemFogMapping>& MemoryFog::mappings() const { return mappings_; }

size_t MemoryFog::safeMaxReservation(u32 multiplier) const {
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    size_t ram = GlobalMemoryStatusEx(&mem)
        ? static_cast<size_t>(mem.ullTotalPhys)
        : static_cast<size_t>(16ULL * 1024 * 1024 * 1024);
    return ram * multiplier;
}

bool MemoryFog::containsAddress(uptr addr) const {
    for (auto& m : mappings_) {
        uptr base = reinterpret_cast<uptr>(m.base);
        if (addr >= base && addr < base + m.size)
            return true;
    }
    return false;
}

VoidResult MemoryFog::activate(const MemFogConfig& cfg) {
    if (!api_.NtCreateTransaction || !api_.NtCreateSection ||
        !api_.NtMapViewOfSection || !api_.CreateFileTransactedA) {
        //log_.info("[MemFog] NT APIs unavailable");
        return VoidResult::err(ErrorCode::kProcNotFound);
    }

    // Create a transaction for the ghost file (never committed to disk)
    OBJECT_ATTRIBUTES oa{};
    oa.Length = sizeof(oa);
    HANDLE hTx = nullptr;
    NTSTATUS st = api_.NtCreateTransaction(
        &hTx, MG_CONST(0x12003F), &oa,
        nullptr, nullptr, 0, 0, 0, nullptr, nullptr);
    if (!NT_SUCCESS(st) || !hTx) {
        //log_.error("[MemFog] NtCreateTransaction failed (0x{:08X})", static_cast<u32>(st));
        return VoidResult::err(ErrorCode::kInitFailed);
    }

    // Create transacted temp file (exists only within the transaction)
    char tmp[32]{};
    snprintf(tmp, sizeof(tmp), "~mfg%llx.tmp", GetTickCount64());
    HANDLE hFile = api_.CreateFileTransactedA(
        tmp, GENERIC_READ | GENERIC_WRITE, 0,
        nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
        nullptr, hTx, nullptr, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        CloseHandle(hTx);
        //log_.error("[MemFog] CreateFileTransactedA failed");
        return VoidResult::err(ErrorCode::kInitFailed);
    }

    // Write minimal content to make the file valid for section backing
    const u8 kContent[] = { 'm', 'e', 'm', 'f', 'o', 'g' };
    DWORD written = 0;
    if (!WriteFile(hFile, kContent, sizeof(kContent), &written, nullptr)) {
        CloseHandle(hFile);
        CloseHandle(hTx);
        //log_.error("[MemFog] WriteFile failed");
        return VoidResult::err(ErrorCode::kInitFailed);
    }

    // Create section backed by the transacted file
    HANDLE hSection = nullptr;
    st = api_.NtCreateSection(
        &hSection, SECTION_ALL_ACCESS, nullptr, nullptr,
        PAGE_READWRITE, SEC_COMMIT, hFile);
    CloseHandle(hFile);
    if (!NT_SUCCESS(st) || !hSection) {
        CloseHandle(hTx);
        //log_.error("[MemFog] NtCreateSection failed (0x{:08X})", static_cast<u32>(st));
        return VoidResult::err(ErrorCode::kInitFailed);
    }
    section_ = hSection;

    // Map huge reserved views to flood the virtual address space.
    // Each view forces memory scanners to iterate page table entries.
    // Only 1 page (0x1000) is physically committed per view.
    const size_t maxTotal = safeMaxReservation(cfg.ramMultiplier);
    void* hint = nullptr;
    mappings_.reserve(cfg.viewCount);

    for (u32 i = 0; i < cfg.viewCount; ++i) {
        if (totalReserved_ >= maxTotal) break;

        bool mapped = false;
        for (u32 shift = cfg.maxSizeShift; shift >= cfg.minSizeShift; --shift) {
            void* base = hint;
            SIZE_T viewSize = static_cast<SIZE_T>(1ULL << shift);
            st = api_.NtMapViewOfSection(
                hSection, GetCurrentProcess(), &base,
                0,        // ZeroBits — no address constraint
                0x1000,   // CommitSize — 1 page physically committed
                nullptr,  // SectionOffset
                &viewSize,
                2,        // ViewUnmap (section_inherit)
                0x2000,   // MEM_RESERVE — key to the technique
                PAGE_READWRITE);

            if (NT_SUCCESS(st) && base) {
                mappings_.push_back({base, viewSize});
                totalReserved_ += viewSize;
                hint = static_cast<u8*>(base) + viewSize;
                //log_.info("[MemFog] View {}: 0x{:X} size={}MB",
                //    i, reinterpret_cast<uptr>(base), viewSize / (1024 * 1024));
                mapped = true;
                break;
            }
        }
        //if (!mapped)
            //log_.info("[MemFog] View {} — all sizes failed", i);
    }
    CloseHandle(hTx);

    if (mappings_.empty()) {
        //log_.error("[MemFog] All mapping attempts failed");
        CloseHandle(hSection);
        section_ = nullptr;
        return VoidResult::err(ErrorCode::kInitFailed);
    }

    //log_.info("[MemFog] Active: {} views, {}MB reserved",
    //   mappings_.size(), totalReserved_ / (1024 * 1024));
    return VoidResult::ok();
}

void MemoryFog::deactivate() {
    for (auto& m : mappings_) {
        if (m.base && api_.NtUnmapViewOfSection)
            api_.NtUnmapViewOfSection(GetCurrentProcess(), m.base);
    }
    mappings_.clear();
    totalReserved_ = 0;

    if (section_) {
        CloseHandle(static_cast<HANDLE>(section_));
        section_ = nullptr;
    }
}

} // namespace mg
