#include "pch.h"
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
namespace MegaGuard
{
	namespace Splash
	{
		static ULONG_PTR gdiplusToken = 0;
		static std::wstring currentQuote;
		static int lastQuoteIndex = -1;
		static HWND g_splashHwnd = nullptr;
		static DWORD g_lastQuoteChange = 0;
		static bool g_done = false;
		static HANDLE g_splashThread = nullptr;

		static const std::vector<std::wstring> LOADING_QUOTES = {
			L"\"Rumpus Room feels lonely today...\"",
			L"\"Someone left toys in Junk Yard!\"",
			L"\"Knox raided the fridge again.\"",
			L"\"Pandora sharpening her fangs!\"",
			L"\"Naomi says patience is virtue~\"",
			L"\"Chip running diagnostics...\"",
			L"\"MegaGuard protecting you <3\""
		};

		static std::wstring GetRandomQuote()
		{
			static std::mt19937 rng(static_cast<unsigned>(GetTickCount()));
			std::uniform_int_distribution<int> dist(0, static_cast<int>(LOADING_QUOTES.size()) - 1);
			int newIndex;
			do { newIndex = dist(rng); } while (newIndex == lastQuoteIndex && LOADING_QUOTES.size() > 1);
			lastQuoteIndex = newIndex;
			return LOADING_QUOTES[newIndex];
		}

		static HINSTANCE hInstance = GetModuleHandleW(nullptr);

		static LRESULT CALLBACK SplashWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			constexpr UINT_PTR TIMER_ID = 1;
			constexpr int SIZE = 180;

			switch (message)
			{
			case WM_CREATE:
				SetTimer(hWnd, TIMER_ID, 50, nullptr);
				return 0;

			case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hWnd, &ps);
				HDC memDC = CreateCompatibleDC(hdc);
				HBITMAP memBitmap = CreateCompatibleBitmap(hdc, SIZE, SIZE);
				HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

				Gdiplus::Graphics graphics(memDC);
				graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
				graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

				wchar_t exePath[MAX_PATH];
				GetModuleFileNameW(hInstance, exePath, MAX_PATH);
				PathRemoveFileSpecW(exePath);
				wcscat_s(exePath, L"\\megaguard\\splash.png");

				Gdiplus::Image image(exePath);
				if (image.GetLastStatus() == Gdiplus::Ok)
					graphics.DrawImage(&image, 0, 0, SIZE, SIZE);
				else
				{
					Gdiplus::SolidBrush bgBrush(Gdiplus::Color(255, 20, 20, 25));
					graphics.FillRectangle(&bgBrush, 0, 0, SIZE, SIZE);
				}

				// Quote
				Gdiplus::FontFamily fontFamily(L"Segoe UI");
				Gdiplus::Font quoteFont(&fontFamily, 9, Gdiplus::FontStyleItalic, Gdiplus::UnitPoint);
				Gdiplus::SolidBrush textBrush(Gdiplus::Color(240, 255, 220, 120));
				Gdiplus::StringFormat sf;
				sf.SetAlignment(Gdiplus::StringAlignmentCenter);
				Gdiplus::RectF textRect(0, SIZE - 50, (float)SIZE, 20);
				graphics.DrawString(currentQuote.c_str(), -1, &quoteFont, textRect, &sf, &textBrush);

				// Progress bar
				constexpr int barMargin = 12, barHeight = 10, barY = SIZE - 22;
				constexpr int barWidth = SIZE - (barMargin * 2);

				Gdiplus::SolidBrush barBgBrush(Gdiplus::Color(200, 25, 25, 30));
				graphics.FillRectangle(&barBgBrush, barMargin, barY, barWidth, barHeight);

				int progress = g_loadProgress.load();
				int fillWidth = (barWidth - 2) * progress / 100;
				if (fillWidth > 2)
				{
					Gdiplus::LinearGradientBrush gradientBrush(
						Gdiplus::Point(barMargin, barY), Gdiplus::Point(barMargin + barWidth, barY),
						Gdiplus::Color(255, 180, 30, 30), Gdiplus::Color(255, 255, 200, 50));
					graphics.FillRectangle(&gradientBrush, barMargin + 1, barY + 1, fillWidth, barHeight - 2);
				}

				Gdiplus::Pen barBorderPen(Gdiplus::Color(220, 100, 60, 40), 1.0f);
				graphics.DrawRectangle(&barBorderPen, barMargin, barY, barWidth - 1, barHeight - 1);

				BitBlt(hdc, 0, 0, SIZE, SIZE, memDC, 0, 0, SRCCOPY);
				SelectObject(memDC, oldBitmap);
				DeleteObject(memBitmap);
				DeleteDC(memDC);
				EndPaint(hWnd, &ps);
				return 0;
			}

			case WM_TIMER:
				if (wParam == TIMER_ID)
				{
					DWORD now = GetTickCount();
					if ((now - g_lastQuoteChange) > 4000 && !g_done)
					{
						currentQuote = GetRandomQuote();
						g_lastQuoteChange = now;
					}

					if (g_allManagersReady && !g_done)
					{
						if (g_loadProgress < 100)
							g_loadProgress++;
						else
						{
							static DWORD finishTime = 0;
							if (finishTime == 0)
							{
								finishTime = GetTickCount();
								currentQuote = L"\"Ready to play! Have fun!\"";
							}
							else if ((GetTickCount() - finishTime) > 3000)
							{
								g_done = true;
								DestroyWindow(hWnd);
							}
						}
					}
					InvalidateRect(hWnd, nullptr, FALSE);
				}
				return 0;

			case WM_DESTROY:
				KillTimer(hWnd, TIMER_ID);
				PostQuitMessage(0);
				return 0;
			}
			return DefWindowProc(hWnd, message, wParam, lParam);
		}

		static DWORD WINAPI SplashThreadProc(LPVOID)
		{
			currentQuote = GetRandomQuote();
			g_loadProgress = 0;
			g_done = false;
			g_allManagersReady = false;
			g_managersReady = 0;
			g_lastQuoteChange = GetTickCount();

			Gdiplus::GdiplusStartupInput gdiplusStartupInput;
			if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Gdiplus::Ok)
				return 0;

			const wchar_t CLASS_NAME[] = L"MegaGuardSplash";

			WNDCLASSEXW wc = {};
			wc.cbSize = sizeof(WNDCLASSEXW);
			wc.lpfnWndProc = SplashWndProc;
			wc.hInstance = hInstance;
			wc.lpszClassName = CLASS_NAME;
			wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
			RegisterClassExW(&wc);

			constexpr int SIZE = 180, MARGIN = 12;
			RECT workArea;
			SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
			int posX = workArea.right - SIZE - MARGIN;
			int posY = workArea.bottom - SIZE - MARGIN;

			g_splashHwnd = CreateWindowExW(
				WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
				CLASS_NAME, L"", WS_POPUP,
				posX, posY, SIZE, SIZE,
				nullptr, nullptr, hInstance, nullptr);

			if (g_splashHwnd)
			{
				SetLayeredWindowAttributes(g_splashHwnd, 0, 250, LWA_ALPHA);
				ShowWindow(g_splashHwnd, SW_SHOWNOACTIVATE);
				UpdateWindow(g_splashHwnd);

				MSG msg;
				while (GetMessage(&msg, nullptr, 0, 0) > 0)
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}

			Gdiplus::GdiplusShutdown(gdiplusToken);
			g_splashHwnd = nullptr;
			return 0;
		}

		void UpdateProgress(int progress)
		{
			int current = g_loadProgress.load();
			if (progress > current && progress <= 100)
				g_loadProgress.store(progress);
		}

		void OnManagerReady()
		{
			if (++g_managersReady >= TOTAL_MANAGERS)
				g_allManagersReady = true;
		}

		void StartSplash()
		{
			g_splashThread = CreateThread(nullptr, 0, SplashThreadProc, nullptr, 0, nullptr);
		}
	}
}