// =============================================================================
// Network engine swap — CNetEngine + per-frame pump  [1:1 reimplementation]
// =============================================================================
// Faithful rewrite of libnetengine/src/NetEngine.cpp, reversed from
// MicroVolts.exe.i64. CNetEngine ties the command queue and the net model
// together. It is NOT threaded: the game loop (CNetMgr::NetFrameTick) drives the
// engine each frame —
//   * SendPump (vtbl[6]) -> model->Run  -> dispatchers' send/keepalive pump
//   * RecvPump (vtbl[4]) -> model->Select(0) — the WSAWaitForMultipleEvents loop
//   * the game-layer drain (DrainAndRoute) -> cmdQueue.Get -> PacketsCallbackDistribution
// Install hooks are written but left COMMENTED OUT (see InstallNetEngine).
// =============================================================================
#pragma once

namespace mg::game::net {

void InstallNetEngine();

} // namespace mg::game::net
