#include "renegade/bridge/VideoAssetService.h"

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ResourceAssetCacheIdentityService.h"
#include "renegade/bridge/ResourceAssetRuntimeService.h"

#include <Utility/minimp4.h>
#include <Utility/h264.h>
#include "renegade/bridge/CreatorVideoWorkflowService.h"
#include <wiHelper.h>
#include <climits>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    // Use the same sample-aligned MP4 decode path for Annex B streams. The
    // pinned raw reader can lose byte alignment after parsing a slice header.
    bool WrapAnnexBForPlayback(const std::vector<uint8_t>& input,std::vector<uint8_t>& mp4)
    {
        constexpr unsigned char prefix[]={0,0,1};
        auto start=std::search(input.begin(),input.end(),std::begin(prefix),std::end(prefix));
        auto write=[](int64_t offset,const void* bytes,size_t count,void* token)->int {
            auto& out=*static_cast<std::vector<uint8_t>*>(token);
            if (offset<0 || static_cast<uint64_t>(offset)>out.max_size()-count) return 1;
            const auto end=static_cast<size_t>(offset)+count;
            if (out.size()<end) out.resize(end);
            std::memcpy(out.data()+offset,bytes,count);
            return 0;
        };
        auto* mux=MP4E_open(0,0,&mp4,write);
        if (!mux) return false;
        int track=-1;
        bool success=true,hasPicture=false,hasPps=false;
        std::vector<uint8_t> sample;
        while (success && start!=input.end())
        {
            const auto nal=start+sizeof(prefix);
            start=std::search(nal,input.end(),std::begin(prefix),std::end(prefix));
            auto end=start;
            while (end!=nal && *(end-1)==0) --end;
            const auto length=end-nal;
            if (length<2) continue;
            if (length>INT_MAX-4) {success=false;break;}
            const auto type=*nal&31;
            if (type==7 && track<0)
            {
                h264::Bitstream bits{};
                bits.init(&*(nal+1),length-1);
                h264::SPS sps{};
                h264::read_sps(&sps,&bits);
                MP4E_track_t description{};
                description.track_media_kind=e_video;
                description.object_type_indication=MP4_OBJECT_TYPE_AVC;
                description.time_scale=60000;
                description.default_duration=1000;
                description.u.v.width=(sps.pic_width_in_mbs_minus1+1)*16-2*(sps.frame_crop_left_offset+sps.frame_crop_right_offset);
                description.u.v.height=(2-sps.frame_mbs_only_flag)*(sps.pic_height_in_map_units_minus1+1)*16-2*(sps.frame_crop_top_offset+sps.frame_crop_bottom_offset);
                track=MP4E_add_track(mux,&description);
                success=track>=0 && MP4E_set_sps(mux,track,&*nal,static_cast<int>(length))==0;
            }
            else if (type==8 && track>=0 && !hasPps)
            {
                hasPps=MP4E_set_pps(mux,track,&*nal,static_cast<int>(length))==0;
                success=hasPps;
            }
            else if (type==1 || type==5)
            {
                if (!hasPps || !(*(nal+1)&0x80)) {success=false;break;}
                const auto n=static_cast<uint32_t>(length);
                sample={uint8_t(n>>24),uint8_t(n>>16),uint8_t(n>>8),uint8_t(n)};
                sample.insert(sample.end(),nal,end);
                success=MP4E_put_sample(mux,track,sample.data(),static_cast<int>(sample.size()),1000,
                    type==5 ? MP4E_SAMPLE_RANDOM_ACCESS : MP4E_SAMPLE_DEFAULT)==0;
                hasPicture=true;
            }
        }
        const int finalStatus=MP4E_close(mux);
        return success && hasPicture && finalStatus==0;
    }

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

    bool MetadataEmpty(const wi::scene::MetadataComponent& metadata)
    {
        return metadata.bool_values.names.empty() &&
            metadata.int_values.names.empty() &&
            metadata.float_values.names.empty() &&
            metadata.string_values.names.empty();
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
    bool ValidateScreenVideoFile(const std::string& path,std::string& error)
    {
        std::error_code ec;
        const auto bytes=fs::file_size(fs::u8path(path),ec);
        if (ec || !bytes || bytes>CreatorVideoWorkflowService::MaximumCreatorVideoBytes)
        {
            error="Choose an available, non-empty video of 4 GiB or less.";
            return false;
        }
        PreparedVideoAsset clip;
        clip.sourceFormat=DetectResourceSourceFormat(path);
        clip.logicalResourceName=path;
        if (!wi::helper::FileRead(path,clip.payload))
        {
            error="Could not read the selected video.";
            return false;
        }
        return LoadPreparedVideoAsset(clip,error).IsValid();
    }

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

    bool PreparePackagedVideo(const std::string& root,const StableId& project,
        const StableId& id,PreparedVideoAsset& prepared,std::string& error)
    {
        PackagedResourceAsset asset;
        if (!PreparePackagedResourceAsset(root,project,id,asset,error) || asset.resourceClass!=ResourceClass::Video) return false;
        return PrepareFromPayload(project,id,asset.packagedAssetPath,asset.sourceFormat,
            asset.payloadHash,std::move(asset.payload),prepared,error);
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

        std::vector<uint8_t> wrapped;
        const bool raw=prepared.sourceFormat==ResourceSourceFormat::H264;
        if (raw && !WrapAnnexBForPlayback(prepared.payload,wrapped))
        {
            error="Could not read the raw H.264 clip. Export single-slice H.264 or H.264 MP4.";
            return {};
        }
        const auto& payload=raw ? wrapped : prepared.payload;
        wi::Resource resource = wi::resourcemanager::Load(
            raw ? prepared.logicalResourceName+".mp4" : prepared.logicalResourceName,
            wi::resourcemanager::Flags::NONE,payload.data(),payload.size());
        if (!resource.IsValid() || !resource.GetVideo().IsValid())
        {
            error = "Wicked could not play this video. Choose H.264 (AVC) in MP4 or a raw H.264 file.";
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

    bool SetVideoAssetCommand::RestoreAfter()
    {
        if (scene_ == nullptr || !capturedAfter_ || !afterResource_.IsValid())
            return false;
        auto* video = scene_->videos.GetComponent(videoEntity_);
        if (video == nullptr)
        {
            error_ = "Video redo requires the original native VideoComponent.";
            return false;
        }

        wi::video::VideoInstance nextInstance;
        if (!wi::video::CreateVideoInstance(&afterResource_.GetVideo(), &nextInstance))
        {
            error_ = "Wicked could not recreate the governed video instance for Redo.";
            return false;
        }

        video->Stop();
        video->filename.clear();
        video->videoResource = afterResource_;
        video->videoinstance = std::move(nextInstance);
        video->currentTimer = 0.0f;
        video->SetLooped(afterLooped_);

        auto* metadata = scene_->metadatas.GetComponent(videoEntity_);
        if (metadata == nullptr)
            metadata = &scene_->metadatas.Create(videoEntity_);
        metadata->int_values.set(VideoAssetBindingVersionMetadataKey, VideoAssetBindingVersion);
        metadata->string_values.set(VideoAssetIdMetadataKey, prepared_.assetId);
        error_.clear();
        return true;
    }

    bool SetVideoAssetCommand::Execute()
    {
        if (capturedAfter_)
            return RestoreAfter();

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
        if (!ApplyPreparedVideoAsset(*scene_, videoEntity_, prepared_, error_))
            return false;

        auto* video = scene_->videos.GetComponent(videoEntity_);
        if (video == nullptr || !video->videoResource.IsValid())
        {
            error_ = "Governed video assignment did not retain a live native resource.";
            RestoreBefore();
            return false;
        }
        afterResource_ = video->videoResource;
        afterLooped_ = video->IsLooped();
        capturedAfter_ = true;

        // Wicked's MP4 loader has already copied the compressed video stream into
        // its native Resource/GPU buffer. Retaining the original .rasset payload
        // inside CommandService would pin a second potentially multi-gigabyte CPU
        // copy for the lifetime of the Undo stack. Redo uses afterResource_ instead.
        std::vector<std::uint8_t>().swap(prepared_.payload);
        return true;
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
            {
                metadata->int_values.erase(VideoAssetBindingVersionMetadataKey);
                metadata->string_values.erase(VideoAssetIdMetadataKey);
                if (MetadataEmpty(*metadata))
                    scene_->metadatas.Remove(videoEntity_);
            }
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
