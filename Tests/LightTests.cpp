#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

#include "renegade/bridge/LightService.h"

namespace
{
    bool NearlyEqual(const float left, const float right)
    {
        return std::abs(left - right) < 0.001f;
    }

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
}

int main()
{
    wi::scene::Scene scene;
    const auto entity = wi::ecs::CreateEntity();
    auto& light = scene.lights.Create(entity);
    light.SetType(wi::scene::LightComponent::SPOT);
    light.color = XMFLOAT3(0.10f, 0.20f, 0.30f);
    light.intensity = 12.0f;
    light.range = 24.0f;
    light.outerConeAngle = 40.0f * XM_PI / 180.0f;
    light.innerConeAngle = 15.0f * XM_PI / 180.0f;
    light.SetCastShadow(true);
    light.SetVolumetricsEnabled(false);
    light.volumetric_boost = 0.25f;

    // Native shape fields are creator-facing in the Light Inspector.
    light.radius = 1.25f;
    light.length = 2.50f;
    light.height = 3.75f;
    // Sentinel values outside the curated V1 state must survive every edit.
    light.cascade_distances = { 3.0f, 17.0f, 91.0f };
    light.forced_shadow_resolution = 2048;
    light.SetVisualizerEnabled(true);
    light.SetStatic(true);
    light.SetVolumetricCloudsEnabled(true);
    const auto cameraSource = wi::ecs::CreateEntity();
    light.cameraSource = cameraSource;
    light.lensFlareNames = { "flare-a.png", "flare-b.png" };

    const auto before = renegade::bridge::CaptureLight(light);
    auto after = before;
    after.type = wi::scene::LightComponent::RECTANGLE;
    after.color = XMFLOAT3(0.0f, 0.75f, 1.0f);
    after.intensity = 350.0f;
    after.range = 90.0f;
    after.outerConeDegrees = 55.0f;
    after.innerConeDegrees = 18.0f;
    after.radius = 4.25f;
    after.length = 5.50f;
    after.height = 6.75f;
    after.castShadow = false;
    after.volumetrics = true;
    after.volumetricBoost = 3.5f;

    renegade::bridge::CommandService commands;
    if (!commands.Execute(std::make_unique<renegade::bridge::SetLightCommand>(
            scene,
            entity,
            before,
            after)))
    {
        return Fail("light command did not execute");
    }
    if (light.GetType() != wi::scene::LightComponent::RECTANGLE ||
        !NearlyEqual(light.color.y, 0.75f) ||
        !NearlyEqual(light.intensity, 350.0f) ||
        !NearlyEqual(light.range, 90.0f) ||
        !NearlyEqual(light.outerConeAngle, 55.0f * XM_PI / 180.0f) ||
        !NearlyEqual(light.radius, 4.25f) ||
        !NearlyEqual(light.length, 5.50f) ||
        !NearlyEqual(light.height, 6.75f) ||
        light.IsCastingShadow() || !light.IsVolumetricsEnabled() ||
        !NearlyEqual(light.volumetric_boost, 3.5f))
    {
        return Fail("curated light state did not reach the native component");
    }
    if (light.cascade_distances.size() != 3 ||
        light.forced_shadow_resolution != 2048 ||
        !light.IsVisualizerEnabled() || !light.IsStatic() ||
        !light.IsVolumetricCloudsEnabled() ||
        light.cameraSource != cameraSource ||
        light.lensFlareNames.size() != 2)
    {
        return Fail("light edit overwrote an unexposed Wicked field");
    }

    if (!commands.Undo() ||
        light.GetType() != wi::scene::LightComponent::SPOT ||
        !NearlyEqual(light.intensity, 12.0f) ||
        !NearlyEqual(light.radius, 1.25f) ||
        !NearlyEqual(light.length, 2.50f) ||
        !NearlyEqual(light.height, 3.75f) ||
        !light.IsCastingShadow() || light.IsVolumetricsEnabled())
    {
        return Fail("light Undo did not restore the curated state");
    }
    if (!commands.Redo() ||
        light.GetType() != wi::scene::LightComponent::RECTANGLE ||
        !NearlyEqual(light.intensity, 350.0f))
    {
        return Fail("light Redo did not restore the edited state");
    }

    const auto current = renegade::bridge::CaptureLight(light);
    renegade::bridge::CommandService noOpCommands;
    if (noOpCommands.Execute(
            std::make_unique<renegade::bridge::SetLightCommand>(
                scene,
                entity,
                current,
                current)) ||
        noOpCommands.UndoCount() != 0 || noOpCommands.IsDirty())
    {
        return Fail("identical light state polluted command history");
    }

    auto unsafe = current;
    unsafe.color = XMFLOAT3(-1.0f, 2.0f, 0.5f);
    unsafe.intensity = -10.0f;
    unsafe.range = 500000.0f;
    unsafe.outerConeDegrees = 120.0f;
    unsafe.innerConeDegrees = 100.0f;
    unsafe.radius = -1.0f;
    unsafe.length = 500000.0f;
    unsafe.height = -5.0f;
    unsafe.volumetricBoost = 99.0f;
    const auto safe = renegade::bridge::SanitizeLightState(unsafe);
    if (!NearlyEqual(safe.color.x, 0.0f) ||
        !NearlyEqual(safe.color.y, 1.0f) ||
        !NearlyEqual(safe.intensity, 0.0f) ||
        !NearlyEqual(safe.range, 100000.0f) ||
        !NearlyEqual(safe.outerConeDegrees, 89.9f) ||
        !NearlyEqual(safe.innerConeDegrees, 89.9f) ||
        !NearlyEqual(safe.radius, 0.0f) ||
        !NearlyEqual(safe.length, 100000.0f) ||
        !NearlyEqual(safe.height, 0.0f) ||
        !NearlyEqual(safe.volumetricBoost, 10.0f))
    {
        return Fail("light safety bounds are incorrect");
    }

    auto removedAfter = current;
    removedAfter.intensity += 1.0f;
    auto removed = std::make_unique<renegade::bridge::SetLightCommand>(
        scene,
        entity,
        current,
        removedAfter);
    scene.lights.Remove(entity);
    renegade::bridge::CommandService removedCommands;
    if (removedCommands.Execute(std::move(removed)) ||
        removedCommands.UndoCount() != 0)
    {
        return Fail("removed light produced a command-history entry");
    }

    // Add Light is a native entity-authoring command, not a Studio-only
    // shortcut. Prove all four types create the components Wicked owns.
    wi::scene::Scene authored;
    renegade::bridge::CommandService createCommands;
    constexpr std::array<wi::scene::LightComponent::LightType, 4> types = {
        wi::scene::LightComponent::POINT,
        wi::scene::LightComponent::SPOT,
        wi::scene::LightComponent::DIRECTIONAL,
        wi::scene::LightComponent::RECTANGLE,
    };
    std::array<wi::ecs::Entity, types.size()> created = {};
    const XMFLOAT4 authoredRotation = XMFLOAT4(
        0.0f,
        std::sin(XM_PIDIV4 * 0.5f),
        0.0f,
        std::cos(XM_PIDIV4 * 0.5f));
    for (std::size_t index = 0; index < types.size(); ++index)
    {
        const XMFLOAT3 position(
            10.0f + static_cast<float>(index),
            20.0f,
            30.0f);
        auto command = std::make_unique<renegade::bridge::CreateLightCommand>(
            authored,
            types[index],
            position,
            authoredRotation);
        auto* create = command.get();
        if (!createCommands.Execute(std::move(command)))
        {
            return Fail("native Add Light command did not execute");
        }
        created[index] = create->CreatedEntity();
        const auto* createdLight = authored.lights.GetComponent(created[index]);
        const auto* transform = authored.transforms.GetComponent(created[index]);
        const auto* name = authored.names.GetComponent(created[index]);
        if (createdLight == nullptr || transform == nullptr || name == nullptr ||
            !authored.layers.Contains(created[index]) ||
            createdLight->GetType() != types[index] || name->name.empty() ||
            createdLight->IsVisualizerEnabled() ||
            !NearlyEqual(transform->translation_local.x, position.x) ||
            !NearlyEqual(transform->translation_local.y, position.y) ||
            !NearlyEqual(transform->translation_local.z, position.z) ||
            !NearlyEqual(transform->rotation_local.y, authoredRotation.y) ||
            !NearlyEqual(transform->rotation_local.w, authoredRotation.w))
        {
            return Fail("Add Light did not create a complete native entity");
        }
    }
    const auto* rectangle = authored.lights.GetComponent(created.back());
    if (rectangle == nullptr || !NearlyEqual(rectangle->length, 2.0f) ||
        !NearlyEqual(rectangle->height, 2.0f))
    {
        return Fail("new rectangle light lost its native source shape");
    }

    auto secondPoint =
        std::make_unique<renegade::bridge::CreateLightCommand>(
            authored,
            wi::scene::LightComponent::POINT,
            XMFLOAT3(0.0f, 1.0f, 2.0f));
    auto* secondPointCommand = secondPoint.get();
    if (!createCommands.Execute(std::move(secondPoint)))
    {
        return Fail("second Add Light command did not execute");
    }
    const auto secondPointEntity = secondPointCommand->CreatedEntity();
    const auto* secondPointName = authored.names.GetComponent(secondPointEntity);
    if (secondPointName == nullptr || secondPointName->name != "Point Light 2")
    {
        return Fail("new lights did not receive unique creator-facing names");
    }

    if (!createCommands.Undo() || authored.lights.Contains(secondPointEntity) ||
        !createCommands.Redo() || !authored.lights.Contains(secondPointEntity))
    {
        return Fail("Add Light Undo/Redo did not restore the same entity");
    }
    const auto* redonePointName = authored.names.GetComponent(secondPointEntity);
    if (redonePointName == nullptr || redonePointName->name != "Point Light 2")
    {
        return Fail("Add Light Redo did not preserve the authored identity");
    }

    renegade::bridge::CommandService deleteCommands;
    if (!deleteCommands.Execute(
            std::make_unique<renegade::bridge::DeleteEntityCommand>(
                authored,
                secondPointEntity)) ||
        authored.lights.Contains(secondPointEntity) ||
        !deleteCommands.Undo() ||
        !authored.lights.Contains(secondPointEntity))
    {
        return Fail("a newly added light did not support Delete and Undo");
    }

    std::cout << "PASS: native light edit, Add, Delete, preservation and history\n";
    return 0;
}
