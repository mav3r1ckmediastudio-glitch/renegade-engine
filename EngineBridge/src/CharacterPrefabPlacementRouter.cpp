#include "renegade/bridge/ReusableAssetService.h"

#include "renegade/bridge/CharacterPrefabService.h"

#include <algorithm>

namespace renegade::bridge
{
    PreparedReusableModelPlacement ReusableAssetService::PrepareModelAssetPlacement(
        const ReusableModelPlacementRequest& request) const
    {
        // Ordinary imported model/Character assets continue through the exact
        // accepted LP07/CW-04 backend. Only a registered generated Character
        // Prefab is diverted to resolve its stable base Character dependency.
        AssetRegistry registry;
        std::string registryError;
        const bool registryReadable =
            IsValidStableId(request.projectId) &&
            IsValidStableId(request.assetId) &&
            !request.projectRoot.empty() &&
            ReadAssetRegistry(
                request.projectRoot,
                request.projectId,
                registry,
                registryError);

        const AssetRecord* record = nullptr;
        if (registryReadable)
        {
            const auto found = std::find_if(
                registry.records.begin(), registry.records.end(),
                [&request](const AssetRecord& candidate)
                {
                    return candidate.assetId == request.assetId;
                });
            if (found != registry.records.end())
                record = &*found;
        }

        const bool characterPrefab =
            record != nullptr &&
            record->provider == "renegade.character_prefab" &&
            record->providerVersion == CharacterPrefabSchemaVersion &&
            IsCharacterPrefabProjectPath(record->projectRelativePath);
        if (!characterPrefab)
            return PrepareModelAssetPlacementLegacy(request);

        PreparedReusableModelPlacement result;
        auto preparedPrefab = PrepareCharacterPrefabPlacement(
            request.projectRoot,
            request.projectId,
            request.assetId);
        if (!preparedPrefab.IsReady())
        {
            result.result_.assetId = request.assetId;
            result.result_.error = preparedPrefab.error.empty()
                ? "Character Prefab could not be prepared for placement."
                : preparedPrefab.error;
            return result;
        }

        result.scene_ = preparedPrefab.ReleaseScene();
        if (!result.scene_.IsValid())
        {
            result.result_.assetId = request.assetId;
            result.result_.error =
                "Character Prefab lost its prepared base Character scene.";
            return result;
        }

        std::string markerError;
        if (!MarkCharacterPrefabPlacementTemplate(
                *result.scene_, preparedPrefab.document, markerError))
        {
            result.scene_.reset();
            result.result_.assetId = request.assetId;
            result.result_.error =
                "Character Prefab authoring layer could not be attached to its prepared base template: " +
                markerError;
            return result;
        }

        result.result_.assetId = request.assetId;
        result.result_.sourceAssetId = preparedPrefab.document.baseCharacterAssetId;
        result.result_.assetProjectRelativePath = record->projectRelativePath;
        result.result_.sceneSummary = ImportService::Summarize(*result.scene_);
        result.result_.modelEvidence =
            ImportService::SummarizeModelEvidence(*result.scene_);
        result.result_.succeeded = true;
        result.result_.error.clear();
        return result;
    }
}
