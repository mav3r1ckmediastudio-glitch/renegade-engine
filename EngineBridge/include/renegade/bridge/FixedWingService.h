#pragma once

#include "FixedWingAerodynamics.h"
#include <WickedEngine.h>

namespace renegade::bridge
{
    struct FixedWingGearPoint
    {
        // Fully extended contact position in the rigid body's local frame.
        FlightVector position;
        bool steerable = false;
    };

    struct FixedWingAircraft
    {
        FixedWingSetup aerodynamics;
        // Nose, port main, starboard main.
        std::array<FixedWingGearPoint, 3> gear;
        FlightVector wind;
        float airDensity = 1.225f;
        float gearSpringScale = 1.f;
        float gearDampingScale = 1.f;
    };

    struct FixedWingTelemetry
    {
        float airspeed = 0.f;
        unsigned groundContacts = 0;
    };

    // Owner-thread methods. The fixed physics step retains the last command
    // until the caller clears it or the scene/body is removed.
    bool RegisterFixedWing(wi::scene::Scene& scene, wi::ecs::Entity entity,
        const FixedWingAircraft& aircraft);
    bool CommandFixedWing(wi::scene::Scene& scene, wi::ecs::Entity entity,
        FixedWingInput input, float brakes);
    FixedWingTelemetry ReadFixedWing(const wi::scene::Scene& scene,
        wi::ecs::Entity entity);
}
