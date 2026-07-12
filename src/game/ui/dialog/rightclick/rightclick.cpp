// =============================================================================
// DlgRightClick - Right-click item handler (CExSLTInv)
// =============================================================================
// Hooks:
//   - HandleRightClickIds (0x6B0260): intercepts right-click dialog messages
//   - rightClickHandle: processes item right-click (sell/delete/evolution)
// =============================================================================
#include "pch.hpp"
#include "game/ui/dialog/rightclick/rightclick.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "platform/memory.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::game {

	namespace {

		// ── Game function typedefs ──────────────────────────────────────────────────

		using tPlaySound           = void(__stdcall*)(const char*, bool, bool);
		using tGetSlotItem         = int(__cdecl*)();
		using tFindDialog          = u8(__thiscall*)(CUIMsgBox*, const char*);
		using tHandleSpecialItem   = void(__thiscall*)(u32);
		using tUpdateRightClick    = void(__thiscall*)(u32);
		using tProcessRightClick   = void(__thiscall*)(u32*);
		using tGetDialogInfo       = u32* (__thiscall*)(CGUIManager*, const char*);
		using tQuickResellItem     = void(__thiscall*)(u32*, int);
		using tQuickDeleteItem     = void(__thiscall*)(u32*);
		using tSendSlotNotify      = void(__thiscall*)(void*, u32, u32, u32);
		using tGetSlotHandler      = u32(__cdecl*)();
		using tGetSlotFromInput    = u32(__thiscall*)(void*, int);
		using tReleaseSlotCapture  = void(__thiscall*)(void*, u32);
		using tGetSlotCount        = u32(__thiscall*)(u32*);
		using tGetSlotAt           = u32*(__thiscall*)(u32*, u32);
		using tSetSlotCapture      = BOOL(__thiscall*)(void*, int);
		using tPtInRect            = BOOL(__stdcall*)(const RECT*, POINT);
		using tScrollHandler       = char(__thiscall*)(void*, int);

		// ── Reimplemented game functions (no addresses in this version) ────────────

		// sub_B95A60: returns item data pointer from slot
		int GetItemData(u32 ptr)
		{
			u32 v = *reinterpret_cast<u32*>(ptr + 0x20);
			return v ? static_cast<int>(v) : *reinterpret_cast<int*>(ptr + 0x8);
		}

		// sub_6A3A60: char __thiscall (void* this, int a2)
		char CanSellItem(u32 _this, int a2)
		{
			if (!a2)
				return 0;

			auto& log = mg::ctx().logger();
			char v5 = *reinterpret_cast<u8*>(MG_CONST(addr::ui::rightclick::SellFlag)); // byte_11A3D5A
			int itemId = GetItemData(static_cast<u32>(a2));
			auto mp_sellprice = *reinterpret_cast<u32*>(itemId + 0x48);// prolly 0x60
			if (itemId && !mp_sellprice)
			{
				v5 = 0;
			}
			int v3 = (*reinterpret_cast<u32*>(a2 + 0x24) >> 23) & 0x1FF;
			int result = GetItemData(static_cast<u32>(a2));
			auto idk2 = *reinterpret_cast<u32*>(result + 0x28);
			if (v3 != idk2)
			{
				return 0;
			}
				

			return v5;
		}

		// sub_6A3B00: bool __stdcall (int a1)
		bool CanDeleteItem(int a1)
		{
			return a1 && *reinterpret_cast<u64*>(a1 + 0x2C) != 0;
		}

		// _this = CExSLTInv
		void rightClickHandle(u32 _this, u32 dialogData)
		{
			auto PlaySound = reinterpret_cast<tPlaySound>(
				MG_CONST(addr::ui::sound::PlayUISound));
			if (!dialogData) return;

			PlaySound(MG_STR("ui_slot_mouseclick.wav"), true, false);
			mg::writeValue<DWORD>(dialogData, 0xA04, -1);
			*reinterpret_cast<DWORD*>(MG_CONST(addr::ui::rightclick::SomeValue)) = 0;
			auto GetSlotItem = reinterpret_cast<tGetSlotItem>(
				MG_CONST(addr::ui::rightclick::GetSlotItem));
			int v5 = GetSlotItem();
			if (!v5) return;
			
			auto* UIMsgBox = *reinterpret_cast<CUIMsgBox**>(
				MG_CONST(addr::ui::ui_msgbox::Get));

			auto FindDialog = reinterpret_cast<tFindDialog>(
				MG_CONST(addr::ui::FindDialog));

			
			if (*reinterpret_cast<u8*>(_this + 0x8CC - 0x14) // 0x8CC
				&& FindDialog(UIMsgBox, MG_STR("E_DLG_EVOLUTION_UPGRADE"))
				&& *reinterpret_cast<u32*>(_this + 0x8C8 - 0x14)) // 0x8C8
			{
				*reinterpret_cast<u32*>(_this + 0x844 - 0x14) = 0; // 0x844
			}
			else
			{
				*reinterpret_cast<u32*>(_this + 0x844 - 0x14) = v5; // 0x844
				*reinterpret_cast<u32*>(_this + 0x848 - 0x14) = 0; // 0x848
				u32 selectedItem = *reinterpret_cast<u32*>(_this + 0x844 - 0x14); // 0x844
				int itemId = GetItemData(selectedItem);
				if (!itemId) return;
				int itemType = *reinterpret_cast<u32*>(itemId + 4);
				if (itemType == 24 || itemType == 17)
				{
					auto HandleSpecialItem = reinterpret_cast<tHandleSpecialItem>(
						MG_CONST(addr::ui::rightclick::HandleSpecialItem));
					HandleSpecialItem(_this);
				}

				auto UpdateRightClick = reinterpret_cast<tUpdateRightClick>(
					MG_CONST(addr::ui::rightclick::UpdateRightClick));
				UpdateRightClick(_this);

				auto ProcessRightClick = reinterpret_cast<tProcessRightClick>(
					MG_CONST(addr::ui::rightclick::ProcessRightClick));
				ProcessRightClick(reinterpret_cast<u32*>(_this));

				if (CanSellItem(_this,
					*reinterpret_cast<int*>(_this + 0x844 - 0x14))) // 0x844
				{
					auto GUIManager = reinterpret_cast<CGUIManager*>(
						MG_CONST(addr::ui::gui_manager::Get));

					auto GetDialogInfo = reinterpret_cast<tGetDialogInfo>(
						MG_CONST(addr::ui::gui_manager::GetDialogInfo));
					auto* v3 = GetDialogInfo(GUIManager, MG_STR("E_DLG_BTN_INVEN"));
					if (v3)
					{
						auto QuickResellItem = reinterpret_cast<tQuickResellItem>(
							MG_CONST(addr::ui::rightclick::QuickResellItem));
						QuickResellItem(v3, 2);
					}
				}
				else
				{
					if (CanDeleteItem(*reinterpret_cast<int*>(_this + 0x844 - 0x14))) // 0x844
					{
						auto QuickDeleteItem = reinterpret_cast<tQuickDeleteItem>(
							MG_CONST(addr::ui::rightclick::QuickDeleteItem));
						QuickDeleteItem(reinterpret_cast<u32*>(_this));
					}
				}
			}
			
		}
		
		void __fastcall hkHandleRightClickIds(u32 _this, u32 edx, u32 type, u32 dialogId, u32 dialogData)
		{
			// Call original
			auto& registry = mg::ctx().hookRegistry();
			auto original = registry.findDetour(HookId::DlgRightClick)
				->getOriginal<decltype(&hkHandleRightClickIds)>();


			if (type == 0x808 && dialogId == 101093)//101117)
				rightClickHandle(_this, dialogData);

			original(_this, edx, type, dialogId, dialogData);
		}

		void __fastcall hkHandleRightClickInput(u32 _this, u32 edx, u32 type, POINT pt, u32 idk, u32 idk2)
		{
			if (!(*reinterpret_cast<u8*>(_this + 0x372)
				&& *reinterpret_cast<u8*>(_this + 0x371)))
				return;

			auto PtInRectFn = *reinterpret_cast<tPtInRect*>(MG_CONST(addr::msvc::PtInRect));

			switch (type)
			{
			case WM_MOUSEMOVE: // 512
			{
				*reinterpret_cast<u32*>(_this + 0x7AC) = 0; // 1964
				u32 slotCount = mg::call<tGetSlotCount>(
					MG_CONST(addr::ui::rightclick::GetSlotCount),
					reinterpret_cast<u32*>(_this + 0x7B4));
				for (u32 i = 0; i < slotCount; ++i)
				{
					u32 slot = *mg::call<tGetSlotAt>(
						MG_CONST(addr::ui::rightclick::GetSlotAt),
						reinterpret_cast<u32*>(_this + 0x7B4), i);
					if (PtInRectFn(reinterpret_cast<const RECT*>(slot + 0xC), pt))
					{
						if (*reinterpret_cast<u32*>(slot + 0x20))
						{
							if (*reinterpret_cast<u32*>(slot) != 1)
							{
								*reinterpret_cast<u32*>(slot) = 1;
								auto notifyTarget = *reinterpret_cast<void**>(_this + 0x5F4);
								mg::call<tSendSlotNotify>(
									MG_CONST(addr::ui::rightclick::SendSlotNotify),
									notifyTarget, 0x804u, 1u, _this);
								u32 handler = mg::call<tGetSlotHandler>(
									MG_CONST(addr::ui::rightclick::GetSlotHandler));
								u32 slotId = mg::call<tGetSlotFromInput>(
									MG_CONST(addr::ui::rightclick::GetSlotFromInput),
									reinterpret_cast<void*>(_this), 3);
								mg::callVFunc(reinterpret_cast<void*>(handler), 8, slotId);
							}
						}
						*reinterpret_cast<u32*>(_this + 0x7AC) = slot;
					}
					else
					{
						*reinterpret_cast<u32*>(slot) = 0;
					}
				}
				break;
			}

			case WM_LBUTTONDOWN: // 513
			{
				if (!*reinterpret_cast<u8*>(_this + 0x36F)) // 879
				{
					auto captureTarget = *reinterpret_cast<void**>(_this + 0x5F4);
					mg::call<tReleaseSlotCapture>(
						MG_CONST(addr::ui::rightclick::ReleaseSlotCapture),
						captureTarget, _this);
				}

				u32 slotCount = mg::call<tGetSlotCount>(
					MG_CONST(addr::ui::rightclick::GetSlotCount),
					reinterpret_cast<u32*>(_this + 0x7B4));
				for (u32 j = 0; j < slotCount; ++j)
				{
					u32 slot = *mg::call<tGetSlotAt>(
						MG_CONST(addr::ui::rightclick::GetSlotAt),
						reinterpret_cast<u32*>(_this + 0x7B4), j);
					if (slot && PtInRectFn(reinterpret_cast<const RECT*>(slot + 0xC), pt))
					{
						*reinterpret_cast<u8*>(slot + 8) = 1;
						*reinterpret_cast<u32*>(
							MG_CONST(addr::ui::rightclick::ActiveSlotPtr)) = slot;
						*reinterpret_cast<u32*>(slot + 0x4DF4) = _this;

						void* captureTarget = *reinterpret_cast<void**>(_this + 0x5F4);
						int captureParam = *reinterpret_cast<int*>(_this + 0x814);
						mg::call<tSetSlotCapture>(
							MG_CONST(addr::ui::rightclick::SetSlotCapture),
							captureTarget, captureParam);

						if (*reinterpret_cast<u32*>(slot + 0x20))
						{
							*reinterpret_cast<u32*>(_this + 0x810) = j;
							*reinterpret_cast<u32*>(
								MG_CONST(addr::ui::rightclick::SomeValue)) = slot;
						}

						auto notifyTarget = *reinterpret_cast<void**>(_this + 0x5F4);
						mg::call<tSendSlotNotify>(
							MG_CONST(addr::ui::rightclick::SendSlotNotify),
							notifyTarget, 0x801u, 1u, _this);

						u32 handler = mg::call<tGetSlotHandler>(
							MG_CONST(addr::ui::rightclick::GetSlotHandler));
						u32 slotId = mg::call<tGetSlotFromInput>(
							MG_CONST(addr::ui::rightclick::GetSlotFromInput),
							reinterpret_cast<void*>(_this), 0);
						mg::callVFunc(reinterpret_cast<void*>(handler), 8, slotId);
					}
				}
				break;
			}

			case WM_LBUTTONUP: // 514
			{
				u32 activeSlot = *reinterpret_cast<u32*>(
					MG_CONST(addr::ui::rightclick::ActiveSlotPtr));

				if (activeSlot && *reinterpret_cast<u8*>(activeSlot + 8))
				{
					*reinterpret_cast<u8*>(activeSlot + 8) = 0;

					if (activeSlot != *reinterpret_cast<u32*>(_this + 0x7AC))
					{
						if (*reinterpret_cast<u32*>(activeSlot + 0x4DF4) == _this)
						{
							auto notifyTarget = *reinterpret_cast<void**>(_this + 0x5F4);
							mg::call<tSendSlotNotify>(
								MG_CONST(addr::ui::rightclick::SendSlotNotify),
								notifyTarget, 0x806u, 1u, _this);

							u32 handler = mg::call<tGetSlotHandler>(
								MG_CONST(addr::ui::rightclick::GetSlotHandler));
							u32 slotId = mg::call<tGetSlotFromInput>(
								MG_CONST(addr::ui::rightclick::GetSlotFromInput),
								reinterpret_cast<void*>(_this), 5);
							mg::callVFunc(reinterpret_cast<void*>(handler), 8, slotId);
						}
						else
						{
							auto notifyTarget = *reinterpret_cast<void**>(_this + 0x5F4);
							mg::call<tSendSlotNotify>(
								MG_CONST(addr::ui::rightclick::SendSlotNotify),
								notifyTarget, 0x803u, 1u, _this);

							u32 handler = mg::call<tGetSlotHandler>(
								MG_CONST(addr::ui::rightclick::GetSlotHandler));
							u32 slotId = mg::call<tGetSlotFromInput>(
								MG_CONST(addr::ui::rightclick::GetSlotFromInput),
								reinterpret_cast<void*>(_this), 2);
							mg::callVFunc(reinterpret_cast<void*>(handler), 8, slotId);
						}
						*reinterpret_cast<u32*>(activeSlot + 0x4DF4) = 0;
						*reinterpret_cast<u32*>(
							MG_CONST(addr::ui::rightclick::ActiveSlotPtr)) = 0;
					}
				}
				break;
			}

			case WM_LBUTTONDBLCLK: // 515
			{
				u32 slotCount = mg::call<tGetSlotCount>(
					MG_CONST(addr::ui::rightclick::GetSlotCount),
					reinterpret_cast<u32*>(_this + 0x7B4));
				for (u32 k = 0; k < slotCount; ++k)
				{
					u32 slot = *mg::call<tGetSlotAt>(
						MG_CONST(addr::ui::rightclick::GetSlotAt),
						reinterpret_cast<u32*>(_this + 0x7B4), k);
					if (PtInRectFn(reinterpret_cast<const RECT*>(slot + 0xC), pt))
					{
						*reinterpret_cast<u32*>(
							MG_CONST(addr::ui::rightclick::ActiveSlotPtr)) = slot;
						*reinterpret_cast<u32*>(slot + 0x4DF4) = _this;
						*reinterpret_cast<float*>(slot + 0x1C) = 1.0f;

						auto notifyTarget = *reinterpret_cast<void**>(_this + 0x5F4);
						mg::call<tSendSlotNotify>(
							MG_CONST(addr::ui::rightclick::SendSlotNotify),
							notifyTarget, 0x802u, 1u, _this);

						u32 handler = mg::call<tGetSlotHandler>(
							MG_CONST(addr::ui::rightclick::GetSlotHandler));
						u32 slotId = mg::call<tGetSlotFromInput>(
							MG_CONST(addr::ui::rightclick::GetSlotFromInput),
							reinterpret_cast<void*>(_this), 1);
						mg::callVFunc(reinterpret_cast<void*>(handler), 8, slotId);
						*reinterpret_cast<u32*>(
							MG_CONST(addr::ui::rightclick::ActiveSlotPtr)) = 0;
						return;
					}
				}
				break;
			}

			case WM_RBUTTONDOWN: // 516
			{
					if (!*reinterpret_cast<u8*>(_this + 0x36F)) // 879
					{
						auto captureTarget = *reinterpret_cast<void**>(_this + 0x5F4); // 1524
						mg::call<tReleaseSlotCapture>(
							MG_CONST(addr::ui::rightclick::ReleaseSlotCapture),
							captureTarget, _this);
					}

					u32 slotCount = mg::call<tGetSlotCount>(
						MG_CONST(addr::ui::rightclick::GetSlotCount),
						reinterpret_cast<u32*>(_this + 0x7B4)); // 1972
					for (u32 m = 0; m < slotCount; ++m)
					{
						u32 slot = *mg::call<tGetSlotAt>(
							MG_CONST(addr::ui::rightclick::GetSlotAt),
							reinterpret_cast<u32*>(_this + 0x7B4), m); // 1972
						if (slot && PtInRectFn(reinterpret_cast<const RECT*>(slot + 0xC), pt))
						{
							*reinterpret_cast<u8*>(slot + 8) = 1;

							u32 activeSlot = *reinterpret_cast<u32*>(
								MG_CONST(addr::ui::rightclick::ActiveSlotPtr));
							if (activeSlot && activeSlot != slot)
							{
								*reinterpret_cast<u8*>(activeSlot + 8) = 0;
							}
								
							*reinterpret_cast<u32*>(
								MG_CONST(addr::ui::rightclick::ActiveSlotPtr)) = slot;
							*reinterpret_cast<u32*>(slot + 0x4DF4) = _this; // 19956

							void* captureTarget = *reinterpret_cast<void**>(_this + 0x5F4); // 1524
							int captureParam = *reinterpret_cast<int*>(_this + 0x814); // 2068
							mg::call<tSetSlotCapture>(
								MG_CONST(addr::ui::rightclick::SetSlotCapture),
								captureTarget, captureParam);

							if (*reinterpret_cast<u32*>(slot + 0x20)) // +32
							{
								*reinterpret_cast<u32*>(_this + 0x810) = m; // 2064
								*reinterpret_cast<u32*>(
									MG_CONST(addr::ui::rightclick::ActiveSlotPtr)) = slot;
							}

							auto notifyTarget = *reinterpret_cast<void**>(_this + 0x5F4); // 1524
							mg::call<tSendSlotNotify>(
								MG_CONST(addr::ui::rightclick::SendSlotNotify),
								notifyTarget, 0x807u, 1u, _this);
							u32 handler = mg::call<tGetSlotHandler>(
								MG_CONST(addr::ui::rightclick::GetSlotHandler));

							u32 slotId = mg::call<tGetSlotFromInput>(
								MG_CONST(addr::ui::rightclick::GetSlotFromInput),
								reinterpret_cast<void*>(_this), 0);
							mg::callVFunc(reinterpret_cast<void*>(handler), 8, slotId);
						}
					}
				break;
			}

			case WM_RBUTTONUP: // 517
			{
					u32 activeSlot = *reinterpret_cast<u32*>(
						MG_CONST(addr::ui::rightclick::ActiveSlotPtr));

					if (activeSlot && *reinterpret_cast<u8*>(activeSlot + 8))
					{
						*reinterpret_cast<u8*>(activeSlot + 8) = 0;

						if (activeSlot == *reinterpret_cast<u32*>(_this + 0x7AC )) // 0x7AC
						{
							auto target = *reinterpret_cast<void**>(_this + 0x5F4); // 0x5F4

							mg::call<tSendSlotNotify>(
								MG_CONST(addr::ui::rightclick::SendSlotNotify),
								target, 0x808u, 1u, _this);

							u32 handler = mg::call<tGetSlotHandler>(
								MG_CONST(addr::ui::rightclick::GetSlotHandler));

							u32 slotId = mg::call<tGetSlotFromInput>(
								MG_CONST(addr::ui::rightclick::GetSlotFromInput),
								reinterpret_cast<void*>(_this), 0);

							// vtable[8](handler, slotId) — offset 0x20 = index 8
							mg::callVFunc(reinterpret_cast<void*>(handler), 8, slotId);
						}

						*reinterpret_cast<u32*>(activeSlot + 0x4DF4) = 0; // 19956
						*reinterpret_cast<u32*>(
							MG_CONST(addr::ui::rightclick::ActiveSlotPtr)) = 0;
					}
				break;
			}

			case WM_MOUSEWHEEL: // 522
			{
				if (!*reinterpret_cast<u8*>(_this + 0x818)
					&& *reinterpret_cast<u32*>(_this + 0x81C))
				{
					mg::call<tScrollHandler>(
						MG_CONST(addr::ui::rightclick::ScrollHandler),
						reinterpret_cast<void*>(*reinterpret_cast<u32*>(_this + 0x81C)),
						static_cast<int>(idk));
				}
				break;
			}

			default:
				break;
			}
		}
	}

// ── DlgRightClick class ─────────────────────────────────────────────────────

DlgRightClick::DlgRightClick(MegaGuardContext& ctx) : ctx_(ctx) {}
DlgRightClick::~DlgRightClick() = default;

VoidResult DlgRightClick::install() {
	auto& registry = ctx_.hookRegistry();

	registry.registerDetour(HookId::DlgRightClick)
		.create(MG_CONST(addr::ui::rightclick::HandleIds), hkHandleRightClickIds);

	registry.registerDetour(HookId::DlgRightClickInput)
		.create(MG_CONST(addr::ui::rightclick::HandleInput), hkHandleRightClickInput);

	return VoidResult::ok();
}

} // namespace mg::game
