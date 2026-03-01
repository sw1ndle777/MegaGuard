// =============================================================================
// SpectatePov - Full implementation ported from spectate_pov.h (~895 lines)
// =============================================================================
// Complete TPS camera override with:
//  - Crouch detection (anim IDs 9,10,11,12,24)
//  - Bone position resolution (Human/Zombie/ZombieKing by bone type)
//  - Yaw/pitch calculation from player model rotation + room info
//  - NiMatrix3 rotation matrix computation
//  - Camera offset via sub_465A40
//  - Collision check via sub_5797B0
//  - NiAVObject transform update
//  - FPS RenderText overlay
//  Two major branches: WATCH mode (spectating) vs GAME mode (normal play)
// =============================================================================
#include "pch.hpp"
#include "modding/features/spectate_pov.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"

#include <cmath>

namespace mg::modding {

using namespace mg::game;

namespace {

// ── Normalize helpers ─────────────────────────────────────────────────────

void normalizeRadians(float* radian)
{
    *radian = std::fmod(*radian, TWO_PI);
    if (*radian < 0.0f) *radian += TWO_PI;
}

void clamp_yaw(float* yaw)
{
    while (*yaw < 0.0f)    *yaw += TWO_PI;
    while (*yaw > TWO_PI)  *yaw -= TWO_PI;
}

// ── Engine function pointer types ─────────────────────────────────────────────

using Sub57B550Func = void(__thiscall*)(CCameraTPS*);
using Sub579690Func = void(__thiscall*)(CCameraTPS*, float*, float*, float);
using Sub5778F0Func = void(__thiscall*)(CCameraTPS*, float);
using Sub577D90Func = void(__thiscall*)(CCameraTPS*);
using Sub4585C0Func = void(__cdecl*)(float*);
using Sub578030Func = void(__thiscall*)(CCameraTPS*);
using Sub577EE0Func = void(__thiscall*)(CCameraTPS*);
using NiMatrix3FromEulerAnglesZYXFunc = void(__thiscall*)(float*, float, float, float);
using Sub465A40Func = float* (__thiscall*)(float*, float*, float*);
using Sub5797B0Func = void(__thiscall*)(CCameraTPS*, float*, float*, float*);
using NiAVObjectUpdateFunc = void(__thiscall*)(NiAVObject*, float, char);
using Sub4586C0Func = float(__cdecl*)(float, float);
using Sub56F5C0Func = void(__thiscall*)(CCameraTPS*, float);
using Sub430760Func = int(__thiscall*)(char*, u32, u32, u32, const char*, u32);

// ── Resolve engine functions via MG_CONST ─────────────────────────────────────

Sub57B550Func                  sub_57B550;
Sub579690Func                  sub_579690;
Sub5778F0Func                  sub_5778F0;
Sub577D90Func                  sub_577D90;
Sub4585C0Func                  NormalizeRadian;
Sub578030Func                  sub_578030;
Sub577EE0Func                  sub_577EE0;
NiMatrix3FromEulerAnglesZYXFunc NiMatrix3FromEulerAnglesZYX;
Sub465A40Func                  sub_465A40;
Sub5797B0Func                  sub_5797B0;
NiAVObjectUpdateFunc           NiAVObjectUpdate;
Sub4586C0Func                  sub_4586C0;
Sub56F5C0Func                  sub_56F5C0;
Sub430760Func                  sub_430760;

// ── Per-frame state (NOT in data section — stored via context accessor) ──────
// These are camera state that must persist across frames but are confined
// to this TU and accessed only from the hook callback on the game thread.
u32  old_move_state_anim = 0;
bool reset_crouch = false;

// ── RenderText ────────────────────────────────────────────────────────────────

int RenderText(u32 x, u32 y, Color color, const char* text, u32 len)
{
    return sub_430760(reinterpret_cast<char*>(MG_CONST(0x011C9C58)), x, y, color.toARGB(), text, len);
}

// ── Main spectate hook ───────────────────────────────────────────────────────

void __fastcall hkSpectate(CCameraTPS* cam, u32 /*edx*/, float idk)
{
    // FPS overlay
    auto fps = reinterpret_cast<u32*>(MG_CONST(0x11D81F0));
    auto my_fps = fps ? *fps : 0u;
    u32 y_pos = 10;

    char fpsBuf[32];
    int fpsLen = sprintf_s(fpsBuf, "FPS: %u", my_fps);
    RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fpsBuf, fpsLen);
    y_pos += 20;

    // ── Only handle GAME or WATCH control modes ──────────────────────────
    if (cam->m_eControlMode == ControlMode_GAME ||
        cam->m_eControlMode == ControlMode_WATCH)
    {
        // ═══════════════════════════════════════════════════════════════════
        // BRANCH 1: WATCH + WATCH_CAMERA_VIEW (spectating another player)
        // ═══════════════════════════════════════════════════════════════════
        if (cam->m_eControlMode == ControlMode_WATCH &&
            cam->m_eViewMode == ViewMode_WATCH_CAMERA_VIEW)
        {
            sub_5778F0(cam, idk);

            if (cam->CGamePlayer->CPlayerModelProperty)
            {
                float tmp2PrevBonePos[3] = {0, 0, 0};

                if (cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode)
                {
                    cam->m_vBonePos[0] = cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[0];
                    cam->m_vBonePos[1] = cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[1];
                    cam->m_vBonePos[2] = cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[2];
                }

                cam->m_vBonePos[2] += cam->CGamePlayer->CPlayerModelProperty->idk_c8;
                tmp2PrevBonePos[0] = cam->m_vBonePos[0];
                tmp2PrevBonePos[1] = cam->m_vBonePos[1];
                tmp2PrevBonePos[2] = cam->m_vBonePos[2];

                // Crouch detection for spectated player
                if (old_move_state_anim != cam->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id)
                {
                    auto anim = cam->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id;
                    if (anim == kNormalCrouchAnimId || anim == kRightCrouchAnimId ||
                        anim == kLeftCrouchAnimId || anim == kForwardCrouchAnimId ||
                        anim == kBackwardCrouchAnimId)
                    {
                        cam->IsCrouch = 1;
                        cam->IsCrouchAnimActive = 1;
                        cam->FinalCurrHeight = 30.0f;
                        reset_crouch = true;
                    }
                    else if (old_move_state_anim == kNormalCrouchAnimId ||
                             old_move_state_anim == kRightCrouchAnimId ||
                             old_move_state_anim == kLeftCrouchAnimId ||
                             old_move_state_anim == kForwardCrouchAnimId ||
                             old_move_state_anim == kBackwardCrouchAnimId)
                    {
                        cam->IsCrouch = 0;
                        cam->IsCrouchAnimActive = 1;
                        cam->FinalCurrHeight = 90.0f;
                    }
                    old_move_state_anim = cam->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id;
                }

                sub_577D90(cam); // crouch animation processing
                cam->m_vBonePos[2] += cam->CurrHeight;
                tmp2PrevBonePos[2] += cam->CurrHeight;

                // Calculate yaw and pitch from player model
                auto my_yaw = cam->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Yaw
                            + cam->CGamePlayer->CPlayerModelProperty->Rotation;
                auto my_pitch = cam->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Pitch
                              + cam->CGamePlayer->CPlayerModelProperty->Pitch
                              + cam->CGamePlayer->CPlayerModelProperty->idk_c4
                              + cam->CGamePlayer->CPlayerModelProperty->idk_ac;

                NormalizeRadian(&my_yaw);
                clamp_yaw(&my_yaw);
                NormalizeRadian(&my_pitch);

                sub_578030(cam);
                sub_577EE0(cam);

                float v3 = 0.0f;
                if (my_pitch < (PI + HALF_PI))
                    v3 = -my_pitch * PITCH_50;
                else
                    v3 = (TWO_PI - my_pitch) * PITCH_50;
                cam->ViewOffsetByPitch = v3;

                // Normalize forward vector
                auto matrix = cam->Matrix;
                auto forward_x = &matrix->matrix_13;
                auto forward_y = &matrix->matrix_23;
                auto forward_z = &matrix->matrix_33;
                *forward_z = 0.0f;

                float length = sqrt((*forward_x) * (*forward_x) + (*forward_y) * (*forward_y) + (*forward_z) * (*forward_z));
                if (length > *reinterpret_cast<double*>(MG_CONST(0x00FEB350)))
                {
                    float invLength = 1.0f / length;
                    *forward_x *= invLength;
                    *forward_y *= invLength;
                    *forward_z *= invLength;
                }
                else
                {
                    *forward_x = 0.0f;
                    *forward_y = 0.0f;
                    *forward_z = 0.0f;
                }

                float v40[9] = {0};
                float v49[9] = {0};
                float v50[9] = {0};

                auto matrix2 = cam->Matrix;
                auto right_x = matrix2->matrix_11 * cam->float114;
                auto right_y = matrix2->matrix_21 * cam->float114;
                auto right_z = matrix2->matrix_31 * cam->float114;

                v40[6] = right_x;
                v40[7] = right_y;
                v40[8] = right_z;
                auto v22 = right_x + cam->m_vBonePos[0];
                auto v23 = right_y + cam->m_vBonePos[1];
                auto v24 = right_z + cam->m_vBonePos[2];
                v40[3] = v22;
                v40[4] = v23;
                v40[5] = v24;
                tmp2PrevBonePos[0] = v22;
                tmp2PrevBonePos[1] = v23;
                tmp2PrevBonePos[2] = v24;

                NiMatrix3FromEulerAnglesZYX(v49, my_yaw, my_pitch, 0.0f);
                v40[0] = -cam->idkk[0];
                v40[1] = 0.0f;
                v40[2] = 0.0f;

                float v39[3] = {0};
                auto v18 = sub_465A40(v49, v39, v40);
                cam->WorldPos[0] = tmp2PrevBonePos[0] + v18[0];
                cam->WorldPos[1] = tmp2PrevBonePos[1] + v18[1];
                cam->WorldPos[2] = tmp2PrevBonePos[2] + v18[2];

                float previous_world_pos[3] = {cam->WorldPos[0], cam->WorldPos[1], cam->WorldPos[2]};

                if (my_pitch < (PI + HALF_PI))
                    my_pitch = my_pitch * static_cast<float>(DIFFERENCE_IDK) + my_pitch;
                else
                    my_pitch = (TWO_PI - my_pitch) * static_cast<float>(DIFFERENCE_IDK) + my_pitch;

                NiMatrix3FromEulerAnglesZYX(v50, my_yaw, my_pitch, 0.0f);

                if (cam->byte148)
                    sub_5797B0(cam, cam->m_vBonePos, tmp2PrevBonePos, previous_world_pos);

                sub_579690(cam, v50, previous_world_pos, 0.0f);
                cam->float74 = previous_world_pos[0];
                cam->float78 = previous_world_pos[1];
                cam->float7C = previous_world_pos[2];
                cam->NiAVObject->m_localTranslate[0] = previous_world_pos[0];
                cam->NiAVObject->m_localTranslate[1] = previous_world_pos[1];
                cam->NiAVObject->m_localTranslate[2] = previous_world_pos[2];
                cam->NiAVObject->m_localRotate[0] = v50[0];
                cam->NiAVObject->m_localRotate[1] = v50[1];
                cam->NiAVObject->m_localRotate[2] = v50[2];
                cam->NiAVObject->m_localRotate[3] = v50[3];
                cam->NiAVObject->m_localRotate[4] = v50[4];
                cam->NiAVObject->m_localRotate[5] = v50[5];
                cam->NiAVObject->m_localRotate[6] = v50[6];
                cam->NiAVObject->m_localRotate[7] = v50[7];
                cam->NiAVObject->m_localRotate[8] = v50[8];

                NiAVObjectUpdate(cam->NiAVObject, 0.0f, 1);
                sub_57B550(cam);
                cam->float4 = sub_4586C0(my_yaw, cam->RotationX);
                cam->float8 = sub_4586C0(my_pitch, cam->RotationY);
                cam->RotationX = my_yaw;
                cam->RotationY = my_pitch;
            }
        }
        // ═══════════════════════════════════════════════════════════════════
        // BRANCH 2: Normal play mode (ControlMode_GAME or other WATCH modes)
        // ═══════════════════════════════════════════════════════════════════
        else
        {
            auto dword_11DE160 = reinterpret_cast<DWORD*>(MG_CONST(0x011DE160));
            auto flt_11DE13C   = reinterpret_cast<float*>(MG_CONST(0x011DE13C));

            if ((*dword_11DE160 & 1) == 0)
                *dword_11DE160 |= 1u;

            flt_11DE13C[0] = 1.0f; flt_11DE13C[1] = 0.0f; flt_11DE13C[2] = 0.0f;
            flt_11DE13C[3] = 0.0f; flt_11DE13C[4] = 1.0f; flt_11DE13C[5] = 0.0f;
            flt_11DE13C[6] = 0.0f; flt_11DE13C[7] = 0.0f; flt_11DE13C[8] = 1.0f;

            if (cam->byte149)
            {
                // Direct coordinate follow mode
                auto my_coords = cam->CGamePlayer->CExPlayer->MovementInfo->coords;
                float idk2[9] = {0};
                sub_579690(cam, idk2, my_coords, 0.0f);
                cam->NiAVObject->m_localTranslate[0] = my_coords[0];
                cam->NiAVObject->m_localTranslate[1] = my_coords[1];
                cam->NiAVObject->m_localTranslate[2] = my_coords[2];
                cam->NiAVObject->m_localRotate[0] = idk2[0];
                cam->NiAVObject->m_localRotate[1] = idk2[1];
                cam->NiAVObject->m_localRotate[2] = idk2[2];
                cam->NiAVObject->m_localRotate[3] = idk2[3];
                cam->NiAVObject->m_localRotate[4] = idk2[4];
                cam->NiAVObject->m_localRotate[5] = idk2[5];
                cam->NiAVObject->m_localRotate[6] = idk2[6];
                cam->NiAVObject->m_localRotate[7] = idk2[7];
                cam->NiAVObject->m_localRotate[8] = idk2[8];
                NiAVObjectUpdate(cam->NiAVObject, 0.0f, 1);
                sub_57B550(cam);
            }
            else
            {
                sub_5778F0(cam, idk);

                if (cam->CGamePlayer->CPlayerModelProperty)
                {
                    float tmp2PrevBonePos[3] = {0, 0, 0};

                    // ── First person: resolve head bone ──────────────────
                    if (cam->m_eViewMode == ViewMode_FirstPerson)
                    {
                        auto character_bone_type = cam->CGamePlayer->CPlayerModelProperty->character_bone_type;
                        float bone_pos[3] = {0, 0, 0};

                        if (character_bone_type == 0 && cam->CGamePlayer->CPlayerModelProperty->HumanBones)
                        {
                            bone_pos[0] = cam->CGamePlayer->CPlayerModelProperty->HumanBones->position[0];
                            bone_pos[1] = cam->CGamePlayer->CPlayerModelProperty->HumanBones->position[1];
                            bone_pos[2] = cam->CGamePlayer->CPlayerModelProperty->HumanBones->position[2];
                        }
                        else if (character_bone_type == 1 && cam->CGamePlayer->CPlayerModelProperty->ZombieBones)
                        {
                            bone_pos[0] = cam->CGamePlayer->CPlayerModelProperty->ZombieBones->position[0];
                            bone_pos[1] = cam->CGamePlayer->CPlayerModelProperty->ZombieBones->position[1];
                            bone_pos[2] = cam->CGamePlayer->CPlayerModelProperty->ZombieBones->position[2];
                        }
                        else if (character_bone_type == 2 && cam->CGamePlayer->CPlayerModelProperty->ZombieKingBones)
                        {
                            bone_pos[0] = cam->CGamePlayer->CPlayerModelProperty->ZombieKingBones->position[0];
                            bone_pos[1] = cam->CGamePlayer->CPlayerModelProperty->ZombieKingBones->position[1];
                            bone_pos[2] = cam->CGamePlayer->CPlayerModelProperty->ZombieKingBones->position[2];
                        }
                        else
                        {
                            auto bone_info = reinterpret_cast<float*>(MG_CONST(0x011ED050));
                            bone_pos[0] = bone_info[0];
                            bone_pos[1] = bone_info[1];
                            bone_pos[2] = bone_info[2];
                        }

                        cam->m_vBonePos[0] = bone_pos[0];
                        cam->m_vBonePos[1] = bone_pos[1];
                        cam->m_vBonePos[2] = bone_pos[2];
                        tmp2PrevBonePos[0] = cam->m_vBonePos[0];
                        tmp2PrevBonePos[1] = cam->m_vBonePos[1];
                        tmp2PrevBonePos[2] = cam->m_vBonePos[2];
                    }
                    // ── Third person: use root node + offset ─────────────
                    else
                    {
                        if (cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode)
                        {
                            cam->m_vBonePos[0] = cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[0];
                            cam->m_vBonePos[1] = cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[1];
                            cam->m_vBonePos[2] = cam->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[2];
                        }

                        cam->m_vBonePos[2] += cam->CGamePlayer->CPlayerModelProperty->idk_c8;
                        tmp2PrevBonePos[0] = cam->m_vBonePos[0];
                        tmp2PrevBonePos[1] = cam->m_vBonePos[1];
                        tmp2PrevBonePos[2] = cam->m_vBonePos[2];
                    }

                    // Reset crouch when transitioning from watch to game
                    if (reset_crouch)
                    {
                        cam->IsCrouch = 0;
                        cam->IsCrouchAnimActive = 0;
                        cam->FinalCurrHeight = 90.0f;
                        cam->CurrHeight = 90.0f;
                        reset_crouch = false;
                    }

                    sub_577D90(cam);
                    cam->m_vBonePos[2] += cam->CurrHeight;
                    tmp2PrevBonePos[2] += cam->CurrHeight;

                    auto my_yaw = cam->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Yaw
                                + cam->CGamePlayer->CPlayerModelProperty->Rotation;
                    auto my_pitch = cam->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Pitch
                                  + cam->CGamePlayer->CPlayerModelProperty->Pitch
                                  + cam->CGamePlayer->CPlayerModelProperty->idk_c4
                                  + cam->CGamePlayer->CPlayerModelProperty->idk_ac;

                    NormalizeRadian(&my_yaw);
                    NormalizeRadian(&my_pitch);

                    if (cam->m_eControlMode == ControlMode_WATCH)
                    {
                        my_yaw = 0.0f;
                        my_pitch = 0.0f;
                    }
                    if (cam->m_eViewMode == ViewMode_SELF_CAMERA_VIEW)
                        my_pitch = 0.0f;

                    sub_578030(cam);
                    sub_577EE0(cam);

                    float v3 = 0.0f;
                    if (my_pitch < (PI + HALF_PI))
                        v3 = -my_pitch * PITCH_50;
                    else
                        v3 = (TWO_PI - my_pitch) * PITCH_50;
                    cam->ViewOffsetByPitch = v3;

                    auto matrix = cam->Matrix;
                    auto forward_x = &matrix->matrix_13;
                    auto forward_y = &matrix->matrix_23;
                    auto forward_z = &matrix->matrix_33;
                    *forward_z = 0.0f;

                    float length = sqrt((*forward_x) * (*forward_x) + (*forward_y) * (*forward_y) + (*forward_z) * (*forward_z));
                    if (length > *reinterpret_cast<double*>(MG_CONST(0x00FEB350)))
                    {
                        float invLength = 1.0f / length;
                        *forward_x *= invLength;
                        *forward_y *= invLength;
                        *forward_z *= invLength;
                    }
                    else
                    {
                        *forward_x = 0.0f;
                        *forward_y = 0.0f;
                        *forward_z = 0.0f;
                    }

                    float v40[9] = {0};
                    float v49[9] = {0};
                    float v50[9] = {0};

                    if (cam->m_eViewMode != ViewMode_ZoomIn || cam->m_eViewMode != ViewMode_DoubleZoomIn)
                    {
                        auto matrix2 = cam->Matrix;
                        auto v25 = cam->float114;
                        auto v26 = matrix2->matrix_11 * v25;
                        auto v27 = matrix2->matrix_21 * v25;
                        auto v28 = matrix2->matrix_31 * v25;
                        v40[6] = v26;
                        v40[7] = v27;
                        v40[8] = v28;
                        auto v22 = v26 + cam->m_vBonePos[0];
                        auto v23 = v27 + cam->m_vBonePos[1];
                        auto v24 = v28 + cam->m_vBonePos[2];
                        v40[3] = v22;
                        v40[4] = v23;
                        v40[5] = v24;
                        tmp2PrevBonePos[0] = v22;
                        tmp2PrevBonePos[1] = v23;
                        tmp2PrevBonePos[2] = v24;
                    }

                    auto v14 = my_pitch + cam->CameraPitch;
                    auto v13 = my_yaw + cam->CameraYaw;
                    NiMatrix3FromEulerAnglesZYX(v49, v13, v14, 0.0f);
                    v40[0] = -cam->idkk[15 * cam->m_eViewMode];
                    v40[1] = 0.0f;
                    v40[2] = 0.0f;

                    float v39[3] = {0};
                    auto v18 = sub_465A40(v49, v39, v40);
                    cam->WorldPos[0] = tmp2PrevBonePos[0] + v18[0];
                    cam->WorldPos[1] = tmp2PrevBonePos[1] + v18[1];
                    cam->WorldPos[2] = tmp2PrevBonePos[2] + v18[2];

                    float previous_world_pos[3] = {cam->WorldPos[0], cam->WorldPos[1], cam->WorldPos[2]};

                    if (my_pitch < (PI + HALF_PI))
                        my_pitch = my_pitch * static_cast<float>(DIFFERENCE_IDK) + my_pitch;
                    else
                        my_pitch = (TWO_PI - my_pitch) * static_cast<float>(DIFFERENCE_IDK) + my_pitch;

                    // Compute final rotation matrix based on control mode
                    if (cam->m_eControlMode == ControlMode_GAME)
                    {
                        if (cam->m_eViewMode == ViewMode_WATCH_CAMERA_VIEW ||
                            cam->m_eViewMode == ViewMode_SELF_CAMERA_VIEW)
                        {
                            auto v11 = my_pitch + cam->float90 + cam->CameraPitch;
                            auto v10 = my_yaw + cam->float8C + cam->CameraYaw;
                            NiMatrix3FromEulerAnglesZYX(v50, v10, v11, cam->float30);
                        }
                        else
                        {
                            auto v9 = my_pitch + cam->CameraPitch;
                            auto v8 = my_yaw + cam->CameraYaw;
                            NiMatrix3FromEulerAnglesZYX(v50, v8, v9, cam->float30);
                        }
                    }
                    else
                    {
                        auto v7 = my_pitch + cam->float90 + cam->CameraPitch;
                        auto v6 = my_yaw + cam->float8C + cam->CameraYaw;
                        NiMatrix3FromEulerAnglesZYX(v50, v6, v7, cam->float30);
                    }

                    if (cam->byte148)
                        sub_5797B0(cam, cam->m_vBonePos, tmp2PrevBonePos, previous_world_pos);

                    sub_579690(cam, v50, previous_world_pos, 0.0f);
                    cam->float74 = previous_world_pos[0];
                    cam->float78 = previous_world_pos[1];
                    cam->float7C = previous_world_pos[2];
                    cam->NiAVObject->m_localTranslate[0] = previous_world_pos[0];
                    cam->NiAVObject->m_localTranslate[1] = previous_world_pos[1];
                    cam->NiAVObject->m_localTranslate[2] = previous_world_pos[2];
                    cam->NiAVObject->m_localRotate[0] = v50[0];
                    cam->NiAVObject->m_localRotate[1] = v50[1];
                    cam->NiAVObject->m_localRotate[2] = v50[2];
                    cam->NiAVObject->m_localRotate[3] = v50[3];
                    cam->NiAVObject->m_localRotate[4] = v50[4];
                    cam->NiAVObject->m_localRotate[5] = v50[5];
                    cam->NiAVObject->m_localRotate[6] = v50[6];
                    cam->NiAVObject->m_localRotate[7] = v50[7];
                    cam->NiAVObject->m_localRotate[8] = v50[8];

                    NiAVObjectUpdate(cam->NiAVObject, 0.0f, 1);
                    sub_57B550(cam);
                    cam->float4 = sub_4586C0(my_yaw, cam->RotationX);
                    cam->float8 = sub_4586C0(my_pitch, cam->RotationY);
                    cam->RotationX = my_yaw;
                    cam->RotationY = my_pitch;
                }
            }
        }
    }
    // ═══════════════════════════════════════════════════════════════════════
    // BRANCH 3: Other control modes (FREE, TARGET, UNKNOWN)
    // ═══════════════════════════════════════════════════════════════════════
    else
    {
        sub_56F5C0(cam, idk);
        sub_57B550(cam);
    }
}
} // anonymous namespace
// ── SpectatePov class ─────────────────────────────────────────────────────────

SpectatePov::SpectatePov(MegaGuardContext& ctx) : ctx_(ctx) {}
SpectatePov::~SpectatePov() = default;

VoidResult SpectatePov::install()
{
    // Resolve all engine function pointers via MG_CONST
    sub_57B550                  = reinterpret_cast<Sub57B550Func>(MG_CONST(0x0057B550));
    sub_579690                  = reinterpret_cast<Sub579690Func>(MG_CONST(0x00579690));
    sub_5778F0                  = reinterpret_cast<Sub5778F0Func>(MG_CONST(0x005778F0));
    sub_577D90                  = reinterpret_cast<Sub577D90Func>(MG_CONST(0x00577D90));
    NormalizeRadian             = reinterpret_cast<Sub4585C0Func>(MG_CONST(0x004585C0));
    sub_578030                  = reinterpret_cast<Sub578030Func>(MG_CONST(0x00578030));
    sub_577EE0                  = reinterpret_cast<Sub577EE0Func>(MG_CONST(0x00577EE0));
    NiMatrix3FromEulerAnglesZYX = reinterpret_cast<NiMatrix3FromEulerAnglesZYXFunc>(MG_CONST(0x00CCA8E0));
    sub_465A40                  = reinterpret_cast<Sub465A40Func>(MG_CONST(0x00465A40));
    sub_5797B0                  = reinterpret_cast<Sub5797B0Func>(MG_CONST(0x005797B0));
    NiAVObjectUpdate            = reinterpret_cast<NiAVObjectUpdateFunc>(MG_CONST(0x00CC7660));
    sub_4586C0                  = reinterpret_cast<Sub4586C0Func>(MG_CONST(0x004586C0));
    sub_56F5C0                  = reinterpret_cast<Sub56F5C0Func>(MG_CONST(0x0056F5C0));
    sub_430760                  = reinterpret_cast<Sub430760Func>(MG_CONST(0x00430760));

    auto& registry = ctx_.hookRegistry();
    registry.registerDetour(HookId::SpectateCamera)
        .create(MG_CONST(addr::features::SpectateObserver), hkSpectate);

    return VoidResult::ok();
}

} // namespace mg::modding
