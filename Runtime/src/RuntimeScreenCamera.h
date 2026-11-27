#pragma once
#include "renegade/bridge/ScreenService.h"
#include <map>

namespace renegade::runtime
{
    // Runtime-only screen overrides survive Story Flow navigation during this game.
    struct RuntimeScreenCameraOverride
    {
        bool authored=true;
        std::optional<bridge::ScreenCameraSource> source;
        std::uint64_t revision=0;
    };
    using ScreenCameraKey=std::pair<bridge::StableId,bridge::StableId>;
    inline std::map<ScreenCameraKey,RuntimeScreenCameraOverride>& ScreenCameraOverrides()
    {
        static std::map<ScreenCameraKey,RuntimeScreenCameraOverride> overrides;
        return overrides;
    }
}
