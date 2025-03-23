#pragma once

namespace MegaGuard
{
	namespace Splash
	{
		static ULONG_PTR gdiplusToken;
		static LRESULT CALLBACK SplashWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
		static HWND CreateSplashWindow(HINSTANCE hInstance);
		static void PositionWindow(HWND hwnd);
		void InitializeSplash();
	}
}

//#endif