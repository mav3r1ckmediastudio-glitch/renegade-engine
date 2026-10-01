#include "renegade/bridge/ModelImportCandidateService.h"
#include "renegade/bridge/ModelImporterFailureAdapter.h"
#include "renegade/bridge/IdentityService.h"
#include <ModelImporter.h>
#define UFBX_REAL_TYPE float
#include <ufbx.h>
#include <algorithm>
#include <filesystem>
#include <exception>
#include <fstream>
#include <memory>
#include <set>
#include <stdexcept>

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
        std::string& error)
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
            fs::path texturePath = fs::u8path(dependency.referenceName);
            if (texturePath.filename().empty())
            {
                error = "FBX contains an unnamed texture; give embedded images filenames before exporting.";
                return false;
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
                    error = "FBX texture is missing or unreadable: " +
                        fs::u8path(dependency.referenceName).filename().generic_u8string();
                    return false;
                }
                if (!Within(texturePath, source.parent_path()))
                {
                    error = "FBX texture is outside the source folder. Place the FBX and its textures in one folder tree and export relative references.";
                    return false;
                }
                dependency.sourcePath = texturePath.generic_u8string();
                dependency.retainedRelativePath = texturePath.lexically_relative(source.parent_path()).generic_u8string();
            }
            if (!destinations.insert(dependency.retainedRelativePath).second)
            {
                error = "FBX texture destinations are ambiguous.";
                return false;
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
    ModelImportCandidate ModelImportCandidateService::PrepareModel(const std::string& sourcePath) const
    {
        return PrepareStaticModel(sourcePath);
    }
    ModelImportCandidate ModelImportCandidateService::PrepareGlb(const std::string& sourcePath) const
    {
        if (ImportService::ClassifyModelSourceFormat(sourcePath) == ModelSourceFormat::Glb)
            return PrepareStaticModel(sourcePath);
        ModelImportCandidate candidate;
        candidate.error_ = "The GLB compatibility entry point accepts GLB files only.";
        return candidate;
    }
    ModelImportCandidate ModelImportCandidateService::PrepareStaticModel(const std::string& sourcePath) const
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
        if (candidate.sourceFormat_ != ModelSourceFormat::Glb && candidate.sourceFormat_ != ModelSourceFormat::Fbx)
        {
            candidate.error_ = "This model importer accepts static GLB and FBX files only.";
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
            !SnapshotFbxTextures(source, candidate.dependencies_, candidate.error_)) return candidate;

        candidate.scene_ = wi::allocator::make_shared_single<wi::scene::Scene>();
        ClearWickedModelImporterFailureDiagnostic();
        try
        {
            if (candidate.sourceFormat_ == ModelSourceFormat::Fbx)
                ImportModel_FBX(candidate.sourcePath_, *candidate.scene_);
            else ImportModel_GLTF(candidate.sourcePath_, *candidate.scene_);
            // Unique cache keys force the preview to use the exact snapshotted
            // bytes, even when a previous import cached the original filename.
            for (size_t i = 0; i < candidate.scene_->materials.GetCount(); ++i)
            {
                for (auto& texture : candidate.scene_->materials[i].textures)
                {
                    if (candidate.sourceFormat_ != ModelSourceFormat::Fbx || texture.name.empty()) continue;
                    const auto found = std::find_if(candidate.dependencies_.begin(), candidate.dependencies_.end(),
                        [&texture](const ModelImportDependency& d) { return d.referenceName == texture.name; });
                    if (found == candidate.dependencies_.end())
                        throw std::runtime_error("FBX converter texture was not declared by dependency inspection.");
                    texture.resource = wi::resourcemanager::Load(found->previewResourceName,
                        wi::resourcemanager::Flags::IMPORT_RETAIN_FILEDATA, found->bytes.data(), found->bytes.size());
                    if (!texture.resource.IsValid() || !texture.resource.GetTexture().IsValid())
                        throw std::runtime_error("FBX texture could not decode: " + fs::u8path(found->referenceName).filename().generic_u8string());
                    texture.name = found->previewResourceName;
                }
                candidate.scene_->materials[i].SetDirty();
            }
        }
        catch (const std::exception& exception) { candidate.error_ = std::string("Model conversion failed: ") + exception.what(); }
        catch (...) { candidate.error_ = "Model conversion failed with an unknown error."; }
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
