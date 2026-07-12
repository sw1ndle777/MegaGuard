// =============================================================================
// Game Addresses - All hardcoded addresses for the target client
// =============================================================================
// Uses CW_CONST in release builds for compile-time obfuscation.
// In dev builds, addresses are plain constants.
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::game::addr {

// ── Helper: address declaration ───────────────────────────────────────────────
// In release, these could be wrapped with CW_CONST via a macro.
// For now, plain constexpr for clarity. The .cpp will resolve at runtime.

// ── Features ──────────────────────────────────────────────────────────────────

namespace features {
    constexpr uptr HideWeaponSlot          = 0x005CEAC9;
    constexpr uptr NationIndex             = 0x011C8A70;   // DATA address where nation index value lives
    constexpr uptr GetPublisher            = 0x005628C0;
    constexpr uptr GetPublisherURL         = 0x005629C0;
    constexpr uptr Custom_GetNationIndex   = 0x00562AC0;
    constexpr uptr Custom_GetWindowTitle   = 0x00562BC0;
    constexpr uptr GetNationFlag           = 0x00562CC0;
    constexpr uptr GetFontName             = 0x00562DC0;
    constexpr uptr GetBoldFontName         = 0x00562EC0;
    constexpr uptr GetNationID             = 0x00562FC0;
    constexpr uptr GetGameBrand            = 0x005630B0;
    constexpr uptr GetGameTitle            = 0x00563180;
    constexpr uptr SpectateObserver        = 0x00576E40;
    constexpr uptr StartDropWeapon         = 0x00884A10;   // CCh_BasicDropper::vtbl[1] — weapon ragdoll drop on death
    constexpr uptr MovementSend            = 0x00B20440;   // SEND_USER.cpp — client→server movement
    constexpr uptr MovementReceive         = 0x00AE1E10;   // OTHER.cpp — OTHER_MOVE receive handler
    constexpr uptr DrawDebugInfo           = 0x0050DDC0;
    constexpr uptr CommonAgoraDlgConstruct = 0x00694BE0;
    constexpr uptr CommonAgoraDlgInit      = 0x0059AC10;
    constexpr uptr InitPcBangInfo          = 0x00698910;
    constexpr uptr AgoraMenuOnMessage      = 0x006961C0;   // CDlgAgoraMenu::OnMessage vtable[3] — button click handler

    namespace item_info {
        constexpr uptr ListGet            = 0x00520A20;   // CSingleton<CItemInfoList>::Get() → dword_11DC9BC
        constexpr uptr ListFind           = 0x0051A880;   // CItemInfoList::Find(itemId) __thiscall → item info ptr or 0
    }

    namespace slot_render {
        constexpr uptr BaseSlotListVtable = 0x010F728C;   // UI_SlotList base vtable (46 entries), vtable[41]=0x710A80 no-op
        constexpr uptr IconInfoListGet    = 0x005F3AA0;   // CSingleton<CIconInfoList>::Get() → singleton ptr
        constexpr uptr IconInfoListFind   = 0x005EFB30;   // CIconInfoList::Find(iconKey) __thiscall, key on stack
        constexpr uptr IconInfoListInst   = 0x011DE470;   // CIconInfoList singleton data (ecx for ExtractTexture)
        constexpr uptr ExtractTexture     = 0x005ACC00;   // __thiscall(CIconInfoList*, outBuf[5], iconInfo) → fills tex coords + handle
        constexpr uptr ClipSlotRect       = 0x00E8B890;   // __thiscall(UI_SlotList*, RECT*) — clips rect to slot area
        constexpr uptr GetRenderSingleton = 0x00E7AEC0;   // () → render context singleton (dword_15F2E38)
        constexpr uptr RenderSprite       = 0x00E7B290;   // __thiscall(renderCtx, 15 stack params) — draws textured quad
    }

    namespace dlg {
        constexpr uptr CFactoryGet              = 0x005205E0;
        constexpr uptr CExPICBaseAlloc          = 0x005EE0F0;
        constexpr uptr CExSLTMonthEventFactory  = 0x005EF270;
        constexpr uptr GetDlgInfo               = 0x008B4600;
        constexpr uptr GetDlgId                 = 0x00DFA720;
        constexpr uptr GetDlgById               = 0x00DFA680;
        constexpr uptr AssignDlgInfo            = 0x00E4E760;
        constexpr uptr GetDialogByName          = 0x00E4E660;   // __thiscall(reg=0x11DE330, const char* name) -> dialog obj (data area at +0x80C)
        constexpr uptr GetSelectedSlotItem      = 0x00E941B0;   // __thiscall(ignored) -> *(g_selectedSlotEntry+32) item-info ptr (sub_E941B0)
        constexpr uptr StampAppearReset         = 0x00E8BA40;   // __thiscall(control) resets appear-anim state (sub_E8BA40); pair with +1636=5,+874=1,+1956=0
    }

    namespace hud_toggle {
        constexpr uptr Process               = 0x00BA2140;
        constexpr uptr CheckHudHotkey        = 0x00BAF1B0;
        constexpr uptr RenderDialog          = 0x007400B0;
        constexpr uptr CheckKeyDown          = 0x004392F0;
        constexpr uptr CheckPrimaryAction    = 0x00559580;
        constexpr uptr HandlePrimaryAction   = 0x00BA1EF0;
        constexpr uptr ToggleBattleDialog    = 0x00BA1C20;
        constexpr uptr SetBattleCommonState  = 0x00B8C040;
        constexpr uptr UpdateInputState      = 0x005BF5B0;
        constexpr uptr ProcessInputA         = 0x005C0CB0;
        constexpr uptr ProcessInputB         = 0x005BE840;
        constexpr uptr ProcessInputC         = 0x005BEC20;
        constexpr uptr CheckKeyUp            = 0x00439370;
        constexpr uptr HandleKey25           = 0x00BA1D50;
        constexpr uptr HandleKey14           = 0x00BA2030;
        constexpr uptr HandleKey16           = 0x00BA2430;
        constexpr uptr UpdateExtendedStateA  = 0x00BA2360;
        constexpr uptr UpdateExtendedStateB  = 0x00BA1B70;
        constexpr uptr UpdateExtendedStateC  = 0x00BA2570;
        constexpr uptr WorldGetInstance      = 0x00563420;
        constexpr uptr CrosshairSetVisibility = 0x0058D560;

        constexpr uptr RuntimeContext        = 0x011D5F38;
        constexpr uptr RuntimeReady          = 0x011D5E8C;
        constexpr uptr BattleCommonEnabled   = 0x011C7CCE;
        constexpr uptr KeyState              = 0x011D5E88;
        constexpr uptr InputStateObject      = 0x011DEC2C;
        constexpr uptr ReleaseBattleState    = 0x011D5E9B;
        constexpr uptr ExtendedInputEnabled  = 0x011D5E8E;
        constexpr uptr ShowHud               = 0x011C8F5C;
    }

    // ── Per-zoom mouse sensitivity (rifle ADS / sniper scope) ───────────────
    namespace zoom_sensitivity {
        // Hook targets
        constexpr uptr SensMultiplier     = 0x008FF010; // double __thiscall(CCameraTPS*, int rawDelta) — look-delta scaler (sub_8FF010)
        constexpr uptr BattleAdjust       = 0x00BA2570; // void __cdecl() — in-battle sens/accel keybind handler (sub_BA2570)
        constexpr uptr LoadCfg            = 0x0040C9E0; // char __thiscall(COption*, char load, int which, int section) (sub_40C9E0)
        constexpr uptr SaveCfg            = 0x00412840; // char __thiscall(COption*, char doWrite) (sub_412840)

        // Engine helpers
        constexpr uptr CWorld_GetInstance = 0x00563420; // __cdecl()->CWorld*
        constexpr uptr NationFolder       = 0x00562AC0; // __cdecl()->const char* ("ROM"/"JPN"/...) (sub_562AC0)
        constexpr uptr StringMyptr        = 0x00F8B3E0; // std::string::_Myptr __thiscall(str)->char*

        // Game-state gate: zoom runtime only runs in an actual playing match, so the
        // pointer walk (and its VirtualQuery probes) never run during the cold first
        // match-load — that load-time per-frame work contended with asset streaming
        // and showed as a black screen. 0x1F = MOD_PLAYING, 0x20 = TUTORIAL_PLAYING.
        constexpr uptr GameState          = 0x011E6D38; // g_CurrGameState
        constexpr int  StateModPlaying    = 0x1F;
        constexpr int  StateTutorialPlay  = 0x20;

        // Live globals
        constexpr uptr SensVal            = 0x011C6908; // int base sensitivity (senitivity_val)
        constexpr uptr AccelVal           = 0x011C690C; // int base accel factor (acceleration_val)
        constexpr uptr AccelOnVal         = 0x011C6912; // byte: mouse acceleration enabled (read in sub_8FF010)
        constexpr uptr InvertVal          = 0x011C6911; // byte: invert mouse (applied in the look path)

        // Option-dialog radios for the two booleans (per-tier). Off/On pairs; reading
        // the "On" control's +RadioCheckedField gives the current bool (per sub_64F470).
        constexpr int  IdAccelOffRadio    = 103204;
        constexpr int  IdAccelOnRadio     = 103205;
        constexpr int  IdInvertOffRadio   = 103206;
        constexpr int  IdInvertOnRadio    = 103207;
        constexpr int  RadioCheckedField  = 4068;  // component+4068 -> checked (0/1)
        constexpr int  RadioSetCheckedOff = 236;   // component vtable BYTE offset 236 -> select/check radio

        // Layout offsets
        constexpr int  ActiveCamOff       = 0x268; // CWorld+0x268 -> active camera (sub_563990)
        constexpr int  CamOwner           = 0x20;  // camera+0x20 -> owner
        constexpr int  OwnerExPlayer      = 0x10C; // owner+0x10C -> CExPlayer
        constexpr int  ExPlayerSlot       = 0x28;  // exPlayer+0x28 -> equip slot
        constexpr int  VtblGetWeapon      = 9;     // slot vtable index 9 (byte 36) -> CWeapon
        constexpr int  WeaponCategory     = 0x28C; // 3 = sniper, 1 = rifle
        constexpr int  WeaponZoomA        = 0xD0;  // sniper scope-stage state A
        constexpr int  WeaponZoomB        = 0xD4;  // sniper scope-stage state B
        constexpr int  CamViewMode        = 212;   // active cam ViewMode (1 = rifle ADS)
        constexpr int  OptionBasePath     = 8468;  // COption+8468 -> std::string base path

        // ── Options menu (Control tab) integration ──
        constexpr uptr OptionApply        = 0x0064F470; // void __thiscall(dlg, int tab) — UI -> globals (sub_64F470)
        constexpr uptr OptionPopulate     = 0x0064D890; // void __thiscall(dlg, int tab) — globals -> UI (sub_64D890)
        constexpr uptr DlgFindComponent   = 0x008B4600; // int* __thiscall(dlg+1712, &id); *ret = component (sub_8B4600)
        constexpr uptr SliderSetValue     = 0x00E84A70; // int __thiscall(component, int value, char flag) — clamp+set+refresh (sub_E84A70)
        constexpr int  DlgComponentMap    = 1712;  // dlg+1712 -> component map (1st arg to DlgFindComponent)
        constexpr int  SliderValueField   = 2016;  // component+2016 -> current slider value (read on apply)
        constexpr int  LabelSetTextVIdx   = 52;    // component vtable index 52 (byte +208) -> set numeric text
        constexpr int  OptionTabControl   = 0;     // tab arg for the Control page (also 5 = all)
        constexpr int  OptionTabAll       = 5;

        // ── Per-zoom selector (Normal/Rifle/Sniper combo + the stock sens/accel sliders) ──
        // Mirrors the Steam client: ONE combo on the Control tab picks the tier, and the
        // existing mouse sens/accel sliders edit whichever tier is selected. The combo
        // (E_DLG_OPTION_CBX_CON_OP01_0, id 103400) is added to SCENE_COMMON.xml; the
        // sliders/value-labels below are the stock controls this build already ships.
        constexpr int  IdZoomCombo        = 103400; // the Normal/Rifle/Sniper combo we added
        constexpr int  IdSensSlider       = 103234; // E_DLG_OPTION_SLI_CON_OP01 (mouse sensitivity)
        constexpr int  IdAccelSlider      = 103235; // E_DLG_OPTION_SLI_CON_OP02 (mouse acceleration)
        constexpr int  IdSensValueLabel   = 103256; // numeric label beside the sens slider
        constexpr int  IdAccelValueLabel  = 103260; // numeric label beside the accel slider

        // ComboBox API (all __thiscall on the combo component returned by DlgFindComponent)
        constexpr uptr ComboReset         = 0x00E925A0; // ResetContent(combo) — clears items, sets the "current" combo (sub_E925A0)
        constexpr uptr ComboAddItem       = 0x00E923A0; // __thiscall AddItem(combo, const wchar_t*, 0, data) (sub_E923A0)
        constexpr uptr ComboSetCurSel     = 0x00E92810; // SetCurSel(combo, int idx, char refresh) (sub_E92810)
        constexpr uptr ComboGetCount      = 0x00E92E20; // GetItemCount(combo) (sub_E92E20)
        constexpr int  ComboCurSelField   = 4104;       // combo+0x1008 -> current selection index
        // Control event dispatch: sub_E74FC0(target, msg, flag, sourceControl). Fires msg 513
        // on a combo selection change from ANY path (dropdown click OR programmatic SetCurSel),
        // so this is what we hook to live-refresh the sliders when the user picks a tier.
        constexpr uptr ComboEventDispatch = 0x00E74FC0; // sub_E74FC0
        constexpr int  ComboSelChangeMsg  = 513;        // selection-change notification
        // Combo mouse handler: char __thiscall(combo, msg, POINT, a4, a5) (sub_E90920).
        // We hook it to turn a click on our combo into "advance tier" (click-to-cycle)
        // instead of opening the dropdown popup, which this binary can't render for a
        // hook-injected Controls combo. The header text renders fine, so cycling shows
        // Normal/Rifle/Sniper there; the 513 it fires drives our slider reload.
        constexpr uptr ComboMouseHandler  = 0x00E90920; // sub_E90920
        constexpr int  MsgLBtnDown        = 513;        // WM_LBUTTONDOWN
        constexpr int  MsgLBtnUp          = 514;        // WM_LBUTTONUP

        // ── Combo controller registration ───────────────────────────────────────
        // The binary registers every native combo's controller/binding in sub_5AB010
        // but SKIPS our CON combo (it doesn't exist in the stock client). Without that
        // binding the dropdown popup never initializes — this is THE missing setup.
        // We hook sub_5AB010 and append the same registration the native combos get.
        constexpr uptr ComboRegisterAll   = 0x005AB010; // sub_5AB010 — registers all combos (hook target)
        constexpr uptr UiFactoryRegistry  = 0x005205E0; // sub_5205E0 — get factory-registry singleton (__cdecl)
        constexpr uptr ComboRegFactory    = 0x00DFA680; // sub_DFA680(mgr, id, factory) — register factory for id
        constexpr uptr ComboMakeBinding   = 0x00DFA720; // sub_DFA720(mgr, id) -> binding (invokes the factory)
        constexpr uptr ComboBindNodename  = 0x00E4E760; // sub_E4E760(reg, nodename, binding) — bind by nodename
        constexpr uptr ComboFactoryFn     = 0x005EF870; // sub_5EF870 — generic combo controller factory
    }

    // Tickrate addresses
    namespace tickrate {
            constexpr uptr Frametime1 = 0x00A542CB;
            constexpr uptr Frametime2 = 0x00A54800;
            constexpr uptr Frametime3 = 0x00A54AD0;
            constexpr uptr Frametime4 = 0x00A54CF5;

            constexpr uptr RotDamp1 = 0x00A541F6;
            constexpr uptr RotDamp2 = 0x00A54A6C;
            constexpr uptr RotDamp3 = 0x00A54C91;

            constexpr uptr MinRotSpeed1 = 0x00A54205;
            constexpr uptr MinRotSpeed2 = 0x00A54214;
            constexpr uptr MinRotSpeed3 = 0x00A54A84;
            constexpr uptr MinRotSpeed4 = 0x00A54A93;
            constexpr uptr MinRotSpeed5 = 0x00A54CA9;
            constexpr uptr MinRotSpeed6 = 0x00A54CB8;

            constexpr uptr RotThreshold1 = 0x00A541F0;
            constexpr uptr RotThreshold2 = 0x00A542DD;
            constexpr uptr RotThreshold3 = 0x00A547D3;  // don't use
            constexpr uptr RotThreshold4 = 0x00A54A66;
            constexpr uptr RotThreshold5 = 0x00A54AE2;
            constexpr uptr RotThreshold6 = 0x00A54C8B;
            constexpr uptr RotThreshold7 = 0x00A54D07;

            constexpr uptr RotThresholdLimit = 0x00A54E87;

            constexpr uptr MaxRotSpeed1 = 0x00A54222;
            constexpr uptr MaxRotSpeed2 = 0x00A54231;
            constexpr uptr MaxRotSpeed3 = 0x00A54AA7;
            constexpr uptr MaxRotSpeed4 = 0x00A54AB6;
            constexpr uptr MaxRotSpeed5 = 0x00A54CCC;
            constexpr uptr MaxRotSpeed6 = 0x00A54CDB;

            constexpr uptr DelayReq1 = 0x009007AF;
            constexpr uptr DelayReq2 = 0x009007E9;
            constexpr uptr DelayReq3 = 0x00920935;
            constexpr uptr DelayReq4 = 0x00920957;
            constexpr uptr DelayReq5 = 0x009573EA;
            constexpr uptr DelayReq6 = 0x0095780A;
            constexpr uptr DelayReq7 = 0x00957829;
            constexpr uptr DelayReq8 = 0x009579DC;
            constexpr uptr DelayReq9 = 0x00957B63;
            constexpr uptr DelayReq10 = 0x00957B7B;
            constexpr uptr DelayReq11 = 0x009581AA;
            constexpr uptr DelayReq12 = 0x009586CA;
            constexpr uptr DelayReq13 = 0x009586E9;
            constexpr uptr DelayReq14 = 0x0095888C;
            constexpr uptr DelayReq15 = 0x00958A13;
            constexpr uptr DelayReq16 = 0x00958A2B;
            constexpr uptr DelayReq17 = 0x00958FFC;
            constexpr uptr DelayReq18 = 0x0095932D;
            constexpr uptr DelayReq19 = 0x00959345;
            constexpr uptr DelayReq20 = 0x0095B55B;
            constexpr uptr DelayReq21 = 0x0095B57D;

            constexpr uptr DelayAnims1 = 0x005CF081;
            constexpr uptr DelayAnims2 = 0x005CF136;
            constexpr uptr DelayAnims3 = 0x009AF2C1;
            constexpr uptr DelayAnims4 = 0x009AF2F2;
            constexpr uptr DelayAnims5 = 0x009AF4AC;
            constexpr uptr DelayAnims6 = 0x00A19C04;
            constexpr uptr DelayAnims7 = 0x00A1D2CC;
            constexpr uptr DelayAnims8 = 0x00A21655;
            constexpr uptr DelayAnims9 = 0x00A21D99;
            constexpr uptr DelayAnims10 = 0x00A21EDA;
            constexpr uptr DelayAnims11 = 0x00A223C5;
            constexpr uptr DelayAnims12 = 0x00A55AE6;
            constexpr uptr DelayAnims13 = 0x00B20067;
            constexpr uptr DelayAnims14 = 0x00781CF3;
            constexpr uptr DelayAnims15 = 0x00A1F161;
            constexpr uptr DelayAnims16 = 0x00A221CE;
            constexpr uptr DelayAnims17 = 0x00A223B2;
            constexpr uptr DelayAnims18 = 0x00A22FD9;

            constexpr uptr MinDistance1 = 0x009007C1;
            constexpr uptr MinDistance2 = 0x009577F0;
            constexpr uptr MinDistance3 = 0x00957B4C;
            constexpr uptr MinDistance4 = 0x009586B0;
            constexpr uptr MinDistance5 = 0x009589FC;
            constexpr uptr MinDistance6 = 0x00959316;
            constexpr uptr MinDistance7 = 0x0095B541;

            constexpr uptr Rot1_Patch = 0x00B207A7;
            constexpr uptr Rot2_Patch = 0x00B20813;
            constexpr uptr Rot3_Patch = 0x00B2088A;

        // sub_A54150: CPlayerModelProperty rotation/lean update. The A/D strafe LEAN is
        // model+0x444 (current, what MovementSend transmits), interpolated toward target
        // model+0x448 and folded into the body Z-rotation; model+0x44C is the aim-yaw
        // lean compensation. Detoured to zero those at the source (see custom_tickrate).
        constexpr uptr ModelRotLeanUpdate = 0x00A54150;
        constexpr u32  ModelLeanCur       = 0x444;   // current strafe lean (sent by MovementSend)
        constexpr u32  ModelLeanTarget    = 0x448;   // interpolation target lean
        constexpr u32  ModelAimComp       = 0x44C;   // aim-yaw lean compensation

        // ── Consolidated arrays and constants for refactored API ──────────
        // Single-address aliases (patch just the first in group — extend if needed)
        constexpr uptr FrameTime         = Frametime1;
        constexpr uptr RotationDamping   = RotDamp1;
        constexpr uptr MinRotationSpeed  = MinRotSpeed1;
        constexpr uptr MaxRotationSpeed  = MaxRotSpeed1;
        constexpr uptr RotationThreshold = RotThreshold1;

        // Arrays

        constexpr uptr RotationThresholds[] = {
            RotThreshold1, RotThreshold2, RotThreshold4,  // RotThreshold3 is "don't use"
            RotThreshold5, RotThreshold6, RotThreshold7
		};

		constexpr u32 kNumRotationThresholds = 6;

        constexpr uptr DelayRequests[] = {
            DelayReq1,  DelayReq2,  DelayReq3,  DelayReq4,  DelayReq5,
            DelayReq6,  DelayReq7,  DelayReq8,  DelayReq9,  DelayReq10,
            DelayReq11, DelayReq12, DelayReq13, DelayReq14, DelayReq15,
            DelayReq16, DelayReq17, DelayReq18, DelayReq19, DelayReq20,
            DelayReq21
        };

        constexpr u32 kNumDelayRequests = 21;

        constexpr uptr DelayAnimations[] = {
            DelayAnims1,  DelayAnims2,  DelayAnims3,  DelayAnims4,  DelayAnims5,
            DelayAnims6,  DelayAnims7,  DelayAnims8,  DelayAnims9,  DelayAnims10,
            DelayAnims11, DelayAnims12, DelayAnims13, DelayAnims14, DelayAnims15,
            DelayAnims16, DelayAnims17, DelayAnims18
        };
        constexpr u32 kNumDelayAnimations = 18;

        constexpr uptr MinDistances[] = {
            MinDistance1, MinDistance2, MinDistance3, MinDistance4,
            MinDistance5, MinDistance6, MinDistance7
        };
        constexpr u32 kNumMinDistances = 7;

        constexpr uptr RotationByteAddresses[] = {
            Rot1_Patch, Rot2_Patch, Rot3_Patch
        };
        constexpr u32 kNumRotationBytePatches = 3;

        // Multi-site rotation/frametime groups. The old CustomTickratePatch patched ALL of
        // these; the refactor had collapsed each to a single-address alias ("patch just the
        // first in group — extend if needed"), leaving the other sites at vanilla 10-tick
        // values — the cause of the rotation snap that survived every value tweak.
        constexpr uptr FrameTimes[] = { Frametime1, Frametime3, Frametime4 }; // Frametime2 unused (old)
        constexpr u32  kNumFrameTimes = 3;

        constexpr uptr RotationDampings[] = { RotDamp1, RotDamp2, RotDamp3 }; // default 0.1 -> 1.0
        constexpr u32  kNumRotationDampings = 3;

        constexpr uptr MinRotationSpeeds[] = {
            MinRotSpeed1, MinRotSpeed2, MinRotSpeed3, MinRotSpeed4, MinRotSpeed5, MinRotSpeed6
        };
        constexpr u32  kNumMinRotationSpeeds = 6;

        constexpr uptr MaxRotationSpeeds[] = {
            MaxRotSpeed1, MaxRotSpeed2, MaxRotSpeed3, MaxRotSpeed4, MaxRotSpeed5, MaxRotSpeed6
        };
        constexpr u32  kNumMaxRotationSpeeds = 6;

        // Patch values (from original CustomTickrate globals)
        inline const double  kFrameTimeValue             = 1.0;
        inline const double  kRotationDampingValue        = 1.0;
        inline const float   kMinRotationSpeedValue       = 0.2f;   // original (no snap)
        inline const float   kMaxRotationSpeedValue       = 5.0f;
        inline const float   kRotationThresholdValue      = 1.570796371f; // 90° (moot while snapping)
        inline const float   kDelayRequestValue           = 1.0f / 128.0f; // hardcoded_tickrate = 1/room_tickrate
        // The game constant at all 18 sites is 0.08 (flt_10149F8 as a 32-bit float, dbl_1030C38
        // as a 64-bit double). It is OVERLOADED across two unrelated jobs (verified in IDA):
        //   • DelayAnimations[0..4] = the ANIMATION cross-fade duration (sub_5CF020, sub_9AF270,
        //     sub_9AF380 — the 8-way directional run blender; 0.08 is the blend time fed to
        //     sub_A4C840). It MUST stay ~0.08 or transitions snap — e.g. running backward +
        //     strafe A/D "doesn't blend / same animation". So we DON'T patch these (leave
        //     vanilla 0.08). It's local render timing, independent of server tick.
        //   • DelayAnimations[5..17] = the dead-reckoning velocity window (displacement/window,
        //     sub_A21600 / sub_A1F100, …). For a 128-tick server that's ~1/128.
        // (NOT the yaw/pitch math. The old 0.8/128 was a guess: 0.08 read as 0.8/10, re-scaled.)
        //
        // CRITICAL width split among the dead-reckoning ones: [5..12] read a 32-bit float (fld
        // dword), [13..17] read a 64-bit double (fcomp/fdiv qword). Patching a qword site with a
        // 4-byte float makes it read 8 bytes = a garbage double (corrupted dead-reckoning/landing),
        // so patch each with the matching-width copy.
        inline const float   kDelayAnimationValue         = 0.8f / 128.0f;  // ORIGINAL single value (used by CustomTickrate)
        inline const float   kDelayAnimationValueF        = 1.0f / 128.0f;  // dead-reckoning, `fld dword` sites (unused after revert)
        inline const double  kDelayAnimationValueD        = 0.8  / 128.0;   // dead-reckoning, `fcomp/fdiv qword` sites
        constexpr u32        kNumDelayAnimBlend           = 5;              // [0..4] = anim blend -> leave VANILLA (0.08)
        constexpr u32        kNumDelayAnimationsFloat     = 13;             // dead-reckoning: [5..12] dword, [13..17] qword
        inline const double  kMinDistanceValue            = 10.0;             // MinDistance1 only (old: minimum_distance)
        inline const double  kMinDistanceValue2           = 0.009999999776482582; // MinDistance2..7 (old: minimum_distance2)
        // Rotation byte patch: FLDZ + NOP padding (replaces FLD instruction)
        inline constexpr u8 kRotationByteValue[] = { 0xD9, 0xEE, 0x90, 0x90, 0x90, 0x90 };
    }

    namespace resolutions {
        constexpr uptr SetAspectRatioScale = 0x00E5C370;

        constexpr uptr WidthBuff[20] = {
            0x0040CE4F, 0x0040CE92, 0x0044D016, 0x0044D03A, 0x0044D0A7,
            0x0044D0CB, 0x0044D266, 0x0044D28A, 0x0044D302, 0x0044D326,
            0x0044D49B, 0x0044D4BF, 0x0044D539, 0x0044D55D, 0x00651DCC,
            0x00651DF2, 0x00652251, 0x0065227B, 0x006522C8, 0x006522F4
        };
        constexpr uptr HeightBuff[19] = {
            0x0040CE69, 0x0040CEBA, 0x0044D006, 0x0044D04F, 0x0044D097,
            0x0044D0DF, 0x0044D256, 0x0044D29F, 0x0044D2F2, 0x0044D33A,
            0x0044D48B, 0x0044D4D4, 0x0044D529, 0x0044D572, 0x00651DBC,
            0x00651E09, 0x0065223E, 0x00652295, 0x006522B5
        };
        constexpr uptr AspectRatioIds[15] = {
            0x0044CFC9, 0x0044CFDB, 0x0044CFED, 0x0044D06D, 0x0044D07F,
            0x0044D20B, 0x0044D21D, 0x0044D22F, 0x0044D2BD, 0x0044D2CF,
            0x0044D446, 0x0044D458, 0x0044D46A, 0x0044D4F3, 0x0044D505
        };
        constexpr uptr ResListSize[6] = {
            0x0065221E, 0x0040CE36, 0x0044CF98, 0x0044D1DA, 0x0044D415, 0x0044D238
        };

        // ── Aliases for refactored API ───────────────────────────────────
        constexpr auto& WidthBuffers        = WidthBuff;
        constexpr auto& HeightBuffers       = HeightBuff;
        constexpr auto& AspectRatioIdBuffers = AspectRatioIds;
        constexpr auto& ResolutionListSizes = ResListSize;
        constexpr u32 kNumWidthBuffers         = 20;
        constexpr u32 kNumHeightBuffers        = 19;
        constexpr u32 kNumAspectRatioIdBuffers = 15;
        constexpr u32 kNumResolutionListSizes  = 6;
        constexpr uptr SetAspectRatioScaling   = SetAspectRatioScale;
    }
}

// ── Bugfixes ──────────────────────────────────────────────────────────────────

namespace grade_threshold {
    constexpr uptr RoomCreate_Cmp      = 0x00B0D771;
    constexpr uptr PartyCreate_Cmp     = 0x00B16FD4;
    constexpr uptr AloneMode_Cmp       = 0x00B07C66;
    constexpr uptr MatchGmBypass1_Cmp  = 0x00660C4F;
    constexpr uptr MatchGmBypass2_Cmp  = 0x006CD356;
    constexpr uptr MatchValidation_Cmp = 0x00B073AB;
    constexpr u8   kNewThreshold       = 0x0C;
}

namespace bugfixes {
    constexpr uptr RoomCreateDialogHandler   = 0x006918C0;
    constexpr uptr RoomSettingsDialogHandler = 0x0066B3F0;
    constexpr uptr RoomMainDialogHandler     = 0x00660280;
    constexpr uptr ScreenshotBug1            = 0x004618F0;
    constexpr uptr GameHWND                  = 0x011C898C;
    constexpr uptr ScreenshotIncrementer     = 0x011D9BB8;
    constexpr uptr InterlockedScreenshot     = 0x011C6898;
    constexpr uptr TakeScreenshot            = 0x004615C0;
    constexpr uptr DisconnectReloginState    = 0x00AC3212;
    constexpr uptr SetDateTimeShit           = 0x00AC83BE;
    constexpr uptr CpuidInit                 = 0x00D7C0E0;
    constexpr uptr WndProc                   = 0x0045F670;
}

// ── Engine Log ────────────────────────────────────────────────────────────────

namespace engine_log {
    constexpr uptr SysLogInstance  = 0x011F1060;   // Syslog.txt
    constexpr uptr ErrLogInstance  = 0x011F1064;   // ErrLog.txt
    constexpr uptr LogFile         = 0x00DF5DF0;   // void __cdecl LogFile(int inst, char* fmt, ...)
    constexpr uptr GameLogInstance = 0x011D6098;   // GameLog.txt
    constexpr uptr GameLogger      = 0x004473B0;   // void __cdecl GameLogger(int inst, int a2, int a3, int level, char* fmt, ...)
}

// ── UI ────────────────────────────────────────────────────────────────────────

namespace ui {
    namespace sound {
		constexpr uptr PlayUISound = 0x004F9990;   // void __stdcall (const char* sound_file, bool idk, bool idk)
    }
	namespace rightclick {
		constexpr uptr HandleIds         = 0x006B0260;   // void __fastcall (u32 _this, u32 edx, u32 type, u32 dialogId, u32 dialogData)
		constexpr uptr SomeValue         = 0x015F2F4C;   // DATA: global DWORD reset to 0 on right-click
		constexpr uptr GetSlotItem       = 0x00E94130;   // sub_EAC530: int __cdecl () — returns item ptr from slot
		constexpr uptr HandleSpecialItem = 0x006B1E60;   // sub_699580: void __cdecl (u32 _this)
		constexpr uptr UpdateRightClick  = 0x006B3AC0;   // sub_69B6F0: void __cdecl (u32 _this)
		constexpr uptr ProcessRightClick = 0x006B6360;   // sub_69E020: void __cdecl (u32 _this)
		constexpr uptr QuickResellItem   = 0x006BBB60;   // sub_6A4DE0: void __cdecl (u32* dlg, int mode)
		constexpr uptr QuickDeleteItem   = 0x006AE980;   // sub_695BF0: void __cdecl (u32 _this)
		constexpr uptr SellFlag          = 0x011C7CB6;   // DATA: byte_11A3D5A — global sell-allowed flag
		constexpr uptr HandleInput       = 0x00E939E0;   // void __thiscall sub_E939E0(int this, int type, POINT pt, int a4, int a5)
		constexpr uptr ActiveSlotPtr     = 0x015F2F50;   // DATA: dword_15F2F50 — global ptr to active slot object
		constexpr uptr SendSlotNotify    = 0x00E74FC0;   // sub_E74FC0: void __cdecl (u32 target, u32 msg, u32 param, u32 sender)
		constexpr uptr GetSlotHandler    = 0x00E4F010;   // sub_E4F010: u32 __cdecl () — returns slot handler instance
		constexpr uptr GetSlotFromInput  = 0x00E8C920;   // sub_E8C920: u32 __thiscall (void* this, int index)
		constexpr uptr ReleaseSlotCapture = 0x00E76480;  // sub_E76480: void __cdecl (u32 captureTarget, u32 sender)
		constexpr uptr GetSlotCount       = 0x00E63210;  // sub_E63210: u32 __cdecl (u32* container)
		constexpr uptr GetSlotAt          = 0x007A90A0;  // sub_7A90A0: u32* __thiscall (u32* this, u32 index)
		constexpr uptr SetSlotCapture     = 0x00E763C0;  // sub_E763C0: void __cdecl (u32 captureTarget, u32 param)
		constexpr uptr ScrollHandler      = 0x00E966F0;  // sub_E966F0: char __thiscall (void* this, int a2)
	}
	namespace quickconfirm {
		constexpr uptr ConfirmBlockFlag            = 0x015F2E08;   // dword_15F2E08: global flag blocking spacebar/enter confirm
		constexpr uptr CDlgGachaResult_VTable7     = 0x010211F0;   // CDlgGachaResult vtable[7] — sub_E75020 to swap
		constexpr uptr CDlgPackageItem_VTable7     = 0x01021498;   // CDlgPackageItem vtable[7] — sub_E75020 to swap
		constexpr uptr CDlgCapsuleItem_VTable7     = 0x01021508;   // CDlgCapsuleItem vtable[7] — sub_E75020 to swap
		constexpr uptr CDlgRepair_VTable7          = 0x01021CE8;   // CDlgRepair vtable[7] — sub_E75020 to swap
		constexpr uptr CDlgBaseMsgBox_VTable7      = 0x01027270;   // CDlgBaseMsgBox vtable[7] — sub_E75020 to swap
		constexpr uptr PlaySoundOnControl          = 0x00E8C8F0;   // sub_E8C8F0: void __thiscall (char* this, int a2, int wavStr)
		constexpr uptr RepairFinish                = 0x0066FB60;   // sub_66FB60: void __cdecl (void* this) — finishes repair dialog
		constexpr uptr BaseMsgBoxFindControl       = 0x005E87A0;   // sub_5E87A0: int* __thiscall (int**, int*) — CDlgBaseMsgBox control lookup
		constexpr uptr DialogTabCycle              = 0x00E766E0;   // sub_E766E0: void __thiscall (int this) — dialog tab cycle, needs ConfirmBlockFlag clear fix
	}
    namespace input_popup {
        constexpr uptr InitTitles              = 0x006554B0;
        constexpr uptr DlgInputHandle           = 0x006556B0;
        constexpr uptr ValidatePassword         = 0x006555F0;   // char __stdcall ValidateAndSanitizeRoomPassword(char*)
        constexpr uptr CloseInputPopup          = 0x00656190;   // char __thiscall sub_656190(int this)
        constexpr uptr GetUIEditBoxString       = 0x00E66C40;   // char* __thiscall (CHAR* subDialogInfo)
        constexpr uptr GetUIEditBoxWide         = 0x00E698F0;   // _BYTE* __thiscall (_BYTE* this)
        constexpr uptr DialogAction             = 0x00E622A0;   // char __thiscall sub_E622A0(_DWORD* this, char* Str2)
        constexpr uptr SetPasswordCallback      = 0x005CC480;   // void __thiscall (char* dest, char* src) strncpy_s pw
        constexpr uptr ChattingBoxBadwordCheck  = 0x0058C0E0;   // _DWORD __thiscall (int this, int wstr)
        constexpr uptr IdkPopup                 = 0x006562F0;
    }

    namespace state_mgr {
        constexpr uptr Instance                 = 0x011E6D30;   // CStateMgr singleton
        constexpr uptr GetState                 = 0x00B8F1D0;   // return *(this + 4*a2 + 0x18)
        constexpr uptr SetTransition            = 0x00B8F020;   // void __thiscall (int this, GameState)
        constexpr uptr PrevGameStatePtr         = 0x011E6D44;   // n21 / dword_11E6D44 — global holding the previous CState* (orig sub_6556B0 passes it straight to _RTDynamicCast)
        constexpr uptr CurrGameState            = 0x011E6D38;   // g_CurrGameState global
    }

    namespace room_join {
        constexpr uptr JoinRoom                 = 0x00B0E760;   // char __cdecl (i16 room, i16 channel, int n36, char* pw, char a5)
        constexpr uptr PartyClanOtherJoin       = 0x00B182E0;   // char __cdecl (int partyIdChannelId, char* pass)
        constexpr uptr RoomRuleModPassUpdate    = 0x00B13050;   // char __cdecl (u32, u32, char*)
        constexpr uptr RoomRuleMaxPlayerUpdate  = 0x00B12610;   // char __cdecl (u32)
        constexpr uptr GetCServerList           = 0x00473A90;   // int __cdecl ()
        constexpr uptr StateRoomJoinSetServer   = 0x00B951D0;   // _DWORD* __thiscall (int this, int)
        constexpr uptr StateRoomJoinJoin        = 0x00B965E0;   // int __thiscall (int this, int, int, char*, char*, int, int)
        constexpr uptr CState_RTTI              = 0x01197BC8;
        constexpr uptr CStateRoomJoin_RTTI      = 0x01197BE0;
        constexpr uptr RTDynamicCast            = 0x00F19972;   // int __cdecl (int, int, int, int, int)
    }
    namespace gui_manager {
        constexpr uptr Get = 0x011DE330;
        constexpr uptr GetDialogInfo    = 0x00E4E660;
        constexpr uptr RegisterDialog   = 0x00E4E570;   // char __thiscall(void*, const char* name, u32 dialogObj)
        constexpr uptr DialogBaseCtor   = 0x00E73E90;   // float* __thiscall(float* this) — UI_Dialog base ctor
    }
    namespace ui_msgbox {
        constexpr uptr Get = 0x011DE354;
        constexpr uptr CloseDialog = 0x00E62950;   // char __thiscall(CUIMsgBox*, const char* name) — searches +272/+296 containers
        constexpr uptr CloseByName = 0x00E622A0;   // char __thiscall(CUIMsgBox*, const char* name) — searches the +62 active-dialog list (what closes E_DLG_OPTION). Hides + removes from active; object stays registered so it can re-open.
    }
    namespace ui_msgadapter {
        constexpr uptr Get = 0x011DECAC;
    }
    namespace userlist {
        // sub_6769F0: DlgUserListPopupMenu::OnEvent — void __thiscall(this, evtType, ctrlId, a4)
        constexpr uptr PopupOnEvent     = 0x006769F0;
        // sub_B986A0: the *universal* player-list right-click opener. It builds
        // E_DLG_LISTINFO and dispatches to one of three context populates —
        // sub_677720 (channel/stranger), sub_678250 (friend), sub_677B40 (clan) —
        // then shows the dialog. Hooking it (not a single populate) is the only way
        // to reveal Trade in every context (the Friend menu uses sub_678250, NOT
        // sub_677720). char __thiscall(void* this, u8 atCursor).
        constexpr uptr UserListOpen     = 0x00B986A0;
        // sub_677720: E_DLG_LISTINFO channel-context populate (kept for reference).
        constexpr uptr PopulateListInfo = 0x00677720;
        constexpr u32  NameField        = 0x80C;   // popup+2060 = selected user's name (ASCII, 16)
        constexpr int  TradeMenuId      = 109020;  // E_DLG_LISTINFO_BTN_TRADE (added to SCENE_COMMON.xml; 109017-109019 already used by PIC_BACK/PIC_USERNAME_BACK/STC_USERNAME)
        constexpr int  BlockMenuId      = 109004;  // E_DLG_LISTINFO_BTN_BLOCK — the native populate positions it last in every context; Trade anchors one row below it
    }
    namespace trade {
        // E_DLG_TRADE control wiring (ToyBattles CDlgTrade reimplemented on the
        // native XML dialog). All slot ops reuse the verified rightclick slot fns.
        constexpr u32  CtrlContainerOff = 0x6B0;   // dialog+0x6B0 = control list (FindControl arg)
        constexpr u32  SlotVectorOff    = 0x9A8;   // slotControl+0x9A8 = slot std::vector (tb +2472) [VERIFY in-game]
        constexpr u32  SlotItemOff      = 0x20;    // slot+0x20 = item ptr (0 = empty)
        constexpr u32  SlotFlagOff      = 0x9;     // slot+0x9  = occupied flag
        constexpr u32  SlotIdOff        = 0x4;     // slot+0x4  = aux id
        constexpr u32  ItemSerialLo     = 44;      // item+44 = ItemSerialInfo low dword
        constexpr u32  ItemSerialHi     = 48;      // item+48 = ItemSerialInfo high dword
        // E_DLG_TRADE control ids (from SCENE_INVEN_2ND.xml dialog id 110000)
        constexpr int  IdBtnCancel  = 110001, IdBtnConfirm = 110002, IdBtnLock = 110003;
        constexpr int  IdSlotMe     = 110012, IdSlotYou    = 110013;
        constexpr int  IdNameMe     = 110014, IdNameYou    = 110015;
        // tb sub_B99930 = CItemFactory create-from-itemId. Needed only to render the
        // PARTNER's offered items in SLT_TRADE_YOU; my own items use the live drag
        // object directly. 0 = not yet located in MegaVolts -> YOU render skipped.
        constexpr uptr ItemFactory  = 0x00000000;  // TODO(RE): CItemFactory::Create(itemId)
    }
    namespace gacha {
        constexpr uptr GachaBack             = 0x0063E3E0;   // sub_63E3E0: void __thiscall (int this) — gacha back button
        constexpr uptr GachaNext             = 0x0063E4B0;   // sub_63E4B0: void __thiscall (int this) — gacha next button
        constexpr uptr GachaSelect           = 0x0063C1D0;   // sub_63C1D0: void __thiscall (int this) — gacha list select / currency swap
        constexpr uptr UpdateGachaLucky      = 0x0063B440;   // sub_640EC0: void __thiscall (int* this) — updates gacha lucky meter UI
        constexpr uptr GachaLuckyPointsPatch = 0x0063C90C;   // midhook: mov eax,[ebp-58h]; mov [eax+0Ch],edx — override lucky points per type
    }
    namespace custom_packets {
        constexpr uptr PacketsCallbackDistribution = 0x00AB5340; // char __stdcall PacketsCallbackDistribution(_WORD*)
        constexpr uptr NetFrameTick                = 0x00AB40A0; // int __thiscall(this) — per-frame net/scene update (runs every frame)
    }

    constexpr uptr IMsgDataInit = 0x00E5A130;
    constexpr uptr CMsgDataLoginBanUser_vftbl = 0x010735F4; // CMsgDataLoginBanUser vftable
    constexpr uptr CMsgDataLoginAuthorize_vftbl  = 0x010213B8; // CMsgDataLoginAuthorize vftable
    constexpr uptr DisconnectOfflinePrepare  = 0x00AB5AB0;   // void __thiscall(CNetMgr*, int)
    constexpr uptr CloseAllMessages           = 0x00E62E70;   // sub_E62E70: pushes CMsgCloseAll into CUIMsgBox
    constexpr uptr DisconnectOfflineFinalize = 0x00AC2BB0;   // void __stdcall()
    constexpr uptr AuthorizeDisconnectAll     = 0x00AB5F60;   // void __thiscall AuthorizeDisconnectAll(CNetMgr*, const char*, u32, u32)
    constexpr uptr FindDialog                 = 0x00E61540;   // void* __thiscall CUIMsgBox::FindDialog(const char* name)
    constexpr uptr GetDlgInfo                 = 0x008B4600;
    constexpr uptr GetUIMsgText               = 0x005C48A0;
    constexpr uptr FormatStringSafe           = 0x0048B250;   // int __cdecl (char* Buffer, char* Format, ...)
}

namespace editbox {
    constexpr uptr KeyHandler       = 0x00E67970;   // char __thiscall(UI_EditBox*, int msg, int key, int param) — vtable[13]
    constexpr uptr CopyToClipboard  = 0x00E670B0;   // void __thiscall(UI_EditBox*)
    constexpr uptr PasteFromClip    = 0x00E67210;   // int __thiscall(UI_EditBox*)
    constexpr uptr DeleteSelection  = 0x00E66F40;   // int __thiscall(UI_EditBox*)
    constexpr uptr SetCaretPos      = 0x00E66A10;   // int __thiscall(UI_EditBox*, int pos)
    constexpr uptr GetTextW         = 0x00E698F0;   // u8* __thiscall(UI_EditBox*) — returns wstring ptr
    constexpr uptr RefreshDisplay   = 0x00E69550;   // void __thiscall(UI_EditBox*)
    constexpr uptr NotifyParent     = 0x00E74FC0;   // int __thiscall(void* parent, int msg, u8 flag, u32* sender)
    constexpr u32  ParentPtrOff     = 0x5F4;        // offset in editbox struct to parent ptr
    constexpr u32  NotifyTextChange = 0x602;         // message ID for text changed notification (1538)
    constexpr u32  SelectionAnchor  = 0x83C;         // offset in editbox struct for selection anchor
    constexpr u32  WmKeyDown        = 0x100;         // WM_KEYDOWN
}

namespace msvc {
    constexpr uptr operator_new       = 0x00F1988C;   // ??2@YAPAXI@Z
    constexpr uptr operator_delete    = 0x00F19620;   // ??3@YAXPAX@Z
    constexpr uptr operator_new_array  = 0x00EB65B0;   // ??_U@YAPAXI@Z
    constexpr uptr operator_delete_array = 0x00F1996C; // ??_V@YAXPAX@Z
    constexpr uptr WStringCtor  = 0x00F8B47C;   // IAT entry: int __thiscall std::wstring::wstring(_DWORD, _DWORD) — dereference before calling
    constexpr uptr WStringDtor  = 0x00F8B474;   // IAT entry: void __thiscall std::wstring::~wstring(void*) — dereference before calling
    constexpr uptr PtInRect     = 0x00F8B910;   // IAT entry: BOOL __stdcall PtInRect(const RECT*, POINT) — dereference before calling
}


// ── Anticheat ─────────────────────────────────────────────────────────────────

namespace anticheat {
    namespace ack_handlers {
        constexpr uptr Disconnect = 0x00AC2BF0;
        constexpr uptr NetworkInitCrypto = 0x00AC7840;
		constexpr uptr FrontAuthorize = 0x00AD6A60;
    }
    namespace req_handlers {
        constexpr uptr MainAuthorize = 0x00AF98F0;
    }
    namespace cdbm {
        constexpr uptr Load = 0x0042F070;
    }
    namespace heartbeat {
        constexpr uptr Init = 0x00AC8D90;
    }
    
    namespace crypto {
        namespace rc5 {
            constexpr uptr KeySetup         = 0x00DF5050;  // hook target
            constexpr uptr InternalKeySetup = 0x00DF4960;  // called inside the hook
            constexpr u8   K[] = {
                0xc0, 0x58, 0x1e, 0x07, 0xd9, 0x39, 0x43, 0x12,
                0x31, 0xd0, 0xce, 0x21, 0xdd, 0xaf, 0x90, 0xad
            };
        }
        namespace rc6 {
            constexpr uptr KeySetup         = 0x00DF5370;  // hook target
            constexpr uptr InternalKeySetup = 0x00DF4EF0;  // called inside the hook
            constexpr u8   K[] = { 
                0x55, 0x35, 0x34, 0xb1, 0x9f, 0x85, 0x46, 0x23, 
                0x46, 0x08, 0xb4, 0x75, 0xc3, 0xd4, 0x9e, 0x9c, 
                0x66, 0x0d, 0xab, 0x76, 0x74, 0xe7, 0x74, 0xf1, 
                0x35, 0x4b, 0x53, 0xc7, 0x4d, 0xe6, 0x69, 0xfe 
            };
        }
        namespace cgd {
            constexpr uptr static_pw1 = 0x00FE3040;
            constexpr uptr static_pw2 = 0x0107C338;
        }
        constexpr uptr archiveloader_pw  = 0x011C8A54;
        constexpr uptr Encrypt           = 0x00DF4940;
        constexpr uptr Decrypt           = 0x00DF4950;
        constexpr uptr DecryptReturnAddr = 0x00E13500;
    }

    namespace game_managers {
        namespace room {
            constexpr uptr Get     = 0x004739C0;
            constexpr uptr Destroy = 0x0051EBC0;
            constexpr uptr Init    = 0x00548CA0;
            constexpr u32  AllocSize = 552;
        }
        namespace unit_container {
            constexpr uptr Get     = 0x004728D0;
            constexpr uptr Destroy = 0x0051EA60;
            constexpr uptr Init    = 0x0090E800;
            constexpr u32  AllocSize = 116;
        }
        namespace unit_mgr {
            constexpr uptr Get     = 0x00472800;
            constexpr uptr Destroy = 0x0051F160;
            constexpr uptr Init    = 0x0091D7D0;
            constexpr u32  AllocSize = 188;
        }
        namespace net_mgr {
            constexpr uptr Get     = 0x00473D20;
            constexpr uptr Destroy = 0x0051EEC0;
            constexpr uptr Init    = 0x00AB2E60;
            constexpr u32  AllocSize = 0x1518; // original 0x14F8 + 0x20 for auth_token[32] at offset 0x14F8
			constexpr uptr NetworkThreadIdk = 0x00AB3EF0;
        }
        namespace dynamics {
            constexpr uptr Get     = 0x00472730;
            constexpr uptr Destroy = 0x0051E9B0;
            constexpr uptr Init    = 0x007AA850;
            constexpr u32  AllocSize = 1760;
        }
    }
    namespace option_ddd {
        constexpr uptr TutorialSkip = 0x011C7E1C;

    }
}

// ── NationIndex config ────────────────────────────────────────────────────────

namespace config {
    inline bool NationIndexIsRom = false;
}

// ── Network engine (1:1 reimplementation targets) ─────────────────────────────
// Addresses of the original client net-engine functions we reimplement in
// game/network/engine/. Hooks that swap them in are written but left commented
// out (see each *_install()). Ground truth: MicroVolts.exe.i64 (103 client).
namespace net {
    // CTcpSocket : CRawSocket  (libnetengine/src/TcpSocket.cpp)
    namespace tcp_socket {
        constexpr uptr Ctor           = 0x00E19180;   // CTcpSocket::CTcpSocket
        constexpr uptr Dtor           = 0x00E19200;   // CTcpSocket::~CTcpSocket
        constexpr uptr ScalarDelDtor  = 0x00E19870;   // `scalar deleting destructor'
        constexpr uptr CreateSocket   = 0x00E192D0;   // int  __thiscall()
        constexpr uptr ClearSendQueue = 0x00E19270;   // void __thiscall()
        constexpr uptr Listen         = 0x00E19370;   // bool __thiscall(addr, port)
        constexpr uptr Accept         = 0x00E195C0;   // SOCKET __thiscall()
        constexpr uptr SendBlock      = 0x00E195E0;   // SOCKET __thiscall(void* src, size_t len)
        constexpr uptr SendTrust      = 0x00E196A0;   // int  __thiscall(char* buf, int len)
        constexpr uptr SendQueue      = 0x00E19890;   // bool __thiscall()
        constexpr uptr Send           = 0x00E1A430;   // int  __thiscall(char* buf, int len)
        constexpr uptr Read           = 0x00E1A7D0;   // int  __thiscall(char* buf, int len)
    }
    // CConnector / CTcpConnector  (libnetengine/src/ConnectorTcp.cpp)
    namespace connector {
        constexpr uptr Send         = 0x00E13430;   // int  __thiscall(cmd, size, arg8, a4)
        constexpr uptr SendSafty    = 0x00E12C20;   // int  __thiscall(CCommand*, a3, a4)
        constexpr uptr SendObsolete = 0x00E12ED0;   // int  __thiscall(CCommand*, a3, a4)
        constexpr uptr Read         = 0x00E13470;   // char __thiscall()  (recv + dispatch)
        constexpr uptr KeepAlive    = 0x00E13990;   // bool __thiscall()
        constexpr uptr Connect      = 0x00E13340;   // char __thiscall(addr, port)
        constexpr uptr SetConnected = 0x00E13240;   // u64  __thiscall()
        constexpr uptr Encrypt      = 0x00E10FE0;   // void __thiscall(a2, in, out, size)
        constexpr uptr Decrypt      = 0x00E11170;   // void __thiscall(a2, in, out, size)
        constexpr uptr Queuing      = 0x00E13940;   // void __thiscall(arg)
        constexpr uptr Ctor          = 0x00E13160;  // CTcpConnector::CTcpConnector (base) — alloc socket + 0xC0000 recv buf
        constexpr uptr BaseCtor      = 0x00E10F30;  // CConnector::CConnector (sub_E10F30)
        constexpr uptr Clear         = 0x00E12B00;  // CTcpConnector::Clear (vtbl Clear target)
        constexpr uptr BaseDisconnect = 0x00E12BA0; // sub_E12BA0 — net socket teardown (defer on in-flight)
        constexpr uptr ConnectorTcpVft = 0x010F21DC;// CConnectorTcp vftable (base, set by ctor)
        constexpr u32  TcpSocketSize  = 0xA8;       // operator new size for CTcpSocket
        constexpr u32  RecvBufSize    = 0xC0000;    // 768KB recv stream buffer
    }
    // Leaf helpers the socket layer calls (kept as direct calls into the client).
    namespace helpers {
        constexpr uptr RawSetSocketOption  = 0x00E13C10; // CRawSocket::SetSocketOption(int,int)
        constexpr uptr RawSetLocalAddress  = 0x00E13CB0; // CRawSocket::SetLocalAddress(char*,int)
        constexpr uptr RawSetRemoteAddress = 0x00E13D40; // CRawSocket::SetRemoteAddress(char*,int)
        constexpr uptr BlockQueuePutStream = 0x00E1D0E0; // CBlockQueue::PutStream(void*,u32)
        constexpr uptr BlockQueueClear     = 0x00E1CF40; // CBlockQueue::Clear()
        constexpr uptr BlockQueuePeek      = 0x00E1D040; // CBlockQueue::Peek() -> CTcpPacketBuffer*
        constexpr uptr BlockQueuePop       = 0x00E1CFC0; // CBlockQueue::Pop()
        constexpr uptr StreamBufferMove    = 0x00E19120; // CStreamBuffer::Move(size) -> new m_stSize
        constexpr uptr StreamBufferClear   = 0x00E190F0; // CStreamBuffer::Clear()
        constexpr uptr StreamBufferCtor    = 0x00E190C0; // CStreamBuffer::CStreamBuffer
        constexpr uptr StreamBufferSetBuffer = 0x00E190D0; // CStreamBuffer::SetBuffer(buf, size)
        constexpr uptr TickGetInstance     = 0x004D5C80; // CTick::GetInstance()
        constexpr uptr TickGetMSec         = 0x00DF8F20; // CTick::GetMSec()
        constexpr uptr TickGetTick         = 0x00DF8F00; // CTick::GetTick()
        // ── Command-queue leaf helpers (MSVC std::deque ring + object pools) ──
        // Called into the client rather than reimplemented (same convention as the
        // CBlockQueue/CStreamBuffer leaf helpers above).
        constexpr uptr DequeSubscript      = 0x00E1CC80; // std::deque<Cmd*>::operator[](map,idx) -> Cmd**
        constexpr uptr DequePushBack        = 0x00E18DF0; // std::deque<Cmd*>::push_back(deque, &Cmd*)
        constexpr uptr ExtendPoolAlloc      = 0x00E11710; // CExtendCommand pool alloc(0x5B8) -> raw
        constexpr uptr ExtendPoolInit       = 0x00E117A0; // CExtendCommand pool init(raw) -> CExtendCommand*
        constexpr uptr NativePoolAlloc      = 0x00E18800; // CBasicCommand pool alloc(0x5A0) -> raw
    }

    // ── ICommandQueue / CExCommandQueue (Extend) + CCommandQueue (Native) ──────
    // libnetengine/src/ExCommandQueue.cpp. The net engine reads CMD_TYPE==1 ->
    // Extend (0x5B8 cmd, pool 100), else Native (0x5A0 cmd, pool 10).
    namespace command_queue {
        // CExCommandQueue (Extend)
        constexpr uptr ExCtor        = 0x00E11B60; // ctor (warms pool of 100 CExtendCommand)
        constexpr uptr ExDtor        = 0x00E11D10; // vtbl[0]
        constexpr uptr ExNonEmpty    = 0x00C4D660; // vtbl[3] -> count (this+56); !=0 == non-empty
        constexpr uptr ExGet         = 0x00E119B0; // vtbl[4] -> CExtendCommand* (pop front) or 0
        constexpr uptr ExPut         = 0x00E11D30; // vtbl[5] (connector, sid2, body, size, cryptType)
        constexpr uptr ExPutEx       = 0x00E11FB0; // vtbl[6] (filtered by regIdx & sid2)
        // CCommandQueue (Native)
        constexpr uptr NaCtor        = 0x00E18AA0; // ctor (warms pool of 10 CBasicCommand)
        constexpr uptr NaPut         = 0x00E18EF0; // vtbl[5]
        // Deque field offsets within the queue object (dword indices):
        //   [7]=map  [12]=mapsize  [13]=frontIdx  [14]=count ; CRITICAL_SECTION at +4
        constexpr u32  DequeBase     = 28;   // byte offset of the embedded std::deque
        constexpr u32  CmdQueueAlloc = 0x3C; // operator new size for the queue object
        // CExtendCommand (0x5B8) field byte offsets (set by Put):
        constexpr u32  CmdBody       = 8;    // CTcpPacket (decrypted) — PacketsCallbackDistribution(cmd+8)
        constexpr u32  CmdConnector  = 1440;
        constexpr u32  CmdSid2       = 1444;
        constexpr u32  CmdRegIdx     = 1448;
        constexpr u32  CmdSize       = 1452;
        constexpr u32  CmdTick       = 1456; // u64 GetMSec
        constexpr u32  ExtendCmdSize = 0x5B8;
        constexpr u32  BasicCmdSize  = 0x5A0;
    }

    // ── CDispatcher / CDispatcherArray (libnetengine Dispatcher.cpp) ───────────
    namespace dispatcher {
        // CDispatcher (0x1C, vftable 0x10F3070)
        constexpr uptr Dtor             = 0x00E1C120; // vtbl[0]
        constexpr uptr Init             = 0x00E1C100; // vtbl[1] (model, cmdQueue)
        constexpr uptr Cleanup          = 0x00E1D4F0; // vtbl[2]
        constexpr uptr SetState         = 0x00E1D5D0; // vtbl[3] (int state)
        constexpr uptr OnRead           = 0x00E1D320; // vtbl[4] -> mainConnector->Read (conn vtbl[11])
        constexpr uptr Process          = 0x00E1D660; // vtbl[5] -> socket->SendQueue + connector->KeepAlive
        constexpr uptr SetMainConnector = 0x00E1D890; // vtbl[6] (int type) via g_kConnectorFactory
        constexpr uptr SetSubConnectors = 0x00E1D9E0; // vtbl[7] (int count)
        constexpr uptr Vftable          = 0x010F3070;
        // CDispatcher field byte offsets:
        constexpr u32  Model         = 4;
        constexpr u32  CmdQueue      = 8;
        constexpr u32  MainConnector = 12;
        constexpr u32  Index         = 16;
        constexpr u32  SubArray      = 20;
        constexpr u32  SubCount      = 24;
        constexpr u32  Size          = 0x1C;

        namespace array { // CDispatcherArray (NiSPJob, vftable 0x10F30A4, 0x1A4)
            constexpr uptr Ctor             = 0x00E1C170; // NiSPJob::NiSPJob
            constexpr uptr Dtor             = 0x00E1CC60; // vtbl[0]
            constexpr uptr Init             = 0x00E1C190; // vtbl[1] (model, cmdQueue, count<=64)
            constexpr uptr Run              = 0x00E1C760; // vtbl[5] (idx 0 = all) -> disp Process
            constexpr uptr SetMainConnAll   = 0x00E1C5A0; // vtbl[6]
            constexpr uptr SetSubConnAll    = 0x00E1C680; // vtbl[7]
            constexpr uptr Vftable          = 0x010F30A4;
            constexpr u32  AllocSize        = 0x1A4;
            // field byte offsets: +4 model, +8 count, +12 CDispatcher*[], +16 array[0x190]
            constexpr u32  Model    = 4;
            constexpr u32  Count    = 8;
            constexpr u32  List     = 12;
        }
    }

    // ── CEventSelectModel / ISensor (libnetengine EventSelectModel.cpp) ────────
    namespace model {
        constexpr uptr Ctor            = 0x00E17BF0; // CEventSelectModel::CEventSelectModel (0x318)
        constexpr uptr Dtor            = 0x00E18770; // vtbl[0]
        constexpr uptr Init            = 0x00E17C40; // vtbl[1] (count, cmdQueue) -> new CDispatcherArray
        constexpr uptr Reset           = 0x00E17E40; // vtbl[3] (WSACloseEvent all, memset arrays)
        constexpr uptr Select          = 0x00E17EC0; // vtbl[4] (DWORD timeout) — WSAWaitForMultipleEvents
        constexpr uptr Run             = 0x00E17C30; // vtbl[5] -> CDispatcherArray::Run
        constexpr uptr SetNetType      = 0x00E1B8F0; // vtbl[6]
        constexpr uptr RegisterSocket  = 0x00E183E0; // vtbl[8] (connectorId, SOCKET)
        constexpr uptr UnregisterSocket = 0x00E18580; // vtbl[9]
        constexpr uptr SetMainConnector = 0x00E1B4E0; // vtbl[13] — bind an external connector
        constexpr uptr RegisterConn23  = 0x00E1B9A0; // vtbl[14] — factory create+register type 2/3
        constexpr uptr RegisterConn01  = 0x00E1BD60; // vtbl[15] — factory create+register type 0/1 (front/main)
        constexpr uptr GetDispatcher   = 0x00E1B630; // vtbl[12]-ish helper used by Select (array vtbl[9])
        constexpr uptr Vftable         = 0x010F292C;
        constexpr u32  AllocSize       = 0x318;
        // field byte offsets:
        constexpr u32  Array     = 4;     // CDispatcherArray*
        constexpr u32  Count     = 8;     // registered socket/event count
        constexpr u32  Inited    = 12;    // byte
        constexpr u32  SockSlots = 0x18;  // (SOCKET, connectorId)[64], 8B stride, memset 0xFF (512B)
        constexpr u32  EventArr  = 0x218; // HANDLE event[64] (256B)
        constexpr u32  MaxConns  = 64;
    }

    // ── CNetEngine (libnetengine NetEngine.cpp, vftable 0x10F2040) ─────────────
    namespace engine {
        constexpr uptr Dtor        = 0x00E12730; // vtbl[0]
        constexpr uptr Initialize  = 0x00E12750; // vtbl[1] (SENSOR, CMD, size, NETWORK, a6)
        constexpr uptr Release     = 0x00E12330; // vtbl[2]
        constexpr uptr Clear       = 0x00E12390; // vtbl[3] -> cmdQueue->vtbl[7]
        constexpr uptr RecvPump    = 0x00E12310; // vtbl[4] -> model->Select(0)
        constexpr uptr GetCommand  = 0x00E12300; // vtbl[5] -> cmdQueue->Get
        constexpr uptr SendPump    = 0x00E12320; // vtbl[6] -> model->Run -> array Run -> Process
        constexpr uptr ModelFactory = 0x00E123B0; // sub_E123B0 (SENSOR_TYPE, count) -> model
        constexpr uptr RegisterConnector = 0x00E12560; // sub_E12560(engine, conn) -> model vtbl[13] SetMainConnector
        constexpr uptr CreateConnector   = 0x00E125F0; // sub_E125F0(engine, ip, port, type) -> model vtbl[14/15]
        constexpr uptr Teardown    = 0x00E126D0; // ~CNetEngine body: set vft, Release, set INetEngine vft
        constexpr uptr Vftable     = 0x010F2040;
        constexpr uptr BaseVftable = 0x010F1F3C; // INetEngine vftable
        // field byte offsets:
        constexpr u32  CmdQueue    = 4;
        constexpr u32  Sensor      = 8;   // the model (ISensor)
        constexpr u32  Initialized = 0xC; // byte
    }

    // ── g_kConnectorFactory / g_kDispatcherFactory (CFactory singletons) ───────
    namespace factory {
        constexpr uptr Get    = 0x0051D900; // sub_51D900 -> CFactory singleton (g_kConnectorFactory)
        constexpr uptr Create = 0x00DFA720; // sub_DFA720 (factory, type) -> connector/dispatcher
        constexpr u32  Registered = 32;     // factory+32 must be non-null
    }

    // ── Game-layer wiring (CNetMgr) ────────────────────────────────────────────
    namespace netmgr {
        constexpr uptr Ctor          = 0x00AB2E60; // CNetMgr ctor — constructs netEngine1/2 (NetEngine ctor E122E0)
        constexpr uptr InitEngines   = 0x00AB3140; // CNetMgr::InitializeEngines — Initialize() both engines, sets byte103C
        constexpr uptr InitPreStep   = 0x00AB04A0; // unknown_libname_349 — pre-init step at top of InitializeEngines
        constexpr uptr GameInit      = 0x00506DF0; // Game::Init — calls InitializeEngines at 0x507C3D
        constexpr uptr Dtor          = 0x00AB30C0; // CNetMgr dtor — tears down both engines (E126D0)
        constexpr uptr Shutdown      = 0x00AB3300; // CNetMgr shutdown — suspend NetengineWait thread + Release netEngine1
        constexpr uptr Wiring        = 0x00AB3450; // sub_AB3450 — CNetMgr connector/engine setup (CreateConnectors)
        constexpr uptr FrontPump     = 0x00AB3EF0; // netEngine2 (front) per-frame pump — send+recv+drain, gate byte103D
        constexpr uptr NetFrameTick  = 0x00AB40A0; // sub_AB40A0 — netEngine1 per-frame pump (login flavor)
        constexpr uptr FrameTickInRoom = 0x00AB4000; // sub_AB4000 — netEngine1 per-frame pump (in-room flavor)
        constexpr uptr DrainRoute    = 0x00AB42E0; // sub_AB42E0 — cmdQueue.Get -> PacketsCallbackDistribution
        constexpr uptr PacketsCallbackDistribution = 0x00AB5340; // char __stdcall(_WORD* WzPacket)
        constexpr u32  EngineOffset  = 0x0FCC; // CNetMgr+0xFCC = embedded netEngine1 (4044)
        constexpr u32  RecvGateByte  = 4201;   // CNetMgr+0x1069 = byte gating the netEngine1 recv pump
        constexpr u32  FrontPumpGate = 0x103D; // CNetMgr+0x103D = byte103D, gates the netEngine2 (front) pump
        // Connector subclasses (subclass-specific vtable slots over CTcpConnector)
        constexpr uptr MainCtor      = 0x00AB87B0; // CMainConnectorTcp::CMainConnectorTcp
        constexpr uptr MainVft       = 0x0106D66C; // CMainConnectorTcp vftable
        constexpr uptr MainDisconnect = 0x00AB0760; // CMainConnectorTcp::Disconnect (conn vtbl[14]) — game/UI policy
        constexpr uptr CastCtor      = 0x00AB8720; // CCastConnectorTcp::CCastConnectorTcp
        constexpr uptr CastVft       = 0x0106D60C; // CCastConnectorTcp vftable
        constexpr u32  ConnectorAlloc = 0x60;      // operator new size for a connector subclass
        // CNetMgr embedded fields (verified from sub_AB3450 disasm):
        constexpr u32  Engine1Offset  = 0x0FCC;    // netEngine1 (main+cast) — pumped by NetFrameTick + recv thread
        constexpr u32  Engine2Offset  = 0x0FE8;    // netEngine2 (front/auth) — CNetEngine is 0x1C; 0xFE8-0xFCC
        constexpr u32  TickFnPtr      = 0x0FC4;    // CNetMgr+0xFC4 = per-frame tick fn ptr (set to NetFrameTick)
        constexpr u32  FrontConnOff   = 0x1018;    // CFrontConnectorTcp
        constexpr u32  MainConnOff    = 0x101C;    // CMainConnectorTcp
        constexpr u32  CastConnOff    = 0x1020;    // CCastConnectorTcp
        constexpr u32  UdpConnOff     = 0x1024;    // CExConnectorUdp (presence => already wired)
        constexpr u32  WireGateByte   = 0x103C;    // byte103C — gates whether to wire at all (set by InitializeEngines)
        // CNetEngine::Initialize(sensorType, commandType, size, networkType, a6) args
        // per engine (verified at CNetMgr::InitializeEngines sub_AB3140):
        //   auth/front  netEngine2: (1, 0, 1,  2, 0)  EventSelect, Native queue, 1 dispatcher
        //   main/cast   netEngine1: (1, 1, 53, 1, 0)  EventSelect, Extend queue, 53 dispatchers
        constexpr int  AuthInitSensor = 1, AuthInitCmd = 0, AuthInitSize = 1,  AuthInitNet = 2, AuthInitA6 = 0;
        constexpr int  MainInitSensor = 1, MainInitCmd = 1, MainInitSize = 53, MainInitNet = 1, MainInitA6 = 0;
        constexpr u32  LocalAddrOff   = 0x1030;    // sockaddr filled by sub_E14640 (local addr probe)
        constexpr uptr RawSocketBind  = 0x00E13E80;// CRawSocket::Bind(ip, port) -> -1 on fail
        constexpr uptr LocalAddrProbe = 0x00E14640;// sub_E14640(&sockaddr) — fills CNetMgr local addr
        constexpr uptr FrontBasePort  = 0x11C6E54; // dword_11C6E54 — front bind base port
        constexpr uptr NiThreadSetName = 0x00AB8840; // sub_AB8840(thread, name)
        constexpr uptr NiThreadSetPriority = 0x00D7A570; // NiThread::SystemSetPriority(thread, prio)
        constexpr uptr NiMemObjectNew = 0x00D78BC0; // NiMemObject::operator new(size, hint)
        // ── NetengineWait recv thread (Gamebryo NiThread + CWaitProcedure) ──
        constexpr uptr WaitProcCtor  = 0x00AB8910; // CWaitProcedure(engine, timeout) ctor
        constexpr uptr WaitProcRun   = 0x00AB8950; // CWaitProcedure::Run — loop RecvPump(timeout)+Sleep(50)
        constexpr uptr WaitProcVft   = 0x0106D6CC; // CWaitProcedure vftable
        constexpr uptr NiThreadCreate = 0x00D7A630; // NiThread::Create(NiThreadProcedure*, mask) -> NiThread*
        constexpr u32  WaitProcTimeout = 10;       // Select timeout (ms) the thread passes
        constexpr u32  WaitProcSleep   = 50;       // 0x32 — thread loop Sleep (ms)
    }
}

// ── Game module base ──────────────────────────────────────────────────────────

namespace globals {
    inline uptr g_GameModuleBase = 0;
    inline u32  g_GameModuleSize = 0;
    inline uptr g_AntiCheatModuleBase = 0;
    inline u32  g_AntiCheatModuleSize = 0;

    // Custom vtable for 2FA processing dialog (CMsgDataLoginAuthorize minus sub_5C9B90).
    // [0] = RTTI locator, [1..9] = vtable entries. Object vftable points to &g_2faVtbl[1].
    inline uptr g_2faVtbl[10] = {};
    inline bool g_2faVtblReady = false;
}

} // namespace mg::game::addr
