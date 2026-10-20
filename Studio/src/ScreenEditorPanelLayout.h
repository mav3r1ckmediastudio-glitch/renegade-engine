#pragma once

// Editor-only workspace widths. Input is the Studio's logical width, not the
// Screen document design resolution or the Runtime screen dimensions.
#include <algorithm>
#include <cmath>

namespace renegade::studio
{
    struct ScreenEditorPanelLayout
    {
        float left = 260.0f;
        float right = 360.0f;
        static constexpr float Gap = 14.0f;
        static constexpr float MinLeft = 200.0f;
        static constexpr float MinRight = 340.0f;
        static constexpr float MinCanvas = 380.0f;

        [[nodiscard]] float CanvasWidth(float width) const noexcept
        {
            return std::max(1.0f, width - left - right - 2.0f * Gap);
        }

        // Both panes have preferred minimums. On narrower viewports the
        // central canvas may shrink rather than overlapping either pane.
        [[nodiscard]] static float CanvasReservation(float width) noexcept
        {
            return std::max(1.0f, std::min(MinCanvas,
                width - MinLeft - MinRight - 2.0f * Gap));
        }

        void Clamp(float width) noexcept
        {
            if (!std::isfinite(width)) return;
            const float budget = std::max(1.0f, width - 2.0f * Gap -
                CanvasReservation(width));
            if (budget < MinLeft + MinRight)
            {
                left = budget * (MinLeft / (MinLeft + MinRight));
                right = budget - left;
                return;
            }
            left = std::clamp(left, MinLeft, budget - MinRight);
            right = std::clamp(right, MinRight, budget - left);
        }

        void DragLeft(float pointerX, float width) noexcept
        {
            if (!std::isfinite(pointerX)) return;
            const float budget = std::max(1.0f, width - 2.0f * Gap -
                CanvasReservation(width));
            left = std::clamp(pointerX - Gap * 0.5f,
                std::min(MinLeft, budget - right),
                std::max(std::min(MinLeft, budget - right), budget - right));
        }

        void DragRight(float pointerX, float width) noexcept
        {
            if (!std::isfinite(pointerX) || !std::isfinite(width)) return;
            const float budget = std::max(1.0f, width - 2.0f * Gap -
                CanvasReservation(width));
            right = std::clamp(width - pointerX - Gap * 0.5f,
                std::min(MinRight, budget - left),
                std::max(std::min(MinRight, budget - left), budget - left));
        }
    };
}
