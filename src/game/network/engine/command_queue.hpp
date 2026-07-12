// =============================================================================
// Network engine swap — ICommandQueue (Extend + Native)  [1:1 reimplementation]
// =============================================================================
// Faithful rewrite of libnetengine/src/ExCommandQueue.cpp + CommandQueue.cpp,
// reversed from MicroVolts.exe.i64. The queue is the hand-off between the recv
// path (CConnector::Queuing -> Put) and the per-frame drain that routes commands
// to handlers (game-layer PacketsCallbackDistribution).
// Install hooks are written but left COMMENTED OUT (see InstallCommandQueueEngine).
// =============================================================================
#pragma once

namespace mg::game::net {

void InstallCommandQueueEngine();

} // namespace mg::game::net
