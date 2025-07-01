#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace AntiCheat
        {
			int nocrt_iscntrl(int c)
			{
				if(c <= 0x1f) /* from NUL to US */
					return 1;

				if(c == 0x7f) /* DEL */
					return 1;

				return 0;
			}
			struct FrontLoginAuthorizeReq
			{
				SCommandHeader m_tHeader;
				uint32_t  cryptoKey;
				char      password[40];
				uint32_t  serverTime;
				char      username[68];
			};
			struct FrontEngineConnectAck
			{
				int32_t cryptoKey;
				uint32_t serverTime;
			};
			struct MainEngineConnectAck
			{
				int32_t cryptoKey;
				UniqueId uniqueId;
			};
			struct CastEngineConnectAck
			{
				int32_t cryptoKey;
			};
			using tGetNetMgr = CNetMgr* (__cdecl*)();
			using tGetUnitContainer = CUnitContainer * (__cdecl*)();
			using tInitUnitContainerClass = void(__thiscall*)(CUnitContainer* instance, int a2);
			static auto initContainerClass = enc_ptr(0x009115C0);
            inline bool LoginAuthorizeReq()
            {
				static auto GetNetMgr = reinterpret_cast<tGetNetMgr>(Addresses::Hooks::Anticheat::GameManagers::NetMgr::Get.get());
				auto UIMsgBox = reinterpret_cast<CUIMsgBox*>(Addresses::Hooks::Anticheat::UIMsgBox::Get.get());
				auto loginUsername = GetNetMgr()->loginUsername;
				auto loginPassword = GetNetMgr()->loginPassword;
				int loginUsernameLength = 0, loginPasswordLength = 0;
				for (int i = 0; i < 67 && loginUsername[i] && loginUsername[i] != ' '; i++)
				{
                    if (nocrt_iscntrl(loginUsername[i]))
					{
						UIMsgBox->_vptr_CUIMsgBox->Create(UIMsgBox,
														  "LOGIN_NULL_ID",
														  0,
														  0,
														  1,
														  0);
						return false;
					}
					loginUsernameLength++;
				}

				for (int i = 0; i < 39 && loginPassword[i] && loginPassword[i] != ' '; i++)
				{
					if (nocrt_iscntrl(loginPassword[i]))
					{
						UIMsgBox->_vptr_CUIMsgBox->Create(UIMsgBox,
														  "ROOM_CONFIG_WRONGPASSWORD",
														  0,
														  0,
														  1,
														  0);
						return false;
					}
					loginPasswordLength++;
				}
				loginUsername[loginUsernameLength] = 0;
				loginPassword[loginPasswordLength] = 0;
				if (loginUsernameLength < 1)
				{
					UIMsgBox->_vptr_CUIMsgBox->Create(UIMsgBox,
													  "LOGIN_ID_SHORTLEN",
													  0,
													  0,
													  1,
													  0);
					return false;
				}
				if (loginPasswordLength < 1)
				{
					UIMsgBox->_vptr_CUIMsgBox->Create(UIMsgBox,
													  "LOGIN_PASSWD_SHORTLEN",
													  0,
													  0,
													  1,
													  0);
					return false;
				}
				FrontLoginAuthorizeReq loginReq;

				loginReq.m_tHeader.order = 22;
				loginReq.m_tHeader.extra = 52;

				loginReq.cryptoKey = GetNetMgr()->CFrontConnectorTcp->m_iSerialKey;
				nocrt_strcpy(loginReq.username, loginUsername);
				nocrt_strcpy(loginReq.password, loginPassword);
				if (loginUsernameLength + 1 < 68)
					for (int i = loginUsernameLength + 1; i < 67; i++)
						loginReq.username[i] = (int)((double)rand() * 254.0f / 32768.0f) + 1;

				if (loginPasswordLength + 1 < 40)
					for (int i = loginPasswordLength + 1; i < 39; i++)
						loginReq.password[i] = (int)((double)rand() * 254.0f / 32768.0f) + 1;
				loginReq.serverTime = GetNetMgr()->server_time;
				static auto ConnectorTcp = GetNetMgr()->CFrontConnectorTcp;
				if (ConnectorTcp)
				{
					if (ConnectorTcp->_vptr_CConnector->Send(ConnectorTcp, (char*)&loginReq, 116, 0, 4) < 0)
						return false;
					else
						return true;
				}
				return false;
            }


			void __cdecl NetworkConnectAck(CCommand *cmd)
			{
				static auto GetNetMgr = reinterpret_cast<tGetNetMgr>(Addresses::Hooks::Anticheat::GameManagers::NetMgr::Get.get());
				static auto GetUnitContainer = reinterpret_cast<tGetUnitContainer>(Addresses::Hooks::Anticheat::GameManagers::UnitContainer::Get.get());
				if (cmd->m_tHeader.extra)
				{
					if (cmd->m_tHeader.extra == 54)
					{
						auto engineConnect = reinterpret_cast<CastEngineConnectAck*>(cmd->m_acData);
						GetNetMgr()->CCastConnectorTcp->m_iRegisterIndex = -2;
					}
					else if (cmd->m_tHeader.extra == 34)
					{
						auto engineConnect = reinterpret_cast<FrontEngineConnectAck*>(cmd->m_acData);
						GetNetMgr()->server_time = engineConnect->serverTime;
						GetNetMgr()->CFrontConnectorTcp->m_iSerialKey = engineConnect->cryptoKey;
						if (LoginAuthorizeReq())
						{
							nocrt_memset(GetNetMgr()->loginPassword, 0, 40);
							nocrt_memset(GetNetMgr()->loginKey, 0, 500);
						}
					}
				}
				else
				{
					auto engineConnect = reinterpret_cast<MainEngineConnectAck*>(cmd->m_acData);
					GetNetMgr()->CMainConnectorTcp->m_iSessionId2 = engineConnect->uniqueId.session;
					GetNetMgr()->CMainConnectorTcp->m_iSessionId = engineConnect->uniqueId.session;
					GetNetMgr()->CMainConnectorTcp->m_iSerialKey = engineConnect->cryptoKey;

					if (GetNetMgr()->CFrontConnectorTcp)
					{
						if (!GetNetMgr()->CFrontConnectorTcp->m_pkSocket->m_iNetworkState)
							GetNetMgr()->CFrontConnectorTcp->m_pkSocket->m_iNetworkState = 1;

						GetNetMgr()->CFrontConnectorTcp->_vptr_CConnector->sub_AB0760(GetNetMgr()->CFrontConnectorTcp, GetNetMgr()->CFrontConnectorTcp->m_pkSocket->m_skSocket, true);
						GetNetMgr()->byte103D = 1;
					}
					GetNetMgr()->CMainConnectorTcp->m_iRegisterIndex = cmd->m_tHeader.option; // server_id
					GetNetMgr()->CExConnectorUdp->m_iRegisterIndex = cmd->m_tHeader.option; // server_id
					GetNetMgr()->CExConnectorUdp->m_iSessionId2 = engineConnect->uniqueId.session;

					static auto my_unique_id = UniqueId(engineConnect->uniqueId.session, cmd->m_tHeader.option);
					for (int i = 0; i < 5; i++)
					{
						auto m_pGamePlayer = GetUnitContainer()->m_pGamePlayer[i];
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->uniqueId = my_unique_id;
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0bd0 = 0;//0x0000
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0bd4 = 0;//0x0004
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0bd8 = 0;//0x0008
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0bdc = 0;//0x000C
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0be0 = 0;//0x0010
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0be4 = 0;//0x0014
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0be8 = 0;//0x0018
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0bec = 0;//0x001C
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0bf0 = 0;//0x0020
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0bf4 = 0;//0x0024
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0bf8 = 0;//0x0028
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0bfc = 0;//0x002C
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c00 = 0;//0x0030
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c04 = 0;//0x0034
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c08 = 0;//0x0038
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c0c = 0;//0x003C
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c10 = 0;//0x0040
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c14 = 0;//0x0044
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c18 = 0;//0x0048
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c1c = 0;//0x004C
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c20 = 0;//0x0050
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c24 = 0;//0x0054
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c28 = 0;//0x0058
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c2c = 0;//0x005C
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c30 = 0;//0x0060
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c34 = 0;//0x0064
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c38 = 0;//0x0068
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c3c = 0;//0x006C
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c40 = 0;//0x0070
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c44 = 0;//0x0074
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c48 = 0;//0x0078
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c4c = 0;//0x007C

						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0b94 = 0;
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0b98 = 0;
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0b9c = 0;
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0ba0 = 0;

						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0ca4 = 0;
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0b88 = 0;
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0b8c = 0;
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c14 &= 1u;

						if (m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0bd0 >= 0)
						{
							m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c14 &= ~1u;
							m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0bd4 = (0 << 14) | m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0bd4 & 0x3FFF;
						}
						else
						{
							m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0c14 |= 1u;
							m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0bd4 = (0xFFFFC000 * 0) | m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0bd4 & 0x3FFF;
						}

						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0ca4 = 0;
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword0b90 = 0;
						m_pGamePlayer->m_pExPlayer->m_kRoomInfo->dword1110 = 0;
					}//zero out character's classes info

					static auto InitContainerClass = reinterpret_cast<tInitUnitContainerClass>(initContainerClass.get());
					if (!GetUnitContainer()->m_pGamePlayer[5])
						InitContainerClass(GetUnitContainer(), 0);

					GetNetMgr()->CMainConnectorTcp->m_bIsConnected = 1;
					GetNetMgr()->lastConnectTick = 0;
				}
			}
        }
    }
}