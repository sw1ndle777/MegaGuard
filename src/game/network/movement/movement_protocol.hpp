// =============================================================================
// MovementProtocol — 1:1 reimplementation of the client movement net code
// =============================================================================
// Replaces two engine functions with faithful ports so we can later experiment:
//   * MovementSend   (0x00B20440, SEND_USER.cpp) — client -> server movement
//   * MovementReceive(0x00AE1E10, OTHER.cpp)     — OTHER_MOVE receive handler
//
// The wire layout below is mirrored from the MegaVoltsPP server
// (common/NetEngine/Packets/PacketData.h). It is documentation/reference for
// the packet bytes the engine code builds/parses; the implementation in the
// .cpp reproduces the exact engine bit operations against the raw buffer so the
// produced/consumed bytes are byte-identical to the original.
// =============================================================================
#pragma once

#include "core/types.hpp"
#include "core/fwd.hpp"

namespace mg::game {

// ── Wire format (matches MegaVoltsPP PacketData.h, #pragma pack(1)) ────────────
#pragma pack(push, 1)

struct PositionStruct  { u16 x, y, z; };        // half-floats
struct DirectionStruct { u16 x, y, z; };        // half-floats
struct BulletsStruct   { u16 bullet1, bullet2, bullet3, bullet4; };
struct JumpStruct      { u16 jump1, jump2; };

// Client -> Server. The trailing bitfield packs anim/weapon/rotation.
// (animation1 is 7 bits on this client; 8 bits only on RELEASE_1_0_3.)
struct ClientPlayerInfoBasic {
    PositionStruct  position;       // +0
    DirectionStruct direction;      // +6
    u32             matchTick;      // +12
    u32             bits;           // +16  animation1:7|animation2:6|weapon:4|rotation:9|unknown:6
};
struct ClientPlayerInfoJump    { ClientPlayerInfoBasic player; JumpStruct   jump;    };
struct ClientPlayerInfoBullet  { ClientPlayerInfoBasic player; BulletsStruct bullets; };
struct ClientPlayerInfoComplete{ ClientPlayerInfoBasic player; BulletsStruct bullets; JumpStruct jump; };

// Server -> Client (rebroadcast). Carries SpecificInfo header.
struct SpecificInfo {
    u32 sessionId      : 14;
    u32 enableMovement : 1;   // >>14
    u32 enableBullet   : 1;   // >>15
    u32 animation1     : 7;   // >>16
    u32 enableRotation : 1;   // >>23
    u32 animation2     : 6;   // >>24
    u32 unknown        : 1;   // >>30
    u32 enableJump     : 1;   // >>31
};

#pragma pack(pop)

// ── Feature ───────────────────────────────────────────────────────────────────
class MovementProtocol {
public:
    explicit MovementProtocol(::mg::MegaGuardContext& ctx);
    ~MovementProtocol();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::game
