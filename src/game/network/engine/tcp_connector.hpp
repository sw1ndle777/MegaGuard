// =============================================================================
// Network engine swap — CConnector / CTcpConnector layer (1:1 reimplementation)
// =============================================================================
// Faithful rewrite of the client's CTcpConnector send/recv path
// (libnetengine/src/ConnectorTcp.cpp), reversed from MicroVolts.exe.i64.
// Install hooks are written but left COMMENTED OUT (see InstallTcpConnectorEngine).
// Ground truth: IDA; AutoFramework.cs is reference only.
// =============================================================================
#pragma once

namespace mg::game::net {

void InstallTcpConnectorEngine();

} // namespace mg::game::net
