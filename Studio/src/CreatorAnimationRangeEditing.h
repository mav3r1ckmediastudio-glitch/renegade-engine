#pragma once

#include "renegade/bridge/CreatorModelImportRecipe.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace renegade::studio
{
    // Apply a user-authored native time trim without corrupting the pending recipe.
    // Imported FBX timestamps are floating-point values: accept a field's rounded
    // display of an exact source endpoint, but never allow an out-of-source clip.
    inline bool TryEditCreatorAnimationTrim(
        bridge::CreatorAnimationImportRecipe& clip,
        const float sourceStart, const float sourceEnd,
        const float requested, const bool editStart, std::string& error)
    {
        error.clear();
        if (!std::isfinite(sourceStart) || !std::isfinite(sourceEnd) ||
            sourceEnd <= sourceStart || !std::isfinite(requested) ||
            !std::isfinite(clip.start) || !std::isfinite(clip.end))
        {
            error = "Invalid trim: enter a finite time in seconds.";
            return false;
        }
        const float tolerance = std::max(0.00001f,
            (sourceEnd - sourceStart) * 0.00001f);
        if (requested < sourceStart - tolerance || requested > sourceEnd + tolerance)
        {
            error = "Invalid trim: time is outside the source clip.";
            return false;
        }
        const float value = std::clamp(requested, sourceStart, sourceEnd);
        if (editStart ? value >= clip.end : value <= clip.start)
        {
            error = editStart ? "Invalid trim: Start must be before End." :
                "Invalid trim: End must be after Start.";
            return false;
        }
        if (editStart) clip.start = value;
        else clip.end = value;
        return true;
    }
}