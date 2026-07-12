// =============================================================================
// WindowCloseFix - Taskbar close (WM_CLOSE) bugfix
// =============================================================================
// The game's WndProc (0x0045F670) delegates messages to an internal UI handler
// (sub_E4E1C0) when byte_11DE338 is set. If that handler returns non-zero the
// message is consumed with `return 1`, which swallows WM_CLOSE and prevents
// the window from closing via taskbar → Close or Alt+F4.
//
// Fix: detour WndProc, handle WM_CLOSE before the original runs.
// =============================================================================
#include "pch.hpp"
#include "modding/bugfixes/window_close_fix.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "utils/cloakwork_isolation.hpp"
#include "utils/logger.hpp"

namespace mg::modding {

using namespace mg::game;

namespace {

LRESULT __stdcall hkWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    auto& registry = mg::ctx().hookRegistry();
    auto original = registry.findDetour(HookId::WndProcCloseFix)
        ->getOriginal<decltype(&hkWndProc)>();



    MG_LOG(mg::ctx().logger(),
        "hkWndProc Msg {}", Msg);
    if (Msg == WM_CLOSE)
    {
        DestroyWindow(hWnd);
        return 0;
    }
	else if (Msg == WM_DESTROY)
    {
        PostQuitMessage(0);
        ExitProcess(0);
        return 0;
    }

   


    return original(hWnd, Msg, wParam, lParam);
}

} // anonymous namespace

WindowCloseFix::WindowCloseFix(MegaGuardContext& ctx) : ctx_(ctx) {}
WindowCloseFix::~WindowCloseFix() = default;

VoidResult WindowCloseFix::install()
{
    auto& registry = ctx_.hookRegistry();

    registry.registerDetour(HookId::WndProcCloseFix)
        .create(MG_CONST(addr::bugfixes::WndProc), hkWndProc);

    return VoidResult::ok();
}

} // namespace mg::modding
