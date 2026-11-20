#pragma once

#include "renegade/bridge/FixedWingRig.h"
#include "renegade/bridge/GameplayInputService.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace renegade::runtime
{
    struct BastionTurretShot
    {
        wi::ecs::Entity chassis = wi::ecs::INVALID_ENTITY;
        XMFLOAT3 origin{}, direction{};
    };

    struct RuntimeBastionTurret
    {
        wi::ecs::Entity mount = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity elevation = wi::ecs::INVALID_ENTITY;
        XMFLOAT4 mountRest{}, elevationRest{};
        XMFLOAT3 aimPoint{};
        float traverse = 0.f, pitch = 0.f, cooldown = 0.f;
        float traverseSpeed = XM_PIDIV2, elevationSpeed = .85f;
        float minimumPitch = XMConvertToRadians(-10.f), maximumPitch = XMConvertToRadians(25.f);
        bool aimInitialized = false, sight = false;

        void Bind(const wi::scene::Scene& scene, wi::ecs::Entity yaw, wi::ecs::Entity barrel)
        {
            const auto* yawPose = scene.transforms.GetComponent(yaw);
            const auto* barrelPose = scene.transforms.GetComponent(barrel);
            const auto* parent = scene.hierarchy.GetComponent(barrel);
            if (!yawPose || !barrelPose || !parent || parent->parentID != yaw) return;
            mount = yaw;
            elevation = barrel;
            mountRest = yawPose->rotation_local;
            elevationRest = barrelPose->rotation_local;
            if (const auto* metadata = scene.metadatas.GetComponent(yaw))
            {
                const auto degrees = [&](const char* key, float fallback, float lo, float hi) {
                    const auto& values = metadata->float_values;
                    const float value = values.has(key) ? values.get(key) : fallback;
                    return XMConvertToRadians(std::isfinite(value) ? std::clamp(value, lo, hi) : fallback);
                };
                traverseSpeed = degrees("renegade.turret.traverse_deg_s", 90.f, 1.f, 360.f);
                elevationSpeed = degrees("renegade.turret.elevation_deg_s", 48.7f, 1.f, 180.f);
                minimumPitch = degrees("renegade.turret.min_elevation_deg", -10.f, -45.f, 0.f);
                maximumPitch = degrees("renegade.turret.max_elevation_deg", 25.f, 0.f, 70.f);
            }
        }

        bool Bound() const { return mount != wi::ecs::INVALID_ENTITY; }

        void ResetAim()
        {
            aimInitialized = false;
            sight = false;
        }

        bool Muzzle(const wi::scene::Scene& scene, wi::ecs::Entity chassis,
            BastionTurretShot& shot) const
        {
            if (!Bound()) return false;
            XMMATRIX barrel;
            const auto* hull = scene.transforms.GetComponent(chassis);
            if (!hull || !bridge::FixedWingPartTransform(scene, elevation, chassis, barrel))
                return false;
            const auto world = barrel * XMLoadFloat4x4(&hull->world);
            // The imported Bastion barrel faces local -Z. Its visible tip is
            // 3.467 m past the pivot, measured from the authored mesh.
            XMStoreFloat3(&shot.origin, XMVector3TransformCoord(XMVectorSet(0, 0, -3.49f, 1), world));
            XMStoreFloat3(&shot.direction, XMVector3Normalize(-world.r[2]));
            shot.chassis = chassis;
            return true;
        }

        std::optional<BastionTurretShot> Update(wi::scene::Scene& scene, wi::ecs::Entity chassis,
            const bridge::GameplayInputFrame& input, float dt)
        {
            auto* yawPose = scene.transforms.GetComponent(mount);
            auto* barrelPose = scene.transforms.GetComponent(elevation);
            const auto* hull = scene.transforms.GetComponent(chassis);
            const auto* parent = scene.hierarchy.GetComponent(mount);
            XMMATRIX parentLocal;
            if (!yawPose || !barrelPose || !hull || !parent ||
                !bridge::FixedWingPartTransform(scene, parent->parentID, chassis, parentLocal))
                return {};

            const auto hullWorld = XMLoadFloat4x4(&hull->world);
            const auto origin = XMVector3TransformCoord(XMVectorSet(0, 1.65f, 0, 1), hullWorld);
            if (!aimInitialized)
            {
                XMStoreFloat3(&aimPoint, origin + XMVector3Normalize(hullWorld.r[2]) * 160.f);
                aimInitialized = true;
            }
            if (input.player.lookYaw != 0.f || input.player.lookPitch != 0.f ||
                XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&aimPoint) - origin)) < 4.f)
            {
                XMFLOAT3 current;
                XMStoreFloat3(&current, XMLoadFloat3(&aimPoint) - origin);
                const float azimuth = std::atan2(current.x, current.z) + input.player.lookYaw;
                const float altitude = std::clamp(std::atan2(current.y, std::hypot(current.x, current.z)) -
                    input.player.lookPitch, -.9f, .9f);
                const auto direction = XMVectorSet(std::sin(azimuth) * std::cos(altitude), std::sin(altitude),
                    std::cos(azimuth) * std::cos(altitude), 0);
                XMStoreFloat3(&aimPoint, origin + direction * 160.f);
            }

            const auto neutralYaw = XMMatrixRotationQuaternion(XMLoadFloat4(&mountRest)) *
                XMMatrixTranslationFromVector(XMLoadFloat3(&yawPose->translation_local)) * parentLocal * hullWorld;
            XMFLOAT3 yawTarget;
            XMStoreFloat3(&yawTarget, XMVector3TransformCoord(XMLoadFloat3(&aimPoint),
                XMMatrixInverse(nullptr, neutralYaw)));
            const float step = std::clamp(dt, 0.f, .1f);
            const float yawError = std::remainder(std::atan2(-yawTarget.x, -yawTarget.z) - traverse, XM_2PI);
            traverse = std::remainder(traverse + std::clamp(yawError, -step * traverseSpeed, step * traverseSpeed), XM_2PI);
            XMStoreFloat4(&yawPose->rotation_local, XMQuaternionMultiply(
                XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), traverse), XMLoadFloat4(&mountRest)));
            yawPose->SetDirty();

            XMMATRIX yawLocal;
            if (!bridge::FixedWingPartTransform(scene, mount, chassis, yawLocal)) return {};
            const auto neutralBarrel = XMMatrixRotationQuaternion(XMLoadFloat4(&elevationRest)) *
                XMMatrixTranslationFromVector(XMLoadFloat3(&barrelPose->translation_local)) * yawLocal * hullWorld;
            XMFLOAT3 barrelTarget;
            XMStoreFloat3(&barrelTarget, XMVector3TransformCoord(XMLoadFloat3(&aimPoint),
                XMMatrixInverse(nullptr, neutralBarrel)));
            const float limit = std::clamp(std::atan2(barrelTarget.y, std::hypot(barrelTarget.x, barrelTarget.z)),
                minimumPitch, maximumPitch);
            pitch += std::clamp(limit - pitch, -step * elevationSpeed, step * elevationSpeed);
            XMStoreFloat4(&barrelPose->rotation_local, XMQuaternionMultiply(
                XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), pitch), XMLoadFloat4(&elevationRest)));
            barrelPose->SetDirty();

            sight = input.aimDown;
            cooldown = std::max(0.f, cooldown - step);
            BastionTurretShot shot;
            if (!input.firePressed || cooldown > 0.f || !Muzzle(scene, chassis, shot)) return {};
            cooldown = .85f;
            return shot;
        }
    };
}
