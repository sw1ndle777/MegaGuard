// =============================================================================
// DlgQuickConfirm - Spacebar/Enter quick confirm for dialog popups
// =============================================================================
// Swaps vtable entries on dialog classes so pressing Enter or Space
// triggers the visible confirm/OK button instead of requiring a mouse click.
//
// Each dialog has its own hook handler because the button IDs and
// vtable layouts differ between classes.
//
// Hooks (vtable swaps):
//   - CDlgGachaResult  vtable[7]: hkGachaResultConfirm
//   - CDlgPackageItem  vtable[7]: hkPackageItemConfirm
//   - CDlgCapsuleItem  vtable[7]: hkCapsuleItemConfirm
//   - CDlgRepair       vtable[7]: hkRepairConfirm
//   - CDlgBaseMsgBox   vtable[7]: hkBaseMsgBoxConfirm
//
// Detour:
//   - sub_E766E0 (DialogTabCycle): hkDialogTabCycle — clears ConfirmBlockFlag
// =============================================================================
#include "pch.hpp"
#include "game/ui/dialog/quickconfirm/quickconfirm.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "platform/memory.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"
#include "utils/logger.hpp"

namespace mg::game {

	namespace {

		// ── Game function typedefs ──────────────────────────────────────────────────

		// Original vtable function: bool __thiscall (this, a2, msg, vkey, a5)
		using tDlgVFunc     = bool(__thiscall*)(u32, int, u32, u32, int);

		// sub_8B4600: dialog control lookup — int* __thiscall (containerPtr, &controlId)
		using tFindControl  = int*(__thiscall*)(int**, int*);

		// sub_E8C8F0: play sound — void __thiscall (char* this, int a2, int wavStr)
		using tPlaySound    = int(__thiscall*)(char*, int, int);

		// sub_66FB60: repair finish — void __thiscall (void* this)
		using tRepairFinish = void(__thiscall*)(void*);

		// sub_E766E0: dialog tab cycle — void __thiscall (int this)
		using tDialogTabCycle = void(__thiscall*)(int);

		// ── Shared predicate
		// Returns true when the key event should be passed through to the original.
		MG_FORCEINLINE bool shouldPassThrough(u32 msg, u32 key)
		{
			return msg < WM_KEYDOWN || msg > WM_KEYUP
				|| (key != VK_RETURN && key != VK_SPACE);
				//|| *reinterpret_cast<u32*>(MG_CONST(addr::ui::quickconfirm::ConfirmBlockFlag));
		}

		MG_FORCEINLINE bool shouldPassThrough2(u32 msg, u32 key)
		{
			return msg < WM_KEYDOWN || msg > WM_KEYUP
				|| (key != VK_RETURN && key != VK_SPACE)
				|| *reinterpret_cast<u32*>(MG_CONST(addr::ui::quickconfirm::ConfirmBlockFlag));
		}

		// ── CDlgGachaResult ─────────────────────────────────────────────────────────

		tDlgVFunc g_originalGachaConfirm = nullptr;

		// Checks dialog control IDs 103003/103004/103005 for a visible button,
		// then forwards Enter/Space to it via vtable[13].
		bool __fastcall hkGachaResultConfirm(u32 _this, u32 /*edx*/, int a2, u32 a3, u32 key, int a5)
		{
			if (shouldPassThrough(a3, key))
				return g_originalGachaConfirm(_this, a2, a3, key, a5);

			auto FindControl = reinterpret_cast<tFindControl>(
				MG_CONST(addr::ui::GetDlgInfo));

			u32 v10 = 0;

			// Try button IDs 103003, 103004, 103005 — pick the last visible one
			constexpr int kButtonIds[] = { 103003, 103004, 103005 };
			for (int id : kButtonIds)
			{
				int controlId = id;
				int* result = FindControl(
					reinterpret_cast<int**>(_this + 0x6B0), &controlId);
				u32 v11 = *reinterpret_cast<u32*>(result);
				if (v11 && mg::callVFunc<u8>(reinterpret_cast<void*>(v11), 25))
					v10 = v11;
			}

			return (v10
				&& mg::callVFunc<u8>(reinterpret_cast<void*>(v10), 13, a3, key, a5))
				|| g_originalGachaConfirm(_this, a2, a3, key, a5);
		}

		// ── CDlgPackageItem ─────────────────────────────────────────────────────────

		tDlgVFunc g_originalPackageItemConfirm = nullptr;

		// Reads dialog state at this+0x814 (old 0x828 - 0x14),
		// picks button 102002 for states 1-3, 102003 for state 5.
		bool __fastcall hkPackageItemConfirm(u32 _this, u32 /*edx*/, int a2, u32 msg, u32 key, int a5) // not working
		{
			if (shouldPassThrough(msg, key))
				return g_originalPackageItemConfirm(_this, a2, msg, key, a5);

			auto FindControl = reinterpret_cast<tFindControl>(
				MG_CONST(addr::ui::GetDlgInfo));

			u32 v10 = 0;
			int state = *reinterpret_cast<int*>(_this + 0x814); // old 0x828 - 0x14

			if (state > 0)
			{
				if (state <= 3)
				{
					int controlId = 102002;
					v10 = *reinterpret_cast<u32*>(
						FindControl(reinterpret_cast<int**>(_this + 0x6B0), &controlId));
				}
				else if (state == 5)
				{
					int controlId = 102003;
					v10 = *reinterpret_cast<u32*>(
						FindControl(reinterpret_cast<int**>(_this + 0x6B0), &controlId));
				}
			}

			return (v10
				&& mg::callVFunc<u8>(reinterpret_cast<void*>(v10), 13, msg, key, a5))
				|| g_originalPackageItemConfirm(_this, a2, msg, key, a5);
		}

		// ── CDlgCapsuleItem ─────────────────────────────────────────────────────────

		tDlgVFunc g_originalCapsuleItemConfirm = nullptr;

		// Checks dialog control IDs 109003/109002 for a visible button,
		// then forwards Enter/Space to it via vtable[13].
		bool __fastcall hkCapsuleItemConfirm(u32 _this, u32 /*edx*/, int a2, u32 a3, u32 key, int a5) //idk if work
		{
			if (shouldPassThrough(a3, key))
				return g_originalCapsuleItemConfirm(_this, a2, a3, key, a5);

			auto FindControl = reinterpret_cast<tFindControl>(
				MG_CONST(addr::ui::GetDlgInfo));

			u32 v9 = 0;

			constexpr int kButtonIds[] = { 109003, 109002 };
			for (int id : kButtonIds)
			{
				int controlId = id;
				int* result = FindControl(
					reinterpret_cast<int**>(_this + 0x6B0), &controlId);
				u32 v10 = *reinterpret_cast<u32*>(result);
				if (v10 && mg::callVFunc<u8>(reinterpret_cast<void*>(v10), 25))
					v9 = v10;
			}

			return (v9
				&& mg::callVFunc<u8>(reinterpret_cast<void*>(v9), 13, a3, key, a5))
				|| g_originalCapsuleItemConfirm(_this, a2, a3, key, a5);
		}

		// ── CDlgRepair ──────────────────────────────────────────────────────────────

		tDlgVFunc g_originalRepairConfirm = nullptr;

		// Finds button 103002, plays click sound, calls vtable[1] on inner object
		// at this+0x80C, calls RepairFinish, then always falls through to original.
		bool __fastcall hkRepairConfirm(u32 _this, u32 /*edx*/, int a2, u32 msg, u32 key, int a5) // not working
		{

			if (!shouldPassThrough(msg, key))
			{
				auto FindControl = reinterpret_cast<tFindControl>(
					MG_CONST(addr::ui::GetDlgInfo));
				auto PlaySound = reinterpret_cast<tPlaySound>(
					MG_CONST(addr::ui::quickconfirm::PlaySoundOnControl));
				auto RepairFinish = reinterpret_cast<tRepairFinish>(
					MG_CONST(addr::ui::quickconfirm::RepairFinish));

				int controlId = 103002;
				char** v5 = reinterpret_cast<char**>(
					FindControl(reinterpret_cast<int**>(_this + 0x6B0), &controlId));
				PlaySound(*v5, 2, reinterpret_cast<int>("ui_mouse_click.wav"));

				// Call vtable[1] on the object at *(this + 0x80C)
				u32 innerObj = *reinterpret_cast<u32*>(_this + 0x80C);
				mg::callVFunc<void>(reinterpret_cast<void*>(innerObj), 1);

				RepairFinish(reinterpret_cast<void*>(_this));
			}

			return g_originalRepairConfirm(_this, a2, msg, key, a5);
		}

		// ── CDlgBaseMsgBox ──────────────────────────────────────────────────────────

		tDlgVFunc g_originalBaseMsgBoxConfirm = nullptr;

		// Reads state at this+0x80C, picks button by state:
		//   1 → 104004, 2 → 104001, 3 → 104003.
		// Uses sub_5E87A0 for control lookup instead of sub_8B4600.
		bool __fastcall hkBaseMsgBoxConfirm(u32 _this, u32 /*edx*/, int a2, u32 a3, u32 key, int a5)
		{
			if (shouldPassThrough2(a3, key))
				return g_originalBaseMsgBoxConfirm(_this, a2, a3, key, a5);

			auto FindControl = reinterpret_cast<tFindControl>(
				MG_CONST(addr::ui::quickconfirm::BaseMsgBoxFindControl));

			u32 v11 = 0;
			int state = *reinterpret_cast<int*>(_this + 0x80C);

			switch (state)
			{
			case 1: {
				int controlId = 104004;
				v11 = *reinterpret_cast<u32*>(
					FindControl(reinterpret_cast<int**>(_this + 0x6B0), &controlId));
				break;
			}
			case 2: {
				int controlId = 104001;
				v11 = *reinterpret_cast<u32*>(
					FindControl(reinterpret_cast<int**>(_this + 0x6B0), &controlId));
				break;
			}
			case 3: {
				int controlId = 104003;
				v11 = *reinterpret_cast<u32*>(
					FindControl(reinterpret_cast<int**>(_this + 0x6B0), &controlId));
				break;
			}
			}

			return (v11
				&& mg::callVFunc<u8>(reinterpret_cast<void*>(v11), 13, a3, key, a5))
				|| g_originalBaseMsgBoxConfirm(_this, a2, a3, key, a5);
		}

		// ── DialogTabCycle (sub_E766E0) ─────────────────────────────────────────
		// Newer client clears dword_15F2E08 to 0 after calling vtable[18] on the
		// current block-flag object. Without this, CDlgRepair and CDlgPackageItem
		// quick confirm won't work because the flag stays set.

		void __fastcall hkDialogTabCycle(int _this, int /*edx*/)
		{
			u32* pBlockFlag = reinterpret_cast<u32*>(
				MG_CONST(addr::ui::quickconfirm::ConfirmBlockFlag));

			if (*pBlockFlag)
			{
				// Call vtable[18] on the block-flag object, passing this as arg
				mg::callVFunc<void>(reinterpret_cast<void*>(*pBlockFlag), 18, _this);
				// Clear the flag — newer client behavior
				*pBlockFlag = 0;
			}

			// Call original (will skip the if-block since flag is now 0,
			// then proceed to the iterator that may set a new dialog)
			auto& registry = mg::ctx().hookRegistry();
			auto original = registry.findDetour(HookId::QuickConfirmDialogTabCycle)
				->getOriginal<tDialogTabCycle>();
			original(_this);
		}
	}

// ── DlgQuickConfirm class ───────────────────────────────────────────────────

DlgQuickConfirm::DlgQuickConfirm(MegaGuardContext& ctx) : ctx_(ctx) {}
DlgQuickConfirm::~DlgQuickConfirm() = default;

VoidResult DlgQuickConfirm::install() {
	auto& registry = ctx_.hookRegistry();


	// ── DialogTabCycle: detour sub_E766E0 ────────────────────────────────
	// Fixes ConfirmBlockFlag not being cleared — required for Repair/Package
	//registry.registerDetour(HookId::QuickConfirmDialogTabCycle)
	//	.create(MG_CONST(addr::ui::quickconfirm::DialogTabCycle), hkDialogTabCycle);

	// ── CDlgGachaResult: swap vtable[7] ──────────────────────────────────
	g_originalGachaConfirm = reinterpret_cast<tDlgVFunc>(
		*reinterpret_cast<uptr*>(MG_CONST(addr::ui::quickconfirm::CDlgGachaResult_VTable7)));

	registry.registerSwapPatch(HookId::QuickConfirmGachaResult)
		.patch(MG_CONST(addr::ui::quickconfirm::CDlgGachaResult_VTable7),
			reinterpret_cast<void*>(hkGachaResultConfirm), 0);

	// ── CDlgPackageItem: swap vtable[7] ──────────────────────────────────
	g_originalPackageItemConfirm = reinterpret_cast<tDlgVFunc>(
		*reinterpret_cast<uptr*>(MG_CONST(addr::ui::quickconfirm::CDlgPackageItem_VTable7)));

	registry.registerSwapPatch(HookId::QuickConfirmPackageItem)
		.patch(MG_CONST(addr::ui::quickconfirm::CDlgPackageItem_VTable7),
			reinterpret_cast<void*>(hkPackageItemConfirm), 0);

	// ── CDlgCapsuleItem: swap vtable[7] ──────────────────────────────────
	g_originalCapsuleItemConfirm = reinterpret_cast<tDlgVFunc>(
		*reinterpret_cast<uptr*>(MG_CONST(addr::ui::quickconfirm::CDlgCapsuleItem_VTable7)));

	registry.registerSwapPatch(HookId::QuickConfirmCapsuleItem)
		.patch(MG_CONST(addr::ui::quickconfirm::CDlgCapsuleItem_VTable7),
			reinterpret_cast<void*>(hkCapsuleItemConfirm), 0);

	// ── CDlgRepair: swap vtable[7] ───────────────────────────────────────
	g_originalRepairConfirm = reinterpret_cast<tDlgVFunc>(
		*reinterpret_cast<uptr*>(MG_CONST(addr::ui::quickconfirm::CDlgRepair_VTable7)));

	registry.registerSwapPatch(HookId::QuickConfirmRepair)
		.patch(MG_CONST(addr::ui::quickconfirm::CDlgRepair_VTable7),
			reinterpret_cast<void*>(hkRepairConfirm), 0);

	// ── CDlgBaseMsgBox: swap vtable[7] ───────────────────────────────────
	g_originalBaseMsgBoxConfirm = reinterpret_cast<tDlgVFunc>(
		*reinterpret_cast<uptr*>(MG_CONST(addr::ui::quickconfirm::CDlgBaseMsgBox_VTable7)));

	registry.registerSwapPatch(HookId::QuickConfirmBaseMsgBox)
		.patch(MG_CONST(addr::ui::quickconfirm::CDlgBaseMsgBox_VTable7),
			reinterpret_cast<void*>(hkBaseMsgBoxConfirm), 0);

	

	return VoidResult::ok();
}

} // namespace mg::game
