#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include "renegade/bridge/ProjectService.h"

namespace
{
    namespace fs = std::filesystem;
    using renegade::bridge::ProjectDocumentTransaction;
    using renegade::bridge::ProjectDocumentTransactionHookAction;
    using renegade::bridge::ProjectDocumentTransactionOptions;
    using renegade::bridge::ProjectDocumentTransactionStage;
    using renegade::bridge::ProjectDocumentWrite;
    using renegade::bridge::ProjectMetadata;
    using renegade::bridge::ProjectService;

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

    void WriteScene(const fs::path& path)
    {
        fs::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary).put('\0');
    }

    std::string LegacyDescriptor(const std::string& name)
    {
        return "format = renegade-project\n"
            "version = 1\n\n"
            "[project]\n"
            "name = " + name + "\n"
            "startup_scene = Content/Scenes/Main.wiscene\n";
    }

    std::string CurrentDescriptor(
        const std::string& name,
        const std::string& projectId)
    {
        return "format = renegade-project\n"
            "version = 1\n\n"
            "[project]\n"
            "project_id = " + projectId + "\n"
            "name = " + name + "\n"
            "startup_scene = Content/Scenes/Main.wiscene\n"
            "startup_flow_id = \n"
            "startup_flow = \n"
            "startup_screen_id = \n"
            "startup_screen = \n";
    }

    bool HasTransactionArtifacts(const fs::path& root)
    {
        std::error_code error;
        if (!fs::exists(root, error))
        {
            return false;
        }
        for (fs::recursive_directory_iterator iterator(root, error);
            !error && iterator != fs::recursive_directory_iterator();
            iterator.increment(error))
        {
            const std::string name =
                iterator->path().filename().generic_u8string();
            if (name.find(".renegade-stage-") != std::string::npos ||
                name.find(".renegade-backup-") != std::string::npos ||
                name.find(".renegade-restore-") != std::string::npos ||
                name.find(".renegade-transaction-") != std::string::npos ||
                name.find(".journal.writing") != std::string::npos)
            {
                return true;
            }
        }
        return error.value() != 0;
    }

    void TestTransactionalLegacyMigration(const fs::path& root)
    {
        const fs::path descriptor = root / "LegacyProject.renegade";
        const fs::path scene = root / "Content/Scenes/Main.wiscene";
        const std::string legacy = LegacyDescriptor("LegacyProject");
        WriteScene(scene);
        WriteText(descriptor, legacy);

        ProjectService projects;
        projects.Initialize((root / "editor-state.ini").generic_u8string());
        Check(projects.OpenProject(descriptor.generic_u8string()),
            "transactional legacy migration did not open");
        if (!projects.HasProject())
        {
            return;
        }

        Check(renegade::bridge::IsValidStableId(
                projects.CurrentProject().projectId),
            "transactional migration did not assign a valid project ID");

        const fs::path backup = root / "LegacyProject.bak.renegade";
        Check(fs::is_regular_file(backup),
            "transactional migration did not retain the previous descriptor");
        Check(ReadText(backup) == legacy,
            "transactional migration backup did not preserve exact old bytes");

        ProjectMetadata inspected;
        std::string error;
        Check(projects.InspectProject(
                descriptor.generic_u8string(), inspected, error),
            "migrated descriptor did not pass read-only inspection");
        Check(inspected.projectId == projects.CurrentProject().projectId,
            "migrated descriptor ID changed during inspection");
        Check(!HasTransactionArtifacts(root),
            "successful migration left transaction artifacts");
    }

    void TestInterruptedOpenRecovery(const fs::path& root)
    {
        constexpr const char* projectId =
            "71111111-1111-4111-8111-111111111111";
        const fs::path descriptor = root / "RecoveryProject.renegade";
        const fs::path scene = root / "Content/Scenes/Main.wiscene";
        const std::string original =
            CurrentDescriptor("RecoveryProject", projectId);
        WriteScene(scene);
        WriteText(descriptor, original);

        ProjectDocumentWrite write;
        write.destinationPath = descriptor.generic_u8string();
        const std::string invalid = "format = interrupted\n";
        write.content.assign(invalid.begin(), invalid.end());
        write.validator = [](
            const std::string&,
            std::string& error)
        {
            error.clear();
            return true;
        };

        ProjectDocumentTransactionOptions options;
        options.transactionId = "project-open-recovery";
        options.journalDirectory =
            (root / "Intermediate/Transactions").generic_u8string();
        options.allowedRoot = root.generic_u8string();
        options.operationHook = [](
            const ProjectDocumentTransactionStage stage,
            const std::size_t index,
            const std::string&,
            std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::AfterReplace &&
                index == 0)
            {
                error = "simulated process interruption";
                return ProjectDocumentTransactionHookAction::Interrupt;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        ProjectDocumentTransaction transaction;
        auto interrupted = transaction.Execute({std::move(write)}, options);
        Check(!interrupted.success && interrupted.recoveryRequired,
            "interrupted project descriptor did not retain recovery evidence");
        Check(ReadText(descriptor) == invalid,
            "interrupted transaction did not reach the intended crash window");

        ProjectService projects;
        projects.Initialize((root / "editor-state.ini").generic_u8string());
        Check(projects.OpenProject(descriptor.generic_u8string()),
            "project open did not recover an interrupted descriptor transaction");
        Check(ReadText(descriptor) == original,
            "project open recovery did not restore exact descriptor bytes");
        Check(projects.CurrentProject().projectId == projectId,
            "project open recovery loaded the wrong project identity");
        Check(!projects.LastWarning().empty(),
            "project open recovery did not surface recovery evidence");
        Check(!HasTransactionArtifacts(root),
            "project open recovery left transaction artifacts");
    }


    void TestMigrationBackupFailurePreservesDescriptor(const fs::path& root)
    {
        const fs::path descriptor = root / "BlockedProject.renegade";
        const fs::path scene = root / "Content/Scenes/Main.wiscene";
        const fs::path backup = root / "BlockedProject.bak.renegade";
        const std::string legacy = LegacyDescriptor("BlockedProject");
        WriteScene(scene);
        WriteText(descriptor, legacy);
        fs::create_directories(backup);

        ProjectService projects;
        projects.Initialize((root / "editor-state.ini").generic_u8string());
        Check(!projects.OpenProject(descriptor.generic_u8string()),
            "migration unexpectedly succeeded without a valid backup destination");
        Check(!projects.HasProject(),
            "failed migration activated the project");
        Check(ReadText(descriptor) == legacy,
            "backup failure changed the legacy descriptor");
        Check(fs::is_directory(backup),
            "backup failure replaced the blocking directory");
        Check(!HasTransactionArtifacts(root),
            "backup failure left transaction artifacts");
    }

    void TestFreshProjectHasNoPreviousBackup(const fs::path& root)
    {
        const fs::path parent = root / "Projects";
        const fs::path templateScene = root / "Template.wiscene";
        fs::create_directories(parent);
        WriteScene(templateScene);

        ProjectService projects;
        projects.Initialize((root / "editor-state.ini").generic_u8string());
        Check(projects.CreateProject(
                parent.generic_u8string(),
                "FreshProject",
                templateScene.generic_u8string()),
            "fresh project creation failed through transaction path");

        const fs::path projectRoot = parent / "FreshProject";
        const fs::path descriptor = projectRoot / "FreshProject.renegade";
        Check(fs::is_regular_file(descriptor),
            "fresh transactional descriptor was not created");
        Check(!fs::exists(projectRoot / "FreshProject.bak.renegade"),
            "fresh project incorrectly created a previous-version backup");
        Check(!HasTransactionArtifacts(projectRoot),
            "fresh project creation left transaction artifacts");

        ProjectMetadata inspected;
        std::string error;
        Check(projects.InspectProject(
                descriptor.generic_u8string(), inspected, error),
            "fresh transactional descriptor did not round-trip");
    }

    void TestDependencyInspectionRetainsMissingStartup(const fs::path& root)
    {
        const fs::path descriptor = root / "MissingScene.renegade";
        WriteText(descriptor, CurrentDescriptor(
            "MissingScene", "11111111-1111-4111-8111-111111111111"));

        ProjectService projects;
        ProjectMetadata metadata;
        std::string error;
        Check(!projects.InspectProject(
                descriptor.generic_u8string(), metadata, error),
            "normal project inspection accepted a missing startup scene");
        Check(projects.InspectProjectForDependencies(
                descriptor.generic_u8string(), metadata, error),
            "dependency inspection rejected a declared missing startup scene");
        Check(metadata.startupScene == "Content/Scenes/Main.wiscene",
            "dependency inspection discarded the missing startup declaration");
        renegade::bridge::ProjectDependencyDocument dependencies;
        auto reader = renegade::bridge::MakeProjectDependencyReader();
        Check(reader(descriptor.generic_u8string(), dependencies, error),
            "project dependency adapter rejected valid descriptor metadata");
        Check(dependencies.startupScene == "Content/Scenes/Main.wiscene",
            "project dependency adapter discarded the startup declaration");
    }

    void TestAlwaysIncludeRoundTripAndProjection(const fs::path& root)
    {
        const fs::path templateScene = root / "Template.wiscene";
        WriteScene(templateScene);

        ProjectService projects;
        projects.Initialize((root / "editor-state.ini").generic_u8string());
        Check(projects.CreateProject(
                root.generic_u8string(), "AlwaysInclude",
                templateScene.generic_u8string()),
            "Always Include fixture project did not create transactionally");
        if (!projects.HasProject())
            return;

        const fs::path projectRoot = root / "AlwaysInclude";
        const fs::path descriptor = projectRoot / "AlwaysInclude.renegade";
        WriteText(projectRoot / "Content/Textures/rock,wet.png", "texture");
        WriteText(projectRoot / "Content/Data/state#1; final.bin", "generated");
        WriteText(projectRoot / "Content/Fonts/runtime font.ttf", "font");

        const std::vector<std::string> declarations = {
            "texture:Content/Textures/rock,wet.png",
            "generated_data:Content/Data/state#1; final.bin",
            "font:Content/Fonts/runtime font.ttf",
        };

        Check(projects.SetAlwaysInclude(declarations),
            "Always Include descriptor did not write transactionally");
        const std::string serialized = ReadText(descriptor);
        Check(serialized.find("always_include_format = 1") != std::string::npos &&
                serialized.find("rock%2Cwet.png") != std::string::npos &&
                serialized.find("state%231%3B%20final.bin") != std::string::npos &&
                serialized.find("runtime%20font.ttf") != std::string::npos,
            "Always Include descriptor did not encode config delimiters");

        ProjectMetadata inspected;
        std::string error;
        Check(projects.InspectProject(
                descriptor.generic_u8string(), inspected, error),
            "encoded Always Include descriptor did not inspect");
        Check(inspected.alwaysInclude == declarations &&
                projects.CurrentProject().alwaysInclude == declarations,
            "Always Include declarations did not round-trip exactly");
        Check(!projects.SetAlwaysInclude({"texture:../escape.png"}) &&
                projects.CurrentProject().alwaysInclude == declarations &&
                ReadText(descriptor) == serialized,
            "invalid Always Include update changed active or persisted metadata");

        auto reader = renegade::bridge::MakeProjectDependencyReader();
        renegade::bridge::ProjectDependencyDocument document;
        Check(reader(descriptor.generic_u8string(), document, error),
            "project dependency reader rejected Always Include declarations");
        Check(document.alwaysInclude.size() == 3 &&
                document.alwaysInclude[0].dependencyClass ==
                    renegade::bridge::DependencyClass::Texture &&
                document.alwaysInclude[1].dependencyClass ==
                    renegade::bridge::DependencyClass::GeneratedData &&
                document.alwaysInclude[2].dependencyClass ==
                    renegade::bridge::DependencyClass::Font,
            "Always Include reader lost declared dependency classes");

        renegade::bridge::ProjectDependencyProvider provider(reader);
        renegade::bridge::DependencyCollector collector(
            projectRoot.generic_u8string());
        Check(collector.RegisterProvider(provider, error) &&
                collector.AddRoot({"AlwaysInclude.renegade",
                    renegade::bridge::DependencyClass::ProjectDocument,
                    renegade::bridge::DependencyRequirement::Required,
                    "fixture.project"}, error) &&
                collector.DiscoverRootDependencies(error),
            "Always Include production provider did not build a graph");
        const auto& graph = collector.Graph();
        Check(graph.nodes.size() == 5 && graph.edges.size() == 4,
            "Always Include graph omitted startup or declared dependencies");
        Check(graph.nodes[3].dependencyClass ==
                renegade::bridge::DependencyClass::GeneratedData,
            "external generated data lost its declared graph class");

        const std::string projectId = projects.CurrentProject().projectId;
        const fs::path legacyDescriptor = projectRoot /
            "LegacyAlwaysInclude.renegade";
        WriteText(legacyDescriptor,
            CurrentDescriptor("LegacyAlwaysInclude", projectId) +
            "\n[dependencies]\n"
            "always_include = generated_data:Content/Data/legacy.bin\n");
        Check(projects.InspectProjectForDependencies(
                legacyDescriptor.generic_u8string(), inspected, error) &&
                inspected.alwaysInclude.size() == 1,
            "PR #29 legacy Always Include format no longer reads");

        const fs::path malformedDescriptor = projectRoot /
            "MalformedAlwaysInclude.renegade";
        WriteText(malformedDescriptor,
            CurrentDescriptor("MalformedAlwaysInclude", projectId) +
            "\n[dependencies]\n"
            "always_include_format = 1\n"
            "always_include_count = 1\n"
            "always_include_0_class = texture\n"
            "always_include_0_path = Content/Textures/bad%ZZ.png\n");
        Check(!projects.InspectProjectForDependencies(
                malformedDescriptor.generic_u8string(), inspected, error),
            "malformed encoded Always Include path was accepted");
    }
}

int main()
{
    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-project-service-transaction-" +
            std::to_string(unique));
    fs::create_directories(root);

    TestTransactionalLegacyMigration(root / "01-migration");
    TestInterruptedOpenRecovery(root / "02-recovery");
    TestFreshProjectHasNoPreviousBackup(root / "03-create");
    TestMigrationBackupFailurePreservesDescriptor(root / "04-backup-failure");
    TestDependencyInspectionRetainsMissingStartup(root / "05-dependency-inspection");
    TestAlwaysIncludeRoundTripAndProjection(root / "06-always-include");

    std::error_code ignored;
    fs::remove_all(root, ignored);

    if (failures != 0)
    {
        std::cerr << failures << " project service transaction checks failed\n";
        return 1;
    }

    std::cout << "PASS: project descriptor transaction, migration and recovery\n";
    return 0;
}
