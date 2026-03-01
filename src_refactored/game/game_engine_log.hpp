// =============================================================================
// GameEngineLog — Wrapper for the game engine's three log subsystems
// =============================================================================
// SysLog  → Syslog.txt   (instance at 0x011F1060)
// ErrLog  → ErrLog.txt   (instance at 0x011F1064)
// GameLog → GameLog.txt  (instance at 0x011D6098)
//
// All calls forward to the game's own LogFile / GameLogger functions.
// Thread-safe: the underlying game functions use CRITICAL_SECTIONs.
// =============================================================================
#pragma once

#include "core/types.hpp"
#include <cstdarg>

namespace mg::game {

class GameEngineLog {
public:
    // ── SysLog (Syslog.txt) ──────────────────────────────────────────────
    static void sys(const char* fmt, ...);

    // ── ErrLog (ErrLog.txt) ──────────────────────────────────────────────
    static void err(const char* fmt, ...);

    // Error log with file(line) : message\n  (matches original debug macro)
    static void errLine(const char* file, int line, const char* msg);

    // Error log with file(line) : formatted message\n
    static void errLineFmt(const char* file, int line, const char* fmt, ...);

    // ── GameLog (GameLog.txt) ────────────────────────────────────────────
    // level: 0 = Info, 1 = Warning, 2 = Error
    static void game(int a2, int a3, int level, const char* fmt, ...);
};

} // namespace mg::game
