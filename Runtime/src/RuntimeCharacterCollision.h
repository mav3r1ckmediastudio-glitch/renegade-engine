#pragma once

#include "renegade/bridge/NavigationService.h"

namespace renegade::runtime
{
    // Once per loaded Level, before native Character controllers are activated.
    // Character surface queries use Scene geometry, independently of Jolt and
    // independently of whether the Level contains a navigation grid.
    inline void PrepareRuntimeCharacterCollisionScene(wi::scene::Scene& scene)
    {
        (void)bridge::PrepareRigidBodyNavigationGeometry(scene);
        scene.Update(0.0f);
        // The first object update has no previous-frame transforms. Do not let
        // a stationary surface impart its initial world translation as platform
        // inertia on the first simulated Character frame.
        scene.matrix_objects_prev = scene.matrix_objects;
    }
}
