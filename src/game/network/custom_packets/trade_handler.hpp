// =============================================================================
// TradeHandler - Player↔player trading (ported from ToyBattles; stripped in
// MegaVolts). Implements the client side of the SEND_TRADE / RECV_TRADE protocol
// against the native E_DLG_TRADE dialog (added to SCENE_INVEN_2ND.xml).
//
// Wire protocol (opcode = CommandHeader.order, SAME both directions; result in
// CommandHeader.extra). VERIFIED 1:1 against the ToyBattles client binary; the
// recv handler table base is order 190. Body offsets are from m_data start
// (after the 4-byte command header):
//   190 Open      S->C only  : open dialog + both players' TradePlayerInfo(20)
//   191 Init      C->S target@body+8 (invite) ; S->C invite-request popup
//   192 Ack       C->S target@body+8 (accept) ; S->C my-invite answered (+extra)
//   193 Money     money set/notify           — IGNORED (no-money trade design)
//   194 AddItem   C->S serial@body+8         ; S->C partner item -> SLT_TRADE_YOU
//   195 RemoveItem C->S serial@body+8        ; S->C partner removed item
//   196 Lock      C->S empty                 ; S->C partner locked (PIC_TRADE_LOCK_YOU)
//   197 Finalize  C->S empty                 ; S->C partner confirmed (wait)
//   198 Cancel    C->S empty                 ; S->C partner cancelled -> close
//   199 Complete  S->C only  : BOTH confirmed -> items swapped, refresh inventory
// MegaVoltsPP server must use this same numbering (ToyBattlesHQ base-190).
// See memory: trade-feature-task.
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::game {

class CustomPacketDispatcher;

class TradeHandler {
public:
    TradeHandler(::mg::MegaGuardContext& ctx, CustomPacketDispatcher& dispatcher);
    ~TradeHandler();

    VoidResult install();

    // Register the E_DLG_TRADE dialog object at lobby entry — BEFORE the inventory
    // scene (SCENE_INVEN_2ND) is ever loaded — so that scene's loader binds the trade
    // XML controls + owning game-state to our object (the engine only binds dialogs
    // that are already registered by name when the scene loads). Called from the
    // lobby-entry hook alongside the reward-dialog registration.
    void registerDialogOnLobby();

    // ── Outgoing requests (call from UI hooks / invite menu) ──────────────────
    void sendInvite(const char* targetName); // order 191 TradeStartReq (by name; server resolves)
    void sendAccept(u32 targetAccountId);    // order 192 TradeAcceptReq
    void acceptInvite();                     // accept the last incoming invite (191->192)
    void sendAddItem(u64 itemSerialInfo);    // order 194 TradeAddItem
    void sendRemoveItem(u64 itemSerialInfo); // order 195 TradeRemoveItem
    void sendLock();                         // order 196 TradeLock
    void sendFinalize();                     // order 197 TradeFinalize
    void sendCancel();                       // order 198 TradeCancel

private:
    ::mg::MegaGuardContext& ctx_;
    CustomPacketDispatcher& dispatcher_;
};

} // namespace mg::game
