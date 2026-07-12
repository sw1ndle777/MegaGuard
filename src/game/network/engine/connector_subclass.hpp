// =============================================================================
// Network engine swap — CTcpConnector ctor + Main/Cast subclasses  [1:1]
// =============================================================================
// Base CTcpConnector ctor (socket + 0xC0000 recv buffer), the CMain/CCast
// connector subclass ctors, and the shared base socket teardown. Reversed from
// MicroVolts.exe.i64; install hooks written but left COMMENTED OUT.
// =============================================================================
#pragma once

namespace mg::game::net {

void InstallConnectorSubclasses();

} // namespace mg::game::net
