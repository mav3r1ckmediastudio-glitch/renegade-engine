#pragma once

#include "CommandService.h"
#include "FixedWingRig.h"

#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace renegade::bridge
{
    struct VehicleCockpitCamera
    {
        XMFLOAT3 position{};
        XMFLOAT3 angles{}; // Pitch, yaw, roll in degrees.
        float verticalFov = 60.f;
    };

    inline constexpr float MinimumCockpitFov = 15.f;
    inline constexpr float MaximumCockpitFov = 125.f;
    inline constexpr char CockpitCameraEnabled[] = "renegade.vehicle.cockpit_camera";
    inline constexpr std::array<const char*, 7> CockpitCameraKeys{
        "renegade.vehicle.cockpit_x", "renegade.vehicle.cockpit_y", "renegade.vehicle.cockpit_z",
        "renegade.vehicle.cockpit_pitch", "renegade.vehicle.cockpit_yaw", "renegade.vehicle.cockpit_roll",
        "renegade.vehicle.cockpit_vertical_fov",
    };
    using CockpitCameraFields = std::array<float, CockpitCameraKeys.size()>;

    inline CockpitCameraFields CockpitFields(const VehicleCockpitCamera& value)
    {
        return {value.position.x, value.position.y, value.position.z,
            value.angles.x, value.angles.y, value.angles.z, value.verticalFov};
    }

    inline VehicleCockpitCamera CockpitFromFields(CockpitCameraFields values)
    {
        for (size_t i = 0; i < 3; ++i)
            values[i] = std::isfinite(values[i]) ? std::clamp(values[i], -100.f, 100.f) : 0.f;
        for (size_t i = 3; i < 6; ++i)
            values[i] = std::isfinite(values[i]) ? std::remainder(values[i], 360.f) : 0.f;
        const auto fov = std::isfinite(values[6]) ? values[6] : 60.f;
        return {{values[0], values[1], values[2]}, {values[3], values[4], values[5]},
            std::clamp(fov, MinimumCockpitFov, MaximumCockpitFov)};
    }

    inline float CockpitLensMillimetres(float verticalFov)
    {
        // A 36 x 24 mm reference frame. Only the vertical angle is stored.
        return 12.f / std::tan(XMConvertToRadians(verticalFov) * .5f);
    }

    inline float CockpitFovFromLens(float millimetres)
    {
        return XMConvertToDegrees(2.f * std::atan2(12.f, millimetres));
    }

    inline bool SupportsCockpitCamera(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        return scene.transforms.Contains(entity) && scene.rigidbodies.Contains(entity) &&
            GetPlayerVehicleMode(scene, entity) != PlayerVehicleMode::Disabled;
    }

    inline wi::ecs::Entity ResolveCockpitCameraChassis(const wi::scene::Scene& scene, wi::ecs::Entity selected)
    {
        for (auto remaining = scene.hierarchy.GetCount() + 1;
            remaining && selected != wi::ecs::INVALID_ENTITY; --remaining)
        {
            if (scene.rigidbodies.Contains(selected))
                return SupportsCockpitCamera(scene, selected) ? selected : wi::ecs::INVALID_ENTITY;
            const auto* link = scene.hierarchy.GetComponent(selected);
            if (!link) break;
            selected = link->parentID;
        }
        return wi::ecs::INVALID_ENTITY;
    }

    inline std::vector<wi::ecs::Entity> FindCockpitGlass(const wi::scene::Scene& scene, wi::ecs::Entity chassis)
    {
        std::vector<wi::ecs::Entity> glass;
        const bool opaqueCanopy = IsPhysicalVtol(scene, chassis);
        for (size_t i = 0; i < scene.objects.GetCount(); ++i)
        {
            if (!scene.objects[i].IsRenderable()) continue;
            const auto entity = scene.objects.GetEntity(i);
            auto part = entity;
            bool lens = false;
            for (unsigned depth = 0; depth < 16 && part != wi::ecs::INVALID_ENTITY; ++depth)
            {
                if (part == chassis)
                {
                    if (lens) glass.push_back(entity);
                    break;
                }
                if (const auto* name = scene.names.GetComponent(part))
                {
                    const auto& label = name->name;
                    lens = lens || label == "Windshield" || (opaqueCanopy && label == "Canopy_Glazing") ||
                        label == "Window1" || label == "Window2" || label == "Window3" || label == "Window4";
                }
                const auto* parent = scene.hierarchy.GetComponent(part);
                part = parent ? parent->parentID : wi::ecs::INVALID_ENTITY;
            }
        }
        return glass;
    }

    inline std::optional<VehicleCockpitCamera> ReadVehicleCockpitCamera(
        const wi::scene::Scene& scene, wi::ecs::Entity chassis)
    {
        const auto* metadata = scene.metadatas.GetComponent(chassis);
        if (!metadata || !metadata->bool_values.has(CockpitCameraEnabled) ||
            !metadata->bool_values.get(CockpitCameraEnabled))
            return {};
        CockpitCameraFields values{0, 0, 0, 0, 0, 0, 60.f};
        for (size_t i = 0; i < values.size(); ++i)
            if (metadata->float_values.has(CockpitCameraKeys[i]))
                values[i] = metadata->float_values.get(CockpitCameraKeys[i]);
        return CockpitFromFields(values);
    }

    inline wi::ecs::Entity ResolveDefaultCockpitMarker(wi::scene::Scene& scene, wi::ecs::Entity chassis)
    {
        if (IsFixedWing(scene, chassis))
            return ReadFixedWingRig(scene, chassis).Part(static_cast<size_t>(FixedWingPart::PilotEye));
        if (IsTrackedVehicle(scene, chassis))
            return scene.Entity_FindByName("REF_Driver_View", chassis);
        if (IsForceFlightVehicle(scene, chassis))
            return scene.Entity_FindByName(IsPhysicalVtol(scene, chassis) ? "REF_Pilot_Eye" : "REF_Eye_Pilot", chassis);
        return wi::ecs::INVALID_ENTITY;
    }

    inline VehicleCockpitCamera DefaultVehicleCockpitCamera(const wi::scene::Scene& scene,
        wi::ecs::Entity chassis, wi::ecs::Entity marker, float verticalFov)
    {
        const bool flight = IsFlightVehicle(scene, chassis);
        VehicleCockpitCamera value{
            flight ? XMFLOAT3{0, .91f, .64f} : XMFLOAT3{-.31f, .52f, -.02f},
            {XMConvertToDegrees(flight ? std::atan2(.81f, 9.36f) : std::atan2(1.22f, 10.02f)), 0, 0},
            verticalFov};
        XMMATRIX local;
        if (marker != wi::ecs::INVALID_ENTITY && FixedWingPartTransform(scene, marker, chassis, local))
        {
            XMStoreFloat3(&value.position, local.r[3]);
            value.angles = {};
            if (IsTrackedVehicle(scene, chassis))
            {
                value.position.y += .22f;
                value.position.z += .12f;
                value.angles.x = XMConvertToDegrees(-std::atan2(.05f, 12.f));
            }
        }
        return value;
    }

    inline XMMATRIX CockpitChassisOrientation(const XMMATRIX& world)
    {
        XMVECTOR scale, rotation, position;
        if (XMMatrixDecompose(&scale, &rotation, &position, world))
            return XMMatrixRotationQuaternion(rotation);
        return XMMatrixInverse(nullptr, XMMatrixLookToLH(XMVectorZero(), world.r[2], world.r[1]));
    }

    inline bool CaptureVehicleCockpitFromView(const wi::scene::TransformComponent& chassis,
        const wi::scene::CameraComponent& camera, VehicleCockpitCamera& value)
    {
        XMVECTOR determinant;
        const auto world = XMLoadFloat4x4(&chassis.world);
        const auto inverse = XMMatrixInverse(&determinant, world);
        const float det = XMVectorGetX(determinant);
        if (!std::isfinite(det) || std::abs(det) < 1e-7f) return false;
        const auto local = camera.GetInvView() * XMMatrixTranspose(CockpitChassisOrientation(world));
        const float cosPitch = std::hypot(XMVectorGetX(local.r[2]), XMVectorGetZ(local.r[2]));
        const float pitch = std::atan2(-XMVectorGetY(local.r[2]), cosPitch);
        const float yaw = cosPitch > 1e-5f ? std::atan2(XMVectorGetX(local.r[2]), XMVectorGetZ(local.r[2])) :
            std::atan2(-XMVectorGetZ(local.r[0]), XMVectorGetX(local.r[0]));
        const float roll = cosPitch > 1e-5f ? std::atan2(XMVectorGetY(local.r[0]), XMVectorGetY(local.r[1])) : 0.f;
        XMStoreFloat3(&value.position, XMVector3TransformCoord(XMLoadFloat3(&camera.Eye), inverse));
        value.angles = {XMConvertToDegrees(pitch), XMConvertToDegrees(yaw), XMConvertToDegrees(roll)};
        value.verticalFov = XMConvertToDegrees(camera.fov);
        value = CockpitFromFields(CockpitFields(value));
        return true;
    }

    inline void ApplyVehicleCockpitCamera(wi::scene::CameraComponent& camera,
        const wi::scene::TransformComponent& chassis, const VehicleCockpitCamera& value, bool useFov)
    {
        const auto local = XMMatrixRotationRollPitchYaw(XMConvertToRadians(value.angles.x),
            XMConvertToRadians(value.angles.y), XMConvertToRadians(value.angles.z));
        const auto chassisWorld = XMLoadFloat4x4(&chassis.world);
        const auto orientation = local * CockpitChassisOrientation(chassisWorld);
        const auto eye = XMVector3TransformCoord(XMLoadFloat3(&value.position), chassisWorld);
        const auto view = XMMatrixLookToLH(eye, orientation.r[2], orientation.r[1]);
        wi::scene::TransformComponent pose;
        pose.MatrixTransform(XMMatrixInverse(nullptr, view));
        pose.UpdateTransform();
        if (useFov) camera.fov = XMConvertToRadians(value.verticalFov);
        camera.TransformCamera(pose);
        camera.UpdateCamera();
    }

    class SetVehicleCockpitCameraCommand final : public ICommand
    {
    public:
        SetVehicleCockpitCameraCommand(wi::scene::Scene& scene, wi::ecs::Entity chassis,
            const std::optional<VehicleCockpitCamera>& after)
            : scene_(scene), chassis_(chassis), before_(ReadVehicleCockpitCamera(scene, chassis)),
              after_(after ? std::optional<VehicleCockpitCamera>(CockpitFromFields(CockpitFields(*after))) : std::nullopt) {}

        bool Execute() override
        {
            if (!SupportsCockpitCamera(scene_, chassis_) || (!before_ && !after_) ||
                (before_ && after_ && CockpitFields(*before_) == CockpitFields(*after_))) return false;
            Write(after_);
            return true;
        }

        void Undo() override { Write(before_); }

    private:
        void Write(const std::optional<VehicleCockpitCamera>& value)
        {
            auto* metadata = scene_.metadatas.GetComponent(chassis_);
            if (value)
            {
                if (!metadata) metadata = &scene_.metadatas.Create(chassis_);
                const auto fields = CockpitFields(*value);
                metadata->bool_values.set(CockpitCameraEnabled, true);
                for (size_t i = 0; i < fields.size(); ++i)
                    metadata->float_values.set(CockpitCameraKeys[i], fields[i]);
            }
            else if (metadata)
            {
                metadata->bool_values.erase(CockpitCameraEnabled);
                for (const auto* key : CockpitCameraKeys) metadata->float_values.erase(key);
            }
        }

        wi::scene::Scene& scene_;
        wi::ecs::Entity chassis_;
        std::optional<VehicleCockpitCamera> before_;
        std::optional<VehicleCockpitCamera> after_;
    };
}
