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
    constexpr uptr Custom_GetNationIndex   = 0x00562AC0;   // FUNCTION address (hook target)
    constexpr uptr Custom_GetWindowTitle   = 0x00562BC0;
    constexpr uptr SpectateObserver        = 0x00576E40;
    constexpr uptr DrawDebugInfo           = 0x0050DDC0;
    constexpr uptr CommonAgoraDlgConstruct = 0x00694BE0;
    constexpr uptr CommonAgoraDlgInit      = 0x0059AC10;
    constexpr uptr InitPcBangInfo          = 0x00698910;

    namespace dlg {
        constexpr uptr CFactoryGet       = 0x005205E0;
        constexpr uptr CExPICBaseAlloc   = 0x005EE0F0;
        constexpr uptr GetDlgInfo        = 0x008B4600;
        constexpr uptr GetDlgId          = 0x00DFA720;
        constexpr uptr GetDlgById        = 0x00DFA680;
        constexpr uptr AssignDlgInfo     = 0x00E4E760;
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

        constexpr uptr DelayReq1  = 0x009007AF;
        constexpr uptr DelayReq2  = 0x009007E9;
        constexpr uptr DelayReq3  = 0x00920935;
        constexpr uptr DelayReq4  = 0x00920957;
        constexpr uptr DelayReq5  = 0x009573EA;
        constexpr uptr DelayReq6  = 0x0095780A;
        constexpr uptr DelayReq7  = 0x00957829;
        constexpr uptr DelayReq8  = 0x009579DC;
        constexpr uptr DelayReq9  = 0x00957B63;
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

        constexpr uptr DelayAnims1  = 0x005CF081;
        constexpr uptr DelayAnims2  = 0x005CF136;
        constexpr uptr DelayAnims3  = 0x009AF2C1;
        constexpr uptr DelayAnims4  = 0x009AF2F2;
        constexpr uptr DelayAnims5  = 0x009AF4AC;
        constexpr uptr DelayAnims6  = 0x00A19C04;
        constexpr uptr DelayAnims7  = 0x00A1D2CC;
        constexpr uptr DelayAnims8  = 0x00A21655;
        constexpr uptr DelayAnims9  = 0x00A21D99;
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

        // ── Consolidated arrays and constants for refactored API ──────────
        // Single-address aliases (patch just the first in group — extend if needed)
        constexpr uptr FrameTime         = Frametime1;
        constexpr uptr RotationDamping   = RotDamp1;
        constexpr uptr MinRotationSpeed  = MinRotSpeed1;
        constexpr uptr MaxRotationSpeed  = MaxRotSpeed1;
        constexpr uptr RotationThreshold = RotThreshold1;

        // Arrays
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

        // Patch values (from original CustomTickrate globals)
        inline const double  kFrameTimeValue             = 1.0;
        inline const double  kRotationDampingValue        = 1.0;
        inline const float   kMinRotationSpeedValue       = 0.2f;
        inline const float   kMaxRotationSpeedValue       = 5.0f;
        inline const float   kRotationThresholdValue      = 1.570796371f;
        inline const float   kDelayRequestValue           = 1.0f / 128.0f; // hardcoded_tickrate = 1/room_tickrate
        inline const float   kDelayAnimationValue         = 0.8f / 128.0f; // hardcoded_tickrate2
        inline const double  kMinDistanceValue            = 10.0;
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

namespace bugfixes {
    constexpr uptr RoomCreateDialogHandler   = 0x006918C0;
    constexpr uptr RoomSettingsDialogHandler = 0x0066B3F0;
    constexpr uptr RoomMainDialogHandler     = 0x00660280;
    constexpr uptr ScreenshotBug1            = 0x004618F0;
    constexpr uptr GameHWND                  = 0x011C898C;
    constexpr uptr ScreenshotIncrementer     = 0x011D9BB8;
    constexpr uptr InterlockedScreenshot     = 0x011C6898;
    constexpr uptr TakeScreenshot            = 0x004615C0;
    constexpr uptr SetDateTimeShit           = 0x00AC83BE;
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
        constexpr uptr PrevGameStatePtr         = 0x00000000;   // TODO: address of g_PrevGameStatePtr global (CState*)
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
        constexpr uptr GetDialogInfo = 0x00E4E660;

    }
    namespace ui_msgbox {
        constexpr uptr Get = 0x011DE354;
    }
    namespace ui_msgadapter {
        constexpr uptr Get = 0x011DECAC;
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
    }

    constexpr uptr IMsgDataInit = 0x00E5A130;
    constexpr uptr CMsgDataLoginBanUser_vftbl = 0x010735F4; // CMsgDataLoginBanUser vftable
    constexpr uptr CMsgDataLoginAuthorize_vftbl  = 0x010213B8; // CMsgDataLoginAuthorize vftable
    constexpr uptr CloseAllMessages           = 0x00E62E70;   // sub_E62E70: pushes CMsgCloseAll into CUIMsgBox
    constexpr uptr AuthorizeDisconnectAll     = 0x00AB5F60;   // void __thiscall AuthorizeDisconnectAll(CNetMgr*, const char*, u32, u32)
    constexpr uptr FindDialog                 = 0x00E61540;   // void* __thiscall CUIMsgBox::FindDialog(const char* name)
    constexpr uptr GetDlgInfo                 = 0x008B4600;
    constexpr uptr GetUIMsgText               = 0x005C48A0;
    constexpr uptr FormatStringSafe           = 0x0048B250;   // int __cdecl (char* Buffer, char* Format, ...)
}

namespace msvc {
    constexpr uptr operator_new = 0x00F1988C;
    constexpr uptr WStringCtor  = 0x00F8B47C;   // IAT entry: int __thiscall std::wstring::wstring(_DWORD, _DWORD) — dereference before calling
    constexpr uptr WStringDtor  = 0x00F8B474;   // IAT entry: void __thiscall std::wstring::~wstring(void*) — dereference before calling
    constexpr uptr PtInRect     = 0x00F8B910;   // IAT entry: BOOL __stdcall PtInRect(const RECT*, POINT) — dereference before calling
}


// ── Anticheat ─────────────────────────────────────────────────────────────────

namespace anticheat {
    namespace ack_handlers {
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
            constexpr u32  AllocSize = 0x1518; // old 0x14F8 aka last 0x14F4 offset now 0x1518 because i added uint8_t auth_token[32]
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
