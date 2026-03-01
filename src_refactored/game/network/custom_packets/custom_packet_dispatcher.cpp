// =============================================================================
// CustomPacketDispatcher - Implementation
// =============================================================================
// Hooks PacketsCallbackDistribution(__stdcall) to intercept custom packet IDs.
// Extracts order via (*a1 >> 6) & 0x3FF, checks custom handler array first,
// falls through to original if no custom handler is registered.
// =============================================================================
#include "pch.hpp"
#include "game/network/custom_packets/custom_packet_dispatcher.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "utils/logger.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::game {

namespace {

// ── Custom handler table ─────────────────────────────────────────────────────
// Max 504 entries to match game's callbackList bounds check
inline PacketHandler g_customHandlers[504] = {};

// ── Hook ─────────────────────────────────────────────────────────────────────

char __stdcall hkPacketsCallbackDistribution(u16* a1)
{
    u32 order = ((*a1 >> 6) & 0x3FF);

    // Check custom handlers first for valid non-zero order within bounds
    if (order != 0 && order < 504 && g_customHandlers[order])
    {
        return g_customHandlers[order](a1);
    }

    // Fall through to original game handler
    auto& registry = mg::ctx().hookRegistry();
    auto original = registry.findDetour(HookId::CustomPacketDispatcher)
        ->getOriginal<decltype(&hkPacketsCallbackDistribution)>();
    return original(a1);
}

} // anonymous namespace

// ── Public API ───────────────────────────────────────────────────────────────

CustomPacketDispatcher::CustomPacketDispatcher(::mg::MegaGuardContext& ctx)
    : ctx_(ctx)
{
}

CustomPacketDispatcher::~CustomPacketDispatcher() = default;

VoidResult CustomPacketDispatcher::install()
{
    auto& registry = ctx_.hookRegistry();
    auto& logger   = ctx_.logger();

    registry.registerDetour(HookId::CustomPacketDispatcher)
        .create(MG_CONST(addr::ui::custom_packets::PacketsCallbackDistribution),
                &hkPacketsCallbackDistribution);
    logger.info("CustomPacketDispatcher: installed");
    return VoidResult::ok();
}

void CustomPacketDispatcher::registerHandler(u32 packetId, PacketHandler handler)
{
    if (packetId > 0 && packetId < 504)
    {
        g_customHandlers[packetId] = handler;
        ctx_.logger().info("CustomPacketDispatcher: registered handler for packet {}", packetId);
    }
}

} // namespace mg::game
