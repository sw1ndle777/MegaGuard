#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace Features
        {
            using namespace MegaGuard::Globals::Features;
            inline void CustomTickratePatch()
            {
                Features::Tickrate_Frametime1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_Frametime1.get(), &CustomTickrate::frame_time);
                //Features::Tickrate_Frametime2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_Frametime2, &CustomTickrate::frame_time);
                Features::Tickrate_Frametime3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_Frametime3.get(), &CustomTickrate::frame_time);
                Features::Tickrate_Frametime4.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_Frametime4.get(), &CustomTickrate::frame_time);

                Features::Tickrate_RotDamp1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotDamp1.get(), &CustomTickrate::rotation_damping);
                Features::Tickrate_RotDamp2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotDamp2.get(), &CustomTickrate::rotation_damping);
                Features::Tickrate_RotDamp3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotDamp3.get(), &CustomTickrate::rotation_damping);

                Features::Tickrate_MinRotSpeed1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinRotSpeed1.get(), &CustomTickrate::minimum_rotation_speed);
                Features::Tickrate_MinRotSpeed2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinRotSpeed2.get(), &CustomTickrate::minimum_rotation_speed);
                Features::Tickrate_MinRotSpeed3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinRotSpeed3.get(), &CustomTickrate::minimum_rotation_speed);
                Features::Tickrate_MinRotSpeed4.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinRotSpeed4.get(), &CustomTickrate::minimum_rotation_speed);
                Features::Tickrate_MinRotSpeed5.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinRotSpeed5.get(), &CustomTickrate::minimum_rotation_speed);
                Features::Tickrate_MinRotSpeed6.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinRotSpeed6.get(), &CustomTickrate::minimum_rotation_speed);

                Features::Tickrate_RotThreeshold1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreeshold1.get(), &CustomTickrate::rotation_threeshold);
                Features::Tickrate_RotThreeshold2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreeshold2.get(), &CustomTickrate::rotation_threeshold);
                //Features::Tickrate_RotThreeshold3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreeshold3, &CustomTickrate::rotation_threeshold); //don't use
                Features::Tickrate_RotThreeshold4.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreeshold4.get(), &CustomTickrate::rotation_threeshold);
                Features::Tickrate_RotThreeshold5.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreeshold5.get(), &CustomTickrate::rotation_threeshold);
                Features::Tickrate_RotThreeshold6.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreeshold6.get(), &CustomTickrate::rotation_threeshold);
                Features::Tickrate_RotThreeshold7.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreeshold7.get(), &CustomTickrate::rotation_threeshold);

                Features::Tickrate_RotThreesholdLimit.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreesholdLimit.get(), &CustomTickrate::rotation_threeshold_limit);

                Features::Tickrate_MaxRotSpeed1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MaxRotSpeed1.get(), &CustomTickrate::maximum_rotation_speed);
                Features::Tickrate_MaxRotSpeed2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MaxRotSpeed2.get(), &CustomTickrate::maximum_rotation_speed);
                Features::Tickrate_MaxRotSpeed3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MaxRotSpeed3.get(), &CustomTickrate::maximum_rotation_speed);
                Features::Tickrate_MaxRotSpeed4.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MaxRotSpeed4.get(), &CustomTickrate::maximum_rotation_speed);
                Features::Tickrate_MaxRotSpeed5.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MaxRotSpeed5.get(), &CustomTickrate::maximum_rotation_speed);
                Features::Tickrate_MaxRotSpeed6.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MaxRotSpeed6.get(), &CustomTickrate::maximum_rotation_speed);

                Features::Tickrate_DelayReq1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq1.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq2.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq3.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq4.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq4.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq5.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq5.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq6.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq6.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq7.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq7.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq8.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq8.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq9.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq9.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq10.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq10.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq11.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq11.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq12.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq12.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq13.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq13.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq14.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq14.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq15.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq15.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq16.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq16.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq17.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq17.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq18.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq18.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq19.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq19.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq20.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq20.get(), &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq21.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq21.get(), &CustomTickrate::hardcoded_tickrate);

				Features::Tickrate_DelayAnims1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims1.get(), &CustomTickrate::hardcoded_tickrate2);
                Features::Tickrate_DelayAnims2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims2.get(), &CustomTickrate::hardcoded_tickrate2);
                Features::Tickrate_DelayAnims3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims3.get(), &CustomTickrate::hardcoded_tickrate2);
                Features::Tickrate_DelayAnims4.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims4.get(), &CustomTickrate::hardcoded_tickrate2);
                Features::Tickrate_DelayAnims5.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims5.get(), &CustomTickrate::hardcoded_tickrate2);
                Features::Tickrate_DelayAnims6.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims6.get(), &CustomTickrate::hardcoded_tickrate2);
                Features::Tickrate_DelayAnims7.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims7.get(), &CustomTickrate::hardcoded_tickrate2);
                Features::Tickrate_DelayAnims8.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims8.get(), &CustomTickrate::hardcoded_tickrate2);
                Features::Tickrate_DelayAnims9.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims9.get(), &CustomTickrate::hardcoded_tickrate2);
                Features::Tickrate_DelayAnims10.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims10.get(), &CustomTickrate::hardcoded_tickrate2);
                Features::Tickrate_DelayAnims11.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims11.get(), &CustomTickrate::hardcoded_tickrate2);
                Features::Tickrate_DelayAnims12.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims12.get(), &CustomTickrate::hardcoded_tickrate2);
                Features::Tickrate_DelayAnims13.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims13.get(), &CustomTickrate::hardcoded_tickrate2);

                Features::Tickrate_DelayAnims14.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims14.get(), &CustomTickrate::hardcoded_tickrate3);
                Features::Tickrate_DelayAnims15.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims15.get(), &CustomTickrate::hardcoded_tickrate3);
                Features::Tickrate_DelayAnims16.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims16.get(), &CustomTickrate::hardcoded_tickrate3);
                Features::Tickrate_DelayAnims17.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims17.get(), &CustomTickrate::hardcoded_tickrate3);
                Features::Tickrate_DelayAnims18.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayAnims18.get(), &CustomTickrate::hardcoded_tickrate3);

                Features::Tickrate_MinDistance1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinDistance1, &CustomTickrate::minimum_distance);
                Features::Tickrate_MinDistance2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinDistance2, &CustomTickrate::minimum_distance2);
                Features::Tickrate_MinDistance3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinDistance3, &CustomTickrate::minimum_distance2);
                Features::Tickrate_MinDistance4.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinDistance4, &CustomTickrate::minimum_distance2);
                Features::Tickrate_MinDistance5.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinDistance5, &CustomTickrate::minimum_distance2);
                Features::Tickrate_MinDistance6.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinDistance6, &CustomTickrate::minimum_distance2);
                Features::Tickrate_MinDistance7.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinDistance7, &CustomTickrate::minimum_distance2);

                Features::Rot1_PatchBytes.Patch(MegaGuard::Addresses::Hooks::Features::Rot1_Patch.get(), "\xD9\xEE\x90\x90\x90\x90", 6);
                Features::Rot2_PatchBytes.Patch(MegaGuard::Addresses::Hooks::Features::Rot2_Patch.get(), "\xD9\xEE\x90\x90\x90\x90", 6);
                Features::Rot3_PatchBytes.Patch(MegaGuard::Addresses::Hooks::Features::Rot3_Patch.get(), "\xD9\xEE\x90\x90\x90\x90", 6);
            }
            inline void CustomTickrateUnpatch()
            {
                /*
                Features::Tickrate_Frametime1.Unpatch();
                //Features::Tickrate_Frametime2.Unpatch();
                Features::Tickrate_Frametime3.Unpatch();
                Features::Tickrate_Frametime4.Unpatch();

                Features::Tickrate_RotDamp1.Unpatch();
                Features::Tickrate_RotDamp2.Unpatch();
                Features::Tickrate_RotDamp3.Unpatch();

                Features::Tickrate_MinRotSpeed1.Unpatch();
                Features::Tickrate_MinRotSpeed2.Unpatch();
                Features::Tickrate_MinRotSpeed3.Unpatch();
                Features::Tickrate_MinRotSpeed4.Unpatch();
                Features::Tickrate_MinRotSpeed5.Unpatch();
                Features::Tickrate_MinRotSpeed6.Unpatch();

                Features::Tickrate_RotThreeshold1.Unpatch();
                Features::Tickrate_RotThreeshold2.Unpatch();
                Features::Tickrate_RotThreeshold3.Unpatch();
                Features::Tickrate_RotThreeshold4.Unpatch();
                Features::Tickrate_RotThreeshold5.Unpatch();
                Features::Tickrate_RotThreeshold6.Unpatch();
                Features::Tickrate_RotThreeshold7.Unpatch();

                Features::Tickrate_RotThreesholdLimit.Unpatch();

                Features::Tickrate_MaxRotSpeed1.Unpatch();
                Features::Tickrate_MaxRotSpeed2.Unpatch();
                Features::Tickrate_MaxRotSpeed3.Unpatch();
                Features::Tickrate_MaxRotSpeed4.Unpatch();
                Features::Tickrate_MaxRotSpeed5.Unpatch();
                Features::Tickrate_MaxRotSpeed6.Unpatch();

                Features::Tickrate_DelayReq1.Unpatch();
                Features::Tickrate_DelayReq2.Unpatch();
                Features::Tickrate_DelayReq3.Unpatch();
                Features::Tickrate_DelayReq4.Unpatch();
                Features::Tickrate_DelayReq5.Unpatch();
                Features::Tickrate_DelayReq6.Unpatch();
                Features::Tickrate_DelayReq7.Unpatch();
                Features::Tickrate_DelayReq8.Unpatch();
                Features::Tickrate_DelayReq9.Unpatch();
                Features::Tickrate_DelayReq10.Unpatch();
                Features::Tickrate_DelayReq11.Unpatch();
                Features::Tickrate_DelayReq12.Unpatch();
                Features::Tickrate_DelayReq13.Unpatch();
                Features::Tickrate_DelayReq14.Unpatch();
                Features::Tickrate_DelayReq15.Unpatch();
                Features::Tickrate_DelayReq16.Unpatch();
                Features::Tickrate_DelayReq17.Unpatch();
                Features::Tickrate_DelayReq18.Unpatch();
                Features::Tickrate_DelayReq19.Unpatch();
                Features::Tickrate_DelayReq20.Unpatch();
                Features::Tickrate_DelayReq21.Unpatch();

                Features::Tickrate_DelayAnims1.Unpatch();
                Features::Tickrate_DelayAnims2.Unpatch();
                Features::Tickrate_DelayAnims3.Unpatch();
                Features::Tickrate_DelayAnims4.Unpatch();
                Features::Tickrate_DelayAnims5.Unpatch();
                Features::Tickrate_DelayAnims6.Unpatch();
                Features::Tickrate_DelayAnims7.Unpatch();
                Features::Tickrate_DelayAnims8.Unpatch();
                Features::Tickrate_DelayAnims9.Unpatch();
                Features::Tickrate_DelayAnims10.Unpatch();
                Features::Tickrate_DelayAnims11.Unpatch();
                Features::Tickrate_DelayAnims12.Unpatch();
                Features::Tickrate_DelayAnims13.Unpatch();
                Features::Tickrate_DelayAnims14.Unpatch();
                Features::Tickrate_DelayAnims15.Unpatch();
                Features::Tickrate_DelayAnims16.Unpatch();
                Features::Tickrate_DelayAnims17.Unpatch();
                Features::Tickrate_DelayAnims18.Unpatch();

                Features::Tickrate_DelayAnims1.Unpatch();
                Features::Tickrate_DelayAnims1.Unpatch();
                Features::Tickrate_DelayAnims1.Unpatch();
                Features::Tickrate_DelayAnims1.Unpatch();

                Features::Tickrate_MinDistance1.Unpatch();
                Features::Tickrate_MinDistance2.Unpatch();
                Features::Tickrate_MinDistance3.Unpatch();
                Features::Tickrate_MinDistance4.Unpatch();
                Features::Tickrate_MinDistance5.Unpatch();
                Features::Tickrate_MinDistance6.Unpatch();
                Features::Tickrate_MinDistance7.Unpatch();
                Features::Rot1_PatchBytes.Unpatch();
				Features::Rot2_PatchBytes.Unpatch();
				Features::Rot3_PatchBytes.Unpatch();
                */
            }
        }
    }
}