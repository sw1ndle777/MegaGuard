// =============================================================================
// IATScrubber - Import Address Table sanitization
// =============================================================================
#pragma once

#include "core/types.hpp"

namespace mg {

class MegaGuardContext;

class IATScrubber {
public:
    explicit IATScrubber(MegaGuardContext& ctx);
    ~IATScrubber();

    IATScrubber(const IATScrubber&) = delete;
    IATScrubber& operator=(const IATScrubber&) = delete;

    VoidResult scrubDebugImports();

private:
    MegaGuardContext& ctx_;
};

} // namespace mg
