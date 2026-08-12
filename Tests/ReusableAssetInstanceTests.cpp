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
            "replaceable payload root was not normalized into wrapper-local space"))
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

    fs::remove_all(outputRoot, ec);
    std::cout << "LP07 GATE 6 INSTANCE PASS // stable wrapper identity survives Undo/Redo and WISCENE Save/Open\n";
    return 0;
}
