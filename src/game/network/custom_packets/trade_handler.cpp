// =============================================================================
// TradeHandler - Implementation (client side of the trade protocol)
// =============================================================================
#include "pch.hpp"
#include "game/network/custom_packets/trade_handler.hpp"
#include "game/network/custom_packets/custom_packet_dispatcher.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "utils/call_helper.hpp"
#include "utils/logger.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::game {

namespace {

// ── Opcodes (CommandHeader.order) — order = headerWord>>6, SAME both directions.
// VERIFIED 1:1 against the ToyBattles client binary (SEND_TRADE.cpp senders set
// `hdr & 0x3F | (order<<6)`; recv handler table base = order 190):
//   191 TradeStartReq   0x2FC0   192 TradeAcceptReq 0x3000   193 money(skip) 0x3040
//   194 TradePushReq    0x3080   195 TradeError2Req 0x30C0   196 TradeBlockReq 0x3100
//   197 TradeConfirmReq 0x3140   198 sub_B3D0D0      0x3180
// Server→client recv table (unk_11C1C60[order-190]): 190 open, 191 request,
//   192 ack, 193 money, 194 add, 195 remove, 196 lock, 197 finalize, 198 cancel,
//   199 complete.
// NOTE: MegaVoltsPP server MUST use this same numbering (ToyBattlesHQ base-190).
enum TradeOrder : u32 {
    TRADE_OPEN     = 190, // S->C only: open dialog + both players' info
    TRADE_INIT     = 191, // C->S TradeStartReq (invite) ; S->C invite request popup
    TRADE_ACK      = 192, // C->S TradeAcceptReq (accept); S->C my-invite answered
    TRADE_MONEY    = 193, // money set/notify — IGNORED (no-money trade per design)
    TRADE_ADD      = 194, // TradeAddItem  (serial @ body+8)
    TRADE_REMOVE   = 195, // TradeRemoveItem (serial @ body+8)
    TRADE_LOCK     = 196, // TradeLock (empty)
    TRADE_FINALIZE = 197, // TradeFinalize/confirm (empty)
    TRADE_CANCEL   = 198, // TradeCancel (empty)
    TRADE_COMPLETE = 199, // S->C only: both confirmed -> items swapped
};

// ── Result codes (CommandHeader.extra = TradeSystemExtra) ─────────────────────
enum TradeExtra : u8 {
    TX_SUCCESS                 = 1,  // also CANCELLED / CONFIRMED_NOTIFY (disambiguated by order)
    TX_TARGET_NO_SPACE         = 7,
    TX_NOT_ENOUGH_MONEY        = 14,
    TX_MAINTENANCE             = 15,
    TX_COOLDOWN                = 21,
    TX_DECLINED                = 31,
    TX_ITEMS_LOCKED            = 44,
    TX_BOTH_MUST_CONFIRM       = 45,
    TX_CANNOT_TRADE_OR_OFFLINE = 47,
    TX_NOT_FRIENDS             = 73,
    TX_LEVEL_TOO_LOW           = 74,
    TX_MAX_ITEMS               = 75,
};

// ── Packet structures (PACK 1) ────────────────────────────────────────────────
// Body offsets are measured from the start of m_data (right after the 4-byte
// SCommandHeader). The server's parseData(request, N) reads m_data[N].
#pragma pack(push, 1)

// 8-byte item identity: itemNumber:20, serverId:4, unknown:4, origin:4, date:32.
// In the ToyBattles inventory item object these are item+44 (lo) / item+48 (hi).
struct ItemSerialInfo { u64 raw; };

// ── Outgoing ── (offsets verified: TradeStartReq writes body[0]@+4=mine,
// body[1]@+8=target; TradePushReq writes serial dwords @ body+8/+12).
struct TradeInviteReq {              // Init(191): invite a player by name
    SCommandHeader hdr;
    char targetName[16];             // body+0 (server resolves name -> account id)
};
struct TradeTargetReq {              // Ack(192): accept a known inviter by account id
    SCommandHeader hdr;
    u32 unused0;                     // body+0 (= my account id; ignored by server)
    u32 targetAccountId;             // body+4 (the inviter we are accepting)
};
struct TradeItemReq {                // Add(194) / Remove(195): serial @ body+8
    SCommandHeader hdr;
    u32 unused0;                     // body+0
    u32 unused1;                     // body+4
    ItemSerialInfo serial;           // body+8
};
struct TradeEmptyReq {               // Lock(196) / Finalize(197) / Cancel(198)
    SCommandHeader hdr;
};

// ── Incoming (body starts at (u8*)a1 + 4) ──
struct TradePlayerInfo {             // 190/191/192 response, 36 bytes
    u32  unused;
    u32  accountId;
    u32  characterId;
    u32  equippedHair;
    u32  equippedEyes;
    char nickname[16];               // body+20 — the partner's name (server fills it)
};
struct TradeAddedItem {              // Add(194)/Remove(193) response, 32 bytes
    u32            unused;
    u32            originalItemOwnerAccountId; // whose side -> ME vs YOU slot
    ItemSerialInfo serial;
    u32            itemId;
    u32            unused2;
    u32            unknown1;
    u32            unknown2;
};
#pragma pack(pop)

// ── Session state ─────────────────────────────────────────────────────────────
inline u32  g_partnerAccountId = 0;   // who we're trading with (set on invite/accept/open)
inline u32  g_inviterAccountId = 0;   // last incoming invite's sender (acceptInvite uses it)
inline bool g_tradeOpen        = false;
inline char g_partnerName[17]  = {};  // partner nickname (inviter knows it from the invite)
inline u32  g_partnerChar      = 0;   // partner characterId / equipped hair / eyes (for PIC_FACE)
inline u32  g_partnerHair      = 0;
inline u32  g_partnerEyes      = 0;
// ME vs YOU is decided purely by comparing item owner to g_partnerAccountId, so no
// local-account lookup is needed (ToyBattles used *(sub_476F20()+4188)).

// ── Open-via-state-transition bookkeeping ─────────────────────────────────────
// ToyBattles opens trade by entering the INVENTORY game-state (TB 15 == MV 20),
// which loads SCENE_INVEN_2ND so E_DLG_TRADE's XML controls actually bind; it is NOT
// a popup over the lobby. The scene may take a frame or two to finish loading, so we
// flag a pending show and complete it from the per-frame net tick once we're in
// inventory and settled.
inline int  g_preTradeState    = -1;    // game-state to restore on close (-1 = stay)
inline bool g_pendingTradeShow = false; // deferred show waiting for inventory scene
inline int  g_tradeShowFrames  = 0;     // frames elapsed in inventory while pending
constexpr int kInvenSettleFrames = 3;   // let SCENE_INVEN_2ND parse/bind before showing
constexpr int kInvenGiveUpFrames = 300; // ~5s @60fps: bail if the dialog never registers

// ── Engine helpers ────────────────────────────────────────────────────────────
using tGetNetMgr      = CNetMgr* (__cdecl*)();
// sub_E4E660 is __THISCALL, not __stdcall: it takes the GUI-manager object in ECX
// and the dialog name on the stack, then uses (this + 0x80) as the dialog-name map
// (verified in the disassembly: `mov [var],ecx` … `mov ecx,[var]; add ecx,80h; call
// find`). Calling it without `this` leaves ECX garbage, so `this+0x80` is a wild map
// pointer and the red-black-tree walk faults (0xC0000005 in sub_E5BFB0). This was
// THE invite crash. The working gacha handler uses this same __thiscall form.
using tGetDialogInfo  = int* (__thiscall*)(void* guiMgr, const char*); // sub_E4E660
using tCloseDialog    = char (__thiscall*)(CUIMsgBox*, const char*);   // sub_E62950

inline CNetMgr* netMgr() {
    return reinterpret_cast<tGetNetMgr>(
        MG_CONST(addr::anticheat::game_managers::net_mgr::Get))();
}
inline CUIMsgBox* uiMsgBox() {
    return *reinterpret_cast<CUIMsgBox**>(MG_CONST(addr::ui::ui_msgbox::Get));
}
// The GUI manager is a singleton whose object lives AT this static address (it is
// passed by address, not dereferenced — same as gacha_pity_handler).
inline void* guiManager() {
    return reinterpret_cast<void*>(MG_CONST(addr::ui::gui_manager::Get));
}

inline int* tradeDialogObject() {     // live E_DLG_TRADE scene object, or null
    return reinterpret_cast<tGetDialogInfo>(
        MG_CONST(addr::ui::gui_manager::GetDialogInfo))(guiManager(), MG_STR("E_DLG_TRADE"));
}
inline int* listInfoDialog() {        // live E_DLG_LISTINFO (player-list popup), or null
    return reinterpret_cast<tGetDialogInfo>(
        MG_CONST(addr::ui::gui_manager::GetDialogInfo))(guiManager(), MG_STR("E_DLG_LISTINFO"));
}
// ── Game-state machine (StateMgr) ─────────────────────────────────────────────
// SetTransition is __thiscall(StateMgr* this, GameState); this+8 holds the current
// state (== the g_CurrGameState global). It no-ops (logs "over-called") if asked to
// re-enter the state it is already in, so we read the current state first.
using tSetTransition = void(__thiscall*)(uptr, GameState);
inline GameState curGameState() {
    return static_cast<GameState>(
        *reinterpret_cast<u32*>(MG_CONST(addr::ui::state_mgr::CurrGameState)));
}
inline void enterGameState(GameState s) {
    reinterpret_cast<tSetTransition>(MG_CONST(addr::ui::state_mgr::SetTransition))(
        MG_CONST(addr::ui::state_mgr::Instance), s);
}

// The single TradeHandler instance, so the userlist detour + the dialog-event
// handler (both free functions) can reach the packet senders.
inline TradeHandler* g_self = nullptr;

// ── SlotList helpers (drive SLT_TRADE_ME / SLT_TRADE_YOU) ─────────────────────
// A dialog's controls live at dlg+0x6B0; a slot control's slot std::vector is at
// control+0x9A8; each slot has item@+0x20, flag@+0x9, id@+0x4 (ToyBattles layout).
// Reuses the verified rightclick slot functions (GetSlotItem/Count/At).
namespace tr = addr::ui::trade;
using tFindControl = int*  (__thiscall*)(void* container, int* id);
using tSlotCount   = u32   (__thiscall*)(void* vec);
using tSlotAt      = void**(__thiscall*)(void* vec, u32 i);
using tGetSlotItem = int   (__cdecl*)();
using tItemFactory = void* (__stdcall*)(int itemId);

inline void* tradeControl(int* dlg, int id) {
    if (!dlg) return nullptr;
    int cid = id;
    void* container = reinterpret_cast<u8*>(dlg) + tr::CtrlContainerOff;
    int* res = reinterpret_cast<tFindControl>(MG_CONST(addr::ui::GetDlgInfo))(container, &cid);
    return res ? *reinterpret_cast<void**>(res) : nullptr;
}
// Set a control's text via vtbl+0xD8 (SetText, idx 54) — verified, used by
// weekly_reward + input_pop. Safe: a virtual method call, no raw offset write.
inline void setTradeText(int* dlg, int ctrlId, const char* text) {
    void* c = tradeControl(dlg, ctrlId);
    if (!c || !text) return;
    uptr vtbl = *reinterpret_cast<uptr*>(c);
    reinterpret_cast<void(__thiscall*)(void*, const char*)>(
        *reinterpret_cast<uptr*>(vtbl + 0xD8))(c, text);
}
// Adopt the partner nickname the server put in the trade player record (so the
// invitee, which never had the name, can also fill STC_NAME_YOU).
inline void adoptPartnerName(const char* nick) {
    if (!nick || !nick[0]) return;
    int i = 0;
    for (; i < 16 && nick[i]; ++i) g_partnerName[i] = nick[i];
    g_partnerName[i < 16 ? i : 15] = 0;
}
// Fill the peer's character preview (PIC_FACE_YOU) — 1:1 with ToyBattles sub_6B93B0
// effect (2). The avatar config lives off CUnitContainer::GetInstance (MV 0x4728D0,
// the SAME singleton TB sub_475CA0 returns): result=*(uc+0x80); P1=*(result+0x10C);
// P2=*(P1+4); then charId bits @P2+4, hair @P2+0xC70, eyes @P2+0xC74. Every deref is
// guarded so a layout mismatch bails instead of faulting.
inline void fillPeerFace(u32 charId, u32 hair, u32 eyes) {
    uptr uc = mg::call<uptr(__cdecl*)()>(MG_CONST(0x004728D0));   // CUnitContainer::GetInstance
    if (!uc) return;
    uptr result = *reinterpret_cast<uptr*>(uc + 0x80);
    if (!result) return;
    uptr p1 = *reinterpret_cast<uptr*>(result + 0x10C);
    if (!p1) return;
    uptr p2 = *reinterpret_cast<uptr*>(p1 + 4);
    if (!p2) return;
    u32& cfg = *reinterpret_cast<u32*>(p2 + 4);
    cfg = (cfg & 0xFFFFF07F) | ((charId & 0x1F) << 7);
    *reinterpret_cast<u32*>(p2 + 0xC70) = hair;
    *reinterpret_cast<u32*>(p2 + 0xC74) = eyes;
}
inline void* slotVec(void* ctl)   { return reinterpret_cast<u8*>(ctl) + tr::SlotVectorOff; }
inline u32   slotCnt(void* vec)   { return reinterpret_cast<tSlotCount>(MG_CONST(addr::ui::rightclick::GetSlotCount))(vec); }
inline void* slotAt(void* vec, u32 i) {
    auto pp = reinterpret_cast<tSlotAt>(MG_CONST(addr::ui::rightclick::GetSlotAt))(vec, i);
    return pp ? *pp : nullptr;
}
inline u32&  slotItem(void* s) { return *reinterpret_cast<u32*>(reinterpret_cast<u8*>(s) + tr::SlotItemOff); }
inline u8&   slotFlag(void* s) { return *reinterpret_cast<u8*>(reinterpret_cast<u8*>(s) + tr::SlotFlagOff); }

inline void slotInsert(void* ctl, void* item) {
    if (!ctl || !item) return;
    void* vec = slotVec(ctl); u32 n = slotCnt(vec);
    for (u32 i = 0; i < n; ++i) {
        void* s = slotAt(vec, i);
        if (s && slotItem(s) == 0) {
            slotItem(s) = reinterpret_cast<u32>(item);
            slotFlag(s) = 1;
            *reinterpret_cast<u32*>(reinterpret_cast<u8*>(s) + tr::SlotIdOff) = 0;
            return;
        }
    }
}
inline void slotClearAll(void* ctl) {
    if (!ctl) return;
    void* vec = slotVec(ctl); u32 n = slotCnt(vec);
    for (u32 i = 0; i < n; ++i) { void* s = slotAt(vec, i); if (s) { slotItem(s) = 0; slotFlag(s) = 0; } }
}
inline u64 itemSerial(void* item) {
    return  *reinterpret_cast<u32*>(reinterpret_cast<u8*>(item) + tr::ItemSerialLo)
         | (static_cast<u64>(*reinterpret_cast<u32*>(reinterpret_cast<u8*>(item) + tr::ItemSerialHi)) << 32);
}
inline void slotRemoveBySerial(void* ctl, u64 serial) {
    if (!ctl) return;
    void* vec = slotVec(ctl); u32 n = slotCnt(vec);
    for (u32 i = 0; i < n; ++i) {
        void* s = slotAt(vec, i);
        if (s && slotItem(s) && itemSerial(reinterpret_cast<void*>(slotItem(s))) == serial) {
            slotItem(s) = 0; slotFlag(s) = 0; return;
        }
    }
}
inline void* activeDragItem() {   // GetSlotItem(): item of the active/dragged slot
    return reinterpret_cast<void*>(reinterpret_cast<tGetSlotItem>(MG_CONST(addr::ui::rightclick::GetSlotItem))());
}

// ── E_DLG_TRADE button events ─────────────────────────────────────────────────
// E_DLG_TRADE has no native CDlgTrade controller in MegaVolts, so its dialog
// object's vtbl[3] (OnEvent) is a base no-op. We point the live instance's vtable
// at a private copy whose [3] routes the trade buttons, chaining everything else
// to the original. ToyBattles button map (CDlgTrade OnEvent 0x6B7B00):
//   110001 BTN_CANCLE -> Cancel(198) ; 110002 BTN_COMFIRM -> Finalize(197)
//   110003 BTN_LOCK   -> Lock(196). (Confirm pop-ups are skipped by design.)
using tDlgOnEvent = void(__thiscall*)(void* dlg, int evt, int ctrlId, int a4);
inline tDlgOnEvent g_origTradeDlgOnEvent = nullptr;
// Sized generously: a UI dialog's real vtable can exceed 64 entries, and the
// engine may call a high-index method during show/render. Copying MORE slots than
// the dialog actually has is safe (the surplus is never dispatched); copying too
// FEW would send a real high-index call into this array's tail and crash.
inline void*       g_tradeDlgVtbl[128]   = {};   // private vtable copy
inline bool        g_tradeDlgVtblReady   = false;

void __fastcall tradeDlgOnEvent(void* dlg, void* /*edx*/, int evt, int ctrlId, int a4) {
    if (g_self) {
        if (evt == 0x101 /*257 click*/) {
            switch (ctrlId) {
                case tr::IdBtnCancel:  g_self->sendCancel();   return;
                case tr::IdBtnConfirm: g_self->sendFinalize(); return;
                case tr::IdBtnLock:    g_self->sendLock();     return;
            }
        }
        // 2051 = item dropped from inventory onto the trade dialog -> add to ME.
        else if (evt == 2051) {
            if (void* item = activeDragItem()) {
                g_self->sendAddItem(itemSerial(item));
                slotInsert(tradeControl(tradeDialogObject(), tr::IdSlotMe), item);
            }
            return;
        }
        // 2050/2055 on SLT_TRADE_ME = pull my own item back out of the trade.
        else if ((evt == 2050 || evt == 2055) && ctrlId == tr::IdSlotMe) {
            if (void* item = activeDragItem()) {
                g_self->sendRemoveItem(itemSerial(item));
                void* me = tradeControl(tradeDialogObject(), tr::IdSlotMe);
                slotRemoveBySerial(me, itemSerial(item));
            }
            return;
        }
    }
    if (g_origTradeDlgOnEvent) g_origTradeDlgOnEvent(dlg, evt, ctrlId, a4);
}

// ── E_DLG_TRADE registration — 1:1 with ToyBattles sub_593D00 ────────────────
// ToyBattles registers E_DLG_TRADE at global UI init like every other dialog:
//   factory = CDlgFactory::Get()              (tb sub_520260 = CFactoryGet)
//   factory->RegisterById(0x1ADB0, sub_5E3F00)(tb sub_E0C5F0 = GetDlgById)
//   obj = factory->Create(0x1ADB0)            (tb sub_E0C690 = GetDlgId)
//   GUIManager->RegisterDialog("E_DLG_TRADE", obj) (tb sub_E62210 = RegisterDialog)
// (0x1ADB0 = 110000 = the E_DLG_TRADE dialog id from SCENE_INVEN_2ND.xml.)
// MegaVolts STRIPPED this one entry from its registration table, so the object is
// never created and GetDialogInfo("E_DLG_TRADE") returns 0 even though the XML
// template is parsed. We add back exactly that entry. ToyBattles' factory
// sub_5E3F00 = `new(0x864)` + CDlgTrade ctor (sub_6B7950); MegaVolts has no
// CDlgTrade, so — like weekly_reward — we use the generic base-dialog ctor and the
// engine binds E_DLG_TRADE's XML controls; our vtbl[3] handles the buttons.
inline int __cdecl tradeDialogFactory() {
    auto mem = reinterpret_cast<u8*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x864));
    if (!mem) return 0;
    reinterpret_cast<void*(__thiscall*)(void*)>(
        MG_CONST(addr::ui::gui_manager::DialogBaseCtor))(mem);
    return reinterpret_cast<int>(mem);
}

inline bool g_tradeDialogRegistered = false;
inline void registerTradeDialog() {
    if (g_tradeDialogRegistered) return;

    auto f1 = reinterpret_cast<u32(*)()>(MG_CONST(addr::features::dlg::CFactoryGet))();
    reinterpret_cast<void(__thiscall*)(u32, u32, void*)>(
        MG_CONST(addr::features::dlg::GetDlgById))(f1, 0x1ADB0, reinterpret_cast<void*>(&tradeDialogFactory));

    auto f2  = reinterpret_cast<u32(*)()>(MG_CONST(addr::features::dlg::CFactoryGet))();
    u32  dlg = reinterpret_cast<u32(__thiscall*)(u32, u32)>(
        MG_CONST(addr::features::dlg::GetDlgId))(f2, 0x1ADB0);
    if (!dlg) {
        mg::ctx().logger().info("Trade: factory failed to create E_DLG_TRADE (0x1ADB0)");
        return;
    }

    // Attach our button handler (CDlgTrade had this natively): copy the base vtable,
    // route [3] (OnEvent) to tradeDlgOnEvent, point the instance at our copy.
    uptr baseVtbl = *reinterpret_cast<uptr*>(dlg);
    for (int i = 0; i < 128; ++i)
        g_tradeDlgVtbl[i] = *reinterpret_cast<void**>(baseVtbl + i * sizeof(uptr));
    g_origTradeDlgOnEvent = reinterpret_cast<tDlgOnEvent>(g_tradeDlgVtbl[3]);
    g_tradeDlgVtbl[3]     = reinterpret_cast<void*>(&tradeDlgOnEvent);
    g_tradeDlgVtblReady   = true;
    *reinterpret_cast<uptr*>(dlg) = reinterpret_cast<uptr>(g_tradeDlgVtbl);

    reinterpret_cast<char(__thiscall*)(void*, const char*, u32)>(
        MG_CONST(addr::ui::gui_manager::RegisterDialog))(
            reinterpret_cast<void*>(MG_CONST(addr::ui::gui_manager::Get)), MG_STR("E_DLG_TRADE"), dlg);

    g_tradeDialogRegistered = true;
    mg::ctx().logger().info("Trade: registered E_DLG_TRADE at 0x{:X}", dlg);
}

// Open the trade window — 1:1 with ToyBattles sub_B027C0 case 1. Trade is hosted by
// the INVENTORY game-state (TB 15 == MV 20): entering it loads SCENE_INVEN_2ND, which
// is where E_DLG_TRADE's XML lives, so the dialog's slot/face controls only exist once
// that scene is loaded. The old popup-over-lobby path showed an empty panel because
// the scene was never loaded. We enter the state here and finish the show (and later
// the face fill) from the per-frame tick once the scene has settled.
inline void showTradeDialog() {
    // NB: do NOT register the dialog here — registration/creation must happen while
    // the inventory scene is active (completeTradeShow), so the object binds against
    // SCENE_INVEN_2ND's state/components rather than the lobby's.
    const GameState cur = curGameState();
    if (cur != GAME_STATE_INVENTORY) {
        g_preTradeState = static_cast<int>(cur);    // remember where to return on close
        enterGameState(GAME_STATE_INVENTORY);       // SetTransition(20) -> load inventory scene
        mg::ctx().logger().info("Trade: entering inventory state from {}", static_cast<int>(cur));
    } else {
        g_preTradeState = -1;                       // already in inventory; stay on close
    }
    g_pendingTradeShow = true;                      // completeTradeShow() finishes it
    g_tradeShowFrames  = 0;
}

// Runs from the per-frame net tick while a show is pending. Waits until we are in the
// inventory state and the scene has had a few frames to bind controls, then opens
// E_DLG_TRADE (positioned by its XML inside the inventory layout).
inline void completeTradeShow() {
    if (!g_pendingTradeShow) return;
    if (curGameState() != GAME_STATE_INVENTORY) return;     // still transitioning/loading
    if (++g_tradeShowFrames < kInvenSettleFrames) return;   // let the scene settle

    // The dialog object was registered at lobby entry (registerDialogOnLobby) so the
    // inventory scene could bind its controls + state; here it must already exist and
    // be scene-bound.
    int* trade = tradeDialogObject();
    if (!trade) {
        if (g_tradeShowFrames > kInvenGiveUpFrames) {
            g_pendingTradeShow = false;
            mg::ctx().logger().info("Trade: E_DLG_TRADE not registered (lobby reg missed?) — give up");
        }
        return;                                             // keep waiting for the scene
    }
    // Show via CUIMsgBox vtable+16 (index 4) — the CUSTOM-dialog open that attaches
    // the dialog to the CURRENT game-state (so its state ptr / component holder are
    // valid). This is exactly how weekly_reward shows its manually-registered dialogs.
    // The vtable+8 (index 2) variant is the NATIVE open and assumes the dialog is
    // already state-bound; using it on our custom dialog left state ptr null and
    // crashed in UI_Dialog::Render (sub_E78040 "state is NULL"). We must be in the
    // inventory state here so it binds against SCENE_INVEN_2ND (which owns trade.dds).
    if (auto* mb = uiMsgBox()) {
        bool visible = reinterpret_cast<bool(__thiscall*)(CUIMsgBox*, const char*)>(
            MG_CONST(addr::ui::FindDialog))(mb, MG_STR("E_DLG_TRADE"));
        if (!visible)
            mg::callVFunc<char>(mb, 4, MG_STR("E_DLG_TRADE"), 0);
    }
    // Fill the partner's name into STC_NAME_YOU (110015). The inviter knows the name
    // from the invite; the invitee only receives an accountId (the packet carries no
    // name), so that side stays blank until the server is extended to send names.
    // SetText is a safe vtable call. PIC_FACE + my-own-name are intentionally NOT
    // written here — those need control offsets verified against the live panel.
    if (g_partnerName[0])
        setTradeText(trade, tr::IdNameYou, g_partnerName);
    // Peer character preview (PIC_FACE_YOU) — 1:1 with TB sub_6B93B0 effect (2).
    if (g_partnerChar)
        fillPeerFace(g_partnerChar, g_partnerHair, g_partnerEyes);

    g_pendingTradeShow = false;
    mg::ctx().logger().info("Trade: panel shown in inventory (partner={} name={})",
                            g_partnerAccountId, g_partnerName[0] ? g_partnerName : "?");
}

inline void hideTradeDialog() {
    g_pendingTradeShow = false;
    if (auto* mb = uiMsgBox())
        reinterpret_cast<tCloseDialog>(
            MG_CONST(addr::ui::ui_msgbox::CloseDialog))(mb, MG_STR("E_DLG_TRADE"));
    // Return to the screen we came from (skip if we were already in inventory).
    if (g_preTradeState >= 0 && static_cast<GameState>(g_preTradeState) != GAME_STATE_INVENTORY)
        enterGameState(static_cast<GameState>(g_preTradeState));
    g_preTradeState = -1;
}

// Per-frame net/scene tick detour — drives the deferred trade-window show. Hooking
// the engine's own frame update is the reliable place to wait out the inventory
// scene load; the cost when idle is a single bool test.
using tNetFrameTick = int(__thiscall*)(void*);
int __fastcall hkNetFrameTick(void* _this, void* /*edx*/) {
    int ret = mg::ctx().hookRegistry()
        .findDetour(HookId::TradeFrameTick)->getOriginal<tNetFrameTick>()(_this);
    if (g_pendingTradeShow) completeTradeShow();
    return ret;
}

// Low-level send: prepend nothing — CConnector::Send builds the TcpHeader; we
// pass [SCommandHeader][body]. size = sizeof(header)+sizeof(body).
inline void sendRaw(void* req, int size) {
    auto nm = netMgr();
    if (nm && nm->CMainConnectorTcp) {
        nm->CMainConnectorTcp->_vptr_CConnector->Send(
            nm->CMainConnectorTcp, reinterpret_cast<char*>(req), size, 0, 0);
    }
}

// ── Incoming handlers ─────────────────────────────────────────────────────────
// a1 points at the CommandHeader: order=(*a1>>6)&0x3FF, extra=*((u8*)a1+2),
// option=*((u8*)a1+3), body=(u8*)a1+4. Returning 1 marks the packet consumed.

inline u8 pktExtra(u16* a1) { return *(reinterpret_cast<u8*>(a1) + 2); }
template <typename T> inline T* pktBody(u16* a1) {
    return reinterpret_cast<T*>(reinterpret_cast<u8*>(a1) + 4);
}

char __cdecl onTradeOpen(u16* a1) {           // 190: open dialog + cache players
    // ToyBattles sub_B02590 caches the two participant ids into the dialog
    // (+0x840/+0x844); the engine instantiates E_DLG_TRADE and its open method
    // (sub_6B8140) clears lock overlays, disables COMFIRM, enables LOCK and fills
    // the faces. We open the native dialog here and remember the partner.
    auto* info = pktBody<TradePlayerInfo>(a1);
    // partner id is already known from the invite/accept handshake; only adopt the
    // packet's id as a fallback if we somehow don't have one yet.
    if (!g_partnerAccountId && info->accountId) g_partnerAccountId = info->accountId;
    adoptPartnerName(info->nickname);
    g_partnerChar = info->characterId; g_partnerHair = info->equippedHair; g_partnerEyes = info->equippedEyes;
    if (!g_tradeOpen) { g_tradeOpen = true; showTradeDialog(); }
    mg::ctx().logger().info("Trade: open partner={} name={}", g_partnerAccountId, info->nickname);
    // TODO(ui): fill PIC_FACE_ME/YOU (control+2504=characterId,+2488=1) and
    //   STC_NAME_ME/YOU (control vtbl+216) from the two TradePlayerInfo records.
    return 1;
}

char __cdecl onTradeRequest(u16* a1) {        // 191: someone invited me to trade
    auto* info = pktBody<TradePlayerInfo>(a1);   // body+4 = inviter accountId
    g_inviterAccountId = info->accountId;
    mg::ctx().logger().info("Trade: invite from acct={} (auto-accept)", g_inviterAccountId);
    // ToyBattles shows MSG_TRADE_REQUEST (CMsgTradingACK) -> OK = TradeAcceptReq.
    // Auto-accept for now so the full flow is testable end-to-end; replace with a
    // native confirm popup (vtbl-swap its YES button to acceptInvite) once UX is set.
    if (g_self) g_self->acceptInvite();
    return 1;
}

char __cdecl onTradeAck(u16* a1) {            // 192: my invite was answered
    const u8 extra = pktExtra(a1);
    auto*    info  = pktBody<TradePlayerInfo>(a1);
    auto&    log   = mg::ctx().logger();
    if (extra != TX_SUCCESS) {                // 31 declined / 73 not-friend / 74 lvl / 21 cooldown
        log.info("Trade: invite refused extra={}", extra);
        g_tradeOpen = false; g_partnerAccountId = 0;
        return 1;
    }
    g_partnerAccountId = info->accountId;     // ToyBattles sub_6B93B0 sets up peer face
    adoptPartnerName(info->nickname);
    g_partnerChar = info->characterId; g_partnerHair = info->equippedHair; g_partnerEyes = info->equippedEyes;
    if (!g_tradeOpen) { g_tradeOpen = true; showTradeDialog(); }
    log.info("Trade: invite accepted, partner={} char={} name={}", info->accountId, info->characterId, info->nickname);
    // TODO(ui): set up partner PIC_FACE_YOU / STC_NAME_YOU from this record.
    return 1;
}

char __cdecl onTradeItemChange(u16* a1) {     // 194 add / 195 remove
    const u32 order = (*a1 >> 6) & 0x3FF;
    const u8  extra = pktExtra(a1);
    auto*     item  = pktBody<TradeAddedItem>(a1);
    auto&     log   = mg::ctx().logger();

    if (extra != TX_SUCCESS) {                // 7 no-space / 14 money / 75 max-items
        log.info("Trade: item change (order {}) failed extra={}", order, extra);
        return 1;
    }

    // ToyBattles only reflects the PARTNER's items via recv (my own items are
    // shown locally at drag time into SLT_TRADE_ME). owner == partner => YOU slot.
    const bool partners = (item->originalItemOwnerAccountId == g_partnerAccountId);
    log.info("Trade: {} item id={} owner={} -> {} slot",
             order == TRADE_ADD ? "ADD" : "REMOVE",
             item->itemId, item->originalItemOwnerAccountId, partners ? "YOU" : "ME(local)");

    if (!partners) return 1;                       // my own side already updated locally
    void* you = tradeControl(tradeDialogObject(), tr::IdSlotYou);
    if (order == TRADE_REMOVE) {
        slotRemoveBySerial(you, item->serial.raw);
        return 1;
    }
    // ADD: render the partner's offered item. Needs CItemFactory::Create(itemId)
    // to build a display item; until that address is located (addr::ui::trade::
    // ItemFactory == 0) the YOU side stays blank but the trade still completes.
    if (tr::ItemFactory && you) {
        if (void* it = reinterpret_cast<tItemFactory>(MG_CONST(tr::ItemFactory))(item->itemId)) {
            *reinterpret_cast<u32*>(reinterpret_cast<u8*>(it) + tr::ItemSerialLo) =
                static_cast<u32>(item->serial.raw);
            *reinterpret_cast<u32*>(reinterpret_cast<u8*>(it) + tr::ItemSerialHi) =
                static_cast<u32>(item->serial.raw >> 32);
            slotInsert(you, it);
        }
    }
    return 1;
}

char __cdecl onTradeLock(u16* a1) {           // 196: partner locked their items
    mg::ctx().logger().info("Trade: partner locked");
    // ToyBattles sub_6B9360: PIC_TRADE_LOCK_YOU(110009) vtbl+96(1) = Show.
    if (void* lock = tradeControl(tradeDialogObject(), 110009 /*PIC_TRADE_LOCK_YOU*/))
        mg::callVFunc<void>(lock, 24, 1);     // control vtbl[24] (+0x60) = Show(visible)
    return 1;
}

char __cdecl onTradeFinalize(u16* a1) {       // 197: partner confirmed (waiting on us)
    mg::ctx().logger().info("Trade: partner confirmed (waiting on us)");
    // TODO(ui): MSG_TRADE_LOCK_WAIT / reflect partner-confirmed on COMFIRM.
    return 1;
}

inline void clearTradeSlots() {
    int* dlg = tradeDialogObject();
    slotClearAll(tradeControl(dlg, tr::IdSlotMe));
    slotClearAll(tradeControl(dlg, tr::IdSlotYou));
}

char __cdecl onTradeCancel(u16* a1) {         // 198: partner cancelled / left
    mg::ctx().logger().info("Trade: cancelled by partner");
    g_tradeOpen = false; g_partnerAccountId = 0;
    clearTradeSlots();
    hideTradeDialog();                        // ToyBattles MSG_TRADE_CANCEL + close
    return 1;
}

char __cdecl onTradeComplete(u16* a1) {       // 199: trade done, items swapped
    mg::ctx().logger().info("Trade: COMPLETE — items swapped");
    g_tradeOpen = false; g_partnerAccountId = 0;
    clearTradeSlots();                        // ToyBattles sub_6B98F0: clear both slots
    hideTradeDialog();
    // Inventory is refreshed by the server (it respawns the received items).
    return 1;
}

// ── Invite trigger ────────────────────────────────────────────────────────────
// Detour on the userlist right-click popup OnEvent (sub_6769F0). MegaVolts has no
// Trade menu item, so we add E_DLG_LISTINFO_BTN_TRADE (id 109017) in the XML and
// handle its click here: the selected user's account id lives at popup+0x854
// (the same field sub_AF5D40/sub_B25740 send for blacklist/invite). Then chain to
// the original (its default case closes the popup).
void __fastcall hkUserListPopup(u32 _this, u32 /*edx*/, int evt, int ctrlId, int a4) {
    if (evt == 0x101 /*257*/ && ctrlId == addr::ui::userlist::TradeMenuId && g_self) {
        const char* name = reinterpret_cast<const char*>(_this + addr::ui::userlist::NameField);
        // The Trade button is grayed (disabled) whenever Invite is, so a disabled
        // click never reaches here; the server also validates the target (offline ->
        // extra 47). So just send by name.
        if (name[0]) {
            mg::ctx().logger().info("Trade: invite -> {}", name);
            g_self->sendInvite(name);
        }
    }
    auto& registry = mg::ctx().hookRegistry();
    registry.findDetour(HookId::TradeInviteMenu)
        ->getOriginal<decltype(&hkUserListPopup)>()(_this, 0, evt, ctrlId, a4);
}

// The player-list popup is built by the universal opener sub_B986A0, which picks
// one of three per-context populates (channel/friend/clan) and then shows
// E_DLG_LISTINFO. None of them touch our group-0 Trade button, so we detour the
// opener: run the original (which populates + positions the visible items for the
// current context), then anchor + reveal the Trade item.
//
// Each context shows a different number of menu items, but BLOCK (109004) is the
// last item the native populate positions in every context (sub_677720 computes
// block.y as the bottom slot). So we place Trade one row (27px) below Block's live
// y instead of a fixed XML y — that follows the menu in the sparse lobby context
// as well as the full friend menu. Position fields: control+0x308 = x, +0x30C = y,
// applied via the control's relayout vtbl (idx 27); matches sub_677720's own
// block-repositioning. char __thiscall(void* this, u8 atCursor).
char __fastcall hkUserListOpen(void* _this, void* /*edx*/, u8 atCursor) {
    using tOpen = char(__fastcall*)(void*, void*, u8);
    char ret = mg::ctx().hookRegistry()
        .findDetour(HookId::TradePopupPopulate)->getOriginal<tOpen>()(_this, nullptr, atCursor);

    int* dlg   = listInfoDialog();
    void* trade = tradeControl(dlg, addr::ui::userlist::TradeMenuId);
    if (!trade) return ret;

    // Control layout (verified): visible flag @ +881, enabled flag @ +882,
    // x @ +0x308, y @ +0x30C. All native menu buttons are 109001..109016 and always
    // exist as controls, so reading these fields is safe.
    constexpr int kPosXOff = 0x308, kPosYOff = 0x30C, kVisOff = 881, kEnaOff = 882, kRowH = 27;
    auto vis = [](void* c){ return c && *(reinterpret_cast<u8*>(c) + kVisOff); };

    // Position: anchor one row below the LOWEST visible standard menu button. Block
    // is the bottom item in most contexts, but in "Players Recommended" it is not —
    // scanning every visible button places Trade correctly in every context.
    bool found = false; int bestX = 0, bestY = 0;
    for (int id = 109001; id <= 109016; ++id) {
        void* c = tradeControl(dlg, id);
        if (!vis(c)) continue;
        int y = *reinterpret_cast<int*>(reinterpret_cast<u8*>(c) + kPosYOff);
        if (!found || y > bestY) { found = true; bestY = y; bestX = *reinterpret_cast<int*>(reinterpret_cast<u8*>(c) + kPosXOff); }
    }
    if (found) {
        *reinterpret_cast<int*>(reinterpret_cast<u8*>(trade) + kPosXOff) = bestX;
        *reinterpret_cast<int*>(reinterpret_cast<u8*>(trade) + kPosYOff) = bestY + kRowH;
        mg::callVFunc<void>(trade, 27);   // relayout (vtbl+0x6C)
    }

    // Gray-out: mirror the native Invite button exactly. The visible Invite control
    // (BATINVITE_FRIEND in the friend popup, BATINVITE elsewhere) carries the
    // engine's own invitable/uninvitable state in its enabled flag (+882); copy it.
    bool inviteEnabled = true;
    for (int invId : { 109003, 109002 }) {
        void* inv = tradeControl(dlg, invId);
        if (vis(inv)) { inviteEnabled = *(reinterpret_cast<u8*>(inv) + kEnaOff) != 0; break; }
    }
    mg::callVFunc<void>(trade, 24, 1);                       // Show
    mg::callVFunc<void>(trade, 22, inviteEnabled ? 1 : 0);   // Enable = mirror Invite
    return ret;
}

} // anonymous namespace

// ── Senders ───────────────────────────────────────────────────────────────────

void TradeHandler::sendInvite(const char* targetName) {
    // Invite by name (the same identifier whisper/friend-add use). The partner's
    // account id arrives later via the 192/190 recv and sets g_partnerAccountId.
    g_partnerAccountId = 0;
    TradeInviteReq req{};
    req.hdr.order = TRADE_INIT;
    int i = 0;
    for (; i < 15 && targetName[i]; ++i) { req.targetName[i] = targetName[i]; g_partnerName[i] = targetName[i]; }
    g_partnerName[i] = 0;                 // remember for STC_NAME_YOU
    sendRaw(&req, sizeof(req));
}

void TradeHandler::sendAccept(u32 targetAccountId) {
    g_partnerAccountId = targetAccountId;
    TradeTargetReq req{};
    req.hdr.order = TRADE_ACK;
    req.targetAccountId = targetAccountId;
    sendRaw(&req, sizeof(req));
}

void TradeHandler::acceptInvite() {
    if (g_inviterAccountId) sendAccept(g_inviterAccountId);
}

void TradeHandler::sendAddItem(u64 itemSerialInfo) {
    TradeItemReq req{};
    req.hdr.order = TRADE_ADD;
    req.serial.raw = itemSerialInfo;         // body+8
    sendRaw(&req, sizeof(req));
}

void TradeHandler::sendRemoveItem(u64 itemSerialInfo) {
    TradeItemReq req{};
    req.hdr.order = TRADE_REMOVE;
    req.serial.raw = itemSerialInfo;
    sendRaw(&req, sizeof(req));
}

void TradeHandler::sendLock()     { TradeEmptyReq r{}; r.hdr.order = TRADE_LOCK;     sendRaw(&r, sizeof(r)); }
void TradeHandler::sendFinalize() { TradeEmptyReq r{}; r.hdr.order = TRADE_FINALIZE; sendRaw(&r, sizeof(r)); }
void TradeHandler::sendCancel()   { TradeEmptyReq r{}; r.hdr.order = TRADE_CANCEL;   sendRaw(&r, sizeof(r)); g_tradeOpen = false; g_partnerAccountId = 0; }

// ── Lifecycle ─────────────────────────────────────────────────────────────────

TradeHandler::TradeHandler(::mg::MegaGuardContext& ctx, CustomPacketDispatcher& dispatcher)
    : ctx_(ctx), dispatcher_(dispatcher) {}

TradeHandler::~TradeHandler() = default;

void TradeHandler::registerDialogOnLobby() {
    registerTradeDialog();   // idempotent; see registerTradeDialog() for the 1:1 detail
}

VoidResult TradeHandler::install() {
    g_self = this;

    // Invite trigger: detour the userlist right-click popup OnEvent.
    ctx_.hookRegistry().registerDetour(HookId::TradeInviteMenu)
        .create(MG_CONST(addr::ui::userlist::PopupOnEvent), &hkUserListPopup);
    // Reveal the Trade menu item in every context: detour the universal opener.
    ctx_.hookRegistry().registerDetour(HookId::TradePopupPopulate)
        .create(MG_CONST(addr::ui::userlist::UserListOpen), &hkUserListOpen);
    // Per-frame tick: completes the deferred trade-window show after the inventory
    // scene loads (the trade panel is hosted by GAME_STATE_INVENTORY, not a popup).
    ctx_.hookRegistry().registerDetour(HookId::TradeFrameTick)
        .create(MG_CONST(addr::ui::custom_packets::NetFrameTick), &hkNetFrameTick);

    dispatcher_.registerHandler(TRADE_OPEN,     &onTradeOpen);
    dispatcher_.registerHandler(TRADE_INIT,     &onTradeRequest);   // S->C invite arrived
    dispatcher_.registerHandler(TRADE_ACK,      &onTradeAck);       // S->C invite answered
    dispatcher_.registerHandler(TRADE_ADD,      &onTradeItemChange);
    dispatcher_.registerHandler(TRADE_REMOVE,   &onTradeItemChange);
    dispatcher_.registerHandler(TRADE_LOCK,     &onTradeLock);
    dispatcher_.registerHandler(TRADE_FINALIZE, &onTradeFinalize);
    dispatcher_.registerHandler(TRADE_CANCEL,   &onTradeCancel);
    dispatcher_.registerHandler(TRADE_COMPLETE, &onTradeComplete);
    // 193 (money) intentionally not registered — no-money trade design.
    return VoidResult::ok();
}

} // namespace mg::game
