#include "renegade/bridge/DecalProbeService.h"

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
        std::cerr << "PHASE5 GATE3 DECAL/PROBE FAIL // " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    using namespace renegade::bridge;

    wi::scene::Scene scene;
    if (scene.Entity_CreateDecal("Decal", "", "") == wi::ecs::INVALID_ENTITY)
        Fail("fixture decal creation failed");
    if (scene.Entity_CreateEnvironmentProbe(
            "Environment Probe", XMFLOAT3(0.0f, 0.0f, 0.0f)) ==
        wi::ecs::INVALID_ENTITY)
    {
        Fail("fixture probe creation failed");
    }

    CommandService commands;

    DecalState decalState;
    decalState.baseColorOnlyAlpha = true;
    decalState.slopeBlendPower = 3.5f;

    TransformState decalPose;
    decalPose.translation = XMFLOAT3(2.0f, 1.0f, -4.0f);
    decalPose.rotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    decalPose.scale = XMFLOAT3(2.0f, 0.5f, 1.5f);

    auto createDecal = std::make_unique<CreateDecalCommand>(
        scene, decalState, decalPose);
    auto* createDecalRaw = createDecal.get();
    if (!commands.Execute(std::move(createDecal)))
        Fail("CreateDecalCommand did not execute");

    const auto decalEntity = createDecalRaw->CreatedEntity();
    const auto* decalName = scene.names.GetComponent(decalEntity);
    const auto* decal = scene.decals.GetComponent(decalEntity);
    const auto* decalTransform = scene.transforms.GetComponent(decalEntity);
    const auto* decalMaterial = scene.materials.GetComponent(decalEntity);
    if (decalName == nullptr || decalName->name != "Decal 2")
        Fail("decal naming was not unique");
    if (decal == nullptr || decalTransform == nullptr || decalMaterial == nullptr)
        Fail("native decal, transform or material component missing");

    const auto createdDecal = CaptureDecal(*decal);
    if (!createdDecal.baseColorOnlyAlpha ||
        !Near(createdDecal.slopeBlendPower, 3.5f))
    {
        Fail("created decal did not preserve native authored state");
    }
    if (!Near(decalTransform->translation_local.x, 2.0f) ||
        !Near(decalTransform->scale_local.x, 2.0f) ||
        !Near(decalTransform->scale_local.y, 0.5f) ||
        !Near(decalTransform->scale_local.z, 1.5f))
    {
        Fail("decal transform/projection volume was not preserved");
    }

    DecalState decalEdited = createdDecal;
    decalEdited.baseColorOnlyAlpha = false;
    decalEdited.slopeBlendPower = 7.0f;
    if (!commands.Execute(
            std::make_unique<SetDecalCommand>(scene, decalEntity, decalEdited)))
    {
        Fail("SetDecalCommand did not execute");
    }
    const auto changedDecal = CaptureDecal(*scene.decals.GetComponent(decalEntity));
    if (changedDecal.baseColorOnlyAlpha ||
        !Near(changedDecal.slopeBlendPower, 7.0f))
    {
        Fail("decal edit did not map to native Wicked fields");
    }
    if (!commands.Undo())
        Fail("decal property Undo failed");
    const auto undoneDecal = CaptureDecal(*scene.decals.GetComponent(decalEntity));
    if (!undoneDecal.baseColorOnlyAlpha ||
        !Near(undoneDecal.slopeBlendPower, 3.5f))
    {
        Fail("decal property Undo did not restore prior state");
    }
    if (!commands.Redo())
        Fail("decal property Redo failed");

    EnvironmentProbeState probeState;
    probeState.realTime = true;
    probeState.msaa = true;
    probeState.resolution = 512;
    probeState.updateInterval = 0.5f;
    probeState.viewDistance = 250.0f;

    TransformState probePose;
    probePose.translation = XMFLOAT3(-3.0f, 4.0f, 8.0f);
    probePose.rotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    probePose.scale = XMFLOAT3(12.0f, 6.0f, 10.0f);

    auto createProbe = std::make_unique<CreateEnvironmentProbeCommand>(
        scene, probeState, probePose);
    auto* createProbeRaw = createProbe.get();
    if (!commands.Execute(std::move(createProbe)))
        Fail("CreateEnvironmentProbeCommand did not execute");

    const auto probeEntity = createProbeRaw->CreatedEntity();
    const auto* probeName = scene.names.GetComponent(probeEntity);
    const auto* probe = scene.probes.GetComponent(probeEntity);
    const auto* probeTransform = scene.transforms.GetComponent(probeEntity);
    if (probeName == nullptr || probeName->name != "Environment Probe 2")
        Fail("probe naming was not unique");
    if (probe == nullptr || probeTransform == nullptr)
        Fail("native EnvironmentProbeComponent or TransformComponent missing");

    const auto createdProbe = CaptureEnvironmentProbe(*probe);
    if (!createdProbe.realTime || !createdProbe.msaa ||
        createdProbe.resolution != 512 ||
        !Near(createdProbe.updateInterval, 0.5f) ||
        !Near(createdProbe.viewDistance, 250.0f))
    {
        Fail("created probe did not preserve native authored state");
    }
    if (!Near(probeTransform->translation_local.x, -3.0f) ||
        !Near(probeTransform->scale_local.x, 12.0f) ||
        !Near(probeTransform->scale_local.y, 6.0f) ||
        !Near(probeTransform->scale_local.z, 10.0f))
    {
        Fail("probe transform/influence volume was not preserved");
    }

    EnvironmentProbeState probeEdited = createdProbe;
    probeEdited.realTime = false;
    probeEdited.msaa = false;
    probeEdited.resolution = 1024;
    probeEdited.updateInterval = 2.0f;
    probeEdited.viewDistance = -1.0f;
    if (!commands.Execute(std::make_unique<SetEnvironmentProbeCommand>(
            scene, probeEntity, probeEdited)))
    {
        Fail("SetEnvironmentProbeCommand did not execute");
    }
    const auto changedProbe = CaptureEnvironmentProbe(
        *scene.probes.GetComponent(probeEntity));
    if (changedProbe.realTime || changedProbe.msaa ||
        changedProbe.resolution != 1024 ||
        !Near(changedProbe.updateInterval, 2.0f) ||
        !Near(changedProbe.viewDistance, -1.0f))
    {
        Fail("probe edit did not map to native Wicked fields");
    }
    if (!commands.Undo())
        Fail("probe property Undo failed");
    const auto undoneProbe = CaptureEnvironmentProbe(
        *scene.probes.GetComponent(probeEntity));
    if (!undoneProbe.realTime || !undoneProbe.msaa ||
        undoneProbe.resolution != 512 ||
        !Near(undoneProbe.updateInterval, 0.5f))
    {
        Fail("probe property Undo did not restore prior state");
    }
    if (!commands.Redo())
        Fail("probe property Redo failed");

    auto* refreshProbe = scene.probes.GetComponent(probeEntity);
    refreshProbe->SetDirty(false);
    refreshProbe->first_render = false;
    if (!RefreshEnvironmentProbe(scene, probeEntity) ||
        !refreshProbe->IsDirty() || !refreshProbe->first_render)
    {
        Fail("probe Refresh did not request a native recapture");
    }

    const DecalState unsafeDecal{true, 99.0f};
    if (SanitizeDecalState(unsafeDecal).slopeBlendPower > 8.0f)
        Fail("decal sanitization accepted invalid slope blend");

    EnvironmentProbeState unsafeProbe;
    unsafeProbe.resolution = 700;
    unsafeProbe.updateInterval = -5.0f;
    unsafeProbe.viewDistance = -50.0f;
    const auto safeProbe = SanitizeEnvironmentProbeState(unsafeProbe);
    if (safeProbe.resolution != 512 || safeProbe.updateInterval < 0.0f ||
        !Near(safeProbe.viewDistance, -1.0f))
    {
        Fail("probe sanitization accepted invalid native state");
    }

    if (!commands.Undo())
        Fail("probe property Undo before create Undo failed");
    if (!commands.Undo())
        Fail("probe create Undo failed");
    if (scene.probes.Contains(probeEntity))
        Fail("probe create Undo did not remove the native probe");
    if (!commands.Redo())
        Fail("probe create Redo failed");
    if (!scene.probes.Contains(probeEntity))
        Fail("probe create Redo did not restore the same native probe entity");

    std::cout << "PHASE5 GATE3 DECAL/PROBE PASS // native create, edit, refresh, transform ownership and Undo/Redo\n";
    return EXIT_SUCCESS;
}
