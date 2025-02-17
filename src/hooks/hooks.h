#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace Features
        {
            extern midhook::Hook HideWeaponSlot;

            extern SwapAddressPatch Tickrate_Frametime1;
            extern SwapAddressPatch Tickrate_Frametime2;
            extern SwapAddressPatch Tickrate_Frametime3;
            extern SwapAddressPatch Tickrate_Frametime4;

            extern SwapAddressPatch Tickrate_RotDamp1;
            extern SwapAddressPatch Tickrate_RotDamp2;
            extern SwapAddressPatch Tickrate_RotDamp3;

            extern SwapAddressPatch Tickrate_MinRotSpeed1;
            extern SwapAddressPatch Tickrate_MinRotSpeed2;
            extern SwapAddressPatch Tickrate_MinRotSpeed3;
            extern SwapAddressPatch Tickrate_MinRotSpeed4;
            extern SwapAddressPatch Tickrate_MinRotSpeed5;
            extern SwapAddressPatch Tickrate_MinRotSpeed6;

            extern SwapAddressPatch Tickrate_RotThreeshold1;
            extern SwapAddressPatch Tickrate_RotThreeshold2;
            extern SwapAddressPatch Tickrate_RotThreeshold3;//don't use
            extern SwapAddressPatch Tickrate_RotThreeshold4;
            extern SwapAddressPatch Tickrate_RotThreeshold5;
            extern SwapAddressPatch Tickrate_RotThreeshold6;
            extern SwapAddressPatch Tickrate_RotThreeshold7;

            extern SwapAddressPatch Tickrate_RotThreesholdLimit;

            extern SwapAddressPatch Tickrate_MaxRotSpeed1;
            extern SwapAddressPatch Tickrate_MaxRotSpeed2;
            extern SwapAddressPatch Tickrate_MaxRotSpeed3;
            extern SwapAddressPatch Tickrate_MaxRotSpeed4;
            extern SwapAddressPatch Tickrate_MaxRotSpeed5;
            extern SwapAddressPatch Tickrate_MaxRotSpeed6;

            extern SwapAddressPatch Tickrate_DelayReq1;
            extern SwapAddressPatch Tickrate_DelayReq2;
            extern SwapAddressPatch Tickrate_DelayReq3;
            extern SwapAddressPatch Tickrate_DelayReq4;
            extern SwapAddressPatch Tickrate_DelayReq5;
            extern SwapAddressPatch Tickrate_DelayReq6;
            extern SwapAddressPatch Tickrate_DelayReq7;
            extern SwapAddressPatch Tickrate_DelayReq8;
            extern SwapAddressPatch Tickrate_DelayReq9;
            extern SwapAddressPatch Tickrate_DelayReq10;
            extern SwapAddressPatch Tickrate_DelayReq11;
            extern SwapAddressPatch Tickrate_DelayReq12;
            extern SwapAddressPatch Tickrate_DelayReq13;
            extern SwapAddressPatch Tickrate_DelayReq14;
            extern SwapAddressPatch Tickrate_DelayReq15;
            extern SwapAddressPatch Tickrate_DelayReq16;
            extern SwapAddressPatch Tickrate_DelayReq17;
            extern SwapAddressPatch Tickrate_DelayReq18;
            extern SwapAddressPatch Tickrate_DelayReq19;
            extern SwapAddressPatch Tickrate_DelayReq20;
            extern SwapAddressPatch Tickrate_DelayReq21;

            extern SwapAddressPatch Tickrate_MinDistance1;
            extern SwapAddressPatch Tickrate_MinDistance2;
            extern SwapAddressPatch Tickrate_MinDistance3;
            extern SwapAddressPatch Tickrate_MinDistance4;
            extern SwapAddressPatch Tickrate_MinDistance5;
            extern SwapAddressPatch Tickrate_MinDistance6;
            extern SwapAddressPatch Tickrate_MinDistance7;

            extern PatchBytes VoiceSpecialTypeA;
            extern PatchBytes VoiceSpecialTypeB;
            extern PatchBytes VoiceSpecialTypeC;
            extern PatchBytes VoiceSpecialTypeD;
            extern PatchBytes VoiceSpecialTypeE;
        }
        namespace BugFixes
        {
            extern std::unique_ptr<PLH::NatDetour> FixWeaponSelectDetour;
            extern std::unique_ptr<PLH::NatDetour> FixWeaponSelectDetour_Setting;
            extern std::unique_ptr<PLH::NatDetour> FixWeaponSelectDetour_Main;
            extern midhook::Hook NiSystemDesc_CPUID_CPUCount;
        }
        namespace AntiCheat
        {
            //extern std::uint32_t* __cdecl GetGRoom();
            //extern void __cdecl DestroyCRoom();
            namespace GameManagers
            {
                namespace Room
                {
                    extern std::unique_ptr<PLH::NatDetour> Get;
                    extern std::unique_ptr<PLH::NatDetour> Destroy;
                }
            }
        }
        extern void InitFeaturesHooks();
        extern void InitBugFixesHooks();
        extern void InitAntiCheatHooks();
        extern void RemoveFeaturesHooks();
        extern void RemoveBugFixesHooks();
        extern void RemoveAntiCheatHooks();
        extern void InitializeAllHooks();
        extern void RemoveAllHooks();
    }
}