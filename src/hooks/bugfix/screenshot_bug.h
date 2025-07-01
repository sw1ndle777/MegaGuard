#pragma once
#include <d3d9.h>
#include <d3dx9tex.h>
#pragma comment(lib, "d3dx9.lib")
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace BugFixes
        {
            static tagRECT global_rect;
            static volatile LONG* g_screenshot_lock = (volatile LONG*)MegaGuard::Addresses::Hooks::Bugfixes::InterlockedScreenshot.get();

			static char g_filename_buffer[256];
			static char g_screenshot_text[64];
            
            struct ScreenshotProc
            {
                void** vtable;
                IDirect3DDevice9* d3d_device;
                char overlay[64];
            };
            using tTakeScreenshot = bool(__thiscall*)(ScreenshotProc*, RECT*, char*, const char*);
            static auto oTakeScreenshot = reinterpret_cast<tTakeScreenshot>(MegaGuard::Addresses::Hooks::Bugfixes::TakeScreenshot.get());

            inline bool SaveScreenshotToFile(ScreenshotProc* screenshot_proc, const RECT *rect, const char *filePath, const void *pixelData)
            {
                if (!pixelData)
                    return 0;

                int width  = rect->right - rect->left;
                int height = rect->bottom - rect->top;

                IDirect3DTexture9 *sysMemTexture = nullptr;

                // Create a system memory texture
                HRESULT createRes = screenshot_proc->d3d_device->CreateTexture(
                    width,
                    height,
                    1,
                    0,
                    D3DFMT_A8R8G8B8,
                    D3DPOOL_MANAGED,
                    &sysMemTexture,
                    nullptr
                );

                if (FAILED(createRes) || !sysMemTexture)
                    return false;

                D3DLOCKED_RECT lockedRect;
                RECT fullRect = { 0, 0, width, height };

                HRESULT lockRes = sysMemTexture->LockRect(0, &lockedRect, &fullRect, 0);
                if (FAILED(lockRes))
                {
                    sysMemTexture->Release();
                    return false;
                }

                // Copy pixels from raw buffer to texture memory (flipped vertically)
                DWORD *dest = (DWORD *)lockedRect.pBits;
                const DWORD *src = (const DWORD *)pixelData;

                for (int y = 0; y < height; ++y)
                {
                    for (int x = 0; x < width; ++x)
                    {
                        int srcIdx = x + width * (height - 1 - y); // vertical flip
                        *dest++ = src[srcIdx] | 0xFF000000;        // force alpha to full
                    }
                }
                sysMemTexture->UnlockRect(0);
                D3DXSaveTextureToFileA(filePath, D3DXIFF_PNG, sysMemTexture, nullptr);
                sysMemTexture->Release();

                return true;
            }

            bool CreateDirectoryRecursive(const char* path)
            {
                char buffer[MAX_PATH];
                strncpy_s(buffer, path, _TRUNCATE);
                for (char* p = buffer + 1; *p; ++p)
                {
                    if (*p == '\\' || *p == '/')
                    {
                        char temp = *p;
                        *p = '\0';

                        CreateDirectoryA(buffer, nullptr);

                        *p = temp;
                    }
                }

                return CreateDirectoryA(buffer, nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
            }

            bool EnsureScreenshotDirectory(const char* fullPath)
            {
                if (!fullPath || !*fullPath)
                    return false;

                char pathOnly[MAX_PATH];
                strncpy_s(pathOnly, fullPath, _TRUNCATE);
                for (int i = strlen(pathOnly) - 1; i >= 0; --i)
                {
                    if (pathOnly[i] == '\\' || pathOnly[i] == '/')
                    {
                        pathOnly[i] = '\0';
                        break;
                    }
                }

                return CreateDirectoryRecursive(pathOnly);
            }

            bool TakeScreenshot(ScreenshotProc* screenshot_proc, RECT* rect, const char* fullPath, const char* overlayText)
            {
                HDC hdcScreen = CreateDCA("DISPLAY", nullptr, nullptr, nullptr);
                if (!hdcScreen) return false;

                BITMAPINFO bmi = {};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = rect->right - rect->left;
                bmi.bmiHeader.biHeight = rect->bottom - rect->top;  
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;

                void* bits = nullptr;
                HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
                if (!hBitmap)
                {
                    DeleteDC(hdcScreen);
                    return false;
                }

                HDC hdcMem = CreateCompatibleDC(hdcScreen);
                HGDIOBJ oldObj = SelectObject(hdcMem, hBitmap);

                BitBlt(hdcMem, 0, 0, rect->right - rect->left, rect->bottom - rect->top, hdcScreen, rect->left, rect->top, SRCCOPY);

                HFONT hFont = CreateFontA(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                          ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          NONANTIALIASED_QUALITY, FF_SWISS, "Arial");
                SelectObject(hdcMem, hFont);
                SetBkMode(hdcMem, TRANSPARENT);
                SetTextColor(hdcMem, RGB(255, 255, 0));
                if (overlayText)
                    TextOutA(hdcMem, rect->right - rect->left - 320, 35, overlayText, lstrlenA(overlayText));
                DeleteObject(hFont);
                SelectObject(hdcMem, oldObj);
                DeleteDC(hdcMem);

                auto saved_ss = EnsureScreenshotDirectory(fullPath);

                saved_ss = SaveScreenshotToFile(screenshot_proc, rect, fullPath, bits);
                
                if ( hBitmap )
                    DeleteObject(hBitmap);
                if ( hdcScreen )
                    DeleteDC(hdcScreen);

                return saved_ss;
            }

            inline void __fastcall ScreenShot(ScreenshotProc* instance, int edx, int a2)
            {
                InterlockedIncrement(g_screenshot_lock);
                SYSTEMTIME SystemTime;
                GetLocalTime(&SystemTime);
				sprintf_s(g_filename_buffer, 256, ".\\Screenshot\\%s_%02d%02d%02d_%02d%02d%02d_%02d.png", 
                          "MegaVoltsOnline",
                          SystemTime.wYear % 100,
                          SystemTime.wMonth,
                          SystemTime.wDay,
                          SystemTime.wHour,
                          SystemTime.wMinute,
                          SystemTime.wSecond,
                          _rv<DWORD>(MegaGuard::Addresses::Hooks::Bugfixes::ScreenshotIncrementer.get()));

				sprintf_s(g_screenshot_text, 64, "20%02d/%02d/%02d %02d:%02d:%02d MegaVolts Online",
                          SystemTime.wYear % 100,
                          SystemTime.wMonth,
                          SystemTime.wDay,
						  SystemTime.wHour,
						  SystemTime.wMinute,
						  SystemTime.wSecond);
                
                
				D3DDEVICE_CREATION_PARAMETERS creation_parameters;
                instance->d3d_device->GetCreationParameters(&creation_parameters);
                RECT rect;
                GetWindowRect(creation_parameters.hFocusWindow, &rect);

                if (TakeScreenshot(instance, &rect, g_filename_buffer, g_screenshot_text))
                {
                    auto value = _rv<DWORD>(MegaGuard::Addresses::Hooks::Bugfixes::ScreenshotIncrementer.get());
                    _wv<DWORD>(MegaGuard::Addresses::Hooks::Bugfixes::ScreenshotIncrementer.get(), 0, value + 1);
                }
                InterlockedDecrement(g_screenshot_lock);
            }
        }
    }
}