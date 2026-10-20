#pragma once

// Editor-only canvas snapping. This header deliberately has no Wicked or
// Runtime dependencies; the Screen document stores authored positions, not
// editor grid preferences.
#include "renegade/bridge/ScreenService.h"

#include <algorithm>
#include <cmath>

namespace renegade::studio
{
    enum class ScreenGridGesture
    {
        Move,
        ResizeTopLeft,
        ResizeTopRight,
        ResizeBottomLeft,
        ResizeBottomRight,
    };

    inline float SnapScreenGridCoordinate(float coordinate, float spacing) noexcept
    {
        if (!std::isfinite(coordinate) || !std::isfinite(spacing) ||
            spacing <= 0.0f)
            return coordinate;
        return std::round(coordinate / spacing) * spacing;
    }

    inline bridge::ScreenRect SnapScreenGridRect(
        bridge::ScreenRect rect,
        ScreenGridGesture gesture,
        float spacing,
        float canvasWidth,
        float canvasHeight) noexcept
    {
        if (!std::isfinite(spacing) || spacing <= 0.0f ||
            !std::isfinite(canvasWidth) || !std::isfinite(canvasHeight) ||
            canvasWidth <= 0.0f || canvasHeight <= 0.0f)
            return rect;
        constexpr float minSize = 8.0f;
        const auto snap = [spacing](float value)
        {
            return SnapScreenGridCoordinate(value, spacing);
        };
        const float right = rect.x + rect.width;
        const float bottom = rect.y + rect.height;
        switch (gesture)
        {
        case ScreenGridGesture::Move:
            rect.x = std::clamp(snap(rect.x), 0.0f,
                std::max(0.0f, canvasWidth - rect.width));
            rect.y = std::clamp(snap(rect.y), 0.0f,
                std::max(0.0f, canvasHeight - rect.height));
            break;
        case ScreenGridGesture::ResizeTopLeft:
            rect.x = std::clamp(snap(rect.x), 0.0f, right - minSize);
            rect.y = std::clamp(snap(rect.y), 0.0f, bottom - minSize);
            rect.width = right - rect.x;
            rect.height = bottom - rect.y;
            break;
        case ScreenGridGesture::ResizeTopRight:
            rect.y = std::clamp(snap(rect.y), 0.0f, bottom - minSize);
            rect.width = std::clamp(snap(right), rect.x + minSize,
                canvasWidth) - rect.x;
            rect.height = bottom - rect.y;
            break;
        case ScreenGridGesture::ResizeBottomLeft:
            rect.x = std::clamp(snap(rect.x), 0.0f, right - minSize);
            rect.width = right - rect.x;
            rect.height = std::clamp(snap(bottom), rect.y + minSize,
                canvasHeight) - rect.y;
            break;
        case ScreenGridGesture::ResizeBottomRight:
            rect.width = std::clamp(snap(right), rect.x + minSize,
                canvasWidth) - rect.x;
            rect.height = std::clamp(snap(bottom), rect.y + minSize,
                canvasHeight) - rect.y;
            break;
        }
        return rect;
    }
}
