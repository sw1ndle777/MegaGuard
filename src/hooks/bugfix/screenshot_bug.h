#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace BugFixes
        {
            struct tagRECT global_rect;

            inline void __fastcall ScreenShot(std::uint32_t instance, std::uint32_t edx, std::uint32_t a2)
            {
                static auto original = MegaGuard::HooksMgr::BugFixes::Screeshot_bug.GetOriginal<decltype(&ScreenShot)>();
                /*
                auto hwnd = _rv<HWND>(MegaGuard::Addresses::Hooks::Bugfixes::GameHWND.get());
                GetWindowRect(hwnd, &global_rect);
                global_rect.left = 0;
                global_rect.top = 0;
                if (global_rect.left > 0 || global_rect.top > 0) {
                    GetWindowRect(GetDesktopWindow(), &global_rect);
                }
                else {
                    global_rect.left = 0;
                    global_rect.top = 0;
                }
                */

				original(instance, edx, a2);
            }

            inline bool __stdcall Screenshot2(struct tagRECT* a1, char* FullPath, LPCSTR lpString)
            {
                static auto original = MegaGuard::HooksMgr::BugFixes::Screenshot2_bug.GetOriginal<decltype(&Screenshot2)>();
				return original(a1, FullPath, lpString);
            }
        }
    }
}