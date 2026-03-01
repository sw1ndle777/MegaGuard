// =============================================================================
// ImportResolver - Implementation
// =============================================================================
// CloakWork included ONLY here for CW_HASH, CW_GET_MODULE, CW_GET_PROC.
// =============================================================================
#include "pch.hpp"
#include "platform/import_resolver.hpp"
#include "platform/memory_pool.hpp"
#include "platform/memory.hpp"
#include "utils/cloakwork_isolation.hpp"

// Custom PEB structs — the SDK's winternl.h hides fields behind Reserved[]
namespace {

struct MG_PEB_LDR_DATA {
    ULONG Length;
    BOOLEAN Initialized;
    PVOID SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
};

struct MG_LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
};

struct MG_PEB {
    BOOLEAN InheritedAddressSpace;
    BOOLEAN ReadImageFileExecOptions;
    BOOLEAN BeingDebugged;
    BOOLEAN Spare;
    HANDLE  Mutant;
    PVOID   ImageBaseAddress;
    MG_PEB_LDR_DATA* Ldr;
};

} // anonymous namespace

namespace mg {

ImportResolver::ImportResolver(MemoryPool& pool)
    : pool_(pool)
{}

ImportResolver::~ImportResolver() = default;

VoidResult ImportResolver::initialize() {
    // Pre-resolve critical APIs that we need frequently
    // Each resolve is done lazily on first call, but we can pre-warm the cache
    return VoidResult::ok();
}

void* ImportResolver::resolve(u32 moduleHash, u32 procHash) {
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : cache_) {
            if (entry.moduleHash == moduleHash && entry.procHash == procHash) {
                return entry.address;
            }
        }
    }

    // Walk PEB->Ldr->InLoadOrderModuleList to find module by hash
    void* result = nullptr;

#ifdef _WIN64
    auto* peb = reinterpret_cast<MG_PEB*>(__readgsqword(0x60));
#else
    auto* peb = reinterpret_cast<MG_PEB*>(__readfsdword(0x30));
#endif

    auto* ldr = peb->Ldr;
    auto* head = &ldr->InLoadOrderModuleList;
    auto* entry = head->Flink;

    while (entry != head) {
        auto* mod = CONTAINING_RECORD(entry, MG_LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        entry = entry->Flink;

        if (!mod->DllBase) continue;

        // Hash the module name (case-insensitive)
        u32 modHash = 0x811C9DC5; // FNV-1a basis
        auto* name = mod->BaseDllName.Buffer;
        auto nameLen = mod->BaseDllName.Length / sizeof(wchar_t);
        for (u16 i = 0; i < nameLen; i++) {
            wchar_t c = name[i];
            if (c >= L'A' && c <= L'Z') c += 32; // toLower
            modHash ^= static_cast<u32>(c);
            modHash *= 0x01000193; // FNV prime
        }

        if (modHash != moduleHash) continue;

        // Found the module — now walk its export table
        auto* dosHeader = static_cast<IMAGE_DOS_HEADER*>(mod->DllBase);
        auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
            reinterpret_cast<u8*>(dosHeader) + dosHeader->e_lfanew);
        auto& exportDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

        if (exportDir.VirtualAddress == 0) break;

        auto* exports = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(
            reinterpret_cast<u8*>(dosHeader) + exportDir.VirtualAddress);
        auto* names = reinterpret_cast<u32*>(
            reinterpret_cast<u8*>(dosHeader) + exports->AddressOfNames);
        auto* ordinals = reinterpret_cast<u16*>(
            reinterpret_cast<u8*>(dosHeader) + exports->AddressOfNameOrdinals);
        auto* functions = reinterpret_cast<u32*>(
            reinterpret_cast<u8*>(dosHeader) + exports->AddressOfFunctions);

        for (u32 i = 0; i < exports->NumberOfNames; i++) {
            auto* funcName = reinterpret_cast<const char*>(
                reinterpret_cast<u8*>(dosHeader) + names[i]);

            // Hash function name
            u32 fHash = 0x811C9DC5;
            while (*funcName) {
                fHash ^= static_cast<u32>(*funcName);
                fHash *= 0x01000193;
                funcName++;
            }

            if (fHash == procHash) {
                u16 ordinal = ordinals[i];
                result = reinterpret_cast<void*>(
                    reinterpret_cast<u8*>(dosHeader) + functions[ordinal]);
                break;
            }
        }
        break;
    }

    // Cache the result
    if (result) {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.push_back({moduleHash, procHash, result});
    }

    return result;
}

void* ImportResolver::resolveByName(const char* moduleName, const char* procName) {
    // Runtime FNV-1a hash
    auto hashStr = [](const char* s, bool caseInsensitive) -> u32 {
        u32 h = 0x811C9DC5;
        while (*s) {
            char c = *s;
            if (caseInsensitive && c >= 'A' && c <= 'Z') c += 32;
            h ^= static_cast<u32>(c);
            h *= 0x01000193;
            s++;
        }
        return h;
    };
    auto hashWstr = [](const char* s) -> u32 {
        u32 h = 0x811C9DC5;
        while (*s) {
            wchar_t c = static_cast<wchar_t>(*s);
            if (c >= L'A' && c <= L'Z') c += 32;
            h ^= static_cast<u32>(c);
            h *= 0x01000193;
            s++;
        }
        return h;
    };

    return resolve(hashWstr(moduleName), hashStr(procName, false));
}

void* ImportResolver::getModuleHandle(u32 moduleHash) {
#ifdef _WIN64
    auto* peb = reinterpret_cast<MG_PEB*>(__readgsqword(0x60));
#else
    auto* peb = reinterpret_cast<MG_PEB*>(__readfsdword(0x30));
#endif

    auto* ldr = peb->Ldr;
    auto* head = &ldr->InLoadOrderModuleList;
    auto* entry = head->Flink;

    while (entry != head) {
        auto* mod = CONTAINING_RECORD(entry, MG_LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        entry = entry->Flink;
        if (!mod->DllBase) continue;

        u32 modHash = 0x811C9DC5;
        auto* name = mod->BaseDllName.Buffer;
        auto nameLen = mod->BaseDllName.Length / sizeof(wchar_t);
        for (u16 i = 0; i < nameLen; i++) {
            wchar_t c = name[i];
            if (c >= L'A' && c <= L'Z') c += 32;
            modHash ^= static_cast<u32>(c);
            modHash *= 0x01000193;
        }

        if (modHash == moduleHash) return mod->DllBase;
    }
    return nullptr;
}

} // namespace mg
