#pragma once

#include "renegade/bridge/FixedWingTuning.h"
#include "renegade/bridge/FixedWingRig.h"

namespace renegade::runtime
{
    struct RuntimeFixedWing
    {
        using Part = bridge::FixedWingPart;
        std::array<wi::ecs::Entity, bridge::FixedWingParts.size()> parts{};
        std::array<XMFLOAT4, bridge::FixedWingParts.size()> restAngles{};
        float throttle = 0.f;
        float pitchTarget = 0.f;
        float mouseTurn = 0.f;
        bridge::FixedWingTuning tuning;

        bridge::FixedWingInput SampleMouseControls(float lookYaw, float lookPitch, float dt,
            bool onGround, const XMFLOAT4& orientation)
        {
            const float vertical = std::isfinite(lookPitch) ? lookPitch : 0.f;
            const float horizontal = std::isfinite(lookYaw) ? lookYaw : 0.f;
            pitchTarget = std::clamp(pitchTarget - vertical * .74f * tuning.pitchResponse, -.20f, .30f);
            mouseTurn = bridge::FlightControl(mouseTurn * std::exp(-1.65f * dt) + horizontal * 4.2f * tuning.bankResponse);

            const auto basis = XMMatrixRotationQuaternion(XMLoadFloat4(&orientation));
            const float pitch = std::asin(std::clamp(XMVectorGetY(basis.r[2]), -1.f, 1.f));
            const float bank = std::atan2(-XMVectorGetY(basis.r[0]), XMVectorGetY(basis.r[1]));
            const float bankTarget = mouseTurn * .42f;
            const float elevator = bridge::FlightControl((pitchTarget - pitch) * 7.f);
            const float ailerons = std::clamp((bankTarget - bank) * 2.6f, -.36f, .36f);
            return {throttle, elevator, onGround ? 0.f : ailerons,
                mouseTurn * (onGround ? .65f : .14f)};
        }
        float propeller = 0.f;
        std::array<float, 3> wheels{};

        bool Register(wi::scene::Scene& scene, wi::ecs::Entity chassis)
        {
            tuning = bridge::ReadFixedWingTuning(scene, chassis);
            bridge::FixedWingAircraft aircraft;
            aircraft.aerodynamics.maxThrust = tuning.propThrust;
            aircraft.aerodynamics.propellerPitchSpeed = tuning.thrustFadeSpeed;
            aircraft.aerodynamics.portWing.area = aircraft.aerodynamics.starboardWing.area = tuning.wingArea * .5f;
            aircraft.aerodynamics.portWing.incidence = aircraft.aerodynamics.starboardWing.incidence =
                XMConvertToRadians(tuning.wingIncidence);
            aircraft.aerodynamics.fuselageDragArea = tuning.dragArea;
            aircraft.aerodynamics.wingStallAngle = XMConvertToRadians(tuning.stallAngle);
            aircraft.gearSpringScale = tuning.gearSpring;
            aircraft.gearDampingScale = tuning.gearDamping;
            const auto rig = bridge::ReadFixedWingRig(scene, chassis);
            for (size_t i = 0; i < parts.size(); ++i)
            {
                parts[i] = rig.Part(i);
                if (const auto* transform = scene.transforms.GetComponent(parts[i]))
                    restAngles[i] = transform->rotation_local;
            }
            constexpr auto firstContact = static_cast<size_t>(Part::NoseContact);
            constexpr auto firstWheel = static_cast<size_t>(Part::NoseWheel);
            for (size_t i = 0; i < aircraft.gear.size(); ++i)
            {
                XMMATRIX local;
                float radius = 0.f;
                if (!bridge::FixedWingPartTransform(scene, parts[firstContact + i], chassis, local))
                {
                    if (!bridge::FixedWingPartTransform(scene, parts[firstWheel + i], chassis, local))
                        return false;
                    radius = i == 0 ? tuning.noseWheelRadius : tuning.mainWheelRadius;
                }
                const auto point = local.r[3];
                aircraft.gear[i] = {{XMVectorGetX(point), XMVectorGetY(point) - radius, XMVectorGetZ(point)}, i == 0};
            }

            const std::array forcePoints{
                &aircraft.aerodynamics.portWing.centre, &aircraft.aerodynamics.starboardWing.centre,
                &aircraft.aerodynamics.tail.centre, &aircraft.aerodynamics.fin.centre,
                &aircraft.aerodynamics.propeller
            };
            const auto firstForce = static_cast<size_t>(Part::PortLift);
            for (size_t i = 0; i < forcePoints.size(); ++i)
            {
                XMMATRIX local;
                if (bridge::FixedWingPartTransform(scene, parts[firstForce + i], chassis, local))
                {
                    const auto point = local.r[3];
                    *forcePoints[i] = {XMVectorGetX(point), XMVectorGetY(point), XMVectorGetZ(point)};
                }
            }

            return bridge::RegisterFixedWing(scene, chassis, aircraft);
        }

        void Animate(wi::scene::Scene& scene, Part part, XMVECTOR axis, float radians)
        {
            const auto index = static_cast<size_t>(part);
            if (auto* transform = scene.transforms.GetComponent(parts[index]))
            {
                XMStoreFloat4(&transform->rotation_local, XMQuaternionMultiply(
                    XMQuaternionRotationAxis(axis, radians), XMLoadFloat4(&restAngles[index])));
                transform->SetDirty();
            }
        }

        void UpdateVisuals(wi::scene::Scene& scene, wi::ecs::Entity chassis, bridge::FixedWingInput input,
            const bridge::FixedWingTelemetry& telemetry, float dt)
        {
            const auto x = XMVectorSet(1.f, 0.f, 0.f, 0.f);
            const auto y = XMVectorSet(0.f, 1.f, 0.f, 0.f);
            const auto z = XMVectorSet(0.f, 0.f, 1.f, 0.f);
            Animate(scene, Part::PortAileron, x, input.roll * .32f);
            Animate(scene, Part::StarboardAileron, x, input.roll * -.32f);
            Animate(scene, Part::Elevator, x, input.pitch * -.40f);
            Animate(scene, Part::Rudder, y, input.yaw * -.40f);
            Animate(scene, Part::Steering, y, input.yaw * .32f);

            propeller = std::remainder(propeller + throttle * 125.f * dt, XM_2PI);
            Animate(scene, Part::Propeller, z, propeller);
            if (telemetry.groundContacts == 0) return;
            const auto* transform = scene.transforms.GetComponent(chassis);
            auto* body = scene.rigidbodies.GetComponent(chassis);
            if (!transform || !body || !body->physicsobject) return;
            const auto velocity = wi::physics::GetVelocity(*body);
            const auto forward = XMVector3Normalize(XMLoadFloat4x4(&transform->world).r[2]);
            const auto groundSpeed = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&velocity), forward));
            const std::array<float, 3> radii{tuning.noseWheelRadius, tuning.mainWheelRadius, tuning.mainWheelRadius};
            for (size_t i = 0; i < wheels.size(); ++i)
            {
                wheels[i] = std::remainder(wheels[i] - groundSpeed * dt / radii[i], XM_2PI);
                Animate(scene, static_cast<Part>(static_cast<size_t>(Part::NoseWheel) + i), x, wheels[i]);
            }
        }
    };
}
