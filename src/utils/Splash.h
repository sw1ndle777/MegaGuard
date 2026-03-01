#pragma once

namespace MegaGuard
{
    namespace Splash
    {
        void UpdateProgress(int progress);
        void StartSplash();   // Non-blocking - spawns thread
        void OnManagerReady();
        
        inline std::atomic<int> g_managersReady = 0;
        inline std::atomic<bool> g_allManagersReady = false;
        inline std::atomic<int> g_loadProgress = 0;
        constexpr int TOTAL_MANAGERS = 4;
    }
}