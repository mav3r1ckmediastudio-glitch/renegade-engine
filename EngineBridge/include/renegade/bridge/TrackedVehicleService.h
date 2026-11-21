#pragma once

#include <WickedEngine.h>
#include <array>

namespace renegade::bridge
{
    inline constexpr char TrackedVehicleKey[] = "renegade.vehicle.tracked";
    constexpr unsigned TrackSides = 2;
    constexpr unsigned TrackedWheelsPerSide = 9;
    constexpr unsigned TrackedWheelCount = TrackSides * TrackedWheelsPerSide;

    inline bool IsTrackedVehicle(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata && metadata->bool_values.has(TrackedVehicleKey) &&
            metadata->bool_values.get(TrackedVehicleKey);
    }

    using TrackedWheelAnchors = std::array<XMFLOAT3, TrackedWheelCount>;
    struct TrackedVehicleState
    {
        bool ready = false;
        unsigned contacts = 0;
        std::array<float, TrackSides> trackSpeed{};
        std::array<float, TrackedWheelCount> suspension{};
        std::array<float, TrackedWheelCount> wheelAngle{};
    };

    // Anchors are in chassis metres, left then right, front to rear, +Z forward.
    bool RegisterTrackedVehicle(wi::scene::Scene& scene, wi::ecs::Entity entity,
        const TrackedWheelAnchors& anchors);
    bool DriveTrackedVehicle(wi::scene::Scene& scene, wi::ecs::Entity entity,
        float forward, float turn, float brake);
    TrackedVehicleState ReadTrackedVehicle(const wi::scene::Scene& scene, wi::ecs::Entity entity);
}
