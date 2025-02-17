#include "../pch.h"
#include "features/hideweaponslots.h"
#include "features/custom_tickrate.h"
#include "features/voice_types_all.h"

#include "bugfix/weaponrestriction_roomsettings.h"
#include "bugfix/intel_hyperthreading.h"

#include "anticheat/gamemanagers.h"

namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace Features
        {
            midhook::Hook HideWeaponSlot;

            SwapAddressPatch Tickrate_Frametime1;
            SwapAddressPatch Tickrate_Frametime2;
            SwapAddressPatch Tickrate_Frametime3;
            SwapAddressPatch Tickrate_Frametime4;

            SwapAddressPatch Tickrate_RotDamp1;
            SwapAddressPatch Tickrate_RotDamp2;
            SwapAddressPatch Tickrate_RotDamp3;

            SwapAddressPatch Tickrate_MinRotSpeed1;
            SwapAddressPatch Tickrate_MinRotSpeed2;
            SwapAddressPatch Tickrate_MinRotSpeed3;
            SwapAddressPatch Tickrate_MinRotSpeed4;
            SwapAddressPatch Tickrate_MinRotSpeed5;
            SwapAddressPatch Tickrate_MinRotSpeed6;

            SwapAddressPatch Tickrate_RotThreeshold1;
            SwapAddressPatch Tickrate_RotThreeshold2;
            SwapAddressPatch Tickrate_RotThreeshold3;//don't use
            SwapAddressPatch Tickrate_RotThreeshold4;
            SwapAddressPatch Tickrate_RotThreeshold5;
            SwapAddressPatch Tickrate_RotThreeshold6;
            SwapAddressPatch Tickrate_RotThreeshold7;

            SwapAddressPatch Tickrate_RotThreesholdLimit;

            SwapAddressPatch Tickrate_MaxRotSpeed1;
            SwapAddressPatch Tickrate_MaxRotSpeed2;
            SwapAddressPatch Tickrate_MaxRotSpeed3;
            SwapAddressPatch Tickrate_MaxRotSpeed4;
            SwapAddressPatch Tickrate_MaxRotSpeed5;
            SwapAddressPatch Tickrate_MaxRotSpeed6;

            SwapAddressPatch Tickrate_DelayReq1;
            SwapAddressPatch Tickrate_DelayReq2;
            SwapAddressPatch Tickrate_DelayReq3;
            SwapAddressPatch Tickrate_DelayReq4;
            SwapAddressPatch Tickrate_DelayReq5;
            SwapAddressPatch Tickrate_DelayReq6;
            SwapAddressPatch Tickrate_DelayReq7;
            SwapAddressPatch Tickrate_DelayReq8;
            SwapAddressPatch Tickrate_DelayReq9;
            SwapAddressPatch Tickrate_DelayReq10;
            SwapAddressPatch Tickrate_DelayReq11;
            SwapAddressPatch Tickrate_DelayReq12;
            SwapAddressPatch Tickrate_DelayReq13;
            SwapAddressPatch Tickrate_DelayReq14;
            SwapAddressPatch Tickrate_DelayReq15;
            SwapAddressPatch Tickrate_DelayReq16;
            SwapAddressPatch Tickrate_DelayReq17;
            SwapAddressPatch Tickrate_DelayReq18;
            SwapAddressPatch Tickrate_DelayReq19;
            SwapAddressPatch Tickrate_DelayReq20;
            SwapAddressPatch Tickrate_DelayReq21;

            SwapAddressPatch Tickrate_MinDistance1;
            SwapAddressPatch Tickrate_MinDistance2;
            SwapAddressPatch Tickrate_MinDistance3;
            SwapAddressPatch Tickrate_MinDistance4;
            SwapAddressPatch Tickrate_MinDistance5;
            SwapAddressPatch Tickrate_MinDistance6;
            SwapAddressPatch Tickrate_MinDistance7;

            PatchBytes VoiceSpecialTypeA;
            PatchBytes VoiceSpecialTypeB;
            PatchBytes VoiceSpecialTypeC;
            PatchBytes VoiceSpecialTypeD;
            PatchBytes VoiceSpecialTypeE;
        }
        namespace BugFixes
        {
            std::unique_ptr<PLH::NatDetour> FixWeaponSelectDetour;
            std::unique_ptr<PLH::NatDetour> FixWeaponSelectDetour_Setting;
            std::unique_ptr<PLH::NatDetour> FixWeaponSelectDetour_Main;
            midhook::Hook NiSystemDesc_CPUID_CPUCount;
        }
        namespace AntiCheat
        {
            namespace GameManagers
            {
                namespace Room
                {
                    std::unique_ptr<PLH::NatDetour> Get;
                    std::unique_ptr<PLH::NatDetour> Destroy;
                }
            }
        }
        inline void InitFeaturesHooks()
        {
            Features::HideWeaponSlot.Create(MegaGuard::Addresses::Hooks::Features::HideWeaponSlot, Features::HideWeaponSlots);
            Features::CustomTickratePatch();
            Features::CustomVoiceTypesPatch();
        }
        inline void InitBugFixesHooks()
        {
           // BugFixes::NiSystemDesc_CPUID_CPUCount.Create(MegaGuard::Addresses::Hooks::Bugfixes::NiSystemDesc_CPUID_CPUCount, BugFixes::CPUID_CPUCount, false);
            
            BugFixes::FixWeaponSelectDetour = std::make_unique<PLH::NatDetour>(
                MegaGuard::Addresses::Hooks::Bugfixes::RoomCreateDialogHandler, 
                reinterpret_cast<std::uint64_t>(BugFixes::RoomCreateDialogHandler),                             
                &MegaGuard::Addresses::Hooks::Bugfixes::original_RoomCreateDialogHandler    
            );
            BugFixes::FixWeaponSelectDetour->hook();
            
            BugFixes::FixWeaponSelectDetour_Setting = std::make_unique<PLH::NatDetour>(
                MegaGuard::Addresses::Hooks::Bugfixes::RoomSettingsDialogHandler,
                reinterpret_cast<std::uint64_t>(BugFixes::RoomSettingDialogHandler),
                &MegaGuard::Addresses::Hooks::Bugfixes::original_RoomSettingsDialogHandler
            );
            BugFixes::FixWeaponSelectDetour_Setting->hook();
            
            BugFixes::FixWeaponSelectDetour_Main = std::make_unique<PLH::NatDetour>(
                MegaGuard::Addresses::Hooks::Bugfixes::RoomMainDialogHandler,
                reinterpret_cast<std::uint64_t>(BugFixes::RoomMainDialogHandler),
                &MegaGuard::Addresses::Hooks::Bugfixes::original_RoomMainDialogHandler
            );
            BugFixes::FixWeaponSelectDetour_Main->hook();
            
        }
        inline void InitAntiCheatHooks()
        {
            InitializeCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);
            
            AntiCheat::GameManagers::Room::Get = std::make_unique<PLH::NatDetour>(
                MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::GetCRoom,
                reinterpret_cast<std::uint64_t>(AntiCheat::GetGRoom),
                &MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::original_GetCRoom
            );
            AntiCheat::GameManagers::Room::Get->hook();

            AntiCheat::GameManagers::Room::Destroy = std::make_unique<PLH::NatDetour>(
                MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::DestroyCRoom,
                reinterpret_cast<std::uint64_t>(AntiCheat::DestroyCRoom),
                &MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::original_DestroyCRoom
            );
            
            AntiCheat::GameManagers::Room::Destroy->hook();
        }
        inline void RemoveFeaturesHooks()
        {
            Features::HideWeaponSlot.Remove();
            Features::CustomTickrateUnpatch();
            Features::CustomVoiceTypesUnpatch();
            

        }
        inline void RemoveBugFixesHooks()
        {
            BugFixes::FixWeaponSelectDetour->unHook();
            BugFixes::FixWeaponSelectDetour_Setting->unHook();
            BugFixes::FixWeaponSelectDetour_Main->unHook();
        }
        inline void RemoveAntiCheatHooks()
        {
            AntiCheat::GameManagers::Room::Get->unHook();
            AntiCheat::GameManagers::Room::Destroy->unHook();
            DeleteCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);
        }

        [[clang::annotate("x-vm,x-full,x-cfg,ind-br,alias-access,custom-cc")]]
        inline void InitializeAllHooks()
        {
            
            //VMProtectBeginUltra("InitializeAllHooks_vmp");
            InitFeaturesHooks();
            InitBugFixesHooks();
            InitAntiCheatHooks();
           // VMProtectEnd();
        }
        [[clang::annotate("x-vm,x-full,x-cfg,ind-br,alias-access,custom-cc")]]
        inline void RemoveAllHooks()
        {
            //VMProtectBeginUltra("RemoveAllHooks_vmp");
            RemoveFeaturesHooks();
            RemoveBugFixesHooks();
            RemoveAntiCheatHooks();
            //VMProtectEnd();
        }
    }
}