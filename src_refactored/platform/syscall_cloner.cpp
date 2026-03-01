// =============================================================================
// SyscallCloner - Implementation
// =============================================================================
#include "pch.hpp"
#include "platform/syscall_cloner.hpp"
#include "platform/memory_pool.hpp"
#include "platform/import_resolver.hpp"
#include "platform/memory.hpp"

namespace mg {

SyscallCloner::SyscallCloner(MemoryPool& pool, ImportResolver& resolver)
    : pool_(pool)
    , resolver_(resolver)
{}

SyscallCloner::~SyscallCloner() = default;

VoidResult SyscallCloner::loadNtdllFromDisk() {
    char systemDir[MAX_PATH];
    if (GetSystemWow64DirectoryA(systemDir, MAX_PATH) == 0 ||
        GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) {
        GetSystemDirectoryA(systemDir, MAX_PATH);
    }

    char filePath[MAX_PATH];
    nocrtStrcpy(filePath, systemDir);
    // Append \\ntdll.dll manually
    auto len = nocrtStrlen(filePath);
    filePath[len] = '\\';
    nocrtStrcpy(filePath + len + 1, "ntdll.dll");

    std::ifstream file(filePath, std::ios::binary);
    if (!file) return VoidResult::err(ErrorCode::kFileIoError);

    ntdllDiskData_ = std::vector<u8>(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    if (ntdllDiskData_.empty()) return VoidResult::err(ErrorCode::kFileIoError);
    return VoidResult::ok();
}

VoidResult SyscallCloner::initialize() {
    if (initialized_) return VoidResult::ok();

    // Get in-memory ntdll base
    auto* ntdllHandle = GetModuleHandleA("ntdll.dll");
    if (!ntdllHandle) return VoidResult::err(ErrorCode::kModuleNotFound);
    ntdllBaseInMemory_ = reinterpret_cast<uptr>(ntdllHandle);

    auto diskResult = loadNtdllFromDisk();
    if (diskResult.isErr()) return diskResult;

    initialized_ = true;
    return VoidResult::ok();
}

uptr SyscallCloner::cloneInternal(uptr base, uptr offset) {
    if (ntdllDiskData_.empty()) return 0;

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(ntdllDiskData_.data());
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(ntdllDiskData_.data() + dos->e_lfanew);
    const auto* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(nt + 1);

    auto funcOffset = offset - base - nt->OptionalHeader.BaseOfCode;

    u8 blob[0x1000]{};
    ZydisDecoder decoder{};
    ZydisDecodedInstruction insn{};
    ZydisDecodedOperand ops[ZYDIS_MAX_OPERAND_COUNT]{};

    for (u16 i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        const auto& section = sections[i];
        if (section.VirtualAddress > funcOffset ||
            funcOffset > section.VirtualAddress + section.Misc.VirtualSize) {
            continue;
        }

        auto ntOffset = sections[0].PointerToRawData + funcOffset;
        const auto* buffer = ntdllDiskData_.data() + ntOffset;
        ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32);

        ZyanUSize codeOffset = 0;
        while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(
            &decoder, buffer + codeOffset, sizeof(blob) - codeOffset, &insn, ops))) {
            nocrtMemcpy(blob + codeOffset, buffer + codeOffset, insn.length);

            const auto& op = ops[1];
            if (op.type == ZYDIS_OPERAND_TYPE_MEMORY || op.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                if (op.imm.value.u > nt->OptionalHeader.ImageBase &&
                    op.imm.value.u < nt->OptionalHeader.ImageBase + nt->OptionalHeader.SizeOfImage) {
                    auto opOffset = insn.length - sizeof(u32);
                    auto& m = *reinterpret_cast<u32*>(blob + codeOffset + opOffset);
                    m += static_cast<u32>(base);
                    m -= static_cast<u32>(nt->OptionalHeader.ImageBase);
                }
            }

            codeOffset += insn.length;
            if (insn.mnemonic == ZYDIS_MNEMONIC_RET || insn.mnemonic == ZYDIS_MNEMONIC_IRET) {
                break;
            }
        }

        auto* finalBlob = pool_.alloc(codeOffset);
        if (!finalBlob) return 0;
        nocrtMemcpy(finalBlob, blob, codeOffset);
        return reinterpret_cast<uptr>(finalBlob);
    }
    return 0;
}

template <typename T>
T SyscallCloner::cloneSyscall(uptr ntdllBase, uptr functionOffset) {
    auto cloned = cloneInternal(ntdllBase, functionOffset);
    return reinterpret_cast<T>(cloned);
}

template <typename T>
T SyscallCloner::cloneSyscallByName(const char* funcName) {
    if (!initialized_) return nullptr;

    auto* func = GetProcAddress(reinterpret_cast<HMODULE>(ntdllBaseInMemory_), funcName);
    if (!func) return nullptr;

    auto cloned = cloneInternal(ntdllBaseInMemory_, reinterpret_cast<uptr>(func));
    if (cloned) return reinterpret_cast<T>(cloned);
    return reinterpret_cast<T>(func); // Fallback to original
}

// Explicit instantiations for common types
template void* SyscallCloner::cloneSyscallByName<void*>(const char*);

} // namespace mg
