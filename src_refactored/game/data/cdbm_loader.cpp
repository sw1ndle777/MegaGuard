// =============================================================================
// CDBMLoader - Implementation
// =============================================================================
#include "pch.hpp"
#include "game/data/cdbm_loader.hpp"
#include "core/context.hpp"
#include "core/context_accessor.hpp"
#include "engine/hook_id.hpp"
#include "engine/hook_registry.hpp"
#include "game/addresses.hpp"
#include "utils/call_helper.hpp"
#include "utils/cloakwork_isolation.hpp"

#include <fstream>

namespace mg::game {

namespace {

constexpr uptr kCdbmInit  = 0x42DDB0;
constexpr uptr kCdbmParse = 0x42F480;

bool __fastcall hkCDBMLoad(int thisptr, int /*edx*/, const char* path) {
    mg::call<void(__thiscall*)(int)>(kCdbmInit, thisptr);

    std::ifstream file(path, std::ios::in | std::ios::binary);
    char buffer[64];
    file.rdbuf()->pubsetbuf(buffer, sizeof(buffer));

    auto opened = mg::call<bool(__thiscall*)(int, const char*)>(kCdbmParse, thisptr, path);
    file.close();
    return opened;
}

} // anonymous namespace

CDBMLoader::CDBMLoader(MegaGuardContext& ctx) : ctx_(ctx) {}
CDBMLoader::~CDBMLoader() = default;

VoidResult CDBMLoader::install() {
    auto& registry = ctx_.hookRegistry();
    registry.registerDetour(HookId::CDBMLoad).create(addr::anticheat::cdbm::Load, hkCDBMLoad);
    return VoidResult::ok();
}

} // namespace mg::game
