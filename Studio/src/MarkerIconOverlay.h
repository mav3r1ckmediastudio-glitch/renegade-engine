#pragma once

#include <WickedEngine.h>

namespace renegade::studio
{
    class StudioRenderPath;

    // Registers one clickable, editor-only marker overlay with the
    // authoritative Level Editor GUI. Safe to call every frame.
    void EnsureMarkerIconOverlay(StudioRenderPath& owner);
}
