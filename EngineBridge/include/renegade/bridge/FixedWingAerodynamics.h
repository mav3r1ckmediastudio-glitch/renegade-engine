#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace renegade::bridge
{
    // Aircraft-local SI units. +X is starboard, +Y is up and +Z is forward.
    struct FlightVector
    {
        float x = 0.f, y = 0.f, z = 0.f;

        FlightVector operator+(FlightVector other) const
        {
            return {x + other.x, y + other.y, z + other.z};
        }
        FlightVector operator-(FlightVector other) const
        {
            return {x - other.x, y - other.y, z - other.z};
        }
        FlightVector operator*(float scale) const
        {
            return {x * scale, y * scale, z * scale};
        }
        FlightVector Cross(FlightVector other) const
        {
            return {y * other.z - z * other.y, z * other.x - x * other.z,
                    x * other.y - y * other.x};
        }
    };

    struct FixedWingInput
    {
        float throttle = 0.f;
        float pitch = 0.f; // positive raises the nose
        float roll = 0.f;  // positive lowers the starboard wing
        float yaw = 0.f;   // positive turns the nose to starboard
    };

    struct FixedWingSurface
    {
        FlightVector centre;
        float area = 0.f;
        float incidence = 0.f; // radians, measured from the zero-lift line
    };

    struct FixedWingSetup
    {
        FixedWingSurface portWing{{-2.6f, .7f, 0.f}, 8.1f, .091f};
        FixedWingSurface starboardWing{{2.6f, .7f, 0.f}, 8.1f, .091f};
        FixedWingSurface tail{{0.f, .16f, -3.1f}, 2.65f, -.03f};
        FixedWingSurface fin{{0.f, .66f, -3.1f}, 1.45f, 0.f};
        FlightVector propeller{0.f, .18f, 2.8f};
        float maxThrust = 2450.f;       // newtons at zero airspeed
        float propellerPitchSpeed = 76.f; // metres per second
        float fuselageDragArea = .45f;  // square metres times drag coefficient
        float wingStallAngle = .263f;
    };

    struct FixedWingLoad
    {
        FlightVector position;
        FlightVector force;
    };

    inline float FlightControl(float value)
    {
        return std::isfinite(value) ? std::clamp(value, -1.f, 1.f) : 0.f;
    }

    inline FixedWingLoad EvaluateAirfoil(const FixedWingSurface& surface,
        FlightVector airflow, FlightVector angularVelocity, float controlIncidence,
        float density, bool vertical, float stallAngle = .263f)
    {
        const FlightVector local = airflow + angularVelocity.Cross(surface.centre);
        const float across = vertical ? local.x : local.y;
        const float forward = local.z;
        const float speed = std::hypot(across, forward);
        if (speed < .05f || density <= 0.f || surface.area <= 0.f)
            return {surface.centre, {}};

        const float attack = std::atan2(-across, std::abs(forward)) +
            surface.incidence + controlIncidence;

        const float attached = 1.f - std::clamp(
            (std::abs(attack) - stallAngle) / .33f, 0.f, 1.f);
        const float liftCoefficient = forward > 0.f
            ? std::clamp(5.25f * attack, -1.38f, 1.38f) * attached +
                .55f * std::sin(2.f * attack) * (1.f - attached)
            : 0.f;
        const float dragCoefficient = .027f + .067f * liftCoefficient * liftCoefficient +
            (1.f - attached) * .72f + (forward < 0.f ? .55f : 0.f);
        const float dynamicLoad = .5f * density * speed * surface.area;
        const float crossForce = dynamicLoad * (forward * liftCoefficient - across * dragCoefficient);
        const float forwardForce = dynamicLoad * (-across * liftCoefficient - forward * dragCoefficient);
        return {surface.centre, vertical ? FlightVector{crossForce, 0.f, forwardForce}
                                        : FlightVector{0.f, crossForce, forwardForce}};
    }

    inline std::array<FixedWingLoad, 6> EvaluateFixedWing(const FixedWingSetup& setup,
        FixedWingInput input, FlightVector airRelativeVelocity, FlightVector angularVelocity,
        float airDensity)
    {
        const float throttle = std::max(0.f, FlightControl(input.throttle));
        const float pitch = FlightControl(input.pitch) * .14f;
        const float roll = FlightControl(input.roll) * .092f;
        const float yaw = FlightControl(input.yaw) * .15f;
        const float speed = std::hypot(airRelativeVelocity.x, airRelativeVelocity.y, airRelativeVelocity.z);
        const float density = std::isfinite(airDensity) ? std::max(0.f, airDensity) : 0.f;
        const float thrustFraction = std::clamp(1.f - airRelativeVelocity.z /
            std::max(1.f, setup.propellerPitchSpeed), 0.f, 1.f);
        const float thrust = density > 0.f
            ? setup.maxThrust * throttle * thrustFraction * (density / 1.225f)
            : 0.f;
        return {
            EvaluateAirfoil(setup.portWing, airRelativeVelocity, angularVelocity, roll, density, false, setup.wingStallAngle),
            EvaluateAirfoil(setup.starboardWing, airRelativeVelocity, angularVelocity, -roll, density, false, setup.wingStallAngle),
            EvaluateAirfoil(setup.tail, airRelativeVelocity, angularVelocity, -pitch, density, false),
            EvaluateAirfoil(setup.fin, airRelativeVelocity, angularVelocity, -yaw, density, true),
            FixedWingLoad{setup.propeller, {0.f, 0.f, thrust}},
            FixedWingLoad{{}, airRelativeVelocity * (-.5f * density * speed * setup.fuselageDragArea)}
        };
    }
}
