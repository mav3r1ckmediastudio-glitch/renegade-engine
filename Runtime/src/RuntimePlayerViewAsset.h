#pragma once

#include "RuntimePlayerViewRig.h"

#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ResourceAssetRuntimeService.h"
#include "renegade/bridge/ReusableAssetRuntimeService.h"
#include "renegade/bridge/ReusableAssetService.h"

#include <string>

namespace renegade::runtime
{
    inline void RemoveRuntimePlayerViewRigProofGeometry(
        wi::scene::Scene& scene,
        RuntimePlayerViewRigState& state) noexcept
    {
        if (state.primaryArmProof != wi::ecs::INVALID_ENTITY &&
            scene.transforms.Contains(state.primaryArmProof))
        {
            scene.Entity_Remove(state.primaryArmProof);
        }
        if (state.offHandArmProof != wi::ecs::INVALID_ENTITY &&
            scene.transforms.Contains(state.offHandArmProof))
        {
            scene.Entity_Remove(state.offHandArmProof);
        }
        state.primaryArmProof = wi::ecs::INVALID_ENTITY;
        state.offHandArmProof = wi::ecs::INVALID_ENTITY;
    }

    inline void SanitizeRuntimePlayerViewModelHierarchy(
        wi::scene::Scene& scene,
        const wi::ecs::Entity root) noexcept
    {
        if (root == wi::ecs::INVALID_ENTITY)
            return;

        for (std::size_t index = 0; index < scene.transforms.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.transforms.GetEntity(index);
            if (entity != root && !scene.Entity_IsDescendant(entity, root))
                continue;

            // A first-person view model is presentation. Retain mesh, armature,
            // animation, material and transform data, but never let an imported
            // Character template become a second controller/physics actor.
            scene.characters.Remove(entity);
            scene.rigidbodies.Remove(entity);
            scene.colliders.Remove(entity);

            if (auto* metadata = scene.metadatas.GetComponent(entity))
            {
                metadata->bool_values.erase(
                    bridge::CharacterAssetTemplateMetadataKey);
                metadata->int_values.erase(
                    bridge::CharacterAssetTemplateVersionMetadataKey);
                metadata->string_values.erase(
                    bridge::PersistentEntityIdMetadataKey);
            }
        }
    }

    [[nodiscard]] inline bool CommitRuntimePlayerViewAsset(
        wi::scene::Scene& scene,
        RuntimePlayerViewRigState& state,
        wi::scene::Scene& preparedScene,
        const bridge::StableId& assetId,
        std::string& error)
    {
        const wi::ecs::Entity viewModelRoot =
            scene.Instantiate(preparedScene, true);
        if (viewModelRoot == wi::ecs::INVALID_ENTITY ||
            !scene.transforms.Contains(viewModelRoot))
        {
            error = "Wicked could not instantiate the first-person arms asset.";
            return false;
        }

        SanitizeRuntimePlayerViewModelHierarchy(scene, viewModelRoot);

        if (!AttachRuntimeViewModelHierarchy(
                scene,
                state,
                viewModelRoot,
                PlayerViewRigSocket::PresentationRoot,
                error))
        {
            scene.Entity_Remove(viewModelRoot);
            return false;
        }

        RemoveRuntimePlayerViewRigProofGeometry(scene, state);
        state.viewModelRoot = viewModelRoot;
        state.viewModelAssetId = assetId;
        error.clear();
        return true;
    }

    [[nodiscard]] inline bool LoadRuntimePlayerViewAsset(
        wi::scene::Scene& scene,
        RuntimePlayerViewRigState& state,
        const std::string& projectRoot,
        const bridge::StableId& projectId,
        const bridge::StableId& assetId,
        std::string& error)
    {
        error.clear();
        if (!state.IsSpawned())
        {
            error = "Runtime Player View Rig is not spawned.";
            return false;
        }
        if (!bridge::IsValidStableId(projectId) ||
            !bridge::IsValidStableId(assetId) ||
            projectRoot.empty())
        {
            error =
                "First-person arms require a valid project root, project ID and asset ID.";
            return false;
        }

        bridge::ReusableModelPlacementRequest request;
        request.projectRoot = projectRoot;
        request.projectId = projectId;
        request.assetId = assetId;

        bridge::ReusableAssetService reusableAssets;
        auto prepared = reusableAssets.PrepareModelAssetPlacement(request);
        wi::scene::Scene* preparedScene = prepared.PeekMutableScene();
        if (!prepared.IsReady() || preparedScene == nullptr)
        {
            error = prepared.Result().error.empty()
                ? "First-person arms asset could not be prepared."
                : prepared.Result().error;
            return false;
        }

        return CommitRuntimePlayerViewAsset(
            scene, state, *preparedScene, assetId, error);
    }

    [[nodiscard]] inline bool LoadPackagedRuntimePlayerViewAsset(
        wi::scene::Scene& scene,
        RuntimePlayerViewRigState& state,
        const std::string& packageRoot,
        const bridge::StableId& projectId,
        const bridge::StableId& assetId,
        std::string& error)
    {
        error.clear();
        if (!state.IsSpawned())
        {
            error = "Runtime Player View Rig is not spawned.";
            return false;
        }

        bridge::PreparedPackagedReusableAsset prepared;
        if (!bridge::PreparePackagedReusableAsset(
                packageRoot, projectId, assetId, prepared, error) ||
            !prepared.IsReady() || !prepared.scene.IsValid())
        {
            if (error.empty())
                error = "Packaged first-person arms asset could not be prepared.";
            return false;
        }

        bridge::PackagedMaterialTextureRefreshResult textureRefresh;
        if (!bridge::RefreshPackagedMaterialTextureAssets(
                *prepared.scene,
                packageRoot,
                projectId,
                textureRefresh,
                error))
        {
            error =
                "Packaged first-person arms material restore failed: " + error;
            return false;
        }

        return CommitRuntimePlayerViewAsset(
            scene, state, *prepared.scene, assetId, error);
    }
}
