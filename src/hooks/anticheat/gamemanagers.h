#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace AntiCheat
        {
            using namespace MegaGuard::Addresses::Hooks::Anticheat::GameManagers;
            std::uint32_t* __cdecl GetGRoom()
            {
                auto return_address = reinterpret_cast<std::uint32_t>(_ReturnAddress());
                if (Room::EncryptedRoom)
                {
                    if (Room::whitelist_return_addres.contains(return_address))
                        return Room::ptr_encrypt ? Room::ptr_encrypt->process<std::uint32_t*>(Room::EncryptedRoom) : nullptr;
                    else
                    {
                        if (return_address >= MegaGuard::Globals::g_AntiCheatModuleBase && return_address < MegaGuard::Globals::g_AntiCheatModuleBase + MegaGuard::Globals::g_AntiCheatModuleSize)
                            return Room::ptr_encrypt ? Room::ptr_encrypt->process<std::uint32_t*>(Room::EncryptedRoom) : nullptr;
                        else
                        {
                            //MegaGuard::EventLog->Debug(std::source_location::current(), "call GetGRoom() from non whitelisted 0x{:08X}", return_address);
                            return Room::ptr_encrypt ? Room::ptr_encrypt->process<std::uint32_t*>(Room::EncryptedRoom) : nullptr;
                        } 
                    }
                }
                EnterCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);

                try
                {
                    if (!Room::EncryptedRoom)
                    {
                        //GrdMem* buffer = new GrdMem(2208);
                        //std::uint32_t* room_allocated_memory = new (buffer->Data()) std::uint32_t[552];
                        std::uint32_t* room_allocated_memory = new (std::nothrow) std::uint32_t[552];
                        if (!room_allocated_memory)
                        {
                            LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);
                            return nullptr;
                        }
                        if (!Room::ptr_encrypt)
                            Room::ptr_encrypt = new pointer_encryption();
                        Room::DecryptedRoom = _call<std::uint32_t * (__thiscall*)(void*)>(
                            Room::InitCRoom, room_allocated_memory);
                        Room::EncryptedRoom = Room::ptr_encrypt->process<std::uint32_t*>(Room::DecryptedRoom);
                        //MegaGuard::EventLog->Debug(std::source_location::current(), "Room::DecryptedRoom: 0x{:08X}", reinterpret_cast<std::uint32_t>(Room::DecryptedRoom));
                        //MegaGuard::EventLog->Debug(std::source_location::current(), "Room::EncryptedRoom: 0x{:08X}", reinterpret_cast<std::uint32_t>(Room::EncryptedRoom));
                    }
                }
                catch (...)
                {
                    LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);
                    throw;
                }
                LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);
                return Room::ptr_encrypt ? Room::ptr_encrypt->process<std::uint32_t*>(Room::EncryptedRoom) : nullptr;
            }
            void __cdecl DestroyCRoom()
            {
                if (Room::EncryptedRoom)
                {
                    EnterCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);
                    if (Room::EncryptedRoom)
                    {
                        Memory::CallVFunc<void>(Room::EncryptedRoom, 1);//free CRoom
                        Room::EncryptedRoom = nullptr;
                        Room::ptr_encrypt = nullptr;
                        delete Room::ptr_encrypt;
                    }
                    LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);
                }
            }
        }
    }
}