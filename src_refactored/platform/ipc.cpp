// =============================================================================
// IpcClient - Implementation
// =============================================================================
#include "pch.hpp"
#include "platform/ipc.hpp"

namespace mg {

constexpr const char* kIpcName = "Local\\MegaGuardSplashIPC_v1";
constexpr long kIpcVersion = 1;

IpcClient::IpcClient() = default;

IpcClient::~IpcClient() {
    close();
}

bool IpcClient::open() {
    mapping_ = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, kIpcName);
    if (!mapping_) return false;

    data_ = static_cast<IpcData*>(
        MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(IpcData)));
    if (!data_) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
        return false;
    }

    if (InterlockedCompareExchange(&data_->version, kIpcVersion, kIpcVersion) != kIpcVersion) {
        UnmapViewOfFile(data_);
        CloseHandle(mapping_);
        data_ = nullptr;
        mapping_ = nullptr;
        return false;
    }

    return true;
}

void IpcClient::close() {
    if (data_) {
        UnmapViewOfFile(data_);
        data_ = nullptr;
    }
    if (mapping_) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
    }
}

bool IpcClient::isConnected() const {
    return data_ != nullptr;
}

void IpcClient::setProgress(int progress) {
    if (data_) InterlockedExchange(&data_->progress, progress);
}

void IpcClient::setPhase(LoadPhase phase) {
    if (data_) InterlockedExchange(&data_->phase, static_cast<long>(phase));
}

void IpcClient::setManagerReady(int readyCount, int totalCount) {
    if (!data_) return;
    InterlockedExchange(&data_->managersReady, readyCount);
    InterlockedExchange(&data_->totalManagers, totalCount);
}

void IpcClient::signalAllReady() {
    if (data_) InterlockedExchange(&data_->allReady, 1);
}

void IpcClient::signalClose() {
    if (data_) InterlockedExchange(&data_->shouldClose, 1);
}

} // namespace mg
