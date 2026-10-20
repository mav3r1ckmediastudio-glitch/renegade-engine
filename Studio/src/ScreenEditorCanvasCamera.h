#pragma once
#include <algorithm>
#include <cmath>

namespace renegade::studio
{
    // UI-11 editor-only camera. Does not alter ScreenDocument design pixels.
    struct ScreenEditorCanvasCamera
    {
        float zoom = 1.0f;
        float panX = 0.0f;
        float panY = 0.0f;
        static constexpr float MinZoom = 0.25f;
        static constexpr float MaxZoom = 4.0f;

        void Reset() noexcept { zoom = 1.0f; panX = panY = 0.0f; }

        // Screen-space logical pixels relative to the viewport's origin.
        // The zoom pivot stays beneath the cursor.
        void ZoomAt(float factor, float cursorX, float cursorY) noexcept
        {
            if (!std::isfinite(factor) || factor <= 0.0f ||
                !std::isfinite(cursorX) || !std::isfinite(cursorY)) return;
            const float next = std::clamp(zoom * factor, MinZoom, MaxZoom);
            const float ratio = next / zoom;
            panX = cursorX - (cursorX - panX) * ratio;
            panY = cursorY - (cursorY - panY) * ratio;
            zoom = next;
        }

        void PanBy(float dx, float dy) noexcept
        {
            if (!std::isfinite(dx) || !std::isfinite(dy)) return;
            panX += dx;
            panY += dy;
        }

        // Conversion is reversible, independent of authored screen scale.
        [[nodiscard]] float ViewX(float x) const noexcept { return x * zoom + panX; }
        [[nodiscard]] float ViewY(float y) const noexcept { return y * zoom + panY; }
        [[nodiscard]] float DesignX(float x) const noexcept { return (x - panX) / zoom; }
        [[nodiscard]] float DesignY(float y) const noexcept { return (y - panY) / zoom; }
    };
}
