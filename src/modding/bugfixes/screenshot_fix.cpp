// =============================================================================
// ScreenshotFix - D3D9/GDI screenshot capture bugfix
// =============================================================================
// Ported from screenshot_bug.h. Replaces broken in-game screenshot with:
//   - GDI screen capture via CreateDCA("DISPLAY") + BitBlt
//   - Font overlay with timestamp (Arial 20pt yellow)
//   - D3D9 texture → PNG via D3DXSaveTextureToFileA
//   - Vertical-flip pixel copy with forced alpha
//   - InterlockedIncrement/Decrement screenshot lock
// =============================================================================
#include "pch.hpp"
#include "modding/bugfixes/screenshot_fix.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"

#include <gdiplus.h>     // GDI+ for screen capture
#pragma comment(lib, "gdiplus.lib")
#include <d3d9.h>
#include <d3dx9tex.h>
#pragma comment(lib, "d3dx9.lib")

namespace mg::modding {

using namespace mg::game;

namespace {

// ── Per-module state (frame-persistent, thread-local to game thread) ─────────
char g_filename_buffer[256];
char g_screenshot_text[64];

// ── ScreenshotProc structure matching the game's internal layout ─────────────
struct ScreenshotProc
{
    void** vtable;
    IDirect3DDevice9* d3d_device;
    char overlay[64];
};

// ── SaveScreenshotToFile ─────────────────────────────────────────────────────
// Creates a D3D9 system-memory texture, copies pixels (vertical flip + alpha
// force), saves as PNG via D3DXSaveTextureToFileA.

bool SaveScreenshotToFile(
    ScreenshotProc* screenshot_proc,
    const RECT* rect,
    const char* filePath,
    const void* pixelData)
{
    if (!pixelData)
        return false;

    int width  = rect->right - rect->left;
    int height = rect->bottom - rect->top;

    IDirect3DTexture9* sysMemTexture = nullptr;

    HRESULT createRes = screenshot_proc->d3d_device->CreateTexture(
        width, height, 1, 0,
        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
        &sysMemTexture, nullptr);

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
    DWORD* dest = reinterpret_cast<DWORD*>(lockedRect.pBits);
    const DWORD* src = reinterpret_cast<const DWORD*>(pixelData);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int srcIdx = x + width * (height - 1 - y); // vertical flip
            *dest++ = src[srcIdx] | 0xFF000000;         // force alpha to full
        }
    }

    sysMemTexture->UnlockRect(0);
    D3DXSaveTextureToFileA(filePath, D3DXIFF_PNG, sysMemTexture, nullptr);
    sysMemTexture->Release();

    return true;
}

// ── CreateDirectoryRecursive ─────────────────────────────────────────────────

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

// ── EnsureScreenshotDirectory ────────────────────────────────────────────────

bool EnsureScreenshotDirectory(const char* fullPath)
{
    if (!fullPath || !*fullPath)
        return false;

    char pathOnly[MAX_PATH];
    strncpy_s(pathOnly, fullPath, _TRUNCATE);
    for (int i = static_cast<int>(strlen(pathOnly)) - 1; i >= 0; --i)
    {
        if (pathOnly[i] == '\\' || pathOnly[i] == '/')
        {
            pathOnly[i] = '\0';
            break;
        }
    }
    return CreateDirectoryRecursive(pathOnly);
}

// ── TakeScreenshot ───────────────────────────────────────────────────────────
// GDI capture: CreateDCA("DISPLAY") → DIB section → BitBlt → font overlay →
// SaveScreenshotToFile

bool TakeScreenshot(
    ScreenshotProc* screenshot_proc,
    RECT* rect,
    const char* fullPath,
    const char* overlayText)
{
    HDC hdcScreen = CreateDCA(MG_STR("DISPLAY"), nullptr, nullptr, nullptr);
    if (!hdcScreen) return false;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = rect->right - rect->left;
    bmi.bmiHeader.biHeight      = rect->bottom - rect->top;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
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

    BitBlt(hdcMem, 0, 0,
        rect->right - rect->left,
        rect->bottom - rect->top,
        hdcScreen, rect->left, rect->top, SRCCOPY);

    // Font overlay: Arial 20pt bold, yellow, transparent background
    HFONT hFont = CreateFontA(
        20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, FF_SWISS, MG_STR("Arial"));

    SelectObject(hdcMem, hFont);
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, RGB(255, 255, 0));

    if (overlayText)
    {
        TextOutA(hdcMem,
            rect->right - rect->left - 320, 35,
            overlayText, lstrlenA(overlayText));
    }

    DeleteObject(hFont);
    SelectObject(hdcMem, oldObj);
    DeleteDC(hdcMem);

    EnsureScreenshotDirectory(fullPath);
    auto saved = SaveScreenshotToFile(screenshot_proc, rect, fullPath, bits);

    if (hBitmap) DeleteObject(hBitmap);
    if (hdcScreen) DeleteDC(hdcScreen);

    return saved;
}

// ── ScreenShot hook ──────────────────────────────────────────────────────────

void __fastcall hkScreenShot(ScreenshotProc* instance, int edx, int a2)
{
    auto g_screenshot_lock = reinterpret_cast<volatile LONG*>(
        MG_CONST(addr::bugfixes::InterlockedScreenshot));

    InterlockedIncrement(g_screenshot_lock);

    SYSTEMTIME SystemTime;
    GetLocalTime(&SystemTime);

    sprintf_s(g_filename_buffer, 256,
        ".\\Screenshot\\%s_%02d%02d%02d_%02d%02d%02d_%02d.png",
        MG_STR("MegaVoltsOnline"),
        SystemTime.wYear % 100,
        SystemTime.wMonth,
        SystemTime.wDay,
        SystemTime.wHour,
        SystemTime.wMinute,
        SystemTime.wSecond,
        mg::readValue<DWORD>(MG_CONST(addr::bugfixes::ScreenshotIncrementer), 0));

    sprintf_s(g_screenshot_text, 64,
        "20%02d/%02d/%02d %02d:%02d:%02d %s",
        SystemTime.wYear % 100,
        SystemTime.wMonth,
        SystemTime.wDay,
        SystemTime.wHour,
        SystemTime.wMinute,
        SystemTime.wSecond,
        MG_STR("MegaVolts Online"));

    D3DDEVICE_CREATION_PARAMETERS creation_parameters;
    instance->d3d_device->GetCreationParameters(&creation_parameters);

    RECT rect;
    GetWindowRect(creation_parameters.hFocusWindow, &rect);

    if (TakeScreenshot(instance, &rect, g_filename_buffer, g_screenshot_text))
    {
        auto value = mg::readValue<DWORD>(
            MG_CONST(addr::bugfixes::ScreenshotIncrementer), 0);
        mg::writeValue<DWORD>(
            MG_CONST(addr::bugfixes::ScreenshotIncrementer), 0, value + 1);
    }

    InterlockedDecrement(g_screenshot_lock);
}

} // anonymous namespace

// ── ScreenshotFix class ──────────────────────────────────────────────────────

ScreenshotFix::ScreenshotFix(MegaGuardContext& ctx) : ctx_(ctx) {}
ScreenshotFix::~ScreenshotFix() = default;

VoidResult ScreenshotFix::install()
{
    auto& registry = ctx_.hookRegistry();

    registry.registerDetour(HookId::ScreenshotBug)
        .create(MG_CONST(addr::bugfixes::ScreenshotBug1), hkScreenShot);

    // NOP out the SetDateTimeShit (6 bytes → 0x90)
    registry.registerPatchBytes(HookId::SetDateTimeNop)
        .patch(MG_CONST(addr::bugfixes::SetDateTimeShit),
            "\x90\x90\x90\x90\x90\x90", 6);

    return VoidResult::ok();
}

} // namespace mg::modding
