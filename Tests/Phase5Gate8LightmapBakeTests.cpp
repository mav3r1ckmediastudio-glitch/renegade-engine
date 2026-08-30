#include "renegade/bridge/LightmapBakeService.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool Check(const bool condition, const char* message)
    {
        if (condition)
            return true;
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }

    struct Fixture
    {
        wi::scene::Scene scene;
        wi::ecs::Entity mesh = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity object = wi::ecs::INVALID_ENTITY;

        Fixture()
        {
            mesh = scene.Entity_CreateTransform("Bake Mesh");
            object = scene.Entity_CreateTransform("Bake Object");

            auto& meshComponent = scene.meshes.Create(mesh);
            meshComponent.vertex_positions = {
                XMFLOAT3(-1.0f, 0.0f, 0.0f),
                XMFLOAT3(1.0f, 0.0f, 0.0f),
                XMFLOAT3(0.0f, 1.0f, 0.0f),
            };
            meshComponent.vertex_normals = {
                XMFLOAT3(0.0f, 0.0f, 1.0f),
                XMFLOAT3(0.0f, 0.0f, 1.0f),
                XMFLOAT3(0.0f, 0.0f, 1.0f),
            };
            meshComponent.indices = {0, 1, 2};

            auto& objectComponent = scene.objects.Create(object);
            objectComponent.meshID = mesh;
        }
    };
}

int main()
{
    using namespace renegade::bridge;

    bool ok = true;

    ok &= Check(LightmapBakeService::IsValidResolution(32),
        "minimum power-of-two resolution rejected");
    ok &= Check(LightmapBakeService::IsValidResolution(512),
        "default resolution rejected");
    ok &= Check(LightmapBakeService::IsValidResolution(8192),
        "maximum power-of-two resolution rejected");
    ok &= Check(!LightmapBakeService::IsValidResolution(31),
        "below-minimum resolution accepted");
    ok &= Check(!LightmapBakeService::IsValidResolution(33),
        "non-power-of-two resolution accepted");
    ok &= Check(!LightmapBakeService::IsValidResolution(16384),
        "above-maximum resolution accepted");

    Fixture fixture;
    const auto emptyStatus = LightmapBakeService::CaptureStatus(fixture.scene, {});
    ok &= Check(!emptyStatus.eligible && emptyStatus.targetCount == 0,
        "empty selection was reported bakeable");

    auto validStatus = LightmapBakeService::CaptureStatus(
        fixture.scene, {fixture.object, fixture.object});
    ok &= Check(validStatus.eligible && validStatus.targetCount == 1,
        "target normalisation did not deduplicate the selected object");

    LightmapBakeSession bakeSession;
    LightmapBakeSettings settings;
    std::string error;

    settings.resolution = 33;
    ok &= Check(!LightmapBakeService::BeginBake(
            fixture.scene, {fixture.object}, settings, bakeSession, error),
        "invalid resolution started a bake");
    ok &= Check(!bakeSession.IsActive(),
        "failed resolution validation left an active bake session");

    settings.resolution = 512;
    settings.uvSource = LightmapUvSource::CopyUv0;
    fixture.scene.meshes.GetComponent(fixture.mesh)->vertex_uvset_0 = {
        XMFLOAT2(0.0f, 0.0f),
    };
    error.clear();
    ok &= Check(!LightmapBakeService::BeginBake(
            fixture.scene, {fixture.object}, settings, bakeSession, error),
        "incomplete UV0 started a bake");
    ok &= Check(!bakeSession.IsActive(),
        "failed UV validation left an active bake session");
    ok &= Check(!fixture.scene.objects.GetComponent(fixture.object)->IsLightmapRenderRequested(),
        "failed UV validation left a native lightmap render request active");
    ok &= Check(fixture.scene.meshes.GetComponent(fixture.mesh)->vertex_atlas.empty(),
        "failed UV validation mutated the native lightmap atlas");

    fixture.scene.meshes.GetComponent(fixture.mesh)->vertex_uvset_0.clear();
    settings.uvSource = LightmapUvSource::KeepAtlas;
    error.clear();
    ok &= Check(!LightmapBakeService::BeginBake(
            fixture.scene, {fixture.object}, settings, bakeSession, error),
        "missing atlas started a Keep Atlas bake");

    fixture.scene.objects.GetComponent(fixture.object)->SetDynamic(true);
    const auto dynamicStatus = LightmapBakeService::CaptureStatus(
        fixture.scene, {fixture.object});
    ok &= Check(!dynamicStatus.eligible,
        "dynamic object was reported eligible for static baking");
    fixture.scene.objects.GetComponent(fixture.object)->SetDynamic(false);

    fixture.scene.meshes.GetComponent(fixture.mesh)->armatureID =
        fixture.scene.Entity_CreateTransform("Armature");
    const auto skinnedStatus = LightmapBakeService::CaptureStatus(
        fixture.scene, {fixture.object});
    ok &= Check(!skinnedStatus.eligible,
        "skinned mesh was reported eligible for static baking");
    fixture.scene.meshes.GetComponent(fixture.mesh)->armatureID = wi::ecs::INVALID_ENTITY;

    VertexAoBakeSettings ao;
    ao.rayCount = 7;
    CommandService commands;
    error.clear();
    ok &= Check(!LightmapBakeService::ComputeVertexAo(
            fixture.scene, {fixture.object}, ao, commands, error),
        "Vertex AO accepted a ray count below the native Gate 8 range");
    ok &= Check(commands.UndoCount() == 0,
        "failed Vertex AO validation created an Undo entry");

    ao.rayCount = 256;
    ao.rayLength = 1001.0f;
    error.clear();
    ok &= Check(!LightmapBakeService::ComputeVertexAo(
            fixture.scene, {fixture.object}, ao, commands, error),
        "Vertex AO accepted an out-of-range ray length");
    ok &= Check(commands.UndoCount() == 0,
        "failed Vertex AO ray-length validation created an Undo entry");

    error.clear();
    ok &= Check(!LightmapBakeService::ClearLightmaps(
            fixture.scene, {fixture.object}, commands, error),
        "clearing a missing lightmap reported success");
    ok &= Check(commands.UndoCount() == 0,
        "failed lightmap clear created an Undo entry");

    error.clear();
    ok &= Check(!LightmapBakeService::ClearVertexAo(
            fixture.scene, {fixture.object}, commands, error),
        "clearing missing Vertex AO reported success");
    ok &= Check(commands.UndoCount() == 0,
        "failed Vertex AO clear created an Undo entry");

    if (!ok)
        return 1;

    std::cout << "Phase 5 Gate 8 lightmap bake validation regression passed\n";
    return 0;
}
