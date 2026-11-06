#pragma once

#include <WickedEngine.h>

namespace renegade::studio
{
    // GUI overlays are drawn after TAA. Cancel the render camera's clip-space
    // sample offset so their position and hit testing do not follow that jitter.
    inline DirectX::XMMATRIX EditorOverlayViewProjection(
        const wi::scene::CameraComponent& camera)
    {
        return camera.GetViewProjection() * DirectX::XMMatrixTranslation(
            -camera.jitter.x, -camera.jitter.y, 0.0f);
    }
}
