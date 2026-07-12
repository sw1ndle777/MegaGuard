// =============================================================================
// Resolutions - Implementation
// =============================================================================
#include "pch.hpp"
#include "modding/features/resolutions.hpp"
#include "core/context.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::modding {

using namespace mg::game;

// ── Resolution table (stride = 140 bytes, matching game binary layout) ────────
// Aspect index: 0=4:3, 1=5:3, 2=5:4, 3=16:9, 4=16:10
const Resolution kResolutions[kNumResolutions] = {

    // ================= 4:3 =================
    {  640,  480, 0, u"(4:3)" },   // SD
    {  800,  600, 0, u"(4:3)" },   // SVGA
    { 1024,  768, 0, u"(4:3)" },   // XGA
    { 1152,  864, 0, u"(4:3)" },   // XGA+
    { 1280,  960, 0, u"(4:3)" },   // 960p
    { 1400, 1050, 0, u"(4:3)" },   // SXGA+
    { 1600, 1200, 0, u"(4:3)" },   // UXGA
    { 1920, 1440, 0, u"(4:3)" },   // 1440p 4:3
    { 2048, 1536, 0, u"(4:3)" },   // QXGA
    { 2560, 1920, 0, u"(4:3)" },   // 5MP 4:3
    { 3200, 2400, 0, u"(4:3)" },   // QUXGA
    { 4096, 3072, 0, u"(4:3)" },   // ~12MP 4:3

    // ================= 5:4 =================
    { 1280, 1024, 2, u"(5:4)" },   // SXGA

    // ================= 5:3 =================
    { 1280,  768, 1, u"(5:3)" },   // WXGA variant

    // ================= 16:9 =================
    {  854,  480, 3, u"(16:9)" },  // FWVGA / 480p
    { 1280,  720, 3, u"(16:9)" },  // HD / 720p
    { 1360,  768, 3, u"(16:9)" },  // HD ready
    { 1366,  768, 3, u"(16:9)" },  // HD ready (common laptop)
    { 1600,  900, 3, u"(16:9)" },  // HD+
    { 1920, 1080, 3, u"(16:9)" },  // Full HD / 1080p
    { 2560, 1440, 3, u"(16:9)" },  // QHD / 1440p
    { 3840, 2160, 3, u"(16:9)" },  // 4K UHD
    { 5120, 2880, 3, u"(16:9)" },  // 5K
    { 7680, 4320, 3, u"(16:9)" },  // 8K UHD

    // ================= 16:10 =================
    { 1280,  800, 4, u"(16:10)" }, // WXGA
    { 1440,  900, 4, u"(16:10)" }, // WXGA+
    { 1680, 1050, 4, u"(16:10)" }, // WSXGA+
    { 1920, 1200, 4, u"(16:10)" }, // WUXGA
    { 2560, 1600, 4, u"(16:10)" }, // WQXGA
    { 3840, 2400, 4, u"(16:10)" }, // 4K+ (16:10)

    // ================= 21:9 (Ultrawide) =================
    { 2560, 1080, 3, u"(21:9)" },  // Ultrawide FHD
    { 3440, 1440, 3, u"(21:9)" },  // Ultrawide QHD
    { 3840, 1600, 3, u"(21:9)" },  // Ultrawide 1600p
    { 5120, 2160, 3, u"(21:9)" },  // Ultrawide 5K

    // ================= 32:9 (Super Ultrawide) =================
    { 5120, 1440, 3, u"(32:9)" },  // Super Ultrawide (Dual QHD)

    // ================= 21:10 =================
    { 2560, 1200, 4, u"(21:10)" }, // Ultrawide variant
};

namespace {

// ── Aspect ratio hook (thiscall via fastcall trick — matches game signature) ─

void __fastcall hkSetAspectRatioScaling(uptr _thisptr, u32 /*edx*/,
    i32 a2, i32 a3, i32 game_width, i32 game_height)
{
    mg::writeValue<u32>(_thisptr, 0x20, a2);
    mg::writeValue<u32>(_thisptr, 0x24, a3);
    mg::writeValue<u32>(_thisptr, 0x28, game_width);
    mg::writeValue<u32>(_thisptr, 0x2C, game_height);
    mg::writeValue<u32>(_thisptr, 0x08, mg::readValue<u32>(_thisptr, 0x00));
    mg::writeValue<u32>(_thisptr, 0x0C, mg::readValue<u32>(_thisptr, 0x04));

    auto diff  = game_width - a2;
    auto diff2 = game_height - a3;
    mg::writeValue<u32>(_thisptr, 0x00, diff);
    mg::writeValue<u32>(_thisptr, 0x04, diff2);

    auto aspect_ratio = static_cast<double>(diff) / static_cast<double>(diff2);

    u32 aspectType = 0;
    if (aspect_ratio < 1.30)
        aspectType = 1;                 // 5:4
    else if (aspect_ratio < 1.40)
        aspectType = 0;                 // 4:3
    else if (aspect_ratio < 1.61)
        aspectType = 3;                 // 16:9
    else if (aspect_ratio < 1.80)
        aspectType = 2;                 // 16:10
    else if (aspect_ratio < 2.15)
        aspectType = 3;                 // 16:9 / 21:10
    else
        aspectType = 2;                 // 21:9 / 32:9 ultrawide

    mg::writeValue<u32>(_thisptr, 0x58, aspectType);

    // projection ratio used by UI / engine
    if (aspectType == 1)
        mg::writeValue<float>(_thisptr, 0x54, 1.25f);          // 5:4
    else
        mg::writeValue<float>(_thisptr, 0x54, 1.333333373f);   // 4:3 base

    /*
    if (aspect_ratio >= 1.29999995231628)
    {
        if (aspect_ratio >= 1.39999997615814)
        {
            if (aspect_ratio >= 1.61000001430511)
            {
                if (aspect_ratio < 1.79999995231628)
                    mg::writeValue<u32>(_thisptr, 0x58, 2);       // 16:10
                else if (aspect_ratio < 2.15)
                    mg::writeValue<u32>(_thisptr, 0x58, 3);       // 16:9
                else
                    mg::writeValue<u32>(_thisptr, 0x58, 2);       // 21:9+
            }
            else
                mg::writeValue<u32>(_thisptr, 0x58, 3);           // ~16:9
        }
        else
            mg::writeValue<u32>(_thisptr, 0x58, 0);               // 4:3
    }
    else
        mg::writeValue<u32>(_thisptr, 0x58, 1);                   // 5:4

    if (mg::readValue<u32>(_thisptr, 0x58) == 1)
        mg::writeValue<float>(_thisptr, 0x54, 1.25f);             // 5/4
    else
        mg::writeValue<float>(_thisptr, 0x54, 1.333333373f);      // 4/3
    */
}

// ── Resolution list size patch bytes ─────────────────────────────────────
// These overwrite raw instruction bytes to change the hardcoded count.
// For 36 resolutions: CMP uses 0x24 (36), MOV uses 0x23 (35 = max index).

constexpr u8 kResListSize1[] = { 0x83, 0xBD, 0xE0, 0xFD, 0xFF, 0xFF, 0x24 };              // cmp [ebp-0x220], 36
constexpr u8 kResListSize2[] = { 0x83, 0xBD, 0xC4, 0xFD, 0xFF, 0xFF, 0x24 };              // cmp [ebp-0x23C], 36
constexpr u8 kResListSize3[] = { 0xC7, 0x45, 0xE8, 0x23, 0x00, 0x00, 0x00 };              // mov [ebp-0x18], 35
constexpr u8 kResListSize4[] = { 0xC7, 0x45, 0xD4, 0x23, 0x00, 0x00, 0x00 };              // mov [ebp-0x2C], 35
constexpr u8 kResListSize5[] = { 0xC7, 0x45, 0xC0, 0x23, 0x00, 0x00, 0x00 };              // mov [ebp-0x40], 35
constexpr u8 kResListSize6[] = { 0x83, 0x7D, 0xD4, 0x10, 0x77, 0x0A, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }; // bounds check + nops

struct ResListSizePatch {
    const u8* bytes;
    u32       size;
};
constexpr ResListSizePatch kResListSizePatches[6] = {
    { kResListSize1, sizeof(kResListSize1) },
    { kResListSize2, sizeof(kResListSize2) },
    { kResListSize3, sizeof(kResListSize3) },
    { kResListSize4, sizeof(kResListSize4) },
    { kResListSize5, sizeof(kResListSize5) },
    { kResListSize6, sizeof(kResListSize6) },
};

} // anonymous namespace

CustomResolutions::CustomResolutions(MegaGuardContext& ctx) : ctx_(ctx) {}
CustomResolutions::~CustomResolutions() = default;

VoidResult CustomResolutions::patch() {
    auto& registry = ctx_.hookRegistry();
    namespace res = addr::features::resolutions;

    // ── Patch width buffer pointers ──────────────────────────────────────
    // SwapAddressPatch replaces the base address operand in instructions.
    // The game uses stride = sizeof(Resolution) = 140, so we must point
    // into the struct array, NOT flat arrays.
    for (u32 i = 0; i < res::kNumWidthBuffers; ++i) {
        registry.registerSwapPatch(HookId::ResWidth_Base + i).patch(
            res::WidthBuffers[i], &kResolutions[0].width);
    }

    // ── Patch height buffer pointers ─────────────────────────────────────
    for (u32 i = 0; i < res::kNumHeightBuffers; ++i) {
        registry.registerSwapPatch(HookId::ResHeight_Base + i).patch(
            res::HeightBuffers[i], &kResolutions[0].height);
    }

    // ── Patch aspect ratio ID pointers ───────────────────────────────────
    for (u32 i = 0; i < res::kNumAspectRatioIdBuffers; ++i) {
        registry.registerSwapPatch(HookId::ResAspect_Base + i).patch(
            res::AspectRatioIdBuffers[i], &kResolutions[0].aspectRatioIdx);
    }

    // ── Patch resolution list sizes (raw instruction byte patches) ───────
    for (u32 i = 0; i < res::kNumResolutionListSizes; ++i) {
        registry.registerPatchBytes(HookId::ResListSize_Base + i).patch(
            res::ResolutionListSizes[i],
            kResListSizePatches[i].bytes,
            kResListSizePatches[i].size);
    }

    // ── Aspect ratio scaling hook (detour) ───────────────────────────────
    registry.registerDetour(HookId::SetAspectRatioScale).create(
        res::SetAspectRatioScaling, hkSetAspectRatioScaling);

    return VoidResult::ok();
}

} // namespace mg::modding
