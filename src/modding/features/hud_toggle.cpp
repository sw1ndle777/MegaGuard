// =============================================================================
// HudToggle - Legacy CTRL+U HUD visibility toggle
// =============================================================================
#include "pch.hpp"
#include "modding/features/hud_toggle.hpp"

#include "core/context.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "utils/call_helper.hpp"

namespace mg::modding {

using namespace mg::game;

namespace {

constexpr int kBattleCommonDialogHotkey = 40;
constexpr int kBattleDialogHotkey = 34;
constexpr int kBattleCommonHoldHotkey = 15;
constexpr int kHudToggleHotkey = 22;
constexpr int kExtendedHotkey25 = 25;
constexpr int kExtendedHotkey14 = 14;
constexpr int kExtendedHotkey16 = 16;

constexpr std::ptrdiff_t kInputContextOffset = 60;
constexpr std::ptrdiff_t kBattleStateOffset = 64;
constexpr std::ptrdiff_t kCrosshairOffset = 0x258;

void __fastcall hkLegacyHudToggleProcess(void* instance, u32 /*edx*/)
{
    const auto runtimeContext = mg::readValue<uptr>(addr::features::hud_toggle::RuntimeContext);
    const auto runtimeReady = mg::readValue<u8>(addr::features::hud_toggle::RuntimeReady) != 0;

    u8* inputContext = nullptr;
    if (runtimeContext && runtimeReady) {
        inputContext = mg::readValue<u8*>(runtimeContext, kInputContextOffset);
    }

    if (!inputContext) {
        return;
    }

    const auto battleState = (runtimeContext && runtimeReady)
        ? mg::readValue<i32>(runtimeContext, kBattleStateOffset)
        : 0;

    if (!battleState) {
        return;
    }

    auto* const keyState = reinterpret_cast<u8*>(addr::features::hud_toggle::KeyState);
    const auto inputContextAddress = static_cast<i32>(reinterpret_cast<uptr>(inputContext));

    if (mg::readValue<u8>(addr::features::hud_toggle::BattleCommonEnabled) != 0) {
        if (mg::call<bool(__thiscall*)(u8*, int)>(
            addr::features::hud_toggle::CheckKeyDown,
            keyState,
            kBattleCommonDialogHotkey)) {

            auto* const dialog = mg::call<void*(__thiscall*)(void*, const char*)>(
                addr::ui::gui_manager::GetDialogInfo,
                reinterpret_cast<void*>(addr::ui::gui_manager::Get),
                "E_DLG_BATTLE_COMMON");

            if (dialog) {
                mg::call<char(__thiscall*)(void*)>(
                    addr::features::hud_toggle::RenderDialog,
                    dialog);
            }
        }
    }

    if (mg::call<bool(__thiscall*)(u8*, int)>(
        addr::features::hud_toggle::CheckPrimaryAction,
        inputContext,
        1)) {
        mg::call<int(__thiscall*)(void*)>(
            addr::features::hud_toggle::HandlePrimaryAction,
            instance);
    }

    if (mg::call<bool(__thiscall*)(u8*, int)>(
        addr::features::hud_toggle::CheckKeyDown,
        keyState,
        kBattleDialogHotkey)) {
        mg::call<void(*)()>(addr::features::hud_toggle::ToggleBattleDialog);
    }

    if (mg::call<bool(__thiscall*)(u8*, int)>(
        addr::features::hud_toggle::CheckKeyDown,
        keyState,
        kBattleCommonHoldHotkey)) {
        mg::call<char(__stdcall*)(char)>(
            addr::features::hud_toggle::SetBattleCommonState,
            1);
    }

    mg::call<char(__thiscall*)(u32*)>(
        addr::features::hud_toggle::UpdateInputState,
        reinterpret_cast<u32*>(addr::features::hud_toggle::InputStateObject));

    mg::call<int(__stdcall*)(int, int)>(
        addr::features::hud_toggle::ProcessInputA,
        inputContextAddress,
        battleState);
    mg::call<void(__stdcall*)(int, int)>(
        addr::features::hud_toggle::ProcessInputB,
        inputContextAddress,
        battleState);
    mg::call<void(__stdcall*)(int, int)>(
        addr::features::hud_toggle::ProcessInputC,
        inputContextAddress,
        battleState);
    mg::call<int(__stdcall*)(int, int)>(
        addr::features::hud_toggle::ProcessInputA,
        inputContextAddress,
        battleState);

    if (mg::readValue<u8>(addr::features::hud_toggle::ReleaseBattleState) != 0 ||
        mg::call<bool(__thiscall*)(u8*, int)>(
            addr::features::hud_toggle::CheckKeyUp,
            keyState,
            kBattleCommonHoldHotkey)) {
        mg::call<char(__stdcall*)(char)>(
            addr::features::hud_toggle::SetBattleCommonState,
            0);
    }

    i32 hudToggleState = 0;
    if (mg::call<char(__thiscall*)(u32*, int, i32*)>(
        addr::features::hud_toggle::CheckHudHotkey,
        reinterpret_cast<u32*>(inputContext),
        kHudToggleHotkey,
        &hudToggleState) && (hudToggleState & 1) != 0) {

        const u8 showHudValue = mg::readValue<u8>(addr::features::hud_toggle::ShowHud) == 0 ? 1u : 0u;
        mg::writeValue<u8>(addr::features::hud_toggle::ShowHud, 0, showHudValue);

        auto* const world = mg::call<char*(__cdecl*)()>(addr::features::hud_toggle::WorldGetInstance);
        if (world) {
            const auto crosshair = mg::readValue<uptr>(reinterpret_cast<uptr>(world), kCrosshairOffset);
            if (crosshair != 0) {
                mg::call<void(__thiscall*)(uptr, int)>(
                    addr::features::hud_toggle::CrosshairSetVisibility,
                    crosshair,
                    static_cast<int>(showHudValue));
            }
        }
    }
    else if (mg::readValue<u8>(addr::features::hud_toggle::ExtendedInputEnabled) != 0) {
        if (mg::call<bool(__thiscall*)(u8*, int)>(
            addr::features::hud_toggle::CheckKeyDown,
            keyState,
            kExtendedHotkey25)) {
            mg::call<int(*)()>(addr::features::hud_toggle::HandleKey25);
        }

        if (mg::call<bool(__thiscall*)(u8*, int)>(
            addr::features::hud_toggle::CheckKeyDown,
            keyState,
            kExtendedHotkey14)) {
            mg::call<char(*)()>(addr::features::hud_toggle::HandleKey14);
        }

        if (mg::call<bool(__thiscall*)(u8*, int)>(
            addr::features::hud_toggle::CheckKeyDown,
            keyState,
            kExtendedHotkey16)) {
            mg::call<void*(__thiscall*)(void*)>(
                addr::features::hud_toggle::HandleKey16,
                instance);
        }

        mg::call<void(__stdcall*)()>(addr::features::hud_toggle::UpdateExtendedStateA);
        mg::call<void(__stdcall*)()>(addr::features::hud_toggle::UpdateExtendedStateB);
        mg::call<void(*)()>(addr::features::hud_toggle::UpdateExtendedStateC);
    }
}

} // anonymous namespace

HudToggle::HudToggle(MegaGuardContext& ctx) : ctx_(ctx) {}
HudToggle::~HudToggle() = default;

VoidResult HudToggle::install()
{
    auto& registry = ctx_.hookRegistry();
    if (!registry.registerDetour(HookId::HudToggleProcess)
            .create(addr::features::hud_toggle::Process, hkLegacyHudToggleProcess)) {
        return VoidResult::err(ErrorCode::kHookFailed);
    }

    return VoidResult::ok();
}

} // namespace mg::modding
