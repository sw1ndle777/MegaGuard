#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
        namespace Features
        {
            using namespace MegaGuard::Globals::Features;

            constexpr float TWO_PI = 6.283185482f;
            constexpr float PI = 3.141592741f;
            constexpr float HALF_PI = 1.570796371f;
            constexpr float DIFFERENCE_EPSILON = 0.001f;
            constexpr double DIFFERENCE_IDK = 0.2000000029802322f;
            constexpr double EPSILON = 9.999999974752427e-7;
            constexpr float PITCH_50 = 50.0f;




            void normalizeRadians(float* radian) 
            {
                *radian = std::fmod(*radian, TWO_PI);
                if (*radian < 0.0f) *radian += TWO_PI;
            }
            void clamp_yaw(float* yaw)
            {
                while (*yaw < 0.0f)
                    *yaw += TWO_PI;
                while (*yaw > TWO_PI)
                    *yaw -= TWO_PI;
            }

           


            enum ControlMode : std::uint32_t
            {
                ControlMode_FREE = 0,
                ControlMode_TARGET = 1,
                ControlMode_GAME = 2,
                ControlMode_WATCH = 3,
                ControlMode_UNKNOWN = 4
            };

            enum ViewMode
            {
                ViewMode_Normal = 0x0,
                ViewMode_Shoulder = 0x1,
                ViewMode_ZoomIn = 0x2,
                ViewMode_DoubleZoomIn = 0x3,
                ViewMode_FirstPerson = 0x4,
                ViewMode_WATCH_CAMERA_VIEW = 0x6,
                ViewMode_SELF_CAMERA_VIEW = 0x7,
                ViewMode_ZOMBIE_KING_VIEW = 0x9,
                ViewMode_ZOMBIE_SPEEDUP_VIEW = 0xA,
            };
            inline void* qmemcpy(void* dst, const void* src, size_t cnt)
            {
                char* out = (char*)dst;
                const char* in = (const char*)src;
                while (cnt > 0)
                {
                    *out++ = *in++;
                    --cnt;
                }
                return dst;
            }

            struct NiAVObject
            {
                std::uint8_t gap0[0x20];//0x0000
                float m_kWorldBound[3];//0x0020
                float m_fRadius;//0x002C
                float m_localRotate[9];//0x0030
                float m_localTranslate[3];//0x0054
                float m_fLocalScale;//0x0060
                float m_worldRotate[9];//0x0064
                float m_worldTranslate[3];//0x0088
                float m_worldScale;//0x0094
                std::uint8_t gap98[0x4];//0x0098
                DWORD* pdword9C;//0x009C
                std::uint8_t gapA0[0x4];//0x00A0
                DWORD dwordA4;//0x00A4
                void* m_spCollisionObject;//0x00A8
            };

            struct Matrix
            {
				char pad_0x0000[32];//0x0000
				float WorldPos[3];//0x0020
				char pad_0x002C[56];//0x002C
				float matrix_13;//0x0064 FORWARD X
				float matrix_12;//0x0068 UP Y
				float matrix_11;//0x006C
				float matrix_23;//0x0070 FORWARD Y
				float matrix_22;//0x0074 UP X
				float matrix_21;//0x0078
				float matrix_33;//0x007C FORWARD Z
				float matrix_32;//0x0080 UP Z
				float matrix_31;//0x0084
                float WorldPos2[3];
                char pad_0x0094[24];
                float matrix1[4];
                float matrix2[4];
                float matrix3[4];
                float matrix4[4];
                char pad_0x00EC[16];
                float Realnear;
                float Realfar;
                char pad_0x0104[12];
                float Viewport[4];
            };


            struct MovementInfo
            {
                std::uint8_t gap0[16];
                float coords[3];
            };

            struct CNetIO
            {
                std::uint8_t gap0[48];
                DWORD ping;
            };


            struct PlayerRoomInfo
            {
                DWORD sid;//0x0000
                DWORD Team;//0x0004
                DWORD RoomStatus;//0x0008
                DWORD HP1;//0x000C
                DWORD notHP2;//0x0010
                DWORD HP2;//0x0014
                std::uint8_t idk1[0x10c];//0x0018
                DWORD MeleeRange;//0x0124
                std::uint8_t _idk2[0x9FC];//0x0128
                DWORD HP_MAX1;//0x0B24
                DWORD notHP_MAX2;//0x0B28
                DWORD HP_MAX2;//0x0B2C
                float Yaw;//0x0B30
                float Pitch;//0x0B34
            };

            struct CGamePlayerVirtualFuncs
            {
                std::uint8_t gap0[36];
                int(__thiscall* pfunc24)(struct CGamePlayer*);
            };

            struct CExPlayer
            {
				DWORD* VirtualFunctions;//0x0000
				PlayerRoomInfo* room_info;//0x0004
				DWORD unk1;//0x0008
				CNetIO* CNetIO;//0x000C
				MovementInfo* MovementInfo;//0x0010
            };
            struct CUnitRootNode
            {
                std::uint8_t gap0[0x88];
                float root_pos[3];
            };
            class CBone
            {
            public:
                int8_t _empty[0x88];
                float position[3];
                float* GetTransform(int index)
                {
                    return (float*)(((DWORD*)&this->position) + (0x37 * index));
                }
            };
            struct UnitMoveState
            {
                std::uint32_t virtual_functions;
                std::uint32_t idk;
                std::uint32_t anim_id;
            };
            struct CUnitMoveStateMgr
            {
                std::uint8_t idk1[0x18];//0x0000
				UnitMoveState* UnitMoveState;//0x0018
            };

            struct PlayerActionState
            {
                std::uint32_t virtual_functions;
                std::uint32_t anim_id;
            };
            struct CPlayerActionStateMgr
            {
                std::uint8_t idk1[0xC];//0x0000
                PlayerActionState* PlayerActionState;//0x00C
            };

            struct CGamePlayer
            {
                CGamePlayerVirtualFuncs* VirtualFunctions;//0x0
                std::uint8_t gap4[264];//0x4
                CExPlayer* CExPlayer;//0x10c
                std::uint8_t gap110[0x14];//0x110
                struct CPlayerModelProperty* CPlayerModelProperty;//0x124
				CUnitMoveStateMgr* CUnitMoveStateMgr;//0x128
				CPlayerActionStateMgr* CPlayerActionStateMgr;//0x12C

            };

            struct CPlayerModelProperty
            {
                std::uint8_t idk1[0x98];//0x0000
                std::uint32_t character_bone_type;//0x98
                std::uint8_t idk2[0x8];//0x9c
                float Rotation;//0x00a4
                float Pitch;//0x00a8
                float idk_ac;//0x00ac
                //std::uint8_t idk3[0x14];//0x00b0
				float idk_b0;//0x00b0
				float idk_b4;//0x00b4
				float idk_b8;//0x00b8
				float idk_bc;//0x00bc
				float idk_c0;//0x00c0
                float idk_c4;//0x00c4
                float idk_c8;//0x00c8
                std::uint8_t idk6[0xA4];//0x00cc
                CBone* HumanBones;//0x0170
                CBone* ZombieBones;//0x0174
                CBone* ZombieKingBones;//0x0178
                std::uint8_t idk7[0x290];//0x17C
                CUnitRootNode* CUnitRootNode;//0x40c
                std::uint8_t idk8[0x40];//0x410
                struct CGamePlayer* CGamePlayer;//0x450
            };



            struct CCameraTPS
            {
                std::uint8_t gap0[4];//0x0000
                float float4;//0x0004
                float float8;//0x0008
                float RotationX;//0x000C
                float RotationY;//0x0010
                DWORD m_eControlMode;//0x0014
                std::uint8_t gap18[16];//0x0018
                float CameraPitch;//0x0028 Camera Pitch
                float CameraYaw;//0x002C Camera Yaw
                float float30;//0x0030
                std::uint8_t gap34[64];//0x0034
                float float74;//0x0074
                float float78;//0x0078
                float float7C;//0x007C
                float WorldPos[3];  //0x0080
                float float8C;//0x008C
                float float90;//0x0090
                std::uint8_t gap94[36];//0x0094
                Matrix* Matrix;//0x00B8
                NiAVObject* NiAVObject;//0x00BC
                std::uint8_t gapC0[20];//0x00C0
                DWORD m_eViewMode;//0x00D4
                std::uint8_t gapD8[60];//0x00D8
                float float114;//0x0114
                std::uint8_t gap118[8];//0x0118
                float ViewOffsetByPitch;//0x0120 ViewOffset by Pitch
				std::uint8_t IsCrouch;//0x0124
				std::uint8_t IsCrouchAnimActive;//0x0125
				std::uint8_t IsCrouchIdk1;//0x0126
				std::uint8_t IsCrouchIdk2;//0x0127
                float CurrHeight;//0x0128 CurrHeight
				float FinalCurrHeight;//0x012C FinalCurrHeight
                std::uint8_t gap12C[24];//0x0130
                std::uint8_t byte148;//0x0148   
                std::uint8_t byte149;//0x0149
                float m_vBonePos[3];//0x014C
                std::uint8_t gap158[756];//0x0158
                CGamePlayer* CGamePlayer;//0x044C
				std::uint8_t gap450[76];//0x0450
				float idkk[100];//0x049C
            };

            static auto sub_57B550_addy = enc_ptr(0x0057B550);
            static auto sub_579690_addy = enc_ptr(0x00579690);
            static auto sub_5778F0_addy = enc_ptr(0x005778F0);
            static auto sub_577D90_addy = enc_ptr(0x00577D90);
            static auto NormalizeRadian_addy = enc_ptr(0x004585C0);
            static auto sub_578030_addy = enc_ptr(0x00578030);
            static auto sub_577EE0_addy = enc_ptr(0x00577EE0);
            static auto NiMatrix3FromEulerAnglesZYX_addy = enc_ptr(0x00CCA8E0);
			static auto sub_465A40_addy = enc_ptr(0x00465A40);
			static auto sub_5797B0_addy = enc_ptr(0x005797B0);
			static auto NiAVObjectUpdate_addy = enc_ptr(0x00CC7660);
			static auto sub_4586C0_addy = enc_ptr(0x004586C0);
			static auto sub_56F5C0_addy = enc_ptr(0x0056F5C0);


            using Sub57B550Func = void(__thiscall*)(CCameraTPS*);
            auto sub_57B550 = reinterpret_cast<Sub57B550Func>(sub_57B550_addy.get());

            using Sub579690Func = void(__thiscall*)(CCameraTPS*, float*, float*, float);
            auto sub_579690 = reinterpret_cast<Sub579690Func>(sub_579690_addy.get());

            using Sub5778F0Func = void(__thiscall*)(CCameraTPS*, float);
            auto sub_5778F0 = reinterpret_cast<Sub5778F0Func>(sub_5778F0_addy.get());

            using Sub577D90Func = void(__thiscall*)(CCameraTPS*);
            auto sub_577D90 = reinterpret_cast<Sub577D90Func>(sub_577D90_addy.get());

            using Sub4585C0Func = void(__cdecl*)(float*);
            auto NormalizeRadian = reinterpret_cast<Sub4585C0Func>(NormalizeRadian_addy.get()); //NormalizeRadian

            using Sub578030Func = void(__thiscall*)(CCameraTPS*);
            auto sub_578030 = reinterpret_cast<Sub578030Func>(sub_578030_addy.get());

            using Sub577EE0Func = void(__thiscall*)(CCameraTPS*);
            auto sub_577EE0 = reinterpret_cast<Sub577EE0Func>(sub_577EE0_addy.get());

            using NiMatrix3FromEulerAnglesZYXFunc = void(__thiscall*)(float*, float, float, float);
            auto NiMatrix3FromEulerAnglesZYX = reinterpret_cast<NiMatrix3FromEulerAnglesZYXFunc>(NiMatrix3FromEulerAnglesZYX_addy.get());

            using Sub465A40Func = float* (__thiscall*)(float*, float*, float*);
            auto sub_465A40 = reinterpret_cast<Sub465A40Func>(sub_465A40_addy.get());

            using Sub5797B0Func = void(__thiscall*)(CCameraTPS*, float*, float*, float*);
            auto sub_5797B0 = reinterpret_cast<Sub5797B0Func>(sub_5797B0_addy.get());

            using NiAVObjectUpdateFunc = void(__thiscall*)(NiAVObject*, float, char);
            auto NiAVObjectUpdate = reinterpret_cast<NiAVObjectUpdateFunc>(NiAVObjectUpdate_addy.get());

            using Sub4586C0Func = float(__cdecl*)(float, float);
            auto sub_4586C0 = reinterpret_cast<Sub4586C0Func>(sub_4586C0_addy.get());

            using Sub56F5C0Func = void(__thiscall*)(CCameraTPS*, float);
            auto sub_56F5C0 = reinterpret_cast<Sub56F5C0Func>(sub_56F5C0_addy.get());


            struct Color
            {
                std::uint8_t r, g, b, a;

                // Constructor with default alpha as 255
                Color(float red, float green, float blue, float alpha = 1.0f)
                {
                    r = convertComponent(red);
                    g = convertComponent(green);
                    b = convertComponent(blue);
                    a = convertComponent(alpha);
                }

                // Convert float [0.0 - 1.0] to uint8 [0 - 255]
                static std::uint8_t convertComponent(float component)
                {
                    constexpr double multiplier = 255.0;
                    constexpr double rounding = 0.5;
                    return (component < 1.0f) ? ((component > 0.0f) ? static_cast<std::uint8_t>(component * multiplier + rounding) : 0) : 255;
                }

                // Convert to 32-bit color (ARGB)
                std::uint32_t toARGB() const
                {
                    return (static_cast<std::uint32_t>(a) << 24) |
                        (static_cast<std::uint32_t>(r) << 16) |
                        (static_cast<std::uint32_t>(g) << 8) |
                        static_cast<std::uint32_t>(b);
                }
            };
			static auto sub_430760_addy = enc_ptr(0x00430760);
			static auto addy_011C9C58 = enc_ptr(0x011C9C58);
			static auto addy_011DE160 = enc_ptr(0x011DE160);
			static auto addy_011DE13C = enc_ptr(0x011DE13C);
			static auto addy_00FEB350 = enc_ptr(0x00FEB350);
            int RenderText(std::uint32_t x, std::uint32_t y, Color color, std::string text)
            {
                using Sub430760Func = int(__thiscall*)(char*, std::uint32_t, std::uint32_t, std::uint32_t, const char*, std::uint32_t);
                auto sub_430760 = reinterpret_cast<Sub430760Func>(sub_430760_addy.get());
                return sub_430760(reinterpret_cast<char*>(addy_011C9C58.get()), x, y, color.toARGB(), text.c_str(), text.size());
            }
            std::uint32_t old_move_state_anim = 0;
			constexpr std::uint32_t normal_crouch_anim_id = 24;
			constexpr std::uint32_t right_crouch_anim_id = 9;
			constexpr std::uint32_t left_crouch_anim_id = 10;
			constexpr std::uint32_t forward_crouch_anim_id = 11;
			constexpr std::uint32_t backward_crouch_anim_id = 12;
            bool reset_crouch = false;
            void __fastcall Spectate(CCameraTPS* CCameraTPS, std::uint32_t edx, float idk)
            {
                static auto original = MegaGuard::HooksMgr::Features::SpectatePov.GetOriginal<decltype(&Spectate)>();
                //auto return_address = reinterpret_cast<std::uint32_t>(_ReturnAddress());
                //MegaGuard::EventLog->Debug(nostd::source_location::current(), "call Spectate from 0x{:08X}", return_address);


                auto fps = reinterpret_cast<std::uint32_t*>(0x11D81F0);
                auto my_fps = fps ? *fps : 0;
                std::uint32_t y_pos = 10;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("FPS: {}", my_fps));
				y_pos += 20;

                if (CCameraTPS->m_eControlMode == ControlMode_GAME ||
                    CCameraTPS->m_eControlMode == ControlMode_WATCH)
                {

                    if (CCameraTPS->m_eControlMode == ControlMode_WATCH && CCameraTPS->m_eViewMode == ViewMode_WATCH_CAMERA_VIEW)
                    {
                        sub_5778F0(CCameraTPS, idk);
                        if (CCameraTPS->CGamePlayer->CPlayerModelProperty)
                        {
                            float tmp2PrevBonePos[3] = { 0,0,0 };
                            if (CCameraTPS->CGamePlayer->CPlayerModelProperty->CUnitRootNode)
                            {
                                CCameraTPS->m_vBonePos[0] = CCameraTPS->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[0];
                                CCameraTPS->m_vBonePos[1] = CCameraTPS->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[1];
                                CCameraTPS->m_vBonePos[2] = CCameraTPS->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[2];
                            }

                            CCameraTPS->m_vBonePos[2] = CCameraTPS->m_vBonePos[2] + CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c8;
                            tmp2PrevBonePos[0] = CCameraTPS->m_vBonePos[0];
                            tmp2PrevBonePos[1] = CCameraTPS->m_vBonePos[1];
                            tmp2PrevBonePos[2] = CCameraTPS->m_vBonePos[2];

							if (old_move_state_anim != CCameraTPS->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id)
							{
                                if (CCameraTPS->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id == normal_crouch_anim_id ||
                                    CCameraTPS->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id == right_crouch_anim_id ||
                                    CCameraTPS->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id == left_crouch_anim_id ||
                                    CCameraTPS->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id == forward_crouch_anim_id ||
                                    CCameraTPS->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id == backward_crouch_anim_id) // crouch
                                {
									CCameraTPS->IsCrouch = 1;
									CCameraTPS->IsCrouchAnimActive = 1;
                                    CCameraTPS->FinalCurrHeight = 30.0f;
                                    reset_crouch = true;
								}
								else if (old_move_state_anim == normal_crouch_anim_id ||
                                    old_move_state_anim == right_crouch_anim_id ||
                                    old_move_state_anim == left_crouch_anim_id ||
                                    old_move_state_anim == forward_crouch_anim_id ||
                                    old_move_state_anim == backward_crouch_anim_id) // crouch
                                {
									CCameraTPS->IsCrouch = 0;
									CCameraTPS->IsCrouchAnimActive = 1;
									CCameraTPS->FinalCurrHeight = 90.0f;
								}
								old_move_state_anim = CCameraTPS->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id;
							}
                            sub_577D90(CCameraTPS);//start crouch anim(usually used only locally)
                            CCameraTPS->m_vBonePos[2] = CCameraTPS->m_vBonePos[2] + CCameraTPS->CurrHeight;
                            tmp2PrevBonePos[2] = tmp2PrevBonePos[2] + CCameraTPS->CurrHeight;

                            auto my_yaw = CCameraTPS->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Yaw + CCameraTPS->CGamePlayer->CPlayerModelProperty->Rotation; //yaw + rotation;
                            auto my_pitch = CCameraTPS->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Pitch + CCameraTPS->CGamePlayer->CPlayerModelProperty->Pitch + CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c4 + CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_ac;
                            NormalizeRadian(&my_yaw);
							clamp_yaw(&my_yaw);
                            NormalizeRadian(&my_pitch);
                            sub_578030(CCameraTPS);
                            sub_577EE0(CCameraTPS);
                            float v3 = 0.0f;
                            if (my_pitch < (PI + HALF_PI))
                                v3 = -my_pitch * PITCH_50;
                            else
                                v3 = (TWO_PI - my_pitch) * PITCH_50;

                            CCameraTPS->ViewOffsetByPitch = v3;
                            auto matrix = CCameraTPS->Matrix;
                            auto forward_x = &matrix->matrix_13;
                            auto forward_y = &matrix->matrix_23;
                            auto forward_z = &matrix->matrix_33;;
                            *forward_z = 0.0f;

                            float length = sqrt((*forward_x) * (*forward_x) + (*forward_y) * (*forward_y) + (*forward_z) * (*forward_z));
                            if (length > *reinterpret_cast<double*>(addy_00FEB350.get()))
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

                            float v40[9]{ 0,0,0,0,0,0,0,0,0 };
                            float v49[9]{ 0,0,0,0,0,0,0,0,0 };
                            float v50[9]{ 0,0,0,0,0,0,0,0,0 };

                            auto matrix2 = CCameraTPS->Matrix;

                            auto right_x = matrix2->matrix_11 * CCameraTPS->float114;
                            auto right_y = matrix2->matrix_21 * CCameraTPS->float114;
                            auto right_z = matrix2->matrix_31 * CCameraTPS->float114;

                            v40[6] = right_x;
                            v40[7] = right_y;
                            v40[8] = right_z;
                            auto v22 = right_x + CCameraTPS->m_vBonePos[0];
                            auto v23 = right_y + CCameraTPS->m_vBonePos[1];
                            auto v24 = right_z + CCameraTPS->m_vBonePos[2];
                            v40[3] = v22;
                            v40[4] = v23;
                            v40[5] = v24;
                            tmp2PrevBonePos[0] = v22;
                            tmp2PrevBonePos[1] = v23;
                            tmp2PrevBonePos[2] = v24;

                            NiMatrix3FromEulerAnglesZYX(v49, my_yaw, my_pitch, 0.0);
                            v40[0] = -CCameraTPS->idkk[0];
                            v40[1] = 0.0f;
                            v40[2] = 0.0f;
                            float v39[3]{ 0,0,0 };
                            auto v18 = sub_465A40(v49, v39, v40);
                            CCameraTPS->WorldPos[0] = tmp2PrevBonePos[0] + v18[0];
                            CCameraTPS->WorldPos[1] = tmp2PrevBonePos[1] + v18[1];
                            CCameraTPS->WorldPos[2] = tmp2PrevBonePos[2] + v18[2];
                            float previous_world_pos[3] = { CCameraTPS->WorldPos[0],CCameraTPS->WorldPos[1],CCameraTPS->WorldPos[2] };
                            if (my_pitch < (PI + HALF_PI))
                                my_pitch = my_pitch * DIFFERENCE_IDK + my_pitch;
                            else
                                my_pitch = (TWO_PI - my_pitch) * DIFFERENCE_IDK + my_pitch;

                            NiMatrix3FromEulerAnglesZYX(v50, my_yaw, my_pitch, 0.0);

                            if (CCameraTPS->byte148)
                                sub_5797B0(CCameraTPS, CCameraTPS->m_vBonePos, tmp2PrevBonePos, previous_world_pos);

                            sub_579690(CCameraTPS, v50, previous_world_pos, 0.0);
                            CCameraTPS->float74 = previous_world_pos[0];
                            CCameraTPS->float78 = previous_world_pos[1];
                            CCameraTPS->float7C = previous_world_pos[2];
                            CCameraTPS->NiAVObject->m_localTranslate[0] = previous_world_pos[0];
                            CCameraTPS->NiAVObject->m_localTranslate[1] = previous_world_pos[1];
                            CCameraTPS->NiAVObject->m_localTranslate[2] = previous_world_pos[2];
                            CCameraTPS->NiAVObject->m_localRotate[0] = v50[0];
                            CCameraTPS->NiAVObject->m_localRotate[1] = v50[1];
                            CCameraTPS->NiAVObject->m_localRotate[2] = v50[2];
                            CCameraTPS->NiAVObject->m_localRotate[3] = v50[3];
                            CCameraTPS->NiAVObject->m_localRotate[4] = v50[4];
                            CCameraTPS->NiAVObject->m_localRotate[5] = v50[5];
                            CCameraTPS->NiAVObject->m_localRotate[6] = v50[6];
                            CCameraTPS->NiAVObject->m_localRotate[7] = v50[7];
                            CCameraTPS->NiAVObject->m_localRotate[8] = v50[8];
                            NiAVObjectUpdate(CCameraTPS->NiAVObject, 0.0, 1);
                            sub_57B550(CCameraTPS);
                            CCameraTPS->float4 = sub_4586C0(my_yaw, CCameraTPS->RotationX);
                            CCameraTPS->float8 = sub_4586C0(my_pitch, CCameraTPS->RotationY);
                            CCameraTPS->RotationX = my_yaw;
                            CCameraTPS->RotationY = my_pitch;
                        }
                    }
                    else
                    {

                        auto dword_11DE160 = reinterpret_cast<DWORD*>(addy_011DE160.get());
                        auto flt_11DE13C = reinterpret_cast<float*>(addy_011DE13C.get());
                        if ((*dword_11DE160 & 1) == 0)
                            *dword_11DE160 |= 1u;

                        flt_11DE13C[0] = 1.0f;
                        flt_11DE13C[1] = 0.0f;
                        flt_11DE13C[2] = 0.0f;
                        flt_11DE13C[3] = 0.0f;
                        flt_11DE13C[4] = 1.0f;
                        flt_11DE13C[5] = 0.0f;
                        flt_11DE13C[6] = 0.0f;
                        flt_11DE13C[7] = 0.0f;
                        flt_11DE13C[8] = 1.0f;
                        if (CCameraTPS->byte149)
                        {
                            auto my_coords = CCameraTPS->CGamePlayer->CExPlayer->MovementInfo->coords;
                            float idk[9]{ 0,0,0,0,0,0,0,0,0 };
                            sub_579690(CCameraTPS, idk, my_coords, 0.0f);
                            CCameraTPS->NiAVObject->m_localTranslate[0] = my_coords[0];
                            CCameraTPS->NiAVObject->m_localTranslate[1] = my_coords[1];
                            CCameraTPS->NiAVObject->m_localTranslate[2] = my_coords[2];
                            CCameraTPS->NiAVObject->m_localRotate[0] = idk[0];
                            CCameraTPS->NiAVObject->m_localRotate[1] = idk[1];
                            CCameraTPS->NiAVObject->m_localRotate[2] = idk[2];
                            CCameraTPS->NiAVObject->m_localRotate[3] = idk[3];
                            CCameraTPS->NiAVObject->m_localRotate[4] = idk[4];
                            CCameraTPS->NiAVObject->m_localRotate[5] = idk[5];
                            CCameraTPS->NiAVObject->m_localRotate[6] = idk[6];
                            CCameraTPS->NiAVObject->m_localRotate[7] = idk[7];
                            CCameraTPS->NiAVObject->m_localRotate[8] = idk[8];
                            NiAVObjectUpdate(CCameraTPS->NiAVObject, 0.0f, 1);
                            sub_57B550(CCameraTPS);
                        }
                        else
                        {
                            sub_5778F0(CCameraTPS, idk);
                            if (CCameraTPS->CGamePlayer->CPlayerModelProperty)
                            {
                                float tmp2PrevBonePos[3] = { 0,0,0 };

                                if (CCameraTPS->m_eViewMode == ViewMode_FirstPerson)
                                {
                                    auto character_bone_type = CCameraTPS->CGamePlayer->CPlayerModelProperty->character_bone_type;

                                    float bone_pos[3] = { 0,0,0 };

                                    if (character_bone_type == 0 && CCameraTPS->CGamePlayer->CPlayerModelProperty->HumanBones)
                                    {
                                        bone_pos[0] = CCameraTPS->CGamePlayer->CPlayerModelProperty->HumanBones->position[0];
                                        bone_pos[1] = CCameraTPS->CGamePlayer->CPlayerModelProperty->HumanBones->position[1];
                                        bone_pos[2] = CCameraTPS->CGamePlayer->CPlayerModelProperty->HumanBones->position[2];
                                    }
                                    else if (character_bone_type == 1 && CCameraTPS->CGamePlayer->CPlayerModelProperty->ZombieBones)
                                    {
                                        bone_pos[0] = CCameraTPS->CGamePlayer->CPlayerModelProperty->ZombieBones->position[0];
                                        bone_pos[1] = CCameraTPS->CGamePlayer->CPlayerModelProperty->ZombieBones->position[1];
                                        bone_pos[2] = CCameraTPS->CGamePlayer->CPlayerModelProperty->ZombieBones->position[2];
                                    }
                                    else if (character_bone_type == 2 && CCameraTPS->CGamePlayer->CPlayerModelProperty->ZombieKingBones)
                                    {
                                        bone_pos[0] = CCameraTPS->CGamePlayer->CPlayerModelProperty->ZombieKingBones->position[0];
                                        bone_pos[1] = CCameraTPS->CGamePlayer->CPlayerModelProperty->ZombieKingBones->position[1];
                                        bone_pos[2] = CCameraTPS->CGamePlayer->CPlayerModelProperty->ZombieKingBones->position[2];
                                    }
                                    else
                                    {
                                        auto bone_info = reinterpret_cast<float*>(0x011ED050);
                                        bone_pos[0] = bone_info[0];
                                        bone_pos[1] = bone_info[1];
                                        bone_pos[2] = bone_info[2];
                                    }
                                    CCameraTPS->m_vBonePos[0] = bone_pos[0];
                                    CCameraTPS->m_vBonePos[1] = bone_pos[1];
                                    CCameraTPS->m_vBonePos[2] = bone_pos[2];
                                    tmp2PrevBonePos[0] = CCameraTPS->m_vBonePos[0];
                                    tmp2PrevBonePos[1] = CCameraTPS->m_vBonePos[1];
                                    tmp2PrevBonePos[2] = CCameraTPS->m_vBonePos[2];
                                }
                                else
                                {
                                    if (CCameraTPS->CGamePlayer->CPlayerModelProperty->CUnitRootNode)
                                    {
                                        CCameraTPS->m_vBonePos[0] = CCameraTPS->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[0];
                                        CCameraTPS->m_vBonePos[1] = CCameraTPS->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[1];
                                        CCameraTPS->m_vBonePos[2] = CCameraTPS->CGamePlayer->CPlayerModelProperty->CUnitRootNode->root_pos[2];
                                    }
                                    
                                    CCameraTPS->m_vBonePos[2] = CCameraTPS->m_vBonePos[2] + CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c8;
                                    tmp2PrevBonePos[0] = CCameraTPS->m_vBonePos[0];
                                    tmp2PrevBonePos[1] = CCameraTPS->m_vBonePos[1];
                                    tmp2PrevBonePos[2] = CCameraTPS->m_vBonePos[2];
                                }
                                if (reset_crouch)
                                {
                                    CCameraTPS->IsCrouch = 0;
                                    CCameraTPS->IsCrouchAnimActive = 0;
                                    CCameraTPS->FinalCurrHeight = 90.0f;
									CCameraTPS->CurrHeight = 90.f;
									reset_crouch = false;
                                }
                                sub_577D90(CCameraTPS);
                                CCameraTPS->m_vBonePos[2] = CCameraTPS->m_vBonePos[2] + CCameraTPS->CurrHeight;
                               
                                tmp2PrevBonePos[2] = tmp2PrevBonePos[2] + CCameraTPS->CurrHeight;
                                auto my_yaw = CCameraTPS->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Yaw + CCameraTPS->CGamePlayer->CPlayerModelProperty->Rotation; //yaw + rotation;
                                auto my_pitch = CCameraTPS->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Pitch + CCameraTPS->CGamePlayer->CPlayerModelProperty->Pitch + CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c4 + CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_ac;
                                NormalizeRadian(&my_yaw);
                                NormalizeRadian(&my_pitch);
                                if (CCameraTPS->m_eControlMode == ControlMode_WATCH)
                                {
                                    my_yaw = 0.0;
                                    my_pitch = 0.0;
                                }
                                if (CCameraTPS->m_eViewMode == ViewMode_SELF_CAMERA_VIEW)
                                    my_pitch = 0.0;

                                sub_578030(CCameraTPS);
                                sub_577EE0(CCameraTPS);
                                float v3 = 0.0f;
                                if (my_pitch < (PI + HALF_PI))
                                    v3 = -my_pitch * PITCH_50;
                                else
                                    v3 = (TWO_PI - my_pitch) * PITCH_50;

                                CCameraTPS->ViewOffsetByPitch = v3;
                                auto matrix = CCameraTPS->Matrix;
                                auto forward_x = &matrix->matrix_13;
                                auto forward_y = &matrix->matrix_23;
                                auto forward_z = &matrix->matrix_33;;
                                *forward_z = 0.0f;

                                float length = sqrt((*forward_x) * (*forward_x) + (*forward_y) * (*forward_y) + (*forward_z) * (*forward_z));
                                if (length > *reinterpret_cast<double*>(0x00FEB350))
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
                                


                                float v40[9]{ 0,0,0,0,0,0,0,0,0 };
                                float v49[9]{ 0,0,0,0,0,0,0,0,0 };
                                float v50[9]{ 0,0,0,0,0,0,0,0,0 };
                                if (CCameraTPS->m_eViewMode != ViewMode_ZoomIn || CCameraTPS->m_eViewMode != ViewMode_DoubleZoomIn)
                                {
                                    auto matrix2 = CCameraTPS->Matrix;
                                    auto matrix_21 = matrix2->matrix_21;
                                    auto matrix_31 = matrix2->matrix_31;
                                    auto matrix_11 = matrix2->matrix_11;
                                    auto v41 = matrix_21;
                                    auto v42 = matrix_31;
                                    auto v25 = CCameraTPS->float114;
                                    
                                    auto v26 = matrix_11 * v25;
                                    auto v27 = matrix_21 * v25;
                                    auto v28 = matrix_31 * v25;
                                    v40[6] = v26;
                                    v40[7] = v27;
                                    v40[8] = v28;
                                    auto v22 = v26 + CCameraTPS->m_vBonePos[0];
                                    auto v23 = v27 + CCameraTPS->m_vBonePos[1];
                                    auto v24 = v28 + CCameraTPS->m_vBonePos[2];
                                    v40[3] = v22;
                                    v40[4] = v23;
                                    v40[5] = v24;
                                    tmp2PrevBonePos[0] = v22;
                                    tmp2PrevBonePos[1] = v23;
                                    tmp2PrevBonePos[2] = v24;
                                }
                                auto v14 = my_pitch + CCameraTPS->CameraPitch;
                                auto v13 = my_yaw + CCameraTPS->CameraYaw;
                                NiMatrix3FromEulerAnglesZYX(v49, v13, v14, 0.0);
                                v40[0] = -CCameraTPS->idkk[15 * CCameraTPS->m_eViewMode];
                                
                                v40[1] = 0.0f;
                                v40[2] = 0.0f;
                                float v39[3]{ 0,0,0 };
                                auto v18 = sub_465A40(v49, v39, v40);
                                CCameraTPS->WorldPos[0] = tmp2PrevBonePos[0] + v18[0];
                                CCameraTPS->WorldPos[1] = tmp2PrevBonePos[1] + v18[1];
                                CCameraTPS->WorldPos[2] = tmp2PrevBonePos[2] + v18[2];
                                float previous_world_pos[3] = { CCameraTPS->WorldPos[0],CCameraTPS->WorldPos[1],CCameraTPS->WorldPos[2] };
                                if (my_pitch < (PI + HALF_PI))
                                    my_pitch = my_pitch * DIFFERENCE_IDK + my_pitch;
                                else
                                    my_pitch = (TWO_PI - my_pitch) * DIFFERENCE_IDK + my_pitch;

                                if (CCameraTPS->m_eControlMode == ControlMode_GAME)
                                {
                                    

                                    if (CCameraTPS->m_eViewMode == ViewMode_WATCH_CAMERA_VIEW || CCameraTPS->m_eViewMode == ViewMode_SELF_CAMERA_VIEW)
                                    {
                                        auto v11 = my_pitch + CCameraTPS->float90 + CCameraTPS->CameraPitch;
                                        auto v10 = my_yaw + CCameraTPS->float8C + CCameraTPS->CameraYaw;
                                        NiMatrix3FromEulerAnglesZYX(v50, v10, v11, CCameraTPS->float30);
                                    }
                                    else
                                    {
                                        auto v9 = my_pitch + CCameraTPS->CameraPitch;
                                        auto v8 = my_yaw + CCameraTPS->CameraYaw;
                                        NiMatrix3FromEulerAnglesZYX(v50, v8, v9, CCameraTPS->float30);
                                    }
                                }
                                else
                                {
                                    auto v7 = my_pitch + CCameraTPS->float90 + CCameraTPS->CameraPitch;
                                    auto v6 = my_yaw + CCameraTPS->float8C + CCameraTPS->CameraYaw;
                                    NiMatrix3FromEulerAnglesZYX(v50, v6, v7, CCameraTPS->float30);
                                }
                                if (CCameraTPS->byte148)
                                    sub_5797B0(CCameraTPS, CCameraTPS->m_vBonePos, tmp2PrevBonePos, previous_world_pos);

                                sub_579690(CCameraTPS, v50, previous_world_pos, 0.0);
                                CCameraTPS->float74 = previous_world_pos[0];
                                CCameraTPS->float78 = previous_world_pos[1];
                                CCameraTPS->float7C = previous_world_pos[2];
                                CCameraTPS->NiAVObject->m_localTranslate[0] = previous_world_pos[0];
                                CCameraTPS->NiAVObject->m_localTranslate[1] = previous_world_pos[1];
                                CCameraTPS->NiAVObject->m_localTranslate[2] = previous_world_pos[2];
                                CCameraTPS->NiAVObject->m_localRotate[0] = v50[0];
                                CCameraTPS->NiAVObject->m_localRotate[1] = v50[1];
                                CCameraTPS->NiAVObject->m_localRotate[2] = v50[2];
                                CCameraTPS->NiAVObject->m_localRotate[3] = v50[3];
                                CCameraTPS->NiAVObject->m_localRotate[4] = v50[4];
                                CCameraTPS->NiAVObject->m_localRotate[5] = v50[5];
                                CCameraTPS->NiAVObject->m_localRotate[6] = v50[6];
                                CCameraTPS->NiAVObject->m_localRotate[7] = v50[7];
                                CCameraTPS->NiAVObject->m_localRotate[8] = v50[8];
                                NiAVObjectUpdate(CCameraTPS->NiAVObject, 0.0, 1);
                                sub_57B550(CCameraTPS);
                                CCameraTPS->float4 = sub_4586C0(my_yaw, CCameraTPS->RotationX);
                                CCameraTPS->float8 = sub_4586C0(my_pitch, CCameraTPS->RotationY);
                                
                                CCameraTPS->RotationX = my_yaw;
                                CCameraTPS->RotationY = my_pitch;
                                
                               
                                
                            }
                        }
                    }
                }
                else
                {
                    sub_56F5C0(CCameraTPS, idk);
                    sub_57B550(CCameraTPS);
                }
                /*
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c8: {}", CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c8));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CurrHeight: {}", CCameraTPS->CurrHeight));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->FinalCurrHeight: {}", CCameraTPS->FinalCurrHeight)); 
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->IsCrouch: {}", CCameraTPS->IsCrouch));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->IsCrouchAnimActive: {}", CCameraTPS->IsCrouchAnimActive));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->IsCrouchIdk1: {}", CCameraTPS->IsCrouchIdk1));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->IsCrouchIdk2: {}", CCameraTPS->IsCrouchIdk2));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id: {}", CCameraTPS->CGamePlayer->CUnitMoveStateMgr->UnitMoveState->anim_id));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerActionStateMgr->PlayerActionState->anim_id: {}", CCameraTPS->CGamePlayer->CPlayerActionStateMgr->PlayerActionState->anim_id));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->m_eViewMode: {}", CCameraTPS->m_eViewMode));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->idkk[15 * CCameraTPS->m_eViewMode]: {}", CCameraTPS->idkk[15 * CCameraTPS->m_eViewMode]));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->float90: {}", CCameraTPS->float90));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->float8C: {}", CCameraTPS->float8C));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->float4: {}", CCameraTPS->float4));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->float8: {}", CCameraTPS->float8));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->float114: {}", CCameraTPS->float114));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->RotationX: {}", CCameraTPS->RotationX));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->RotationY: {}", CCameraTPS->RotationY));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CameraPitch: {}", CCameraTPS->CameraPitch));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CameraYaw: {}", CCameraTPS->CameraYaw));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->float30: {}", CCameraTPS->float30));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Yaw: {}", CCameraTPS->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Yaw));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerModelProperty->Rotation: {}", CCameraTPS->CGamePlayer->CPlayerModelProperty->Rotation));
                y_pos += 20;
                auto my_yaw = CCameraTPS->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Yaw + CCameraTPS->CGamePlayer->CPlayerModelProperty->Rotation; //yaw + rotation;
				normalizeRadians(&my_yaw);
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("final my_yaw: {}", my_yaw));
                y_pos += 20;

                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Pitch: {}", CCameraTPS->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Pitch));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerModelProperty->Pitch: {}", CCameraTPS->CGamePlayer->CPlayerModelProperty->Pitch));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c4: {}", CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c4));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_ac: {}", CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_ac));
                y_pos += 20;
                auto my_pitch = CCameraTPS->CGamePlayer->CPlayerModelProperty->CGamePlayer->CExPlayer->room_info->Pitch + CCameraTPS->CGamePlayer->CPlayerModelProperty->Pitch + CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c4 + CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_ac;
				normalizeRadians(&my_pitch);
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("final my_pitch: {}", my_pitch));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_b0): {}", CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_b0));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_b4): {}", CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_b4));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_b8): {}", CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_b8));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_bc): {}", CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_bc));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c0): {}", CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c0));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c4): {}", CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c4));
                y_pos += 20;
                RenderText(10, y_pos, Color(0.0f, 1.0f, 0.0f), fmt::format("CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c8): {}", CCameraTPS->CGamePlayer->CPlayerModelProperty->idk_c8));
                y_pos += 20;
                */
                
            }
        }
    }
}