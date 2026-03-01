// =============================================================================
// MegaGuard - Forward Declarations
// =============================================================================
// Public header: intentionally avoids heavy includes so other modules can
// forward-declare without pulling in the world.
// =============================================================================
#pragma once

#include <cstdint>

namespace mg {

// core/
class MegaGuardContext;
enum class ErrorCode : std::uint32_t;

// platform/
class MemoryPool;
class ImportResolver;
class SyscallCloner;
class IpcClient;

// engine/
class DetourHook;
class SwapAddressPatch;
class PatchBytes;
class MidHook;
class Assembler;
class PatternScanner;
class PointerEncryption;
class HookRegistry;

// anticheat/
class AntiDebugEngine;
class IntegrityEngine;
class IatScrubber;
class ControlFlowProtector;
class DetectionEngine;

class ManualSEH;
class GuardRegions;

// game/managers/
class RoomManager;
class UnitContainerManager;
class UnitManager;
class NetManager;
class DynamicsManager;
class WorldManager;

// game/network/
class CryptoEngine;
class HeartbeatHandler;
class ConnectHandler;
class AuthorizeHandler;

// game/data/
class CdbmLoader;

// modding/features/
class NationIndexFeature;
class CustomTickrateFeature;
class ResolutionsFeature;
class SpectatePovFeature;
class PcBangFeature;

// modding/bugfixes/
class ScreenshotFix;
class WeaponRestrictionFix;

// utils/
class Logger;
class Splash;

} // namespace mg
