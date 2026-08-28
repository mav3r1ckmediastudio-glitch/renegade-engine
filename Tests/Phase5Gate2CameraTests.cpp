#include "renegade/bridge/CameraService.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
    bool Near(const float left, const float right, const float epsilon = 0.001f)
    {
        return std::abs(left - right) <= epsilon;
    }

    [[noreturn]] void Fail(const std::string& message)
    {
        std::cerr << "PHASE5 GATE2 CAMERA FAIL // " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    using namespace renegade::bridge;

    wi::scene::Scene scene;
    const auto existing = scene.Entity_CreateCamera(
        "Camera", 1280.0f, 720.0f, 0.1f, 1000.0f, XM_PIDIV4);
    if (existing == wi::ecs::INVALID_ENTITY)
        Fail("fixture camera creation failed");

    CameraState authored;
    authored.orthographic = false;
    authored.fieldOfViewDegrees = 72.0f;
    authored.nearPlane = 0.05f;
    authored.farPlane = 8000.0f;
    authored.focalLength = 7.5f;
    authored.apertureSize = 0.35f;
    authored.orthoVerticalSize = 24.0f;

    TransformState pose;
    pose.translation = XMFLOAT3(4.0f, 6.0f, -12.0f);
    pose.rotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

    CommandService commands;
    auto create = std::make_unique<CreateCameraCommand>(
        scene, authored, pose, 1920.0f, 1080.0f);
    auto* createRaw = create.get();
    if (!commands.Execute(std::move(create)))
        Fail("CreateCameraCommand did not execute");

    const auto entity = createRaw->CreatedEntity();
    if (entity == wi::ecs::INVALID_ENTITY)
        Fail("CreateCameraCommand returned invalid entity");

    const auto* name = scene.names.GetComponent(entity);
    if (name == nullptr || name->name != "Camera 2")
        Fail("camera naming was not unique and creator-facing");

    const auto* camera = scene.cameras.GetComponent(entity);
    const auto* transform = scene.transforms.GetComponent(entity);
    if (camera == nullptr || transform == nullptr)
        Fail("native CameraComponent or TransformComponent missing");

    const auto created = CaptureCamera(*camera);
    if (created.orthographic ||
        !Near(created.fieldOfViewDegrees, 72.0f) ||
        !Near(created.nearPlane, 0.05f) ||
        !Near(created.farPlane, 8000.0f) ||
        !Near(created.focalLength, 7.5f) ||
        !Near(created.apertureSize, 0.35f) ||
        !Near(created.orthoVerticalSize, 24.0f))
    {
        Fail("created camera did not preserve native authored state");
    }
    if (!Near(transform->translation_local.x, 4.0f) ||
        !Near(transform->translation_local.y, 6.0f) ||
        !Near(transform->translation_local.z, -12.0f))
    {
        Fail("create-from-view transform was not stored on the scene camera");
    }

    const float widthBeforeEdit = camera->width;
    const float heightBeforeEdit = camera->height;
    CameraState edited = created;
    edited.orthographic = true;
    edited.fieldOfViewDegrees = 55.0f;
    edited.nearPlane = 0.2f;
    edited.farPlane = 2500.0f;
    edited.focalLength = 12.0f;
    edited.apertureSize = 0.7f;
    edited.orthoVerticalSize = 40.0f;

    if (!commands.Execute(
            std::make_unique<SetCameraCommand>(scene, entity, edited)))
    {
        Fail("SetCameraCommand did not execute");
    }

    camera = scene.cameras.GetComponent(entity);
    const auto changed = CaptureCamera(*camera);
    if (!changed.orthographic ||
        !Near(changed.fieldOfViewDegrees, 55.0f) ||
        !Near(changed.nearPlane, 0.2f) ||
        !Near(changed.farPlane, 2500.0f) ||
        !Near(changed.focalLength, 12.0f) ||
        !Near(changed.apertureSize, 0.7f) ||
        !Near(changed.orthoVerticalSize, 40.0f))
    {
        Fail("camera edit did not map to native Wicked fields");
    }
    if (!Near(camera->width, widthBeforeEdit) ||
        !Near(camera->height, heightBeforeEdit))
    {
        Fail("camera property edit destroyed viewport dimensions");
    }

    if (!commands.Undo())
        Fail("camera property Undo failed");
    const auto undone = CaptureCamera(*scene.cameras.GetComponent(entity));
    if (undone.orthographic || !Near(undone.fieldOfViewDegrees, 72.0f) ||
        !Near(undone.apertureSize, 0.35f))
    {
        Fail("camera property Undo did not restore prior state");
    }

    if (!commands.Redo())
        Fail("camera property Redo failed");
    const auto redone = CaptureCamera(*scene.cameras.GetComponent(entity));
    if (!redone.orthographic || !Near(redone.orthoVerticalSize, 40.0f))
        Fail("camera property Redo did not restore edited state");

    if (!commands.Undo())
        Fail("camera property Undo before create Undo failed");
    if (!commands.Undo())
        Fail("camera create Undo failed");
    if (scene.cameras.Contains(entity))
        Fail("camera create Undo did not remove the camera");

    if (!commands.Redo())
        Fail("camera create Redo failed");
    if (!scene.cameras.Contains(entity))
        Fail("camera create Redo did not restore the same camera entity");

    CameraState unsafe;
    unsafe.fieldOfViewDegrees = 999.0f;
    unsafe.nearPlane = -20.0f;
    unsafe.farPlane = -10.0f;
    unsafe.apertureSize = 4.0f;
    unsafe.orthoVerticalSize = 0.0f;
    const auto safe = SanitizeCameraState(unsafe);
    if (safe.fieldOfViewDegrees > 179.0f || safe.nearPlane < 0.001f ||
        safe.farPlane <= safe.nearPlane || safe.apertureSize > 1.0f ||
        safe.orthoVerticalSize < 0.001f)
    {
        Fail("camera sanitization accepted invalid projection state");
    }

    std::cout << "PHASE5 GATE2 CAMERA PASS // native scene camera create, edit, Undo/Redo and transform ownership\n";
    return EXIT_SUCCESS;
}
