#pragma once

#include "FixedWingService.h"
#include "FlightVehicleService.h"

#include <optional>

namespace renegade::bridge
{
    struct FixedWingTuning
    {
        float propThrust = FixedWingSetup{}.maxThrust;
        float thrustFadeSpeed = FixedWingSetup{}.propellerPitchSpeed;
        float noseWheelRadius = .23f;
        float mainWheelRadius = .293f;
        float wingArea = FixedWingSetup{}.portWing.area + FixedWingSetup{}.starboardWing.area;
        float wingIncidence = FixedWingSetup{}.portWing.incidence * (180.f / XM_PI);
        float dragArea = FixedWingSetup{}.fuselageDragArea;
        float stallAngle = FixedWingSetup{}.wingStallAngle * (180.f / XM_PI);
        float pitchResponse = 1.f;
        float bankResponse = 1.f;
        float gearSpring = 1.f;
        float gearDamping = 1.f;
    };

    struct FixedWingTuningField
    {
        float FixedWingTuning::*member;
        const char* key;
        const char* label;
        float minimum;
        float maximum;
    };

    inline constexpr FixedWingTuning DefaultFixedWingTuning{};
    inline constexpr FixedWingTuningField FixedWingTuningFields[]{
        {&FixedWingTuning::propThrust, "renegade.fixed_wing.prop_thrust", "MAX THRUST (N)", 0.f, 250000.f},
        {&FixedWingTuning::thrustFadeSpeed, "renegade.fixed_wing.thrust_fade", "THRUST FADE SPEED (M/S)", 10.f, 1500.f},
        {&FixedWingTuning::noseWheelRadius, "renegade.fixed_wing.nose_wheel_radius", "NOSE TYRE RADIUS (M)", .05f, 2.f},
        {&FixedWingTuning::mainWheelRadius, "renegade.fixed_wing.main_wheel_radius", "MAIN TYRE RADIUS (M)", .05f, 2.f},
        {&FixedWingTuning::wingArea, "renegade.fixed_wing.wing_area", "TOTAL WING AREA (M2)", .5f, 100.f},
        {&FixedWingTuning::wingIncidence, "renegade.fixed_wing.incidence", "WING INCIDENCE (DEG)", -10.f, 15.f},
        {&FixedWingTuning::dragArea, "renegade.fixed_wing.drag_area", "FUSELAGE DRAG AREA (M2)", 0.f, 8.f},
        {&FixedWingTuning::stallAngle, "renegade.fixed_wing.stall_angle", "WING STALL ANGLE (DEG)", 5.f, 35.f},
        {&FixedWingTuning::pitchResponse, "renegade.fixed_wing.pitch_response", "PITCH SENSITIVITY", .25f, 2.f},
        {&FixedWingTuning::bankResponse, "renegade.fixed_wing.bank_response", "BANK SENSITIVITY", .25f, 2.f},
        {&FixedWingTuning::gearSpring, "renegade.fixed_wing.gear_spring", "GEAR STIFFNESS", .35f, 3.f},
        {&FixedWingTuning::gearDamping, "renegade.fixed_wing.gear_damping", "GEAR DAMPING", .15f, 3.f},
    };

    inline float SanitizeFixedWingTuningValue(const FixedWingTuningField& field, float value)
    {
        return std::isfinite(value) ? std::clamp(value, field.minimum, field.maximum)
            : DefaultFixedWingTuning.*field.member;
    }

    inline std::optional<float> ReadFixedWingTuningValue(const wi::scene::Scene& scene,
        wi::ecs::Entity entity, const FixedWingTuningField& field)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata && metadata->float_values.has(field.key))
            return metadata->float_values.get(field.key);
        return std::nullopt;
    }

    inline FixedWingTuning ReadFixedWingTuning(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        auto tuning = DefaultFixedWingTuning;
        for (const auto& field : FixedWingTuningFields)
            if (const auto value = ReadFixedWingTuningValue(scene, entity, field))
                tuning.*field.member = SanitizeFixedWingTuningValue(field, *value);
        return tuning;
    }

    inline bool WriteFixedWingTuningValue(wi::scene::Scene& scene, wi::ecs::Entity entity,
        const FixedWingTuningField& field, std::optional<float> value)
    {
        if (!IsFixedWing(scene, entity) || !scene.rigidbodies.Contains(entity))
            return false;
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (value)
            metadata->float_values.set(field.key, SanitizeFixedWingTuningValue(field, *value));
        else
            metadata->float_values.erase(field.key);
        return true;
    }
}
