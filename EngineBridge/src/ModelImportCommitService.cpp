#include "renegade/bridge/ModelImportCommitService.h"

#include "renegade/bridge/AssetCatalogueService.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/IdentityService.h"

#include <WickedEngine.h>
#include "renegade/bridge/ModelAnimationPreviewService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include <json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <vector>

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        bool Within(const fs::path& candidate, const fs::path& root)
        {
            return std::mismatch(root.begin(), root.end(),
                candidate.begin(), candidate.end()).first == root.end();
        }

        bool ValidAssetName(const std::string& name)
        {
            if (name.empty() || name.size() > 80 ||
                name.front() == ' ' || name.back() == ' ')
                return false;
            if (!std::all_of(name.begin(), name.end(), [](unsigned char c)
                { return std::isalnum(c) || c == '_' || c == '-' || c == ' '; }))
                return false;
            std::string upper = name;
            std::transform(upper.begin(), upper.end(), upper.begin(),
                [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            constexpr std::array<const char*, 4> reserved = {"CON", "PRN", "AUX", "NUL"};
            if (std::find_if(reserved.begin(), reserved.end(),
                    [&upper](const char* word) { return upper == word; }) != reserved.end())
                return false;
            return !(upper.size() == 4 &&
                ((upper.rfind("COM", 0) == 0) || (upper.rfind("LPT", 0) == 0)) &&
                upper[3] >= '1' && upper[3] <= '9');
        }

        bool ReadBytes(const fs::path& path, std::vector<std::uint8_t>& bytes)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream) return false;
            stream.seekg(0, std::ios::end);
            const auto length = stream.tellg();
            if (length <= 0 ||
                static_cast<std::uintmax_t>(length) >
                    static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
                return false;
            stream.seekg(0, std::ios::beg);
            bytes.resize(static_cast<std::size_t>(length));
            stream.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
            return static_cast<bool>(stream);
        }

        std::uint64_t Fingerprint(const std::vector<std::uint8_t>& bytes)
        {
            std::uint64_t hash = 1469598103934665603ull;
            for (const std::uint8_t value : bytes)
            {
                hash ^= value;
                hash *= 1099511628211ull;
            }
            return hash;
        }

        std::string Hash(const std::vector<std::uint8_t>& bytes)
        {
            std::ostringstream stream;
            stream << "fnv1a64:" << std::hex << std::setfill('0')
                   << std::setw(16) << Fingerprint(bytes);
            return stream.str();
        }

        std::uint32_t U32(const std::vector<std::uint8_t>& bytes,
            std::size_t offset)
        {
            return static_cast<std::uint32_t>(bytes[offset]) |
                (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
                (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
                (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
        }

        bool HasExternalUri(const nlohmann::json& value)
        {
            if (value.is_object())
            {
                for (auto it = value.begin(); it != value.end(); ++it)
                {
                    if (it.key() == "uri" && it.value().is_string())
                    {
                        const std::string uri = it.value().get<std::string>();
                        if (uri.rfind("data:", 0) != 0) return true;
                    }
                    if (HasExternalUri(it.value())) return true;
                }
            }
            else if (value.is_array())
            {
                for (const auto& child : value)
                    if (HasExternalUri(child)) return true;
            }
            return false;
        }

        bool SelfContainedGlb(const std::vector<std::uint8_t>& bytes,
            std::string& error)
        {
            if (bytes.size() < 20 || U32(bytes, 0) != 0x46546c67u ||
                U32(bytes, 4) != 2u || U32(bytes, 8) != bytes.size() ||
                U32(bytes, 16) != 0x4e4f534au ||
                static_cast<std::uint64_t>(U32(bytes, 12)) > bytes.size() - 20)
            {
                error = "GLB header or JSON chunk is malformed.";
                return false;
            }
            try
            {
                const std::string jsonText(bytes.begin() + 20,
                    bytes.begin() + 20 + U32(bytes, 12));
                const auto document = nlohmann::json::parse(jsonText);
                if (!document.is_object() || HasExternalUri(document))
                {
                    error = "GLB references an external file; this gate requires embedded data.";
                    return false;
                }
            }
            catch (const nlohmann::json::exception&)
            {
                error = "GLB JSON chunk could not be parsed.";
                return false;
            }
            return true;
        }

        bool SerializeScene(wi::scene::Scene& scene,
            const fs::path& temporary, std::vector<std::uint8_t>& bytes,
            std::string& error)
        {
            struct Cleanup
            {
                fs::path path;
                ~Cleanup() { std::error_code ignored; fs::remove(path, ignored); }
            } cleanup{temporary};
            wi::Archive archive(temporary.generic_u8string(), false, false);
            if (!archive.IsOpen())
            {
                error = "Could not create isolated WISCENE archive.";
                return false;
            }
            // A reusable payload must carry its resource bytes independently
            // of the source bundle and of the user's current save preference.
            struct RestoreResourceMode
            {
                wi::resourcemanager::Mode previous = wi::resourcemanager::GetMode();
                ~RestoreResourceMode() { wi::resourcemanager::SetMode(previous); }
            } resourceMode;
            wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
            scene.Serialize(archive);
            const bool saved = archive.SaveFile(temporary.generic_u8string());
            archive = wi::Archive(); // disarm Wicked's second destructor save
            if (!saved || !ReadBytes(temporary, bytes))
            {
                error = "Could not save or read isolated WISCENE payload.";
                return false;
            }
            wi::Archive check(temporary.generic_u8string(), true, false);
            if (!check.IsOpen())
            {
                error = "Serialized WISCENE payload could not reopen.";
                return false;
            }
            wi::scene::Scene reopened;
            reopened.Serialize(check);
            if (check.GetPos() != check.GetSize() ||
                !(ImportService::Summarize(scene) == ImportService::Summarize(reopened)))
            {
                error = "WISCENE structure changed during serialization.";
                return false;
            }
            return true;
        }

        ProjectDocumentWrite ExactWrite(const fs::path& path,
            std::vector<std::uint8_t> bytes)
        {
            ProjectDocumentWrite write;
            write.destinationPath = path.generic_u8string();
            write.content = std::move(bytes);
            const auto expected = write.content;
            write.validator = [expected](const std::string& stagedPath,
                std::string& error)
            {
                std::vector<std::uint8_t> staged;
                if (!ReadBytes(fs::u8path(stagedPath), staged) || staged != expected)
                {
                    error = "Staged model import bytes changed before commit.";
                    return false;
                }
                return true;
            };
            return write;
        }

        ProjectDocumentWrite TextWrite(const fs::path& path,
            const std::string& text)
        {
            return ExactWrite(path,
                std::vector<std::uint8_t>(text.begin(), text.end()));
        }
    }

    ModelImportCommitResult ModelImportCommitService::CommitGlb(
        const ModelImportCommitRequest& request,
        ModelImportCandidate& candidate) const
    {
        if (candidate.SourceFormat() == ModelSourceFormat::Glb)
            return CommitStaticModel(request, candidate);
        ModelImportCommitResult result;
        result.error = "The GLB compatibility entry point accepts GLB files only.";
        return result;
    }

    ModelImportCommitResult ModelImportCommitService::CommitStaticModel(
        const ModelImportCommitRequest& request, ModelImportCandidate& candidate) const
    {
        if (!request.characterAsset) return CommitModel(request, candidate);
        ModelImportCommitResult result;
        result.error = "The static compatibility entry point cannot create Character assets.";
        return result;
    }

    ModelImportCommitResult ModelImportCommitService::CommitModel(
        const ModelImportCommitRequest& request, ModelImportCandidate& candidate) const
    {
        ModelImportCommitResult result;
        if (!candidate.IsReady() || candidate.PeekMutableScene() == nullptr ||
            !IsValidStableId(request.projectId))
        {
            result.error = "Model candidate or project identity is not ready.";
            return result;
        }
        if (!ValidAssetName(request.assetName))
        {
            result.error = "Asset name must use letters, numbers, spaces, _ or - and be 1-80 characters.";
            return result;
        }
        // Static compatibility requests must not silently accept a rig or clips.
        // Character requests use the separately validated native rig round-trip.
        if (!request.characterAsset && (candidate.Evidence().HasRigOrAnimationPayload() ||
            candidate.Summary().animations != 0))
        {
            result.error = "This model importer accepts static GLB or FBX content only.";
            return result;
        }

        if (request.characterAsset && (candidate.Evidence().skinnedMeshes == 0 ||
            candidate.Evidence().armatureBones == 0))
        {
            result.error = "Character import requires a skinned mesh and skeleton.";
            return result;
        }

        std::error_code ec;
        const fs::path root = fs::weakly_canonical(
            fs::absolute(fs::u8path(request.projectRoot), ec), ec);
        if (ec || !fs::is_directory(root, ec) || ec)
        {
            result.error = "Active project root is unavailable.";
            return result;
        }
        const fs::path sourcePath = fs::weakly_canonical(
            fs::u8path(candidate.SourcePath()), ec);
        std::vector<std::uint8_t> sourceBytes;
        if (ec || !ReadBytes(sourcePath, sourceBytes) ||
            sourceBytes.size() != candidate.SourceBytes() ||
            Fingerprint(sourceBytes) != candidate.SourceFingerprint())
        {
            result.error = "Selected model changed after conversion; choose it again.";
            return result;
        }
        const bool isFbx = candidate.SourceFormat() == ModelSourceFormat::Fbx;
        if (!isFbx && candidate.SourceFormat() != ModelSourceFormat::Glb)
        {
            result.error = "Unsupported model candidate source format.";
            return result;
        }
        if (!isFbx && !SelfContainedGlb(sourceBytes, result.error)) return result;
        for (const auto& dependency : candidate.Dependencies())
        {
            std::vector<std::uint8_t> current;
            if (!dependency.sourcePath.empty() &&
                (!ReadBytes(fs::u8path(dependency.sourcePath), current) || current != dependency.bytes))
            {
                result.error = "FBX texture changed or disappeared after preview: " +
                    fs::u8path(dependency.sourcePath).filename().generic_u8string();
                return result;
            }
        }

        if (!request.thumbnailPng.empty())
        {
            constexpr std::array<std::uint8_t, 8> signature =
                {137, 80, 78, 71, 13, 10, 26, 10};
            const auto& png = request.thumbnailPng;
            if (png.size() < 33 || png.size() > 4 * 1024 * 1024 ||
                !std::equal(signature.begin(), signature.end(), png.begin()) ||
                png[12] != 'I' || png[13] != 'H' || png[14] != 'D' || png[15] != 'R' ||
                png[16] != 0 || png[17] != 0 || png[18] != 2 || png[19] != 0 ||
                png[20] != 0 || png[21] != 0 || png[22] != 1 || png[23] != 64)
            {
                result.error = "Rendered thumbnail must be a bounded 512 by 320 PNG.";
                return result;
            }
            const auto thumbnail = wi::resourcemanager::Load(
                "model-import-thumbnail-" + GenerateStableId() + ".png",
                wi::resourcemanager::Flags::NONE, request.thumbnailPng.data(),
                request.thumbnailPng.size());
            if (!thumbnail.IsValid() || !thumbnail.GetTexture().IsValid() ||
                thumbnail.GetTexture().GetDesc().width != 512 ||
                thumbnail.GetTexture().GetDesc().height != 320)
            {
                result.error = "Rendered thumbnail must be a valid 512 by 320 PNG.";
                return result;
            }
        }

        const fs::path sourceFolder = root / "SourceAssets" / "Models";
        const fs::path assetFolder = root / "Content" / "Models";
        const fs::path workFolder = root / "Intermediate" / "Imports";
        for (const auto& folder : {sourceFolder, assetFolder, workFolder})
        {
            fs::create_directories(folder, ec);
            if (ec || !Within(fs::weakly_canonical(folder, ec), root) || ec)
            {
                result.error = "Project model folder is unavailable or resolves outside the project.";
                return result;
            }
        }
        const fs::path sourceBundle = isFbx ? sourceFolder / fs::u8path(request.assetName) : sourceFolder;
        if (isFbx && fs::exists(sourceBundle, ec) && !fs::is_empty(sourceBundle, ec))
        {
            result.error = "Model source bundle already exists; choose another asset name.";
            return result;
        }
        const fs::path retainedSource = isFbx
            ? sourceBundle / sourcePath.filename()
            : sourceFolder / fs::u8path(request.assetName + ".glb");
        std::vector<fs::path> dependencyPaths;
        struct EmptyBundleCleanup
        {
            fs::path bundle;
            std::vector<fs::path>& paths;
            ~EmptyBundleCleanup()
            {
                if (bundle.empty()) return;
                // Failed transactions may leave empty generated directories.
                // Never remove files or non-empty recovery directories.
                std::error_code ignored;
                for (auto it = paths.rbegin(); it != paths.rend(); ++it)
                    for (auto parent = it->parent_path(); parent != bundle && Within(parent, bundle); parent = parent.parent_path())
                        fs::remove(parent, ignored);
                fs::remove(bundle, ignored);
            }
        } cleanup{isFbx ? sourceBundle : fs::path{}, dependencyPaths};
        for (const auto& dependency : candidate.Dependencies())
        {
            const fs::path relative = fs::u8path(dependency.retainedRelativePath);
            const fs::path destination = (sourceBundle / relative).lexically_normal();
            if (relative.empty() || relative.is_absolute() ||
                !Within(destination, sourceBundle) || destination == retainedSource ||
                fs::exists(destination, ec) || ec)
            {
                result.error = "FBX retained texture destination is invalid or already occupied.";
                return result;
            }
            dependencyPaths.push_back(destination);
        }
        if (isFbx)
        {
            fs::create_directories(sourceBundle, ec);
            if (ec || !Within(fs::weakly_canonical(sourceBundle, ec), root) || ec)
            {
                result.error = "Could not prepare retained FBX source bundle.";
                return result;
            }
        }
        const fs::path assetPath = assetFolder /
            fs::u8path(request.assetName + ".rasset");
        result.sourceProjectRelativePath = retainedSource.lexically_relative(root)
            .generic_u8string();
        result.assetProjectRelativePath = assetPath.lexically_relative(root)
            .generic_u8string();
        const fs::path projectionPath = root / fs::u8path(
            ResolveReusableModelManagedProjectionPath(
                result.assetProjectRelativePath));
        const fs::path thumbnailPath = assetFolder /
            fs::u8path(request.assetName + ".thumbnail.png");
        for (const auto& path : {retainedSource, assetPath, projectionPath, thumbnailPath})
        {
            ec.clear();
            if (fs::exists(path, ec) || ec)
            {
                result.error = "Model destination already exists; choose another asset name.";
                return result;
            }
        }

        AssetRegistry registry;
        const fs::path registryPath = root / AssetRegistryDocumentName;
        ec.clear();
        if (fs::exists(registryPath, ec) && !ec)
        {
            if (!ReadAssetRegistry(root.generic_u8string(), request.projectId,
                    registry, result.error)) return result;
        }
        else if (ec)
        {
            result.error = "Could not inspect the project asset registry.";
            return result;
        }
        else registry.projectId = request.projectId;

        for (const auto& record : registry.records)
        {
            if (record.projectRelativePath == result.assetProjectRelativePath ||
                record.projectRelativePath == result.sourceProjectRelativePath)
            {
                result.error = "Model destination already has a stable registry identity.";
                return result;
            }
        }
        for (const auto& missing : registry.missingAssets)
        {
            if (missing.lastKnownPath == result.assetProjectRelativePath ||
                missing.lastKnownPath == result.sourceProjectRelativePath)
            {
                result.error = "Model destination has a recovery tombstone; recover it first.";
                return result;
            }
        }
        const auto idUnused = [&registry](const StableId& id)
        {
            if (!IsValidStableId(id)) return false;
            for (const auto& record : registry.records)
                if (record.assetId == id) return false;
            for (const auto& missing : registry.missingAssets)
                if (missing.assetId == id) return false;
            return true;
        };
        result.sourceAssetId = GenerateStableId();
        result.assetId = GenerateStableId();
        if (!idUnused(result.sourceAssetId) || !idUnused(result.assetId) ||
            result.sourceAssetId == result.assetId)
        {
            result.error = "Could not allocate unique model asset IDs.";
            return result;
        }

        std::vector<std::uint8_t> payload;
        wi::scene::Scene commitScene;
        try
        {
            // Relocation happens on a clone; failed commits leave the candidate
            // and preview retryable with their original resource identities.
            wi::Archive clone;
            candidate.PeekMutableScene()->Serialize(clone);
            clone.SetReadModeAndResetPos(true);
            commitScene.Serialize(clone);
            PauseImportedModelAnimations(commitScene);
            if (request.characterAsset && commitScene.transforms.GetCount() != 0)
            {
                const auto rootEntity = commitScene.transforms.GetEntity(0);
                auto* metadata = commitScene.metadatas.GetComponent(rootEntity);
                if (!metadata) metadata = &commitScene.metadatas.Create(rootEntity);
                metadata->bool_values.set(ModelImportStartsPausedMetadataKey, true);
            }
            for (size_t i = 0; i < commitScene.materials.GetCount(); ++i)
            {
                for (auto& texture : commitScene.materials[i].textures)
                {
                    for (size_t d = 0; d < candidate.Dependencies().size(); ++d)
                    {
                        const auto& dependency = candidate.Dependencies()[d];
                        if (texture.name != dependency.previewResourceName) continue;
                        texture.name = dependencyPaths[d].generic_u8string();
                        texture.resource = wi::resourcemanager::Load(texture.name,
                            wi::resourcemanager::Flags::IMPORT_RETAIN_FILEDATA,
                            dependency.bytes.data(), dependency.bytes.size());
                        if (!texture.resource.IsValid() || !texture.resource.GetTexture().IsValid())
                            throw std::runtime_error("Retained FBX texture could not decode.");
                    }
                }
                commitScene.materials[i].SetDirty();
            }
            if (!SerializeScene(commitScene,
                    (isFbx ? sourceBundle : workFolder) /
                        fs::u8path(".renegade-" + result.assetId + ".import.wiscene"),
                    payload, result.error)) return result;
        }
        catch (const std::exception& exception)
        {
            result.error = std::string("Model scene serialization failed: ") + exception.what();
            return result;
        }
        catch (...)
        {
            result.error = "Model scene serialization failed.";
            return result;
        }

        ModelDerivedMetadata metadataValue;
        metadataValue.known = true;
        metadataValue.skinned = candidate.Evidence().skinnedMeshes != 0;
        metadataValue.animated = candidate.Summary().animations != 0;
        const auto count = [&result](std::size_t value, std::uint32_t& output)
        {
            if (value > (std::numeric_limits<std::uint32_t>::max)())
            {
                result.error = "Model metadata count exceeds supported range.";
                return false;
            }
            output = static_cast<std::uint32_t>(value);
            return true;
        };
        if (!count(candidate.Summary().meshes, metadataValue.meshCount) ||
            !count(candidate.Summary().materials, metadataValue.materialCount))
            return result;
        if (!count(candidate.Summary().animations, metadataValue.animationClipCount) ||
            !count(candidate.Evidence().animationChannels, metadataValue.animationChannelCount) ||
            !count(candidate.PeekScene()->armatures.GetCount(), metadataValue.armatureCount) ||
            !count(candidate.Evidence().armatureBones, metadataValue.boneCount)) return result;
        std::size_t morphCount = 0;
        for (std::size_t i = 0; i < candidate.PeekScene()->meshes.GetCount(); ++i)
            morphCount += candidate.PeekScene()->meshes[i].morph_targets.size();
        if (!count(morphCount, metadataValue.morphTargetCount)) return result;

        nlohmann::json recipe = {{"options", nlohmann::json::object()},
            {"source_format", isFbx ? "fbx" : "glb"}};
        if (request.characterAsset) recipe["options"]["asset_kind"] = "character";
        const std::string recipeJson = recipe.dump();
        ReusableModelAssetDocument document;
        document.manifest.projectId = request.projectId;
        document.manifest.assetId = result.assetId;
        document.manifest.sourceAssetId = result.sourceAssetId;
        document.manifest.sourceFormat = isFbx ? "fbx" : "glb";
        document.manifest.importer = isFbx ? "wicked.fbx" : "wicked.gltf";
        document.manifest.settingsJson = recipeJson;
        document.manifest.payloadHash = Hash(payload);
        document.payload = std::move(payload);
        std::vector<std::uint8_t> assetBytes;
        if (!SerializeReusableModelAssetDocument(document, assetBytes,
                result.error)) return result;

        ReusableModelManagedProjection projection;
        projection.projectId = request.projectId;
        projection.assetId = result.assetId;
        projection.sourceAssetId = result.sourceAssetId;
        projection.sourceProjectRelativePath = result.sourceProjectRelativePath;
        projection.assetProjectRelativePath = result.assetProjectRelativePath;
        projection.sourceFormat = document.manifest.sourceFormat;
        projection.importer = document.manifest.importer;
        projection.settingsJson = recipeJson;
        projection.payloadHash = document.manifest.payloadHash;
        projection.modelMetadata = metadataValue;
        if (!request.thumbnailPng.empty())
            projection.thumbnailProjectRelativePath =
                thumbnailPath.lexically_relative(root).generic_u8string();
        std::string projectionJson;
        if (!SerializeReusableModelManagedProjection(projection,
                projectionJson, result.error)) return result;

        const std::string sourceHash = Hash(sourceBytes);
        const std::string assetHash = Hash(assetBytes);
        AssetRecord sourceRecord;
        sourceRecord.assetId = result.sourceAssetId;
        sourceRecord.dependencyNodeId = "lp07.source:" + result.sourceAssetId;
        sourceRecord.projectRelativePath = result.sourceProjectRelativePath;
        sourceRecord.dependencyClass = DependencyClass::ImportedContent;
        sourceRecord.requirement = DependencyRequirement::EditorOnly;
        sourceRecord.provider = "lp07.source_asset";
        sourceRecord.contentHash = sourceHash;
        registry.records.push_back(std::move(sourceRecord));
        for (size_t d = 0; d < dependencyPaths.size(); ++d)
        {
            AssetRecord textureRecord;
            textureRecord.assetId = GenerateStableId();
            if (!idUnused(textureRecord.assetId))
            {
                result.error = "Could not allocate unique retained texture identity.";
                return result;
            }
            textureRecord.dependencyNodeId = "lp07.source:" + textureRecord.assetId;
            textureRecord.projectRelativePath = dependencyPaths[d].lexically_relative(root).generic_u8string();
            textureRecord.dependencyClass = DependencyClass::ImportedContent;
            textureRecord.requirement = DependencyRequirement::EditorOnly;
            textureRecord.provider = "lp07.source_asset";
            textureRecord.contentHash = Hash(candidate.Dependencies()[d].bytes);
            registry.records.push_back(std::move(textureRecord));
        }
        AssetRecord product;
        product.assetId = result.assetId;
        product.dependencyNodeId = "lp07.rasset:" + result.assetId;
        product.projectRelativePath = result.assetProjectRelativePath;
        product.dependencyClass = DependencyClass::ImportedContent;
        product.requirement = DependencyRequirement::Required;
        product.provider = "lp07.rasset";
        product.contentHash = assetHash;
        registry.records.push_back(std::move(product));
        ImportedProductRecord provenance;
        provenance.sourceAssetId = result.sourceAssetId;
        provenance.productAssetId = result.assetId;
        provenance.importer = document.manifest.importer;
        provenance.settingsSchema = document.manifest.settingsSchema;
        provenance.settingsJson = recipeJson;
        provenance.sourceContentHashAtImport = sourceHash;
        provenance.productContentHashAtImport = assetHash;
        auto associations = registry.importedProducts;
        associations.push_back(std::move(provenance));
        if (!SetImportedProductRecords(registry, std::move(associations),
                result.error)) return result;

        AssetCatalogueMetadataDocument metadata;
        if (!ReadAssetCatalogueMetadata(root.generic_u8string(),
                request.projectId, metadata, result.error) ||
            !SetAssetModelDerivedMetadata(metadata, result.assetId,
                metadataValue, result.error)) return result;
        std::string registryJson, metadataJson;
        if (!SerializeAssetRegistry(registry, registryJson, result.error) ||
            !SerializeAssetCatalogueMetadata(metadata, metadataJson,
                result.error)) return result;

        std::vector<ProjectDocumentWrite> writes;
        writes.push_back(ExactWrite(retainedSource, std::move(sourceBytes)));
        for (size_t d = 0; d < dependencyPaths.size(); ++d)
            writes.push_back(ExactWrite(dependencyPaths[d], candidate.Dependencies()[d].bytes));
        writes.push_back(ExactWrite(assetPath, std::move(assetBytes)));
        writes.push_back(TextWrite(projectionPath, projectionJson));
        if (!request.thumbnailPng.empty())
            writes.push_back(ExactWrite(thumbnailPath, request.thumbnailPng));
        writes.push_back(TextWrite(registryPath, registryJson));
        writes.push_back(TextWrite(root / AssetCatalogueMetadataDocumentName,
            metadataJson));
        ProjectDocumentTransactionOptions options;
        options.journalDirectory =
            (root / "Intermediate" / "Transactions").generic_u8string();
        options.allowedRoot = root.generic_u8string();
        result.transaction = ProjectDocumentTransaction().Execute(
            std::move(writes), std::move(options));
        result.committed = result.transaction.committed;
        if (!result.transaction.success || !result.committed)
        {
            result.error = "Model asset transaction failed: " +
                result.transaction.message;
            return result;
        }

        ReusableModelAssetDocument reopened;
        if (!ReadReusableModelAssetDocument(assetPath.generic_u8string(),
                reopened, result.error) ||
            reopened.manifest.assetId != result.assetId ||
            reopened.manifest.projectId != request.projectId)
        {
            if (result.error.empty()) result.error = "Committed model identity changed on reopen.";
            return result;
        }
        AssetCatalogue catalogue;
        if (!CreatorAssetWorkflowService().BuildCatalogueSnapshot(
                root.generic_u8string(), request.projectId,
                catalogue, result.error)) return result;
        const auto visible = std::find_if(catalogue.entries.begin(),
            catalogue.entries.end(), [&result](const AssetCatalogueEntry& entry)
            {
                return entry.registered && entry.assetId == result.assetId &&
                    entry.projectRelativePath == result.assetProjectRelativePath &&
                    entry.state == AssetCatalogueState::Current;
            });
        if (visible == catalogue.entries.end())
        {
            result.error = "Committed model is absent from the current Project Assets catalogue.";
            return result;
        }
        ReusableModelPlacementRequest placement;
        placement.projectRoot = root.generic_u8string();
        placement.projectId = request.projectId;
        placement.assetId = result.assetId;
        const auto prepared = ReusableAssetService().PrepareModelAssetPlacement(placement);
        if (!prepared.IsReady())
        {
            result.error = "Committed model cannot reopen for placement: " +
                prepared.Result().error;
            return result;
        }
        result.succeeded = true;
        return result;
    }
}
