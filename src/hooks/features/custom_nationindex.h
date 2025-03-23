#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace Features
        {
            inline const char* GetNationIndex()
            {
                return MegaGuard::Addresses::Features::NationIndexIsRom ? "ROM" : "";
            }
            inline const char* GetWindowTitle()
            {
                return "MegaVolts Online";
            }
        }
    }
}