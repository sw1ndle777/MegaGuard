// =============================================================================
// ZoomSensitivity - Per-zoom (rifle ADS / sniper scope) mouse sensitivity + accel
// =============================================================================
// The stock client exposes a single mouse sensitivity (senitivity_val) plus an
// acceleration factor. The newer Steam client added MouseSenseZoom1 (rifle aim-
// down-sights) and MouseSenseZoom2 (sniper scope) tiers. This feature ports that
// behaviour 1:1 onto the old client WITHOUT reimplementing any vanilla math:
//
//   • hkSensMultiplier  - wraps the core look-delta scaler sub_8FF010. It detects
//       the active zoom stage and TEMPORARILY swaps senitivity_val to the matching
//       per-zoom tier around the original call, so the vanilla sniper FOV-ratio
//       scaling and the acceleration curve are preserved byte-for-byte.
//   • hkBattleAdjust    - wraps the in-battle sens/accel keybind handler
//       (sub_BA2570) so the sensitivity up/down keys edit the tier for whatever
//       zoom stage the player is currently in (mirrors the newer client).
//   • hkLoadCfg/hkSaveCfg - read/write MouseSenseZoom1/MouseSenseZoom2 in the
//       [Input] section of option.ini, using the same key names the new client
//       does (so configs stay cross-compatible).
//
// The per-zoom→weapon mapping matches the newer client: rifle ADS uses Zoom1,
// sniper scope (either stage) uses Zoom2, everything else uses the base value.
// =============================================================================
#include "pch.hpp"
#include "modding/features/zoom_sensitivity.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"

#include <windows.h>
#include <cstdio>

namespace mg::modding {

using namespace mg::game;
namespace zs = addr::features::zoom_sensitivity;

namespace {

// Per-zoom sensitivity tiers (1..100), MegaGuard-owned and persisted to
// option.ini. A value <= 0 means "unset → fall back to the base sensitivity".
int g_senZoom1 = 0;   // rifle aim-down-sights  (MouseSenseZoom1)
int g_senZoom2 = 0;   // sniper scope, any stage (MouseSenseZoom2)

// Per-zoom acceleration-factor tiers (0..100). 0 is a VALID value (no accel), so
// the "unset" sentinel here is < 0 (not <= 0 like sensitivity, whose min is 1).
int g_accZoom1 = -1;  // rifle aim-down-sights  (MouseAccelFactorZoom1)
int g_accZoom2 = -1;  // sniper scope, any stage (MouseAccelFactorZoom2)

// Per-zoom booleans: mouse-acceleration-enabled and invert-mouse. -1 = unset (fall
// back to the base value). The base values mirror the stock globals (byte_11C6912 /
// byte_11C6911) but are tracked here too because the live bytes are swapped per zoom
// stage at runtime. Persisted to option.ini alongside the sens/accel tiers.
int g_accelOnBase = -1, g_accelOn1 = -1, g_accelOn2 = -1; // MouseAccelEnableZoom1/2
int g_invertBase  = -1, g_invert1  = -1, g_invert2  = -1; // MouseInvertZoom1/2
int g_rtStage     = -1; // last zoom stage the runtime applied the bools for

void persistZoom();   // defined below (used by the in-battle keybind hook above it)

// Effective per-stage booleans (unset tier -> base). stage 0 = base.
inline int effAccelOn(int stage)
{
    if (stage == 1) return g_accelOn1 >= 0 ? g_accelOn1 : g_accelOnBase;
    if (stage == 2) return g_accelOn2 >= 0 ? g_accelOn2 : g_accelOnBase;
    return g_accelOnBase;
}
inline int effInvert(int stage)
{
    if (stage == 1) return g_invert1 >= 0 ? g_invert1 : g_invertBase;
    if (stage == 2) return g_invert2 >= 0 ? g_invert2 : g_invertBase;
    return g_invertBase;
}

// Are we in an actual playing match? Gates all the per-frame zoom runtime work so it
// never runs during the (cold) match-load transition — that's what black-screened the
// first Single Wave. Cheap: one global read, no pointer walk.
inline bool inMatch()
{
    const int s = mg::readValue<int>(zs::GameState);
    return s == zs::StateModPlaying || s == zs::StateTutorialPlay;
}

// Cheap user-space pointer sanity check (no syscall — just two compares). On this
// 32-bit, non-LARGE-ADDRESS-AWARE client, every valid heap/object pointer lives in
// [0x10000, 0x80000000); the null page below and the kernel range at/above 0x80000000
// can never hold a game object. A transient mid-match pointer (weapon switch / death /
// respawn) is frequently *non-null garbage* in exactly those ranges — e.g. the
// 0xEDFFE7FF that crashed callVFunc here — which the old null-only checks waved through.
// This rejects that class outright, so it never reaches a dereference, and unlike the
// per-frame VirtualQuery the author removed it costs nothing on the hot look path.
inline bool okPtr(uptr p) { return p >= 0x10000 && p < 0x80000000; }

// ── Active zoom stage of the local player ───────────────────────────────────
// 0 = hipfire / none, 1 = rifle ADS, 2 = sniper scope. Mirrors the weapon/zoom
// test the vanilla multiplier (sub_8FF010) and the newer client both perform.
// The camera->owner->exPlayer->slot->weapon walk dereferences raw game pointers.
// Each link is okPtr-validated (a non-null but bogus pointer is the common mid-match
// failure, and that's what faulted callVFunc), and the whole walk is additionally
// wrapped in SEH as a backstop for a plausible-range-but-unmapped pointer. __declspec
// (noinline) keeps this its own frame so the SEH scope isn't folded away by the
// optimizer/obfuscator (the unhooked release build did NOT reliably dispatch the
// __except, which is why the okPtr guards — not SEH — are the real fix). The inMatch()
// gate on the callers keeps this from running during the cold match-load. POD locals
// only, so __try is legal; the one MG_CONST is resolved before the guarded block.
__declspec(noinline) int zoomStage(uptr camera)
{
    if (!okPtr(camera)) return 0;
    const uptr worldFn = MG_CONST(zs::CWorld_GetInstance);
    int result = 0;
    __try {
        const uptr owner = mg::readValue<uptr>(camera, zs::CamOwner);
        if (!okPtr(owner)) return 0;
        const uptr exPlayer = mg::readValue<uptr>(owner, zs::OwnerExPlayer);
        if (!okPtr(exPlayer)) return 0;
        const uptr slot = mg::readValue<uptr>(exPlayer, zs::ExPlayerSlot);
        if (!okPtr(slot)) return 0;
        // Validate the vtable pointer itself before callVFunc dereferences slot[0].
        if (!okPtr(mg::readValue<uptr>(slot))) return 0;
        const uptr weapon = mg::callVFunc<uptr>(reinterpret_cast<void*>(slot), zs::VtblGetWeapon);
        if (!okPtr(weapon)) return 0;

        const int category = mg::readValue<int>(weapon, zs::WeaponCategory);
        if (category == 3) {            // sniper — two scope stages, both use Zoom2
            const int a = mg::readValue<int>(weapon, zs::WeaponZoomA);
            const int b = mg::readValue<int>(weapon, zs::WeaponZoomB);
            if ((a == 1 && b) || (a == 2 && b == 2)) result = 2;
        } else if (category == 1) {     // rifle — single ADS, uses Zoom1
            const uptr world = reinterpret_cast<uptr(__cdecl*)()>(worldFn)();
            const uptr activeCam = okPtr(world) ? mg::readValue<uptr>(world, zs::ActiveCamOff) : 0;
            if (okPtr(activeCam) && mg::readValue<int>(activeCam, zs::CamViewMode) == 1) result = 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return result;
}

uptr activeCamera()
{
    const uptr world = mg::call<uptr(__cdecl*)()>(MG_CONST(zs::CWorld_GetInstance));
    return world ? mg::readValue<uptr>(world, zs::ActiveCamOff) : 0;
}

// ── hkSensMultiplier — wraps sub_8FF010 (look-delta scaler) ──────────────────
double __fastcall hkSensMultiplier(void* self, void* /*edx*/, int rawDelta)
{
    auto& registry = mg::ctx().hookRegistry();
    auto  original = registry.findDetour(HookId::ZoomSensMultiplier)
        ->getOriginal<double(__fastcall*)(void*, void*, int)>();

    // Not in a playing match (loading / lobby / plaza): do NO zoom work — this is the
    // fix for the cold first-load black screen. If we'd swapped the accel/invert bytes
    // for a tier, restore them to base so out-of-match look behaves normally.
    if (!inMatch()) {
        if (g_accelOnBase >= 0 && g_rtStage != 0) {
            mg::writeValue<u8>(zs::AccelOnVal, 0, static_cast<u8>(g_accelOnBase));
            mg::writeValue<u8>(zs::InvertVal,  0, static_cast<u8>(g_invertBase));
            g_rtStage = 0;
        }
        return original(self, nullptr, rawDelta);
    }

    const int stage = zoomStage(reinterpret_cast<uptr>(self));

    // Lazy-capture the base booleans from the live globals on first use.
    if (g_accelOnBase < 0) g_accelOnBase = mg::readValue<u8>(zs::AccelOnVal) ? 1 : 0;
    if (g_invertBase  < 0) g_invertBase  = mg::readValue<u8>(zs::InvertVal)  ? 1 : 0;

    // Per-tier accel-enabled + invert: the look path reads these live bytes, so set
    // them to the active stage's values on each stage change (stage 0 -> base).
    if (stage != g_rtStage) {
        mg::writeValue<u8>(zs::AccelOnVal, 0, static_cast<u8>(effAccelOn(stage)));
        mg::writeValue<u8>(zs::InvertVal,  0, static_cast<u8>(effInvert(stage)));
        g_rtStage = stage;
    }

    if (stage == 0)
        return original(self, nullptr, rawDelta);   // hipfire / no-scope / non-ADS: vanilla

    const int sensTier  = (stage == 1) ? g_senZoom1 : g_senZoom2;
    const int accelTier = (stage == 1) ? g_accZoom1 : g_accZoom2;
    const bool doSens  = sensTier  > 0;     // sens min is 1 -> <=0 means unset
    const bool doAccel = accelTier >= 0;    // accel 0 is valid -> <0 means unset
    if (!doSens && !doAccel)
        return original(self, nullptr, rawDelta);

    const int savedSens  = mg::readValue<int>(zs::SensVal);
    const int savedAccel = mg::readValue<int>(zs::AccelVal);
    if (doSens)  mg::writeValue<int>(zs::SensVal,  0, sensTier);
    if (doAccel) mg::writeValue<int>(zs::AccelVal, 0, accelTier);
    const double result = original(self, nullptr, rawDelta);
    mg::writeValue<int>(zs::SensVal,  0, savedSens);
    mg::writeValue<int>(zs::AccelVal, 0, savedAccel);
    return result;
}

// ── hkBattleAdjust — wraps sub_BA2570 (in-battle sens/accel keybinds) ─────────
void __cdecl hkBattleAdjust()
{
    auto& registry = mg::ctx().hookRegistry();
    auto  original = registry.findDetour(HookId::ZoomSensBattleAdjust)
        ->getOriginal<void(__cdecl*)()>();

    if (!inMatch()) { original(); return; }   // no zoom walk outside a playing match

    const int stage = zoomStage(activeCamera());
    if (stage == 0) {               // base sensitivity + accel keys: vanilla path
        original();
        return;
    }

    // While scoped/ADS, route BOTH the sensitivity keys (17/18 -> senitivity_val)
    // and the acceleration keys (19/20 -> acceleration_val) to the per-zoom tiers.
    int* sSlot = (stage == 1) ? &g_senZoom1 : &g_senZoom2;
    int* aSlot = (stage == 1) ? &g_accZoom1 : &g_accZoom2;
    const int savedSens  = mg::readValue<int>(zs::SensVal);
    const int savedAccel = mg::readValue<int>(zs::AccelVal);
    if (*sSlot <= 0) *sSlot = savedSens;   // seed tiers from base on first use
    if (*aSlot < 0)  *aSlot = savedAccel;

    const int beforeS = *sSlot, beforeA = *aSlot;
    mg::writeValue<int>(zs::SensVal,  0, *sSlot);
    mg::writeValue<int>(zs::AccelVal, 0, *aSlot);
    original();                                  // ++/-- + clamp + HUD msg on the swapped-in tiers
    *sSlot = mg::readValue<int>(zs::SensVal);    // capture adjusted tiers
    *aSlot = mg::readValue<int>(zs::AccelVal);
    mg::writeValue<int>(zs::SensVal,  0, savedSens);
    mg::writeValue<int>(zs::AccelVal, 0, savedAccel);

    if (*sSlot != beforeS || *aSlot != beforeA)  // mirror stock client: rewrite config on in-game change
        persistZoom();
}

// ── option.ini path, rebuilt exactly like the game's COption code ────────────
// LoadCfg uses "%sconfig/%s/option.ini"; SaveCfg uses "%s/config/%s/option.ini".
// Read the COption base-path std::string the way the game's own _Myptr does, but
// INLINE — never calling through the StringMyptr import slot (0xF8B3E0). That slot
// is an IAT entry, not a function: the game does `call [0xF8B3E0]`, so calling it
// directly executed a pointer as code and faulted 0xC0000096. VC9 _SECURE_SCL
// layout (matches OldString): _Myproxy@+0, _Bx(union)@+4, _Mysize@+20, _Myres@+24;
// _Myptr == (_Myres >= 16 ? _Bx._Ptr : _Bx._Buf).
inline const char* readGameStringMyptr(uptr strObj)
{
    const u32  myres = *reinterpret_cast<u32*>(strObj + 24);   // capacity (_Myres)
    const uptr bx    = strObj + 4;                             // _Bx union
    return (myres >= 16) ? *reinterpret_cast<const char* const*>(bx)
                         : reinterpret_cast<const char*>(bx);
}

void buildIniPath(uptr optionThis, bool save, char* out, size_t cap)
{
    out[0] = '\0';
    const char* nation = nullptr;
    const char* base   = nullptr;
    // NationFolder is a real function (patched GetNationIndex); the base-path read is
    // inline (no import call). SEH only guards a bad COption pointer (a read fault,
    // which is recoverable — unlike the old call-into-data crash).
    __try {
        nation = mg::call<const char*(__cdecl*)()>(MG_CONST(zs::NationFolder));
        base   = readGameStringMyptr(optionThis + zs::OptionBasePath);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    if (!base || !nation || !base[0]) return;
    std::snprintf(out, cap, save ? "%s/config/%s/option.ini" : "%sconfig/%s/option.ini",
                  base, nation);
}

// Resolved option.ini path, cached on the first load/save (the load-format path
// is the file the game just successfully read, so it's the correct target). This
// lets in-battle changes rewrite the config immediately, matching the stock
// client, without needing the COption pointer at keybind time.
char g_iniPath[268] = {0};

void cacheIniPath(uptr optionThis)
{
    if (g_iniPath[0]) return;
    char path[268];
    buildIniPath(optionThis, /*save=*/false, path, sizeof(path));
    if (path[0])
        std::snprintf(g_iniPath, sizeof(g_iniPath), "%s", path);
}

// Writes only the tiers that are actually set, so a stale/unset global (sens 0 /
// accel -1) can never clobber the config with a bad value.
void persistZoom()
{
    if (!g_iniPath[0]) return;
    char buf[16];
    if (g_senZoom1 > 0)  { std::snprintf(buf, sizeof(buf), "%d", g_senZoom1); WritePrivateProfileStringA("Input", "MouseSenseZoom1", buf, g_iniPath); }
    if (g_senZoom2 > 0)  { std::snprintf(buf, sizeof(buf), "%d", g_senZoom2); WritePrivateProfileStringA("Input", "MouseSenseZoom2", buf, g_iniPath); }
    if (g_accZoom1 >= 0) { std::snprintf(buf, sizeof(buf), "%d", g_accZoom1); WritePrivateProfileStringA("Input", "MouseAccelFactorZoom1", buf, g_iniPath); }
    if (g_accZoom2 >= 0) { std::snprintf(buf, sizeof(buf), "%d", g_accZoom2); WritePrivateProfileStringA("Input", "MouseAccelFactorZoom2", buf, g_iniPath); }
    if (g_accelOn1 >= 0) WritePrivateProfileStringA("Input", "MouseAccelEnableZoom1", g_accelOn1 ? "1" : "0", g_iniPath);
    if (g_accelOn2 >= 0) WritePrivateProfileStringA("Input", "MouseAccelEnableZoom2", g_accelOn2 ? "1" : "0", g_iniPath);
    if (g_invert1  >= 0) WritePrivateProfileStringA("Input", "MouseInvertZoom1",      g_invert1  ? "1" : "0", g_iniPath);
    if (g_invert2  >= 0) WritePrivateProfileStringA("Input", "MouseInvertZoom2",      g_invert2  ? "1" : "0", g_iniPath);
}

// ── hkLoadCfg — wraps sub_40C9E0 (COption::LoadCfg) ──────────────────────────
char __fastcall hkLoadCfg(void* self, void* edx, char load, int which, int section)
{
    auto& registry = mg::ctx().hookRegistry();
    auto  original = registry.findDetour(HookId::ZoomSensLoadCfg)
        ->getOriginal<char(__fastcall*)(void*, void*, char, int, int)>();
    const char ret = original(self, edx, load, which, section);

    // Real file parsed (load!=0), user profile (which==0 -> option.ini), and the
    // [Input] section is in scope (section 7=all or 0=input) — matches LoadCfg.
    if (load && which == 0 && (section == 7 || section == 0)) {
        cacheIniPath(reinterpret_cast<uptr>(self));
        if (g_iniPath[0]) {
            const int defS = GetPrivateProfileIntA("Input", "MouseSense", 10, g_iniPath);
            const int defA = GetPrivateProfileIntA("Input", "MouseAccelFactor", 0, g_iniPath);
            g_senZoom1 = GetPrivateProfileIntA("Input", "MouseSenseZoom1", defS, g_iniPath);
            g_senZoom2 = GetPrivateProfileIntA("Input", "MouseSenseZoom2", defS, g_iniPath);
            g_accZoom1 = GetPrivateProfileIntA("Input", "MouseAccelFactorZoom1", defA, g_iniPath);
            g_accZoom2 = GetPrivateProfileIntA("Input", "MouseAccelFactorZoom2", defA, g_iniPath);
            // Per-tier booleans: -1 (absent) means "unset -> fall back to base".
            g_accelOn1 = GetPrivateProfileIntA("Input", "MouseAccelEnableZoom1", -1, g_iniPath);
            g_accelOn2 = GetPrivateProfileIntA("Input", "MouseAccelEnableZoom2", -1, g_iniPath);
            g_invert1  = GetPrivateProfileIntA("Input", "MouseInvertZoom1",      -1, g_iniPath);
            g_invert2  = GetPrivateProfileIntA("Input", "MouseInvertZoom2",      -1, g_iniPath);
            // Capture the base bools the stock LoadCfg just set from MouseAccel/Invert.
            g_accelOnBase = mg::readValue<u8>(zs::AccelOnVal) ? 1 : 0;
            g_invertBase  = mg::readValue<u8>(zs::InvertVal)  ? 1 : 0;
            g_rtStage = -1;
        }
    }
    return ret;
}

// ── hkSaveCfg — wraps sub_412840 (COption::SaveCfg) ──────────────────────────
char __fastcall hkSaveCfg(void* self, void* edx, char doWrite)
{
    auto& registry = mg::ctx().hookRegistry();
    auto  original = registry.findDetour(HookId::ZoomSensSaveCfg)
        ->getOriginal<char(__fastcall*)(void*, void*, char)>();
    const char ret = original(self, edx, doWrite);
    if (doWrite) {
        cacheIniPath(reinterpret_cast<uptr>(self));
        persistZoom();
    }
    return ret;
}

// ── Options-menu (Control tab) per-zoom integration ──────────────────────────
// Mirrors the Steam client: a single Normal/Rifle/Sniper combo (IdZoomCombo, added
// to SCENE_COMMON.xml) picks the tier, and the STOCK mouse sens/accel sliders edit
// whichever tier is selected. Components resolve via sub_8B4600(dlg, &id) -> *p;
// slider value at component+SliderValueField; combo selection at component+ComboCurSelField.
//
//   tier 0 = Normal -> senitivity_val / acceleration_val (the base globals)
//   tier 1 = Rifle  -> g_senZoom1 / g_accZoom1
//   tier 2 = Sniper -> g_senZoom2 / g_accZoom2
uptr findComponent(uptr dlg, int id)
{
    int local = id;
    int* p = mg::call<int*(__thiscall*)(void*, int*)>(
        MG_CONST(zs::DlgFindComponent),
        reinterpret_cast<void*>(dlg + zs::DlgComponentMap), &local);
    return (p && *p) ? static_cast<uptr>(static_cast<unsigned>(*p)) : 0;
}

int getSliderValue(uptr dlg, int id)
{
    const uptr comp = findComponent(dlg, id);
    return comp ? mg::readValue<int>(comp, zs::SliderValueField) : -1;
}

void setSliderValue(uptr dlg, int id, int value)
{
    const uptr comp = findComponent(dlg, id);
    if (comp)
        mg::call<int(__thiscall*)(uptr, int, char)>(MG_CONST(zs::SliderSetValue), comp, value, 0);
}

void setLabelNumber(uptr dlg, int id, int value)
{
    const uptr comp = findComponent(dlg, id);
    if (comp)
        mg::callVFunc<void>(reinterpret_cast<void*>(comp), zs::LabelSetTextVIdx, value);
}

// ── Radio helpers (the per-tier accel-on / invert booleans) ──────────────────
// Checking a radio (vtbl byte+236, args 1,1,0) selects it and clears its group.
void selectRadio(uptr dlg, int id)
{
    const uptr comp = findComponent(dlg, id);
    if (!comp) return;
    using Fn = void(__thiscall*)(uptr, int, int, int);
    Fn fn = *reinterpret_cast<Fn*>(*reinterpret_cast<uptr*>(comp) + zs::RadioSetCheckedOff);
    fn(comp, 1, 1, 0);
}
int radioChecked(uptr dlg, int id)  // 1/0, or -1 if the control is missing
{
    const uptr comp = findComponent(dlg, id);
    return comp ? (mg::readValue<u8>(comp, zs::RadioCheckedField) ? 1 : 0) : -1;
}

// ── ComboBox helpers (thiscall on the combo component) ───────────────────────
int  comboItemCount(uptr combo) { return mg::call<int(__thiscall*)(uptr)>(MG_CONST(zs::ComboGetCount), combo); }
void comboReset(uptr combo)     { mg::call<int(__thiscall*)(uptr)>(MG_CONST(zs::ComboReset), combo); }
// AddItem is __thiscall(combo, text, 0, data) — sub_E92380 is just a thiscall wrapper
// over this, so calling sub_E923A0 directly is equivalent. Must pass the combo as `this`.
void comboAddItem(uptr combo, const wchar_t* text)
{
    mg::call<int(__thiscall*)(uptr, const wchar_t*, int, int)>(MG_CONST(zs::ComboAddItem), combo, text, 0, 0);
}
int  comboGetSel(uptr combo) { return mg::readValue<int>(combo, zs::ComboCurSelField); }
void comboSetSel(uptr combo, int idx)
{
    mg::call<int(__thiscall*)(uptr, int, char)>(MG_CONST(zs::ComboSetCurSel), combo, idx, 1);
}

// ── Per-tier dialog state ────────────────────────────────────────────────────
int  g_curTier  = 0;   // tier currently shown by the shared sliders
uptr g_optDlg   = 0;   // option dialog (E_DLG_OPTION) while open
uptr g_optCombo = 0;   // the IdZoomCombo component while open
bool g_suppress = false; // true while WE drive the combo (so our own SetCurSel's 513 event is ignored)

inline int clampTier(int t) { return (t < 0 || t > 2) ? 0 : t; }

// tier -> shared sliders/labels. Unset tiers (sens<=0 / accel<0) fall back to base.
void loadTierToSliders(uptr dlg, int tier)
{
    const int base    = mg::readValue<int>(zs::SensVal);
    const int baseAcc = mg::readValue<int>(zs::AccelVal);
    const int s = (tier == 1) ? (g_senZoom1 > 0 ? g_senZoom1 : base)
                : (tier == 2) ? (g_senZoom2 > 0 ? g_senZoom2 : base)
                              : base;
    const int a = (tier == 1) ? (g_accZoom1 >= 0 ? g_accZoom1 : baseAcc)
                : (tier == 2) ? (g_accZoom2 >= 0 ? g_accZoom2 : baseAcc)
                              : baseAcc;
    setSliderValue(dlg, zs::IdSensSlider,      s);
    setLabelNumber(dlg, zs::IdSensValueLabel,  s);
    setSliderValue(dlg, zs::IdAccelSlider,     a);
    setLabelNumber(dlg, zs::IdAccelValueLabel, a);

    // per-tier booleans -> the accel-on / invert radios
    if (g_accelOnBase < 0) g_accelOnBase = mg::readValue<u8>(zs::AccelOnVal) ? 1 : 0;
    if (g_invertBase  < 0) g_invertBase  = mg::readValue<u8>(zs::InvertVal)  ? 1 : 0;
    selectRadio(dlg, effAccelOn(tier) ? zs::IdAccelOnRadio  : zs::IdAccelOffRadio);
    selectRadio(dlg, effInvert(tier)  ? zs::IdInvertOnRadio : zs::IdInvertOffRadio);
}

// shared sliders -> tier globals (captures edits to the tier we're leaving).
void saveSlidersToTier(uptr dlg, int tier)
{
    const int s = getSliderValue(dlg, zs::IdSensSlider);
    int       a = getSliderValue(dlg, zs::IdAccelSlider);
    if (s < 0) return;            // component missing — nothing to capture
    if (a < 0) a = 0;
    const int ao = radioChecked(dlg, zs::IdAccelOnRadio);   // -1 if missing
    const int iv = radioChecked(dlg, zs::IdInvertOnRadio);
    if (tier == 1)      { g_senZoom1 = s; g_accZoom1 = a; if (ao >= 0) g_accelOn1 = ao; if (iv >= 0) g_invert1 = iv; }
    else if (tier == 2) { g_senZoom2 = s; g_accZoom2 = a; if (ao >= 0) g_accelOn2 = ao; if (iv >= 0) g_invert2 = iv; }
    else {
        mg::writeValue<int>(zs::SensVal, 0, s); mg::writeValue<int>(zs::AccelVal, 0, a);
        if (ao >= 0) g_accelOnBase = ao;
        if (iv >= 0) g_invertBase  = iv;
    }
}

// ── hkComboEvent — wraps the control event dispatch (sub_E74FC0) ──────────────
// Fires for EVERY control event; we act only on msg 513 (selection-change) for our
// zoom combo. This catches a real dropdown-row click (which never calls SetCurSel),
// so switching Normal/Rifle/Sniper live-swaps the shared sliders to that tier.
void __fastcall hkComboEvent(void* self, void* /*edx*/, int msg, char flag, int source)
{
    auto& registry = mg::ctx().hookRegistry();
    auto  original = registry.findDetour(HookId::ZoomSensComboChange)
        ->getOriginal<void(__fastcall*)(void*, void*, int, char, int)>();
    original(self, nullptr, msg, flag, source);

    if (g_suppress || msg != zs::ComboSelChangeMsg || !g_optDlg)
        return;
    const uptr combo = static_cast<uptr>(static_cast<unsigned>(source));
    if (!combo || combo != g_optCombo)
        return;

    const int idx = comboGetSel(combo);
    if (idx >= 0 && idx <= 2 && idx != g_curTier) {
        saveSlidersToTier(g_optDlg, g_curTier);   // keep edits to the tier we leave
        g_curTier = idx;
        loadTierToSliders(g_optDlg, idx);
    }
}

// ── hkComboRegister — wraps the combo-controller registry init (sub_5AB010) ───
// The binary registers a controller binding for every native combo here but skips
// ours (it has no CON combo). We append the identical registration so our combo
// gets the same binding the stock combos do — the missing per-combo setup that lets
// the dropdown popup actually render. Mirrors one entry of sub_5AB010 / sub_5AD380.
void __fastcall hkComboRegister(void* self, void* /*edx*/)
{
    auto& registry = mg::ctx().hookRegistry();
    auto  original = registry.findDetour(HookId::ZoomSensComboRegister)
        ->getOriginal<void(__fastcall*)(void*, void*)>();
    original(self, nullptr);                               // register all native combos first

    const int id = zs::IdZoomCombo;                       // 103400
    const int mgr1 = mg::call<int(__cdecl*)()>(MG_CONST(zs::UiFactoryRegistry));
    mg::call<void(__thiscall*)(int, void*, uptr)>(
        MG_CONST(zs::ComboRegFactory), mgr1, reinterpret_cast<void*>(id), MG_CONST(zs::ComboFactoryFn));
    const int mgr2 = mg::call<int(__cdecl*)()>(MG_CONST(zs::UiFactoryRegistry));
    const int binding = mg::call<int(__thiscall*)(int, int)>(MG_CONST(zs::ComboMakeBinding), mgr2, id);
    mg::call<void(__thiscall*)(void*, const char*, int)>(
        MG_CONST(zs::ComboBindNodename), self, "E_DLG_OPTION_CBX_CON_OP01_0", binding);
}

// ── hkComboClick — wraps the combo mouse handler (sub_E90920) ─────────────────
// For OUR zoom combo we turn a left-click into "advance to the next tier" instead
// of opening the (unrenderable) dropdown. The cycle goes through SetCurSel, which
// updates the header text (renders fine) and fires the 513 event that hkComboEvent
// uses to live-swap the shared sliders. All other combos pass straight through.
char __fastcall hkComboClick(void* self, void* edx, int msg, int ptx, int pty, int a4, int a5)
{
    auto& registry = mg::ctx().hookRegistry();
    auto  original = registry.findDetour(HookId::ZoomSensComboClick)
        ->getOriginal<char(__fastcall*)(void*, void*, int, int, int, int, int)>();

    const uptr combo = reinterpret_cast<uptr>(self);
    if (combo && combo == g_optCombo) {
        if (msg == zs::MsgLBtnUp) {
            const int n = comboItemCount(combo);
            if (n > 0)
                comboSetSel(combo, (comboGetSel(combo) + 1) % n);  // header + 513 -> slider reload
            return 1;                                              // consume; never open the popup
        }
        if (msg == zs::MsgLBtnDown)
            return 1;                                              // consume so no capture/open setup
    }
    return original(self, edx, msg, ptx, pty, a4, a5);
}

// ── hkOptionPopulate — wraps sub_64D890 (globals -> dialog widgets, on open) ───
void __fastcall hkOptionPopulate(void* self, void* /*edx*/, int tab)
{
    auto& registry = mg::ctx().hookRegistry();
    auto  original = registry.findDetour(HookId::ZoomSensOptionPopulate)
        ->getOriginal<void(__fastcall*)(void*, void*, int)>();
    original(self, nullptr, tab);

    if (tab != zs::OptionTabControl && tab != zs::OptionTabAll)
        return;

    const uptr dlg   = reinterpret_cast<uptr>(self);
    const uptr combo = findComponent(dlg, zs::IdZoomCombo);
    if (!combo)
        return;

    g_optDlg   = dlg;
    g_optCombo = combo;

    // Suppress our event handler while WE mutate the combo (item add + initial select),
    // so the 513 events those fire don't trigger a tier save/reload mid-setup.
    g_suppress = true;
    if (comboItemCount(combo) == 0) {
        comboReset(combo);
        comboAddItem(combo, L"Normal");
        comboAddItem(combo, L"Rifle Zoom");
        comboAddItem(combo, L"Sniper Zoom");
    }
    const int tier = clampTier(g_curTier);
    comboSetSel(combo, tier);                // restore last-used tier
    g_suppress = false;

    g_curTier = tier;
    loadTierToSliders(dlg, tier);            // override the base values the stock populate just set
}

// ── hkOptionApply — wraps sub_64F470 (dialog widgets -> globals, on Apply) ─────
void __fastcall hkOptionApply(void* self, void* /*edx*/, int tab)
{
    auto& registry = mg::ctx().hookRegistry();
    auto  original = registry.findDetour(HookId::ZoomSensOptionApply)
        ->getOriginal<void(__fastcall*)(void*, void*, int)>();

    const uptr dlg   = reinterpret_cast<uptr>(self);
    const uptr combo = findComponent(dlg, zs::IdZoomCombo);
    const int  tier  = combo ? clampTier(comboGetSel(combo)) : clampTier(g_curTier);

    // The stock apply blindly writes the shared sliders into senitivity_val/
    // acceleration_val. Save the true base so a non-Normal tier can't clobber it.
    const int savedBaseSens = mg::readValue<int>(zs::SensVal);
    const int savedBaseAcc  = mg::readValue<int>(zs::AccelVal);

    original(self, nullptr, tab);   // sens/accel sliders -> senitivity_val/acceleration_val

    if (tab != zs::OptionTabControl && tab != zs::OptionTabAll)
        return;

    const int shownSens = mg::readValue<int>(zs::SensVal);
    const int shownAcc  = mg::readValue<int>(zs::AccelVal);
    const int shownAOn  = mg::readValue<u8>(zs::AccelOnVal) ? 1 : 0;  // stock apply set these from the radios
    const int shownInv  = mg::readValue<u8>(zs::InvertVal)  ? 1 : 0;
    if (g_accelOnBase < 0) g_accelOnBase = shownAOn;
    if (g_invertBase  < 0) g_invertBase  = shownInv;

    if (tier == 1) {
        g_senZoom1 = shownSens; g_accZoom1 = shownAcc; g_accelOn1 = shownAOn; g_invert1 = shownInv;
        mg::writeValue<int>(zs::SensVal,  0, savedBaseSens);
        mg::writeValue<int>(zs::AccelVal, 0, savedBaseAcc);
        mg::writeValue<u8>(zs::AccelOnVal, 0, static_cast<u8>(g_accelOnBase));
        mg::writeValue<u8>(zs::InvertVal,  0, static_cast<u8>(g_invertBase));
    } else if (tier == 2) {
        g_senZoom2 = shownSens; g_accZoom2 = shownAcc; g_accelOn2 = shownAOn; g_invert2 = shownInv;
        mg::writeValue<int>(zs::SensVal,  0, savedBaseSens);
        mg::writeValue<int>(zs::AccelVal, 0, savedBaseAcc);
        mg::writeValue<u8>(zs::AccelOnVal, 0, static_cast<u8>(g_accelOnBase));
        mg::writeValue<u8>(zs::InvertVal,  0, static_cast<u8>(g_invertBase));
    } else {
        g_accelOnBase = shownAOn; g_invertBase = shownInv;   // Normal tier == the base
    }
    g_rtStage = -1;   // force the runtime to re-apply the active stage's bools next look-frame

    persistZoom();   // menu Apply also calls SaveCfg, but persist here too so it never lags
}

} // anonymous namespace

// ── ZoomSensitivity class ─────────────────────────────────────────────────────

ZoomSensitivity::ZoomSensitivity(MegaGuardContext& ctx) : ctx_(ctx) {}
ZoomSensitivity::~ZoomSensitivity() = default;

VoidResult ZoomSensitivity::install()
{
    auto& registry = ctx_.hookRegistry();
    // Runtime zoom hooks re-enabled — now load-safe: hkSensMultiplier/hkBattleAdjust
    // gate on inMatch() so the camera walk never runs during the cold match-load
    // (that load-time per-frame work was the first-Single-Wave black screen).
    registry.registerDetour(HookId::ZoomSensMultiplier)
        .create(MG_CONST(zs::SensMultiplier), hkSensMultiplier);
    registry.registerDetour(HookId::ZoomSensBattleAdjust)
        .create(MG_CONST(zs::BattleAdjust), hkBattleAdjust);
    registry.registerDetour(HookId::ZoomSensLoadCfg)
        .create(MG_CONST(zs::LoadCfg), hkLoadCfg);
    registry.registerDetour(HookId::ZoomSensSaveCfg)
        .create(MG_CONST(zs::SaveCfg), hkSaveCfg);
    registry.registerDetour(HookId::ZoomSensOptionPopulate)
        .create(MG_CONST(zs::OptionPopulate), hkOptionPopulate);
    registry.registerDetour(HookId::ZoomSensOptionApply)
        .create(MG_CONST(zs::OptionApply), hkOptionApply);
    // Control event dispatch — global, but our handler only acts on msg 513 for the
    // zoom combo. Live-refreshes the shared sliders when the user switches the tier.
    registry.registerDetour(HookId::ZoomSensComboChange)
        .create(MG_CONST(zs::ComboEventDispatch), hkComboEvent);
    // Combo-controller registration — give our combo the same binding native combos
    // get (the binary skips it), so the dropdown popup initializes and renders.
    registry.registerDetour(HookId::ZoomSensComboRegister)
        .create(MG_CONST(zs::ComboRegisterAll), hkComboRegister);

    // Combo mouse handler — click-to-cycle fallback. DISABLED while we test whether the
    // registration above makes the native dropdown render. Re-enable if it doesn't.
    // registry.registerDetour(HookId::ZoomSensComboClick)
    //     .create(MG_CONST(zs::ComboMouseHandler), hkComboClick);
    return VoidResult::ok();
}

} // namespace mg::modding
