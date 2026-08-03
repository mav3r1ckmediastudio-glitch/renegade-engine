#include "renegade/bridge/ImportService.h"

#include <cmath>
#include <iostream>
#include <utility>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "RenegadeImportTests: " << message << '\n';
        return 1;
    }
}

int main()
{
    renegade::bridge::ImportService imports;

    const auto missing = imports.PrepareGltfAsset(
        "missing.glb",
        "missing.wiscene");
    if (missing.IsReady() ||
        missing.Result().error.find("does not exist") == std::string::npos)
    {
        return Fail("missing source validation did not fail clearly");
    }

    const auto unsupported = imports.PrepareGltfAsset(
        "model.fbx",
        "model.wiscene");
    if (unsupported.IsReady() ||
        unsupported.Result().error.find("only accepts") == std::string::npos)
    {
        return Fail("unsupported format validation did not fail clearly");
    }

    const auto wrongDestination = imports.PrepareGltfAsset(
        "model.glb",
        "model.asset");
    if (wrongDestination.IsReady() ||
        wrongDestination.Result().error.find(".wiscene") == std::string::npos)
    {
        return Fail("destination format validation did not fail clearly");
    }

    wi::scene::Scene fixture;
    const auto root = wi::ecs::CreateEntity();
    fixture.names.Create(root) = "Imported Root";
    fixture.transforms.Create(root);
    const auto summary = renegade::bridge::ImportService::Summarize(fixture);
    if (summary.names != 1 || summary.transforms != 1 || summary.meshes != 0)
    {
        return Fail("scene structural summary is incorrect");
    }

    // ResolveScaleFactor: Original/Meters are always a no-op for a
    // glTF-mandated metres source; Centimeters/Inches are fixed literal
    // multipliers; Automatic normalizes the union of every mesh's local
    // vertex-position bounds to the 2 m target extent.
    {
        wi::scene::Scene empty;
        const float originalFactor = renegade::bridge::ImportService::
            ResolveScaleFactor(
                renegade::bridge::ModelScaleMode::Original,
                empty);
        const float metersFactor = renegade::bridge::ImportService::
            ResolveScaleFactor(
                renegade::bridge::ModelScaleMode::Meters,
                empty);
        if (originalFactor != 1.0f || metersFactor != 1.0f)
        {
            return Fail("Original/Meters scale mode did not resolve to 1.0");
        }

        const float centimetersFactor = renegade::bridge::ImportService::
            ResolveScaleFactor(
                renegade::bridge::ModelScaleMode::Centimeters,
                empty);
        if (centimetersFactor != 0.01f)
        {
            return Fail("Centimeters scale mode did not resolve to 0.01");
        }

        const float inchesFactor = renegade::bridge::ImportService::
            ResolveScaleFactor(
                renegade::bridge::ModelScaleMode::Inches,
                empty);
        if (inchesFactor != 0.0254f)
        {
            return Fail("Inches scale mode did not resolve to 0.0254");
        }

        // An empty scene has no vertex data to normalize against; Automatic
        // must fall back to a no-op rather than divide by zero.
        const float emptyAutomaticFactor = renegade::bridge::ImportService::
            ResolveScaleFactor(
                renegade::bridge::ModelScaleMode::Automatic,
                empty);
        if (emptyAutomaticFactor != 1.0f)
        {
            return Fail("Automatic scale mode did not fall back to 1.0 for an empty scene");
        }

        wi::scene::Scene withMesh;
        const auto meshEntity = wi::ecs::CreateEntity();
        auto& mesh = withMesh.meshes.Create(meshEntity);
        // A 20-unit cube: the largest extent (20) should normalize to the
        // 2 m target extent, i.e. a 0.1x factor.
        mesh.vertex_positions.push_back(XMFLOAT3(-10.0f, -10.0f, -10.0f));
        mesh.vertex_positions.push_back(XMFLOAT3(10.0f, 10.0f, 10.0f));
        const float automaticFactor = renegade::bridge::ImportService::
            ResolveScaleFactor(
                renegade::bridge::ModelScaleMode::Automatic,
                withMesh);
        if (std::abs(automaticFactor - 0.1f) > 0.0001f)
        {
            return Fail("Automatic scale mode did not normalize the largest mesh extent");
        }
    }

    const auto incomplete = imports.CompleteGltfAsset({});
    if (incomplete.succeeded || incomplete.error.find("not ready") == std::string::npos)
    {
        return Fail("unprepared WISCENE completion did not fail clearly");
    }

    // PlaceImportedModelCommand: Execute merges a prepared scene into the
    // target scene and positions its root, Undo removes the whole imported
    // entity, and Redo restores it under the same entity id. This exercises
    // the command contract directly with a synthetic scene rather than a
    // real GLTF conversion, since the pinned converter requires an
    // initialized graphics device this harness does not have.
    {
        wi::scene::Scene target;
        const auto preExistingEntity = wi::ecs::CreateEntity();
        target.names.Create(preExistingEntity) = "Existing Root";
        target.transforms.Create(preExistingEntity);

        auto prepared = wi::allocator::make_shared_single<wi::scene::Scene>();
        const auto importedEntity = wi::ecs::CreateEntity();
        prepared->names.Create(importedEntity) = "Imported Root";
        prepared->transforms.Create(importedEntity);

        // Mirrors ModelImporter_GLTF.cpp: every imported animation entity is
        // Component_Attach'd under the import root, and its AnimationComponent
        // starts paused (LOOPED but not PLAYING) until something calls Play().
        const auto animationEntity = wi::ecs::CreateEntity();
        prepared->names.Create(animationEntity) = "Imported Animation";
        prepared->Component_Attach(animationEntity, importedEntity);
        auto& importedAnimation = prepared->animations.Create(animationEntity);
        if (importedAnimation.IsPlaying())
        {
            return Fail("test fixture animation was not created in the paused default state");
        }

        const XMFLOAT3 placement(1.0f, 2.0f, 3.0f);
        const float scaleFactor = 0.1f;
        renegade::bridge::PlaceImportedModelCommand place(
            target, std::move(prepared), placement, scaleFactor);

        if (!place.Execute())
        {
            return Fail("PlaceImportedModelCommand did not merge the prepared scene");
        }
        const auto placedEntity = place.PlacedEntity();
        if (placedEntity != importedEntity)
        {
            return Fail("PlaceImportedModelCommand did not identify the merged root entity");
        }
        if (target.transforms.GetCount() != 2)
        {
            return Fail("PlaceImportedModelCommand did not merge the prepared entity");
        }
        const auto* placedTransform =
            target.transforms.GetComponent(placedEntity);
        if (placedTransform == nullptr ||
            placedTransform->translation_local.x != placement.x ||
            placedTransform->translation_local.y != placement.y ||
            placedTransform->translation_local.z != placement.z)
        {
            return Fail("PlaceImportedModelCommand did not position the imported root");
        }
        if (placedTransform->scale_local.x != scaleFactor ||
            placedTransform->scale_local.y != scaleFactor ||
            placedTransform->scale_local.z != scaleFactor)
        {
            return Fail("PlaceImportedModelCommand did not apply the scale factor");
        }
        const auto* placedAnimation =
            target.animations.GetComponent(animationEntity);
        if (placedAnimation == nullptr || !placedAnimation->IsPlaying())
        {
            return Fail("PlaceImportedModelCommand did not auto-play the imported animation");
        }

        place.Undo();
        if (target.transforms.GetCount() != 1 ||
            target.animations.Contains(animationEntity))
        {
            return Fail("PlaceImportedModelCommand::Undo did not remove the imported entity and its animation");
        }

        if (!place.Execute())
        {
            return Fail("PlaceImportedModelCommand redo did not restore the imported entity");
        }
        const auto* redoneTransform =
            target.transforms.GetComponent(placedEntity);
        if (target.transforms.GetCount() != 2 || redoneTransform == nullptr)
        {
            return Fail("PlaceImportedModelCommand redo did not restore the original entity id");
        }
        const auto* redoneAnimation =
            target.animations.GetComponent(animationEntity);
        if (redoneAnimation == nullptr || !redoneAnimation->IsPlaying())
        {
            return Fail("PlaceImportedModelCommand redo did not restore the playing animation");
        }
        if (redoneTransform->scale_local.x != scaleFactor ||
            redoneTransform->scale_local.y != scaleFactor ||
            redoneTransform->scale_local.z != scaleFactor)
        {
            return Fail("PlaceImportedModelCommand redo did not restore the applied scale factor");
        }
    }

    std::cout << "RenegadeImportTests passed\n";
    return 0;
}
