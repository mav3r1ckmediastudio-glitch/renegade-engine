#pragma once

// Design-space sibling alignment for the editor, independent of Wicked and
// project serialization. Caller supplies exact resolved sibling rectangles.
// The grid fallback is retained independently on each unmatched axis.
#include "ScreenCanvasGrid.h"
#include <cmath>
#include <vector>

namespace renegade::studio
{
    struct ScreenElementSnapResult
    {
        bridge::ScreenRect rect;
        bool guideX = false;
        bool guideY = false;
        float guideCoordinateX = 0.0f;
        float guideCoordinateY = 0.0f;
    };

    inline ScreenElementSnapResult SnapScreenToSiblings(
        const bridge::ScreenRect& raw,
        const bridge::ScreenRect& gridFallback,
        ScreenGridGesture gesture,
        const std::vector<bridge::ScreenRect>& siblings,
        float toleranceX,
        float toleranceY,
        float canvasWidth,
        float canvasHeight) noexcept
    {
        ScreenElementSnapResult result;
        result.rect = gridFallback;
        if (!std::isfinite(toleranceX) || !std::isfinite(toleranceY) ||
            toleranceX <= 0.0f || toleranceY <= 0.0f)
            return result;
        const bool move = gesture == ScreenGridGesture::Move;
        const bool left = gesture == ScreenGridGesture::ResizeTopLeft ||
            gesture == ScreenGridGesture::ResizeBottomLeft;
        const bool right = gesture == ScreenGridGesture::ResizeTopRight ||
            gesture == ScreenGridGesture::ResizeBottomRight;
        const bool top = gesture == ScreenGridGesture::ResizeTopLeft ||
            gesture == ScreenGridGesture::ResizeTopRight;
        const bool bottom = gesture == ScreenGridGesture::ResizeBottomLeft ||
            gesture == ScreenGridGesture::ResizeBottomRight;
        constexpr float minSize = 8.0f;

        float bestX = toleranceX;
        float bestY = toleranceY;
        float matchedX = 0.0f;
        float matchedY = 0.0f;
        float guideX = 0.0f;
        float guideY = 0.0f;

        for (const auto& peer : siblings)
        {
            if (!std::isfinite(peer.x) || !std::isfinite(peer.y) ||
                !std::isfinite(peer.width) || !std::isfinite(peer.height) ||
                peer.width <= 0.0f || peer.height <= 0.0f)
                continue;
            const float peerXs[] = {peer.x, peer.x + peer.width * 0.5f,
                peer.x + peer.width};
            const float peerYs[] = {peer.y, peer.y + peer.height * 0.5f,
                peer.y + peer.height};
            for (float target : peerXs)
            {
                // Move supports matching left/centre/right to a peer guide;
                // resize only adjusts the actively dragged edge.
                const int count = move ? 3 : 1;
                for (int i = 0; i < count; ++i)
                {
                    const float fraction = move ? i * 0.5f : 0.0f;
                    const float proposed = move
                        ? target - raw.width * fraction : target;
                    const float anchor = move ? raw.x :
                        left ? raw.x : raw.x + raw.width;
                    const bool valid = move
                        ? proposed >= 0.0f &&
                            proposed + raw.width <= canvasWidth
                        : left ? proposed >= 0.0f &&
                            proposed <= raw.x + raw.width - minSize
                        : right && proposed >= raw.x + minSize &&
                            proposed <= canvasWidth;
                    const float distance = std::abs(proposed -
                        (move ? raw.x : anchor));
                    if (valid && distance <= bestX)
                    {
                        bestX = distance;
                        matchedX = proposed;
                        guideX = target;
                        result.guideX = true;
                    }
                }
            }
            for (float target : peerYs)
            {
                const int count = move ? 3 : 1;
                for (int i = 0; i < count; ++i)
                {
                    const float fraction = move ? i * 0.5f : 0.0f;
                    const float proposed = move
                        ? target - raw.height * fraction : target;
                    const float anchor = move ? raw.y :
                        top ? raw.y : raw.y + raw.height;
                    const bool valid = move
                        ? proposed >= 0.0f &&
                            proposed + raw.height <= canvasHeight
                        : top ? proposed >= 0.0f &&
                            proposed <= raw.y + raw.height - minSize
                        : bottom && proposed >= raw.y + minSize &&
                            proposed <= canvasHeight;
                    const float distance = std::abs(proposed -
                        (move ? raw.y : anchor));
                    if (valid && distance <= bestY)
                    {
                        bestY = distance;
                        matchedY = proposed;
                        guideY = target;
                        result.guideY = true;
                    }
                }
            }
        }
        if (result.guideX)
        {
            result.guideCoordinateX = guideX;
            if (move) result.rect.x = matchedX;
            else if (left)
            {
                const float fixedRight = raw.x + raw.width;
                result.rect.x = matchedX;
                result.rect.width = fixedRight - matchedX;
            }
            else if (right) result.rect.width = matchedX - raw.x;
        }
        if (result.guideY)
        {
            result.guideCoordinateY = guideY;
            if (move) result.rect.y = matchedY;
            else if (top)
            {
                const float fixedBottom = raw.y + raw.height;
                result.rect.y = matchedY;
                result.rect.height = fixedBottom - matchedY;
            }
            else if (bottom) result.rect.height = matchedY - raw.y;
        }
        return result;
    }
}
