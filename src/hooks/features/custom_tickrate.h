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
                Features::Tickrate_Frametime1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_Frametime1, &CustomTickrate::frame_time);
                Features::Tickrate_Frametime2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_Frametime2, &CustomTickrate::frame_time);
                Features::Tickrate_Frametime3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_Frametime3, &CustomTickrate::frame_time);
                Features::Tickrate_Frametime4.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_Frametime4, &CustomTickrate::frame_time);

                Features::Tickrate_RotDamp1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotDamp1, &CustomTickrate::rotation_damping);
                Features::Tickrate_RotDamp2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotDamp2, &CustomTickrate::rotation_damping);
                Features::Tickrate_RotDamp3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotDamp3, &CustomTickrate::rotation_damping);

                Features::Tickrate_MinRotSpeed1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinRotSpeed1, &CustomTickrate::minimum_rotation_speed);
                Features::Tickrate_MinRotSpeed2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinRotSpeed2, &CustomTickrate::minimum_rotation_speed);
                Features::Tickrate_MinRotSpeed3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinRotSpeed3, &CustomTickrate::minimum_rotation_speed);
                Features::Tickrate_MinRotSpeed4.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinRotSpeed4, &CustomTickrate::minimum_rotation_speed);
                Features::Tickrate_MinRotSpeed5.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinRotSpeed5, &CustomTickrate::minimum_rotation_speed);
                Features::Tickrate_MinRotSpeed6.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinRotSpeed6, &CustomTickrate::minimum_rotation_speed);

                Features::Tickrate_RotThreeshold1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreeshold1, &CustomTickrate::rotation_threeshold);
                Features::Tickrate_RotThreeshold2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreeshold2, &CustomTickrate::rotation_threeshold);
                //Features::Tickrate_RotThreeshold3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreeshold3, &CustomTickrate::rotation_threeshold); //don't use
                Features::Tickrate_RotThreeshold4.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreeshold4, &CustomTickrate::rotation_threeshold);
                Features::Tickrate_RotThreeshold5.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreeshold5, &CustomTickrate::rotation_threeshold);
                Features::Tickrate_RotThreeshold6.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreeshold6, &CustomTickrate::rotation_threeshold);
                Features::Tickrate_RotThreeshold7.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreeshold7, &CustomTickrate::rotation_threeshold);

                Features::Tickrate_RotThreesholdLimit.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_RotThreesholdLimit, &CustomTickrate::rotation_threeshold_limit);

                Features::Tickrate_MaxRotSpeed1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MaxRotSpeed1, &CustomTickrate::maximum_rotation_speed);
                Features::Tickrate_MaxRotSpeed2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MaxRotSpeed2, &CustomTickrate::maximum_rotation_speed);
                Features::Tickrate_MaxRotSpeed3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MaxRotSpeed3, &CustomTickrate::maximum_rotation_speed);
                Features::Tickrate_MaxRotSpeed4.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MaxRotSpeed4, &CustomTickrate::maximum_rotation_speed);
                Features::Tickrate_MaxRotSpeed5.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MaxRotSpeed5, &CustomTickrate::maximum_rotation_speed);
                Features::Tickrate_MaxRotSpeed6.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MaxRotSpeed6, &CustomTickrate::maximum_rotation_speed);

                Features::Tickrate_DelayReq1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq1, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq2, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq3, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq4.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq4, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq5.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq5, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq6.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq6, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq7.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq7, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq8.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq8, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq9.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq9, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq10.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq10, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq11.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq11, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq12.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq12, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq13.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq13, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq14.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq14, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq15.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq15, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq16.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq16, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq17.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq17, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq18.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq18, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq19.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq19, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq20.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq20, &CustomTickrate::hardcoded_tickrate);
                Features::Tickrate_DelayReq21.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_DelayReq21, &CustomTickrate::hardcoded_tickrate);

                Features::Tickrate_MinDistance1.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinDistance1, &CustomTickrate::minimum_distance);
                Features::Tickrate_MinDistance2.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinDistance2, &CustomTickrate::minimum_distance2);
                Features::Tickrate_MinDistance3.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinDistance3, &CustomTickrate::minimum_distance2);
                Features::Tickrate_MinDistance4.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinDistance4, &CustomTickrate::minimum_distance2);
                Features::Tickrate_MinDistance5.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinDistance5, &CustomTickrate::minimum_distance2);
                Features::Tickrate_MinDistance6.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinDistance6, &CustomTickrate::minimum_distance2);
                Features::Tickrate_MinDistance7.Patch(MegaGuard::Addresses::Hooks::Features::Tickrate_MinDistance7, &CustomTickrate::minimum_distance2);
            }
            inline void CustomTickrateUnpatch()
            {
                Features::Tickrate_Frametime1.Unpatch();
                Features::Tickrate_Frametime2.Unpatch();
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

                Features::Tickrate_MinDistance1.Unpatch();
                Features::Tickrate_MinDistance2.Unpatch();
                Features::Tickrate_MinDistance3.Unpatch();
                Features::Tickrate_MinDistance4.Unpatch();
                Features::Tickrate_MinDistance5.Unpatch();
                Features::Tickrate_MinDistance6.Unpatch();
                Features::Tickrate_MinDistance7.Unpatch();
            }
        }
    }
}