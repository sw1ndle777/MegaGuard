#include "../pch.h"

#include "anticheat/gamemanagers.h"
#include "anticheat/ccrypt.h"
#include "anticheat/heartbeat.h"
#include "anticheat/cdbm_load.h"
#include "anticheat/network_connect_ack.h"
#include "anticheat/front_main_authorize.h"

#include "features/hideweaponslots.h"
#include "features/custom_tickrate.h"
#include "features/spectate_pov.h"
#include "features/custom_nationindex.h"
#include "features/pcbang.h"
#include "features/resolutions.h"

#include "bugfix/weaponrestriction_roomsettings.h"
#include "bugfix/screenshot_bug.h"


#define ROTL16(x,y) ((uint16_t)((((uint16_t)(x))<<((y)&15)) | (((uint16_t)(x))>>(16-((y)&15)))))
#define ROTR16(x,y) ((uint16_t)((((uint16_t)(x))>>((y)&15)) | (((uint16_t)(x))<<(16-((y)&15)))))
#define ROTL32(x,y) ((uint32_t)((((uint32_t)(x))<<((y)&31)) | (((uint32_t)(x))>>(32-((y)&31)))))
#define ROTR32(x,y) ((uint32_t)((((uint32_t)(x))>>((y)&31)) | (((uint32_t)(x))<<(32-((y)&31)))))

namespace MegaGuard
{
    class CCrypt
    {
    public:
        uint32_t RC5S[26];
        uint32_t RC6S[84];
        int32_t UserKey;

        void RC5KeySetup()
        {
            
            uint32_t A, B, L[4];
            int i, j, k, l = 0;
            char UserKeyBytes[4];
            UserKeyBytes[0] = UserKey & 0xFF;
            UserKeyBytes[1] = UserKey >> 8 & 0xFF;
            UserKeyBytes[2] = UserKey >> 16 & 0xFF;
            UserKeyBytes[3] = UserKey >> 24 & 0xFF;
            for (i = 15, L[3] = 0; i >= 0; i--)
            {
                L[i / 4] = (L[i / 4] << 8) + MegaGuard::Addresses::Hooks::Anticheat::Crypto::RC5::K[i] + UserKeyBytes[l];
                if (++l > 3) l = 0;
            }
            for (RC5S[0] = 0x5163, i = 1; i < 26; i++)
                RC5S[i] = RC5S[i - 1] + 0x79b9;
            for (A = B = i = j = k = 0; k < 26 * 3; k++, i = (i + 26 / 2) % 26, j = (j + 1) % 4)
            {
                A = RC5S[i] = ROTL32(RC5S[i] + (A + B), 3);
                B = L[j] = ROTL32(L[j] + (A + B), (A + B));
            }
        }

        void RC6KeySetup()
        {

            
            uint32_t A, B, L[8];
            int i, j, k, l = 0;
            char UserKeyBytes[4];
            UserKeyBytes[0] = UserKey & 0xFF;
            UserKeyBytes[1] = UserKey >> 8 & 0xFF;
            UserKeyBytes[2] = UserKey >> 16 & 0xFF;
            UserKeyBytes[3] = UserKey >> 24 & 0xFF;
            for (i = 31, L[7] = 0; i >= 0; i--)
            {
                L[i / 4] = (L[i / 4] << 8) + MegaGuard::Addresses::Hooks::Anticheat::Crypto::RC6::K[i] + UserKeyBytes[l];
                if (++l > 3) l = 0;
            }
            for (RC6S[0] = 0xb7e15163, i = 1; i < 84; i++)
                RC6S[i] = RC6S[i - 1] + 0x9e3779b9;
            for (A = B = i = j = k = 0; k < 84 * 3; k++, i = (i + 1) % 84, j = (j + 1) % 8)
            {
                A = RC6S[i] = ROTL32(RC6S[i] + A + B, 3);
                B = L[j] = ROTL32(L[j] + A + B, A + B);
            }
        }

        void KeySetup(int32_t key = 0) {
            UserKey = key;
            RC5KeySetup();
            RC6KeySetup();
        }

        CCrypt(int32_t key = 0)
        {
            KeySetup(key);
        }

        void RC5Encrypt32(const void* source, void* destination, int size)
        {
            uint16_t A, B, * src = (uint16_t*)source, * dst = (uint16_t*)destination;
            int i, j;
            if (source != destination && size % 4) nocrt_memcpy((char*)destination + size - size % 4, (char*)source + size - size % 4, size % 4);
            for (j = size / 4; j > 0; j--, src += 2, dst += 2)
            {
                A = src[0] + RC5S[0];
                B = src[1] + RC5S[1];
                for (i = 1; i <= 12; i++)
                {
                    A = ROTL16(A ^ B, B) + RC5S[2 * i];
                    B = ROTL16(B ^ A, A) + RC5S[2 * i + 1];
                }
                dst[0] = A ^ UserKey;
                dst[1] = B ^ UserKey;
            }
        }

        void RC5Decrypt32(const void* source, void* destination, int size)
        {
            uint16_t A, B, * src = (uint16_t*)source, * dst = (uint16_t*)destination;
            int i, j;
            if (source != destination && size % 4) nocrt_memcpy((char*)destination + size - size % 4, (char*)source + size - size % 4, size % 4);
            for (j = size / 4; j > 0; j--, src += 2, dst += 2)
            {
                A = src[0] ^ UserKey;
                B = src[1] ^ UserKey;
                for (i = 12; i > 0; i--)
                {
                    B = ROTR16(B - RC5S[2 * i + 1], A) ^ A;
                    A = ROTR16(A - RC5S[2 * i], B) ^ B;
                }
                dst[0] = A - RC5S[0];
                dst[1] = B - RC5S[1];
            }
        }

        void RC5Encrypt64(const void* source, void* destination, int size)
        {
            uint32_t A, B, * src = (uint32_t*)source, * dst = (uint32_t*)destination;
            int i, j;
            for (j = size / 8; j > 0; j--, src += 2, dst += 2)
            {
                A = src[0] + RC5S[0];
                B = src[1] + RC5S[1];
                for (i = 1; i <= 12; i++)
                {
                    A = ROTL32(A ^ B, B) + RC5S[2 * i];
                    B = ROTL32(B ^ A, A) + RC5S[2 * i + 1];
                }
                dst[0] = A ^ UserKey;
                dst[1] = B ^ UserKey;
            }
            RC5Encrypt32((uint32_t*)source + (size - size % 8) / 4, (uint32_t*)destination + (size - size % 8) / 4, size % 8);
        }

        void RC5Decrypt64(const void* source, void* destination, int size)
        {
            uint32_t A, B, * src = (uint32_t*)source, * dst = (uint32_t*)destination;
            int i, j;
            for (j = size / 8; j > 0; j--, src += 2, dst += 2)
            {
                A = src[0] ^ UserKey;
                B = src[1] ^ UserKey;
                for (i = 12; i > 0; i--)
                {
                    B = ROTR32(B - RC5S[2 * i + 1], A) ^ A;
                    A = ROTR32(A - RC5S[2 * i], B) ^ B;
                }
                dst[0] = A - RC5S[0];
                dst[1] = B - RC5S[1];
            }
            RC5Decrypt32((uint32_t*)source + (size - size % 8) / 4, (uint32_t*)destination + (size - size % 8) / 4, size % 8);
        }

        void RC6Encrypt128(const void* source, void* destination, int size)
        {
            uint32_t A, B, C, D, t, u, x, * src = (uint32_t*)source, * dst = (uint32_t*)destination;
            int i, j;
            for (j = size / 16; j > 0; j--, src += 4, dst += 4)
            {
                A = src[0];
                B = src[1] + RC6S[0];
                C = src[2];
                D = src[3] + RC6S[1];
                for (i = 2; i <= 2 * 40; i += 2)
                {
                    t = ROTL32(B * (2 * B + 1), 5);
                    u = ROTL32(D * (2 * D + 1), 5);
                    A = ROTL32(A ^ t, u) + RC6S[i];
                    C = ROTL32(C ^ u, t) + RC6S[i + 1];
                    x = A;
                    A = B;
                    B = C;
                    C = D;
                    D = x;
                }
                dst[0] = (A + RC6S[2 * 40 + 2]) ^ UserKey;
                dst[1] = B ^ UserKey;
                dst[2] = (C + RC6S[2 * 40 + 3]) ^ UserKey;
                dst[3] = D ^ UserKey;
            }
            RC5Encrypt64((uint32_t*)source + (size - size % 16) / 4, (uint32_t*)destination + (size - size % 16) / 4, size % 16);
        }

        void RC6Decrypt128(const void* source, void* destination, int size)
        {
            uint32_t A, B, C, D, t, u, x, * src = (uint32_t*)source, * dst = (uint32_t*)destination;
            int i, j;
            for (j = size / 16; j > 0; j--, src += 4, dst += 4)
            {
                A = (src[0] ^ UserKey) - RC6S[2 * 40 + 2];
                B = src[1] ^ UserKey;
                C = (src[2] ^ UserKey) - RC6S[2 * 40 + 3];
                D = src[3] ^ UserKey;
                for (i = 2 * 40; i >= 2; i -= 2)
                {
                    x = D;
                    D = C;
                    C = B;
                    B = A;
                    A = x;
                    u = ROTL32(D * (2 * D + 1), 5);
                    t = ROTL32(B * (2 * B + 1), 5);
                    C = ROTR32(C - RC6S[i + 1], t) ^ u;
                    A = ROTR32(A - RC6S[i], u) ^ t;
                }
                dst[0] = A;
                dst[1] = B - RC6S[0];
                dst[2] = C;
                dst[3] = D - RC6S[1];
            }
            RC5Decrypt64((uint32_t*)source + (size - size % 16) / 4, (uint32_t*)destination + (size - size % 16) / 4, size % 16);
        }
    };
    unsigned char hex_char_to_value(char c) 
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    }
    void hex_to_bytes(const std::string& hex_str, char output[64]) 
    {
        for (size_t i = 0; i < 64; i++) 
        {
            unsigned char high = hex_char_to_value(hex_str[i * 2]);
            unsigned char low = hex_char_to_value(hex_str[i * 2 + 1]);
            output[i] = (high << 4) | low;
        }
    }
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

            SwapAddressPatch Tickrate_DelayAnims1;
            SwapAddressPatch Tickrate_DelayAnims2;
            SwapAddressPatch Tickrate_DelayAnims3;
            SwapAddressPatch Tickrate_DelayAnims4;
            SwapAddressPatch Tickrate_DelayAnims5;
            SwapAddressPatch Tickrate_DelayAnims6;
            SwapAddressPatch Tickrate_DelayAnims7;
            SwapAddressPatch Tickrate_DelayAnims8;
            SwapAddressPatch Tickrate_DelayAnims9;
            SwapAddressPatch Tickrate_DelayAnims10;
            SwapAddressPatch Tickrate_DelayAnims11;
            SwapAddressPatch Tickrate_DelayAnims12;
            SwapAddressPatch Tickrate_DelayAnims13;
            SwapAddressPatch Tickrate_DelayAnims14;
            SwapAddressPatch Tickrate_DelayAnims15;
            SwapAddressPatch Tickrate_DelayAnims16;
            SwapAddressPatch Tickrate_DelayAnims17;
            SwapAddressPatch Tickrate_DelayAnims18;


            SwapAddressPatch Tickrate_MinDistance1;
            SwapAddressPatch Tickrate_MinDistance2;
            SwapAddressPatch Tickrate_MinDistance3;
            SwapAddressPatch Tickrate_MinDistance4;
            SwapAddressPatch Tickrate_MinDistance5;
            SwapAddressPatch Tickrate_MinDistance6;
            SwapAddressPatch Tickrate_MinDistance7;

            PatchBytes Rot1_PatchBytes;
            PatchBytes Rot2_PatchBytes;
            PatchBytes Rot3_PatchBytes;

            CDetourHook NationIndex;
            CDetourHook WindowTitle;
            CDetourHook SpectatePov;
            CDetourHook DrawDebugInfo;

            CDetourHook CommonAgoraDlgInit;
            CDetourHook CommonAgoraDlgConstruct;

            SwapAddressPatch WidthBuff1;
            SwapAddressPatch WidthBuff2;
			SwapAddressPatch WidthBuff3;
			SwapAddressPatch WidthBuff4;
			SwapAddressPatch WidthBuff5;
			SwapAddressPatch WidthBuff6;
			SwapAddressPatch WidthBuff7;
			SwapAddressPatch WidthBuff8;
			SwapAddressPatch WidthBuff9;
			SwapAddressPatch WidthBuff10;
			SwapAddressPatch WidthBuff11;
			SwapAddressPatch WidthBuff12;
			SwapAddressPatch WidthBuff13;
			SwapAddressPatch WidthBuff14;
			SwapAddressPatch WidthBuff15;
			SwapAddressPatch WidthBuff16;
			SwapAddressPatch WidthBuff17;
			SwapAddressPatch WidthBuff18;
			SwapAddressPatch WidthBuff19;
			SwapAddressPatch WidthBuff20;

            SwapAddressPatch HeightBuff1;
            SwapAddressPatch HeightBuff2;
            SwapAddressPatch HeightBuff3;
            SwapAddressPatch HeightBuff4;
            SwapAddressPatch HeightBuff5;
            SwapAddressPatch HeightBuff6;
            SwapAddressPatch HeightBuff7;
            SwapAddressPatch HeightBuff8;
            SwapAddressPatch HeightBuff9;
            SwapAddressPatch HeightBuff10;
            SwapAddressPatch HeightBuff11;
            SwapAddressPatch HeightBuff12;
            SwapAddressPatch HeightBuff13;
            SwapAddressPatch HeightBuff14;
            SwapAddressPatch HeightBuff15;
            SwapAddressPatch HeightBuff16;
            SwapAddressPatch HeightBuff17;
            SwapAddressPatch HeightBuff18;
            SwapAddressPatch HeightBuff19;

            SwapAddressPatch AspectRatioIds1;
            SwapAddressPatch AspectRatioIds2;
            SwapAddressPatch AspectRatioIds3;
            SwapAddressPatch AspectRatioIds4;
            SwapAddressPatch AspectRatioIds5;
            SwapAddressPatch AspectRatioIds6;
            SwapAddressPatch AspectRatioIds7;
            SwapAddressPatch AspectRatioIds8;
            SwapAddressPatch AspectRatioIds9;
            SwapAddressPatch AspectRatioIds10;
            SwapAddressPatch AspectRatioIds11;
            SwapAddressPatch AspectRatioIds12;
            SwapAddressPatch AspectRatioIds13;
            SwapAddressPatch AspectRatioIds14;
            SwapAddressPatch AspectRatioIds15;

            PatchBytes ResolutionListSize1;
            PatchBytes ResolutionListSize2;
            PatchBytes ResolutionListSize3;
            PatchBytes ResolutionListSize4;
            PatchBytes ResolutionListSize5;
            PatchBytes ResolutionListSize6;
            CDetourHook SetAspectRatioScale;

        }
        namespace BugFixes
        {


            CDetourHook FixWeaponSelectDetour;
            CDetourHook FixWeaponSelectDetour_Setting;
            CDetourHook FixWeaponSelectDetour_Main;
            CDetourHook Screeshot_bug;
            CDetourHook Screenshot2_bug;
            PatchBytes SetDateTimeShit;
        }
        namespace AntiCheat
        {
            namespace AckHandlers
            {
                CDetourHook NetworkInitCrypto;
            }
            namespace ReqHandlers
            {
                CDetourHook MainAuthorize;
            }
            namespace GameManagers
            {
                namespace Room
                {
                    CDetourHook Get;
                    CDetourHook Destroy;
                }
                namespace UnitContainer
                {
                    CDetourHook Get;
                    CDetourHook Destroy;
                }
                namespace UnitMgr
                {
                    CDetourHook Get;
                    CDetourHook Destroy;
                }
                namespace NetMgr
                {
                    CDetourHook Get;
                    CDetourHook Destroy;
                }
                namespace Dynamics
                {
                    CDetourHook Get;
                    CDetourHook Destroy;
                }
                namespace World
                {
                    CDetourHook Get;
                    CDetourHook Destroy;
                }
            }
            namespace InitDefaultSettings
            {
                CDetourHook Init;
            }
            namespace Crypto
            {
                SwapAddressPatch ArchiveLoaderPW;
                
                namespace RC5
                {
                    CDetourHook KeySetup;
                }
                namespace RC6
                {
                    CDetourHook KeySetup;
                }
                CDetourHook Encrypt;
                CDetourHook Decrypt;
            }
            namespace CDBM
            {
				CDetourHook Load;
            }
        }
        inline void InitFeaturesHooks()
        {
            Features::CustomResolutionsPatch();
            Features::HideWeaponSlot.Create(MegaGuard::Addresses::Hooks::Features::HideWeaponSlot.get(), Features::HideWeaponSlots);
            Features::CustomTickratePatch();
			Features::NationIndex.Create(MegaGuard::Addresses::Hooks::Features::Custom_GetNationIndex.get(), &Features::GetNationIndex);
            Features::WindowTitle.Create(MegaGuard::Addresses::Hooks::Features::Custom_GetWindowTitle.get(), &Features::GetWindowTitle);
            Features::SpectatePov.Create(MegaGuard::Addresses::Hooks::Features::SpectateObserver.get(), &Features::Spectate);
			Features::CommonAgoraDlgConstruct.Create(MegaGuard::Addresses::Hooks::Features::CommonAgoraDlgConstruct.get(), &Features::Common_Agora_Construct_DLG);
            Features::CommonAgoraDlgInit.Create(MegaGuard::Addresses::Hooks::Features::CommonAgoraDlgInit.get(), &Features::Common_Agora_Init_DLG);
        }
        inline void InitBugFixesHooks()
        {
			BugFixes::FixWeaponSelectDetour.Create(MegaGuard::Addresses::Hooks::Bugfixes::RoomCreateDialogHandler.get(), &BugFixes::RoomCreateDialogHandler);
			BugFixes::FixWeaponSelectDetour_Setting.Create(MegaGuard::Addresses::Hooks::Bugfixes::RoomSettingsDialogHandler.get(), &BugFixes::RoomSettingDialogHandler);
			BugFixes::FixWeaponSelectDetour_Main.Create(MegaGuard::Addresses::Hooks::Bugfixes::RoomMainDialogHandler.get(), &BugFixes::RoomMainDialogHandler);
            BugFixes::SetDateTimeShit.Patch(MegaGuard::Addresses::Hooks::Bugfixes::SetDateTimeShit.get(), "\x90\x90\x90\x90\x90\x90", 6);
			BugFixes::Screeshot_bug.Create(MegaGuard::Addresses::Hooks::Bugfixes::ScreenshotBug1.get(), &BugFixes::ScreenShot);            
        }
        inline const char* get_archiveloader_pw()
        {
            return "45F6256E282EB3505800B7B7405236BA6BDA673B112EE25760F16DA8ADA61610E21544E9CD01F7BFE9E98B0D2F9AC48DAA7C57D4FECC173ADA3EF2A1FEECEEA1";
        }
        inline const char* get_cgd_pw()
        {
            return "02239E046913704B4B17BCDF5CF95FFD4D0524A29A232AE2BAE827620B350E89FAA58BF3D20DDBB9574841426EDEA58DF342C43203CA83EEF602F9242B41DE93";
        }
        inline void InitAntiCheatHooks()
        {
            InitializeCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);
            InitializeCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitContainer::MyCriticalSection);
            InitializeCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitMgr::MyCriticalSection);
            InitializeCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::NetMgr::MyCriticalSection);
            InitializeCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Dynamics::MyCriticalSection);
            
            
            AntiCheat::GameManagers::Room::Get.Create(MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::Get.get(), &AntiCheat::GetCRoom);
            AntiCheat::GameManagers::Room::Destroy.Create(MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::Destroy.get(), &AntiCheat::DestroyCRoom);

            AntiCheat::GameManagers::UnitContainer::Get.Create(MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitContainer::Get.get(), &AntiCheat::GetCUnitContainer);
            AntiCheat::GameManagers::UnitContainer::Destroy.Create(MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitContainer::Destroy.get(), &AntiCheat::DestroyCUnitContainer);

            AntiCheat::GameManagers::UnitMgr::Get.Create(MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitMgr::Get.get(), &AntiCheat::GetCUnitMgr);
            AntiCheat::GameManagers::UnitMgr::Destroy.Create(MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitMgr::Destroy.get(), &AntiCheat::DestroyCUnitMgr);

            AntiCheat::GameManagers::NetMgr::Get.Create(MegaGuard::Addresses::Hooks::Anticheat::GameManagers::NetMgr::Get.get(), &AntiCheat::GetCNetMgr);
            AntiCheat::GameManagers::NetMgr::Destroy.Create(MegaGuard::Addresses::Hooks::Anticheat::GameManagers::NetMgr::Destroy.get(), &AntiCheat::DestroyCNetMgr);

            AntiCheat::GameManagers::Dynamics::Get.Create(MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Dynamics::Get.get(), &AntiCheat::GetCDynamics);
            AntiCheat::GameManagers::Dynamics::Destroy.Create(MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Dynamics::Destroy.get(), &AntiCheat::DestroyCDynamics);
            
            

            //AntiCheat::InitDefaultSettings::Init.Create(MegaGuard::Addresses::Hooks::Anticheat::InitDefaultSettings::Init, &AntiCheat::InitSettings);
            AntiCheat::Crypto::RC5::KeySetup.Create(MegaGuard::Addresses::Hooks::Anticheat::Crypto::RC5::KeySetup.get(), &AntiCheat::RC5_KeySetup);
            AntiCheat::Crypto::RC6::KeySetup.Create(MegaGuard::Addresses::Hooks::Anticheat::Crypto::RC6::KeySetup.get(), &AntiCheat::RC6_KeySetup);
            AntiCheat::Crypto::Decrypt.Create(MegaGuard::Addresses::Hooks::Anticheat::Crypto::Decrypt.get(), &AntiCheat::CCrypt_Decrypt);
            AntiCheat::Crypto::Encrypt.Create(MegaGuard::Addresses::Hooks::Anticheat::Crypto::Encrypt.get(), &AntiCheat::CCrypt_Encrypt);
            nocrt_strcpy(reinterpret_cast<char*>(MegaGuard::Addresses::Hooks::Anticheat::Crypto::CGD::static_pw1.get()), get_cgd_pw());
            nocrt_strcpy(reinterpret_cast<char*>(MegaGuard::Addresses::Hooks::Anticheat::Crypto::CGD::static_pw2.get()), get_cgd_pw());

            char archiveloader_pw[65] = { 0 };
            auto crypt = CCrypt(0);
            hex_to_bytes(get_archiveloader_pw(), archiveloader_pw);
            crypt.RC6Decrypt128(archiveloader_pw, archiveloader_pw, 64);
            archiveloader_pw[64] = '\0';
            *reinterpret_cast<old_string*>(MegaGuard::Addresses::Hooks::Anticheat::Crypto::archiveloader_pw.get()) = archiveloader_pw;
			nocrt_memset(archiveloader_pw, 0, 64);
            //AntiCheat::CDBM::Load.Create(MegaGuard::Addresses::Hooks::Anticheat::CDBM::Load.get(), &AntiCheat::CDBMLoad);
            AntiCheat::AckHandlers::NetworkInitCrypto.Create(MegaGuard::Addresses::Hooks::Anticheat::AckHandlers::NetworkInitCrypto.get(), &AntiCheat::NetworkConnectAck);
            AntiCheat::ReqHandlers::MainAuthorize.Create(MegaGuard::Addresses::Hooks::Anticheat::ReqHandlers::MainAuthorize.get(), &AntiCheat::MainAuthorize);

        }
        /*
        inline void RemoveFeaturesHooks()
        {
            Features::HideWeaponSlot.Remove();
            Features::CustomTickrateUnpatch();
            Features::NationIndex.Remove();
            Features::WindowTitle.Remove();
            Features::SpectatePov.Remove();
           

        }
        inline void RemoveBugFixesHooks()
        {
            BugFixes::FixWeaponSelectDetour.Remove();
			BugFixes::FixWeaponSelectDetour_Setting.Remove();
			BugFixes::FixWeaponSelectDetour_Main.Remove();
        }
        inline void RemoveAntiCheatHooks()
        {
            
            AntiCheat::Crypto::RC5::KeySetup.Remove();
            AntiCheat::Crypto::RC6::KeySetup.Remove();
			AntiCheat::Crypto::Encrypt.Remove();
			AntiCheat::Crypto::Decrypt.Remove();
            AntiCheat::GameManagers::Room::Get.Remove();
			AntiCheat::GameManagers::Room::Destroy.Remove();
            AntiCheat::GameManagers::UnitContainer::Get.Remove();
            AntiCheat::GameManagers::UnitContainer::Destroy.Remove();
            AntiCheat::GameManagers::UnitMgr::Get.Remove();
            AntiCheat::GameManagers::UnitMgr::Destroy.Remove();
            AntiCheat::GameManagers::NetMgr::Get.Remove();
            AntiCheat::GameManagers::NetMgr::Destroy.Remove();
            AntiCheat::GameManagers::Dynamics::Get.Remove();
            AntiCheat::GameManagers::Dynamics::Destroy.Remove();
            DeleteCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);
            DeleteCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitContainer::MyCriticalSection);
            DeleteCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitMgr::MyCriticalSection);
            DeleteCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::NetMgr::MyCriticalSection);
            DeleteCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Dynamics::MyCriticalSection);
        }
        */

        [[clang::annotate("x-vm,x-full,x-cfg,ind-br,alias-access,custom-cc")]]
        inline void InitializeAllHooks()
        {
            if (MH_Initialize() != MH_OK)
            {
                MegaGuard::EventLog->Debug(nostd::source_location::current(), "Failed to initialize MinHook");
            }

            if (g_ipcClient) g_ipcClient->SetProgress(74);
            InitFeaturesHooks();
            
            if (g_ipcClient) g_ipcClient->SetProgress(80);
            InitBugFixesHooks();
            
            if (g_ipcClient) g_ipcClient->SetProgress(88);
            InitAntiCheatHooks();
            
            if (g_ipcClient) g_ipcClient->SetProgress(95);
        }
        /*
        [[clang::annotate("x-vm,x-full,x-cfg,ind-br,alias-access,custom-cc")]]
        inline void RemoveAllHooks()
        {
            //VMProtectBeginUltra("RemoveAllHooks_vmp");
            RemoveFeaturesHooks();
            RemoveBugFixesHooks();
            RemoveAntiCheatHooks();

            MH_Uninitialize();
            
            //VMProtectEnd();
        }
        */
    }
}