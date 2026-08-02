#include "renegade/bridge/ImportService.h"

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

        const XMFLOAT3 placement(1.0f, 2.0f, 3.0f);
        renegade::bridge::PlaceImportedModelCommand place(
            target, std::move(prepared), placement);

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

        place.Undo();
        if (target.transforms.GetCount() != 1)
        {
            return Fail("PlaceImportedModelCommand::Undo did not remove the imported entity");
        }

        if (!place.Execute())
        {
            return Fail("PlaceImportedModelCommand redo did not restore the imported entity");
        }
        if (target.transforms.GetCount() != 2 ||
            target.transforms.GetComponent(placedEntity) == nullptr)
        {
            return Fail("PlaceImportedModelCommand redo did not restore the original entity id");
        }
    }

    std::cout << "RenegadeImportTests passed\n";
    return 0;
}
