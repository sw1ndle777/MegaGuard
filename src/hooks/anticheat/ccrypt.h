#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace AntiCheat
        {
#define ROTL16(x,y) ((uint16_t)((((uint16_t)(x))<<((y)&15)) | (((uint16_t)(x))>>(16-((y)&15)))))
#define ROTR16(x,y) ((uint16_t)((((uint16_t)(x))>>((y)&15)) | (((uint16_t)(x))<<(16-((y)&15)))))
#define ROTL32(x,y) ((uint32_t)((((uint32_t)(x))<<((y)&31)) | (((uint32_t)(x))>>(32-((y)&31)))))
#define ROTR32(x,y) ((uint32_t)((((uint32_t)(x))>>((y)&31)) | (((uint32_t)(x))<<(32-((y)&31)))))

            using namespace MegaGuard::Addresses::Hooks::Anticheat::Crypto;
            uint32_t RC5S[26];
            uint32_t RC6S[84];
            int32_t UserKey;

            typedef void(__thiscall* PVF_SETUP)(struct CCrypt*);
            typedef void(__thiscall* PBF_ENCRYPT)(struct CCrypt*, void*, void*, INT);
            typedef void(__thiscall* PBF_DECRYPT)(struct CCrypt*, void*, void*, INT);


            struct CCrypt
            {
				int(__stdcall** _vptr_CCrypt)();//0x0
				PVF_SETUP m_pvfSetup;//0x4
				PBF_ENCRYPT m_pbfEncrypt;//0x8
				PBF_DECRYPT m_pbfDecrypt;//0xC  
				std::uint16_t m_usKey_RC5[26];//0x10
				std::uint32_t m_uiKey_RC5[26];//0x44
				std::uint32_t m_uiKey_RC6[84];//0xAC
				std::int32_t m_iSerialKey;//0x1F0
            };

            void __fastcall RC5_KeySetup(CCrypt* instance, std::uint32_t edx)
            {
                _call<void(__thiscall*)(CCrypt*, const unsigned char*)>(MegaGuard::Addresses::Hooks::Anticheat::Crypto::RC5::InternalKeySetup.get(), instance, MegaGuard::Addresses::Hooks::Anticheat::Crypto::RC5::K);
            }
            void __fastcall RC6_KeySetup(CCrypt* instance, std::uint32_t edx)
            {
                _call<void(__thiscall*)(CCrypt*, const unsigned char*, std::uint16_t)>(MegaGuard::Addresses::Hooks::Anticheat::Crypto::RC6::InternalKeySetup.get(), instance, MegaGuard::Addresses::Hooks::Anticheat::Crypto::RC6::K, 32);
            }

            std::uint32_t encrypt_tcp_header(std::uint32_t header)
            {
                header = ~header;  // Bitwise NOT
                header = (header << 13) | (header >> (32 - 13));  // Rotate left by 13
                header ^= 0xA5A5A5A5;  // XOR with constant
                header = ((header & 0xF0F0F0F0) >> 4) | ((header & 0x0F0F0F0F) << 4);  // Swap nibbles
                header += 0xCAFEBABE;  // Add constant for diffusion
                header = (header << 7) | (header >> (32 - 7));  // Rotate left by 7
                header ^= (header >> 16);  // XOR mix upper and lower bits
                header = ((header & 0xAAAAAAAA) >> 1) | ((header & 0x55555555) << 1);  // Swap alternating bits
                return header;
            }


            std::uint32_t decrypt_tcp_header(std::uint32_t encrypted_header)
            {
                encrypted_header = ((encrypted_header & 0xAAAAAAAA) >> 1) | ((encrypted_header & 0x55555555) << 1);  // Reverse alternating bit swap
                encrypted_header ^= (encrypted_header >> 16);  // Reverse XOR mix
                encrypted_header = (encrypted_header >> 7) | (encrypted_header << (32 - 7));  // Rotate right by 7
                encrypted_header -= 0xCAFEBABE;  // Subtract constant
                encrypted_header = ((encrypted_header & 0xF0F0F0F0) >> 4) | ((encrypted_header & 0x0F0F0F0F) << 4);  // Reverse nibble swap
                encrypted_header ^= 0xA5A5A5A5;  // Reverse XOR with constant
                encrypted_header = (encrypted_header >> 13) | (encrypted_header << (32 - 13));  // Rotate right by 13
                encrypted_header = ~encrypted_header;  // Bitwise NOT to restore original
                return encrypted_header;
            }

            union STcpPacketHeader
            {
                struct
                {
                    uint32_t bogus : 4;
                    uint32_t sessionId : 14;
                    uint32_t size : 11;
                    uint32_t crypt : 3;
                };

                uint32_t data;

                STcpPacketHeader()
                {
                    memset(this, 0, sizeof(STcpPacketHeader));
                }

                STcpPacketHeader(uint32_t data)
                {
                    memset(this, 0, sizeof(STcpPacketHeader));
                    this->data = data;
                }
            };

            void __fastcall CCrypt_Encrypt(CCrypt* instance, std::uint32_t edx, void* pvIn_, void* pvOut_, std::int32_t iLen_)
            {
                //auto return_address = reinterpret_cast<std::uint32_t>(_ReturnAddress());
                //MegaGuard::EventLog->Debug(nostd::source_location::current(), "ENCRYPT KEY: {}", instance->m_iSerialKey);
                /*if (return_address == 0x00E135B1 || return_address == 0x00E13A73 || return_address == 0x00E12FD6 || return_address == 0x00E12D34)
                {
                    if (iLen_ == 4)
                    {
						MegaGuard::EventLog->Debug(nostd::source_location::current(), "ENCRYPT KEY: {}", instance->m_iSerialKey);
                        *reinterpret_cast<std::uint32_t*>(pvIn_) = encrypt_tcp_header(*reinterpret_cast<std::uint32_t*>(pvIn_));
                    }
                        
                }
                */
                    

                instance->m_pbfEncrypt(instance, pvIn_, pvOut_, iLen_);
            }
			static auto decrypt_retn_addy = enc_ptr(0x00E13500);
            void __fastcall CCrypt_Decrypt(CCrypt* instance, std::uint32_t edx, void* pvIn_, void* pvOut_, std::int32_t iLen_)
            {
#if defined(__clang__) && defined(_MSC_VER)  // Clang with MSVC compatibility
                auto return_address = reinterpret_cast<std::uint32_t>(__builtin_return_address(0));
#elif defined(_MSC_VER)  // Pure MSVC Compiler (No Clang)
                auto return_address = reinterpret_cast<std::uint32_t>(_ReturnAddress());
#elif defined(__clang__) || defined(__GNUC__)  // Pure Clang or GCC
                auto return_address = reinterpret_cast<std::uint32_t>(__builtin_return_address(0));
#endif
                instance->m_pbfDecrypt(instance, pvIn_, pvOut_, iLen_);
               
                if (return_address == decrypt_retn_addy.get() && iLen_ == 4)
                {
                   // MegaGuard::EventLog->Debug(nostd::source_location::current(), "DECRYPT KEY: {}", instance->m_iSerialKey);
                    *reinterpret_cast<std::uint32_t*>(pvOut_) = decrypt_tcp_header(*reinterpret_cast<std::uint32_t*>(pvOut_));
                }       
            }
        }
    }
}