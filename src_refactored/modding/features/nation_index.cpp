// =============================================================================
// NationIndex - Custom nation index + window title hooks
// =============================================================================
// Ported from custom_nationindex.h
// Two simple hooks that replace the game's GetNationIndex and GetWindowTitle
// functions with custom return values.
// =============================================================================
#include "pch.hpp"
#include "modding/features/nation_index.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::modding {

using namespace mg::game;

namespace {

// ── GetNationIndex hook ──────────────────────────────────────────────────
// Returns "ROM" if the nation index was detected as Romanian (20) at startup,
// otherwise returns empty string.

const char* GetNationIndex()
{
    return addr::config::NationIndexIsRom ? "ROM" : "";
}

// ── GetWindowTitle hook ──────────────────────────────────────────────────────

const char* GetWindowTitle()
{
    return "MegaVolts Online";
}

} // anonymous namespace

// ── NationIndex class ────────────────────────────────────────────────────────

NationIndex::NationIndex(MegaGuardContext& ctx) : ctx_(ctx) {}
NationIndex::~NationIndex() = default;

VoidResult NationIndex::install()
{
    auto& registry = ctx_.hookRegistry();

    registry.registerDetour(HookId::NationIndex)
        .create(MG_CONST(addr::features::Custom_GetNationIndex), &GetNationIndex);

    registry.registerDetour(HookId::WindowTitle)
        .create(MG_CONST(addr::features::Custom_GetWindowTitle), &GetWindowTitle);

    return VoidResult::ok();
}

} // namespace mg::modding
