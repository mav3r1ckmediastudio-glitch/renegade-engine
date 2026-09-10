#pragma once

#include <WickedEngine.h>

namespace renegade::studio
{
    class StudioRenderPath;

    // Registers one non-interactive, editor-only marker overlay with the
    // authoritative Level Editor GUI. Safe to call every frame.
    void EnsureMarkerIconOverlay(StudioRenderPath& owner);
}
