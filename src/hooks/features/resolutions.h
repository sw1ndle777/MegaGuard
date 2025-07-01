#pragma once
namespace MegaGuard
{
    namespace HooksMgr
    {
		struct Resolution
		{
			int width;
			int height;
			int aspectRatioIdx;
            char16_t AspectRatio[64];
		};

        inline int widthBuffer[] = {
            640, 800, 960, 1024, 1024,
            1152, 1280, 1280, 1280, 1280,
            1280, 1366, 1400, 1440, 1600,
            1600, 1680, 1920, 1920, 2560,
            2560, 2880, 3200, 3200, 3840,
            3840
        };
        inline int heightBuffer[] = {
            480, 600, 768, 600, 768,
            864, 720, 768, 800, 960,
            1024, 768, 1050, 900, 900,
            1200, 1050, 1080, 1200, 1440,
            1600, 1800, 1800, 2000, 2160,
            2400
        };
		inline int aspectRatioIdx[] = {
			0, 0, 2, 3, 0,
            0, 3, 1, 4, 0,
            2, 3, 0, 4, 3,
            0, 4, 3, 4, 3,
            4, 4, 3, 4, 3,
            4
		};

        inline Resolution customResolutions[] = {
            {  640,  480,  0, u"(4:3)" },
            {  800,  600,  0, u"(4:3)" },
            {  960,  768,  2, u"(5:4)" },
            { 1024,  600,  3, u"(16:9)" },
            { 1024,  768,  0, u"(4:3)" },
            { 1152,  864,  0, u"(4:3)" },
            { 1280,  720,  3, u"(16:9)" },
            { 1280,  768,  1, u"(5:3)" },
            { 1280,  800,  4, u"(16:10)" },
            { 1280,  960,  0, u"(4:3)" },
            { 1280,  1024, 2, u"(5:4)" },
			{ 1366,  768,  3, u"(16:9)" },
			{ 1400,  1050, 0, u"(4:3)" },
			{ 1440,  900,  4, u"(16:10)" },
			{ 1600,  900,  3, u"(16:9)" },
            { 1600,  1200, 0, u"(4:3)" },
            { 1680,  1050, 4, u"(16:10)" },
            { 1920,  1080, 3, u"(16:9)" },
            { 1920,  1200, 4, u"(16:10)" },
            
            { 2560,  1440, 3, u"(16:9)" },
			{ 2560,  1600, 4, u"(16:10)" },
			{ 2880,  1800, 4, u"(16:10)" },
			{ 3200,  1800, 3, u"(16:9)" },
			{ 3200,  2000, 4, u"(16:10)" },
			{ 3840,  2160, 3, u"(16:9)" },
			{ 3840,  2400, 4, u"(16:10)" },

			{ 1792,  768,  3, u"(21:9)" },
            { 2560, 1080,  3, u"(21:9)" },
            { 3440, 1440,  3, u"(21:9)" },
            { 3840, 1600,  3, u"(21:9)" },
            { 5120, 2160,  3, u"(21:9)" },

            { 2520, 1200,  4, u"(21:10)" },
            { 3360, 1600,  4, u"(21:10)" },
            { 4200, 2000,  4, u"(21:10)" }
        };

        using namespace MegaGuard::Globals::Features;
        namespace Features
        {
            inline void __fastcall SetAspectRatioScaling(int _thisptr, int edx, int a2, int a3, int game_width, int game_height)
            {
				_wv<DWORD>(_thisptr, 0x20, a2);
				_wv<DWORD>(_thisptr, 0x24, a3);
				_wv<DWORD>(_thisptr, 0x28, game_width);
				_wv<DWORD>(_thisptr, 0x2C, game_height);
				_wv<DWORD>(_thisptr, 8, _rv<DWORD>(_thisptr, 0));
                _wv<DWORD>(_thisptr, 0xC, _rv<DWORD>(_thisptr, 0x4));
                auto diff = game_width - a2;
                auto diff2 = game_height - a3;
				_wv<DWORD>(_thisptr, 0, diff);
				_wv<DWORD>(_thisptr, 4, diff2);
				auto aspect_ratio = static_cast<double>(diff) / static_cast<double>(diff2);

                
                if (aspect_ratio >= 1.29999995231628)
                {
                    if (aspect_ratio >= 1.39999997615814)
                    {
                        if (aspect_ratio >= 1.61000001430511)
                        {
                            if (aspect_ratio < 1.79999995231628)
								_wv<DWORD>(_thisptr, 0x58, 2); // :9
							else if (aspect_ratio < 2.15f)
								_wv<DWORD>(_thisptr, 0x58, 3); // :10
							else
								_wv<DWORD>(_thisptr, 0x58, 2); // :9
                        }
                        else
                            _wv<DWORD>(_thisptr, 0x58, 3); // :10
                    }
                    else
						_wv<DWORD>(_thisptr, 0x58, 0); // :3
                }
                else
					_wv<DWORD>(_thisptr, 0x58, 1); // :4
                
				if (_rv<DWORD>(_thisptr, 0x58) == 1)
                    _wv<float>(_thisptr, 0x54, 1.25f); // 5/4
				else
                    _wv<float>(_thisptr, 0x54, 1.333333373f); // 4/3
                
            }
            inline void CustomResolutionsPatch()
            {
                
				Features::SetAspectRatioScale.Create(MegaGuard::Addresses::Hooks::Features::Resolutions::SetAspectRatioScale.get(), &SetAspectRatioScaling);

                Features::WidthBuff1.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff1.get(), &customResolutions[0].width);
				Features::WidthBuff2.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff2.get(), &customResolutions[0].width);
				Features::WidthBuff3.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff3.get(), &customResolutions[0].width);
                Features::WidthBuff4.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff4.get(), &customResolutions[0].width);
                Features::WidthBuff5.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff5.get(), &customResolutions[0].width);
                Features::WidthBuff6.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff6.get(), &customResolutions[0].width);
                Features::WidthBuff7.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff7.get(), &customResolutions[0].width);
                Features::WidthBuff8.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff8.get(), &customResolutions[0].width);
                Features::WidthBuff9.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff9.get(), &customResolutions[0].width);
                Features::WidthBuff10.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff10.get(), &customResolutions[0].width);
                Features::WidthBuff11.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff11.get(), &customResolutions[0].width);
                Features::WidthBuff12.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff12.get(), &customResolutions[0].width);
                Features::WidthBuff13.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff13.get(), &customResolutions[0].width);
                Features::WidthBuff14.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff14.get(), &customResolutions[0].width);
                Features::WidthBuff15.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff15.get(), &customResolutions[0].width);
                Features::WidthBuff16.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff16.get(), &customResolutions[0].width);
                Features::WidthBuff17.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff17.get(), &customResolutions[0].width);
                Features::WidthBuff18.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff18.get(), &customResolutions[0].width);
                Features::WidthBuff19.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff19.get(), &customResolutions[0].width);
				Features::WidthBuff20.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::WidthBuff20.get(), &customResolutions[0].width);

                Features::HeightBuff1.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff1.get(), &customResolutions[0].height);
                Features::HeightBuff2.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff2.get(), &customResolutions[0].height);
                Features::HeightBuff3.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff3.get(), &customResolutions[0].height);
                Features::HeightBuff4.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff4.get(), &customResolutions[0].height);
                Features::HeightBuff5.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff5.get(), &customResolutions[0].height);
                Features::HeightBuff6.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff6.get(), &customResolutions[0].height);
                Features::HeightBuff7.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff7.get(), &customResolutions[0].height);
                Features::HeightBuff8.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff8.get(), &customResolutions[0].height);
                Features::HeightBuff9.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff9.get(), &customResolutions[0].height);
                Features::HeightBuff10.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff10.get(), &customResolutions[0].height);
                Features::HeightBuff11.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff11.get(), &customResolutions[0].height);
                Features::HeightBuff12.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff12.get(), &customResolutions[0].height);
                Features::HeightBuff13.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff13.get(), &customResolutions[0].height);
                Features::HeightBuff14.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff14.get(), &customResolutions[0].height);
                Features::HeightBuff15.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff15.get(), &customResolutions[0].height);
                Features::HeightBuff16.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff16.get(), &customResolutions[0].height);
                Features::HeightBuff17.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff17.get(), &customResolutions[0].height);
                Features::HeightBuff18.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff18.get(), &customResolutions[0].height);
                Features::HeightBuff19.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::HeightBuff19.get(), &customResolutions[0].height);

                Features::AspectRatioIds1.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds1.get(), &customResolutions[0].aspectRatioIdx);
                Features::AspectRatioIds2.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds2.get(), &customResolutions[0].aspectRatioIdx);
                Features::AspectRatioIds3.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds3.get(), &customResolutions[0].aspectRatioIdx);
                Features::AspectRatioIds4.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds4.get(), &customResolutions[0].aspectRatioIdx);
                Features::AspectRatioIds5.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds5.get(), &customResolutions[0].aspectRatioIdx);
                Features::AspectRatioIds6.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds6.get(), &customResolutions[0].aspectRatioIdx);
                Features::AspectRatioIds7.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds7.get(), &customResolutions[0].aspectRatioIdx);
                Features::AspectRatioIds8.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds8.get(), &customResolutions[0].aspectRatioIdx);
                Features::AspectRatioIds9.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds9.get(), &customResolutions[0].aspectRatioIdx);
                Features::AspectRatioIds10.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds10.get(), &customResolutions[0].aspectRatioIdx);
                Features::AspectRatioIds11.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds11.get(), &customResolutions[0].aspectRatioIdx);
                Features::AspectRatioIds12.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds12.get(), &customResolutions[0].aspectRatioIdx);
                Features::AspectRatioIds13.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds13.get(), &customResolutions[0].aspectRatioIdx);
                Features::AspectRatioIds14.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds14.get(), &customResolutions[0].aspectRatioIdx);
                Features::AspectRatioIds15.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::AspectRatioIds15.get(), &customResolutions[0].aspectRatioIdx);

                
				Features::ResolutionListSize1.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::ResolutionListSize1_Patch.get(), "\x83\xBD\xE0\xFD\xFF\xFF\x22", 7); //0x13(19) -> 0x1A(26)
                Features::ResolutionListSize2.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::ResolutionListSize2_Patch.get(), "\x83\xBD\xC4\xFD\xFF\xFF\x22", 7); //0x13(19) -> 0x1A(26)
				Features::ResolutionListSize3.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::ResolutionListSize3_Patch.get(), "\xC7\x45\xE8\x21\x00\x00\x00", 7); //0x12(18) -> 0x19(25)
                Features::ResolutionListSize4.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::ResolutionListSize4_Patch.get(), "\xC7\x45\xD4\x21\x00\x00\x00", 7); //0x12(18) -> 0x19(25)
                Features::ResolutionListSize5.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::ResolutionListSize5_Patch.get(), "\xC7\x45\xC0\x21\x00\x00\x00", 7); //0x12(18) -> 0x19(25)
                Features::ResolutionListSize6.Patch(MegaGuard::Addresses::Hooks::Features::Resolutions::ResolutionListSize6_Patch.get(), "\x83\x7D\xD4\x10\x77\x0A\x90\x90\x90\x90\x90\x90", 12); 
            }
        }
    }
}