#include "renegade/bridge/ModelImportCandidateService.h"
#include "renegade/bridge/HumanoidRetargetService.h"
#include "renegade/bridge/ModelAnimationPreviewService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ModelImporterFailureAdapter.h"
#include <ModelImporter.h>
#include "renegade/bridge/MatchingRigAnimationService.h"
#define UFBX_REAL_TYPE float
#include <ufbx.h>
#include <algorithm>
#include <filesystem>
#include <exception>
#include <fstream>
#include <memory>
#include <set>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace
{
    namespace fs = std::filesystem;
    bool ReadBytes(const fs::path& path, std::vector<std::uint8_t>& bytes)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) return false;
        bytes.assign(std::istreambuf_iterator<char>(stream), {});
        return !stream.bad() && !bytes.empty();
    }
    std::uint64_t Fingerprint(const std::vector<std::uint8_t>& bytes)
    {
        std::uint64_t hash = 1469598103934665603ull;
        for (auto value : bytes) { hash ^= value; hash *= 1099511628211ull; }
        return hash;
    }
    bool Within(const fs::path& path, const fs::path& root)
    {
        return std::mismatch(root.begin(), root.end(), path.begin(), path.end()).first == root.end();
    }
    std::string String(ufbx_string value) { return std::string(value.data, value.length); }
    bool SnapshotFbxTextures(const fs::path& source,
        std::vector<renegade::bridge::ModelImportDependency>& dependencies,
        std::string& error, std::vector<std::string>& warnings, const std::set<std::string>* required = nullptr)
    {
        ufbx_load_opts options = {};
        options.ignore_geometry = true;
        options.ignore_animation = true;
        ufbx_error parseError = {};
        const auto name = source.generic_u8string();
        std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> parsed(
            ufbx_load_file(name.c_str(), &options, &parseError), &ufbx_free_scene);
        if (!parsed)
        {
            error = "FBX dependency inspection failed: " + String(parseError.description);
            return false;
        }
        std::set<std::string> destinations;
        for (const auto& file : parsed->texture_files)
        {
            renegade::bridge::ModelImportDependency dependency;
            dependency.referenceName = String(file.filename);
            if(required && !required->count(dependency.referenceName))continue;
            fs::path texturePath = fs::u8path(dependency.referenceName);
            if (texturePath.filename().empty())
            {
                warnings.push_back("An unnamed FBX texture was skipped; assign its map below.");
                continue;
            }
            if (file.content.data != nullptr && file.content.size != 0)
            {
                const auto* first = static_cast<const std::uint8_t*>(file.content.data);
                dependency.bytes.assign(first, first + file.content.size);
                dependency.retainedRelativePath = "embedded/" + std::to_string(file.index) +
                    "/" + texturePath.filename().generic_u8string();
            }
            else
            {
                std::error_code ec;
                if (!texturePath.is_absolute()) texturePath = source.parent_path() / texturePath;
                texturePath = fs::weakly_canonical(texturePath, ec);
                if (ec || !fs::is_regular_file(texturePath, ec) || ec ||
                    !ReadBytes(texturePath, dependency.bytes))
                {
                    // Exporters often keep the author's obsolete absolute path.
                    ec.clear();
                    texturePath = source.parent_path() / fs::u8path(String(file.relative_filename));
                    if (!fs::is_regular_file(texturePath, ec) || ec || !ReadBytes(texturePath, dependency.bytes))
                    {
                        ec.clear();
                        texturePath = source.parent_path() / fs::u8path(dependency.referenceName).filename();
                        if (!fs::is_regular_file(texturePath, ec) || ec || !ReadBytes(texturePath, dependency.bytes))
                        {
                            warnings.push_back("Missing texture: " + dependency.referenceName);
                            continue;
                        }
                    }
                    texturePath = fs::weakly_canonical(texturePath, ec);
                }
                dependency.sourcePath = texturePath.generic_u8string();
                dependency.retainedRelativePath = Within(texturePath, source.parent_path())
                    ? texturePath.lexically_relative(source.parent_path()).generic_u8string()
                    : "external/" + std::to_string(file.index) + "/" + texturePath.filename().generic_u8string();
            }
            if (!destinations.insert(dependency.retainedRelativePath).second)
            {
                dependency.retainedRelativePath = "textures/" + std::to_string(file.index) + "/" + texturePath.filename().generic_u8string();
            }
            dependency.previewResourceName = (source.parent_path() / ".__renegade_preview" /
                renegade::bridge::GenerateStableId() / texturePath.filename()).generic_u8string();
            dependencies.push_back(std::move(dependency));
        }
        return true;
    }
}

namespace renegade::bridge
{
    bool ModelImportCandidateService::AppendExternalAnimations(
        ModelImportCandidate& candidate, const std::string& sourcePath,
        std::string& error) const
    {
        error.clear();
        if (!candidate.IsReady() || candidate.evidence_.skinnedMeshes == 0 ||
            candidate.evidence_.armatureBones == 0)
        { error = "External animations require a ready rigged Character."; return false; }
        const auto sourceKind = ClassifyHumanoidAnimationSource(sourcePath);
        if (sourceKind != HumanoidAnimationSourceFormat::Fbx)
        { error = "External animation import currently accepts FBX only."; return false; }
        std::vector<std::uint8_t> before, after;
        if (!ReadBytes(fs::u8path(sourcePath), before) || before.empty())
        { error = "External animation source could not be read."; return false; }
        auto prepared = wi::allocator::make_shared_single<wi::scene::Scene>();
        try
        {
            wi::Archive clone;
            candidate.scene_->Serialize(clone);
            clone.SetReadModeAndResetPos(true);
            prepared->Serialize(clone);
            const auto first = prepared->animations.GetCount();
            std::vector<wi::ecs::Entity> created;
            const bool matchingRig=ImportMatchingRigAnimations(*prepared,sourcePath,created,error);
            if(!matchingRig) {
            bool alreadyMapped = false;
            for (std::size_t i = 0; i < prepared->humanoids.GetCount(); ++i)
                alreadyMapped = alreadyMapped || prepared->humanoids[i].IsValid();
            if (!alreadyMapped && !EnsureHumanoidAnimationSourceMapping(*prepared, error)) return false;
            (void)DisableDefaultHumanoidLookAt(*prepared);
            wi::ecs::Entity destination = wi::ecs::INVALID_ENTITY;
            for (std::size_t i = 0; i < prepared->humanoids.GetCount(); ++i)
                if (prepared->humanoids[i].IsValid())
                {
                    if (destination != wi::ecs::INVALID_ENTITY)
                    { error = "External animation import requires exactly one humanoid rig."; return false; }
                    destination = prepared->humanoids.GetEntity(i);
                }
            if (destination == wi::ecs::INVALID_ENTITY)
            { error = "Character humanoid mapping is incomplete."; return false; }
            ClearWickedModelImporterFailureDiagnostic();
            RetargetHumanoidAnimationsCommand retarget(*prepared, destination, sourcePath, true);
            const bool succeeded = retarget.Execute();
            const auto importerFailure = ConsumeWickedModelImporterFailureDiagnostic();
            if (!succeeded || !importerFailure.empty())
            { error = importerFailure.empty() ? retarget.Result().error : importerFailure; return false; }
            created.assign(retarget.Result().createdAnimations.begin(),retarget.Result().createdAnimations.end());
            const auto& result = retarget.Result();
            if (result.createdAnimations.size() != result.sourceAnimationCount)
            { error = "Not every source clip could be retargeted; nothing was added."; return false; }

            }
            if (!ReadBytes(fs::u8path(sourcePath), after) || after != before)
            { error = "External animation source changed during conversion."; return false; }
            (void)DisableDefaultHumanoidLookAt(*prepared);
            PauseImportedModelAnimations(*prepared);
            ModelImportDependency source;
            source.sourcePath = fs::absolute(fs::u8path(sourcePath)).generic_u8string();
            source.retainedRelativePath = GenerateStableId() + "/" +
                fs::u8path(sourcePath).filename().generic_u8string();
            source.bytes = std::move(before);
            candidate.externalAnimations_.reserve(candidate.externalAnimations_.size() + 1);
            candidate.dependencies_.reserve(candidate.dependencies_.size() + 1);
            candidate.externalAnimations_.push_back({candidate.dependencies_.size(), first,
                created.size(), matchingRig});
            candidate.dependencies_.push_back(std::move(source));
            candidate.scene_ = std::move(prepared);
            PruneUnusedTextureDependencies(candidate);
            candidate.summary_ = ImportService::Summarize(*candidate.scene_);
            candidate.evidence_ = ImportService::SummarizeModelEvidence(*candidate.scene_);
            error.clear();
            return true;
        }
        catch (const std::exception& exception) { error = exception.what(); }
        catch (...) { error = "External animation conversion failed."; }
        return false;
    }

    ModelImportCandidate ModelImportCandidateService::PrepareModel(const std::string& sourcePath) const
    {
        return PrepareStaticModel(sourcePath);
    }
    ModelImportCandidate ModelImportCandidateService::PrepareModel(const std::string& sourcePath,
        const std::vector<ModelImportTextureRelink>& relinks) const
    { return PrepareWithRelinks(sourcePath,relinks); }
    ModelImportCandidate ModelImportCandidateService::PrepareGlb(const std::string& sourcePath) const
    {
        if (ImportService::ClassifyModelSourceFormat(sourcePath) == ModelSourceFormat::Glb)
            return PrepareStaticModel(sourcePath);
        ModelImportCandidate candidate;
        candidate.error_ = "The GLB compatibility entry point accepts GLB files only.";
        return candidate;
    }
    ModelImportCandidate ModelImportCandidateService::PrepareStaticModel(const std::string& sourcePath) const
    { return PrepareWithRelinks(sourcePath,{}); }
    void ModelImportCandidateService::PruneUnusedTextureDependencies(ModelImportCandidate& candidate)
    {
        for (size_t d = candidate.dependencies_.size(); d-- > 0;)
        {
            const auto& dependency = candidate.dependencies_[d];
            // MTL and external animation snapshots have no preview texture key.
            if (dependency.previewResourceName.empty()) continue;
            bool used = false;
            for (size_t i = 0; i < candidate.scene_->materials.GetCount() && !used; ++i)
                for (const auto& texture : candidate.scene_->materials[i].textures)
                    if (texture.name == dependency.previewResourceName) { used = true; break; }
            if (used) continue;
            candidate.dependencies_.erase(candidate.dependencies_.begin() + d);
            for (auto& animation : candidate.externalAnimations_)
                if (animation.dependencyIndex > d) --animation.dependencyIndex;
        }
    }

    bool ModelImportCandidateService::AssignTexture(ModelImportCandidate& candidate,
        const ModelImportTextureRelink& relink, std::string& error) const
    {
        if (!candidate.IsReady() || relink.materialIndex >= candidate.scene_->materials.GetCount() ||
            relink.textureSlot >= wi::scene::MaterialComponent::TEXTURESLOT_COUNT)
        { error = "Select a valid model material and map slot."; return false; }
        std::error_code ec;
        const auto path = fs::weakly_canonical(fs::absolute(fs::u8path(relink.sourcePath), ec), ec);
        ModelImportDependency d;
        if (ec || !fs::is_regular_file(path, ec) || ec || !ReadBytes(path, d.bytes))
        { error = "Selected map is missing or unreadable."; return false; }
        d.sourcePath = path.generic_u8string();
        d.explicitRelink = true;
        d.materialIndex = relink.materialIndex; d.textureSlot = relink.textureSlot;
        d.referenceName = "relink:" + std::to_string(relink.materialIndex) + ":" + std::to_string(relink.textureSlot);
        d.retainedRelativePath = "relinked/" + std::to_string(relink.materialIndex) + "/" +
            std::to_string(relink.textureSlot) + "/" + path.filename().generic_u8string();
        d.previewResourceName = (path.parent_path() / ".__renegade_preview" / GenerateStableId() / path.filename()).generic_u8string();
        const auto resource = wi::resourcemanager::Load(d.previewResourceName,
            wi::resourcemanager::Flags::IMPORT_RETAIN_FILEDATA, d.bytes.data(), d.bytes.size());
        if (!resource.IsValid() || !resource.GetTexture().IsValid())
        { error = "Selected map could not decode. The previous assignment is unchanged."; return false; }
        auto& material = candidate.scene_->materials[relink.materialIndex];
        auto& texture = material.textures[relink.textureSlot];
        auto existing = std::find_if(candidate.dependencies_.begin(), candidate.dependencies_.end(),
            [&relink](const auto& value){ return value.explicitRelink &&
                value.materialIndex == relink.materialIndex && value.textureSlot == relink.textureSlot; });
        if (existing == candidate.dependencies_.end()) candidate.dependencies_.push_back(d);
        else *existing = d;
        texture.name = d.previewResourceName; texture.resource = resource;
        if (relink.textureSlot == wi::scene::MaterialComponent::NORMALMAP) material.normalMapStrength = 1;
        if (relink.textureSlot == wi::scene::MaterialComponent::OCCLUSIONMAP) material.SetOcclusionEnabled_Secondary(true);
        material.SetDirty();
        PruneUnusedTextureDependencies(candidate);
        error.clear();
        return true;
    }

    ModelImportCandidate ModelImportCandidateService::PrepareWithRelinks(const std::string& sourcePath,
        const std::vector<ModelImportTextureRelink>& relinks) const
    {
        ModelImportCandidate candidate;
        std::error_code ec;
        const fs::path source = fs::weakly_canonical(fs::absolute(fs::u8path(sourcePath), ec), ec);
        if (ec || !fs::is_regular_file(source, ec) || ec)
        {
            candidate.error_ = "Selected model source does not exist.";
            return candidate;
        }
        candidate.sourcePath_ = source.generic_u8string();
        candidate.sourceFormat_ = ImportService::ClassifyModelSourceFormat(candidate.sourcePath_);
        if (candidate.sourceFormat_ != ModelSourceFormat::Glb && candidate.sourceFormat_ != ModelSourceFormat::Fbx && candidate.sourceFormat_ != ModelSourceFormat::Obj)
        {
            candidate.error_ = "This model importer accepts GLB, FBX and OBJ files.";
            return candidate;
        }
        if (wi::graphics::GetDevice() == nullptr)
        {
            candidate.error_ = "Model conversion requires an initialized graphics device.";
            return candidate;
        }
        std::vector<std::uint8_t> sourceBytes;
        if (!ReadBytes(source, sourceBytes))
        {
            candidate.error_ = "Selected model source could not be read.";
            return candidate;
        }
        candidate.sourceBytes_ = sourceBytes.size();
        candidate.sourceFingerprint_ = Fingerprint(sourceBytes);
        if (candidate.sourceFormat_ == ModelSourceFormat::Fbx &&
            !SnapshotFbxTextures(source, candidate.dependencies_, candidate.error_, candidate.warnings_)) return candidate;
        if (candidate.sourceFormat_ == ModelSourceFormat::Obj)
        {
            std::ifstream obj(source);
            std::string line;
            std::set<std::string> retained;
            while (std::getline(obj, line))
            {
                std::istringstream fields(line);
                std::string keyword, name;
                fields >> keyword;
                if (keyword != "mtllib") continue;
                while (fields >> std::quoted(name))
                {
                    if (!name.empty() && name[0] == '#') break;
                    std::error_code pathError;
                    auto path = fs::weakly_canonical(source.parent_path() / fs::u8path(name), pathError);
                    ModelImportDependency d;
                    if (pathError || !ReadBytes(path, d.bytes))
                    { candidate.warnings_.push_back("Missing OBJ material library: " + name); continue; }
                    if (!retained.insert(path.generic_u8string()).second) continue;
                    d.sourcePath = path.generic_u8string();
                    d.referenceName = "mtllib:" + name;
                    d.retainedRelativePath = Within(path, source.parent_path())
                        ? path.lexically_relative(source.parent_path()).generic_u8string()
                        : "libraries/" + std::to_string(candidate.dependencies_.size()) + "/" + path.filename().generic_u8string();
                    candidate.dependencies_.push_back(std::move(d));
                }
            }
        }
        candidate.scene_ = wi::allocator::make_shared_single<wi::scene::Scene>();
        ClearWickedModelImporterFailureDiagnostic();
        try
        {
            if (candidate.sourceFormat_ == ModelSourceFormat::Fbx)
                ImportModel_FBX(candidate.sourcePath_, *candidate.scene_);
            else if (candidate.sourceFormat_ == ModelSourceFormat::Obj) ImportModel_OBJ(candidate.sourcePath_, *candidate.scene_);
            else ImportModel_GLTF(candidate.sourcePath_, *candidate.scene_);
            // Unique cache keys force the preview to use the exact snapshotted
            // bytes, even when a previous import cached the original filename.
            for (size_t i = 0; i < candidate.scene_->materials.GetCount(); ++i)
            {
                for(size_t slot=0;slot<wi::scene::MaterialComponent::TEXTURESLOT_COUNT;++slot)
                {
                    auto& texture=candidate.scene_->materials[i].textures[slot];
                    if (candidate.sourceFormat_ == ModelSourceFormat::Glb || texture.name.empty()) continue;
                    const std::string reference = texture.name;
                    auto found = std::find_if(candidate.dependencies_.begin(), candidate.dependencies_.end(),
                        [&texture](const auto& d){return d.referenceName == texture.name;});
                    if (found == candidate.dependencies_.end() && candidate.sourceFormat_ == ModelSourceFormat::Obj)
                    {
                        ModelImportDependency d;
                        auto path = fs::weakly_canonical(fs::u8path(reference), ec);
                        if (!ec && ReadBytes(path, d.bytes))
                        {
                            d.referenceName = reference;
                            d.sourcePath = path.generic_u8string();
                            d.retainedRelativePath = Within(path, source.parent_path())
                                ? path.lexically_relative(source.parent_path()).generic_u8string()
                                : "textures/" + std::to_string(candidate.dependencies_.size()) + "/" + path.filename().generic_u8string();
                            d.previewResourceName = (source.parent_path() / ".__renegade_preview" / GenerateStableId() / path.filename()).generic_u8string();
                            candidate.dependencies_.push_back(std::move(d));
                            found = candidate.dependencies_.end() - 1;
                        }
                    }
                    if (found != candidate.dependencies_.end())
                    {
                        auto resource = wi::resourcemanager::Load(found->previewResourceName,
                            wi::resourcemanager::Flags::IMPORT_RETAIN_FILEDATA, found->bytes.data(), found->bytes.size());
                        if (resource.IsValid() && resource.GetTexture().IsValid())
                        { texture.resource = resource; texture.name = found->previewResourceName; continue; }
                    }
                    candidate.warnings_.push_back("Material " + std::to_string(i + 1) + ", map " +
                        std::to_string(slot) + ": unavailable or unreadable texture " + reference);
                    texture.name.clear();
                    texture.resource = {};
                }
                candidate.scene_->materials[i].SetDirty();
            }
        }
        catch (const std::exception& exception) { candidate.error_ = std::string("Model conversion failed: ") + exception.what(); }
        catch (...) { candidate.error_ = "Model conversion failed with an unknown error."; }
        if (candidate.error_.empty())
        {
            std::set<std::pair<std::uint32_t,std::uint32_t>> slots;
            for (const auto& relink : relinks)
            {
                if (!slots.emplace(relink.materialIndex, relink.textureSlot).second ||
                    !AssignTexture(candidate, relink, candidate.error_))
                {
                    if (candidate.error_.empty()) candidate.error_ = "Duplicate texture assignment.";
                    break;
                }
            }
        }
        const auto importerFailure = ConsumeWickedModelImporterFailureDiagnostic();
        if (!importerFailure.empty() && candidate.error_.empty()) candidate.error_ = "Model converter reported: " + importerFailure;
        std::vector<std::uint8_t> after;
        if (candidate.error_.empty() && (!ReadBytes(source, after) || after != sourceBytes))
            candidate.error_ = "Model source changed during conversion.";
        for (const auto& d : candidate.dependencies_)
            if (candidate.error_.empty() && !d.sourcePath.empty() &&
                (!ReadBytes(fs::u8path(d.sourcePath), after) || after != d.bytes))
                candidate.error_ = "FBX texture changed during conversion: " + fs::u8path(d.sourcePath).filename().generic_u8string();
        if (!candidate.error_.empty()) { candidate.scene_.reset(); return candidate; }
        // Only retain maps that actually decoded and are used by the candidate.
        candidate.dependencies_.erase(std::remove_if(candidate.dependencies_.begin(), candidate.dependencies_.end(),
            [&candidate](const auto& d) {
                if (d.previewResourceName.empty() || d.explicitRelink) return false;
                for (size_t i = 0; i < candidate.scene_->materials.GetCount(); ++i)
                    for (const auto& texture : candidate.scene_->materials[i].textures)
                        if (texture.name == d.previewResourceName) return false;
                return true;
            }), candidate.dependencies_.end());
        candidate.summary_ = ImportService::Summarize(*candidate.scene_);
        candidate.evidence_ = ImportService::SummarizeModelEvidence(*candidate.scene_);
        if (candidate.summary_.meshes == 0 || candidate.summary_.objects == 0)
        {
            candidate.error_ = "Model conversion produced no mesh and object content.";
            candidate.scene_.reset();
        }
        return candidate;
    }
}
