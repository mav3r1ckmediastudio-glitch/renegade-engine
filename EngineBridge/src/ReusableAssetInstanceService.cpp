#include "renegade/bridge/ReusableAssetInstanceService.h"

#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/AnimationService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/IdentityService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_set>
#include <utility>

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;
        ReusablePlacementCompanionFactory placementCompanionFactory;

        bool WrapperExists(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity) noexcept
        {
            return entity != wi::ecs::INVALID_ENTITY &&
                scene.transforms.GetComponent(entity) != nullptr;
        }

        void StartCharacterIdlePreview(
            wi::scene::Scene& scene, const wi::ecs::Entity characterRoot)
        {
            const auto clips = CollectAnimationClips(scene, characterRoot, true);
            wi::ecs::Entity idle = wi::ecs::INVALID_ENTITY;
            for (const auto& clip : clips)
            {
                std::string name = clip.name;
                std::transform(name.begin(), name.end(), name.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (idle == wi::ecs::INVALID_ENTITY && clip.channelCount != 0 &&
                    (name.find("idle") != std::string::npos ||
                     name.find("breath") != std::string::npos))
                    idle = clip.entity;
                (void)StopAnimation(scene, clip.entity);
            }
            if (auto* animation = scene.animations.GetComponent(idle))
            {
                animation->SetLooped(true);
                (void)PlayAnimation(scene, idle, true);
            }
        }

        bool AssignFreshReusableHierarchyIdentities(
            wi::scene::Scene& scene,
            const wi::ecs::Entity root) noexcept
        {
            if (root == wi::ecs::INVALID_ENTITY)
                return false;

            // One scene scan rather than one scan for each imported bone/entity.
            // Existing IDs (including IDs on unrelated authored entities) remain
            // reserved; generated IDs also enter this set immediately.
            std::unordered_set<renegade::bridge::StableId> usedIds;
            usedIds.reserve(scene.metadatas.GetCount() + 128);
            for (std::size_t i = 0; i < scene.metadatas.GetCount(); ++i)
            {
                const auto& values = scene.metadatas[i].string_values;
                if (values.has(renegade::bridge::PersistentEntityIdMetadataKey))
                    usedIds.insert(values.get(renegade::bridge::PersistentEntityIdMetadataKey));
            }
            const auto entities = EnumeratePersistentSceneEntities(scene);
            for (const wi::ecs::Entity entity : entities)
            {
                if (entity != root && !scene.Entity_IsDescendant(entity, root))
                    continue;

                // Prefab/template instances can carry copied metadata. Replace
                // it unconditionally at creator placement time so every scene
                // instance is addressable independently. The command snapshot
                // is captured afterwards, therefore Undo/Redo retains these IDs.
                renegade::bridge::StableId id;
                for (int attempt = 0; attempt < 32; ++attempt)
                {
                    auto candidate = renegade::bridge::GenerateStableId();
                    if (usedIds.insert(candidate).second)
                    {
                        id = std::move(candidate);
                        break;
                    }
                }
                if (!renegade::bridge::IsValidStableId(id))
                    return false;
                auto* metadata = scene.metadatas.GetComponent(entity);
                if (metadata == nullptr)
                    metadata = &scene.metadatas.Create(entity);
                metadata->string_values.set(
                    renegade::bridge::PersistentEntityIdMetadataKey, id);
            }
            return true;
        }

        bool ReadInstanceAssetId(
            const wi::scene::MetadataComponent& metadata,
            StableId& assetId,
            std::string& error)
        {
            if (!metadata.string_values.has(ReusableAssetInstanceIdMetadataKey))
                return false;

            if (!metadata.int_values.has(
                    ReusableAssetInstanceVersionMetadataKey) ||
                metadata.int_values.get(
                    ReusableAssetInstanceVersionMetadataKey) !=
                    ReusableAssetInstanceVersion)
            {
                error =
                    "Reusable asset instance metadata has an unsupported version.";
                return false;
            }

            assetId = metadata.string_values.get(
                ReusableAssetInstanceIdMetadataKey);
            if (!IsValidStableId(assetId))
            {
                error =
                    "Reusable asset instance metadata contains an invalid stable asset ID.";
                return false;
            }
            return true;
        }

        bool IsPayloadRootMetadata(
            const wi::scene::MetadataComponent& metadata) noexcept
        {
            return metadata.bool_values.has(ReusableAssetPayloadRootMetadataKey) &&
                metadata.bool_values.get(ReusableAssetPayloadRootMetadataKey);
        }

        wi::ecs::Entity FindNewCreatorAuthoredRoot(
            const wi::scene::Scene& scene,
            const wi::unordered_set<wi::ecs::Entity>& entitiesBefore) noexcept
        {
            for (std::size_t index = 0; index < scene.names.GetCount(); ++index)
            {
                if (scene.names[index].name != CreatorAuthoredTransformRootName)
                    continue;
                const wi::ecs::Entity entity = scene.names.GetEntity(index);
                if (entitiesBefore.count(entity) == 0 &&
                    scene.transforms.GetComponent(entity) != nullptr)
                {
                    return entity;
                }
            }
            return wi::ecs::INVALID_ENTITY;
        }

        std::string NormalizeReusableAssetDisplayName(const std::string& value)
        {
            if (value.empty())
                return {};
            const fs::path input = fs::u8path(value);
            std::string filename = input.filename().generic_u8string();
            if (filename.empty())
                filename = value;
            const fs::path title = fs::u8path(filename);
            const std::string extension =
                wi::helper::toUpper(title.extension().generic_u8string());
            if (extension == ".RASSET" || extension == ".RCHARPREFAB")
                return title.stem().generic_u8string();
            return filename;
        }

        bool IsGeneratedWrapperName(const std::string& value) noexcept
        {
            return value.empty() ||
                value == "Asset" ||
                value == "Reusable Asset Instance" ||
                value == "Reusable Asset Drag Preview";
        }

        bool IsCreatorFacingName(const std::string& value) noexcept
        {
            if (value.empty() || value.rfind("__renegade_", 0) == 0)
                return false;
            return value != "Asset" &&
                value != "Reusable Asset Instance" &&
                value != "Reusable Asset Drag Preview" &&
                value != "Imported Payload Root" &&
                value != "Scene" && value != "Root" && value != "RootNode";
        }

        bool IsPreferredModelName(const std::string& value) noexcept
        {
            if (!IsCreatorFacingName(value))
                return false;

            const std::string upper = wi::helper::toUpper(value);
            if (upper == "SKETCHFAB_MODEL" ||
                upper.rfind("OBJECT_", 0) == 0 ||
                upper.rfind("MATERIAL_", 0) == 0 ||
                upper.find("__MATERIAL") != std::string::npos ||
                upper.find("MATERIAL #") != std::string::npos ||
                upper.find(".GLTF") != std::string::npos ||
                upper.find(".GLB") != std::string::npos ||
                upper.find(".FBX") != std::string::npos ||
                upper.find(".OBJ") != std::string::npos)
            {
                return false;
            }

            // Sketchfab imports commonly introduce a long hash-only node
            // between RootNode and the actual model name. Do not expose that
            // implementation/import identifier as the creator's asset label.
            if (value.size() > 20 &&
                value.front() >= '0' && value.front() <= '9')
            {
                return false;
            }
            return true;
        }

        std::string DeriveReusableAssetName(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity payloadRoot)
        {
            // Start at each visible render object and walk toward the payload
            // root. This reliably skips material/Object_N leaves and picks the
            // nearest meaningful model transform (for example crate002).
            for (std::size_t index = 0; index < scene.objects.GetCount(); ++index)
            {
                const wi::ecs::Entity objectEntity = scene.objects.GetEntity(index);
                if (objectEntity != payloadRoot &&
                    !scene.Entity_IsDescendant(objectEntity, payloadRoot))
                {
                    continue;
                }

                wi::ecs::Entity current = objectEntity;
                const std::size_t maximumDepth = scene.hierarchy.GetCount() + 1;
                for (std::size_t depth = 0;
                    current != wi::ecs::INVALID_ENTITY && depth <= maximumDepth;
                    ++depth)
                {
                    if (const auto* name = scene.names.GetComponent(current);
                        name != nullptr && IsPreferredModelName(name->name))
                    {
                        return name->name;
                    }
                    if (current == payloadRoot)
                        break;
                    const auto* hierarchy = scene.hierarchy.GetComponent(current);
                    if (hierarchy == nullptr ||
                        hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                        hierarchy->parentID == current)
                    {
                        break;
                    }
                    current = hierarchy->parentID;
                }
            }

            // Fallback for transform-only assets: use the first meaningful
            // descendant name in component order.
            for (std::size_t index = 0; index < scene.names.GetCount(); ++index)
            {
                const wi::ecs::Entity entity = scene.names.GetEntity(index);
                if (entity != payloadRoot &&
                    !scene.Entity_IsDescendant(entity, payloadRoot))
                {
                    continue;
                }
                if (IsPreferredModelName(scene.names[index].name))
                    return scene.names[index].name;
            }
            return "Asset";
        }

        bool ReusableWrapperNameInUse(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity except,
            const std::string& candidate) noexcept
        {
            for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
            {
                const wi::ecs::Entity entity = scene.metadatas.GetEntity(index);
                if (entity == except)
                    continue;
                const auto& metadata = scene.metadatas[index];
                if (!metadata.string_values.has(ReusableAssetInstanceIdMetadataKey))
                    continue;
                const auto* name = scene.names.GetComponent(entity);
                if (name != nullptr && name->name == candidate)
                    return true;
            }
            return false;
        }

        std::string MakeUniqueReusableAssetName(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity wrapper,
            const std::string& base)
        {
            if (!ReusableWrapperNameInUse(scene, wrapper, base))
                return base;

            for (std::size_t suffix = 2; suffix < 100000; ++suffix)
            {
                const std::string candidate =
                    base + " (" + std::to_string(suffix) + ")";
                if (!ReusableWrapperNameInUse(scene, wrapper, candidate))
                    return candidate;
            }
            return base;
        }

        bool ApplyReusableAssetName(
            wi::scene::Scene& scene,
            const wi::ecs::Entity wrapper,
            const wi::ecs::Entity payloadRoot,
            const std::string& requestedDisplayName = {})
        {
            auto* existing = scene.names.GetComponent(wrapper);
            if (existing != nullptr && !IsGeneratedWrapperName(existing->name))
            {
                // Never overwrite an explicit creator-authored wrapper name.
                return false;
            }

            std::string base = NormalizeReusableAssetDisplayName(requestedDisplayName);
            if (base.empty())
            {
                const auto* metadata = scene.metadatas.GetComponent(wrapper);
                if (metadata != nullptr && metadata->string_values.has(
                        ReusableAssetInstanceDisplayNameMetadataKey))
                {
                    base = NormalizeReusableAssetDisplayName(
                        metadata->string_values.get(
                            ReusableAssetInstanceDisplayNameMetadataKey));
                }
            }
            if (base.empty())
            {
                // Compatibility only for scenes created before the product title
                // was persisted on the reusable root.
                base = DeriveReusableAssetName(scene, payloadRoot);
            }
            if (base.empty())
                base = "Asset";

            const std::string unique =
                MakeUniqueReusableAssetName(scene, wrapper, base);
            if (existing != nullptr)
                existing->name = unique;
            else
                scene.names.Create(wrapper).name = unique;
            return true;
        }
    }

    void SetReusablePlacementCompanionFactory(
        ReusablePlacementCompanionFactory factory)
    {
        placementCompanionFactory = std::move(factory);
    }

    void ClearReusablePlacementCompanionFactory() noexcept
    {
        placementCompanionFactory = {};
    }

    bool InspectReusableAssetInstances(
        const wi::scene::Scene& scene,
        std::vector<ReusableAssetInstanceRecord>& instances,
        std::string& error)
    {
        instances.clear();

        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const wi::ecs::Entity wrapper = scene.metadatas.GetEntity(index);
            const auto& metadata = scene.metadatas[index];
            if (!metadata.string_values.has(ReusableAssetInstanceIdMetadataKey))
                continue;

            StableId assetId;
            if (!ReadInstanceAssetId(metadata, assetId, error))
                return false;
            if (!WrapperExists(scene, wrapper))
            {
                error =
                    "Reusable asset instance metadata is not attached to a transform entity.";
                return false;
            }

            wi::ecs::Entity payloadRoot = wi::ecs::INVALID_ENTITY;
            for (std::size_t candidateIndex = 0;
                candidateIndex < scene.metadatas.GetCount(); ++candidateIndex)
            {
                const auto& candidateMetadata = scene.metadatas[candidateIndex];
                if (!IsPayloadRootMetadata(candidateMetadata))
                    continue;

                const wi::ecs::Entity candidate =
                    scene.metadatas.GetEntity(candidateIndex);
                if (!scene.Entity_IsDescendant(candidate, wrapper))
                    continue;

                if (payloadRoot != wi::ecs::INVALID_ENTITY)
                {
                    error =
                        "Reusable asset instance contains more than one marked payload root.";
                    return false;
                }
                payloadRoot = candidate;
            }

            if (payloadRoot == wi::ecs::INVALID_ENTITY)
            {
                error =
                    "Reusable asset instance is missing its marked payload root.";
                return false;
            }

            instances.push_back({assetId, wrapper, payloadRoot});
        }

        error.clear();
        return true;
    }

    std::size_t RepairReusableAssetInstanceNames(wi::scene::Scene& scene) noexcept
    {
        std::vector<ReusableAssetInstanceRecord> instances;
        std::string error;
        if (!InspectReusableAssetInstances(scene, instances, error))
            return 0;

        std::size_t renamed = 0;
        for (const auto& instance : instances)
        {
            if (ApplyReusableAssetName(
                    scene, instance.instanceRoot, instance.payloadRoot))
            {
                ++renamed;
            }
        }
        return renamed;
    }

    PlaceReusableModelCommand::PlaceReusableModelCommand(
        wi::scene::Scene& targetScene,
        wi::allocator::shared_ptr<wi::scene::Scene> preparedScene,
        StableId assetId,
        const XMFLOAT3& placementPosition,
        const float scaleFactor,
        std::string displayName)
        : scene_(&targetScene)
        , preparedScene_(std::move(preparedScene))
        , assetId_(std::move(assetId))
        , displayName_(NormalizeReusableAssetDisplayName(displayName))
        , placementPosition_(placementPosition)
        , scaleFactor_(scaleFactor > 0.0f ? scaleFactor : 1.0f)
    {
        promoteCharacter_ =
            preparedScene_.IsValid() &&
            IsCharacterAssetTemplateScene(*preparedScene_);
    }

    PlaceReusableModelCommand::PlaceReusableModelCommand(
        wi::scene::Scene& targetScene,
        StableId assetId,
        const wi::ecs::Entity existingInstanceRoot,
        const wi::ecs::Entity existingPayloadRoot,
        const std::size_t firstMaterialIndex,
        std::string displayName,
        const bool preparedCharacterAsset)
        : scene_(&targetScene)
        , assetId_(std::move(assetId))
        , displayName_(NormalizeReusableAssetDisplayName(displayName))
        , entity_(existingInstanceRoot)
        , payloadRoot_(existingPayloadRoot)
        , firstMaterialIndex_(firstMaterialIndex)
        , adoptExisting_(true)
    {
        // Authoritative cached import recipe survives even if an instancing
        // path drops the optional metadata marker on the new payload.
        promoteCharacter_ = preparedCharacterAsset ||
            CharacterAssetTemplateInHierarchy(targetScene, existingPayloadRoot);
    }

    void PlaceReusableModelCommand::CaptureMaterialResources(
        const std::size_t firstMaterialIndex)
    {
        materialResources_.clear();
        if (scene_ == nullptr)
            return;

        for (std::size_t materialIndex = firstMaterialIndex;
            materialIndex < scene_->materials.GetCount(); ++materialIndex)
        {
            const wi::ecs::Entity materialEntity =
                scene_->materials.GetEntity(materialIndex);
            const auto& material = scene_->materials[materialIndex];
            for (std::uint32_t slot = 0;
                slot < wi::scene::MaterialComponent::TEXTURESLOT_COUNT; ++slot)
            {
                const auto& resource = material.textures[slot].resource;
                if (resource.IsValid())
                    materialResources_.push_back({materialEntity, slot, resource});
            }
        }
    }

    void PlaceReusableModelCommand::RestoreCapturedMaterialResources()
    {
        if (scene_ == nullptr)
            return;

        for (const auto& captured : materialResources_)
        {
            auto* material = scene_->materials.GetComponent(captured.materialEntity);
            if (material == nullptr ||
                captured.slot >= wi::scene::MaterialComponent::TEXTURESLOT_COUNT)
            {
                continue;
            }
            material->textures[captured.slot].resource = captured.resource;
            material->SetDirty();
        }
    }

    bool PlaceReusableModelCommand::PromotePreparedCharacter()
    {
        if (!promoteCharacter_)
            return true;
        if (scene_ == nullptr || entity_ == wi::ecs::INVALID_ENTITY ||
            !scene_->transforms.Contains(entity_))
        {
            return false;
        }
        if (IsRenegadeCharacter(*scene_, entity_))
            return true;

        // Reuse the accepted Character foundation rather than inventing a
        // placement-only controller. Fresh hierarchy IDs were assigned before
        // this call, so MakeCharacterCommand adopts the wrapper's new stable ID,
        // creates the native Wicked CharacterComponent, and writes default
        // gameplay authoring metadata. The outer placement snapshot captures the
        // complete result, making placement + promotion one Undo/Redo action.
        MakeCharacterCommand promotion(
            *scene_, entity_, CharacterAuthoringSettings{});
        return promotion.Execute();
    }

    bool PlaceReusableModelCommand::ApplyPlacementCompanion()
    {
        companion_ = {};
        companionActive_ = false;
        if (!placementCompanionFactory)
            return true;

        std::string error;
        if (!placementCompanionFactory(
                *scene_, entity_, payloadRoot_, companion_, error))
        {
            if (companion_.undo)
                companion_.undo();
            companion_ = {};
            return false;
        }
        companionActive_ =
            static_cast<bool>(companion_.undo) ||
            static_cast<bool>(companion_.redo);
        return true;
    }

    bool PlaceReusableModelCommand::Execute()
    {
        if (!hasSnapshot_)
        {
            if (scene_ == nullptr || !IsValidStableId(assetId_))
                return false;

            if (adoptExisting_)
            {
                if (!WrapperExists(*scene_, entity_) ||
                    !WrapperExists(*scene_, payloadRoot_) ||
                    !scene_->Entity_IsDescendant(payloadRoot_, entity_))
                {
                    return false;
                }

                auto& instanceMetadata = scene_->metadatas.Create(entity_);
                instanceMetadata.string_values.set(
                    ReusableAssetInstanceIdMetadataKey, assetId_);
                instanceMetadata.int_values.set(
                    ReusableAssetInstanceVersionMetadataKey,
                    ReusableAssetInstanceVersion);
                if (!displayName_.empty())
                {
                    instanceMetadata.string_values.set(
                        ReusableAssetInstanceDisplayNameMetadataKey, displayName_);
                }

                auto* payloadMetadata = scene_->metadatas.GetComponent(payloadRoot_);
                if (payloadMetadata == nullptr)
                    payloadMetadata = &scene_->metadatas.Create(payloadRoot_);
                payloadMetadata->bool_values.set(
                    ReusableAssetPayloadRootMetadataKey, true);

                ApplyReusableAssetName(*scene_, entity_, payloadRoot_, displayName_);
                if (!AssignFreshReusableHierarchyIdentities(*scene_, entity_) ||
                    !PromotePreparedCharacter() ||
                    !ApplyPlacementCompanion())
                {
                    scene_->Entity_Remove(entity_);
                    return false;
                }

                if (promoteCharacter_)
                    StartCharacterIdlePreview(*scene_, entity_);

                CaptureMaterialResources(firstMaterialIndex_);
                snapshot_.SetReadModeAndResetPos(false);
                wi::ecs::EntitySerializer serializer;
                scene_->Entity_Serialize(snapshot_, serializer, entity_);
                hasSnapshot_ = true;
                return true;
            }

            if (!preparedScene_.IsValid())
                return false;

            wi::unordered_set<wi::ecs::Entity> entitiesBefore;
            scene_->FindAllEntities(entitiesBefore);
            const std::size_t transformCountBefore = scene_->transforms.GetCount();
            const std::size_t animationCountBefore = scene_->animations.GetCount();
            const std::size_t materialCountBefore = scene_->materials.GetCount();

            scene_->Merge(*preparedScene_);
            preparedScene_.reset();
            if (scene_->transforms.GetCount() <= transformCountBefore)
                return false;

            payloadRoot_ = FindNewCreatorAuthoredRoot(*scene_, entitiesBefore);
            const bool creatorAuthoredPayload =
                payloadRoot_ != wi::ecs::INVALID_ENTITY;
            if (!creatorAuthoredPayload)
                payloadRoot_ = scene_->transforms.GetEntity(transformCountBefore);

            auto* payloadTransform = scene_->transforms.GetComponent(payloadRoot_);
            if (payloadTransform == nullptr)
                return false;

            if (!creatorAuthoredPayload)
            {
                payloadTransform->translation_local = XMFLOAT3(0.0f, 0.0f, 0.0f);
                payloadTransform->scale_local = XMFLOAT3(1.0f, 1.0f, 1.0f);
                payloadTransform->SetDirty();
            }

            entity_ = scene_->Entity_CreateTransform("Asset");
            auto* instanceTransform = scene_->transforms.GetComponent(entity_);
            if (instanceTransform == nullptr)
                return false;
            instanceTransform->translation_local = placementPosition_;
            instanceTransform->scale_local = XMFLOAT3(
                scaleFactor_, scaleFactor_, scaleFactor_);
            instanceTransform->SetDirty();
            // Runtime initializes its native controller from WORLD position.
            // Publish the placed wrapper before attaching its payload.
            instanceTransform->UpdateTransform();

            auto& instanceMetadata = scene_->metadatas.Create(entity_);
            instanceMetadata.string_values.set(
                ReusableAssetInstanceIdMetadataKey, assetId_);
            instanceMetadata.int_values.set(
                ReusableAssetInstanceVersionMetadataKey,
                ReusableAssetInstanceVersion);
            if (!displayName_.empty())
            {
                instanceMetadata.string_values.set(
                    ReusableAssetInstanceDisplayNameMetadataKey, displayName_);
            }

            auto* payloadMetadata = scene_->metadatas.GetComponent(payloadRoot_);
            if (payloadMetadata == nullptr)
                payloadMetadata = &scene_->metadatas.Create(payloadRoot_);
            payloadMetadata->bool_values.set(
                ReusableAssetPayloadRootMetadataKey, true);

            scene_->Component_Attach(payloadRoot_, entity_, true);
            ApplyReusableAssetName(*scene_, entity_, payloadRoot_, displayName_);
            if (!AssignFreshReusableHierarchyIdentities(*scene_, entity_) ||
                !PromotePreparedCharacter() ||
                !ApplyPlacementCompanion())
            {
                scene_->Entity_Remove(entity_);
                return false;
            }

            // Studio displays one real idle pose, never a static FBX bind pose
            // or all mutually exclusive imported actions simultaneously.
            if (promoteCharacter_)
                StartCharacterIdlePreview(*scene_, entity_);
            else if (scene_->animations.GetCount() == animationCountBefore + 1)
                scene_->animations[animationCountBefore].Play();

            CaptureMaterialResources(materialCountBefore);

            snapshot_.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(snapshot_, serializer, entity_);
            hasSnapshot_ = true;
            return true;
        }

        if (scene_ == nullptr || WrapperExists(*scene_, entity_))
            return false;

        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        const wi::ecs::Entity restored = scene_->Entity_Serialize(snapshot_, serializer);
        if (restored != entity_)
            return false;

        std::vector<ReusableAssetInstanceRecord> instances;
        std::string error;
        if (!InspectReusableAssetInstances(*scene_, instances, error))
        {
            scene_->Entity_Remove(entity_);
            return false;
        }
        for (const auto& instance : instances)
        {
            if (instance.instanceRoot == entity_)
            {
                payloadRoot_ = instance.payloadRoot;
                RestoreCapturedMaterialResources();
                if (companionActive_ && companion_.redo && !companion_.redo())
                {
                    scene_->Entity_Remove(entity_);
                    return false;
                }
                return true;
            }
        }
        scene_->Entity_Remove(entity_);
        return false;
    }

    void PlaceReusableModelCommand::Undo()
    {
        if (companionActive_ && companion_.undo)
            companion_.undo();
        if (scene_ != nullptr && WrapperExists(*scene_, entity_))
            scene_->Entity_Remove(entity_);
    }

    wi::ecs::Entity PlaceReusableModelCommand::PlacedEntity() const noexcept
    {
        return entity_;
    }

    wi::ecs::Entity PlaceReusableModelCommand::PayloadRootEntity() const noexcept
    {
        return payloadRoot_;
    }

    const StableId& PlaceReusableModelCommand::AssetId() const noexcept
    {
        return assetId_;
    }
}
