#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace BugFixes
        {
			inline auto idk1 = enc_ptr(0x692430);
			inline auto idk2 = enc_ptr(0x66DDB0);
			inline auto idk3 = enc_ptr(0xE61540);
			inline auto idk4 = enc_ptr(0x011DE354);
			inline auto idk5 = enc_ptr(0x6609C0);
		    inline void __fastcall RoomCreateDialogHandler(std::uint32_t crtRoomDlgInstance, std::uint32_t edx, std::uint32_t a2, std::uint32_t dialog_id, std::uint32_t a4)
			{
				static auto original = MegaGuard::HooksMgr::BugFixes::FixWeaponSelectDetour.GetOriginal<decltype(&RoomCreateDialogHandler)>();

				if (a2 == 257)
				{
					switch (dialog_id)
					{
						case 107022:
						{
							auto mod_id = _rv<std::uint32_t>(_rv<std::uintptr_t>(crtRoomDlgInstance, 2060), 4);
							if (mod_id != 4 && mod_id != 7 && mod_id != 8 && mod_id != 11)
								_call<void(__thiscall*)(std::uint32_t, const char*, std::uint32_t, std::uint32_t)>(
									idk1.get(),
									crtRoomDlgInstance,
									"E_DLG_Q_MODE_SEL",
									8,
									_rv<std::uint32_t>(_rv<std::uintptr_t>(crtRoomDlgInstance, 2060), 36)// weapon restriction id
								);
							break;
						}
					}
				}
				original(crtRoomDlgInstance, edx, a2, dialog_id, a4);
			}

			inline void __fastcall RoomSettingDialogHandler(std::uint32_t roomSettingDlgInstance, std::uint32_t edx, std::uint32_t eventType, std::uint32_t dialogId, std::uint32_t a4)
			{
				static auto original = MegaGuard::HooksMgr::BugFixes::FixWeaponSelectDetour_Setting.GetOriginal<decltype(&RoomSettingDialogHandler)>();

				if (eventType == 257)
				{
					switch (dialogId)
					{
						case 103053:
						{
							auto mod_id = _rv<std::uint32_t>(roomSettingDlgInstance, 2064);
							if (mod_id != 4 && mod_id != 7 && mod_id != 8 && mod_id != 11)
								_call<void(__thiscall*)(std::uint32_t, const char*, std::uint32_t, std::uint32_t)>(
									idk2.get(),
									roomSettingDlgInstance,
									"E_DLG_Q_MODE_SEL",
									8,
									_rv<std::uint32_t>(roomSettingDlgInstance, 2060)
								);
							break;
						}
					}
				}
				original(roomSettingDlgInstance, edx, eventType, dialogId, a4);
			}

			inline void __fastcall RoomMainDialogHandler(std::uint32_t roomSettingDlgInstance, std::uint32_t edx, std::uint32_t eventType, std::uint32_t dialogId, std::uint32_t a4)
			{  

				static auto original = MegaGuard::HooksMgr::BugFixes::FixWeaponSelectDetour_Main.GetOriginal<decltype(&RoomMainDialogHandler)>();

				bool checkVendorLobby = _call<std::uint32_t*(__thiscall*)(std::uint32_t, const char*)>(
					idk3.get(),
					_rv<std::uint32_t>(idk4.get(), 0),
					"E_DLG_VENDOR_LOBBY"
				);

				if (!checkVendorLobby && eventType == 257)
				{
					switch (dialogId)
					{
						case 101069:
						{
							auto mod_id = Memory::CallVFunc<std::uint32_t>(MegaGuard::HooksMgr::AntiCheat::GetCRoom(), 15);
							if (mod_id != 4 && mod_id != 7 && mod_id != 8 && mod_id != 11)
								_call<void(__thiscall*)(std::uint32_t, const char*, std::uint32_t, std::uint32_t)>(
									idk5.get(),
									roomSettingDlgInstance,
									"E_DLG_Q_MODE_SEL",
									8,
									_rv<std::uint32_t>(reinterpret_cast<std::uint32_t>(MegaGuard::HooksMgr::AntiCheat::GetCRoom()), 236)
								);
							break;
						}
					}
				}
				original(roomSettingDlgInstance, edx, eventType, dialogId, a4);
			}
        }
    }
}