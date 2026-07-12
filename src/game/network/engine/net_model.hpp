// =============================================================================
// Network engine swap — CEventSelectModel (ISensor)  [1:1 reimplementation]
// =============================================================================
// Faithful rewrite of libnetengine/src/EventSelectModel.cpp, reversed from
// MicroVolts.exe.i64. This is the default net model (SENSOR_TYPE 1). It owns the
// WSAEventSelect registration table + parallel event handles, and its Select()
// method is the recv "wait loop" — polled (timeout≈0) each frame by the engine's
// recv pump rather than from a dedicated thread.
// Install hooks are written but left COMMENTED OUT (see InstallNetModelEngine).
// =============================================================================
#pragma once

namespace mg::game::net {

void InstallNetModelEngine();

} // namespace mg::game::net
