#include "pch.h"

namespace MegaGuard
{
	namespace Splash
	{
        void InitializeSplash()
        {
            const int splashDisplayTime = 5000;

            Gdiplus::GdiplusStartupInput gdiplusStartupInput;    //Initialize GDI+
            Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

            HINSTANCE hInstance = GetModuleHandle(nullptr);

            HWND splashWnd = CreateSplashWindow(hInstance);
            ShowWindow(splashWnd, SW_SHOW);
            UpdateWindow(splashWnd);


            MSG msg;
            bool running = true;
            while (running)
            {
                while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
                {

                    if (msg.message == WM_QUIT)
                    {
                        running = false;
                        break;
                    }

                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }

                // Force repaint to animate the loading bar
                InvalidateRect(splashWnd, nullptr, FALSE);
                UpdateWindow(splashWnd);

                if (GetWindowLongPtr(splashWnd, GWLP_USERDATA) == 1)
                    running = false;

            }

            //Sleep(splashDisplayTime); // Sleep for 5 seconds

            // Hide and destroy splash window
            DestroyWindow(splashWnd);

            // Shutdown GDI+
            Gdiplus::GdiplusShutdown(gdiplusToken);
        }

        void PositionWindow(HWND hwnd)
        {
            RECT rc;
            GetWindowRect(hwnd, &rc);

            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);

            RECT workArea;
            SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

            int taskbarHeight = screenHeight - (workArea.bottom - workArea.top);

            int windowWidth = rc.right - rc.left;
            int windowHeight = rc.bottom - rc.top;

            int x = screenWidth - windowWidth - 10;
            int y = screenHeight - windowHeight - taskbarHeight - 10;

            SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
        }

        HWND CreateSplashWindow(HINSTANCE hInstance)
        {
            const wchar_t CLASS_NAME[] = L"SplashWindowClass";

            WNDCLASS wc = {};
            wc.lpfnWndProc = SplashWndProc;
            wc.hInstance = hInstance;
            wc.lpszClassName = CLASS_NAME;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

            RegisterClass(&wc);

            HWND hwnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, CLASS_NAME, nullptr, WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 180, 180, nullptr, nullptr, hInstance, nullptr);
            //SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);

            PositionWindow(hwnd);

            return hwnd;
        }

        LRESULT CALLBACK SplashWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            static int loadProgress = 0;     // Track loading progress from 0 to 100
            static UINT_PTR timerId = 1;      // Timer ID for progress animation

            switch (message)
            {
            case WM_CREATE:
            {
                SetTimer(hWnd, timerId, 50, nullptr); // Trigger the timer every 50 ms for smooth animation
            } break;

            case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hWnd, &ps);

                RECT clientRect;
                GetClientRect(hWnd, &clientRect);
                int winWidth = clientRect.right - clientRect.left;
                int winHeight = clientRect.bottom - clientRect.top;

                // Create an offscreen memory device context (Double Buffering)
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP memBitmap = CreateCompatibleBitmap(hdc, winWidth, winHeight);
                SelectObject(memDC, memBitmap);

                Gdiplus::Graphics graphics(memDC);

                // Clear background
                graphics.Clear(Gdiplus::Color(0, 0, 0, 0)); // Transparent background

                Gdiplus::Image image(L"megaguard\\splash.png");

                UINT imgWidth = image.GetWidth();
                UINT imgHeight = image.GetHeight();

                if (imgWidth == 0 || imgHeight == 0) // Failed to load image
                {
                    DeleteObject(memBitmap);
                    DeleteDC(memDC);
                    EndPaint(hWnd, &ps);
                    return 0;
                }

                // Adjust the image to fit the window size
                float imgAspect = (float)imgWidth / imgHeight;
                float winAspect = (float)winWidth / winHeight;

                int drawWidth, drawHeight;
                int offsetX = 0, offsetY = 0;

                if (winAspect > imgAspect)
                {
                    drawHeight = winHeight;
                    drawWidth = static_cast<int>(drawHeight * imgAspect);
                    offsetX = (winWidth - drawWidth) / 2;
                }
                else
                {
                    drawWidth = winWidth;
                    drawHeight = static_cast<int>(drawWidth / imgAspect);
                    offsetY = (winHeight - drawHeight) / 2;
                }

                graphics.DrawImage(&image, offsetX, offsetY, drawWidth, drawHeight);

                // Loading bar dimensions and position
                int barWidth = 160;
                int barHeight = 10;
                int barX = (winWidth - barWidth) / 2;
                int barY = winHeight - 20;

                // Colors
                Gdiplus::Color darkGray(50, 50, 50);
                Gdiplus::Color silver(192, 192, 192);
                Gdiplus::Color black(0, 0, 0);

                // Background bar
                Gdiplus::SolidBrush grayBrush(darkGray);
                graphics.FillRectangle(&grayBrush, barX, barY, barWidth, barHeight);

                // Progress bar (animated)
                Gdiplus::SolidBrush silverBrush(silver);
                int filledWidth = static_cast<int>((barWidth - 4) * (loadProgress / 100.0));
                graphics.FillRectangle(&silverBrush, barX + 2, barY + 2, filledWidth, barHeight - 4);

                // Border
                Gdiplus::Pen blackPen(black, 2);
                graphics.DrawRectangle(&blackPen, barX, barY, barWidth, barHeight);

                // Transfer the offscreen rendering to the window
                BitBlt(hdc, 0, 0, winWidth, winHeight, memDC, 0, 0, SRCCOPY);

                // Clean up
                DeleteObject(memBitmap);
                DeleteDC(memDC);

                EndPaint(hWnd, &ps);
            } break;

            case WM_TIMER:
            {
                if (wParam == timerId)
                {
                    loadProgress += 1; // Increment progress by 1%

                    if (loadProgress >= 100)
                    {
                        loadProgress = 100;  // Make sure it doesn't go beyond 100
                        SetWindowLongPtr(hWnd, GWLP_USERDATA, 1);  // Signal completion to InitializeSplash()
                    }

                    InvalidateRect(hWnd, nullptr, FALSE); // Request a redraw
                }
            } break;

            case WM_CLOSE:
            {
                DestroyWindow(hWnd);
            } break;

            case WM_DESTROY:
            {
                KillTimer(hWnd, timerId); // Stop the timer
                PostQuitMessage(0);
            } break;

            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
            return 0;
        }

	}
}