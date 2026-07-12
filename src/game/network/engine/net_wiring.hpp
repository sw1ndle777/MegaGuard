// =============================================================================
// Network engine swap — Connect path, recv thread, CNetMgr wiring  [1:1]
// =============================================================================
// The glue tier: CTcpConnector::Connect, CNetEngine::Create/RegisterConnector,
// the NetengineWait recv thread (CWaitProcedure::Run) and the per-frame
// NetFrameTick pump mirror. Reversed from MicroVolts.exe.i64; install hooks are
// written but left COMMENTED OUT (see InstallNetWiring).
// =============================================================================
#pragma once

namespace mg::game::net {

void InstallNetWiring();

} // namespace mg::game::net
