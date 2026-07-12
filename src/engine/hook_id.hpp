// =============================================================================
// HookId - Compile-time hook identifiers (no strings in binary)
// =============================================================================
// Uses constexpr FNV-1a to generate unique u32 IDs from string literals.
// The string only exists at compile time — never enters the binary.
// Registry uses u32 keys for O(1) lookup with zero string overhead.
//
// Usage:
//   registry.registerDetour(HookId::GetCRoom).create(...);
//   registry.findDetour(HookId::GetCRoom)->getOriginal<T>();
//
// For dynamic patches (loops), use base ID + loop index:
//   registry.registerSwapPatch(HookId::TickDelayReq_Base + i).patch(...);
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg {

// ── Compile-time FNV-1a hash ──────────────────────────────────────────────────
// Evaluated entirely at compile time. The source string never appears in binary.

constexpr u32 hookHash(const char* str) {
    u32 hash = 0x811c9dc5u;
    while (*str) {
        hash ^= static_cast<u32>(*str++);
        hash *= 0x01000193u;
    }
    return hash;
}

// ── Hook identifiers ─────────────────────────────────────────────────────────
// All values are compile-time constants (constexpr + consteval hash).

namespace HookId {

    // ── Anticheat: Game Managers ──────────────────────────────────────────
    inline constexpr u32 GetCRoom              = hookHash("GetCRoom");
    inline constexpr u32 DestroyCRoom          = hookHash("DestroyCRoom");
    inline constexpr u32 GetCUnitContainer     = hookHash("GetCUnitContainer");
    inline constexpr u32 DestroyCUnitContainer = hookHash("DestroyCUnitContainer");
    inline constexpr u32 GetCUnitMgr           = hookHash("GetCUnitMgr");
    inline constexpr u32 DestroyCUnitMgr       = hookHash("DestroyCUnitMgr");
    inline constexpr u32 GetCNetMgr            = hookHash("GetCNetMgr");
    inline constexpr u32 DestroyCNetMgr        = hookHash("DestroyCNetMgr");
    inline constexpr u32 GetCDynamics          = hookHash("GetCDynamics");
    inline constexpr u32 DestroyCDynamics      = hookHash("DestroyCDynamics");

    // ── Anticheat: Crypto ────────────────────────────────────────────────
    inline constexpr u32 RC5KeySetup           = hookHash("RC5KeySetup");
    inline constexpr u32 RC6KeySetup           = hookHash("RC6KeySetup");
    inline constexpr u32 CryptEncrypt          = hookHash("CryptEncrypt");
    inline constexpr u32 CryptDecrypt          = hookHash("CryptDecrypt");

    // ── Anticheat: Network ───────────────────────────────────────────────
    inline constexpr u32 Heartbeat             = hookHash("Heartbeat");
    inline constexpr u32 DisconnectPacket      = hookHash("DisconnectPacket");
    inline constexpr u32 NetworkInitCrypto     = hookHash("NetworkInitCrypto");
    inline constexpr u32 MainAuthorize         = hookHash("MainAuthorize");
	inline constexpr u32 FrontAuthorize        = hookHash("FrontAuthorize");

    // ── Anticheat: Data ──────────────────────────────────────────────────
    inline constexpr u32 CDBMLoad              = hookHash("CDBMLoad");

    // ── Features ─────────────────────────────────────────────────────────
    inline constexpr u32 WeaponDrop            = hookHash("WeaponDrop");
    inline constexpr u32 SpectateCamera        = hookHash("SpectateCamera");
    inline constexpr u32 DrawDebugInfo         = hookHash("DrawDebugInfo");
    inline constexpr u32 AgoraInitDlg          = hookHash("AgoraInitDlg");
    inline constexpr u32 AgoraConstructDlg     = hookHash("AgoraConstructDlg");
    inline constexpr u32 NationIndex           = hookHash("NationIndex");
    inline constexpr u32 NationPublisher       = hookHash("NationPublisher");
    inline constexpr u32 NationPublisherURL    = hookHash("NationPublisherURL");
    inline constexpr u32 NationFlag            = hookHash("NationFlag");
    inline constexpr u32 NationFontName        = hookHash("NationFontName");
    inline constexpr u32 NationBoldFontName    = hookHash("NationBoldFontName");
    inline constexpr u32 NationID              = hookHash("NationID");
    inline constexpr u32 NationGameBrand       = hookHash("NationGameBrand");
    inline constexpr u32 NationGameTitle       = hookHash("NationGameTitle");
    inline constexpr u32 WindowTitle           = hookHash("WindowTitle");
    inline constexpr u32 HudToggleProcess      = hookHash("HudToggleProcess");
    inline constexpr u32 SetAspectRatioScale   = hookHash("SetAspectRatioScale");
    inline constexpr u32 HideWeaponSlot        = hookHash("HideWeaponSlot");

    // ── Per-zoom mouse sensitivity ─────────────────────────────────────
    inline constexpr u32 ZoomSensMultiplier    = hookHash("ZoomSensMultiplier");
    inline constexpr u32 ZoomSensBattleAdjust  = hookHash("ZoomSensBattleAdjust");
    inline constexpr u32 ZoomSensLoadCfg       = hookHash("ZoomSensLoadCfg");
    inline constexpr u32 ZoomSensSaveCfg       = hookHash("ZoomSensSaveCfg");
    inline constexpr u32 ZoomSensOptionApply   = hookHash("ZoomSensOptionApply");
    inline constexpr u32 ZoomSensOptionPopulate= hookHash("ZoomSensOptionPopulate");
    inline constexpr u32 ZoomSensComboChange   = hookHash("ZoomSensComboChange");
    inline constexpr u32 ZoomSensComboClick    = hookHash("ZoomSensComboClick");
    inline constexpr u32 ZoomSensComboRegister = hookHash("ZoomSensComboRegister");

	// ── EditBox Clipboard ──────────────────────────────────────────────
	inline constexpr u32 EditBoxClipboard = hookHash("EditBoxClipboard");

	// ── UI Dialogs ─────────────────────────────────────────────────────
	inline constexpr u32 DlgInputPopupTitles   = hookHash("DlgInputPopupTitles");
	inline constexpr u32 DlgInputHandle        = hookHash("DlgInputHandle");
	inline constexpr u32 DlgRightClick         = hookHash("DlgRightClick");
	inline constexpr u32 DlgRightClickInput    = hookHash("DlgRightClickInput");

	// ── Quick Confirm (vtable swaps) ────────────────────────────────────
	inline constexpr u32 QuickConfirmGachaResult  = hookHash("QuickConfirmGachaResult");
	inline constexpr u32 QuickConfirmPackageItem  = hookHash("QuickConfirmPackageItem");
	inline constexpr u32 QuickConfirmCapsuleItem  = hookHash("QuickConfirmCapsuleItem");
	inline constexpr u32 QuickConfirmRepair       = hookHash("QuickConfirmRepair");
	inline constexpr u32 QuickConfirmBaseMsgBox   = hookHash("QuickConfirmBaseMsgBox");
	inline constexpr u32 QuickConfirmDialogTabCycle = hookHash("QuickConfirmDialogTabCycle");

	// ── Custom Packets ──────────────────────────────────────────────
	inline constexpr u32 CustomPacketDispatcher = hookHash("CustomPacketDispatcher");

	// ── Movement protocol (1:1 reimplementation) ────────────────────
	inline constexpr u32 MovementSend           = hookHash("MovementSend");
	inline constexpr u32 MovementReceive        = hookHash("MovementReceive");

	// ── Gacha Pity ─────────────────────────────────────────────────
	inline constexpr u32 GachaBack              = hookHash("GachaBack");
	inline constexpr u32 GachaNext              = hookHash("GachaNext");
	inline constexpr u32 GachaSelect            = hookHash("GachaSelect");

	// ── Weekly Reward ──────────────────────────────────────────────
	inline constexpr u32 WeeklyRewardButton     = hookHash("WeeklyRewardButton");

	// ── Trade ──────────────────────────────────────────────────────
	inline constexpr u32 TradeInviteMenu        = hookHash("TradeInviteMenu");
	inline constexpr u32 TradePopupPopulate     = hookHash("TradePopupPopulate");
	inline constexpr u32 TradeFrameTick         = hookHash("TradeFrameTick");

	// ── Bugfixes
	inline constexpr u32 WR_RoomCreate         = hookHash("WR_RoomCreate");
	inline constexpr u32 WR_RoomSetting        = hookHash("WR_RoomSetting");
	inline constexpr u32 WR_RoomMain           = hookHash("WR_RoomMain");
	inline constexpr u32 DisconnectReloginState = hookHash("DisconnectReloginState");
	inline constexpr u32 ScreenshotBug         = hookHash("ScreenshotBug");
	inline constexpr u32 SetDateTimeNop        = hookHash("SetDateTimeNop");
	inline constexpr u32 CpuidInitStub         = hookHash("CpuidInitStub");
	inline constexpr u32 WndProcCloseFix       = hookHash("WndProcCloseFix");

    // ── Grade Threshold ──────────────────────────────────────────────────
    inline constexpr u32 GradeRoomCreate      = hookHash("GradeRoomCreate");
    inline constexpr u32 GradePartyCreate     = hookHash("GradePartyCreate");
    inline constexpr u32 GradeAloneMode       = hookHash("GradeAloneMode");
    inline constexpr u32 GradeMatchGmBypass1  = hookHash("GradeMatchGmBypass1");
    inline constexpr u32 GradeMatchGmBypass2  = hookHash("GradeMatchGmBypass2");
    inline constexpr u32 GradeMatchValidation = hookHash("GradeMatchValidation");

    // ── Tickrate (individual patches) ────────────────────────────────────
    inline constexpr u32 TickFrametime         = hookHash("TickFrametime");
    inline constexpr u32 TickRotDamp           = hookHash("TickRotDamp");
    inline constexpr u32 TickMinRot            = hookHash("TickMinRot");
    inline constexpr u32 TickMaxRot            = hookHash("TickMaxRot");
    inline constexpr u32 TickRotThresh         = hookHash("TickRotThresh");
    inline constexpr u32 TickModelLeanZero     = hookHash("TickModelLeanZero");
    inline constexpr u32 TickRotThreshLimit    = hookHash("TickRotThreshLimit");

    // ── Dynamic patch bases (use base + loop index) ──────────────────────
    inline constexpr u32 TickDelayReq_Base     = 0x10000;
    inline constexpr u32 TickDelayAnim_Base    = 0x10100;
    inline constexpr u32 TickMinDist_Base      = 0x10200;
    inline constexpr u32 TickRotByte_Base      = 0x10300;
	inline constexpr u32 TickRotThresh_Base    = 0x10400;
    inline constexpr u32 TickFrametime_Base    = 0x10500;
    inline constexpr u32 TickRotDamp_Base      = 0x10600;
    inline constexpr u32 TickMinRot_Base       = 0x10700;
    inline constexpr u32 TickMaxRot_Base       = 0x10800;
    inline constexpr u32 ResWidth_Base         = 0x20000;
    inline constexpr u32 ResHeight_Base        = 0x20100;
    inline constexpr u32 ResAspect_Base        = 0x20200;
    inline constexpr u32 ResListSize_Base      = 0x20300;

} // namespace HookId

} // namespace mg
