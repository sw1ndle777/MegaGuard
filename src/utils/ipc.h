#pragma once
#include <Windows.h>

// Shared memory name (Local = per-session, avoids needing SeCreateGlobalPrivilege)
#define MEGAGUARD_IPC_NAME "Local\\MegaGuardSplashIPC_v1"

// ============== Progress Phases ==============
enum MegaGuardPhase : LONG {
    MG_PHASE_INITIALIZING = 0,
    MG_PHASE_CONNECTING = 1,
    MG_PHASE_KEY_EXCHANGE = 2,
    MG_PHASE_DOWNLOADING = 3,
    MG_PHASE_DECRYPTING = 4,
    MG_PHASE_DECOMPRESSING = 5,
    MG_PHASE_MAPPING = 6,
    MG_PHASE_ANTICHEAT_INIT = 7,
    MG_PHASE_READY = 8,
    MG_PHASE_ERROR = -1
};

// ============== Shared Data (lives in named file mapping) ==============
struct MegaGuardIPCData {
    volatile LONG version;          // Must == 1 for compatibility check
    volatile LONG progress;         // 0-100 overall progress
    volatile LONG phase;            // MegaGuardPhase enum
    volatile LONG managersReady;    // Count of ready managers (written by mapped DLL)
    volatile LONG totalManagers;    // Total expected managers  (written by mapped DLL)
    volatile LONG allReady;         // 1 when anticheat fully initialized
    volatile LONG shouldClose;      // 1 to force-close splash immediately
};

// ============== Host (bootstrapper side - creates shared memory) ==============
class MegaGuardIPCHost {
public:
    bool Create() {
        m_hMapping = CreateFileMappingA(
            INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
            0, sizeof(MegaGuardIPCData), MEGAGUARD_IPC_NAME);
        if (!m_hMapping) return false;

        m_data = static_cast<MegaGuardIPCData*>(
            MapViewOfFile(m_hMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MegaGuardIPCData)));
        if (!m_data) { CloseHandle(m_hMapping); m_hMapping = nullptr; return false; }

        ZeroMemory(m_data, sizeof(MegaGuardIPCData));
        InterlockedExchange(&m_data->version, 1);
        return true;
    }

    void Destroy() {
        if (m_data) { UnmapViewOfFile(m_data); m_data = nullptr; }
        if (m_hMapping) { CloseHandle(m_hMapping); m_hMapping = nullptr; }
    }

    void SetProgress(int pct) { if (m_data) InterlockedExchange(&m_data->progress, pct); }
    void SetPhase(MegaGuardPhase phase) { if (m_data) InterlockedExchange(&m_data->phase, static_cast<LONG>(phase)); }
    void SignalClose() { if (m_data) InterlockedExchange(&m_data->shouldClose, 1); }

    bool IsAllReady() const { return m_data && m_data->allReady != 0; }
    MegaGuardIPCData* GetData() { return m_data; }

    ~MegaGuardIPCHost() { Destroy(); }

private:
    HANDLE m_hMapping = nullptr;
    MegaGuardIPCData* m_data = nullptr;
};

// ============== Client (mapped DLL side - opens existing shared memory) ==============
class MegaGuardIPCClient {
public:
    bool Open() {
        m_hMapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, MEGAGUARD_IPC_NAME);
        if (!m_hMapping) return false;

        m_data = static_cast<MegaGuardIPCData*>(
            MapViewOfFile(m_hMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MegaGuardIPCData)));
        if (!m_data) { CloseHandle(m_hMapping); m_hMapping = nullptr; return false; }

        return m_data->version == 1;
    }

    void Close() {
        if (m_data) { UnmapViewOfFile(m_data); m_data = nullptr; }
        if (m_hMapping) { CloseHandle(m_hMapping); m_hMapping = nullptr; }
    }

    // Call once early in DllMain to declare how many managers will report ready
    void SetTotalManagers(int count) {
        if (m_data) InterlockedExchange(&m_data->totalManagers, count);
    }

    // Call from each manager when it finishes initializing.
    // Automatically scales progress in the 72-95 range and sets allReady when done.
    void OnManagerReady() {
        if (!m_data) return;
        LONG ready = InterlockedIncrement(&m_data->managersReady);
        LONG total = m_data->totalManagers;
        if (total > 0) {
            int pct = 72 + static_cast<int>(23.0f * ready / total);
            InterlockedExchange(&m_data->progress, pct);
        }
        if (ready >= total && total > 0) {
            InterlockedExchange(&m_data->allReady, 1);
            InterlockedExchange(&m_data->progress, 100);
            InterlockedExchange(&m_data->phase, MG_PHASE_READY);
        }
    }

    // Shortcut: signal everything is ready in one call
    void SetAllReady() {
        if (m_data) {
            InterlockedExchange(&m_data->allReady, 1);
            InterlockedExchange(&m_data->progress, 100);
            InterlockedExchange(&m_data->phase, MG_PHASE_READY);
        }
    }

    // Direct setters (for custom progress logic)
    void SetProgress(int pct) { if (m_data) InterlockedExchange(&m_data->progress, pct); }
    void SetPhase(MegaGuardPhase phase) { if (m_data) InterlockedExchange(&m_data->phase, static_cast<LONG>(phase)); }

    bool IsOpen() const { return m_data != nullptr; }

    ~MegaGuardIPCClient() { Close(); }

private:
    HANDLE m_hMapping = nullptr;
    MegaGuardIPCData* m_data = nullptr;
};
