// =============================================================================
// Resolutions - Custom resolution table + aspect ratio hooks
// =============================================================================
// Resolution struct MUST match the game's binary layout exactly (140 bytes)
// because SwapAddressPatch redirects instruction operands into this struct
// array and the game indexes with stride = sizeof(Resolution).
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg::modding {

// Must match game's resolution struct layout — stride 140 bytes
// Game aspect index: 0=4:3, 1=5:3, 2=5:4, 3=16:9, 4=16:10
struct Resolution {
    i32      width;
    i32      height;
    i32      aspectRatioIdx;
    char16_t aspectRatioLabel[64];  // 128 bytes — matches game's wchar_t[64]
};

// Number of entries and the resolution table itself live at namespace scope
// so they are never emitted as class-level static data (problematic in
// manually-mapped DLLs where .rdata relocations can be unreliable).
enum : u32 { kNumResolutions = 35 };
extern const Resolution kResolutions[kNumResolutions];

class CustomResolutions {
public:
    explicit CustomResolutions(::mg::MegaGuardContext& ctx);
    ~CustomResolutions();

    VoidResult patch();

private:
    ::mg::MegaGuardContext& ctx_;
};

} // namespace mg::modding
