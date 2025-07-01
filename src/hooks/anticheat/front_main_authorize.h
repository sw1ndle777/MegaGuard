#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace AntiCheat
        {

			struct WindowsVersionInfo
			{
				DWORD dwMajorVersion;
				DWORD dwMinorVersion;
				DWORD dwBuildNumber;
			};
			struct FingerprintHWID {
				WindowsVersionInfo winInfo;
				char bios_uuid[36];
				char baseboard_serial[32];
				char baseboard_model[128];
				char cpu_id[24];
				char cpu_model[64];
				char ram_serials[4][32];
				char ram_part_numbers[4][32];
				char disk_serials[4][32];
				char disk_models[4][64];
			};
			struct MainAuthorizeReq
			{
				SCommandHeader m_tHeader;
				uint32_t auth_key;
				uint32_t auth_key2;
				uint32_t server_id;
				uint8_t netVersion1;
				uint8_t netVersion2;
				uint8_t netVersion3;
				uint8_t netVersion4;
				//FingerprintHWID hwid;
			};
			struct FrontChannelInfoReq
			{
				SCommandHeader m_tHeader;
			};
			
			using tGetNetMgr = CNetMgr* (__cdecl*)();
			
			void __cdecl MainAuthorize(uint8_t extra, int a2)
			{
				static auto GetNetMgr = reinterpret_cast<tGetNetMgr>(Addresses::Hooks::Anticheat::GameManagers::NetMgr::Get.get());
				MainAuthorizeReq authReq;

				authReq.m_tHeader.order = 68;
				authReq.m_tHeader.extra = 37;
				authReq.m_tHeader.option = 2;

				authReq.auth_key = GetNetMgr()->auth_key;
				authReq.auth_key2 = GetNetMgr()->auth_key2;
				authReq.server_id = GetNetMgr()->serverinfo_sid;
				authReq.netVersion1 = 3;
				authReq.netVersion2 = 0;
				authReq.netVersion3 = 1;
				authReq.netVersion4 = 0;
				if (Globals::SendPacket(GetNetMgr(), (char*)&authReq, 20) < 0)
				{

				}	
			}
			using tAuthorizeDisconnectAll = void(__thiscall*)(CNetMgr* instance, const char* msg, uint32_t type1, uint32_t tyep2);
			static auto dwAuthorizeDisconnectAll = enc_ptr(0x00AB5F60);

			bool __cdecl RequestChannelInfo()
			{
				static auto GetNetMgr = reinterpret_cast<tGetNetMgr>(Addresses::Hooks::Anticheat::GameManagers::NetMgr::Get.get());
				FrontChannelInfoReq channelInfoReq;

				channelInfoReq.m_tHeader.order = 23;
				if (GetNetMgr()->CFrontConnectorTcp)
					return (GetNetMgr()->CFrontConnectorTcp->_vptr_CConnector->Send(GetNetMgr()->CFrontConnectorTcp, (char*)&channelInfoReq, 0, 0, 0) >= 0);
				else 
					return false;
			}
			void __cdecl FrontAuthorizeAck(CCommand* cmd)
			{
				static auto GetNetMgr = reinterpret_cast<tGetNetMgr>(Addresses::Hooks::Anticheat::GameManagers::NetMgr::Get.get());
				static auto AuthorizeDisconnectAll = reinterpret_cast<tAuthorizeDisconnectAll>(dwAuthorizeDisconnectAll.get());
				switch (cmd->m_tHeader.extra)
				{
					case 1:
					case 37:
						if (cmd->m_tHeader.extra == 1)
						{
							GetNetMgr()->auth_key = *reinterpret_cast<std::uint32_t*>(cmd->m_acData);
							GetNetMgr()->auth_key2 = *reinterpret_cast<std::uint32_t*>(cmd->m_acData[4]);
							GetNetMgr()->grade = cmd->m_tHeader.option;
						}
						if (GetNetMgr()->auth_key && GetNetMgr()->auth_key2)
						{
							if (!RequestChannelInfo())
								AuthorizeDisconnectAll(GetNetMgr(), "LOGIN_AUTHORIZE_FAIL", 5, 41);
						}
						else
							AuthorizeDisconnectAll(GetNetMgr(), "LOGIN_AUTHORIZE_DONT_EXIST", 6, 41);
				}
			}
        }
    }
}