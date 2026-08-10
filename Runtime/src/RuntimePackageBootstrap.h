#pragma once

#include "RuntimeBootstrap.h"

#include <string>
#include <vector>

namespace renegade::runtime
{
    // Parse the normal Runtime command line first. A valid explicit --project
    // remains authoritative for Studio Test Level and tooling. Only a genuine
    // missing-project result may fall back to the Gate 3 package manifest,
    // resolved from the executable's own immutable directory rather than CWD.
    [[nodiscard]] RuntimeBootstrapResult ResolveRuntimeLaunch(
        const std::vector<std::string>& arguments,
        const std::string& executablePath);
}
