#include "renegade/bridge/SceneComponentService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

namespace
{
    using renegade::bridge::CommandService;
    using renegade::bridge::ObjectParticipationProperty;

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
        wi::ecs::Entity root = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity payload = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity objectA = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity objectB = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity helper = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity unrelated = wi::ecs::INVALID_ENTITY;

        Fixture()
        {
            root = scene.Entity_CreateTransform("Imported Crate");
            payload = scene.Entity_CreateTransform("__renegade_creator_authored_transform");
            objectA = scene.Entity_CreateTransform("crate002_mesh_a");
            objectB = scene.Entity_CreateTransform("crate002_mesh_b");
            helper = scene.Entity_CreateTransform("RootNode");
            unrelated = scene.Entity_CreateTransform("Unrelated");

            scene.objects.Create(objectA);
            scene.objects.Create(objectB);
            scene.objects.Create(unrelated);

            scene.Component_Attach(payload, root, true);
            scene.Component_Attach(helper, payload, true);
            scene.Component_Attach(objectA, helper, true);
            scene.Component_Attach(objectB, helper, true);

            auto& metadata = scene.metadatas.Create(root);
            metadata.string_values.set(
                renegade::bridge::ReusableAssetInstanceIdMetadataKey,
                "asset-crate-001");
            metadata.int_values.set(
                renegade::bridge::ReusableAssetInstanceVersionMetadataKey,
                renegade::bridge::ReusableAssetInstanceVersion);
        }
    };
}

int main()
{
    bool ok = true;
    Fixture fixture;

    ok &= Check(
        renegade::bridge::ResolveSceneComponentAuthoringRoot(
            fixture.scene, fixture.objectA) == fixture.root,
        "reusable descendant did not resolve to creator root");
    ok &= Check(
        renegade::bridge::ResolveSceneComponentAuthoringRoot(
            fixture.scene, fixture.unrelated) == fixture.unrelated,
        "ordinary entity was incorrectly redirected");

    CommandService commands;
    ok &= Check(
        commands.Execute(std::make_unique<renegade::bridge::SetSceneNameCommand>(
            fixture.scene,
            fixture.objectA,
            "  Hero Crate  ")),
        "creator-root rename command failed");
    ok &= Check(
        fixture.scene.names.GetComponent(fixture.root)->name == "Hero Crate",
        "creator-root rename did not trim/apply to root");
    ok &= Check(
        fixture.scene.names.GetComponent(fixture.objectA)->name == "crate002_mesh_a",
        "creator-root rename mutated imported child name");
    ok &= Check(commands.Undo(), "rename Undo failed");
    ok &= Check(
        fixture.scene.names.GetComponent(fixture.root)->name == "Imported Crate",
        "rename Undo did not restore root name");
    ok &= Check(commands.Redo(), "rename Redo failed");
    ok &= Check(
        fixture.scene.names.GetComponent(fixture.root)->name == "Hero Crate",
        "rename Redo did not restore new root name");

    auto& existingChildLayer = fixture.scene.layers.Create(fixture.objectA);
    existingChildLayer.layerMask = 0x00000004u;
    commands.Clear();
    ok &= Check(
        commands.Execute(std::make_unique<renegade::bridge::SetSceneLayerBitCommand>(
            fixture.scene,
            fixture.objectB,
            1u,
            false)),
        "layer-bit command failed");
    ok &= Check(
        fixture.scene.layers.GetComponent(fixture.objectA)->layerMask == 0x00000004u,
        "layer-bit edit destroyed unrelated bits on existing child");
    ok &= Check(
        fixture.scene.layers.GetComponent(fixture.objectB) != nullptr &&
            fixture.scene.layers.GetComponent(fixture.objectB)->layerMask == 0xFFFFFFFDu,
        "layer-bit edit did not preserve default bits on missing child layer");
    ok &= Check(commands.Undo(), "layer-bit Undo failed");
    ok &= Check(
        fixture.scene.layers.GetComponent(fixture.objectA)->layerMask == 0x00000004u &&
            fixture.scene.layers.GetComponent(fixture.objectB) == nullptr,
        "layer-bit Undo did not restore exact component state");

    const std::uint32_t authoredMask = 0x00000012u;
    commands.Clear();
    ok &= Check(
        commands.Execute(std::make_unique<renegade::bridge::SetSceneLayerMaskCommand>(
            fixture.scene,
            fixture.objectB,
            authoredMask)),
        "whole-asset layer command failed");
    ok &= Check(
        fixture.scene.layers.GetComponent(fixture.root) != nullptr &&
            fixture.scene.layers.GetComponent(fixture.root)->layerMask == authoredMask,
        "whole-asset layer did not author stable root");
    ok &= Check(
        fixture.scene.layers.GetComponent(fixture.objectA) != nullptr &&
            fixture.scene.layers.GetComponent(fixture.objectA)->layerMask == authoredMask,
        "whole-asset layer did not update existing render child");
    ok &= Check(
        fixture.scene.layers.GetComponent(fixture.objectB) != nullptr &&
            fixture.scene.layers.GetComponent(fixture.objectB)->layerMask == authoredMask,
        "whole-asset layer did not create render-child layer");
    ok &= Check(
        fixture.scene.layers.GetComponent(fixture.helper) == nullptr,
        "whole-asset layer polluted transform-only imported helper");
    ok &= Check(
        fixture.scene.layers.GetComponent(fixture.unrelated) == nullptr,
        "whole-asset layer touched unrelated object");
    ok &= Check(commands.Undo(), "layer Undo failed");
    ok &= Check(
        fixture.scene.layers.GetComponent(fixture.root) == nullptr,
        "layer Undo did not remove newly created root LayerComponent");
    ok &= Check(
        fixture.scene.layers.GetComponent(fixture.objectA) != nullptr &&
            fixture.scene.layers.GetComponent(fixture.objectA)->layerMask == 0x00000004u,
        "layer Undo did not restore existing child mask");
    ok &= Check(
        fixture.scene.layers.GetComponent(fixture.objectB) == nullptr,
        "layer Undo did not remove newly created child LayerComponent");
    ok &= Check(commands.Redo(), "layer Redo failed");

    const auto layerState = renegade::bridge::InspectSceneLayerMask(
        fixture.scene, fixture.objectA);
    ok &= Check(layerState.targetCount == 3, "layer target count is not root + two render objects");
    ok &= Check(!layerState.mixed && layerState.mask == authoredMask,
        "layer inspection did not report uniform whole-asset mask");

    auto* rootMetadata = fixture.scene.metadatas.GetComponent(fixture.root);
    rootMetadata->string_values.set("creator.custom", "preserve me");
    commands.Clear();
    ok &= Check(
        commands.Execute(std::make_unique<renegade::bridge::SetMetadataPresetCommand>(
            fixture.scene,
            fixture.objectA,
            wi::scene::MetadataComponent::Preset::Pickup)),
        "metadata preset command failed");
    ok &= Check(
        fixture.scene.metadatas.GetComponent(fixture.root)->preset ==
            wi::scene::MetadataComponent::Preset::Pickup,
        "metadata preset did not apply to creator root");
    ok &= Check(
        fixture.scene.metadatas.GetComponent(fixture.root)->string_values.get(
            "creator.custom") == "preserve me",
        "metadata preset destroyed unrelated metadata");
    ok &= Check(commands.Undo(), "metadata Undo failed");
    ok &= Check(
        fixture.scene.metadatas.GetComponent(fixture.root)->preset ==
            wi::scene::MetadataComponent::Preset::Custom,
        "metadata Undo did not restore prior preset");
    ok &= Check(
        fixture.scene.metadatas.GetComponent(fixture.root)->string_values.has(
            renegade::bridge::ReusableAssetInstanceIdMetadataKey),
        "metadata Undo destroyed reusable asset identity");

    auto* objectA = fixture.scene.objects.GetComponent(fixture.objectA);
    auto* objectB = fixture.scene.objects.GetComponent(fixture.objectB);
    objectA->SetRenderable(true);
    objectB->SetRenderable(false);
    auto mixed = renegade::bridge::InspectObjectParticipation(
        fixture.scene,
        fixture.root,
        ObjectParticipationProperty::Renderable);
    ok &= Check(mixed.targetCount == 2 && mixed.mixed,
        "mixed reusable renderability was not detected");

    commands.Clear();
    ok &= Check(
        commands.Execute(std::make_unique<renegade::bridge::SetObjectParticipationCommand>(
            fixture.scene,
            fixture.objectB,
            ObjectParticipationProperty::Renderable,
            true)),
        "whole-asset renderable command failed");
    ok &= Check(objectA->IsRenderable() && objectB->IsRenderable(),
        "whole-asset renderable command did not update all render descendants");
    ok &= Check(commands.Undo(), "object participation Undo failed");
    ok &= Check(objectA->IsRenderable() && !objectB->IsRenderable(),
        "object participation Undo did not restore mixed prior state");
    ok &= Check(commands.Redo(), "object participation Redo failed");
    ok &= Check(objectA->IsRenderable() && objectB->IsRenderable(),
        "object participation Redo failed");

    commands.Clear();
    ok &= Check(
        commands.Execute(std::make_unique<renegade::bridge::SetObjectParticipationCommand>(
            fixture.scene,
            fixture.root,
            ObjectParticipationProperty::VisibleInMainCamera,
            false)),
        "main-camera visibility command failed");
    ok &= Check(
        objectA->IsNotVisibleInMainCamera() && objectB->IsNotVisibleInMainCamera(),
        "main-camera visibility did not apply to whole reusable asset");
    ok &= Check(commands.Undo(), "main-camera visibility Undo failed");
    ok &= Check(
        !objectA->IsNotVisibleInMainCamera() && !objectB->IsNotVisibleInMainCamera(),
        "main-camera visibility Undo did not restore visible state");

    commands.Clear();
    auto* unrelatedObject = fixture.scene.objects.GetComponent(fixture.unrelated);
    unrelatedObject->SetCastShadow(true);
    ok &= Check(
        commands.Execute(std::make_unique<renegade::bridge::SetObjectParticipationCommand>(
            fixture.scene,
            fixture.unrelated,
            ObjectParticipationProperty::CastShadow,
            false)),
        "ordinary object participation command failed");
    ok &= Check(!unrelatedObject->IsCastingShadow(),
        "ordinary object participation did not target selected object");
    ok &= Check(objectA->IsCastingShadow() && objectB->IsCastingShadow(),
        "ordinary object participation leaked into reusable asset");

    if (!ok)
        return 1;

    std::cout << "Phase 5 Gate 1 scene component tests passed\n";
    return 0;
}
