// =============================================================================
// GameEngineLog — Implementation
// =============================================================================
#include "pch.hpp"
#include "game/game_engine_log.hpp"
#include "game/addresses.hpp"
#include "utils/cloakwork_isolation.hpp"

namespace mg::game {

namespace {

// ── Function types ───────────────────────────────────────────────────────────

// void __cdecl LogFile(int logInstance, char* Format, ...)
using tLogFile = void(__cdecl*)(int, const char*, ...);

// void __cdecl GameLogger(int logInstance, int a2, int a3, int level, char* Format, ...)
using tGameLogger = void(__cdecl*)(int, int, int, int, const char*, ...);

// ── Helpers ──────────────────────────────────────────────────────────────────

inline tLogFile getLogFile() {
    return reinterpret_cast<tLogFile>(MG_CONST(addr::engine_log::LogFile));
}

// The addresses 0x011F1060/0x011F1064/0x011D6098 are pointers TO the
// log structures, not the structures themselves (they are only 4 bytes
// apart).  LogFile() expects the dereferenced value.
inline int readLogInst(uptr addr) {
    return *reinterpret_cast<int*>(addr);
}

} // anonymous namespace

// ── SysLog ───────────────────────────────────────────────────────────────────

void GameEngineLog::sys(const char* fmt, ...) {
    char buf[2056]{};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);

    int inst = readLogInst(MG_CONST(addr::engine_log::SysLogInstance));
    if (inst) getLogFile()(inst, "%s", buf);
}

// ── ErrLog ───────────────────────────────────────────────────────────────────

void GameEngineLog::err(const char* fmt, ...) {
    char buf[2056]{};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);

    int inst = readLogInst(MG_CONST(addr::engine_log::ErrLogInstance));
    if (inst) getLogFile()(inst, "%s", buf);
}

void GameEngineLog::errLine(const char* file, int line, const char* msg) {
    int inst = readLogInst(MG_CONST(addr::engine_log::ErrLogInstance));
    if (!inst) return;

    auto logFile = getLogFile();
    logFile(inst, "%s", file);
    logFile(inst, "%s", "(");
    logFile(inst, "%d", line);
    logFile(inst, "%s", ") : ");
    logFile(inst, "%s", msg);
    logFile(inst, "%s", "\n");
}

void GameEngineLog::errLineFmt(const char* file, int line, const char* fmt, ...) {
    char buf[2056]{};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);

    errLine(file, line, buf);
}

// ── GameLog ──────────────────────────────────────────────────────────────────

void GameEngineLog::game(int a2, int a3, int level, const char* fmt, ...) {
    char buf[2056]{};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);

    int inst = readLogInst(MG_CONST(addr::engine_log::GameLogInstance));
    if (!inst) return;

    auto gameLogger = reinterpret_cast<tGameLogger>(MG_CONST(addr::engine_log::GameLogger));
    gameLogger(inst, a2, a3, level, "%s", buf);
}

} // namespace mg::game
