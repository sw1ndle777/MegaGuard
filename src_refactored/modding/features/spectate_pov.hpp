// =============================================================================
// SpectatePov - Third-person spectator camera hook
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::modding {

// ── Forward declarations ──────────────────────────────────────────────────────
struct CCameraTPS;

// ── Math constants ────────────────────────────────────────────────────────────
constexpr float TWO_PI  = 6.283185482f;
constexpr float PI      = 3.141592741f;
constexpr float HALF_PI = 1.570796371f;
constexpr float PITCH_50 = 50.0f;
constexpr double DIFFERENCE_IDK = 0.2000000029802322;

// ── Enums ─────────────────────────────────────────────────────────────────────

enum ControlMode : u32
{
    ControlMode_FREE    = 0,
    ControlMode_TARGET  = 1,
    ControlMode_GAME    = 2,
    ControlMode_WATCH   = 3,
    ControlMode_UNKNOWN = 4
};

enum ViewMode : u32
{
    ViewMode_Normal              = 0x0,
    ViewMode_Shoulder            = 0x1,
    ViewMode_ZoomIn              = 0x2,
    ViewMode_DoubleZoomIn        = 0x3,
    ViewMode_FirstPerson         = 0x4,
    ViewMode_WATCH_CAMERA_VIEW   = 0x6,
    ViewMode_SELF_CAMERA_VIEW    = 0x7,
    ViewMode_ZOMBIE_KING_VIEW    = 0x9,
    ViewMode_ZOMBIE_SPEEDUP_VIEW = 0xA,
};

// ── Crouch animation IDs ──────────────────────────────────────────────────────
constexpr u32 kNormalCrouchAnimId   = 24;
constexpr u32 kRightCrouchAnimId    = 9;
constexpr u32 kLeftCrouchAnimId     = 10;
constexpr u32 kForwardCrouchAnimId  = 11;
constexpr u32 kBackwardCrouchAnimId = 12;

// ── Game structures (match binary layout) ─────────────────────────────────────

struct NiAVObject
{
    u8    gap0[0x20];
    float m_kWorldBound[3];       // 0x20
    float m_fRadius;              // 0x2C
    float m_localRotate[9];       // 0x30
    float m_localTranslate[3];    // 0x54
    float m_fLocalScale;          // 0x60
    float m_worldRotate[9];       // 0x64
    float m_worldTranslate[3];    // 0x88
    float m_worldScale;           // 0x94
    u8    gap98[0x4];
    DWORD* pdword9C;
    u8    gapA0[0x4];
    DWORD dwordA4;
    void* m_spCollisionObject;    // 0xA8
};

struct Matrix
{
    char  pad_0x0000[32];
    float WorldPos[3];            // 0x20
    char  pad_0x002C[56];
    float matrix_13;              // 0x64 FORWARD X
    float matrix_12;              // 0x68 UP Y
    float matrix_11;              // 0x6C
    float matrix_23;              // 0x70 FORWARD Y
    float matrix_22;              // 0x74 UP X
    float matrix_21;              // 0x78
    float matrix_33;              // 0x7C FORWARD Z
    float matrix_32;              // 0x80 UP Z
    float matrix_31;              // 0x84
    float WorldPos2[3];
    char  pad_0x0094[24];
    float matrix1[4];
    float matrix2[4];
    float matrix3[4];
    float matrix4[4];
    char  pad_0x00EC[16];
    float Realnear;
    float Realfar;
    char  pad_0x0104[12];
    float Viewport[4];
};

struct MovementInfo
{
    u8    gap0[16];
    float coords[3];
};

struct CNetIO
{
    u8   gap0[48];
    DWORD ping;
};

struct PlayerRoomInfo
{
    DWORD sid;
    DWORD Team;
    DWORD RoomStatus;
    DWORD HP1;
    DWORD notHP2;
    DWORD HP2;
    u8    idk1[0x10c];
    DWORD MeleeRange;             // 0x124
    u8    _idk2[0x9FC];
    DWORD HP_MAX1;                // 0xB24
    DWORD notHP_MAX2;             // 0xB28
    DWORD HP_MAX2;                // 0xB2C
    float Yaw;                    // 0xB30
    float Pitch;                  // 0xB34
};

struct CExPlayer
{
    DWORD*          VirtualFunctions;
    PlayerRoomInfo* room_info;
    DWORD           unk1;
    CNetIO*         CNetIO;
    MovementInfo*   MovementInfo;
};

struct CUnitRootNode
{
    u8    gap0[0x88];
    float root_pos[3];
};

class CBone
{
public:
    int8_t _empty[0x88];
    float  position[3];
    float* GetTransform(int index)
    {
        return reinterpret_cast<float*>(reinterpret_cast<DWORD*>(&this->position) + (0x37 * index));
    }
};

struct UnitMoveState
{
    u32 virtual_functions;
    u32 idk;
    u32 anim_id;
};

struct CUnitMoveStateMgr
{
    u8             idk1[0x18];
    UnitMoveState* UnitMoveState;
};

struct PlayerActionState
{
    u32 virtual_functions;
    u32 anim_id;
};

struct CPlayerActionStateMgr
{
    u8                 idk1[0xC];
    PlayerActionState* PlayerActionState;
};

struct CPlayerModelProperty;
struct CGamePlayerLocal
{
    void*                    VirtualFunctions;
    u8                       gap4[264];
    CExPlayer*               CExPlayer;           // 0x10C
    u8                       gap110[0x14];
    CPlayerModelProperty*    CPlayerModelProperty; // 0x124
    CUnitMoveStateMgr*       CUnitMoveStateMgr;   // 0x128
    CPlayerActionStateMgr*   CPlayerActionStateMgr;// 0x12C
};

struct CPlayerModelProperty
{
    u8                gap0[0x98];
    u32               character_bone_type;  // 0x98
    u8                gap9c[0x8];
    float             Rotation;             // 0xA4
    float             Pitch;                // 0xA8
    float             idk_ac;               // 0xAC
    float             idk_b0;               // 0xB0
    float             idk_b4;
    float             idk_b8;
    float             idk_bc;
    float             idk_c0;
    float             idk_c4;               // 0xC4
    float             idk_c8;               // 0xC8
    u8                gapcc[0xA4];
    CBone*            HumanBones;           // 0x170
    CBone*            ZombieBones;          // 0x174
    CBone*            ZombieKingBones;      // 0x178
    u8                gap17c[0x290];
    CUnitRootNode*    CUnitRootNode;        // 0x40C
    u8                gap410[0x40];
    CGamePlayerLocal* CGamePlayer;          // 0x450
};

struct CCameraTPS
{
    u8                gap0[4];
    float             float4;              // 0x04
    float             float8;              // 0x08
    float             RotationX;           // 0x0C
    float             RotationY;           // 0x10
    DWORD             m_eControlMode;      // 0x14
    u8                gap18[16];
    float             CameraPitch;         // 0x28
    float             CameraYaw;           // 0x2C
    float             float30;             // 0x30
    u8                gap34[64];
    float             float74;             // 0x74
    float             float78;             // 0x78
    float             float7C;             // 0x7C
    float             WorldPos[3];         // 0x80
    float             float8C;             // 0x8C
    float             float90;             // 0x90
    u8                gap94[36];
    Matrix*           Matrix;              // 0xB8
    NiAVObject*       NiAVObject;          // 0xBC
    u8                gapC0[20];
    DWORD             m_eViewMode;         // 0xD4
    u8                gapD8[60];
    float             float114;            // 0x114
    u8                gap118[8];
    float             ViewOffsetByPitch;   // 0x120
    u8                IsCrouch;            // 0x124
    u8                IsCrouchAnimActive;  // 0x125
    u8                IsCrouchIdk1;        // 0x126
    u8                IsCrouchIdk2;        // 0x127
    float             CurrHeight;          // 0x128
    float             FinalCurrHeight;     // 0x12C
    u8                gap130[24];
    u8                byte148;             // 0x148
    u8                byte149;             // 0x149
    float             m_vBonePos[3];       // 0x14C
    u8                gap158[756];
    CGamePlayerLocal* CGamePlayer;         // 0x44C
    u8                gap450[76];
    float             idkk[100];           // 0x49C
};

// ── Color ─────────────────────────────────────────────────────────────────────

struct Color
{
    u8 r, g, b, a;

    Color(float red, float green, float blue, float alpha = 1.0f)
    {
        auto conv = [](float c) -> u8 {
            return (c < 1.0f) ?
                ((c > 0.0f) ? static_cast<u8>(c * 255.0 + 0.5) : 0) : 255;
        };
        r = conv(red);
        g = conv(green);
        b = conv(blue);
        a = conv(alpha);
    }

    u32 toARGB() const
    {
        return (static_cast<u32>(a) << 24) |
               (static_cast<u32>(r) << 16) |
               (static_cast<u32>(g) << 8) |
                static_cast<u32>(b);
    }
};

// ── SpectatePov class ─────────────────────────────────────────────────────────

class SpectatePov {
public:
    explicit SpectatePov(::mg::MegaGuardContext& ctx);
    ~SpectatePov();

    VoidResult install();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::modding
