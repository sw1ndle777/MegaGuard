#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        class CDetourHook
        {
        public:
            CDetourHook() = default;
            template <class T, class Fn>
            explicit CDetourHook(T pFunction, Fn pDetour)
                : pBaseFn(reinterpret_cast<void*>(pFunction)), pReplaceFn(reinterpret_cast<void*>(pDetour)) {
            }
            template <class T, class Fn>
            bool Create(T pFunction, Fn pDetour)
            {
                pBaseFn = reinterpret_cast<void*>(pFunction);

                if (pBaseFn == nullptr) return false;
                    
                pReplaceFn = reinterpret_cast<void*>(pDetour);

                if (pReplaceFn == nullptr)  return false;
                   
                const MH_STATUS status = MH_CreateHook(pBaseFn, pReplaceFn, &pOriginalFn);

                //if (status != MH_OK) throw std::runtime_error(std::format("failed to create hook function, status: {}\nbase function -> {:#08X}", MH_StatusToString(status), reinterpret_cast<std::uintptr_t>(pBaseFn)));
  
                if (!this->Replace())  return false;
                   
                return true;
            }
            bool Replace()
            {
                if (pBaseFn == nullptr || bIsHooked) return false;
                    
                const MH_STATUS status = MH_EnableHook(pBaseFn);

                //if (status != MH_OK) throw std::runtime_error(std::format("failed to enable hook function, status: {}\nbase function -> {:#08X} address", MH_StatusToString(status), reinterpret_cast<std::uintptr_t>(pBaseFn)));
                    
                bIsHooked = true;
                return true;
            }
            bool Remove()
            {
                if (!this->Restore()) return false;
                    
                const MH_STATUS status = MH_RemoveHook(pBaseFn);

                //if (status != MH_OK) throw std::runtime_error(std::format("failed to remove hook, status: {}\n base function -> {:#08X} address", MH_StatusToString(status), reinterpret_cast<std::uintptr_t>(pBaseFn)));
                    
                return true;
            }

            bool Restore()
            {
                if (!bIsHooked) return false;
                    
                const MH_STATUS status = MH_DisableHook(pBaseFn);

                //if (status != MH_OK) throw std::runtime_error(std::format("failed to restore hook, status: {}\n base function -> {:#08X} address", MH_StatusToString(status), reinterpret_cast<std::uintptr_t>(pBaseFn)));

                bIsHooked = false;
                return true;
            }
            template <typename Fn>
            Fn GetOriginal() { return static_cast<Fn>(pOriginalFn); }
            inline bool IsHooked() const { return bIsHooked; }

        private:
            bool bIsHooked = false;
            void* pBaseFn = nullptr;
            void* pReplaceFn = nullptr;
            void* pOriginalFn = nullptr;
        };

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

            extern SwapAddressPatch Tickrate_DelayAnims1;
            extern SwapAddressPatch Tickrate_DelayAnims2;
            extern SwapAddressPatch Tickrate_DelayAnims3;
            extern SwapAddressPatch Tickrate_DelayAnims4;
            extern SwapAddressPatch Tickrate_DelayAnims5;
            extern SwapAddressPatch Tickrate_DelayAnims6;
            extern SwapAddressPatch Tickrate_DelayAnims7;
            extern SwapAddressPatch Tickrate_DelayAnims8;
            extern SwapAddressPatch Tickrate_DelayAnims9;
            extern SwapAddressPatch Tickrate_DelayAnims10;
            extern SwapAddressPatch Tickrate_DelayAnims11;
            extern SwapAddressPatch Tickrate_DelayAnims12;
            extern SwapAddressPatch Tickrate_DelayAnims13;
            extern SwapAddressPatch Tickrate_DelayAnims14;
            extern SwapAddressPatch Tickrate_DelayAnims15;
            extern SwapAddressPatch Tickrate_DelayAnims16;
            extern SwapAddressPatch Tickrate_DelayAnims17;
            extern SwapAddressPatch Tickrate_DelayAnims18;



            extern SwapAddressPatch Tickrate_MinDistance1;
            extern SwapAddressPatch Tickrate_MinDistance2;
            extern SwapAddressPatch Tickrate_MinDistance3;
            extern SwapAddressPatch Tickrate_MinDistance4;
            extern SwapAddressPatch Tickrate_MinDistance5;
            extern SwapAddressPatch Tickrate_MinDistance6;
            extern SwapAddressPatch Tickrate_MinDistance7;

			extern PatchBytes Rot1_PatchBytes;
            extern PatchBytes Rot2_PatchBytes;
            extern PatchBytes Rot3_PatchBytes;

            extern CDetourHook NationIndex;
            extern CDetourHook WindowTitle;
            extern CDetourHook SpectatePov;
			extern CDetourHook DrawDebugInfo;

            extern CDetourHook CommonAgoraDlgInit;
            extern CDetourHook CommonAgoraDlgConstruct;
        }
        namespace BugFixes
        {
            extern CDetourHook FixWeaponSelectDetour;
            extern CDetourHook FixWeaponSelectDetour_Setting;
            extern CDetourHook FixWeaponSelectDetour_Main;
            extern CDetourHook Screeshot_bug;
            extern CDetourHook Screenshot2_bug;
            extern PatchBytes SetDateTimeShit;
        }
        namespace AntiCheat
        {
            namespace GameManagers
            {
                namespace Room
                {
                    extern CDetourHook Get;
                    extern CDetourHook Destroy;
                }
                namespace UnitContainer
                {
                    extern CDetourHook Get;
                    extern CDetourHook Destroy;
                }
                namespace UnitMgr
                {
                    extern CDetourHook Get;
                    extern CDetourHook Destroy;
                }
                namespace NetMgr
                {
                    extern CDetourHook Get;
                    extern CDetourHook Destroy;
                }
                namespace Dynamics
                {
                    extern CDetourHook Get;
                    extern CDetourHook Destroy;
                }
                namespace World
                {
                    extern CDetourHook Get;
                    extern CDetourHook Destroy;
                }
            }
            namespace InitDefaultSettings
            {
                extern CDetourHook Init;
            }
            namespace Crypto
            {
                namespace RC5
                {
                    extern CDetourHook KeySetup;
                }
                namespace RC6
                {
                    extern CDetourHook KeySetup;
                }
                extern CDetourHook Encrypt;
                extern CDetourHook Decrypt;
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