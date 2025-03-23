#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace AntiCheat
        {
            using namespace MegaGuard::Addresses::Hooks::Anticheat::Heartbeat;
            std::uint64_t generate_key(const std::uint8_t* buffer, std::size_t size)
            {
                std::uint64_t key = 0x123456789ABCDEF0;
                std::size_t i = 0;
                while (i < size)
                {
                    key ^= buffer[i];
                    key = (key << 7) | (key >> (64 - 7));
                    key += (buffer[i] << (i % 8));
                    key = ~((key << 11) | (key >> (64 - 11)));
                    key ^= 0x5A5A5A5A5A5A5A5A;
                    key = (key << 3) | (key >> (64 - 3));
                    key += buffer[i];
                    key ^= buffer[i] ^ (key >> (i % 64));
                    ++i;
                }
                return key;
            }

            std::uint64_t encrypt_key(std::uint64_t key)
            {
                std::uint64_t result = 0;
                std::size_t i = 0;
                while (i < 8) {
                    std::uint8_t byte = static_cast<std::uint8_t>(key & 0xFF);
                    byte ^= 0xAA;
                    byte = (byte << 4) | (byte >> 4);
                    byte ^= 0x55;
                    result |= (static_cast<std::uint64_t>(byte) << (i * 8));
                    key = (key >> 8);
                    ++i;
                }
                return result;
            }

            std::uint64_t decrypt_key(std::uint64_t key)
            {
                std::uint64_t result = 0;
                std::size_t i = 0;
                while (i < 8) {
                    std::uint8_t byte = static_cast<std::uint8_t>((key >> (i * 8)) & 0xFF);
                    byte ^= 0x55;
                    byte = (byte >> 4) | (byte << 4);
                    byte ^= 0xAA;
                    result |= (static_cast<std::uint64_t>(byte) << (i * 8));
                    ++i;
                }
                return result;
            }


            void xor_encrypt(std::uint8_t* buffer, std::size_t size, std::uint64_t key)
            {
                std::size_t i = 0;
                while (i < size)
                {
                    buffer[i] ^= static_cast<std::uint8_t>(key);
                    buffer[i] = ~buffer[i];
                    buffer[i] = (buffer[i] << 3) | (buffer[i] >> (8 - 3));
                    buffer[i] ^= static_cast<std::uint8_t>((key >> 4) & 0xFF);
                    buffer[i] = (buffer[i] & static_cast<std::uint8_t>(key)) | (~buffer[i] & ~static_cast<std::uint8_t>(key));
                    buffer[i] = (buffer[i] << 2) | (buffer[i] >> (8 - 2));
                    buffer[i] ^= static_cast<std::uint8_t>((key >> 8) & 0xFF);
                    buffer[i] = ((buffer[i] & 0xF0) >> 4) | ((buffer[i] & 0x0F) << 4);
                    buffer[i] = ((buffer[i] & 0xCC) >> 2) | ((buffer[i] & 0x33) << 2);
                    i++;
                }
            }

            void xor_decrypt(std::uint8_t* buffer, std::size_t size, std::uint64_t key)
            {
                std::size_t i = 0;
                while (i < size)
                {
                    buffer[i] = ((buffer[i] & 0xCC) >> 2) | ((buffer[i] & 0x33) << 2);
                    buffer[i] = ((buffer[i] & 0xF0) >> 4) | ((buffer[i] & 0x0F) << 4);
                    buffer[i] ^= static_cast<std::uint8_t>((key >> 8) & 0xFF);
                    buffer[i] = (buffer[i] >> 2) | (buffer[i] << (8 - 2));
                    buffer[i] = (buffer[i] & static_cast<std::uint8_t>(key)) | (~buffer[i] & ~static_cast<std::uint8_t>(key));
                    buffer[i] ^= static_cast<std::uint8_t>((key >> 4) & 0xFF);
                    buffer[i] = (buffer[i] >> 3) | (buffer[i] << (8 - 3));
                    buffer[i] = ~buffer[i];
                    buffer[i] ^= static_cast<std::uint8_t>(key);
                    i++;
                }
            }

            void __cdecl Heartbeat(std::uint32_t data)
            {
				auto header = *reinterpret_cast<SCommandHeader*>(data);
				auto key = encrypt_key(generate_key(reinterpret_cast<std::uint8_t*>(header.data), sizeof(SCommandHeader)));
                if (header.extra == 0) // receive private key
                {
                    std::uint8_t* encrypted_buf = reinterpret_cast<std::uint8_t*>(data + sizeof(SCommandHeader));
					xor_decrypt(encrypted_buf, 8, key);
					MegaGuard::EventLog->Debug(nostd::source_location::current(), "Received private key: 0x{:016X}", *reinterpret_cast<std::uint64_t*>(encrypted_buf));
                }
            }
        }
    }
}