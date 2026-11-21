#pragma once

#include "TrackedVehicleService.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace renegade::bridge
{
    struct TrackedVehicleTuning
    {
        float widthScale = 1.f, lengthScale = 1.f, anchorHeight = 0.f;
        float radius = .30f, width = .10f;
        float suspensionMin = .30f, suspensionTravel = .20f, endSuspension = .30f;
        float spring = 2.4f, damping = .65f;
        float longitudinalGrip = 4.f, lateralGrip = .38f;
        float brakeTorque = 15000.f;
        float engineTorque = 500.f, differentialRatio = 6.f;
        float trackInertia = 10.f, trackDamping = .5f;
        float pivotPower = .85f, steerReduction = .70f, maxTilt = 65.f;
    };

    struct TrackedTuningField
    {
        float TrackedVehicleTuning::*member;
        const char* key;
        const char* label;
        float minimum, maximum;
        bool advanced = false;
    };

    inline constexpr TrackedVehicleTuning DefaultTrackedVehicleTuning{};
    inline constexpr TrackedTuningField TrackedTuningFields[]{
        {&TrackedVehicleTuning::widthScale, "renegade.tracked.width_scale", "LAYOUT WIDTH SCALE", .10f, 4.f},
        {&TrackedVehicleTuning::lengthScale, "renegade.tracked.length_scale", "LAYOUT LENGTH SCALE", .10f, 4.f},
        {&TrackedVehicleTuning::anchorHeight, "renegade.tracked.anchor_height", "ANCHOR Y OFFSET (M)", -3.f, 3.f},
        {&TrackedVehicleTuning::radius, "renegade.tracked.wheel_radius", "WHEEL RADIUS (M)", .05f, 2.f},
        {&TrackedVehicleTuning::width, "renegade.tracked.wheel_width", "WHEEL WIDTH (M)", .03f, 2.f},
        {&TrackedVehicleTuning::suspensionMin, "renegade.tracked.suspension_min", "MIN SUSPENSION LENGTH (M)", 0.f, 1.5f},
        {&TrackedVehicleTuning::suspensionTravel, "renegade.tracked.suspension_travel", "SUSPENSION TRAVEL (M)", .01f, 1.5f},
        {&TrackedVehicleTuning::endSuspension, "renegade.tracked.end_suspension", "IDLER AND SPROCKET LENGTH (M)", 0.f, 2.f},
        {&TrackedVehicleTuning::spring, "renegade.tracked.spring", "SUSPENSION FREQUENCY (HZ)", .5f, 10.f},
        {&TrackedVehicleTuning::damping, "renegade.tracked.damping", "SUSPENSION DAMPING", .05f, 2.f},
        {&TrackedVehicleTuning::longitudinalGrip, "renegade.tracked.longitudinal_grip", "FORWARD GRIP", 0.f, 6.f},
        {&TrackedVehicleTuning::lateralGrip, "renegade.tracked.lateral_grip", "SIDE GRIP", 0.f, 4.f},
        {&TrackedVehicleTuning::brakeTorque, "renegade.tracked.brake_torque", "TRACK BRAKE TORQUE (NM)", 0.f, 100000.f},
        {&TrackedVehicleTuning::engineTorque, "renegade.tracked.engine_torque", "ENGINE MAX TORQUE (NM)", 1.f, 5000.f, true},
        {&TrackedVehicleTuning::differentialRatio, "renegade.tracked.differential_ratio", "FINAL DRIVE RATIO", .5f, 16.f, true},
        {&TrackedVehicleTuning::trackInertia, "renegade.tracked.track_inertia", "TRACK INERTIA (KG M2)", .1f, 100.f, true},
        {&TrackedVehicleTuning::trackDamping, "renegade.tracked.track_damping", "TRACK DAMPING", 0.f, 5.f, true},
        {&TrackedVehicleTuning::pivotPower, "renegade.tracked.pivot_power", "PIVOT POWER", 0.f, 1.f, true},
        {&TrackedVehicleTuning::steerReduction, "renegade.tracked.steer_reduction", "STEERING TRACK REDUCTION", 0.f, .98f, true},
        {&TrackedVehicleTuning::maxTilt, "renegade.tracked.max_tilt", "MAX CHASSIS TILT (DEG)", 15.f, 85.f, true},
    };

    inline float SanitizeTrackedTuningValue(const TrackedTuningField& field, float value)
    {
        return std::isfinite(value) ? std::clamp(value, field.minimum, field.maximum)
            : DefaultTrackedVehicleTuning.*field.member;
    }

    inline std::optional<float> ReadTrackedTuningValue(const wi::scene::Scene& scene,
        wi::ecs::Entity entity, const TrackedTuningField& field)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata && metadata->float_values.has(field.key))
            return metadata->float_values.get(field.key);
        return {};
    }

    inline TrackedVehicleTuning ReadTrackedTuning(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        auto result = DefaultTrackedVehicleTuning;
        for (const auto& field : TrackedTuningFields)
            if (const auto value = ReadTrackedTuningValue(scene, entity, field))
                result.*field.member = SanitizeTrackedTuningValue(field, *value);
        return result;
    }

    inline bool WriteTrackedTuningValue(wi::scene::Scene& scene, wi::ecs::Entity entity,
        const TrackedTuningField& field, std::optional<float> value)
    {
        if (!IsTrackedVehicle(scene, entity) || !scene.rigidbodies.Contains(entity))
            return false;
        auto& values = scene.metadatas.GetComponent(entity)->float_values;
        if (value)
            values.set(field.key, SanitizeTrackedTuningValue(field, *value));
        else
            values.erase(field.key);
        return true;
    }

    inline XMFLOAT3 TrackedDefaultAnchor(unsigned side, unsigned station)
    {
        constexpr float z[] {2.95f, 2.1f, 1.4f, .7f, 0.f, -.7f, -1.4f, -2.1f, -2.95f};
        const bool end = station == 0 || station == TrackedWheelsPerSide - 1;
        return {side == 0 ? -1.70f : 1.70f, end ? 0.f : -.30f, z[station]};
    }
}
