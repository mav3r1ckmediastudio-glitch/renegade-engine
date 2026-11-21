#pragma once

#include "RuntimeBastionTurret.h"
#include "renegade/bridge/FixedWingRig.h"
#include "renegade/bridge/TrackedVehicleTuning.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace renegade::runtime
{
    struct RuntimeTrackedVehicle
    {
        static constexpr unsigned ShoeCount = 78;
        static constexpr float ShoePitch = 13.739f / ShoeCount;

        struct WheelVisual
        {
            wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
            XMFLOAT3 restPosition{};
            XMFLOAT4 restRotation{};
        };

        struct Belt
        {
            std::array<WheelVisual, ShoeCount> shoes{};
            float phase = 0.f;
        };

        std::array<WheelVisual, bridge::TrackedWheelCount> wheels{};
        std::array<Belt, bridge::TrackSides> belts{};
        wi::ecs::Entity driverView = wi::ecs::INVALID_ENTITY;
        RuntimeBastionTurret turret;
        bridge::TrackedVehicleTuning tuning;

        bool Register(wi::scene::Scene& scene, wi::ecs::Entity chassis)
        {
            wi::unordered_map<std::string, wi::ecs::Entity> named;
            for (size_t i = 0; i < scene.names.GetCount(); ++i)
            {
                const auto entity = scene.names.GetEntity(i);
                const auto& name = scene.names[i].name;
                if ((name.rfind("REF_JoltWheel_", 0) == 0 || name.rfind("Wheel_", 0) == 0 ||
                    name.rfind("TrackLink_", 0) == 0 || name == "REF_Driver_View" ||
                    name == "Turret_Yaw" || name == "Barrel_Elevation") &&
                    scene.Entity_IsDescendant(entity, chassis))
                    named.emplace(name, entity);
            }

            const auto get = [&](const char* name) {
                const auto found = named.find(name);
                return found == named.end() ? wi::ecs::INVALID_ENTITY : found->second;
            };
            const auto bindVisual = [&](WheelVisual& visual, const char* name) {
                visual.entity = get(name);
                if (const auto* pose = scene.transforms.GetComponent(visual.entity))
                {
                    visual.restPosition = pose->translation_local;
                    visual.restRotation = pose->rotation_local;
                }
            };
            driverView = get("REF_Driver_View");
            turret.Bind(scene, get("Turret_Yaw"), get("Barrel_Elevation"));
            tuning = bridge::ReadTrackedTuning(scene, chassis);
            bridge::TrackedWheelAnchors anchors;
            for (unsigned side = 0; side < bridge::TrackSides; ++side)
            {
                const char label = side == 0 ? 'L' : 'R';
                char name[48];
                for (unsigned station = 0; station < bridge::TrackedWheelsPerSide; ++station)
                {
                    const unsigned i = side * bridge::TrackedWheelsPerSide + station;
                    std::snprintf(name, sizeof(name), "REF_JoltWheel_%c_%02u", label, station);
                    XMMATRIX local;
                    anchors[i] = bridge::TrackedDefaultAnchor(side, station);
                    if (bridge::FixedWingPartTransform(scene, get(name), chassis, local))
                        XMStoreFloat3(&anchors[i], local.r[3]);
                    const char* type = station == 0 ? "Idler" :
                        station == bridge::TrackedWheelsPerSide - 1 ? "Drive" : "Road";
                    std::snprintf(name, sizeof(name), "Wheel_%c_%02u_%s", label, station, type);
                    bindVisual(wheels[i], name);
                }
                for (unsigned i = 0; i < ShoeCount; ++i)
                {
                    std::snprintf(name, sizeof(name), "TrackLink_%c_%03u", label, i);
                    bindVisual(belts[side].shoes[i], name);

                }
            }
            return bridge::RegisterTrackedVehicle(scene, chassis, anchors);
        }

        void UpdateVisuals(wi::scene::Scene& scene, const bridge::TrackedVehicleState& state, float dt)
        {
            if (!state.ready) return;
            for (unsigned i = 0; i < wheels.size(); ++i)
            {
                const auto& binding = wheels[i];
                auto* pose = scene.transforms.GetComponent(binding.entity);
                if (!pose) continue;
                const unsigned station = i % bridge::TrackedWheelsPerSide;
                const bool end = station == 0 || station == bridge::TrackedWheelsPerSide - 1;
                pose->translation_local = binding.restPosition;
                pose->translation_local.y += (end ? .30f : .40f) + tuning.anchorHeight - state.suspension[i];
                XMStoreFloat4(&pose->rotation_local, XMQuaternionMultiply(
                    XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), -state.wheelAngle[i]),
                    XMLoadFloat4(&binding.restRotation)));
                pose->SetDirty();
            }

            for (unsigned side = 0; side < belts.size(); ++side)
            {
                auto& belt = belts[side];
                if (std::any_of(belt.shoes.begin(), belt.shoes.end(), [](const auto& shoe) {
                    return shoe.entity == wi::ecs::INVALID_ENTITY;
                })) continue;
                belt.phase = std::fmod(belt.phase - state.trackSpeed[side] * tuning.radius *
                    std::clamp(dt, 0.f, .1f) / ShoePitch, float(ShoeCount));
                if (belt.phase < 0.f) belt.phase += float(ShoeCount);
                const auto offset = static_cast<unsigned>(belt.phase);
                const float t = belt.phase - float(offset);
                for (unsigned i = 0; i < ShoeCount; ++i)
                {
                    auto* pose = scene.transforms.GetComponent(belt.shoes[i].entity);
                    if (!pose) continue;
                    const auto& a = belt.shoes[(i + offset) % ShoeCount];
                    const auto& b = belt.shoes[(i + offset + 1) % ShoeCount];
                    XMStoreFloat3(&pose->translation_local, XMVectorLerp(
                        XMLoadFloat3(&a.restPosition), XMLoadFloat3(&b.restPosition), t));
                    XMStoreFloat4(&pose->rotation_local, XMQuaternionSlerp(
                        XMLoadFloat4(&a.restRotation), XMLoadFloat4(&b.restRotation), t));
                    pose->SetDirty();
                }
            }
        }
    };
}
