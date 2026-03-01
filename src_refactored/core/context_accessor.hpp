// =============================================================================
// Context Accessor — encrypted runtime access to MegaGuardContext
// =============================================================================
// Hook callbacks are C-style function pointers and cannot carry state.
// This module provides a SINGLE encrypted access point to MegaGuardContext.
//
// The context pointer is stored in a VirtualAlloc'd block (NOT the DLL's .data
// section), XOR'd with a runtime entropy key. This makes it invisible to
// static analysis of the DLL image.
// =============================================================================
#pragma once

namespace mg {

class MegaGuardContext;

/// Call once from entry point after constructing the context
void initContextAccessor(MegaGuardContext* ctx);

/// Retrieve the context from any hook callback
/// Decrypts on every call — no plain pointer in memory
MegaGuardContext& ctx();

/// Clean up the accessor (call during DLL_PROCESS_DETACH)
void destroyContextAccessor();

} // namespace mg
