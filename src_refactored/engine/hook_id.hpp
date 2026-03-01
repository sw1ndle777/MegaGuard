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
    inline constexpr u32 NetworkInitCrypto     = hookHash("NetworkInitCrypto");
    inline constexpr u32 MainAuthorize         = hookHash("MainAuthorize");
	inline constexpr u32 FrontAuthorize        = hookHash("FrontAuthorize");

    // ── Anticheat: Data ──────────────────────────────────────────────────
    inline constexpr u32 CDBMLoad              = hookHash("CDBMLoad");

    // ── Features ─────────────────────────────────────────────────────────
    inline constexpr u32 SpectateCamera        = hookHash("SpectateCamera");
    inline constexpr u32 DrawDebugInfo         = hookHash("DrawDebugInfo");
    inline constexpr u32 AgoraInitDlg          = hookHash("AgoraInitDlg");
    inline constexpr u32 AgoraConstructDlg     = hookHash("AgoraConstructDlg");
    inline constexpr u32 NationIndex           = hookHash("NationIndex");
    inline constexpr u32 WindowTitle           = hookHash("WindowTitle");
    inline constexpr u32 SetAspectRatioScale   = hookHash("SetAspectRatioScale");
    inline constexpr u32 HideWeaponSlot        = hookHash("HideWeaponSlot");

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

	// ── Gacha Pity ─────────────────────────────────────────────────
	inline constexpr u32 GachaBack              = hookHash("GachaBack");
	inline constexpr u32 GachaNext              = hookHash("GachaNext");
	inline constexpr u32 GachaSelect            = hookHash("GachaSelect");

	// ── Bugfixes
    inline constexpr u32 WR_RoomCreate         = hookHash("WR_RoomCreate");
    inline constexpr u32 WR_RoomSetting        = hookHash("WR_RoomSetting");
    inline constexpr u32 WR_RoomMain           = hookHash("WR_RoomMain");
    inline constexpr u32 ScreenshotBug         = hookHash("ScreenshotBug");
    inline constexpr u32 SetDateTimeNop        = hookHash("SetDateTimeNop");

    // ── Tickrate (individual patches) ────────────────────────────────────
    inline constexpr u32 TickFrametime         = hookHash("TickFrametime");
    inline constexpr u32 TickRotDamp           = hookHash("TickRotDamp");
    inline constexpr u32 TickMinRot            = hookHash("TickMinRot");
    inline constexpr u32 TickMaxRot            = hookHash("TickMaxRot");
    inline constexpr u32 TickRotThresh         = hookHash("TickRotThresh");

    // ── Dynamic patch bases (use base + loop index) ──────────────────────
    inline constexpr u32 TickDelayReq_Base     = 0x10000;
    inline constexpr u32 TickDelayAnim_Base    = 0x10100;
    inline constexpr u32 TickMinDist_Base      = 0x10200;
    inline constexpr u32 TickRotByte_Base      = 0x10300;
    inline constexpr u32 ResWidth_Base         = 0x20000;
    inline constexpr u32 ResHeight_Base        = 0x20100;
    inline constexpr u32 ResAspect_Base        = 0x20200;
    inline constexpr u32 ResListSize_Base      = 0x20300;

} // namespace HookId

} // namespace mg
