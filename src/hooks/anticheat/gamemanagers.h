#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace AntiCheat
        {
            using namespace MegaGuard::Addresses::Hooks::Anticheat::GameManagers;
            

            std::uint32_t* __cdecl GetCRoom()
            {


#if defined(__clang__) && defined(_MSC_VER)  // Clang with MSVC compatibility
                auto return_address = reinterpret_cast<std::uint32_t>(__builtin_return_address(0));
#elif defined(_MSC_VER)  // Pure MSVC Compiler (No Clang)
                auto return_address = reinterpret_cast<std::uint32_t>(_ReturnAddress());
#elif defined(__clang__) || defined(__GNUC__)  // Pure Clang or GCC
                auto return_address = reinterpret_cast<std::uint32_t>(__builtin_return_address(0));
#endif
                
                if (Room::Encrypted)
                {
                    if (Room::whitelist_return_addres.contains(return_address))
                        return Room::Decrypted;
                        //return Room::ptr_encrypt ? Room::ptr_encrypt->process<std::uint32_t*>(Room::Encrypted) : nullptr;
                    else
                    {
                        if (return_address >= MegaGuard::Globals::g_AntiCheatModuleBase && return_address < MegaGuard::Globals::g_AntiCheatModuleBase + MegaGuard::Globals::g_AntiCheatModuleSize)
                            return Room::Decrypted;
                            //return Room::ptr_encrypt ? Room::ptr_encrypt->process<std::uint32_t*>(Room::Encrypted) : nullptr;
                        else
                        {
                            MegaGuard::EventLog->Debug(nostd::source_location::current(), "call GetRoom() from non whitelisted 0x{:08X}, acbase: 0x{:08X} - 0x{:08X}", return_address, MegaGuard::Globals::g_AntiCheatModuleBase, MegaGuard::Globals::g_AntiCheatModuleBase + MegaGuard::Globals::g_AntiCheatModuleSize);
                            return  nullptr;
                        } 
                    }
                }
                EnterCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);
                if (!Room::Encrypted)
                {
                    std::uint32_t* room_allocated_memory = new std::uint32_t[552];
                    if (!room_allocated_memory)
                    {
                        LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);
                        MegaGuard::EventLog->Debug(nostd::source_location::current(), "Failed to allocate memory for CRoom");
                        return nullptr;
                    }
                    //if (!Room::ptr_encrypt)
                    //    Room::ptr_encrypt = new pointer_encryption();

                    Room::Decrypted = _call<std::uint32_t * (__thiscall*)(void*)>(Room::Init.get(), room_allocated_memory);
                    MegaGuard::EventLog->Debug(nostd::source_location::current(), "CRoom::Decrypted: 0x{:08X}", reinterpret_cast<std::uint32_t>(Room::Decrypted));
                    Room::Encrypted = Room::Decrypted;//Room::ptr_encrypt->process<std::uint32_t*>(Room::Decrypted);
                    if (g_ipcClient) g_ipcClient->OnManagerReady();


                }
                LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);
                return Room::Decrypted;
                //return Room::ptr_encrypt ? Room::ptr_encrypt->process<std::uint32_t*>(Room::Encrypted) : nullptr;
            }
            
            void __cdecl DestroyCRoom()
            {
                if (Room::Encrypted)
                {
                    EnterCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);
                    if (Room::Encrypted)
                    {
                        Memory::CallVFunc<void>(Room::Encrypted, 1);//free CRoom
                        Room::Encrypted = nullptr;
                        Room::ptr_encrypt = nullptr;
                        delete Room::ptr_encrypt;
                    }
                    LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::MyCriticalSection);
                }
            }

            std::uint32_t* __cdecl GetCUnitContainer()
            {
#if defined(__clang__) && defined(_MSC_VER)  // Clang with MSVC compatibility
                auto return_address = reinterpret_cast<std::uint32_t>(__builtin_return_address(0));
#elif defined(_MSC_VER)  // Pure MSVC Compiler (No Clang)
                auto return_address = reinterpret_cast<std::uint32_t>(_ReturnAddress());
#elif defined(__clang__) || defined(__GNUC__)  // Pure Clang or GCC
                auto return_address = reinterpret_cast<std::uint32_t>(__builtin_return_address(0));
#endif
                if (UnitContainer::Encrypted)
                {
                    if (UnitContainer::whitelist_return_addres.contains(return_address))
                        return UnitContainer::Decrypted;
                        //return UnitContainer::ptr_encrypt ? UnitContainer::ptr_encrypt->process<std::uint32_t*>(UnitContainer::Encrypted) : nullptr;
                    else
                    {
                        if (return_address >= MegaGuard::Globals::g_AntiCheatModuleBase && return_address < MegaGuard::Globals::g_AntiCheatModuleBase + MegaGuard::Globals::g_AntiCheatModuleSize)
                            return UnitContainer::Decrypted;
                            //return UnitContainer::ptr_encrypt ? UnitContainer::ptr_encrypt->process<std::uint32_t*>(UnitContainer::Encrypted) : nullptr;
                        else
                        {
                            //MegaGuard::EventLog->Debug(nostd::source_location::current(), "call GetCUnitContainer() from non whitelisted 0x{:08X}", return_address);
                            return nullptr;
                        }
                    }
                }
                EnterCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitContainer::MyCriticalSection);
                if (!UnitContainer::Encrypted)
                {
                    std::uint32_t* allocated_memory = new std::uint32_t[116];
                    if (!allocated_memory)
                    {
                        LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitContainer::MyCriticalSection);
                        return nullptr;
                    }
                    //if (!UnitContainer::ptr_encrypt)
                    //    UnitContainer::ptr_encrypt = new pointer_encryption();
                    UnitContainer::Decrypted = _call<std::uint32_t * (__thiscall*)(void*)>(UnitContainer::Init.get(), allocated_memory);
                    MegaGuard::EventLog->Debug(nostd::source_location::current(), "CUnitContainer::Decrypted: 0x{:08X}", reinterpret_cast<std::uint32_t>(UnitContainer::Decrypted));
                    UnitContainer::Encrypted = UnitContainer::Decrypted;//UnitContainer::ptr_encrypt->process<std::uint32_t*>(UnitContainer::Decrypted);
                    if (g_ipcClient) g_ipcClient->OnManagerReady();
                }
                LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitContainer::MyCriticalSection);
                return UnitContainer::Decrypted;
                //return UnitContainer::ptr_encrypt ? UnitContainer::ptr_encrypt->process<std::uint32_t*>(UnitContainer::Encrypted) : nullptr;
            }
            void __cdecl DestroyCUnitContainer()
            {
                if (UnitContainer::Encrypted)
                {
                    EnterCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitContainer::MyCriticalSection);
                    if (UnitContainer::Encrypted)
                    {
                        Memory::CallVFunc<void>(UnitContainer::Encrypted, 1);//free CUnitContainer
                        UnitContainer::Encrypted = nullptr;
                        UnitContainer::ptr_encrypt = nullptr;
                        delete UnitContainer::ptr_encrypt;
                    }
                    LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitContainer::MyCriticalSection);
                }
            }

            std::uint32_t* __cdecl GetCUnitMgr()
            {
#if defined(__clang__) && defined(_MSC_VER)  // Clang with MSVC compatibility
                auto return_address = reinterpret_cast<std::uint32_t>(__builtin_return_address(0));
#elif defined(_MSC_VER)  // Pure MSVC Compiler (No Clang)
                auto return_address = reinterpret_cast<std::uint32_t>(_ReturnAddress());
#elif defined(__clang__) || defined(__GNUC__)  // Pure Clang or GCC
                auto return_address = reinterpret_cast<std::uint32_t>(__builtin_return_address(0));
#endif
                if (UnitMgr::Encrypted)
                {
                    if (UnitMgr::whitelist_return_addres.contains(return_address))
                        return UnitMgr::Decrypted;
                        //return UnitMgr::ptr_encrypt ? UnitMgr::ptr_encrypt->process<std::uint32_t*>(UnitMgr::Encrypted) : nullptr;
                    else
                    {
                        if (return_address >= MegaGuard::Globals::g_AntiCheatModuleBase && return_address < MegaGuard::Globals::g_AntiCheatModuleBase + MegaGuard::Globals::g_AntiCheatModuleSize)
                            return UnitMgr::Decrypted;
                            //return UnitMgr::ptr_encrypt ? UnitMgr::ptr_encrypt->process<std::uint32_t*>(UnitMgr::Encrypted) : nullptr;
                        else
                        {
                            //MegaGuard::EventLog->Debug(nostd::source_location::current(), "call GetCUnitMgr() from non whitelisted 0x{:08X}", return_address);
                            return nullptr;
                        }
                    }
                }
                EnterCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitMgr::MyCriticalSection);
                if (!UnitMgr::Encrypted)
                {
                    std::uint32_t* allocated_memory = new std::uint32_t[188];
                    if (!allocated_memory)
                    {
                        LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitMgr::MyCriticalSection);
                        return nullptr;
                    }
                    //if (!UnitMgr::ptr_encrypt)
                    //    UnitMgr::ptr_encrypt = new pointer_encryption();
                    UnitMgr::Decrypted = _call<std::uint32_t * (__thiscall*)(void*)>(UnitMgr::Init.get(), allocated_memory);
                    MegaGuard::EventLog->Debug(nostd::source_location::current(), "CUnitMgr::Decrypted: 0x{:08X}", reinterpret_cast<std::uint32_t>(UnitMgr::Decrypted));
                    UnitMgr::Encrypted = UnitMgr::Decrypted;//UnitMgr::ptr_encrypt->process<std::uint32_t*>(UnitMgr::Decrypted);
                    if (g_ipcClient) g_ipcClient->OnManagerReady();
                }
                LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitMgr::MyCriticalSection);
                return UnitMgr::Decrypted;
                //return UnitMgr::ptr_encrypt ? UnitMgr::ptr_encrypt->process<std::uint32_t*>(UnitMgr::Encrypted) : nullptr;
            }
            void __cdecl DestroyCUnitMgr()
            {
                if (UnitMgr::Encrypted)
                {
                    EnterCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitMgr::MyCriticalSection);
                    if (UnitMgr::Encrypted)
                    {
                        Memory::CallVFunc<void>(UnitMgr::Encrypted, 1);//free CUnitMgr
                        UnitMgr::Encrypted = nullptr;
                        UnitMgr::ptr_encrypt = nullptr;
                        delete UnitMgr::ptr_encrypt;
                    }
                    LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitMgr::MyCriticalSection);
                }
            }

            std::uint32_t* __stdcall GetCNetMgr()
            {
#if defined(__clang__) && defined(_MSC_VER)  // Clang with MSVC compatibility
                auto return_address = reinterpret_cast<std::uint32_t>(__builtin_return_address(0));
#elif defined(_MSC_VER)  // Pure MSVC Compiler (No Clang)
                auto return_address = reinterpret_cast<std::uint32_t>(_ReturnAddress());
#elif defined(__clang__) || defined(__GNUC__)  // Pure Clang or GCC
                auto return_address = reinterpret_cast<std::uint32_t>(__builtin_return_address(0));
#endif
                if (NetMgr::Encrypted)
                {
                    if (NetMgr::whitelist_return_addres.contains(return_address))
                        return NetMgr::Decrypted;
                        //return NetMgr::ptr_encrypt ? NetMgr::ptr_encrypt->process<std::uint32_t*>(NetMgr::Encrypted) : nullptr;
                    else
                    {
                        if (return_address >= MegaGuard::Globals::g_AntiCheatModuleBase && return_address < MegaGuard::Globals::g_AntiCheatModuleBase + MegaGuard::Globals::g_AntiCheatModuleSize)
                            return NetMgr::Decrypted;
                            //return NetMgr::ptr_encrypt ? NetMgr::ptr_encrypt->process<std::uint32_t*>(NetMgr::Encrypted) : nullptr;
                        else
                        {
                            //MegaGuard::EventLog->Debug(nostd::source_location::current(), "call GetCNetMgr() from non whitelisted 0x{:08X}", return_address);
                            return nullptr;//return nullptr;
                        }
                    }
                }
                EnterCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::NetMgr::MyCriticalSection);
                if (!NetMgr::Encrypted)
                {
                    std::uint32_t* allocated_memory = new (std::nothrow) std::uint32_t[0x1538]; // old 0x14F8 aka last 0x14F4 offset now 0x1538 because i added uint8_t auth_token[64]
                    if (!allocated_memory)
                    {
                        LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::NetMgr::MyCriticalSection);
                        return nullptr;
                    }
                    //if (!NetMgr::ptr_encrypt)
                    //    NetMgr::ptr_encrypt = new pointer_encryption();

                    NetMgr::Decrypted = _call<std::uint32_t * (__thiscall*)(void*)>(NetMgr::Init.get(), allocated_memory);
                    MegaGuard::EventLog->Debug(nostd::source_location::current(), "CNetMgr::Decrypted: 0x{:08X}", reinterpret_cast<std::uint32_t>(NetMgr::Decrypted));
                    NetMgr::Encrypted = NetMgr::Decrypted;//NetMgr::ptr_encrypt->process<std::uint32_t*>(NetMgr::Decrypted);
                    if (g_ipcClient) g_ipcClient->OnManagerReady();
                }
                LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::NetMgr::MyCriticalSection);
                return NetMgr::Decrypted;
                //return NetMgr::ptr_encrypt ? NetMgr::ptr_encrypt->process<std::uint32_t*>(NetMgr::Encrypted) : nullptr;
            }
            void __cdecl DestroyCNetMgr()
            {
                if (NetMgr::Encrypted)
                {
                    EnterCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::NetMgr::MyCriticalSection);
                    if (NetMgr::Encrypted)
                    {
                        Memory::CallVFunc<void>(NetMgr::Encrypted, 1);//free CNetMgr
                        NetMgr::Encrypted = nullptr;
                        NetMgr::ptr_encrypt = nullptr;
                        delete NetMgr::ptr_encrypt;
                    }
                    LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::NetMgr::MyCriticalSection);
                }
            }

            std::uint32_t* __cdecl GetCDynamics()
            {
#if defined(__clang__) && defined(_MSC_VER)  // Clang with MSVC compatibility
                auto return_address = reinterpret_cast<std::uint32_t>(__builtin_return_address(0));
#elif defined(_MSC_VER)  // Pure MSVC Compiler (No Clang)
                auto return_address = reinterpret_cast<std::uint32_t>(_ReturnAddress());
#elif defined(__clang__) || defined(__GNUC__)  // Pure Clang or GCC
                auto return_address = reinterpret_cast<std::uint32_t>(__builtin_return_address(0));
#endif
                if (Dynamics::Encrypted)
                {
                    if (Dynamics::whitelist_return_addres.contains(return_address))
                        return Dynamics::Decrypted;
                        //return Dynamics::ptr_encrypt ? Dynamics::ptr_encrypt->process<std::uint32_t*>(Dynamics::Encrypted) : nullptr;
                    else
                    {
                        if (return_address >= MegaGuard::Globals::g_AntiCheatModuleBase && return_address < MegaGuard::Globals::g_AntiCheatModuleBase + MegaGuard::Globals::g_AntiCheatModuleSize)
                            return Dynamics::Decrypted;
                            //return Dynamics::ptr_encrypt ? Dynamics::ptr_encrypt->process<std::uint32_t*>(Dynamics::Encrypted) : nullptr;
                        else
                        {
                            //MegaGuard::EventLog->Debug(nostd::source_location::current(), "call GetCDynamics() from non whitelisted 0x{:08X}", return_address);
                            return nullptr;
                        }
                    }
                }
                EnterCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Dynamics::MyCriticalSection);
                if (!Dynamics::Encrypted)
                {
                    std::uint32_t* allocated_memory = new std::uint32_t[1760];
                    if (!allocated_memory)
                    {
                        LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Dynamics::MyCriticalSection);
                        return nullptr;
                    }
                    //if (!Dynamics::ptr_encrypt)
                    //    Dynamics::ptr_encrypt = new pointer_encryption();

                    Dynamics::Decrypted = _call<std::uint32_t * (__thiscall*)(void*)>(Dynamics::Init.get(), allocated_memory);
                    MegaGuard::EventLog->Debug(nostd::source_location::current(), "CDynamics::Decrypted: 0x{:08X}", reinterpret_cast<std::uint32_t>(Dynamics::Decrypted));
                    Dynamics::Encrypted = Dynamics::Decrypted;//Dynamics::ptr_encrypt->process<std::uint32_t*>(Dynamics::Decrypted);
                    if (g_ipcClient) g_ipcClient->OnManagerReady();
                }
                LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Dynamics::MyCriticalSection);
                return Dynamics::Decrypted;
                //return Dynamics::ptr_encrypt ? Dynamics::ptr_encrypt->process<std::uint32_t*>(Dynamics::Encrypted) : nullptr;
            }
            void __cdecl DestroyCDynamics()
            {
                if (Dynamics::Encrypted)
                {
                    EnterCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Dynamics::MyCriticalSection);
                    if (Dynamics::Encrypted)
                    {
                        Memory::CallVFunc<void>(Dynamics::Encrypted, 1);//free CDynamics
                        Dynamics::Encrypted = nullptr;
                        Dynamics::ptr_encrypt = nullptr;
                        delete Dynamics::ptr_encrypt;
                    }
                    LeaveCriticalSection(&MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Dynamics::MyCriticalSection);
                }
            }
        }
    }
}