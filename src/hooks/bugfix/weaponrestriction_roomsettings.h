#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace BugFixes
        {
		    inline void __fastcall RoomCreateDialogHandler(std::uint32_t crtRoomDlgInstance, std::uint32_t edx, std::uint32_t a2, std::uint32_t dialog_id, std::uint32_t a4)
			{
				auto original = PLH::FnCast(MegaGuard::Addresses::Hooks::Bugfixes::original_RoomCreateDialogHandler, &RoomCreateDialogHandler);

				if (a2 == 257)
				{
					switch (dialog_id)
					{
						case 107022:
						{
							auto mod_id = _rv<std::uint32_t>(_rv<std::uintptr_t>(crtRoomDlgInstance, 2060), 4);
							if (mod_id != 4 && mod_id != 7 && mod_id != 8 && mod_id != 11)
								_call<void(__thiscall*)(std::uint32_t, const char*, std::uint32_t, std::uint32_t)>(
									0x692430,
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
				auto original = PLH::FnCast(MegaGuard::Addresses::Hooks::Bugfixes::original_RoomSettingsDialogHandler, &RoomSettingDialogHandler);

				if (eventType == 257)
				{
					switch (dialogId)
					{
						case 103053:
						{
							auto mod_id = _rv<std::uint32_t>(roomSettingDlgInstance, 2064);
							if (mod_id != 4 && mod_id != 7 && mod_id != 8 && mod_id != 11)
								_call<void(__thiscall*)(std::uint32_t, const char*, std::uint32_t, std::uint32_t)>(
									0x66DDB0,
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
				auto original = PLH::FnCast(MegaGuard::Addresses::Hooks::Bugfixes::original_RoomMainDialogHandler, &RoomMainDialogHandler);

				bool checkVendorLobby = _call<std::uint32_t * (__thiscall*)(std::uint32_t, const char*)>(
					0xE61540,
					_rv<std::uint32_t>(0x011DE354, 0),
					"E_DLG_VENDOR_LOBBY"
				);

				if (!checkVendorLobby && eventType == 257)
				{
					switch (dialogId)
					{
						case 101069:
						{
							auto mod_id = Memory::CallVFunc<std::uint32_t>(_call<std::uint32_t * (__cdecl*)()>(MegaGuard::Addresses::GameManagers::Room), 15);
							if (mod_id != 4 && mod_id != 7 && mod_id != 8 && mod_id != 11)
								_call<void(__thiscall*)(std::uint32_t, const char*, std::uint32_t, std::uint32_t)>(
									0x6609C0,
									roomSettingDlgInstance,
									"E_DLG_Q_MODE_SEL",
									8,
									_rv<std::uint32_t>(_call<std::uint32_t(__cdecl*)()>(MegaGuard::Addresses::GameManagers::Room), 236)
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