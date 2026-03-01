#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace Features
        {
			inline bool first_time_lobby = true;
			inline void __fastcall Common_Agora_Init_DLG(std::uint32_t instance, std::uint32_t edx)
			{
				auto original = MegaGuard::HooksMgr::Features::CommonAgoraDlgInit.GetOriginal<decltype(&Common_Agora_Init_DLG)>();
				original(instance, edx);

				auto pcroom_1 = _call<std::uint32_t(*)()>(MegaGuard::Addresses::Hooks::Features::DLG::CFactoryGet.get());
				_call<void(__thiscall*)(std::uint32_t, std::uint32_t, void*)>(MegaGuard::Addresses::Hooks::Features::DLG::GetDlgById.get(), pcroom_1, 101075, reinterpret_cast<void*>(MegaGuard::Addresses::Hooks::Features::DLG::CExPICBaseAlloc.get()));
				auto pcroom_2 = _call<std::uint32_t(*)()>(MegaGuard::Addresses::Hooks::Features::DLG::CFactoryGet.get());
				auto pcroom_3 = _call<std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t)>(MegaGuard::Addresses::Hooks::Features::DLG::GetDlgId.get(), pcroom_2, 101075);

				
				_call<void(__thiscall*)(std::uint32_t, const char*, std::uint32_t)>(MegaGuard::Addresses::Hooks::Features::DLG::AssignDlgInfo.get(), instance, "E_DLG_AGORA_MENU_PIC_PCROOM_ICON", pcroom_3);
			}

            inline void __fastcall Common_Agora_Construct_DLG(std::uint32_t instance, std::uint32_t edx)
            {
                static auto original = MegaGuard::HooksMgr::Features::CommonAgoraDlgConstruct.GetOriginal<decltype(&Common_Agora_Construct_DLG)>();
                original(instance, edx);
				std::uint32_t pcroom_id_1 = 101075;
				auto pcroom_result_1 = _call<std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t*)>(MegaGuard::Addresses::Hooks::Features::DLG::GetDlgInfo.get(), (instance + 1712), &pcroom_id_1);
				_wv(
					instance, 2248,
					_rv<std::uintptr_t>(pcroom_result_1, 0)
				);

				std::uint32_t pcroom_id_2 = 101076;
				auto pcroom_result_2 = _call<std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t*)>(MegaGuard::Addresses::Hooks::Features::DLG::GetDlgInfo.get(), (instance + 1712), &pcroom_id_2);
				_wv(
					instance, 2252,
					_rv<std::uintptr_t>(pcroom_result_2, 0)
				);
				std::uint32_t pcroom_id_3 = 101077;
				auto pcroom_result_3 = _call < std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t*) >(MegaGuard::Addresses::Hooks::Features::DLG::GetDlgInfo.get(), (instance + 1712), &pcroom_id_3);
				_wv(
					instance, 2256,
					_rv<std::uintptr_t>(pcroom_result_3, 0)
				);
				if (first_time_lobby) 
				{
					_call<void(__thiscall*)(std::uint32_t)>(MegaGuard::Addresses::Hooks::Features::InitPcBangInfo.get(), instance);
					first_time_lobby = false;
				}

            }
        }
    }
}