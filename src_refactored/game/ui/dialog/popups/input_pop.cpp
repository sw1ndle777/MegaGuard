// =============================================================================
// DlgInputPopup - Edit box input popup handler
// =============================================================================
// Hooks:
//   - InitPopupTitles (0x6554B0): sets dialog title based on popup type
//   - DlgInputHandle  (0x6556B0): handles OK button on the input popup
//     - type 0: join room with password (else branch, includes FormatStringSafe)
//     - type 1: party/clan join with password
//     - type 2: room config password change (with badword filter)
//     - type 3: callback-based password delivery
//     - type 4: join room by number
//     - type 5: character name entry (custom)
//     - type 6: 2FA code entry (custom)
// =============================================================================
#include "pch.hpp"
#include "game/ui/dialog/popups/input_pop.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "platform/memory.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::game {

	namespace {

		// ── Game function typedefs ──────────────────────────────────────────────────

		using tGetDlgInfo = u32 * (__thiscall*)(u32, u32*);
		using tGetUIEditBoxString = char* (__thiscall*)(u32);
		using tGetUIEditBoxWide = u8 * (__thiscall*)(u32);
		using tGetUIMsgText = u32(__thiscall*)(CUIMsgAdapter*, const char*);
		using tValidatePassword = char(__stdcall*)(char*);
		using tRoomRuleModPassUpdate = char(__cdecl*)(u32, u32, char*);
		using tRoomRuleMaxPlayerUpdate = char(__cdecl*)(u32);
		using tGetCRoom = CRoom * (__cdecl*)();
		using tGetCServerList = u32(__cdecl*)();
		using tJoinRoom = char(__cdecl*)(i16, i16, int, char*, char);
		using tPartyClanOtherJoin = char(__cdecl*)(int, char*);
		using tStateMgrGetState = u32(__thiscall*)(u32, int);
		using tStateMgrSetTransition = void(__thiscall*)(u32, u32);
		using tStateRoomJoinSetServer = u32 * (__thiscall*)(u32, int);
		using tStateRoomJoinJoin = int(__thiscall*)(u32, int, int, char*, char*, int, int);
		using tRTDynamicCast = u32(__cdecl*)(u32, int, u32, u32, int);
		using tCloseInputPopup = void(__thiscall*)(u32);
		using tDialogAction = void(__thiscall*)(u32, char*);
		using tSetPasswordCallback = void(__thiscall*)(char*, char*);
		using tChattingBoxBadword = u32(__thiscall*)(u32, u32);
		using tFormatStringSafe = int(__cdecl*)(char*, const char*, ...);
		using tGetNetMgr = CNetMgr * (__cdecl*)();

		struct AuthRetry2faReq
		{
			SCommandHeader m_tHeader;
			u32 auth_key;
			u32 auth_key2;
			u32 code;
		};


		bool Request2faAuthRetry(u32 code)
		{
			auto GetNetMgr = reinterpret_cast<tGetNetMgr>(
				MG_CONST(addr::anticheat::game_managers::net_mgr::Get));

			AuthRetry2faReq authRetry2fa{};
			authRetry2fa.m_tHeader.order = 25;
			authRetry2fa.auth_key = GetNetMgr()->auth_key;
			authRetry2fa.auth_key2 = GetNetMgr()->auth_key2;
			authRetry2fa.code = code;

			if (GetNetMgr()->CFrontConnectorTcp)
			{
				return (GetNetMgr()->CFrontConnectorTcp->_vptr_CConnector->Send(
					GetNetMgr()->CFrontConnectorTcp,
					reinterpret_cast<char*>(&authRetry2fa), 12, 0, 4) >= 0);
			}
			else
			{
				return false;
			}
		}


		// ── InitPopupTitles ─────────────────────────────────────────────────────────

		void __fastcall hkInitPopupTitles(u32 _DialogInfo, u32 edx, u32 type)
		{
			auto GetDlgInfo = reinterpret_cast<tGetDlgInfo>(
				MG_CONST(addr::ui::GetDlgInfo));

			auto GetUIMsg = reinterpret_cast<tGetUIMsgText>(
				MG_CONST(addr::ui::GetUIMsgText));

			auto* UIMsgAdapter = *reinterpret_cast<CUIMsgAdapter**>(
				MG_CONST(addr::ui::ui_msgadapter::Get));

			mg::writeValue<u32>(_DialogInfo, 0xA28, type);

			u32 dialogId = 105008;
			auto info = *GetDlgInfo(_DialogInfo + 0x6B0, &dialogId);

			const char* msgKey = nullptr;
			switch (type) {
			case 4:  msgKey = MG_STR("UIMSG_GOTO_ROOM");              break;
			case 5:  msgKey = MG_STR("UIMSG_CHARACTER_NAME_ENTER");   break;
			case 6:  msgKey = MG_STR("UIMSG_AUTH_2FA");               break;
			default: msgKey = MG_STR("UIMSG_LOBBY_ENTER_PASSWORD");   break;
			}

			auto text = GetUIMsg(UIMsgAdapter, msgKey);
			(*(void(__thiscall**)(u32, u32))(*(DWORD*)info + 0xD8))(info, text);
		}

		// ── DlgInputHandle ──────────────────────────────────────────────────────────
		// Replaces sub_6556B0 (void __thiscall): handles OK button on the input popup.
		// Structure: if (type != 0) { switch } else { case 0 with FormatStringSafe }
		// sub_656190 called once at end of the if(v6) block for all paths.

		void __fastcall hkDlgInputHandle(u32 thisPtr, u32 edx)
		{
			// ── Resolve game function pointers ──────────────────────────────────

			auto GetDlgInfo = reinterpret_cast<tGetDlgInfo>(
				MG_CONST(addr::ui::GetDlgInfo));
			auto GetEditBoxString = reinterpret_cast<tGetUIEditBoxString>(
				MG_CONST(addr::ui::input_popup::GetUIEditBoxString));
			auto GetEditBoxWide = reinterpret_cast<tGetUIEditBoxWide>(
				MG_CONST(addr::ui::input_popup::GetUIEditBoxWide));
			auto ValidatePassword = reinterpret_cast<tValidatePassword>(
				MG_CONST(addr::ui::input_popup::ValidatePassword));
			auto ClosePopup = reinterpret_cast<tCloseInputPopup>(
				MG_CONST(addr::ui::input_popup::CloseInputPopup));
			auto DialogAction = reinterpret_cast<tDialogAction>(
				MG_CONST(addr::ui::input_popup::DialogAction));
			auto SetPwCallback = reinterpret_cast<tSetPasswordCallback>(
				MG_CONST(addr::ui::input_popup::SetPasswordCallback));
			auto BadwordCheck = reinterpret_cast<tChattingBoxBadword>(
				MG_CONST(addr::ui::input_popup::ChattingBoxBadwordCheck));

			auto GetCRoom = reinterpret_cast<tGetCRoom>(
				MG_CONST(addr::anticheat::game_managers::room::Get));
			auto GetCServerList = reinterpret_cast<tGetCServerList>(
				MG_CONST(addr::ui::room_join::GetCServerList));
			auto JoinRoom = reinterpret_cast<tJoinRoom>(
				MG_CONST(addr::ui::room_join::JoinRoom));
			auto PartyClanOtherJoin = reinterpret_cast<tPartyClanOtherJoin>(
				MG_CONST(addr::ui::room_join::PartyClanOtherJoin));
			auto RoomRuleModPassUpdate = reinterpret_cast<tRoomRuleModPassUpdate>(
				MG_CONST(addr::ui::room_join::RoomRuleModPassUpdate));
			auto RoomRuleMaxPlayerUpdate = reinterpret_cast<tRoomRuleMaxPlayerUpdate>(
				MG_CONST(addr::ui::room_join::RoomRuleMaxPlayerUpdate));
			auto StateMgrGetState = reinterpret_cast<tStateMgrGetState>(
				MG_CONST(addr::ui::state_mgr::GetState));
			auto StateMgrSetTransition = reinterpret_cast<tStateMgrSetTransition>(
				MG_CONST(addr::ui::state_mgr::SetTransition));
			auto StateRoomJoinSetServer = reinterpret_cast<tStateRoomJoinSetServer>(
				MG_CONST(addr::ui::room_join::StateRoomJoinSetServer));
			auto StateRoomJoinJoin = reinterpret_cast<tStateRoomJoinJoin>(
				MG_CONST(addr::ui::room_join::StateRoomJoinJoin));
			auto DynCast = reinterpret_cast<tRTDynamicCast>(
				MG_CONST(addr::ui::room_join::RTDynamicCast));

			auto* UIMsgBox = *reinterpret_cast<CUIMsgBox**>(
				MG_CONST(addr::ui::ui_msgbox::Get));
			auto stateMgrInst = MG_CONST(addr::ui::state_mgr::Instance);

			// ── Read the edit box contents ──────────────────────────────────────

			u32 dlgId = 105005;
			u32* dlgInfoPtr = GetDlgInfo(thisPtr + 0x6B0, &dlgId);
			char* inputString = GetEditBoxString(static_cast<u32>(*dlgInfoPtr));

			dlgId = 105005;
			dlgInfoPtr = GetDlgInfo(thisPtr + 0x6B0, &dlgId);
			u8* wideRaw = GetEditBoxWide(static_cast<u32>(*dlgInfoPtr));
			u32 wideStrData = *reinterpret_cast<u32*>(wideRaw);

			u32 popupType = mg::readValue<u32>(thisPtr, 0xA28);

			// ── Empty string handling ───────────────────────────────────────────

			if (!strlen(inputString))
			{
				if (popupType != 2)
				{
					if (popupType == 4)
						UIMsgBox->_vptr_CUIMsgBox->CreateBox(UIMsgBox, MG_STR("MSG_ROOMNUMBER_INPUT"), 0, 0, 1, false);
					else if (popupType == 6)
						UIMsgBox->_vptr_CUIMsgBox->CreateBox(UIMsgBox, MG_STR("MSG_2FA_INPUT"), 0, 0, 1, false);
					else
						UIMsgBox->_vptr_CUIMsgBox->CreateBox(UIMsgBox, MG_STR("LOBBY_ENTER_WRONGKEYNUM"), 0, 0, 1, false);
					return;
				}

				// type 2 with empty string = remove password
				RoomRuleModPassUpdate(
					mg::readValue<u32>(thisPtr, 0xA20),
					mg::readValue<u32>(thisPtr, 0xA24),
					nullptr);

				auto* room = GetCRoom();
				if (room)
					RoomRuleMaxPlayerUpdate(static_cast<u32>(room->m_iMaxPlayers));

				DialogAction(reinterpret_cast<u32>(UIMsgBox), reinterpret_cast<char*>(thisPtr + 0x44C));
				// Falls through to ValidatePassword (type 2 empty string path)
			}

			// ── Validate and sanitize ───────────────────────────────────────────

			if (!ValidatePassword(inputString))
				return;

			// ── Determine effective length (capped at 15) ───────────────────────

			size_t inputLen = inputString ? strnlen(inputString, 15) : 0;

			if (inputLen)
			{
				if (popupType != 0)
				{
					// ── Types 1-4: switch on popup type ─────────────────────────
					switch (popupType)
					{
						// ── Case 1: Party/clan join with password ───────────────────
					case 1:
					{
						PartyClanOtherJoin(
							mg::readValue<i32>(thisPtr, 0x810),
							inputString);
						break;
					}

					// ── Case 2: Room config password change (with badword check) ─
					case 2:
					{
						// Construct a temporary wstring from the wide edit box data
						// WStringCtor/Dtor are IAT entries (FF 15 pattern) — dereference to get actual function
						u8 wsBuf[32] = {};
						uptr wsCtor = mg::readValue<uptr>(MG_CONST(addr::msvc::WStringCtor), 0);
						mg::call<u32(__thiscall*)(u8*, u32)>(wsCtor, wsBuf, wideStrData);

						u32 chattingBoxInst = mg::readValue<u32>(
							MG_CONST(addr::ui::gui_manager::Get), 0x108);

						char isBadword = static_cast<char>(
							BadwordCheck(chattingBoxInst, reinterpret_cast<u32>(wsBuf)));

						uptr wsDtor = mg::readValue<uptr>(MG_CONST(addr::msvc::WStringDtor), 0);
						mg::call<void(__thiscall*)(u8*)>(wsDtor, wsBuf);

						if (isBadword)
						{
							UIMsgBox->_vptr_CUIMsgBox->CreateBox(
								UIMsgBox, MG_STR("ROOM_CONFIG_SALNGPASSWORD"), 0, 0, 1, false);
							return;
						}

						RoomRuleModPassUpdate(
							mg::readValue<u32>(thisPtr, 0xA20),
							mg::readValue<u32>(thisPtr, 0xA24),
							inputString);

						auto* room = GetCRoom();
						if (room)
							RoomRuleMaxPlayerUpdate(static_cast<u32>(room->m_iMaxPlayers));
						break;
					}

					// ── Case 3: Callback-based password delivery ────────────────
					case 3:
					{
						u32 callbackPtr = mg::readValue<u32>(thisPtr, 0xA2C);
						if (callbackPtr)
						{
							SetPwCallback(reinterpret_cast<char*>(callbackPtr), inputString);

							u32 vtbl = mg::readValue<u32>(callbackPtr, 0);
							mg::call<void(__thiscall*)(u32)>(
								mg::readValue<u32>(vtbl, 0x08), callbackPtr);

							u32 obj = mg::readValue<u32>(thisPtr, 0xA2C);
							if (obj)
							{
								u32 objVtbl = mg::readValue<u32>(obj, 0);
								u32 destructor = mg::readValue<u32>(objVtbl, 0);
								mg::call<void(__thiscall*)(u32, int)>(destructor, obj, 1);
								mg::writeValue<u32>(thisPtr, 0xA2C, 0u);
							}
						}
						break;
					}

					// ── Case 4: Join room by number ─────────────────────────────
					case 4:
					{
						u32 stateRoomJoin = DynCast(
							StateMgrGetState(stateMgrInst, 5), 0,
							MG_CONST(addr::ui::room_join::CState_RTTI),
							MG_CONST(addr::ui::room_join::CStateRoomJoin_RTTI), 0);

						if (stateRoomJoin)
						{
							int roomNum = atoi(inputString);

							// Decode room number: channel = bits 11..15, room = bits 0..10
							u32 channelId = (static_cast<u16>(roomNum) >> 11) & 0x1F;
							u16 packed = static_cast<u16>(((roomNum & 0x7FF) << 5) | channelId);

							u32 serverList = GetCServerList();
							bool serverValid = false;

							if (channelId)
							{
								for (u32 i = 0; i < 15; ++i)
								{
									if (mg::readValue<u32>(serverList, 0x21C * i + 0x2C38) == channelId
										&& !mg::readValue<u8>(serverList, 0x21C * i + 0x2C48))
									{
										serverValid = true;
										break;
									}
								}
							}

							if (!serverValid)
							{
								UIMsgBox->_vptr_CUIMsgBox->CreateBox(
									UIMsgBox, MG_STR("MSG_ROOMNUMBER_INPUT_ERROR"), 0, 0, 1, false);
								ClosePopup(thisPtr);
								return;
							}

							mg::writeValue<u32>(thisPtr, 0x918, channelId);
							mg::writeValue<u32>(thisPtr, 0x80C, static_cast<u32>(roomNum));
							mg::writeValue<u32>(thisPtr, 0x810, 100u);

							StateRoomJoinSetServer(stateRoomJoin, channelId);
							StateRoomJoinJoin(stateRoomJoin, roomNum, 100, nullptr, nullptr, 36, 1);
							StateMgrSetTransition(stateMgrInst, GAME_STATE_ROOM_JOIN);
						}
						break;
					}
					case 6:
					{
						// Validate that input contains only digits 0-9
						bool validCode = true;
						for (const char* p = inputString; *p; ++p)
						{
							if (*p < '0' || *p > '9')
							{
								validCode = false;
								break;
							}
						}

						if (!validCode)
						{
							UIMsgBox->_vptr_CUIMsgBox->CreateBox(
								UIMsgBox, MG_STR("MSG_2FA_INVALID"), 0, 0, 1, false);
							return;
						}

						u32 code = static_cast<u32>(atoi(inputString));
						Request2faAuthRetry(code);

						// Build custom vtable once: CMsgDataLoginAuthorize with index 1 replaced by no-op.
						// Uses inline globals in addr::globals — no CRT guards, safe for mapped DLLs.
						if (!addr::globals::g_2faVtblReady)
						{
							uptr src = MG_CONST(addr::ui::CMsgDataLoginAuthorize_vftbl);
							addr::globals::g_2faVtbl[0] = mg::readValue<uptr>(src - sizeof(uptr), 0);
							for (int i = 0; i < 9; ++i)
								addr::globals::g_2faVtbl[i + 1] = mg::readValue<uptr>(src, i * sizeof(uptr));
							addr::globals::g_2faVtbl[2] = addr::globals::g_2faVtbl[3]; // index 1 ← no-op (sub_B27A50)
							addr::globals::g_2faVtblReady = true;
						}

						// Allocate and init IMsgData (0x630 bytes, matching CMsgDataLoginAuthorize)
						auto GameNew = reinterpret_cast<tGameNew>(MG_CONST(addr::msvc::operator_new));
						auto* msgData = GameNew(0x630);
						if (msgData)
						{
							auto Init = reinterpret_cast<tIMsgDataInit>(MG_CONST(addr::ui::IMsgDataInit));
							Init(msgData, 3000, 0, 15, 0, nullptr, nullptr, nullptr);
							*reinterpret_cast<uptr*>(msgData) = reinterpret_cast<uptr>(&addr::globals::g_2faVtbl[1]);
						}

						// Show processing dialog — keeps the network pumped via vtable
						// callbacks while waiting for the server's 2FA response.
						UIMsgBox->_vptr_CUIMsgBox->CreateBox(
							UIMsgBox, MG_STR("LOGIN_AUTHORIZE_PROCESSING"), msgData, 0, 1, false);
						break;
					}
					}
				}
				else
				{
					// ── Type 0: Join room with password ─────────────────────────
					u32 stateRoomJoin = DynCast(
						StateMgrGetState(stateMgrInst, 5), 0,
						MG_CONST(addr::ui::room_join::CState_RTTI),
						MG_CONST(addr::ui::room_join::CStateRoomJoin_RTTI), 0);

					if (stateRoomJoin)
					{
						// Check if re-joining from a previous CStateRoomJoin
						// TODO: set addr::ui::state_mgr::PrevGameStatePtr to the correct address
						u32 prevGameState = mg::readValue<u32>(
							MG_CONST(addr::ui::state_mgr::PrevGameStatePtr), 0);
						u32 prevRoomJoin = DynCast(
							prevGameState, 0,
							MG_CONST(addr::ui::room_join::CState_RTTI),
							MG_CONST(addr::ui::room_join::CStateRoomJoin_RTTI), 0);

						if (prevRoomJoin && mg::readValue<u8>(prevRoomJoin, 230))
						{
							mg::writeValue<u32>(thisPtr, 0x80C, mg::readValue<u32>(prevRoomJoin, 160));
							mg::writeValue<u32>(thisPtr, 0x810, mg::readValue<u32>(prevRoomJoin, 164));
							JoinRoom(
								static_cast<i16>(mg::readValue<u16>(thisPtr, 0x80C)),
								static_cast<i16>(mg::readValue<u16>(thisPtr, 0x810)),
								44, inputString, 0);
						}
						else
						{
							u32 serverId = mg::readValue<u32>(thisPtr, 0x918);
							u32 serverList = GetCServerList();

							bool serverValid = false;
							if (serverId)
							{
								for (u32 j = 0; j < 15; ++j)
								{
									if (mg::readValue<u32>(serverList, 0x21C * j + 0x2C38) == serverId)
									{
										serverValid = true;
										break;
									}
								}
							}

							if (!serverValid)
							{
								UIMsgBox->_vptr_CUIMsgBox->CreateBox(
									UIMsgBox, MG_STR("MSG_ROOMNUMBER_INPUT_ERROR"), 0, 0, 1, false);
								ClosePopup(thisPtr);
								return;
							}

							StateRoomJoinSetServer(stateRoomJoin, mg::readValue<i32>(thisPtr, 0x918));
							StateRoomJoinJoin(stateRoomJoin,
								mg::readValue<i32>(thisPtr, 0x80C),
								mg::readValue<i32>(thisPtr, 0x810),
								reinterpret_cast<char*>(thisPtr + 0x91C),
								inputString, 44, 1);
							StateMgrSetTransition(stateMgrInst, GAME_STATE_ROOM_JOIN);
						}
					}

					// FormatStringSafe — always runs for type 0 after the stateRoomJoin block
					auto FormatStr = reinterpret_cast<tFormatStringSafe>(
						MG_CONST(addr::ui::FormatStringSafe));
					FormatStr(reinterpret_cast<char*>(thisPtr + 0x814), "%s", inputString);
				}

				// Close popup — called for ALL types when inputLen > 0
				ClosePopup(thisPtr);
			}
			else if (popupType != 2)
			{
				// inputLen == 0 and type != 2: show error and clean up callback
				UIMsgBox->_vptr_CUIMsgBox->CreateBox(
					UIMsgBox, MG_STR("ROOM_JOIN_NOTINPUT_PASSWORD"), 0, 0, 1, false);

				u32 callbackPtr = mg::readValue<u32>(thisPtr, 0xA2C);
				if (callbackPtr)
				{
					u32 objVtbl = mg::readValue<u32>(callbackPtr, 0);
					u32 destructor = mg::readValue<u32>(objVtbl, 0);
					mg::call<void(__thiscall*)(u32, int)>(destructor, callbackPtr, 1);
					mg::writeValue<u32>(thisPtr, 0xA2C, 0u);
				}
			}
			// type 2 with inputLen == 0: no-op (implicit return)
		}

	} // anonymous namespace

// ── DlgInputPopup class ─────────────────────────────────────────────────────

DlgInputPopup::DlgInputPopup(MegaGuardContext& ctx) : ctx_(ctx) {}
DlgInputPopup::~DlgInputPopup() = default;

VoidResult DlgInputPopup::install() {
	auto& registry = ctx_.hookRegistry();

	registry.registerDetour(HookId::DlgInputPopupTitles)
		.create(MG_CONST(addr::ui::input_popup::InitTitles), hkInitPopupTitles);

	registry.registerDetour(HookId::DlgInputHandle)
		.create(MG_CONST(addr::ui::input_popup::DlgInputHandle), hkDlgInputHandle);

	return VoidResult::ok();
}

} // namespace mg::game
