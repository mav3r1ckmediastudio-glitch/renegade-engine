#pragma once
#include <algorithm>
namespace renegade::studio
{
    enum class OverviewIntent { None, Navigate, Fit };
    inline OverviewIntent ResolveOverviewIntent(bool inside, bool pressed, bool held,
        bool doubleClick, bool& suppressDrag) noexcept
    {
        if (!held) suppressDrag = false;
        if (!inside) return OverviewIntent::None;
        if (pressed && doubleClick) { suppressDrag = true; return OverviewIntent::Fit; }
        return held && !suppressDrag ? OverviewIntent::Navigate : OverviewIntent::None;
    }
    // Returns a world-space top-left (or additive offset) without mutating camera.
    struct PlacementPoint { float x, y; };
    inline PlacementPoint CentreNewCard(float centreX, float centreY,
        float panX, float panY, float zoom, float width, float height,
        float baseX = 0, float baseY = 0) noexcept
    {
        return {(centreX - panX) / zoom - width * 0.5f - baseX,
                (centreY - panY) / zoom - height * 0.5f - baseY};
    }
    struct OverviewCamera { float zoom, panX, panY; };
    inline OverviewCamera FitOverview(float minX, float minY, float maxX, float maxY,
        float width, float height, float margin, float maxZoom) noexcept
    {
        const float zoom = std::clamp(std::min(
            std::max(1.0f, width - 2 * margin) / std::max(1.0f, maxX - minX),
            std::max(1.0f, height - 2 * margin) / std::max(1.0f, maxY - minY)), 0.05f, maxZoom);
        return {zoom, width * 0.5f - (minX + maxX) * 0.5f * zoom,
            height * 0.5f - (minY + maxY) * 0.5f * zoom};
    }
}
