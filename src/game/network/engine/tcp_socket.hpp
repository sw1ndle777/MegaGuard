// =============================================================================
// Network engine swap — CTcpSocket layer (1:1 reimplementation)
// =============================================================================
// Faithful rewrite of the client's CTcpSocket : CRawSocket methods
// (libnetengine/src/TcpSocket.cpp), reversed from MicroVolts.exe.i64.
//
// These are standalone reimplementations intended to *replace* the originals
// via MinHook detours. The install routine below writes every detour but leaves
// the .create(...) calls COMMENTED OUT — nothing is hooked yet. This is the
// staging step toward swapping the whole net engine (later: asio / Cast-UDP /
// larger packet limits). Ground truth is IDA; AutoFramework.cs is reference only.
// =============================================================================
#pragma once

namespace mg::game::net {

// Registers (commented-out) detours for the reimplemented CTcpSocket methods.
// Currently a no-op: enabling is intentionally left to the engine wiring step.
void InstallTcpSocketEngine();

} // namespace mg::game::net
