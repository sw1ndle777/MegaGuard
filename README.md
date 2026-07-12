# MegaGuard

MegaVolts's anticheat usermode - a mapped x86 payload that protects the game client, hardens
its login/security flows, and fixes/extends the original game at runtime.

## Language & build

- **C++23**.
- manually mapped into the game process by anticheat bootstrapper.

## Dependencies

- [CloakWork](https://github.com/ck0i/Cloakwork) - compile-time string / address obfuscation (using a locally modified fork)
- [StackWalker](https://github.com/JochenKalmbach/StackWalker)
- [fmt](https://github.com/fmtlib/fmt)
- [simdjson](https://github.com/simdjson/simdjson)
- [boost.unordered](https://github.com/boostorg/unordered)
- [asmjit](https://github.com/asmjit/asmjit)
- [asmtk](https://github.com/asmjit/asmtk)
- [minhook](https://github.com/TsudaKageyu/minhook)
- [polyhook2](https://github.com/stevemk14ebr/PolyHook_2_0)
- [Zycore](https://github.com/zyantific/zycore-c)
- [Zydis](https://github.com/zyantific/zydis)

## What it does

### Anti-cheat & detection

A heartbeat-driven detection engine runs a suite of scanners and reports findings
back to the server. Scanners cover, among others:

- Injection & manual-mapping (`injection`, `manual_map`, `section_remap`, `proxy_dll`)
- Hook integrity - inline, IAT, and generic hook tampering (`inline_hook`, `iat_hook`,
  `hook_integrity`)
- Module trust - unsigned / PEB-hidden modules, vulnerable-driver presence
  (`unsigned_module`, `peb_module`, `vulnerable_driver`)
- Anti-debug, anonymous threads, handle watching, NT API monitoring
- Image integrity, signature/string scanning

The **heartbeat** doubles as a liveness/tamper signal: the server expects periodic,
well-formed reports, so silencing or stalling the guard is itself detectable.

### Runtime reverse-engineering hardening

- **Encrypted singleton pointers.** The game's manager singletons are not left as
  plain globals. MegaGuard initializes them itself and stores their pointers through
  a per-instance `PointerEncryption` layer (`src/engine/pointer_encryption.*`) keyed
  off the PEB, module base, and random keys (no static state) so scraping a fixed
  pointer or a static offset out of memory no longer yields a usable manager.
- **Compile-time obfuscation** of strings and addresses via the modified CloakWork,
  so sensitive constants never appear in the binary in the clear.
- **Syscall cloner** (`src/platform/syscall_cloner.*`) - a custom-built ntdll cloner
  that reads the raw `ntdll.dll` straight from disk (bypassing any in-memory hooks
  placed by cheats or injectors), disassembles each target function instruction by
  instruction using Zydis, relocates absolute addresses from the PE image base to the
  live ntdll base, and copies the clean stub into a private RWX memory pool. The
  result is a set of callable NT syscall pointers that are guaranteed unhook-clean,
  which MegaGuard uses internally for its own API calls so that usermode hooks on
  `ntdll` cannot intercept or tamper with the anticheat's system calls.

### Custom loading splash

A GDI+ loading splash screen (`src/utils/splash.*`) is shown during startup and
driven by real initialization progress: because MegaGuard initializes the game
managers itself, the splash advances as each manager becomes ready (`kTotalManagers`)
and reports completion over a cross-process IPC channel (`MegaGuardSplashIPC`). This
replaces the game's original startup experience and hides the manager bring-up /
pointer-encryption work happening underneath.

### Security-flow modding (2FA)

The login/authorize path is extended with a custom **two-factor (2FA) UI and
protocol** - a dedicated code-entry popup and a retry request that carries the auth
keys plus the entered code (`src/game/network/authorize_handler.cpp`,
`src/game/ui/dialog/popups/input_pop.cpp`), with explicit disconnect handling on 2FA
failure.

### Original-client bugfixes

Runtime fixes for long-standing client bugs live in `src/modding/bugfixes/`, e.g.
screenshot handling, and weapon-restriction correctness.

### New features & gameplay improvements

Added/ported client features under `src/modding/features/`, including custom tick
rate, HUD toggle, spectate POV, per-zoom mouse
sensitivity, resolution options, PC-bang handling, and more.
