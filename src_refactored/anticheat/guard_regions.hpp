#pragma once

#include "core/types.hpp"

namespace mg {

class MegaGuardContext;
class DetourHook;

class GuardRegions {
public:
    enum class PageState : u32 {
        kEncrypted,
        kDecrypted,
    };

    struct PageStateData {
        volatile LONG lock;
        PageState state;
        DWORD     oldProtect;
        u32       xorKey;
        DWORD64   lastAccessTick;
        u32       accessCount;
    };

    struct ModuleRange {
        uptr base;
        uptr end;
    };

    static constexpr int kMaxWhitelistRanges = 32;

    explicit GuardRegions(MegaGuardContext& ctx);
    ~GuardRegions();

    GuardRegions(const GuardRegions&) = delete;
    GuardRegions& operator=(const GuardRegions&) = delete;

    VoidResult initialize();
    void       shutdown();

    void encryptSection(const char* sectionName);
    void startReencryptWorker();
    void addWhitelistRange(uptr base, uptr size);
    void addExcludedRange(uptr base, uptr size);

    static GuardRegions* instance();

    BOOLEAN dispatchException(PEXCEPTION_RECORD exRecord, PCONTEXT ctxRecord);

private:
    static PIMAGE_SECTION_HEADER getSectionByName(HMODULE hMod, const char* name);
    static bool  sectionNameEquals(const char* a, const char* b);
    static void  xorEncryptPage(uptr pageAddr, u32 key);
    bool  isCallerWhitelisted(uptr eip) const;
    bool  isPageExcluded(uptr pageAddr) const;
    static DWORD WINAPI reencryptThreadProc(LPVOID param);

    void registerPages(uptr base, std::size_t size, DWORD originalProtect);

    MegaGuardContext& ctx_;

    boost::unordered_flat_map<uptr, PageStateData> pageInfo_;
    std::vector<uptr> pendingPages_;
    volatile LONG mapLock_ = 0;
    volatile LONG queueLock_ = 0;

    ModuleRange whitelistedRanges_[kMaxWhitelistRanges] = {};
    int whitelistCount_ = 0;

    ModuleRange excludedRanges_[kMaxWhitelistRanges] = {};
    int excludedCount_ = 0;

    volatile bool running_ = false;
    HANDLE           workerThread_ = nullptr;

    volatile u32  dbgDecryptCount_ = 0;
    volatile u32  dbgEncryptCount_ = 0;
    volatile u32  dbgForwardCount_ = 0;

    std::unique_ptr<DetourHook> kiDispatcherHook_;

    inline static GuardRegions* s_instance = nullptr;
};

} // namespace mg
