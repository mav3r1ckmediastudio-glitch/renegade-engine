#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ProjectService.h"

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    int failures = 0;

    void Check(const bool condition, const char* message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    void WriteText(const fs::path& path, const std::string& text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    std::string ReadText(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::string(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
    }

    bool HasTransactionArtifacts(const fs::path& root)
    {
        std::error_code error;
        for (fs::recursive_directory_iterator iterator(root, error);
            !error && iterator != fs::recursive_directory_iterator();
            iterator.increment(error))
        {
            const std::string name =
                iterator->path().filename().generic_u8string();
            if (name.find(".renegade-stage-") != std::string::npos ||
                name.find(".renegade-backup-") != std::string::npos ||
                name.find(".renegade-restore-") != std::string::npos ||
                name.find(".renegade-transaction-") != std::string::npos)
                return true;
        }
        return false;
    }

    AssetRegistry MakeRegistry(
        const StableId& projectId,
        const std::string& sceneHash)
    {
        AssetRegistry registry;
        registry.projectId = projectId;

        AssetRecord project;
        project.assetId = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
        project.dependencyNodeId = "project";
        project.projectRelativePath = "PersistenceProject.renegade";
        project.dependencyClass = DependencyClass::ProjectDocument;
        project.provider = "lc01-persistence-fixture";
        project.contentHash = "fnv1a64:1111111111111111";
        project.root = true;
        project.dependencyAssetIds = {
            "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb"};

        AssetRecord scene;
        scene.assetId = "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
        scene.dependencyNodeId = "scene";
        scene.projectRelativePath = "Content/Scenes/Main.wiscene";
        scene.dependencyClass = DependencyClass::Scene;
        scene.provider = "lc01-persistence-fixture";
        scene.contentHash = sceneHash;

        registry.records = {std::move(project), std::move(scene)};
        return registry;
    }

    bool ContainsSceneHash(
        const AssetRegistry& registry,
        const std::string& hash)
    {
        return std::any_of(
            registry.records.begin(), registry.records.end(),
            [&hash](const AssetRecord& record)
            {
                return record.projectRelativePath ==
                        "Content/Scenes/Main.wiscene" &&
                    record.contentHash == hash;
            });
    }
}

int main()
{
    using namespace renegade::bridge;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path base = fs::temp_directory_path() /
        fs::u8path("renegade-asset-registry-persistence-" +
            std::to_string(unique));
    const fs::path templateScene = base / "Template.wiscene";
    WriteText(templateScene, "scene fixture");

    ProjectService projects;
    projects.Initialize((base / "editor-state.ini").generic_u8string());
    Check(projects.CreateProject(
            base.generic_u8string(),
            "PersistenceProject",
            templateScene.generic_u8string()),
        "could not create the persistence fixture project");
    if (!projects.HasProject())
    {
        std::error_code ignored;
        fs::remove_all(base, ignored);
        return 1;
    }

    const fs::path root = base / "PersistenceProject";
    const fs::path descriptor = root / "PersistenceProject.renegade";
    const StableId projectId = projects.CurrentProject().projectId;
    const AssetRegistry original = MakeRegistry(
        projectId, "fnv1a64:2222222222222222");

    AssetRegistryPersistenceOptions initialOptions;
    initialOptions.transactionId = "lc01-initial";
    const auto initial = WriteAssetRegistry(
        root.generic_u8string(), original, std::move(initialOptions));
    Check(initial.success && initial.committed && !initial.noChanges,
        "initial registry transaction did not commit");

    std::string documentPath;
    std::string error;
    Check(ResolveAssetRegistryDocumentPath(
            root.generic_u8string(), documentPath, error) &&
            fs::u8path(documentPath).filename() == AssetRegistryDocumentName,
        "registry did not resolve to the fixed project-root document");
    const std::string originalBytes = ReadText(fs::u8path(documentPath));
    AssetRegistry loaded;
    Check(ReadAssetRegistry(
            root.generic_u8string(), projectId, loaded, error),
        "committed registry did not reload");
    std::string loadedJson;
    Check(SerializeAssetRegistry(loaded, loadedJson, error) &&
            loadedJson == originalBytes,
        "reloaded registry was not byte-identical and canonical");
    Check(!HasTransactionArtifacts(root),
        "successful registry write left transaction artifacts");

    AssetRegistryPersistenceOptions noOpOptions;
    noOpOptions.transactionId = "lc01-noop";
    const auto noOp = WriteAssetRegistry(
        root.generic_u8string(), original, std::move(noOpOptions));
    Check(noOp.success && noOp.committed && noOp.noChanges &&
            ReadText(fs::u8path(documentPath)) == originalBytes,
        "unchanged registry was not a byte-preserving no-op");

    AssetRegistry invalid = original;
    invalid.records[1].assetId = invalid.records[0].assetId;
    const auto rejected = WriteAssetRegistry(root.generic_u8string(), invalid);
    Check(!rejected.success && rejected.code == "asset_registry_invalid" &&
            ReadText(fs::u8path(documentPath)) == originalBytes &&
            !HasTransactionArtifacts(root),
        "invalid registry changed the authoritative document");

    AssetRegistry changed = MakeRegistry(
        projectId, "fnv1a64:3333333333333333");
    const AssetRegistry substitute = MakeRegistry(
        projectId, "fnv1a64:4444444444444444");
    std::string substituteJson;
    Check(SerializeAssetRegistry(substitute, substituteJson, error),
        "could not serialize the staged-substitution fixture");
    AssetRegistryPersistenceOptions substitutionOptions;
    substitutionOptions.transactionId = "lc01-staged-substitution";
    substitutionOptions.operationHook = [substituteJson](
        const ProjectDocumentTransactionStage stage,
        const std::size_t,
        const std::string& path,
        std::string&)
    {
        if (stage == ProjectDocumentTransactionStage::Validate)
            WriteText(fs::u8path(path), substituteJson);
        return ProjectDocumentTransactionHookAction::Continue;
    };
    const auto substituted = WriteAssetRegistry(
        root.generic_u8string(), changed, std::move(substitutionOptions));
    Check(!substituted.success &&
            substituted.stage == ProjectDocumentTransactionStage::Validate &&
            ReadText(fs::u8path(documentPath)) == originalBytes &&
            !HasTransactionArtifacts(root),
        "different valid staged registry was accepted or changed old bytes");

    AssetRegistryPersistenceOptions failureOptions;
    failureOptions.transactionId = "lc01-replace-failure";
    failureOptions.operationHook = [](
        const ProjectDocumentTransactionStage stage,
        const std::size_t,
        const std::string&,
        std::string& hookError)
    {
        if (stage == ProjectDocumentTransactionStage::Replace)
        {
            hookError = "forced registry replacement failure";
            return ProjectDocumentTransactionHookAction::Fail;
        }
        return ProjectDocumentTransactionHookAction::Continue;
    };
    const auto failed = WriteAssetRegistry(
        root.generic_u8string(), changed, std::move(failureOptions));
    Check(!failed.success &&
            ReadText(fs::u8path(documentPath)) == originalBytes &&
            !HasTransactionArtifacts(root),
        "failed registry replacement did not preserve exact old bytes");

    AssetRegistryPersistenceOptions interruptOptions;
    interruptOptions.transactionId = "lc01-interrupted";
    interruptOptions.operationHook = [](
        const ProjectDocumentTransactionStage stage,
        const std::size_t,
        const std::string&,
        std::string&)
    {
        return stage == ProjectDocumentTransactionStage::AfterReplace
            ? ProjectDocumentTransactionHookAction::Interrupt
            : ProjectDocumentTransactionHookAction::Continue;
    };
    const auto interrupted = WriteAssetRegistry(
        root.generic_u8string(), changed, std::move(interruptOptions));
    Check(!interrupted.success && interrupted.recoveryRequired &&
            HasTransactionArtifacts(root),
        "interrupted registry write did not retain recovery evidence");

    ProjectService reopened;
    reopened.Initialize((base / "reopened-editor-state.ini").generic_u8string());
    const bool reopenedProject =
        reopened.OpenProject(descriptor.generic_u8string());
    if (!reopenedProject)
    {
        std::cerr << "RECOVERY ERROR: " << reopened.LastError() << '\n';
    }
    Check(reopenedProject,
        "project open did not recover the interrupted registry transaction");
    Check(!reopened.LastWarning().empty() &&
            ReadText(fs::u8path(documentPath)) == originalBytes &&
            !HasTransactionArtifacts(root),
        "project open did not restore exact registry bytes and clean recovery");
    AssetRegistry recovered;
    Check(ReadAssetRegistry(
            root.generic_u8string(), projectId, recovered, error) &&
            ContainsSceneHash(
                recovered, "fnv1a64:2222222222222222"),
        "recovered registry did not reload the pre-interruption state");

    Check(!ReadAssetRegistry(
            root.generic_u8string(),
            "99999999-9999-4999-8999-999999999999",
            loaded,
            error) && !error.empty(),
        "registry loaded under the wrong project identity");

    WriteText(fs::u8path(documentPath), "\n" + originalBytes);
    Check(!ReadAssetRegistry(
            root.generic_u8string(), projectId, loaded, error) && !error.empty(),
        "non-canonical registry bytes were accepted");

    std::error_code ignored;
    fs::remove_all(base, ignored);
    if (failures != 0)
    {
        std::cerr << failures << " LC01 Gate 2 check(s) failed\n";
        return 1;
    }
    std::cout
        << "PASS: registry commit, reload, no-op, failure preservation, "
           "project-open recovery, and identity validation\n";
    return 0;
}
