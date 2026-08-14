#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/SceneDocumentService.h"

#include <WickedEngine.h>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr const char* AssetId = "77777777-7777-4777-8777-777777777777";

    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "LP07 GATE 6 INSTANCE FAIL // " << message << '\n';
        return false;
    }

    bool Near(const float left, const float right)
    {
        return std::fabs(left - right) < 0.0001f;
    }

    bool SaveSceneForProof(
        wi::scene::Scene& scene,
        const fs::path& path,
        std::string& error)
    {
        try
        {
            wi::Archive archive(path.generic_u8string(), false, false);
            if (!archive.IsOpen())
            {
                error = "could not create proof WISCENE archive";
                return false;
            }
            archive.SetCompressionEnabled(true);
            scene.Serialize(archive);
            if (!archive.SaveFile(path.generic_u8string()))
            {
                error = "could not write proof WISCENE archive";
                return false;
            }
            archive = wi::Archive();
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
        error.clear();
        return true;
    }

    wi::ecs::Entity FindNamedEntity(
        const wi::scene::Scene& scene,
        const std::string& name)
    {
        for (std::size_t index = 0; index < scene.names.GetCount(); ++index)
        {
            if (scene.names[index].name == name)
                return scene.names.GetEntity(index);
        }
        return wi::ecs::INVALID_ENTITY;
    }
}

int main(int argc, char** argv)
{
    using namespace renegade::bridge;

    const fs::path outputRoot = argc > 1
        ? fs::u8path(argv[1])
        : fs::temp_directory_path() / "renegade-lp07-gate6-instance";
    std::error_code ec;
    fs::remove_all(outputRoot, ec);
    ec.clear();
    fs::create_directories(outputRoot, ec);
    if (!Require(!ec, "could not prepare output directory"))
        return 1;

    auto payload = wi::allocator::make_shared<wi::scene::Scene>();
    const wi::ecs::Entity payloadRoot =
        payload->Entity_CreateTransform("Payload Root");
    const wi::ecs::Entity payloadChild =
        payload->Entity_CreateTransform("Payload Child");
    payload->Component_Attach(payloadChild, payloadRoot);

    wi::scene::Scene target;
    PlaceReusableModelCommand command(
        target,
        std::move(payload),
        AssetId,
        XMFLOAT3(4.0f, 5.0f, 6.0f),
        2.5f);
    if (!Require(command.Execute(), "initial reusable placement failed"))
        return 1;

    const wi::ecs::Entity wrapper = command.PlacedEntity();
    std::vector<ReusableAssetInstanceRecord> instances;
    std::string error;
    if (!Require(InspectReusableAssetInstances(target, instances, error),
            "instance inspection failed: " + error) ||
        !Require(instances.size() == 1,
            "initial placement did not expose exactly one reusable instance") ||
        !Require(instances.front().assetId == AssetId,
            "initial placement lost the stable product ID") ||
        !Require(instances.front().instanceRoot == wrapper,
            "instance inspection did not return the placed wrapper") ||
        !Require(instances.front().payloadRoot == command.PayloadRootEntity(),
            "instance inspection did not return the marked payload root"))
        return 1;

    const auto* wrapperTransform = target.transforms.GetComponent(wrapper);
    const auto* childTransform =
        target.transforms.GetComponent(command.PayloadRootEntity());
    if (!Require(wrapperTransform != nullptr && childTransform != nullptr,
            "wrapper/payload transforms are missing") ||
        !Require(Near(wrapperTransform->translation_local.x, 4.0f) &&
                 Near(wrapperTransform->translation_local.y, 5.0f) &&
                 Near(wrapperTransform->translation_local.z, 6.0f),
            "authored placement translation was not stored on the wrapper") ||
        !Require(Near(wrapperTransform->scale_local.x, 2.5f) &&
                 Near(wrapperTransform->scale_local.y, 2.5f) &&
                 Near(wrapperTransform->scale_local.z, 2.5f),
            "authored placement scale was not stored on the wrapper") ||
        !Require(Near(childTransform->translation_local.x, 0.0f) &&
                 Near(childTransform->translation_local.y, 0.0f) &&
                 Near(childTransform->translation_local.z, 0.0f) &&
                 Near(childTransform->scale_local.x, 1.0f) &&
                 Near(childTransform->scale_local.y, 1.0f) &&
                 Near(childTransform->scale_local.z, 1.0f),
            "replaceable legacy payload root was not normalized into wrapper-local space"))
        return 1;

    command.Undo();
    instances.clear();
    if (!Require(InspectReusableAssetInstances(target, instances, error),
            "post-Undo instance inspection failed: " + error) ||
        !Require(instances.empty(),
            "Undo did not remove the reusable wrapper hierarchy"))
        return 1;

    if (!Require(command.Execute(), "Redo reusable placement failed"))
        return 1;
    instances.clear();
    if (!Require(InspectReusableAssetInstances(target, instances, error),
            "post-Redo instance inspection failed: " + error) ||
        !Require(instances.size() == 1 &&
                 instances.front().assetId == AssetId &&
                 instances.front().instanceRoot == wrapper,
            "Redo did not restore the same wrapper identity and stable asset ID"))
        return 1;

    const fs::path scenePath = outputRoot / "ReusableInstance.wiscene";
    if (!Require(SaveSceneForProof(target, scenePath, error),
            "could not save reusable instance proof scene: " + error))
        return 1;

    auto reopened = PrepareWickedSceneOpen(scenePath.generic_u8string());
    if (!Require(reopened.IsReady() && reopened.ReadOnlyScene() != nullptr,
            "saved reusable instance WISCENE did not reopen: " + reopened.Error()))
        return 1;
    instances.clear();
    if (!Require(InspectReusableAssetInstances(
            *reopened.ReadOnlyScene(), instances, error),
            "reopened instance inspection failed: " + error) ||
        !Require(instances.size() == 1 && instances.front().assetId == AssetId,
            "WISCENE Save/Open did not preserve reusable stable identity"))
        return 1;

    const auto* reopenedWrapper =
        reopened.ReadOnlyScene()->transforms.GetComponent(
            instances.front().instanceRoot);
    if (!Require(reopenedWrapper != nullptr &&
            Near(reopenedWrapper->translation_local.x, 4.0f) &&
            Near(reopenedWrapper->translation_local.y, 5.0f) &&
            Near(reopenedWrapper->translation_local.z, 6.0f) &&
            Near(reopenedWrapper->scale_local.x, 2.5f),
            "WISCENE Save/Open did not preserve authored wrapper transform"))
        return 1;

    // PR #57 creator products append a dedicated authored root after the
    // original imported transforms. Placement must choose that marker as the
    // replaceable payload boundary and retain its creator-approved transform;
    // choosing the first merged transform strips the real scale and makes
    // grounding operate in a different coordinate space from rendering.
    auto creatorPayload = wi::allocator::make_shared<wi::scene::Scene>();
    const wi::ecs::Entity originalRoot =
        creatorPayload->Entity_CreateTransform("Original Imported Root");
    const wi::ecs::Entity authoredRoot = wi::ecs::CreateEntity();
    creatorPayload->names.Create(authoredRoot).name =
        CreatorAuthoredTransformRootName;
    auto& authoredTransform = creatorPayload->transforms.Create(authoredRoot);
    authoredTransform.translation_local = XMFLOAT3(0.0f, 0.25f, 0.0f);
    authoredTransform.scale_local = XMFLOAT3(0.125f, 0.125f, 0.125f);
    authoredTransform.SetDirty();
    authoredTransform.UpdateTransform();
    creatorPayload->Component_Attach(originalRoot, authoredRoot, true);

    wi::scene::Scene creatorTarget;
    PlaceReusableModelCommand creatorCommand(
        creatorTarget,
        std::move(creatorPayload),
        AssetId,
        XMFLOAT3(1.0f, 2.0f, 3.0f),
        1.0f);
    if (!Require(creatorCommand.Execute(),
            "creator-authored reusable placement failed"))
        return 1;

    const wi::ecs::Entity placedAuthoredRoot =
        creatorCommand.PayloadRootEntity();
    const wi::ecs::Entity placedOriginalRoot =
        FindNamedEntity(creatorTarget, "Original Imported Root");
    const auto* placedName =
        creatorTarget.names.GetComponent(placedAuthoredRoot);
    const auto* placedAuthoredTransform =
        creatorTarget.transforms.GetComponent(placedAuthoredRoot);
    if (!Require(placedName != nullptr &&
            placedName->name == CreatorAuthoredTransformRootName,
            "placement did not choose the creator-authored marker root") ||
        !Require(placedAuthoredTransform != nullptr &&
            Near(placedAuthoredTransform->translation_local.y, 0.25f) &&
            Near(placedAuthoredTransform->scale_local.x, 0.125f) &&
            Near(placedAuthoredTransform->scale_local.y, 0.125f) &&
            Near(placedAuthoredTransform->scale_local.z, 0.125f),
            "placement normalized away the creator-approved transform") ||
        !Require(placedOriginalRoot != wi::ecs::INVALID_ENTITY &&
            creatorTarget.Entity_IsDescendant(
                placedOriginalRoot, creatorCommand.PlacedEntity()),
            "creator payload hierarchy was not retained below the stable wrapper"))
        return 1;

    creatorCommand.Undo();
    if (!Require(creatorCommand.Execute(),
            "creator-authored Redo failed"))
        return 1;
    const auto* redoneAuthoredTransform =
        creatorTarget.transforms.GetComponent(
            creatorCommand.PayloadRootEntity());
    if (!Require(redoneAuthoredTransform != nullptr &&
            Near(redoneAuthoredTransform->translation_local.y, 0.25f) &&
            Near(redoneAuthoredTransform->scale_local.x, 0.125f),
            "creator-approved transform did not survive Undo/Redo"))
        return 1;

    fs::remove_all(outputRoot, ec);
    std::cout << "LP07 GATE 6 INSTANCE PASS // stable wrapper identity and creator-authored payload transform survive Undo/Redo and WISCENE Save/Open\n";
    return 0;
}
