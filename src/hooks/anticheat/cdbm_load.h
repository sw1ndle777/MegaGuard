#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace AntiCheat
        {
            using namespace MegaGuard::Addresses::Hooks::Anticheat;
           
            static auto cdbm_idk = enc_ptr(0x42DDB0);
            static auto cdbm_parse = enc_ptr(0x42F480);
            inline bool __fastcall CDBMLoad(int _thisptr, int edx, const char* path)
            {
                _call<void(__thiscall*)(int)>(cdbm_idk.get(), _thisptr);
                std::ifstream file(path, std::ios::in | std::ios::binary);
                char buffer[64];
                file.rdbuf()->pubsetbuf(buffer, sizeof(buffer));
                auto opened = _call<bool(__thiscall*)(int, const char*)>(cdbm_parse.get(), _thisptr, path);
                file.close();
                return opened;
            }
        }
    }
}