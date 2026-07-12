#include "pch.hpp"
#include "modding/features/editbox_clipboard.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::modding {

using namespace mg::game;

namespace {

using CopyFn    = void(__thiscall*)(u32*);
using PasteFn   = int(__thiscall*)(u32*);
using DelSelFn  = int(__thiscall*)(u32*);
using SetPosFn  = int(__thiscall*)(u32*, int);
using GetTxtFn  = u8*(__thiscall*)(u32*);
using RefreshFn = void(__thiscall*)(u32*);
using NotifyFn  = int(__thiscall*)(void*, int, u8, u32*);

char __fastcall hkEditBoxKeyHandler(u32* pThis, u32 /*edx*/, int msg, int key, int param)
{
    if (msg == MG_CONST(addr::editbox::WmKeyDown)) {
        short ctrlState = GetKeyState(VK_CONTROL);
        if (ctrlState < 0) {
            switch (key) {
            case 'C':
                mg::call<CopyFn>(MG_CONST(addr::editbox::CopyToClipboard), pThis);
                return 1;

            case 'V':
                mg::call<PasteFn>(MG_CONST(addr::editbox::PasteFromClip), pThis);
                mg::call<NotifyFn>(MG_CONST(addr::editbox::NotifyParent),
                    *reinterpret_cast<void**>(reinterpret_cast<u8*>(pThis) + MG_CONST(addr::editbox::ParentPtrOff)),
                    MG_CONST(addr::editbox::NotifyTextChange), 1u, pThis);
                mg::call<RefreshFn>(MG_CONST(addr::editbox::RefreshDisplay), pThis);
                return 1;

            case 'X':
                mg::call<CopyFn>(MG_CONST(addr::editbox::CopyToClipboard), pThis);
                mg::call<DelSelFn>(MG_CONST(addr::editbox::DeleteSelection), pThis);
                mg::call<NotifyFn>(MG_CONST(addr::editbox::NotifyParent),
                    *reinterpret_cast<void**>(reinterpret_cast<u8*>(pThis) + MG_CONST(addr::editbox::ParentPtrOff)),
                    MG_CONST(addr::editbox::NotifyTextChange), 1u, pThis);
                mg::call<RefreshFn>(MG_CONST(addr::editbox::RefreshDisplay), pThis);
                return 1;

            case 'A':
            {
                auto txt = mg::call<GetTxtFn>(MG_CONST(addr::editbox::GetTextW), pThis);
                int len = *reinterpret_cast<int*>(txt + 8);
                mg::call<SetPosFn>(MG_CONST(addr::editbox::SetCaretPos), pThis, len);
                *reinterpret_cast<u32*>(reinterpret_cast<u8*>(pThis) + MG_CONST(addr::editbox::SelectionAnchor)) = 0;
                mg::call<RefreshFn>(MG_CONST(addr::editbox::RefreshDisplay), pThis);
                return 1;
            }
            }
        }
    }

    auto& registry = mg::ctx().hookRegistry();
    auto original = registry.findDetour(HookId::EditBoxClipboard)
        ->getOriginal<decltype(&hkEditBoxKeyHandler)>();
    return original(pThis, 0, msg, key, param);
}

} // anonymous namespace

EditBoxClipboard::EditBoxClipboard(MegaGuardContext& ctx) : ctx_(ctx) {}
EditBoxClipboard::~EditBoxClipboard() = default;

VoidResult EditBoxClipboard::install() {
    auto& registry = ctx_.hookRegistry();
    registry.registerDetour(HookId::EditBoxClipboard)
        .create(MG_CONST(addr::editbox::KeyHandler), hkEditBoxKeyHandler);
    return VoidResult::ok();
}

} // namespace mg::modding
