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

                if (status != MH_OK)
                {
                    MegaGuard::EventLog->Debug(nostd::source_location::current(),
                        "MH_CreateHook failed: {} at {:#08X}", MH_StatusToString(status),
                        reinterpret_cast<std::uintptr_t>(pBaseFn));
                    return false;
                }
  
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
            extern SwapAddressPatch WidthBuff1;
            extern SwapAddressPatch WidthBuff2;
            extern SwapAddressPatch WidthBuff3;
            extern SwapAddressPatch WidthBuff4;
            extern SwapAddressPatch WidthBuff5;
            extern SwapAddressPatch WidthBuff6;
            extern SwapAddressPatch WidthBuff7;
            extern SwapAddressPatch WidthBuff8;
            extern SwapAddressPatch WidthBuff9;
            extern SwapAddressPatch WidthBuff10;
            extern SwapAddressPatch WidthBuff11;
            extern SwapAddressPatch WidthBuff12;
            extern SwapAddressPatch WidthBuff13;
            extern SwapAddressPatch WidthBuff14;
            extern SwapAddressPatch WidthBuff15;
            extern SwapAddressPatch WidthBuff16;
            extern SwapAddressPatch WidthBuff17;
            extern SwapAddressPatch WidthBuff18;
            extern SwapAddressPatch WidthBuff19;
            extern SwapAddressPatch WidthBuff20;

            extern SwapAddressPatch HeightBuff1;
            extern SwapAddressPatch HeightBuff2;
            extern SwapAddressPatch HeightBuff3;
            extern SwapAddressPatch HeightBuff4;
            extern SwapAddressPatch HeightBuff5;
            extern SwapAddressPatch HeightBuff6;
            extern SwapAddressPatch HeightBuff7;
            extern SwapAddressPatch HeightBuff8;
            extern SwapAddressPatch HeightBuff9;
            extern SwapAddressPatch HeightBuff10;
            extern SwapAddressPatch HeightBuff11;
            extern SwapAddressPatch HeightBuff12;
            extern SwapAddressPatch HeightBuff13;
            extern SwapAddressPatch HeightBuff14;
            extern SwapAddressPatch HeightBuff15;
            extern SwapAddressPatch HeightBuff16;
            extern SwapAddressPatch HeightBuff17;
            extern SwapAddressPatch HeightBuff18;
            extern SwapAddressPatch HeightBuff19;

            extern SwapAddressPatch AspectRatioIds1;
            extern SwapAddressPatch AspectRatioIds2;
            extern SwapAddressPatch AspectRatioIds3;
            extern SwapAddressPatch AspectRatioIds4;
            extern SwapAddressPatch AspectRatioIds5;
            extern SwapAddressPatch AspectRatioIds6;
            extern SwapAddressPatch AspectRatioIds7;
            extern SwapAddressPatch AspectRatioIds8;
            extern SwapAddressPatch AspectRatioIds9;
            extern SwapAddressPatch AspectRatioIds10;
            extern SwapAddressPatch AspectRatioIds11;
            extern SwapAddressPatch AspectRatioIds12;
            extern SwapAddressPatch AspectRatioIds13;
            extern SwapAddressPatch AspectRatioIds14;
            extern SwapAddressPatch AspectRatioIds15;

            extern PatchBytes ResolutionListSize1;
            extern PatchBytes ResolutionListSize2;
            extern PatchBytes ResolutionListSize3;
            extern PatchBytes ResolutionListSize4;
            extern PatchBytes ResolutionListSize5;
            extern PatchBytes ResolutionListSize6;
            extern CDetourHook SetAspectRatioScale;
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
            namespace AckHandlers
            {
                extern CDetourHook NetworkInitCrypto;
            }
            namespace ReqHandlers
            {
                extern CDetourHook MainAuthorize;
            }
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
            namespace CDBM
            {
               extern CDetourHook Load;
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