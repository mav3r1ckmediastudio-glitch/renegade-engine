#include "renegade/bridge/BuildService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <limits>
#include <map>
#include <set>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "json.hpp"

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        bool IsAsciiControl(const unsigned char value)
        {
            return value < 32 || value == 127;
        }

        std::string AsciiLower(std::string value)
        {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](const unsigned char character)
                {
                    if (character >= 'A' && character <= 'Z')
                        return static_cast<char>(character - 'A' + 'a');
                    return static_cast<char>(character);
                });
            return value;
        }

        bool IsReservedWindowsName(const std::string& value)
        {
            std::string stem = value;
            const std::size_t dot = stem.find('.');
            if (dot != std::string::npos)
                stem.resize(dot);
            stem = AsciiLower(stem);

            if (stem == "con" || stem == "prn" || stem == "aux" ||
                stem == "nul")
            {
                return true;
            }
            if (stem.size() == 4 &&
                ((stem.rfind("com", 0) == 0) ||
                    (stem.rfind("lpt", 0) == 0)) &&
                stem[3] >= '1' && stem[3] <= '9')
            {
                return true;
            }
            return false;
        }

        bool IsSafeWindowsName(const std::string& value)
        {
            if (value.empty() || value == "." || value == ".." ||
                value.size() > 96 || value.back() == ' ' || value.back() == '.')
            {
                return false;
            }

            constexpr const char* Invalid = "<>:\"/\\|?*";
            if (std::any_of(
                    value.begin(),
                    value.end(),
                    [](const unsigned char character)
                    {
                        return IsAsciiControl(character);
                    }) ||
                value.find_first_of(Invalid) != std::string::npos)
            {
                return false;
            }
            return !IsReservedWindowsName(value);
        }

        bool IsSafeCanonicalWindowsRelativePath(const std::string& value)
        {
            if (value.empty() || value.find('\\') != std::string::npos)
                return false;

            const fs::path path = fs::u8path(value);
            if (path.is_absolute() || path.has_root_name() ||
                path.generic_u8string() != value ||
                path.lexically_normal().generic_u8string() != value)
            {
                return false;
            }

            for (const fs::path& part : path)
            {
                const std::string segment = part.generic_u8string();
                if (!IsSafeWindowsName(segment))
                    return false;
            }
            return true;
        }

        bool IsSha256(const std::string& value)
        {
            return value.size() == 64 &&
                std::all_of(
                    value.begin(),
                    value.end(),
                    [](const unsigned char character)
                    {
                        return (character >= '0' && character <= '9') ||
                            (character >= 'a' && character <= 'f');
                    });
        }

        const char* RequirementName(const DependencyRequirement requirement)
        {
            switch (requirement)
            {
            case DependencyRequirement::Required: return "required";
            case DependencyRequirement::Optional: return "optional";
            case DependencyRequirement::EditorOnly: return "editor_only";
            }
            return "invalid";
        }

        const char* FileKindName(const WindowsGameBuildFileKind kind)
        {
            switch (kind)
            {
            case WindowsGameBuildFileKind::ProjectContent:
                return "project_content";
            case WindowsGameBuildFileKind::RuntimeSupport:
                return "runtime_support";
            }
            return "invalid";
        }

        const char* DiagnosticName(const DependencyDiagnosticCode code)
        {
            switch (code)
            {
            case DependencyDiagnosticCode::Missing: return "missing";
            case DependencyDiagnosticCode::OutsideProject: return "outside_project";
            case DependencyDiagnosticCode::Duplicate: return "duplicate";
            case DependencyDiagnosticCode::CaseCollision: return "case_collision";
            case DependencyDiagnosticCode::UndeclaredComputedReference:
                return "undeclared_computed_reference";
            }
            return "invalid";
        }

        std::set<std::string> ReachableNodeIds(const DependencyGraph& graph)
        {
            std::set<std::string> reachable(
                graph.rootIds.begin(),
                graph.rootIds.end());
            bool changed = true;
            while (changed)
            {
                changed = false;
                for (const DependencyEdge& edge : graph.edges)
                {
                    if (reachable.count(edge.sourceId) != 0 &&
                        reachable.insert(edge.targetId).second)
                    {
                        changed = true;
                    }
                }
            }
            return reachable;
        }

        std::vector<std::string> BuildProvenance(
            const DependencyGraph& graph,
            const DependencyNode& node,
            const std::set<std::string>& rootIds)
        {
            std::vector<std::string> result;
            if (rootIds.count(node.id) != 0)
                result.push_back("root");
            for (const DependencyEdge& edge : graph.edges)
            {
                if (edge.targetId == node.id)
                {
                    result.push_back(
                        "edge:" + edge.sourceId + ":" + edge.provenance);
                }
            }
            std::sort(result.begin(), result.end());
            result.erase(std::unique(result.begin(), result.end()), result.end());
            return result;
        }

#if defined(_WIN32)
        bool TryDecodeUtf8(const std::string& value, std::wstring& decoded)
        {
            if (value.size() > static_cast<std::size_t>(
                    (std::numeric_limits<int>::max)()))
            {
                return false;
            }
            const int size = static_cast<int>(value.size());
            const int required = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), size, nullptr, 0);
            if (required <= 0)
                return false;
            decoded.resize(static_cast<std::size_t>(required));
            return MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), size,
                decoded.data(), required) == required;
        }
#endif

        bool WindowsPathCaseEquivalent(
            const std::string& left,
            const std::string& right)
        {
            if (left == right)
                return true;
#if defined(_WIN32)
            std::wstring decodedLeft;
            std::wstring decodedRight;
            if (!TryDecodeUtf8(left, decodedLeft) ||
                !TryDecodeUtf8(right, decodedRight))
            {
                return false;
            }
            return CompareStringOrdinal(
                decodedLeft.data(), static_cast<int>(decodedLeft.size()),
                decodedRight.data(), static_cast<int>(decodedRight.size()),
                TRUE) == CSTR_EQUAL;
#else
            return AsciiLower(left) == AsciiLower(right);
#endif
        }

        bool AddDestination(
            std::vector<std::string>& destinations,
            const std::string& destination,
            std::string& error)
        {
            if (!IsSafeCanonicalWindowsRelativePath(destination))
            {
                error = "Standalone build destination is not a safe canonical "
                    "Windows-relative path: " + destination;
                return false;
            }

            const auto collision = std::find_if(
                destinations.begin(),
                destinations.end(),
                [&destination](const std::string& existing)
                {
                    return WindowsPathCaseEquivalent(existing, destination);
                });
            if (collision != destinations.end())
            {
                error = "Standalone build destination collides with another "
                    "planned file: " + destination + " and " + *collision;
                return false;
            }
            destinations.push_back(destination);
            return true;
        }

        bool ValidatePlanIdentity(
            const WindowsGameBuildRequest& request,
            const ProjectMetadata& project,
            StableId& saveDataId,
            std::string& error)
        {
            if (!IsValidStableId(project.projectId))
            {
                error = "Standalone build requires a valid stable project ID.";
                return false;
            }
            if (!IsSafeWindowsName(request.gameName))
            {
                error = "Standalone game name is not a safe Windows name.";
                return false;
            }
            const std::string executableLower =
                AsciiLower(request.executableBaseName);
            const bool hasExeSuffix = executableLower.size() >= 4 &&
                executableLower.compare(
                    executableLower.size() - 4,
                    4,
                    ".exe") == 0;
            if (!IsSafeWindowsName(request.executableBaseName) ||
                request.executableBaseName.size() > 80 ||
                hasExeSuffix)
            {
                error = "Standalone executable base name is invalid or already "
                    "contains the .exe suffix.";
                return false;
            }
            if (request.publicVersion.empty() || request.publicVersion.size() > 64 ||
                request.publicVersion.find('\n') != std::string::npos ||
                request.publicVersion.find('\r') != std::string::npos)
            {
                error = "Standalone public version is empty or invalid.";
                return false;
            }
            if (request.platform != "windows-x64")
            {
                error = "LP06 Gate 1 supports only windows-x64.";
                return false;
            }
            if (request.configuration != "Release")
            {
                error = "LP06 Gate 1 standalone plans require Release Runtime support.";
                return false;
            }

            saveDataId = request.saveDataId.empty() ?
                project.projectId : request.saveDataId;
            if (!IsValidStableId(saveDataId))
            {
                error = "Standalone build requires a valid stable Save Data ID.";
                return false;
            }
            return true;
        }

        bool ValidatePlanForSerialization(
            const WindowsGameBuildPlan& plan,
            std::string& error)
        {
            if (plan.formatIdentifier != "renegade-windows-game-build-plan" ||
                plan.schemaVersion != WindowsGameBuildPlan::CurrentSchemaVersion ||
                !IsValidStableId(plan.projectId) ||
                !IsValidStableId(plan.saveDataId) ||
                !IsSafeWindowsName(plan.gameName) ||
                !IsSafeWindowsName(plan.buildFolderName) ||
                !IsSafeWindowsName(plan.executableFileName) ||
                plan.platform != "windows-x64" ||
                plan.configuration != "Release")
            {
                error = "Standalone build plan identity is invalid.";
                return false;
            }

            std::vector<std::string> destinations;
            bool foundExecutable = false;
            for (const WindowsGameBuildFile& file : plan.files)
            {
                if (!AddDestination(destinations, file.destinationPath, error))
                    return false;

                if (file.kind == WindowsGameBuildFileKind::ProjectContent)
                {
                    if (!IsSafeCanonicalWindowsRelativePath(
                            file.projectRelativeSourcePath) ||
                        !IsValidStableId(file.assetId) ||
                        file.sourceContentHash.empty() ||
                        file.provenance.empty() ||
                        file.destinationPath.rfind("GameData/", 0) != 0)
                    {
                        error = "Standalone project-content plan entry is invalid: " +
                            file.destinationPath;
                        return false;
                    }
                }
                else
                {
                    if (file.runtimeSupportName.empty() || file.byteCount == 0 ||
                        !IsSha256(file.sha256) || file.provenance.empty() ||
                        file.destinationPath.rfind("GameData/", 0) == 0)
                    {
                        error = "Standalone Runtime-support plan entry is invalid: " +
                            file.destinationPath;
                        return false;
                    }
                    if (file.destinationPath == plan.executableFileName)
                        foundExecutable = true;
                }
            }

            if (!foundExecutable)
            {
                error = "Standalone build plan does not contain its named executable.";
                return false;
            }
            error.clear();
            return true;
        }
    }

    bool CreateWindowsGameBuildPlan(
        const ProjectMetadata& project,
        const DependencyGraph& graph,
        const AssetRegistry& registry,
        const WindowsGameBuildRequest& request,
        const std::vector<WindowsRuntimeSupportInput>& runtimeSupport,
        WindowsGameBuildPlan& plan,
        std::string& error)
    {
        plan = {};
        error.clear();

        StableId saveDataId;
        if (!ValidatePlanIdentity(request, project, saveDataId, error))
            return false;

        if (registry.projectId != project.projectId ||
            !ValidateAssetRegistry(registry, error))
        {
            if (error.empty())
                error = "Asset registry does not belong to the build project.";
            else
                error = "Standalone build rejected the LC01 registry: " + error;
            return false;
        }

        std::string graphJson;
        if (!SerializeDependencyGraph(graph, graphJson, error))
        {
            error = "Standalone build rejected the LP05 dependency graph: " + error;
            return false;
        }

        for (const DependencyDiagnostic& diagnostic : graph.diagnostics)
        {
            if (diagnostic.code == DependencyDiagnosticCode::OutsideProject ||
                diagnostic.code == DependencyDiagnosticCode::CaseCollision ||
                diagnostic.code ==
                    DependencyDiagnosticCode::UndeclaredComputedReference)
            {
                error = "Standalone build cannot continue with dependency " +
                    std::string(DiagnosticName(diagnostic.code)) + ": " +
                    diagnostic.path;
                return false;
            }
        }

        std::map<std::string, const AssetRecord*> recordsByNodeId;
        for (const AssetRecord& record : registry.records)
        {
            const auto inserted = recordsByNodeId.emplace(
                record.dependencyNodeId,
                &record);
            if (!inserted.second)
            {
                error = "LC01 registry contains duplicate dependency node IDs.";
                return false;
            }
        }

        std::set<std::string> rootIds(graph.rootIds.begin(), graph.rootIds.end());
        const std::set<std::string> reachableNodeIds = ReachableNodeIds(graph);
        std::vector<std::string> destinations;
        std::set<StableId> includedAssetIds;

        plan.projectId = project.projectId;
        plan.gameName = request.gameName;
        plan.executableFileName = request.executableBaseName + ".exe";
        plan.buildFolderName = request.gameName + " Windows Build";
        plan.publicVersion = request.publicVersion;
        plan.saveDataId = std::move(saveDataId);
        plan.platform = request.platform;
        plan.configuration = request.configuration;

        for (const DependencyDiagnostic& diagnostic : graph.diagnostics)
        {
            if (diagnostic.code == DependencyDiagnosticCode::Duplicate ||
                diagnostic.code == DependencyDiagnosticCode::Missing)
            {
                plan.warnings.push_back(
                    std::string(DiagnosticName(diagnostic.code)) + ":" +
                    diagnostic.path);
            }
        }

        for (const DependencyNode& node : graph.nodes)
        {
            if (reachableNodeIds.count(node.id) == 0)
            {
                ++plan.excludedUnreachable;
                continue;
            }
            if (node.runtimeSupport ||
                node.dependencyClass == DependencyClass::RuntimeSupport)
            {
                continue;
            }
            if (node.requirement == DependencyRequirement::EditorOnly)
            {
                ++plan.excludedEditorOnly;
                continue;
            }
            if (node.applicability != "windows-x64")
            {
                error = "Standalone build encountered a project dependency with "
                    "unsupported applicability: " + node.projectRelativePath;
                return false;
            }
            if (node.contentHash == "missing")
            {
                if (node.requirement == DependencyRequirement::Optional)
                {
                    ++plan.excludedOptionalMissing;
                    plan.warnings.push_back(
                        "optional_missing:" + node.projectRelativePath);
                    continue;
                }
                error = "Standalone build is missing required project content: " +
                    node.projectRelativePath;
                return false;
            }

            const auto recordIt = recordsByNodeId.find(node.id);
            if (recordIt == recordsByNodeId.end())
            {
                error = "LC01 registry has no stable asset record for dependency: " +
                    node.projectRelativePath;
                return false;
            }
            const AssetRecord& record = *recordIt->second;
            if (!record.sourceAvailable ||
                record.projectRelativePath != node.projectRelativePath ||
                record.dependencyClass != node.dependencyClass ||
                record.requirement != node.requirement ||
                record.applicability != node.applicability ||
                record.provider != node.provider ||
                record.providerVersion != node.providerVersion ||
                record.contentHash != node.contentHash)
            {
                error = "LP05 dependency and LC01 asset state disagree for: " +
                    node.projectRelativePath;
                return false;
            }

            WindowsGameBuildFile file;
            file.kind = WindowsGameBuildFileKind::ProjectContent;
            file.destinationPath = "GameData/" + node.projectRelativePath;
            file.projectRelativeSourcePath = node.projectRelativePath;
            file.assetId = record.assetId;
            file.dependencyClass = node.dependencyClass;
            file.requirement = node.requirement;
            file.sourceContentHash = node.contentHash;
            file.provenance = BuildProvenance(graph, node, rootIds);

            if (!AddDestination(destinations, file.destinationPath, error))
                return false;
            includedAssetIds.insert(record.assetId);
            plan.files.push_back(std::move(file));
        }

        for (const ImportedProductRecord& imported : registry.importedProducts)
        {
            if (includedAssetIds.count(imported.productAssetId) == 0)
                continue;

            ImportedProductStatus status;
            if (!GetImportedProductStatus(registry, imported, status, error))
            {
                error = "Standalone build could not validate imported-product "
                    "provenance: " + error;
                return false;
            }
            if (!status.sourceAvailable || !status.productAvailable ||
                status.sourceChanged || status.productChanged)
            {
                error = "Standalone build refuses stale imported product " +
                    imported.productAssetId +
                    "; its recorded LC01 import provenance is no longer current.";
                return false;
            }
        }

        bool foundExecutable = false;
        bool foundDxCompiler = false;
        for (const WindowsRuntimeSupportInput& support : runtimeSupport)
        {
            if (support.logicalName.empty() || support.provenance.empty() ||
                support.byteCount == 0 || !IsSha256(support.sha256) ||
                !IsSafeCanonicalWindowsRelativePath(support.destinationPath) ||
                support.destinationPath.rfind("GameData/", 0) == 0)
            {
                error = "Standalone Runtime-support input is invalid: " +
                    support.destinationPath;
                return false;
            }

            WindowsGameBuildFile file;
            file.kind = WindowsGameBuildFileKind::RuntimeSupport;
            file.destinationPath = support.destinationPath;
            file.runtimeSupportName = support.logicalName;
            file.byteCount = support.byteCount;
            file.sha256 = support.sha256;
            file.provenance = { support.provenance };

            if (!AddDestination(destinations, file.destinationPath, error))
                return false;
            if (file.destinationPath == plan.executableFileName)
                foundExecutable = true;
            if (AsciiLower(file.destinationPath) == "dxcompiler.dll")
                foundDxCompiler = true;
            plan.files.push_back(std::move(file));
        }

        if (!foundExecutable)
        {
            error = "Runtime support does not provide the named standalone "
                "executable: " + plan.executableFileName;
            return false;
        }
        if (!foundDxCompiler)
        {
            error = "LP06 Gate 1 requires dxcompiler.dll in the current Runtime "
                "support allowlist.";
            return false;
        }

        std::sort(
            plan.files.begin(),
            plan.files.end(),
            [](const WindowsGameBuildFile& left,
               const WindowsGameBuildFile& right)
            {
                return left.destinationPath < right.destinationPath;
            });
        std::sort(plan.warnings.begin(), plan.warnings.end());
        plan.warnings.erase(
            std::unique(plan.warnings.begin(), plan.warnings.end()),
            plan.warnings.end());

        if (!ValidatePlanForSerialization(plan, error))
            return false;

        error.clear();
        return true;
    }

    bool SerializeWindowsGameBuildPlan(
        const WindowsGameBuildPlan& plan,
        std::string& json,
        std::string& error)
    {
        json.clear();
        if (!ValidatePlanForSerialization(plan, error))
            return false;

        std::vector<WindowsGameBuildFile> files = plan.files;
        std::sort(
            files.begin(),
            files.end(),
            [](const WindowsGameBuildFile& left,
               const WindowsGameBuildFile& right)
            {
                return left.destinationPath < right.destinationPath;
            });
        std::vector<std::string> warnings = plan.warnings;
        std::sort(warnings.begin(), warnings.end());
        warnings.erase(std::unique(warnings.begin(), warnings.end()), warnings.end());

        nlohmann::json document;
        document["format"] = plan.formatIdentifier;
        document["schema_version"] = plan.schemaVersion;
        document["project_id"] = plan.projectId;
        document["game_name"] = plan.gameName;
        document["executable"] = plan.executableFileName;
        document["build_folder"] = plan.buildFolderName;
        document["public_version"] = plan.publicVersion;
        document["save_data_id"] = plan.saveDataId;
        document["platform"] = plan.platform;
        document["configuration"] = plan.configuration;
        document["excluded_editor_only"] = plan.excludedEditorOnly;
        document["excluded_optional_missing"] = plan.excludedOptionalMissing;
        document["excluded_unreachable"] = plan.excludedUnreachable;
        document["warnings"] = warnings;
        document["files"] = nlohmann::json::array();

        for (const WindowsGameBuildFile& file : files)
        {
            nlohmann::json record;
            record["kind"] = FileKindName(file.kind);
            record["destination"] = file.destinationPath;
            std::vector<std::string> provenance = file.provenance;
            std::sort(provenance.begin(), provenance.end());
            provenance.erase(
                std::unique(provenance.begin(), provenance.end()),
                provenance.end());
            record["provenance"] = provenance;
            if (file.kind == WindowsGameBuildFileKind::ProjectContent)
            {
                record["source"] = file.projectRelativeSourcePath;
                record["asset_id"] = file.assetId;
                record["dependency_class"] =
                    DependencyClassName(file.dependencyClass);
                record["requirement"] = RequirementName(file.requirement);
                record["source_hash"] = file.sourceContentHash;
            }
            else
            {
                record["support_name"] = file.runtimeSupportName;
                record["bytes"] = file.byteCount;
                record["sha256"] = file.sha256;
            }
            document["files"].push_back(std::move(record));
        }

        json = document.dump();
        error.clear();
        return true;
    }
}
