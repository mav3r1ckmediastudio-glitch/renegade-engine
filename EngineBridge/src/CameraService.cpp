#include "renegade/bridge/CameraService.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    constexpr float Epsilon = 0.00001f;
    constexpr float RadiansToDegrees = 180.0f / XM_PI;
    constexpr float DegreesToRadians = XM_PI / 180.0f;

    bool NearlyEqual(const float left, const float right) noexcept
    {
        return std::abs(left - right) <= Epsilon;
    }

    bool EntityExists(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return false;
        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

    void ApplyTransformState(
        wi::scene::TransformComponent& transform,
        const renegade::bridge::TransformState& state) noexcept
    {
        transform.translation_local = state.translation;
        transform.rotation_local = state.rotation;
        transform.scale_local = state.scale;
        transform.SetDirty();
        transform.UpdateTransform();
    }
}

namespace renegade::bridge
{
    CameraState CaptureCamera(
        const wi::scene::CameraComponent& camera) noexcept
    {
        CameraState state;
        state.orthographic = camera.IsOrtho();
        state.fieldOfViewDegrees = camera.fov * RadiansToDegrees;
        state.nearPlane = camera.zNearP;
        state.farPlane = camera.zFarP;
        state.focalLength = camera.focal_length;
        state.apertureSize = camera.aperture_size;
        state.orthoVerticalSize = camera.ortho_vertical_size;
        return state;
    }

    CameraState SanitizeCameraState(const CameraState& state) noexcept
    {
        CameraState result = state;
        result.fieldOfViewDegrees = std::clamp(
            result.fieldOfViewDegrees, 1.0f, 179.0f);
        result.nearPlane = std::clamp(result.nearPlane, 0.001f, 10000.0f);
        result.farPlane = std::clamp(
            result.farPlane,
            result.nearPlane + 0.001f,
            1000000.0f);
        result.focalLength = std::clamp(result.focalLength, 0.001f, 100000.0f);
        result.apertureSize = std::clamp(result.apertureSize, 0.0f, 1.0f);
        result.orthoVerticalSize = std::clamp(
            result.orthoVerticalSize, 0.001f, 1000000.0f);
        return result;
    }

    bool HasCameraStateChange(
        const CameraState& before,
        const CameraState& after) noexcept
    {
        const auto left = SanitizeCameraState(before);
        const auto right = SanitizeCameraState(after);
        return left.orthographic != right.orthographic ||
            !NearlyEqual(left.fieldOfViewDegrees, right.fieldOfViewDegrees) ||
            !NearlyEqual(left.nearPlane, right.nearPlane) ||
            !NearlyEqual(left.farPlane, right.farPlane) ||
            !NearlyEqual(left.focalLength, right.focalLength) ||
            !NearlyEqual(left.apertureSize, right.apertureSize) ||
            !NearlyEqual(left.orthoVerticalSize, right.orthoVerticalSize);
    }

    void ApplyCamera(
        wi::scene::CameraComponent& camera,
        const CameraState& state) noexcept
    {
        const auto safe = SanitizeCameraState(state);
        camera.SetOrtho(safe.orthographic);
        camera.fov = safe.fieldOfViewDegrees * DegreesToRadians;
        camera.zNearP = safe.nearPlane;
        camera.zFarP = safe.farPlane;
        camera.focal_length = safe.focalLength;
        camera.aperture_size = safe.apertureSize;
        camera.ortho_vertical_size = safe.orthoVerticalSize;
        camera.UpdateCamera();
        camera.SetDirty();
    }

    CreateCameraCommand::CreateCameraCommand(
        wi::scene::Scene& scene,
        const CameraState& state,
        const TransformState& transform,
        const float width,
        const float height)
        : scene_(&scene)
        , state_(SanitizeCameraState(state))
        , transform_(transform)
        , width_(std::max(width, 1.0f))
        , height_(std::max(height, 1.0f))
    {
    }

    bool CreateCameraCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;

        if (hasSnapshot_)
        {
            if (EntityExists(*scene_, entity_))
                return false;
            snapshot_.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer serializer;
            serializer.allow_remap = false;
            return scene_->Entity_Serialize(snapshot_, serializer) == entity_;
        }

        entity_ = scene_->Entity_CreateCamera(
            MakeUniqueName(),
            width_,
            height_,
            state_.nearPlane,
            state_.farPlane,
            state_.fieldOfViewDegrees * DegreesToRadians);
        auto* authoredCamera = scene_->cameras.GetComponent(entity_);
        auto* transform = scene_->transforms.GetComponent(entity_);
        if (entity_ == wi::ecs::INVALID_ENTITY || authoredCamera == nullptr ||
            transform == nullptr)
        {
            if (entity_ != wi::ecs::INVALID_ENTITY)
                scene_->Entity_Remove(entity_);
            entity_ = wi::ecs::INVALID_ENTITY;
            return false;
        }

        ApplyCamera(*authoredCamera, state_);
        ApplyTransformState(*transform, transform_);

        snapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer serializer;
        scene_->Entity_Serialize(snapshot_, serializer, entity_);
        hasSnapshot_ = true;
        return true;
    }

    void CreateCameraCommand::Undo()
    {
        if (scene_ != nullptr && EntityExists(*scene_, entity_))
            scene_->Entity_Remove(entity_);
    }

    wi::ecs::Entity CreateCameraCommand::CreatedEntity() const noexcept
    {
        return entity_;
    }

    std::string CreateCameraCommand::MakeUniqueName() const
    {
        const std::string base = "Camera";
        std::string candidate = base;
        int suffix = 2;
        bool collision = true;
        while (collision)
        {
            collision = false;
            for (std::size_t index = 0; index < scene_->names.GetCount(); ++index)
            {
                if (scene_->names[index].name == candidate)
                {
                    collision = true;
                    candidate = base + " " + std::to_string(suffix++);
                    break;
                }
            }
        }
        return candidate;
    }

    SetCameraCommand::SetCameraCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const CameraState& camera)
        : scene_(&scene)
        , entity_(entity)
        , after_(SanitizeCameraState(camera))
    {
        if (const auto* existing = scene.cameras.GetComponent(entity))
            before_ = CaptureCamera(*existing);
    }

    SetCameraCommand::SetCameraCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const CameraState& before,
        const CameraState& after)
        : scene_(&scene)
        , entity_(entity)
        , before_(SanitizeCameraState(before))
        , after_(SanitizeCameraState(after))
    {
    }

    bool SetCameraCommand::Execute()
    {
        return HasCameraStateChange(before_, after_) && Apply(after_);
    }

    void SetCameraCommand::Undo()
    {
        Apply(before_);
    }

    bool SetCameraCommand::Apply(const CameraState& state) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* authoredCamera = scene_->cameras.GetComponent(entity_);
        if (authoredCamera == nullptr)
            return false;
        ApplyCamera(*authoredCamera, state);
        return true;
    }
}
