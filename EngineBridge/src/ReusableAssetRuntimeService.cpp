#include "renegade/bridge/ReusableAssetRuntimeService.h"

#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/ReusableAssetService.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <utility>

#include "json.hpp"

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        struct PackagedAssetEntry
        {
            StableId assetId;
            std::string packagedPath;
            std::string sourceHash;
        };

        struct PreparedAssetPayload
        {
            StableId assetId;
            std::string packagedPath;
            std::string payloadHash;
            wi::allocator::shared_ptr<wi::scene::Scene> scene;
        };

        bool IsWithin(const fs::path& root, const fs::path& candidate)
        {
            auto rootPart = root.begin();
            auto candidatePart = candidate.begin();
            for (; rootPart != root.end(); ++rootPart, ++candidatePart)
            {
                if (candidatePart == candidate.end() || *rootPart != *candidatePart)
                    return false;
            }
            return true;
        }

        bool IsSafeRelativePackagePath(const std::string& value)
        {
            if (value.empty() || value.find('\\') != std::string::npos)
                return false;
            const fs::path path = fs::u8path(value);
            if (path.empty() || path.is_absolute() || path.has_root_name() ||
                path.generic_u8string() != value ||
                path.lexically_normal().generic_u8string() != value)
            {
                return false;
            }
            return std::none_of(path.begin(), path.end(),
                [](const fs::path& part)
                {
                    return part.empty() || part == "." || part == "..";
                });
        }

        bool IsRegularNonSymlink(const fs::path& path)
        {
            std::error_code ec;
            const fs::file_status link = fs::symlink_status(path, ec);
            if (ec || fs::is_symlink(link))
                return false;
            return fs::is_regular_file(path, ec) && !ec;
        }

        bool ReadTextFile(
            const fs::path& path,
            std::string& text,
            std::string& error)
        {
            text.clear();
            if (!IsRegularNonSymlink(path))
            {
                error = "Packaged reusable asset content manifest is missing or symlinked.";
                return false;
            }
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                error = "Could not read packaged reusable asset content manifest.";
                return false;
            }
            text.assign(
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());
            if (!stream && !stream.eof())
            {
                text.clear();
                error = "Could not read complete packaged reusable asset content manifest.";
                return false;
            }
            error.clear();
            return true;
        }

        bool ReadPackageAssetMap(
            const fs::path& packageRoot,
            const StableId& projectId,
            std::map<StableId, PackagedAssetEntry>& entries,
            std::string& error)
        {
            entries.clear();

            std::string text;
            if (!ReadTextFile(
                    packageRoot / "GameData" / "content-manifest.json",
                    text,
                    error))
            {
                return false;
            }

            const nlohmann::json manifest = nlohmann::json::parse(
                text, nullptr, false);
            if (manifest.is_discarded() || !manifest.is_object() ||
                manifest.value("format", std::string{}) !=
                    "renegade-content-manifest" ||
                manifest.value("schema_version", 0) != 1 ||
                manifest.value("project_id", std::string{}) != projectId ||
                !manifest.contains("files") || !manifest["files"].is_array())
            {
                error =
                    "Packaged reusable asset content manifest has invalid identity or schema.";
                return false;
            }

            for (const auto& item : manifest["files"])
            {
                if (!item.is_object())
                {
                    error = "Packaged reusable asset content manifest contains a non-object file record.";
                    return false;
                }
                const std::string assetId =
                    item.value("asset_id", std::string{});
                const std::string path = item.value("path", std::string{});
                if (!IsValidStableId(assetId) ||
                    !IsSafeRelativePackagePath(path) ||
                    path.rfind("GameData/", 0) != 0)
                {
                    error =
                        "Packaged reusable asset content manifest contains an invalid asset/path record.";
                    return false;
                }

                PackagedAssetEntry entry;
                entry.assetId = assetId;
                entry.packagedPath = path;
                entry.sourceHash = item.value("source_hash", std::string{});
                if (!entries.emplace(assetId, std::move(entry)).second)
                {
                    error =
                        "Packaged reusable asset content manifest contains duplicate stable asset IDs.";
                    return false;
                }
            }

            error.clear();
            return true;
        }

        bool PreparePayload(
            const fs::path& packageRoot,
            const StableId& projectId,
            const PackagedAssetEntry& entry,
            PreparedAssetPayload& prepared,
            std::string& error)
        {
            prepared = {};

            if (fs::u8path(entry.packagedPath).extension() != ReusableAssetExtension)
            {
                error =
                    "Reusable asset instance stable ID does not map to a packaged .rasset product.";
                return false;
            }

            std::error_code ec;
            const fs::path assetPath = fs::weakly_canonical(
                packageRoot / fs::u8path(entry.packagedPath), ec);
            if (ec || !IsWithin(packageRoot, assetPath) ||
                !IsRegularNonSymlink(assetPath))
            {
                error =
                    "Packaged reusable asset product is missing, symlinked or outside the package root.";
                return false;
            }

            ReusableModelAssetDocument document;
            if (!ReadReusableModelAssetDocument(
                    assetPath.generic_u8string(), document, error))
            {
                error = "Packaged reusable asset product was rejected: " + error;
                return false;
            }
            if (document.manifest.projectId != projectId ||
                document.manifest.assetId != entry.assetId ||
                document.manifest.payloadFormat != ReusableModelPayloadFormat)
            {
                error =
                    "Packaged reusable asset manifest contradicts package/project stable identity.";
                return false;
            }

            auto payloadScene =
                wi::allocator::make_shared_single<wi::scene::Scene>();
            wi::Archive archive(
                document.payload.data(), document.payload.size());
            if (!archive.IsOpen())
            {
                error =
                    "Packaged reusable asset WISCENE payload could not be opened.";
                return false;
            }
            payloadScene->Serialize(archive);
            if (archive.GetPos() != archive.GetSize() ||
                payloadScene->transforms.GetCount() == 0)
            {
                error =
                    "Packaged reusable asset WISCENE payload is incomplete or contains no transform root.";
                return false;
            }

            prepared.assetId = entry.assetId;
            prepared.packagedPath = entry.packagedPath;
            prepared.payloadHash = document.manifest.payloadHash;
            prepared.scene = std::move(payloadScene);
            error.clear();
            return true;
        }

        bool NormalizeAndAttachReplacement(
            wi::scene::Scene& scene,
            const wi::ecs::Entity replacementRoot,
            const wi::ecs::Entity wrapper,
            std::string& error)
        {
            auto* transform = scene.transforms.GetComponent(replacementRoot);
            if (transform == nullptr)
            {
                error =
                    "Packaged reusable asset replacement root has no transform component.";
                return false;
            }
            transform->translation_local = XMFLOAT3(0.0f, 0.0f, 0.0f);
            transform->rotation_local = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            transform->scale_local = XMFLOAT3(1.0f, 1.0f, 1.0f);
            transform->SetDirty();

            auto* metadata = scene.metadatas.GetComponent(replacementRoot);
            if (metadata == nullptr)
                metadata = &scene.metadatas.Create(replacementRoot);
            metadata->bool_values.set(
                ReusableAssetPayloadRootMetadataKey, true);

            scene.Component_Attach(replacementRoot, wrapper, true);
            for (std::size_t index = 0; index < scene.animations.GetCount(); ++index)
            {
                const wi::ecs::Entity animationEntity =
                    scene.animations.GetEntity(index);
                if (animationEntity == replacementRoot ||
                    scene.Entity_IsDescendant(animationEntity, replacementRoot))
                {
                    scene.animations[index].Play();
                }
            }

            error.clear();
            return true;
        }
    }

    bool RefreshPackagedReusableAssetInstances(
        wi::scene::Scene& scene,
        const std::string& packageRootText,
        const StableId& projectId,
        PackagedReusableAssetRefreshResult& result,
        std::string& error)
    {
        result = {};
        error.clear();

        if (packageRootText.empty() || !IsValidStableId(projectId))
        {
            error =
                "Packaged reusable asset refresh requires a package root and valid project ID.";
            return false;
        }

        std::error_code ec;
        const fs::path packageRoot = fs::weakly_canonical(
            fs::u8path(packageRootText), ec);
        if (ec || !fs::is_directory(packageRoot, ec) || ec)
        {
            error = "Packaged reusable asset refresh package root is unavailable.";
            return false;
        }

        std::vector<ReusableAssetInstanceRecord> instances;
        if (!InspectReusableAssetInstances(scene, instances, error))
        {
            error = "Packaged reusable asset instance metadata was rejected: " + error;
            return false;
        }
        result.discoveredInstanceCount = instances.size();
        if (instances.empty())
            return true;

        std::map<StableId, PackagedAssetEntry> packageEntries;
        if (!ReadPackageAssetMap(
                packageRoot, projectId, packageEntries, error))
        {
            return false;
        }

        // Preparation phase: no target-scene mutation. Every distinct stable
        // asset used by the scene must resolve to a current packaged .rasset
        // and deserialize successfully before any old payload can be replaced.
        std::map<StableId, PreparedAssetPayload> preparedById;
        for (const auto& instance : instances)
        {
            if (preparedById.find(instance.assetId) != preparedById.end())
                continue;
            const auto entry = packageEntries.find(instance.assetId);
            if (entry == packageEntries.end())
            {
                error =
                    "Reusable asset instance stable ID is absent from the packaged content manifest: " +
                    instance.assetId;
                return false;
            }
            PreparedAssetPayload prepared;
            if (!PreparePayload(
                    packageRoot, projectId, entry->second, prepared, error))
            {
                return false;
            }
            preparedById.emplace(instance.assetId, std::move(prepared));
        }

        struct PendingReplacement
        {
            ReusableAssetInstanceRecord previous;
            wi::ecs::Entity replacementRoot = wi::ecs::INVALID_ENTITY;
            PackagedReusableAssetRefreshRecord evidence;
        };
        std::vector<PendingReplacement> replacements;
        replacements.reserve(instances.size());

        // Instantiate all replacements while retaining every old payload. If
        // any instantiation/attachment fails, remove only the new roots and
        // leave the scene's last-good authored payloads untouched.
        for (const auto& instance : instances)
        {
            auto prepared = preparedById.find(instance.assetId);
            if (prepared == preparedById.end() ||
                !prepared->second.scene.IsValid())
            {
                error = "Prepared packaged reusable asset payload disappeared before commit.";
                break;
            }

            const wi::ecs::Entity replacementRoot =
                scene.Instantiate(*prepared->second.scene, true);
            if (replacementRoot == wi::ecs::INVALID_ENTITY ||
                !NormalizeAndAttachReplacement(
                    scene, replacementRoot, instance.instanceRoot, error))
            {
                if (error.empty())
                    error = "Packaged reusable asset payload instantiation failed.";
                if (replacementRoot != wi::ecs::INVALID_ENTITY)
                    scene.Entity_Remove(replacementRoot);
                break;
            }

            PendingReplacement replacement;
            replacement.previous = instance;
            replacement.replacementRoot = replacementRoot;
            replacement.evidence.assetId = instance.assetId;
            replacement.evidence.packagedAssetPath =
                prepared->second.packagedPath;
            replacement.evidence.payloadHash =
                prepared->second.payloadHash;
            replacement.evidence.instanceRoot = instance.instanceRoot;
            replacement.evidence.payloadRoot = replacementRoot;
            replacements.push_back(std::move(replacement));
        }

        if (replacements.size() != instances.size())
        {
            for (const auto& replacement : replacements)
            {
                if (replacement.replacementRoot != wi::ecs::INVALID_ENTITY)
                    scene.Entity_Remove(replacement.replacementRoot);
            }
            if (error.empty())
                error = "Packaged reusable asset replacement did not prepare every scene instance.";
            return false;
        }

        // Commit point. Every replacement is live and attached; removing the
        // old child payloads cannot destroy creator-authored wrapper transform
        // or stable identity.
        for (const auto& replacement : replacements)
            scene.Entity_Remove(replacement.previous.payloadRoot);

        for (const auto& replacement : replacements)
            result.records.push_back(replacement.evidence);
        result.refreshedInstanceCount = result.records.size();
        error.clear();
        return true;
    }
}
