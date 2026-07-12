// =============================================================================
// Network engine swap — CDispatcher / CDispatcherArray  [1:1 reimplementation]
// =============================================================================
// Faithful rewrite of libnetengine/src/Dispatcher.cpp + DispatcherArray.cpp,
// reversed from MicroVolts.exe.i64. A CDispatcher owns one main connector plus an
// optional array of sub-connectors; it is driven each frame by the net model:
//   * OnRead  — pulled by CEventSelectModel::Select on a readable socket.
//   * Process — pulled by CDispatcherArray::Run (the per-frame send/keepalive pump).
// Install hooks are written but left COMMENTED OUT (see InstallDispatcherEngine).
// =============================================================================
#pragma once

namespace mg::game::net {

void InstallDispatcherEngine();

} // namespace mg::game::net
