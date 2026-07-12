// =============================================================================
// MovementProtocol — 1:1 reimplementation (SEND side)
// =============================================================================
// hkMovementSend is a faithful port of MovementSend @ 0x00B20440
// (SEND_USER.cpp). It hooks and fully replaces the original; it never calls the
// trampoline. All engine helpers/globals are reached through their runtime
// addresses so behaviour is byte-identical to the original.
//
// The receive handler (OTHER_MOVE @ 0x00AE1E10) is intentionally NOT installed
// yet — SEND is landed first so it can be validated in isolation, then RECEIVE
// follows. See memory note movement_protocol_rewrite.md.
// =============================================================================
#include "pch.hpp"
#include "game/network/movement/movement_protocol.hpp"
#include "core/context.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "game/structures.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"

#include <cmath>

namespace mg::game {

namespace {

// ── Engine addresses (current client) ─────────────────────────────────────────
namespace addrs {
    constexpr uptr CUnitContainer_GetInstance = 0x004728D0;
    constexpr uptr GetLocalUnit               = 0x00911570; // (container) -> container+0x50
    constexpr uptr CNetMgr_GetInstance        = 0x00473D20;
    constexpr uptr GetRoom_GetInstance        = 0x004739C0;
    constexpr uptr CTick_GetInstance          = 0x004D5C80;
    constexpr uptr CTick_GetTick              = 0x00DF8F00; // m_uliTick / 10  (u64)
    constexpr uptr TcpPacket_Ctor             = 0x00548480; // memsets +4..+0x598
    constexpr uptr Float2Half                 = 0x00DF5C00; // __cdecl(float) -> u16
    constexpr uptr GetEquipSlot               = 0x008DDB40; // __thiscall(unit) -> slot
    constexpr uptr GetAnimId                  = 0x00962DC0; // __thiscall(stateMgr) -> int
    constexpr uptr GetRoomYaw                 = 0x00A4E110; // __thiscall(model) -> float
    constexpr uptr NormalizeRotation          = 0x0045EE20; // __cdecl(float*)
    constexpr uptr CanSendModInfo             = 0x00B1F940; // __cdecl() -> bool
    constexpr uptr PredictMovement            = 0x00B1FC10; // __cdecl(out12, data) -> coords
    constexpr uptr GetCoords                  = 0x00D2BC20; // __thiscall(movementInfo) -> this+16
    constexpr uptr ReplayRecord               = 0x00B27AB0; // __thiscall(room+316, &pkt) qmemcpy 0x5A8
    constexpr uptr RecordOwnHistory           = 0x00B1FA80; // __cdecl(coords, ownId)
    constexpr uptr GetTopCount                = 0x00476150; // __thiscall(room) -> int
    constexpr uptr SendPacket                 = 0x00AB7B40; // __thiscall(netmgr, &pkt, size)

    // data globals
    constexpr uptr byte_AgoraMode             = 0x011DB294; // byte_11DB294
    constexpr uptr g_CurrGameState            = 0x011E6D38;
    constexpr uptr byte_PredictPos            = 0x011C8CA9; // byte_11C8CA9
    constexpr uptr byte_ReplayRecord          = 0x011C8CA8; // byte_11C8CA8
    constexpr uptr n7_JumpState               = 0x011E2A90; // dword_11E2A90
    constexpr uptr byte_JumpFlag              = 0x011E2AAC; // byte_11E2AAC
    constexpr uptr jumpVecX                   = 0x011E2AA0; // dword_11E2AA0 (float)
    constexpr uptr jumpVecY                   = 0x011E2AA4;
    constexpr uptr jumpVecZ                   = 0x011E2AA8;
    constexpr uptr flt_RotBias                = 0x011E45C4; // flt_11E45C4
    constexpr uptr dbl_RotScaleNum            = 0x00FEBCC8; // dbl_FEBCC8
    constexpr uptr flt_RotScaleDen            = 0x0107B178; // flt_107B178
}

// ── Movement packet layout ────────────────────────────────────────────────────
// Named 1:1 after the server's structs in MegaVoltsPP
// common/NetEngine/Packets/PacketData.h. MSVC packs these bitfields LSB-first into
// the same u32, so overlaying them on the buffer is byte-identical to the manual
// dw[]/dd[]/shift math the 1:1 port used — just readable. (Half-floats are u16.)
namespace proto {
#pragma pack(push, 1)
    struct PositionStruct  { u16 positionX, positionY, positionZ; };   // +0
    struct DirectionStruct { u16 directionX, directionY, directionZ; };// +6
    struct JumpStruct      { u16 jump1, jump2; };
    struct BulletsStruct   { u16 bullet1, bullet2, bullet3, bullet4; };

    // client -> server (== server ClientPlayerInfoBasic, RELEASE_1_0_3 widths).
    struct ClientPlayerInfoBasic {
        PositionStruct  position;     // +0
        DirectionStruct direction;    // +6
        u32 matchTick;                // +12  (bit31 doubles as the extended/jump marker)
        u32 animation1 : 8;           // +16  bits  0-7  : action state
        u32 animation2 : 6;           //      bits  8-13 : move state
        u32 weapon     : 4;           //      bits 14-17 : weapon / game-mode
        u32 rotation   : 9;           //      bits 18-26 : 9-bit body yaw
        u32 crouch     : 1;           //      bit  27    : crouch (server lumps 27-31 as "unknown")
        u32 specialAct : 1;           //      bit  28    : sliding / skill
        u32 _unused    : 3;           //      bits 29-31
    };
    static_assert(sizeof(ClientPlayerInfoBasic) == 20, "ClientPlayerInfoBasic must be 20 bytes");

    // server -> client per-entry header (== server SpecificInfo).
    struct SpecificInfo {
        u32 sessionId      : 14;
        u32 enableMovement : 1;   // >>14
        u32 enableBullet   : 1;   // >>15
        u32 animation1     : 7;   // >>16  (action state)
        u32 enableRotation : 1;   // >>23
        u32 animation2     : 6;   // >>24  (move state)
        u32 _unused        : 1;   // >>30
        u32 enableJump     : 1;   // >>31
    };
    static_assert(sizeof(SpecificInfo) == 4, "SpecificInfo must be 4 bytes");
#pragma pack(pop)
} // namespace proto

// ── Typed engine wrappers ─────────────────────────────────────────────────────
MG_FORCEINLINE uptr unitContainer()  { return mg::call<uptr(__cdecl*)()>(MG_CONST(addrs::CUnitContainer_GetInstance)); }
MG_FORCEINLINE uptr localUnit()      { return mg::call<uptr(__thiscall*)(uptr)>(MG_CONST(addrs::GetLocalUnit), unitContainer()); }
MG_FORCEINLINE uptr netMgr()         { return mg::call<uptr(__cdecl*)()>(MG_CONST(addrs::CNetMgr_GetInstance)); }
MG_FORCEINLINE uptr getRoom()        { return mg::call<uptr(__cdecl*)()>(MG_CONST(addrs::GetRoom_GetInstance)); }
MG_FORCEINLINE u32  getTick()        { auto inst = mg::call<uptr(__cdecl*)()>(MG_CONST(addrs::CTick_GetInstance));
                                        return static_cast<u32>(mg::call<u64(__thiscall*)(uptr)>(MG_CONST(addrs::CTick_GetTick), inst)); }
MG_FORCEINLINE u16  f2h(float f)     { return mg::call<u16(__cdecl*)(float)>(MG_CONST(addrs::Float2Half), f); }
MG_FORCEINLINE int  animId(uptr mgr) { return mg::call<int(__thiscall*)(uptr)>(MG_CONST(addrs::GetAnimId), mgr); }
MG_FORCEINLINE float roomYaw(void* model) { return mg::call<float(__thiscall*)(void*)>(MG_CONST(addrs::GetRoomYaw), model); }

template <typename T> MG_FORCEINLINE T   rd(uptr a)            { return *reinterpret_cast<T*>(a); }

// ── hkMovementSend — 1:1 port of MovementSend (0xB20440) ──────────────────────
char __cdecl hkMovementSend()
{
    const bool agora = rd<u8>(MG_CONST(addrs::byte_AgoraMode)) != 0;
    const u32  state = rd<u32>(MG_CONST(addrs::g_CurrGameState));

    if (!agora && state < GAME_STATE_MOD_LOADING)   return 1;
    if (state == GAME_STATE_TUTORIAL_PLAYING)       return 1;
    if (!localUnit())                               return 1;
    if (!agora && !mg::call<bool(__cdecl*)()>(MG_CONST(addrs::CanSendModInfo))) return 1;

    // castserver connector connected?  netmgr[+0x1020] -> +92
    if (!rd<u8>(rd<uptr>(netMgr() + 0x1020) + 92))  return 0;

    const uptr unit  = localUnit();
    auto*      model = mg::callVFunc<float*>(reinterpret_cast<void*>(unit), 9); // GetModelProperty
    if (!model)                                     return 1;

    // ── Packet buffer (matches the engine's on-stack CTcpPacket) ──────────────
    struct alignas(4) TcpPacketBuf {
        u16 cmd;            // +0
        u8  option;         // +2  (pitch)
        u8  extra;          // +3  (roll)
        u8  payload[0x5AC]; // +4  (data)  — >= ctor memset(0x594) and replay copy(0x5A8)
    } pkt;
    mg::call<void(__thiscall*)(void*)>(MG_CONST(addrs::TcpPacket_Ctor), &pkt);

    pkt.cmd = static_cast<u16>((pkt.cmd & 0x3F) | 0x4640);
    if (agora)
        pkt.cmd = static_cast<u16>((pkt.cmd & 0xFFCF) | 0x10);

    u8*  data = pkt.payload;
    u16* dw   = reinterpret_cast<u16*>(data);   // jump/bullet tail past the 20-byte head
    auto* msg = reinterpret_cast<proto::ClientPlayerInfoBasic*>(data);

    // ── position + direction ──────────────────────────────────────────────────
    float dir[3];
    if (rd<u8>(MG_CONST(addrs::byte_PredictPos)))
    {
        // PredictMovement writes msg->position itself and returns the predicted
        // vector used for the direction slot.
        float scratch[3];
        auto* p = mg::call<float*(__cdecl*)(void*, void*)>(
            MG_CONST(addrs::PredictMovement), scratch, data);
        dir[0] = p[0]; dir[1] = p[1]; dir[2] = p[2];
    }
    else
    {
        // pos = coords(localUnit->CExPlayer[+0x10] = MovementInfo)
        auto* movementInfo = rd<char*>(rd<uptr>(unit + 268) + 16);
        auto* pos = mg::call<float*(__thiscall*)(char*)>(MG_CONST(addrs::GetCoords), movementInfo);
        float px = pos[0], py = pos[1], pz = pos[2];

        if (agora && state != GAME_STATE_AGORA_PLAYING)
        {
            auto* u = reinterpret_cast<float*>(unit);
            px = u[137]; py = u[138]; pz = u[139];   // unit+0x224
        }

        msg->position.positionX = f2h(px);
        msg->position.positionY = f2h(py);
        msg->position.positionZ = f2h(pz);

        auto* d = reinterpret_cast<float*>(unit + 148);  // unit+0x94 facing
        dir[0] = d[0]; dir[1] = d[1]; dir[2] = d[2];
    }
    msg->direction.directionX = f2h(dir[0]);
    msg->direction.directionY = f2h(dir[1]);
    msg->direction.directionZ = f2h(dir[2]);

    // ── Body rotation (9 bits @ bits18..26) + pitch/roll header bytes ─────────
    const double scaleNum = rd<double>(MG_CONST(addrs::dbl_RotScaleNum));
    const float  scaleDen = rd<float>(MG_CONST(addrs::flt_RotScaleDen));
    const float  rotScale = static_cast<float>(scaleNum / scaleDen);

    // ── Rotation — faithful to the UNPATCHED original MovementSend ────────────
    // The protocol carries THREE rotation quantities. OTHER_MOVE reconstructs them
    // (see hkMovementReceive readRot + the no-rotation fallback) and the camera /
    // spectate uses  yaw = room_info->Yaw + model->Rotation(0xA4),
    //                pitch = room_info->Pitch + model->Pitch(0xA8) + idk_c4 + idk_ac:
    //
    //   • 9-bit body yaw = room_info->Yaw + model[0x444]      -> remote room_info->Yaw
    //         model[0x444] is the BODY yaw; it carries the A/D strafe LEAN.
    //   • aim-yaw byte   = (model[0x44C] + model[0xA4]) ·180/π -> remote Rotation(0xA4)
    //         model[0x44C] is the lean *COMPENSATION*: it cancels the body lean so the
    //         transmitted gun-aim (Yaw + Rotation) stays true while the body still leans.
    //   • pitch byte     = model[0xA8] ·180/π                 -> remote Pitch(0xA8)
    //
    // In vanilla this is self-consistent: body leans, aim stays correct. The
    // "looking where it rotates / weird angle on A/D" bug was NOT in the original —
    // it was caused by custom_tickrate's rot1/2/3 byte-patches (kRotationByteValue =
    // FLDZ) zeroing model[0x444] (the lean) and model[0x44C] (the aim compensation)
    // INSIDE MovementSend. Because this hook fully REPLACES MovementSend, the right
    // fix is simply to reproduce the original UNPATCHED math below — that bypasses
    // the byte-patches and restores correct aim from every POV *and* the lean anim.
    // (The rot1/2/3 patches in custom_tickrate are now dead while the hook is on;
    //  leave them or disable them, they no longer execute.)
    const float bodyYawDelta = model[273]; // 0x444 — body yaw, includes the A/D lean
    // NOTE: model[275]/0x44C (vanilla "aim-yaw lean compensation") is deliberately NOT used.
    // It only equals -bodyYawDelta in steady state; mid-rotation 0x444 and 0x44C update at
    // different rates so they fail to cancel -> reconstructed aim (body_yaw + Rotation) splits
    // into two directions until the turn settles. We compensate with bodyYawDelta itself below
    // so the cancellation is exact at every instant (keeps the body lean, fixes the aim).

    float yaw = roomYaw(model) + bodyYawDelta;
    if (std::fabs(bodyYawDelta) > roomYaw(model))
        yaw = roomYaw(model) + rd<float>(MG_CONST(addrs::flt_RotBias)) + bodyYawDelta;

    float rotation = rotScale * yaw;
    mg::call<void(__cdecl*)(float*)>(MG_CONST(addrs::NormalizeRotation), &rotation);
    msg->rotation = static_cast<u32>(static_cast<int>(rotation) & 0x1FF);   // 9-bit body yaw

    // VANILLA (verified in MovementSend asm @ b208ec / the option store): the aim-yaw byte is
    // written to m_tHeader.EXTRA and the pitch byte to m_tHeader.OPTION — NOT the other way
    // round. The server then forwards extra->rotation1 (aim) and option->rotation2 (pitch), and
    // OTHER_MOVE maps rotation1->model Rotation(0xA4), rotation2->Pitch(0xA8). Swapping these (the
    // previous code) sends aim as pitch and pitch as aim end-to-end -> the wrong-angle bug.
    pkt.extra  = static_cast<u8>(static_cast<__int64>(rotScale * (model[41] - bodyYawDelta))); // aim-yaw = (0xA4 - 0x444): cancels the body lean EXACTLY (incl. mid-rotation) -> reconstructed aim = roomYaw + 0xA4
    pkt.option = static_cast<u8>(static_cast<__int64>(rotScale * model[42]));                // pitch   -> OPTION -> rotation2 -> Pitch(0xA8)

    // ── matchTick (bit31 set later as the jump-extended marker) ───────────────
    const u32 tick = getTick();
    msg->matchTick = tick & 0x7FFFFFFFu;

    // ── animation2 (move state) ───────────────────────────────────────────────
    const int moveAnim = animId(rd<uptr>(unit + 296));   // unit+0x128
    msg->animation2 = static_cast<u32>(moveAnim & 0x3F);

    // ── animation1 (action state) ────────────────────────────────────────────
    const uptr actionMgr = rd<uptr>(unit + 300);         // unit+0x12C
    u8 anim1;
    if (rd<u32>(actionMgr + 12))
        anim1 = static_cast<u8>(rd<u32>(rd<uptr>(actionMgr + 0xC) + 4));
    else
        anim1 = 0xFF;
    msg->animation1 = anim1;

    // ── special action (sliding/skill) + crouch (source differs in anim 13) ───
    msg->specialAct = (moveAnim == 18 || moveAnim == 15 || moveAnim == 13) ? 1u : 0u;
    msg->crouch     = (moveAnim == 13 ? rd<u8>(unit + 53) : rd<u8>(unit + 52)) ? 1u : 0u;

    // ── Spawn / alive gating ──────────────────────────────────────────────────
    if (agora)
    {
        if (state == GAME_STATE_AGORA_PLAYING)
        {
            if ((rd<u32>(rd<uptr>(rd<uptr>(unit + 268) + 4) + 8) & 0xF) != 0)
                return 1;
        }
    }
    else
    {
        const uptr exPlayer = rd<uptr>(unit + 268);
        const uptr roomInfo = rd<uptr>(exPlayer + 4);
        if ((rd<u32>(roomInfo + 8) & 0xF) != 0xB ||                       // STATUS_NORMAL
            static_cast<int>(rd<u32>(roomInfo + 20) ^ rd<u32>(roomInfo + 12)) <= 0)  // alive (hp xor)
            return 1;
    }

    // ── Weapon / game-mode bits (bits14..17) ──────────────────────────────────
    auto  slot = mg::call<uptr(__thiscall*)(uptr)>(MG_CONST(addrs::GetEquipSlot), unit);
    auto  item = mg::callVFunc<uptr>(reinterpret_cast<void*>(slot), 9);
    if (item)
    {
        const u32 room236 = rd<u32>(getRoom() + 236);
        const u32 wmode   = rd<u32>(item + 652);          // weapon+0x28C
        u32 modeBits;
        if (room236 == 7 || room236 == 9)      modeBits = wmode & 0xF;
        else if (room236 == wmode)             modeBits = wmode & 0xF;
        else if (wmode)                        modeBits = room236 & 0xF;
        else                                   modeBits = wmode & 0xF;
        msg->weapon = modeBits;
    }
    else
    {
        msg->weapon = 1u;   // 0x4000 == weapon field bit0
    }

    // ── Jump / bullet payload sizing ──────────────────────────────────────────
    const int n7 = rd<int>(MG_CONST(addrs::n7_JumpState));
    u32 size;
    if (n7 == 7)
    {
        if (msg->specialAct)     // special action -> trailing jump struct
        {
            dw[10] = f2h(rd<float>(unit + 56));
            dw[11] = static_cast<u16>(getTick() - rd<u16>(unit + 64));
            msg->matchTick &= 0x7FFFFFFFu;
            size = 24;
        }
        else
        {
            msg->matchTick &= 0x7FFFFFFFu;
            size = 20;
        }
    }
    else
    {
        msg->matchTick |= 0x80000000u;                     // extended marker (matchTick bit31)
        u16& v96 = dw[13];                                 // data+26 (bullet4 slot)
        v96 = static_cast<u16>((16 * (rd<u8>(MG_CONST(addrs::byte_JumpFlag)) & 1)) | (v96 & 0xFFEF));
        v96 = static_cast<u16>((n7 & 0xF) | (v96 & 0xFFF0));
        dw[10] = f2h(rd<float>(MG_CONST(addrs::jumpVecX)));
        dw[11] = f2h(rd<float>(MG_CONST(addrs::jumpVecY)));
        dw[12] = f2h(rd<float>(MG_CONST(addrs::jumpVecZ)));
        if (msg->specialAct)     // + trailing jump struct
        {
            dw[14] = f2h(rd<float>(unit + 56));
            dw[15] = static_cast<u16>(getTick() - rd<u16>(unit + 64));
            size = 32;
        }
        else
        {
            size = 28;
        }
    }

    // ── Replay record path, or real send ──────────────────────────────────────
    if (rd<u8>(MG_CONST(addrs::byte_ReplayRecord)))
    {
        mg::call<void(__thiscall*)(uptr, const void*)>(
            MG_CONST(addrs::ReplayRecord), getRoom() + 316, &pkt);
        return 1;
    }

    if (mg::call<int(__thiscall*)(uptr, void*, int)>(
            MG_CONST(addrs::SendPacket), netMgr(), &pkt, static_cast<int>(size)) >= 0)
    {
        const uptr room = getRoom();
        if (mg::callVFunc<u8>(reinterpret_cast<void*>(room), 23) &&
            mg::call<int(__thiscall*)(uptr)>(MG_CONST(addrs::GetTopCount), room) == 1)
        {
            // own network id = *(CExPlayer->room_info) & 0x7FFFFFFF
            const u32 id = rd<u32>(rd<uptr>(rd<uptr>(unit + 268) + 4)) & 0x7FFFFFFF;
            auto* mi = rd<char*>(rd<uptr>(unit + 268) + 16);
            auto* coords = mg::call<float*(__thiscall*)(char*)>(MG_CONST(addrs::GetCoords), mi);
            mg::call<char(__cdecl*)(float*, int)>(MG_CONST(addrs::RecordOwnHistory), coords, static_cast<int>(id));
        }
        return 1;
    }

    return 0;
}

// ── Receive-side engine addresses (current client) ────────────────────────────
namespace raddrs {
    constexpr uptr GuardCheck     = 0x00942E30; // __thiscall(&flag) -> bool (recv allowed)
    constexpr uptr byte_GuardFlag = 0x011E6D30;
    constexpr uptr n21            = 0x011E6D44; // mod-stop owner
    constexpr uptr Seekpos        = 0x00DF8F20; // __thiscall(tickInst) -> i64
    constexpr uptr Sub549930      = 0x00549930; // __thiscall(room, int)
    constexpr uptr UnitRegistry   = 0x00472800; // __cdecl() -> registry
    constexpr uptr LookupUnit     = 0x00925050; // __thiscall(reg, id) -> unit
    constexpr uptr RemoveUnit     = 0x00925270; // __thiscall(reg, unit, u32)
    constexpr uptr HalfToFloat    = 0x00DF5BE0; // __cdecl(u16) -> float
    constexpr uptr InitMoveState  = 0x00A65A70; // __thiscall(&src)
    constexpr uptr ApplyMoveState = 0x00A25960; // __thiscall(moveObj, &src)
    constexpr uptr GetEquip       = 0x008DDB40; // __thiscall(unit) -> equipment
    constexpr uptr GetCoords2     = 0x00D2BC20; // __thiscall(moveObj) -> coords
    constexpr uptr Sub_A15E00     = 0x00A15E00; // __thiscall(moveObj) -> int
    constexpr uptr Sub_A15DE0     = 0x00A15DE0; // __thiscall(moveObj) -> ptr
    constexpr uptr Sub_556550     = 0x00556550; // __thiscall(room, int) -> bool
    constexpr uptr CullTest       = 0x005571F0; // __thiscall(room, float) -> bool
    constexpr uptr SendRoomPacket = 0x00554D40; // __thiscall(room, &pkt, size, 0)
    constexpr uptr Sub_500C20     = 0x00500C20; // __thiscall(x) -> int
    constexpr uptr GunRaceA       = 0x008E4E40; // __thiscall(unit)
    constexpr uptr GunRaceB       = 0x00761570; // __thiscall(unit)
    constexpr uptr GetDlgByName   = 0x00E4E660; // __cdecl(name) -> dialog
    constexpr uptr RoomYaw        = 0x00A4E110; // __thiscall(model) -> float
    constexpr uptr dbl_FEB198     = 0x00FEB198; // tick divisor (double)
    constexpr uptr flt_107359C    = 0x0107359C; // rotation numerator (float)
    constexpr uptr dbl_FEBCC8     = 0x00FEBCC8; // rotation divisor (double)
    constexpr u32  STATUS_NORMAL  = 0x0B;
    constexpr u32  STATUS_DYING   = 0x0C;       // verify against game's UnitStatus enum
}

// Resolve a vtable method: vfn<Fn>(obj, byteOffset)(obj, args...).
template <typename Fn>
MG_FORCEINLINE Fn vfn(uptr obj, int byteOff)
{
    return reinterpret_cast<Fn>(*reinterpret_cast<uptr*>(*reinterpret_cast<uptr*>(obj) + byteOff));
}

struct alignas(4) RecvTcpPacketBuf { u16 cmd; u8 option; u8 extra; u8 payload[0x5AC]; };

// Build the local self / replay echo correction packet (cmd 0x4200, 24 bytes).
MG_FORCEINLINE void buildEcho(RecvTcpPacketBuf& c, u32 netId, float p0, float p1, float p2)
{
    mg::call<void(__thiscall*)(void*)>(MG_CONST(addrs::TcpPacket_Ctor), &c);
    c.cmd = static_cast<u16>((c.cmd & 0x3F) | 0x4200);
    c.option = 0;
    c.extra  = 1;
    u8* d = c.payload;
    *reinterpret_cast<u16*>(d + 4) = f2h(p0);
    *reinterpret_cast<u16*>(d + 6) = f2h(p1);
    *reinterpret_cast<u16*>(d + 8) = f2h(p2);
    *reinterpret_cast<u32*>(d + 0)  = netId;
    *reinterpret_cast<u32*>(d + 12) = netId;
    *reinterpret_cast<u32*>(d + 16) &= 0xFFF00000;
    *reinterpret_cast<u32*>(d + 16) &= 0xFE0FFFFF;
    const uptr r196 = rd<uptr>(getRoom() + 196);
    *reinterpret_cast<u32*>(d + 20) =
        r196 ? mg::call<u32(__thiscall*)(uptr)>(MG_CONST(raddrs::Sub_500C20), r196) : 0u;
    mg::call<void(__thiscall*)(uptr, void*, int, int)>(
        MG_CONST(raddrs::SendRoomPacket), getRoom(), &c, 24, 0);
}

// ── hkMovementReceive — 1:1 port of OTHER_MOVE (0xAE1E10, OTHER.cpp) ───────────
// Parses the server's OTHER_MOVE rebroadcast (one header byte = entry count, a
// shared tick, then `count` variable-length entries) and, per entry: looks up the
// unit, applies the local self-correction echo, or builds + applies the remote
// move-state (pos/vel, muzzle+bullet dir, rotation, animation) via the engine's
// move-state object. Verbose ErrLog debug spam in the original is omitted (debug
// only); all gameplay control flow, packet offsets and engine calls are 1:1.
void __cdecl hkMovementReceive(void* packet)
{
    auto* pkt = static_cast<u8*>(packet);

    if (!mg::call<u8(__thiscall*)(uptr)>(MG_CONST(raddrs::GuardCheck), MG_CONST(raddrs::byte_GuardFlag)))
        return;

    const uptr modOwner = rd<uptr>(MG_CONST(raddrs::n21));
    if (modOwner && rd<u32>(MG_CONST(addrs::g_CurrGameState)) == GAME_STATE_MOD_STOP)
    {
        if (rd<u8>(modOwner + 0x2D))
            return;
    }

    // Room tick housekeeping (lag-compensation timestamps).
    auto tickInst = [&] { return mg::call<uptr(__cdecl*)()>(MG_CONST(addrs::CTick_GetInstance)); };
    auto seekpos  = [&](uptr ti) { return mg::call<long long(__thiscall*)(uptr)>(MG_CONST(raddrs::Seekpos), ti); };
    const long long savedTick = rd<long long>(getRoom() + 0x198);
    mg::call<void(__thiscall*)(uptr, int)>(MG_CONST(raddrs::Sub549930), getRoom(),
        static_cast<int>(seekpos(tickInst()) - savedTick - 100));
    *reinterpret_cast<long long*>(getRoom() + 408) = seekpos(tickInst());
    *reinterpret_cast<long long*>(getRoom() + 416) = seekpos(tickInst());

    const u32    tick      = *reinterpret_cast<u32*>(pkt + 4);
    const double tickDiv   = rd<double>(MG_CONST(raddrs::dbl_FEB198));
    const double rotScaleR = static_cast<double>(rd<float>(MG_CONST(raddrs::flt_107359C)))
                           / rd<double>(MG_CONST(raddrs::dbl_FEBCC8));
    const u32    count     = *reinterpret_cast<u8*>(pkt + 3);

    uptr si = reinterpret_cast<uptr>(pkt + 8);   // rolling specificInfo (entry) pointer

    for (u32 idx = 0; idx < count; ++idx)
    {
        auto h2f = [&](int off) {
            return mg::call<float(__cdecl*)(u32)>(MG_CONST(raddrs::HalfToFloat), *reinterpret_cast<u16*>(si + off));
        };

        // Per-entry header == server SpecificInfo (PacketData.h). `header` kept for the
        // couple of raw-bit reads further down (animation1/animation2/spawn bit).
        const u32   header = *reinterpret_cast<u32*>(si);
        const auto& info   = *reinterpret_cast<const proto::SpecificInfo*>(si);
        const bool  enMove = info.enableMovement;
        const bool  enBul  = info.enableBullet;
        const bool  enRot  = info.enableRotation;
        const bool  enJump = info.enableJump;

        const u32  netHi  = static_cast<u16>(*reinterpret_cast<u32*>(rd<uptr>(netMgr() + 1031 * 4) + 60));
        const u32  unitId = info.sessionId | (netHi << 16);
        const uptr reg    = mg::call<uptr(__cdecl*)()>(MG_CONST(raddrs::UnitRegistry));
        const uptr unit   = mg::call<uptr(__thiscall*)(uptr, u32)>(MG_CONST(raddrs::LookupUnit), reg, unitId);
        if (!unit)
            goto advance;

        if (localUnit() == unit)
        {
            // ── SELF: server position echo / correction ──────────────────────
            if (enMove)
            {
                const float p0 = h2f(4), p1 = h2f(6), p2 = h2f(8);
                if (vfn<u8(__thiscall*)(uptr)>(getRoom(), 92)(getRoom())
                    && mg::call<u8(__thiscall*)(uptr, float)>(MG_CONST(raddrs::CullTest), getRoom(), p2))
                {
                    RecvTcpPacketBuf c;
                    const u32 id = rd<u32>(rd<uptr>(rd<uptr>(unit + 268) + 4)) & 0x7FFFFFFF;
                    buildEcho(c, id, p0, p1, p2);
                }
            }
        }
        else
        {
            // ── REMOTE: build + apply move-state ──────────────────────────────
            const uptr exObj    = rd<uptr>(unit + 268);   // CExPlayer
            const uptr roomInfo = rd<uptr>(exObj + 4);
            const u32  status   = rd<u32>(roomInfo + 8) & 0xF;
            const int  hpXor    = static_cast<int>(rd<u32>(roomInfo + 20) ^ rd<u32>(roomInfo + 12));

            if (info.animation2 != 32)   // move state != 32 (idle)
            {
                if (status == raddrs::STATUS_DYING)
                {
                    if (hpXor > 0)
                    {
                        vfn<void(__thiscall*)(uptr, int)>(exObj, 92)(exObj, 11);
                        mg::call<void(__thiscall*)(uptr, uptr, u32)>(MG_CONST(raddrs::RemoveUnit), reg, unit, 1);
                    }
                }
                if (rd<u32>(MG_CONST(addrs::g_CurrGameState)) == GAME_STATE_AGORA_PLAYING)
                {
                    mg::call<void(__thiscall*)(uptr, uptr, u32)>(MG_CONST(raddrs::RemoveUnit), reg, unit, 1);
                }
                else if (status == raddrs::STATUS_NORMAL)
                {
                    if (hpXor > 0)
                        mg::call<void(__thiscall*)(uptr, uptr, u32)>(MG_CONST(raddrs::RemoveUnit), reg, unit, 1);
                }
                else if (status <= 0xA)
                {
                    goto advance;   // (original logs an OTHER_MOVE error for status > 0xA)
                }
            }

            const uptr moveObj = rd<uptr>(exObj + 16);
            if (moveObj)
            {
                alignas(8) u8 src[0xA0];
                mg::call<void(__thiscall*)(void*)>(MG_CONST(raddrs::InitMoveState), src);
                auto sf  = [&](int o) -> float& { return *reinterpret_cast<float*>(src + o); };

                sf(0)                                     = static_cast<float>(tick) / static_cast<float>(tickDiv);
                *reinterpret_cast<int*>(src + 8)          = static_cast<int>(getTick());
                *reinterpret_cast<int*>(src + 0xC)        = static_cast<int>(info.animation2); // move state

                float v99 = 0, v100 = 0, v101 = 0, v102 = 0, v103 = 0, v104 = 0;
                float v105 = 0, v106 = 0, v107 = 0;          // room-yaw, Rotation(0xA4 aim-yaw), Pitch(0xA8)
                float v109 = 0, v110 = 0, v111 = 0;          // muzzle
                float v112 = 0, v113 = 0, v114 = 0;          // bullet dir
                float v119 = 0; int v120 = 0;
                u8 n7 = 7, n7_2 = 0, v117 = 0;
                u8 animation1 = static_cast<u8>(info.animation1);   // action state

                auto readWeapon = [&](int dirOff, u16 wb) {
                    uptr equip  = mg::call<uptr(__thiscall*)(uptr)>(MG_CONST(raddrs::GetEquip), unit);
                    uptr weapon = equip ? vfn<uptr(__thiscall*)(uptr, int)>(equip, 16)(equip, (wb & 0xF) + 10) : 0;
                    if (weapon)
                    {
                        n7 = wb & 0xF; v117 = (wb & 0x10) != 0;
                        char mbuf[12];
                        float* mz = vfn<float*(__thiscall*)(uptr, char*)>(weapon, 200)(weapon, mbuf);
                        v109 = mz[0]; v110 = mz[1]; v111 = mz[2];
                        v112 = h2f(dirOff); v113 = h2f(dirOff + 2); v114 = h2f(dirOff + 4);
                    }
                    else { n7 = 7; v109 = v99; v110 = v100; v111 = v101; }
                };
                auto readRot = [&](int rotOff) {
                    v105 = static_cast<float>(static_cast<double>(*reinterpret_cast<u16*>(si + rotOff + 2) & 0x1FF) * rotScaleR);
                    v106 = static_cast<float>(*reinterpret_cast<int8_t*>(si + rotOff)     * rotScaleR);
                    v107 = static_cast<float>(*reinterpret_cast<int8_t*>(si + rotOff + 1) * rotScaleR);
                    n7_2 = (*reinterpret_cast<u16*>(si + rotOff + 2) >> 9) & 0xF;
                };

                if (enMove)
                {
                    v99 = h2f(4);  v100 = h2f(6);  v101 = h2f(8);
                    v102 = h2f(10); v103 = h2f(12); v104 = h2f(14);
                    if (enBul)
                    {
                        readWeapon(16, *reinterpret_cast<u16*>(si + 22));
                        if (enRot) { readRot(24); if (enJump) { v119 = h2f(28); v120 = *reinterpret_cast<u16*>(si + 30); } }
                        else if (enJump) { v119 = h2f(24); v120 = *reinterpret_cast<u16*>(si + 26); }
                    }
                    else if (enRot) { readRot(16); if (enJump) { v119 = h2f(20); v120 = *reinterpret_cast<u16*>(si + 22); } }
                    else if (enJump) { v119 = h2f(16); v120 = *reinterpret_cast<u16*>(si + 18); }
                }
                else if (enBul)
                {
                    readWeapon(4, *reinterpret_cast<u16*>(si + 10));
                    if (enRot) { readRot(12); if (enJump) { v119 = static_cast<float>(*reinterpret_cast<u16*>(si + 16)); v120 = *reinterpret_cast<u16*>(si + 18); } }
                    else if (enJump) { v119 = static_cast<float>(*reinterpret_cast<u16*>(si + 12)); v120 = *reinterpret_cast<u16*>(si + 14); }
                }
                else if (enRot)
                {
                    readRot(4);
                    if (enJump) { v119 = static_cast<float>(*reinterpret_cast<u16*>(si + 8)); v120 = *reinterpret_cast<u16*>(si + 10); }
                }
                else if (enJump)
                {
                    v119 = static_cast<float>(*reinterpret_cast<u16*>(si)); v120 = *reinterpret_cast<u16*>(si + 2);
                }

                // No rotation in packet -> derive from the unit's own model + team logic.
                if (!enRot)
                {
                    uptr model = vfn<uptr(__thiscall*)(uptr)>(unit, 36)(unit);
                    v105 = mg::call<float(__thiscall*)(uptr)>(MG_CONST(raddrs::RoomYaw), model);
                    v106 = *reinterpret_cast<float*>(model + 164);
                    v107 = *reinterpret_cast<float*>(model + 168);
                    int n7_1 = ((static_cast<int>(rd<u32>(roomInfo + 8)) >> 4) & 0x1F) - 10;
                    const u32 r236 = rd<u32>(getRoom() + 236);
                    if (n7_1 > 7)                       n7_2 = 1;
                    else if (r236 == 7 || r236 == 9)    n7_2 = static_cast<u8>(n7_1);
                    else if (r236 == static_cast<u32>(n7_1) || n7_1) n7_2 = static_cast<u8>(r236);
                    else                                n7_2 = 0;
                }

                // No movement in packet -> reuse the unit's current coords.
                if (!enMove)
                {
                    if (rd<u8>(moveObj + 56))
                        goto advance;
                    float* coords;
                    if (mg::call<int(__thiscall*)(uptr)>(MG_CONST(raddrs::Sub_A15E00), moveObj) <= 0)
                        coords = mg::call<float*(__thiscall*)(uptr)>(MG_CONST(raddrs::GetCoords2), moveObj);
                    else
                        coords = reinterpret_cast<float*>(mg::call<uptr(__thiscall*)(uptr)>(MG_CONST(raddrs::Sub_A15DE0), moveObj) + 16);
                    v99 = coords[0]; v100 = coords[1]; v101 = coords[2];
                    v102 = v99; v103 = v100; v104 = v101;
                }

                if (animation1 == 68)   // landing -> idle if grounded
                {
                    const int arg = static_cast<int>(rd<u32>(unit + 0x10C) + 0x64);
                    if (mg::call<u8(__thiscall*)(uptr, int)>(MG_CONST(raddrs::Sub_556550), getRoom(), arg))
                        animation1 = 32;
                }

                // Commit the move-state fields and apply.
                sf(0x10) = v99;  sf(0x14) = v100; sf(0x18) = v101;
                sf(0x1C) = v102; sf(0x20) = v103; sf(0x24) = v104;
                sf(0x28) = v105; sf(0x2C) = v106; sf(0x30) = v107;
                *reinterpret_cast<int*>(src + 0x38) = (header >> 30) & 1;
                sf(0x40) = v109; sf(0x44) = v110; sf(0x48) = v111;
                sf(0x4C) = v112; sf(0x50) = v113; sf(0x54) = v114;
                src[0x58] = n7; src[0x59] = n7_2; src[0x5A] = v117; src[0x5B] = animation1;
                sf(0x60) = v119; *reinterpret_cast<int*>(src + 0x64) = v120;

                mg::call<void(__thiscall*)(uptr, void*)>(MG_CONST(raddrs::ApplyMoveState), moveObj, src);

                // GunRace mode special-case.
                if (vfn<int(__thiscall*)(uptr)>(getRoom(), 60)(getRoom()) == 8 && n7 != 7 && n7)
                {
                    mg::call<void(__thiscall*)(uptr)>(MG_CONST(raddrs::GunRaceA), unit);
                    mg::call<uptr(__cdecl*)(const char*)>(MG_CONST(raddrs::GetDlgByName), MG_STR("E_DLG_GUNRACE"));
                    mg::call<void(__thiscall*)(uptr)>(MG_CONST(raddrs::GunRaceB), unit);
                }

                // Replay / spectator echo of the applied position.
                if (vfn<u8(__thiscall*)(uptr)>(getRoom(), 92)(getRoom())
                    && mg::call<u8(__thiscall*)(uptr, float)>(MG_CONST(raddrs::CullTest), getRoom(), v101))
                {
                    RecvTcpPacketBuf c;
                    const u32 id = rd<u32>(rd<uptr>(exObj + 4)) & 0x7FFFFFFF;
                    buildEcho(c, id, v99, v100, v101);
                }
            }
        }

    advance:
        if (enMove)
        {
            if (enBul)      si += enRot ? (enJump ? 32 : 28) : (enJump ? 28 : 24);
            else if (enRot) si += enJump ? 24 : 20;
            else            si += enJump ? 20 : 16;
        }
        else if (enBul)
        {
            if (enRot)      si += enJump ? 20 : 16;
            else            si += enJump ? 16 : 12;
        }
        else if (enRot)     si += enJump ? 12 : 8;
        else                si += 4;
    }

    if (!rd<u8>(getRoom() + 544))
        *reinterpret_cast<u8*>(getRoom() + 544) = 1;
}

} // anonymous namespace

// ── MovementProtocol class ────────────────────────────────────────────────────

MovementProtocol::MovementProtocol(MegaGuardContext& ctx) : ctx_(ctx) {}
MovementProtocol::~MovementProtocol() = default;

VoidResult MovementProtocol::install()
{
    // ── MOVEMENT PATCH HOOK TEMPORARILY DISABLED ──────────────────────────────
    // The SEND/RECEIVE 1:1 movement hooks are commented out while we work on the
    // sniper/rifle zoom custom-sensitivity feature. Re-enable by uncommenting the
    // registerDetour calls below. (hkMovementSend / hkMovementReceive remain
    // compiled — they are simply not installed.)
    //
    // auto& registry = ctx_.hookRegistry();
    // registry.registerDetour(HookId::MovementSend)
    //     .create(MG_CONST(addr::features::MovementSend), hkMovementSend);
    //
    // registry.registerDetour(HookId::MovementReceive)
    //     .create(MG_CONST(addr::features::MovementReceive), hkMovementReceive);

    (void)ctx_;
    (void)&hkMovementSend;
    (void)&hkMovementReceive;
    return VoidResult::ok();
}

} // namespace mg::game
