#include "renegade/bridge/CreatorAssetWorkflowService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr std::uint64_t FnvOffset = 1469598103934665603ull;
        constexpr std::uint64_t FnvPrime = 1099511628211ull;

        struct RefreshIdentity
        {
            StableId assetId;
            std::string nodeId;
            std::string lastKnownPath;
            DependencyClass dependencyClass = DependencyClass::Data;
            DependencyRequirement requirement = DependencyRequirement::Required;
            std::string applicability = "windows-x64";
            std::string provider;
            std::uint32_t providerVersion = 1;
            std::string contentHash;
            bool root = false;
            std::vector<StableId> dependencyAssetIds;
        };

        struct RefreshCandidate
        {
            std::string projectRelativePath;
            std::string contentHash;
        };

        bool IsWithin(const fs::path& candidate, const fs::path& root)
        {
            auto candidatePart = candidate.begin();
            for (auto rootPart = root.begin(); rootPart != root.end();
                ++rootPart, ++candidatePart)
            {
                if (candidatePart == candidate.end() || *candidatePart != *rootPart)
                    return false;
            }
            return true;
        }

        bool ResolveRoot(
            const std::string& projectRoot,
            fs::path& root,
            std::string& error)
        {
            root.clear();
            if (projectRoot.empty())
            {
                error = "Creator asset workflow requires a project root.";
                return false;
            }
            std::error_code ec;
            const fs::path absolute = fs::absolute(fs::u8path(projectRoot), ec);
            if (ec || absolute.empty())
            {
                error = "Could not resolve creator asset project root.";
                return false;
            }
            root = fs::weakly_canonical(absolute, ec);
            if (ec || root.empty() || !fs::is_directory(root, ec) || ec)
            {
                error = "Creator asset project root is unavailable.";
                root.clear();
                return false;
            }
            error.clear();
            return true;
        }

        std::string LowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](const unsigned char character)
                {
                    return character >= 'A' && character <= 'Z'
                        ? static_cast<char>(character + ('a' - 'A'))
                        : static_cast<char>(character);
                });
            return value;
        }

        std::string HashStream(std::istream& stream)
        {
            std::uint64_t hash = FnvOffset;
            char buffer[64 * 1024];
            while (stream)
            {
                stream.read(buffer, sizeof(buffer));
                const std::streamsize count = stream.gcount();
                for (std::streamsize index = 0; index < count; ++index)
                {
                    hash ^= static_cast<unsigned char>(buffer[index]);
                    hash *= FnvPrime;
                }
            }
            std::ostringstream text;
            text << "fnv1a64:" << std::hex << std::setfill('0')
                 << std::setw(16) << hash;
            return text.str();
        }

        bool HashFile(
            const fs::path& path,
            std::string& hash,
            std::string& error)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                error = "Could not read project asset while refreshing LC01: " +
                    path.generic_u8string();
                return false;
            }
            hash = HashStream(stream);
            if (!stream.eof() && stream.fail())
            {
                error = "Could not read complete project asset while refreshing LC01: " +
                    path.generic_u8string();
                return false;
            }
            error.clear();
            return true;
        }

        std::string TopLevelFolder(const std::string& projectRelativePath)
        {
            const fs::path path = fs::u8path(projectRelativePath);
            const auto first = path.begin();
            return first == path.end() ? std::string{} : first->generic_u8string();
        }

        bool ExistingRegistryOrEmpty(
            const fs::path& root,
            const StableId& projectId,
            AssetRegistry& registry,
            std::string& error)
        {
            std::string registryPath;
            if (!ResolveAssetRegistryDocumentPath(
                    root.generic_u8string(), registryPath, error))
                return false;

            std::error_code ec;
            if (!fs::exists(fs::u8path(registryPath), ec) && !ec)
            {
                registry = {};
                registry.projectId = projectId;
                registry.schemaVersion = AssetRegistry::CurrentSchemaVersion;
                error.clear();
                return true;
            }
            if (ec)
            {
                error = "Could not inspect the LC01 registry document: " + ec.message();
                return false;
            }
            return ReadAssetRegistry(root.generic_u8string(), projectId, registry, error);
        }

        DependencyNode NodeFromIdentity(
            const RefreshIdentity& identity,
            const std::string& path,
            const std::string& hash)
        {
            DependencyNode node;
            node.id = identity.nodeId;
            node.projectRelativePath = path;
            node.dependencyClass = identity.dependencyClass;
            node.requirement = identity.requirement;
            node.applicability = identity.applicability;
            node.provider = identity.provider;
            node.providerVersion = identity.providerVersion;
            node.contentHash = hash;
            return node;
        }

        bool SameRecoveryFacts(
            const RefreshIdentity& identity,
            const RefreshCandidate& candidate)
        {
            return identity.contentHash == candidate.contentHash &&
                TopLevelFolder(identity.lastKnownPath) ==
                    TopLevelFolder(candidate.projectRelativePath);
        }

        bool ScanRefreshCandidates(
            const fs::path& root,
            const std::set<std::string>& activePaths,
            std::vector<RefreshCandidate>& candidates,
            std::string& error)
        {
            candidates.clear();
            for (const char* folderName : {"Content", "SourceAssets"})
            {
                const fs::path folder = root / folderName;
                std::error_code ec;
                if (!fs::exists(folder, ec) && !ec)
                    continue;
                if (ec || !fs::is_directory(folder, ec) || ec)
                {
                    error = std::string("Could not scan creator asset folder '") +
                        folderName + "'.";
                    return false;
                }

                fs::recursive_directory_iterator iterator(
                    folder, fs::directory_options::skip_permission_denied, ec);
                const fs::recursive_directory_iterator end;
                for (; !ec && iterator != end; iterator.increment(ec))
                {
                    const fs::directory_entry& entry = *iterator;
                    std::error_code entryError;
                    if (entry.is_symlink(entryError) || entryError ||
                        !entry.is_regular_file(entryError) || entryError)
                        continue;
                    const fs::path canonical = fs::weakly_canonical(entry.path(), entryError);
                    if (entryError || !IsWithin(canonical, folder))
                        continue;
                    const std::string relative = canonical.lexically_relative(root)
                        .lexically_normal().generic_u8string();
                    if (relative.empty() || activePaths.find(relative) != activePaths.end())
                        continue;

                    RefreshCandidate candidate;
                    candidate.projectRelativePath = relative;
                    if (!HashFile(canonical, candidate.contentHash, error))
                        return false;
                    candidates.push_back(std::move(candidate));
                }
                if (ec)
                {
                    error = std::string("Could not complete creator asset folder scan '") +
                        folderName + "': " + ec.message();
                    return false;
                }
            }
            std::sort(candidates.begin(), candidates.end(),
                [](const RefreshCandidate& left, const RefreshCandidate& right)
                { return left.projectRelativePath < right.projectRelativePath; });
            error.clear();
            return true;
        }

        bool RefreshRegistryInternal(
            const fs::path& root,
            const StableId& projectId,
            AssetRegistryRefresh& refresh,
            std::string& error)
        {
            AssetRegistry existing;
            if (!ExistingRegistryOrEmpty(root, projectId, existing, error))
                return false;
            if (existing.records.empty() && existing.missingAssets.empty())
            {
                refresh = {};
                refresh.registry = existing;
                error.clear();
                return true;
            }

            std::vector<RefreshIdentity> identities;
            identities.reserve(existing.records.size() + existing.missingAssets.size());
            std::set<std::string> activePaths;
            std::map<StableId, std::string> nodeByAssetId;
            DependencyGraph graph;
            std::vector<std::size_t> unresolved;

            for (const auto& record : existing.records)
            {
                RefreshIdentity identity;
                identity.assetId = record.assetId;
                identity.nodeId = record.dependencyNodeId;
                identity.lastKnownPath = record.projectRelativePath;
                identity.dependencyClass = record.dependencyClass;
                identity.requirement = record.requirement;
                identity.applicability = record.applicability;
                identity.provider = record.provider;
                identity.providerVersion = record.providerVersion;
                identity.contentHash = record.contentHash;
                identity.root = record.root;
                identity.dependencyAssetIds = record.dependencyAssetIds;
                const std::size_t identityIndex = identities.size();
                identities.push_back(identity);

                std::error_code ec;
                const fs::path absolute = fs::weakly_canonical(
                    root / fs::u8path(record.projectRelativePath), ec);
                if (!ec && fs::is_regular_file(absolute, ec) && !ec &&
                    IsWithin(absolute, root))
                {
                    std::string currentHash;
                    if (!HashFile(absolute, currentHash, error))
                        return false;
                    graph.nodes.push_back(NodeFromIdentity(identity,
                        record.projectRelativePath, currentHash));
                    activePaths.insert(record.projectRelativePath);
                    nodeByAssetId.emplace(record.assetId, identity.nodeId);
                    if (record.root)
                        graph.rootIds.push_back(identity.nodeId);
                }
                else
                {
                    unresolved.push_back(identityIndex);
                }
            }

            for (const auto& missing : existing.missingAssets)
            {
                RefreshIdentity identity;
                identity.assetId = missing.assetId;
                identity.nodeId = "lc01.recovered:" + missing.assetId;
                identity.lastKnownPath = missing.lastKnownPath;
                identity.dependencyClass = missing.dependencyClass;
                identity.requirement = missing.requirement;
                identity.applicability = missing.applicability;
                identity.provider = missing.provider;
                identity.providerVersion = missing.providerVersion;
                identity.contentHash = missing.contentHash;
                unresolved.push_back(identities.size());
                identities.push_back(std::move(identity));
            }

            std::vector<RefreshCandidate> candidates;
            if (!unresolved.empty() &&
                !ScanRefreshCandidates(root, activePaths, candidates, error))
                return false;

            std::map<std::size_t, std::vector<std::size_t>> matchesByIdentity;
            std::map<std::size_t, std::size_t> identityCountByCandidate;
            for (const std::size_t identityIndex : unresolved)
            {
                for (std::size_t candidateIndex = 0;
                    candidateIndex < candidates.size(); ++candidateIndex)
                {
                    if (SameRecoveryFacts(
                            identities[identityIndex], candidates[candidateIndex]))
                    {
                        matchesByIdentity[identityIndex].push_back(candidateIndex);
                        ++identityCountByCandidate[candidateIndex];
                    }
                }
            }

            for (const std::size_t identityIndex : unresolved)
            {
                const auto matches = matchesByIdentity.find(identityIndex);
                if (matches == matchesByIdentity.end() || matches->second.size() != 1)
                    continue;
                const std::size_t candidateIndex = matches->second.front();
                if (identityCountByCandidate[candidateIndex] != 1)
                    continue;

                const auto& identity = identities[identityIndex];
                const auto& candidate = candidates[candidateIndex];
                graph.nodes.push_back(NodeFromIdentity(identity,
                    candidate.projectRelativePath, candidate.contentHash));
                nodeByAssetId.emplace(identity.assetId, identity.nodeId);
                if (identity.root)
                    graph.rootIds.push_back(identity.nodeId);
            }

            // Rebuild only previously authoritative graph edges whose two
            // stable endpoints are still present in this refresh graph.
            for (const auto& identity : identities)
            {
                const auto source = nodeByAssetId.find(identity.assetId);
                if (source == nodeByAssetId.end())
                    continue;
                for (const auto& dependencyId : identity.dependencyAssetIds)
                {
                    const auto target = nodeByAssetId.find(dependencyId);
                    if (target != nodeByAssetId.end())
                    {
                        graph.edges.push_back({
                            source->second,
                            target->second,
                            "lc01.registered_dependency",
                        });
                    }
                }
            }

            // No unknown filesystem item is admitted into graph.nodes above.
            // A defensive generator is still supplied because RefreshAssetRegistry
            // requires one; if recovery facts were ambiguous it must fail closed
            // rather than mint a new identity for an existing provenance endpoint.
            const AssetIdGenerator denyNewIdentity = []() -> StableId
            {
                return {};
            };
            if (!RefreshAssetRegistry(projectId, graph, &existing,
                    refresh, error, denyNewIdentity))
            {
                return false;
            }

            AssetRegistryPersistenceOptions persistence;
            persistence.transactionId = "lp07-gate5-browser-refresh-" + GenerateStableId();
            const auto written = WriteAssetRegistry(
                root.generic_u8string(), refresh.registry, std::move(persistence));
            if (!written.success || !written.committed)
            {
                error = written.message.empty()
                    ? "Could not persist creator Asset Browser LC01 refresh."
                    : written.message;
                refresh = {};
                return false;
            }
            error.clear();
            return true;
        }

        std::string SanitizeStem(std::string value)
        {
            for (char& character : value)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                if (!((byte >= 'a' && byte <= 'z') ||
                      (byte >= 'A' && byte <= 'Z') ||
                      (byte >= '0' && byte <= '9') ||
                      character == '-' || character == '_'))
                {
                    character = '_';
                }
            }
            while (!value.empty() && value.front() == '_') value.erase(value.begin());
            while (!value.empty() && value.back() == '_') value.pop_back();
            return value.empty() ? std::string("Model") : value;
        }

        bool CopyGltfExternalFiles(
            const fs::path& source,
            const fs::path& destinationDirectory,
            std::string& error)
        {
            const std::string extension = LowerAscii(source.extension().generic_u8string());
            if (extension != ".gltf")
            {
                error.clear();
                return true;
            }

            GltfDependencyDocument dependencies;
            const GltfDependencyReader reader = MakeGltfDependencyReader();
            if (!reader(source.generic_u8string(), dependencies, error))
                return false;

            const fs::path sourceDirectory = fs::weakly_canonical(source.parent_path());
            for (const auto& reference : dependencies.references)
            {
                const fs::path dependency = fs::weakly_canonical(
                    fs::u8path(reference.declaredPath));
                if (!fs::is_regular_file(dependency))
                {
                    error = "glTF external dependency is missing: " +
                        dependency.generic_u8string();
                    return false;
                }
                if (!IsWithin(dependency, sourceDirectory))
                {
                    error =
                        "Gate 5 source retention requires glTF external dependencies "
                        "to remain below the selected source folder: " +
                        dependency.generic_u8string();
                    return false;
                }
                const fs::path relative = dependency.lexically_relative(sourceDirectory);
                const fs::path destination = destinationDirectory / relative;
                std::error_code ec;
                fs::create_directories(destination.parent_path(), ec);
                if (ec)
                {
                    error = "Could not create retained glTF dependency folder: " +
                        ec.message();
                    return false;
                }
                fs::copy_file(dependency, destination,
                    fs::copy_options::overwrite_existing, ec);
                if (ec)
                {
                    error = "Could not retain glTF external dependency: " +
                        dependency.generic_u8string() + ": " + ec.message();
                    return false;
                }
            }
            error.clear();
            return true;
        }
    }

    bool CreatorAssetWorkflowService::RefreshRegistryFromDisk(
        const std::string& projectRoot,
        const StableId& projectId,
        AssetRegistry& registry,
        std::string& error) const
    {
        if (!IsValidStableId(projectId))
        {
            error = "Creator Asset Browser requires a valid project ID.";
            return false;
        }
        fs::path root;
        if (!ResolveRoot(projectRoot, root, error))
            return false;
        AssetRegistryRefresh refresh;
        if (!RefreshRegistryInternal(root, projectId, refresh, error))
            return false;
        registry = std::move(refresh.registry);
        return true;
    }

    bool CreatorAssetWorkflowService::BuildCatalogue(
        const std::string& projectRoot,
        const StableId& projectId,
        AssetCatalogue& catalogue,
        std::string& error) const
    {
        if (!IsValidStableId(projectId))
        {
            error = "Creator Asset Browser requires a valid project ID.";
            return false;
        }
        fs::path root;
        if (!ResolveRoot(projectRoot, root, error))
            return false;

        AssetRegistryRefresh refresh;
        if (!RefreshRegistryInternal(root, projectId, refresh, error))
            return false;

        AssetCatalogueMetadataDocument metadata;
        if (!ReadAssetCatalogueMetadata(
                root.generic_u8string(), projectId, metadata, error))
            return false;

        AssetCatalogueBuildOptions options;
        options.movedAssetIds = refresh.recoveredAssetIds;
        return BuildAssetCatalogue(root.generic_u8string(), projectId,
            refresh.registry, metadata, catalogue, error, std::move(options));
    }

    CreatorModelImportResult CreatorAssetWorkflowService::ImportModel(
        const std::string& projectRoot,
        const StableId& projectId,
        const std::string& externalSourcePath) const
    {
        CreatorModelImportResult result;
        if (!IsValidStableId(projectId))
        {
            result.error = "Creator model import requires a valid project ID.";
            return result;
        }
        fs::path root;
        if (!ResolveRoot(projectRoot, root, result.error))
            return result;

        std::error_code ec;
        const fs::path source = fs::weakly_canonical(
            fs::absolute(fs::u8path(externalSourcePath), ec), ec);
        if (ec || !fs::is_regular_file(source, ec) || ec)
        {
            result.error = "Selected model source is unavailable.";
            return result;
        }
        const ModelSourceFormat format = ImportService::ClassifyModelSourceFormat(
            source.generic_u8string());
        if (!ImportService::IsModelSourceFormatSupported(format))
        {
            result.error = "Selected model format is not enabled by LP07.";
            return result;
        }

        const fs::path contentModels = root / "Content" / "Models";
        const fs::path sourceModels = root / "SourceAssets" / "Models";
        fs::create_directories(contentModels, ec);
        if (!ec) fs::create_directories(sourceModels, ec);
        if (ec)
        {
            result.error = "Could not create project model folders: " + ec.message();
            return result;
        }

        const std::string baseStem = SanitizeStem(source.stem().generic_u8string());
        std::string candidateStem = baseStem;
        fs::path snapshotDirectory;
        fs::path assetPath;
        for (std::uint32_t suffix = 0; ; ++suffix)
        {
            candidateStem = suffix == 0
                ? baseStem : baseStem + "_" + std::to_string(suffix + 1);
            snapshotDirectory = sourceModels / fs::u8path(candidateStem);
            assetPath = contentModels / fs::u8path(candidateStem + ReusableAssetExtension);
            ec.clear();
            const bool snapshotExists = fs::exists(snapshotDirectory, ec);
            if (ec)
            {
                result.error = "Could not inspect retained-source destination.";
                return result;
            }
            const bool assetExists = fs::exists(assetPath, ec);
            if (ec)
            {
                result.error = "Could not inspect reusable-asset destination.";
                return result;
            }
            if (!snapshotExists && !assetExists)
                break;
        }

        fs::create_directories(snapshotDirectory, ec);
        if (ec)
        {
            result.error = "Could not create retained model-source folder: " + ec.message();
            return result;
        }
        const auto cleanupSnapshot = [&snapshotDirectory]()
        {
            std::error_code ignored;
            fs::remove_all(snapshotDirectory, ignored);
        };

        const fs::path retainedSource = snapshotDirectory / source.filename();
        fs::copy_file(source, retainedSource, fs::copy_options::none, ec);
        if (ec)
        {
            result.error = "Could not retain selected model source: " + ec.message();
            cleanupSnapshot();
            return result;
        }
        if (!CopyGltfExternalFiles(source, snapshotDirectory, result.error))
        {
            cleanupSnapshot();
            return result;
        }

        result.stagedSourceProjectRelativePath = retainedSource
            .lexically_relative(root).lexically_normal().generic_u8string();
        result.assetProjectRelativePath = assetPath
            .lexically_relative(root).lexically_normal().generic_u8string();

        ReusableModelImportRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = projectId;
        request.sourceProjectRelativePath = result.stagedSourceProjectRelativePath;
        request.assetProjectRelativePath = result.assetProjectRelativePath;
        request.expectedFormat = format;
        result.asset = ReusableAssetService().ImportModelAsset(request);
        if (!result.asset.succeeded)
        {
            result.error = result.asset.error;
            cleanupSnapshot();
            return result;
        }

        result.succeeded = true;
        result.error.clear();
        return result;
    }

    ReusableModelReimportResult CreatorAssetWorkflowService::ReimportModel(
        const std::string& projectRoot,
        const StableId& projectId,
        const StableId& assetId) const
    {
        ReusableModelReimportResult result;
        result.assetId = assetId;
        if (!IsValidStableId(projectId) || !IsValidStableId(assetId))
        {
            result.error = "Creator reimport requires valid project and asset IDs.";
            return result;
        }
        fs::path root;
        if (!ResolveRoot(projectRoot, root, result.error))
            return result;

        AssetRegistryRefresh refresh;
        if (!RefreshRegistryInternal(root, projectId, refresh, result.error))
            return result;

        ReusableModelReimportRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = projectId;
        request.assetId = assetId;
        return ReusableAssetService().ReimportModelAsset(request);
    }

    PreparedReusableModelPlacement CreatorAssetWorkflowService::PrepareModelPlacement(
        const std::string& projectRoot,
        const StableId& projectId,
        const StableId& assetId) const
    {
        ReusableModelPlacementRequest request;
        request.projectRoot = projectRoot;
        request.projectId = projectId;
        request.assetId = assetId;
        return ReusableAssetService().PrepareModelAssetPlacement(request);
    }

    bool CreatorAssetWorkflowService::SetCreatorTags(
        const std::string& projectRoot,
        const StableId& projectId,
        const StableId& assetId,
        std::vector<std::string> tags,
        std::string& error) const
    {
        if (!IsValidStableId(projectId) || !IsValidStableId(assetId))
        {
            error = "Creator tag edit requires valid project and asset IDs.";
            return false;
        }
        fs::path root;
        if (!ResolveRoot(projectRoot, root, error))
            return false;

        AssetRegistry registry;
        if (!ReadAssetRegistry(root.generic_u8string(), projectId, registry, error))
            return false;
        const bool registered = std::any_of(
            registry.records.begin(), registry.records.end(),
            [&assetId](const AssetRecord& record)
            { return record.assetId == assetId; });
        if (!registered)
        {
            error = "Creator tags can only be edited on an active registered asset.";
            return false;
        }

        AssetCatalogueMetadataDocument metadata;
        if (!ReadAssetCatalogueMetadata(
                root.generic_u8string(), projectId, metadata, error) ||
            !SetAssetCreatorTags(metadata, assetId, std::move(tags), error))
            return false;

        AssetCatalogueMetadataPersistenceOptions options;
        options.transactionId = "lp07-gate5-tags-" + GenerateStableId();
        const auto written = WriteAssetCatalogueMetadata(
            root.generic_u8string(), metadata, std::move(options));
        if (!written.success || !written.committed)
        {
            error = written.message.empty()
                ? "Could not persist creator asset tags."
                : written.message;
            return false;
        }
        error.clear();
        return true;
    }
}
