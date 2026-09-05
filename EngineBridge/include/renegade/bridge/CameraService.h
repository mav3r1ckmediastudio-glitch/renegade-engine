#pragma once

#include <string>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    struct CameraState
    {
        bool orthographic = false;
        float fieldOfViewDegrees = 60.0f;
        float nearPlane = 0.1f;
        float farPlane = 5000.0f;
        float focalLength = 1.0f;
        float apertureSize = 0.0f;
        float orthoVerticalSize = 10.0f;
    };

    [[nodiscard]] CameraState CaptureCamera(
        const wi::scene::CameraComponent& camera) noexcept;
    [[nodiscard]] CameraState SanitizeCameraState(
        const CameraState& state) noexcept;
    [[nodiscard]] bool HasCameraStateChange(
        const CameraState& before,
        const CameraState& after) noexcept;

    // Applies only creator-facing projection and depth-of-field state. Viewport
    // dimensions and transform remain owned by their existing systems.
    void ApplyCamera(
        wi::scene::CameraComponent& camera,
        const CameraState& state) noexcept;

    class CreateCameraCommand final : public ICommand
    {
    public:
        CreateCameraCommand(
            wi::scene::Scene& scene,
            const CameraState& state,
            const TransformState& transform,
            float width,
            float height);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept;

    private:
        [[nodiscard]] std::string MakeUniqueName() const;

        wi::scene::Scene* scene_ = nullptr;
        CameraState state_;
        TransformState transform_;
        float width_ = 1280.0f;
        float height_ = 720.0f;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };

    class SetCameraCommand final : public ICommand
    {
    public:
        SetCameraCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const CameraState& camera);
        SetCameraCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const CameraState& before,
            const CameraState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const CameraState& state) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        CameraState before_;
        CameraState after_;
    };
}
