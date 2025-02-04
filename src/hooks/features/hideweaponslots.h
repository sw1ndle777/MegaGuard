#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace Features
        {
            inline void HideWeaponSlots(midhook::regs_t ctx)
            {
                std::uint32_t edx_value = *reinterpret_cast<std::uint32_t*>(ctx.ebp - 0x584);
                std::uint32_t eax_value = *reinterpret_cast<std::uint32_t*>(ctx.ebp - 0x8AC);
                std::uint32_t calculated_offset = edx_value * 0x4C;
                *reinterpret_cast<std::uint8_t*>(calculated_offset + eax_value + 0x38) = 1;
            }
        }
    }
}