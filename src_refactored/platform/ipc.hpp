// =============================================================================
// IpcClient - Shared-memory IPC with bootstrapper
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg {

enum class LoadPhase : i32 {
    kInitializing   = 0,
    kConnecting     = 1,
    kKeyExchange    = 2,
    kDownloading    = 3,
    kDecrypting     = 4,
    kDecompressing  = 5,
    kMapping        = 6,
    kAntiCheatInit  = 7,
    kReady          = 8,
    kError          = -1
};

class IpcClient {
public:
    IpcClient();
    ~IpcClient();

    IpcClient(const IpcClient&) = delete;
    IpcClient& operator=(const IpcClient&) = delete;

    bool open();
    void close();
    bool isConnected() const;

    void setProgress(int progress);
    void setPhase(LoadPhase phase);
    void setManagerReady(int readyCount, int totalCount);
    void signalAllReady();
    void signalClose();

private:
    struct IpcData {
        volatile long version;
        volatile long progress;
        volatile long phase;
        volatile long managersReady;
        volatile long totalManagers;
        volatile long allReady;
        volatile long shouldClose;
    };

    void* mapping_ = nullptr;
    IpcData* data_ = nullptr;
};

} // namespace mg
