#pragma once

#include <WickedEngine.h>
#include <cstdint>

namespace renegade::bridge
{
    struct PhysicsDiagnostics
    {
        uint64_t totalFixedSteps = 0;
        uint32_t totalBodies = 0;
        uint32_t awakeRigidBodies = 0;
        uint32_t awakeSoftBodies = 0;
        uint32_t constraints = 0;
        uint32_t lastUpdateSteps = 0;
        uint32_t lastStepContactManifolds = 0;
        float lastUpdateMilliseconds = 0.f;
        bool ready = false;
    };

    // Scene owner thread. Read the last sample and request another on the next update.
    PhysicsDiagnostics SamplePhysicsDiagnostics(wi::scene::Scene& scene);
    bool RequestPhysicsStep(wi::scene::Scene& scene);
}
