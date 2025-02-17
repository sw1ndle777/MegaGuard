#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace Features
        {
            using namespace MegaGuard::Globals::Features;
            inline void CustomVoiceTypesPatch()
            {
                Features::VoiceSpecialTypeA.Patch(MegaGuard::Addresses::Hooks::Features::VoiceSpecialTypeA, "\x00", 1, 3);
                Features::VoiceSpecialTypeB.Patch(MegaGuard::Addresses::Hooks::Features::VoiceSpecialTypeB, "\x00", 1, 3);
                Features::VoiceSpecialTypeC.Patch(MegaGuard::Addresses::Hooks::Features::VoiceSpecialTypeC, "\x00", 1, 3);
                Features::VoiceSpecialTypeD.Patch(MegaGuard::Addresses::Hooks::Features::VoiceSpecialTypeD, "\x00", 1, 3);
                Features::VoiceSpecialTypeE.Patch(MegaGuard::Addresses::Hooks::Features::VoiceSpecialTypeE, "\x00", 1, 3);
            }
            inline void CustomVoiceTypesUnpatch()
            {
                Features::VoiceSpecialTypeA.Unpatch();
                Features::VoiceSpecialTypeB.Unpatch();
                Features::VoiceSpecialTypeC.Unpatch();
                Features::VoiceSpecialTypeD.Unpatch();
                Features::VoiceSpecialTypeE.Unpatch();
            }
        }
    }
}