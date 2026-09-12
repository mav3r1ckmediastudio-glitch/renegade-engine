#include "renegade/bridge/VideoAssetService.h"

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ResourceAssetCacheIdentityService.h"
#include "renegade/bridge/ResourceAssetRuntimeService.h"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    bool IsWithin(const fs::path& child, const fs::path& parent)
    {
        auto childPart = child.begin();
        for (auto parentPart = parent.begin(); parentPart != parent.end();
            ++parentPart, ++childPart)
        {
            if (childPart == child.end() || *childPart != *parentPart)
                return false;
        }
        return true;
    }

    const AssetRecord* FindAssetRecord(
        const AssetRegistry& registry,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            registry.records.begin(), registry.records.end(),
            [&assetId](const AssetRecord& record)
            { return record.assetId == assetId; });
        return found == registry.records.end() ? nullptr : &*found;
    }

    const ImportedProductRecord* FindImportedProduct(
        const AssetRegistry& registry,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            registry.importedProducts.begin(), registry.importedProducts.end(),
            [&assetId](const ImportedProductRecord& record)
            { return record.productAssetId == assetId; });
        return found == registry.importedProducts.end() ? nullptr : &*found;
    }

    bool PrepareFromPayload(
        const StableId& projectId,
        const StableId& assetId,
        const std::string& productPath,
        const ResourceSourceFormat sourceFormat,
        const std::string& payloadHash,
        std::vector<std::uint8_t> payload,
        PreparedVideoAsset& prepared,
        std::string& error)
    {
        if (!IsValidStableId(projectId) || !IsValidStableId(assetId) || payload.empty() ||
            ClassifyResourceSourceFormat(sourceFormat) != ResourceClass::Video)
        {
            error = "The governed video payload is invalid.";
            return false;
        }

        PreparedVideoAsset candidate;
        candidate.projectId = projectId;
        candidate.assetId = assetId;
        candidate.productProjectRelativePath = productPath;
        candidate.sourceFormat = sourceFormat;
        candidate.payloadHash = payloadHash;
        candidate.payload = std::move(payload);
        if (!BuildResourcePayloadCacheName(
                "renegade_video_", assetId, sourceFormat,
                candidate.payload, candidate.logicalResourceName, error))
        {
            return false;
        }
        prepared = std::move(candidate);
        error.clear();
        return true;
    }
}

namespace renegade::bridge
{
    bool PrepareVideoAsset(
        const std::string& projectRoot,
        const StableId& projectId,
        const StableId& videoAssetId,
        PreparedVideoAsset& prepared,
        std::string& error)
    {
        prepared = {};
        if (projectRoot.empty() || !IsValidStableId(projectId) ||
            !IsValidStableId(videoAssetId))
        {
            error = "Video binding requires valid project and video asset identity.";
            return false;
        }

        AssetRegistry registry;
        if (!ReadAssetRegistry(projectRoot, projectId, registry, error))
            return false;
        const AssetRecord* record = FindAssetRecord(registry, videoAssetId);
        if (record == nullptr || record->dependencyClass != DependencyClass::Video ||
            record->requirement != DependencyRequirement::Required ||
            !record->sourceAvailable || record->provider != "lp08.rasset" ||
            record->providerVersion != 1)
        {
            error = "The selected stable ID is not an available governed video product.";
            return false;
        }
        const ImportedProductRecord* provenance = FindImportedProduct(registry, videoAssetId);
        if (provenance == nullptr || provenance->importer != "wicked.resourcemanager" ||
            provenance->importerVersion != 1)
        {
            error = "The governed video is missing its accepted Wicked import provenance.";
            return false;
        }

        std::error_code ec;
        const fs::path root = fs::weakly_canonical(
            fs::absolute(fs::u8path(projectRoot), ec), ec);
        if (ec || root.empty() || !fs::is_directory(root, ec) || ec)
        {
            error = "The video project root is unavailable.";
            return false;
        }
        const fs::path content = fs::weakly_canonical(root / "Content", ec);
        if (ec || !fs::is_directory(content, ec) || ec)
        {
            error = "The project Content directory is unavailable.";
            return false;
        }
        const fs::path product = fs::weakly_canonical(
            root / fs::u8path(record->projectRelativePath), ec);
        if (ec || !fs::is_regular_file(product, ec) || ec || !IsWithin(product, content))
        {
            error = "The governed video product is unavailable or outside project Content.";
            return false;
        }

        ResourceAssetDocument document;
        if (!ReadResourceAssetDocument(product.generic_u8string(), document, error))
            return false;
        if (document.manifest.projectId != projectId ||
            document.manifest.assetId != videoAssetId ||
            document.manifest.resourceClass != ResourceClass::Video ||
            document.payload.empty())
        {
            error = "The governed video product does not match its LC01 stable identity.";
            return false;
        }

        return PrepareFromPayload(
            projectId,
            videoAssetId,
            record->projectRelativePath,
            document.manifest.sourceFormat,
            document.manifest.payloadHash,
            std::move(document.payload),
            prepared,
            error);
    }

    wi::Resource LoadPreparedVideoAsset(
        const PreparedVideoAsset& prepared,
        std::string& error)
    {
        if (prepared.logicalResourceName.empty() || prepared.payload.empty() ||
            ClassifyResourceSourceFormat(prepared.sourceFormat) != ResourceClass::Video)
        {
            error = "The prepared video payload is empty or invalid.";
            return {};
        }

        wi::Resource resource = wi::resourcemanager::Load(
            prepared.logicalResourceName,
            wi::resourcemanager::Flags::NONE,
            prepared.payload.data(), prepared.payload.size());
        if (!resource.IsValid() || !resource.GetVideo().IsValid())
        {
            error = "Wicked could not decode the governed video payload.";
            return {};
        }
        error.clear();
        return resource;
    }

    bool ApplyPreparedVideoAsset(
        wi::scene::Scene& scene,
        const wi::ecs::Entity videoEntity,
        const PreparedVideoAsset& prepared,
        std::string& error)
    {
        auto* video = scene.videos.GetComponent(videoEntity);
        if (video == nullptr)
        {
            error = "Video assignment requires a native VideoComponent.";
            return false;
        }

        wi::Resource nextResource = LoadPreparedVideoAsset(prepared, error);
        if (!nextResource.IsValid())
            return false;
        wi::video::VideoInstance nextInstance;
        if (!wi::video::CreateVideoInstance(&nextResource.GetVideo(), &nextInstance))
        {
            error = "Wicked could not create a video instance from the governed payload.";
            return false;
        }

        const bool looped = video->IsLooped();
        video->Stop();
        video->filename.clear();
        video->videoResource = std::move(nextResource);
        video->videoinstance = std::move(nextInstance);
        video->currentTimer = 0.0f;
        video->SetLooped(looped);

        auto* metadata = scene.metadatas.GetComponent(videoEntity);
        if (metadata == nullptr)
            metadata = &scene.metadatas.Create(videoEntity);
        metadata->int_values.set(VideoAssetBindingVersionMetadataKey, VideoAssetBindingVersion);
        metadata->string_values.set(VideoAssetIdMetadataKey, prepared.assetId);
        error.clear();
        return true;
    }

    bool InspectVideoAssetBindings(
        const wi::scene::Scene& scene,
        std::vector<VideoAssetBindingRecord>& bindings,
        std::string& error)
    {
        bindings.clear();
        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const auto entity = scene.metadatas.GetEntity(index);
            const auto& metadata = scene.metadatas[index];
            if (!metadata.string_values.has(VideoAssetIdMetadataKey))
                continue;
            if (!scene.videos.Contains(entity))
            {
                error = "A governed video StableId is attached to an entity without VideoComponent.";
                return false;
            }
            if (!metadata.int_values.has(VideoAssetBindingVersionMetadataKey) ||
                metadata.int_values.get(VideoAssetBindingVersionMetadataKey) != VideoAssetBindingVersion)
            {
                error = "A governed video binding has an unsupported metadata version.";
                return false;
            }
            const StableId assetId = metadata.string_values.get(VideoAssetIdMetadataKey);
            if (!IsValidStableId(assetId))
            {
                error = "A governed video binding contains an invalid StableId.";
                return false;
            }
            bindings.push_back({entity, assetId});
        }
        error.clear();
        return true;
    }

    VideoAssetRestoreResult RestoreVideoAssetBindings(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId)
    {
        VideoAssetRestoreResult result;
        std::vector<VideoAssetBindingRecord> bindings;
        if (!InspectVideoAssetBindings(scene, bindings, result.error))
            return result;
        result.discovered = bindings.size();

        for (const auto& binding : bindings)
        {
            PreparedVideoAsset prepared;
            if (!PrepareVideoAsset(
                    projectRoot, projectId, binding.videoAssetId, prepared, result.error))
            {
                return result;
            }
            if (!ApplyPreparedVideoAsset(scene, binding.videoEntity, prepared, result.error))
                return result;
            ++result.restored;
        }
        result.succeeded = true;
        result.error.clear();
        return result;
    }

    VideoAssetRestoreResult RestorePackagedVideoAssetBindings(
        wi::scene::Scene& scene,
        const std::string& packageRoot,
        const StableId& projectId)
    {
        VideoAssetRestoreResult result;
        std::vector<VideoAssetBindingRecord> bindings;
        if (!InspectVideoAssetBindings(scene, bindings, result.error))
            return result;
        result.discovered = bindings.size();

        for (const auto& binding : bindings)
        {
            PackagedResourceAsset packaged;
            if (!PreparePackagedResourceAsset(
                    packageRoot, projectId, binding.videoAssetId, packaged, result.error))
            {
                return result;
            }
            if (packaged.resourceClass != ResourceClass::Video)
            {
                result.error = "Packaged governed video StableId resolved to a non-video resource.";
                return result;
            }

            PreparedVideoAsset prepared;
            if (!PrepareFromPayload(
                    projectId,
                    binding.videoAssetId,
                    packaged.packagedAssetPath,
                    packaged.sourceFormat,
                    packaged.payloadHash,
                    std::move(packaged.payload),
                    prepared,
                    result.error))
            {
                return result;
            }
            if (!ApplyPreparedVideoAsset(scene, binding.videoEntity, prepared, result.error))
                return result;
            ++result.restored;
        }
        result.succeeded = true;
        result.error.clear();
        return result;
    }

    SetVideoAssetCommand::SetVideoAssetCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity videoEntity,
        PreparedVideoAsset prepared)
        : scene_(&scene), videoEntity_(videoEntity), prepared_(std::move(prepared))
    {
    }

    void SetVideoAssetCommand::CaptureBefore()
    {
        if (capturedBefore_ || scene_ == nullptr)
            return;
        const auto* video = scene_->videos.GetComponent(videoEntity_);
        if (video == nullptr)
            return;

        beforeFilename_ = video->filename;
        beforeResource_ = video->videoResource;
        beforeLooped_ = video->IsLooped();
        const auto* metadata = scene_->metadatas.GetComponent(videoEntity_);
        hadMetadata_ = metadata != nullptr;
        if (metadata != nullptr)
        {
            hadVersion_ = metadata->int_values.has(VideoAssetBindingVersionMetadataKey);
            if (hadVersion_)
                beforeVersion_ = metadata->int_values.get(VideoAssetBindingVersionMetadataKey);
            hadAssetId_ = metadata->string_values.has(VideoAssetIdMetadataKey);
            if (hadAssetId_)
                beforeAssetId_ = metadata->string_values.get(VideoAssetIdMetadataKey);
        }
        capturedBefore_ = true;
    }

    bool SetVideoAssetCommand::Execute()
    {
        CaptureBefore();
        if (!capturedBefore_ || scene_ == nullptr || !IsValidStableId(prepared_.assetId))
            return false;

        const auto* metadata = scene_->metadatas.GetComponent(videoEntity_);
        if (metadata != nullptr && metadata->string_values.has(VideoAssetIdMetadataKey) &&
            metadata->string_values.get(VideoAssetIdMetadataKey) == prepared_.assetId)
        {
            const auto* video = scene_->videos.GetComponent(videoEntity_);
            if (video != nullptr && video->videoResource.IsValid())
                return false;
        }
        return ApplyPreparedVideoAsset(*scene_, videoEntity_, prepared_, error_);
    }

    void SetVideoAssetCommand::RestoreBefore() noexcept
    {
        if (scene_ == nullptr || !capturedBefore_)
            return;
        auto* video = scene_->videos.GetComponent(videoEntity_);
        if (video == nullptr)
            return;

        video->Stop();
        video->filename = beforeFilename_;
        video->videoResource = beforeResource_;
        video->videoinstance = {};
        video->currentTimer = 0.0f;
        if (video->videoResource.IsValid())
        {
            (void)wi::video::CreateVideoInstance(
                &video->videoResource.GetVideo(), &video->videoinstance);
        }
        else if (!beforeFilename_.empty())
        {
            video->videoResource = wi::resourcemanager::Load(beforeFilename_);
            if (video->videoResource.IsValid())
            {
                (void)wi::video::CreateVideoInstance(
                    &video->videoResource.GetVideo(), &video->videoinstance);
            }
        }
        video->SetLooped(beforeLooped_);

        auto* metadata = scene_->metadatas.GetComponent(videoEntity_);
        if (!hadMetadata_)
        {
            if (metadata != nullptr)
                scene_->metadatas.Remove(videoEntity_);
            return;
        }
        if (metadata == nullptr)
            metadata = &scene_->metadatas.Create(videoEntity_);

        if (hadVersion_)
            metadata->int_values.set(VideoAssetBindingVersionMetadataKey, beforeVersion_);
        else
            metadata->int_values.erase(VideoAssetBindingVersionMetadataKey);
        if (hadAssetId_)
            metadata->string_values.set(VideoAssetIdMetadataKey, beforeAssetId_);
        else
            metadata->string_values.erase(VideoAssetIdMetadataKey);
    }

    void SetVideoAssetCommand::Undo()
    {
        RestoreBefore();
    }
}
