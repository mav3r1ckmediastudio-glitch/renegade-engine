#include "renegade/bridge/BuildStageService.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* ProjectId =
        "11111111-1111-4111-8111-111111111111";
    constexpr const char* SaveDataId =
        "22222222-2222-4222-8222-222222222222";

    int Fail(const std::string& message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    bool WriteFile(
        const fs::path& path, const std::string& contents, std::string& error)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error = "could not create fixture directory";
            return false;
        }
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "could not create fixture file";
            return false;
        }
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        output.close();
        if (!output)
        {
            error = "could not write fixture file";
            return false;
        }
        return true;
    }

    std::string Fnv1a64(const std::string& contents)
    {
        std::uint64_t hash = 1469598103934665603ull;
        for (const unsigned char value : contents)
        {
            hash ^= value;
            hash *= 1099511628211ull;
        }
        std::ostringstream stream;
        stream << "fnv1a64:" << std::hex << std::setfill('0')
               << std::setw(16) << hash;
        return stream.str();
    }

    WindowsGameBuildFile ProjectFile(
        std::string destination,
        std::string source,
        StableId assetId,
        const DependencyClass dependencyClass,
        const std::string& contents)
    {
        WindowsGameBuildFile file;
        file.kind = WindowsGameBuildFileKind::ProjectContent;
        file.destinationPath = std::move(destination);
        file.projectRelativeSourcePath = std::move(source);
        file.assetId = std::move(assetId);
        file.dependencyClass = dependencyClass;
        file.requirement = DependencyRequirement::Required;
        file.sourceContentHash = Fnv1a64(contents);
        file.provenance = {"lp05:fixture-reachable"};
        return file;
    }

    WindowsGameBuildFile RuntimeFile(
        std::string destination,
        std::string logicalName,
        const std::uint64_t byteCount,
        std::string sha256)
    {
        WindowsGameBuildFile file;
        file.kind = WindowsGameBuildFileKind::RuntimeSupport;
        file.destinationPath = std::move(destination);
        file.runtimeSupportName = std::move(logicalName);
        file.byteCount = byteCount;
        file.sha256 = std::move(sha256);
        file.provenance = {"lp06:gate2:test-runtime"};
        return file;
    }

    WindowsGameBuildPlan Plan(
        const std::string& projectText,
        const std::string& screenText,
        const std::string& levelOneText,
        const std::string& levelTwoText,
        const std::string& importedText)
    {
        WindowsGameBuildPlan plan;
        plan.projectId = ProjectId;
        plan.gameName = "Proof Game";
        plan.executableFileName = "ProofGame.exe";
        plan.buildFolderName = "Proof Game Windows Build";
        plan.publicVersion = "0.1.0-gate2";
        plan.saveDataId = SaveDataId;
        plan.excludedEditorOnly = 1;
        plan.excludedUnreachable = 1;
        plan.files = {
            ProjectFile(
                "GameData/ProofGame.renegade",
                "ProofGame.renegade",
                "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                DependencyClass::ProjectDocument,
                projectText),
            ProjectFile(
                "GameData/Content/UI/Main.renegade-screen",
                "Content/UI/Main.renegade-screen",
                "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
                DependencyClass::RuntimeScreenDocument,
                screenText),
            ProjectFile(
                "GameData/Content/Scenes/LevelOne.wiscene",
                "Content/Scenes/LevelOne.wiscene",
                "cccccccc-cccc-4ccc-8ccc-cccccccccccc",
                DependencyClass::Scene,
                levelOneText),
            ProjectFile(
                "GameData/Content/Scenes/LevelTwo.wiscene",
                "Content/Scenes/LevelTwo.wiscene",
                "dddddddd-dddd-4ddd-8ddd-dddddddddddd",
                DependencyClass::Scene,
                levelTwoText),
            ProjectFile(
                "GameData/Content/Imported/model.wiscene",
                "Content/Imported/model.wiscene",
                "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee",
                DependencyClass::ImportedContent,
                importedText),
            RuntimeFile(
                "ProofGame.exe",
                "renegade-runtime",
                21,
                "ef9571c86943fc2c68d259d45c06c4966ada22612e70a7a61a7c9add7ae5e77c"),
            RuntimeFile(
                "dxcompiler.dll",
                "directx-shader-compiler",
                17,
                "fd499f835cdc7c5fb7b08baa4a23d88f612d3860ad0d8209fb455b82b1cdb979"),
        };
        return plan;
    }

    std::vector<WindowsPackageDocumentInput> Documents(
        const fs::path& fixtureRoot)
    {
        const std::string readme =
            (fixtureRoot / "ReadMe.txt").generic_u8string();
        const std::string legal =
            (fixtureRoot / "Fixture-Legal-Notice.txt").generic_u8string();
        return {
            {"ReadMe.txt", readme, "gate2-test-readme",
                "repo:test-fixture:lp06-gate2-v1"},
            {"Licences/Renegade-Licence-or-Notice.txt", legal,
                "renegade-test-policy", "repo:test-fixture:lp06-gate2-v1"},
            {"Licences/WickedEngine-LICENSE.txt", legal,
                "wicked-test-notice", "repo:test-fixture:lp06-gate2-v1"},
            {"Licences/WickedEngine-third_party_software.txt", legal,
                "wicked-third-party-test-notice",
                "repo:test-fixture:lp06-gate2-v1"},
            {"Licences/DirectXShaderCompiler-LICENSE.txt", legal,
                "dxc-test-notice", "repo:test-fixture:lp06-gate2-v1"},
            {"Licences/DirectXShaderCompiler-ThirdPartyNotices.txt", legal,
                "dxc-third-party-test-notice",
                "repo:test-fixture:lp06-gate2-v1"},
        };
    }

    WindowsGameBuildStagingRequest Request(
        const fs::path& projectRoot,
        const fs::path& outputRoot,
        const fs::path& supportRoot,
        const fs::path& fixtureRoot,
        std::string stagingId)
    {
        WindowsGameBuildStagingRequest request;
        request.projectRootPath = projectRoot.generic_u8string();
        request.outputParentPath = outputRoot.generic_u8string();
        request.stagingId = std::move(stagingId);
        request.renegadeRevision =
            "ca97b05988ed6f8100b83ac8921873f333bf40bb";
        request.wickedRevision =
            "3a800b7134aafe58461093c8abb2e274d4e64033";
        request.runtimeSupportSources = {
            {"ProofGame.exe",
                (supportRoot / "RenegadeRuntime.exe").generic_u8string()},
            {"dxcompiler.dll",
                (supportRoot / "dxcompiler.dll").generic_u8string()},
        };
        request.packageDocuments = Documents(fixtureRoot);
        return request;
    }

    const WindowsGameStagedFile* Find(
        const WindowsGameBuildStageResult& result,
        const std::string& destination)
    {
        const auto found = std::find_if(result.files.begin(), result.files.end(),
            [&destination](const WindowsGameStagedFile& file)
            {
                return file.destinationPath == destination;
            });
        return found == result.files.end() ? nullptr : &*found;
    }
}

int main(int argc, char** argv)
{
    using namespace renegade::bridge;

    if (argc != 2)
        return Fail("expected the Gate 2 package-document fixture directory");

    const auto nonce = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path(u8"Renegade LP06 Gate2 Ω " + std::to_string(nonce));
    const fs::path projectRoot = root / "Project With Spaces";
    const fs::path outputRoot = root / "Build Output";
    const fs::path supportRoot = root / "Runtime Support";
    const fs::path fixtureRoot = fs::u8path(argv[1]);

    const std::string projectText =
        "project_id=11111111-1111-4111-8111-111111111111\n";
    const std::string screenText = "screen=main\n";
    const std::string levelOneText = "scene=level-one\n";
    const std::string levelTwoText = "scene=level-two\n";
    const std::string importedText = "imported=model\n";
    std::string error;

    if (!WriteFile(projectRoot / "ProofGame.renegade", projectText, error) ||
        !WriteFile(projectRoot / "Content/UI/Main.renegade-screen",
            screenText, error) ||
        !WriteFile(projectRoot / "Content/Scenes/LevelOne.wiscene",
            levelOneText, error) ||
        !WriteFile(projectRoot / "Content/Scenes/LevelTwo.wiscene",
            levelTwoText, error) ||
        !WriteFile(projectRoot / "Content/Imported/model.wiscene",
            importedText, error) ||
        !WriteFile(projectRoot / "Content/Unused/unused.txt",
            "must-not-be-cooked\n", error) ||
        !WriteFile(supportRoot / "RenegadeRuntime.exe",
            "runtime-binary-gate2\n", error) ||
        !WriteFile(supportRoot / "dxcompiler.dll",
            "dxc-binary-gate2\n", error))
    {
        fs::remove_all(root);
        return Fail(error);
    }

    const WindowsGameBuildPlan plan = Plan(
        projectText, screenText, levelOneText, levelTwoText, importedText);
    WindowsGameBuildStagingRequest request = Request(
        projectRoot, outputRoot, supportRoot, fixtureRoot, "stage-a");

    WindowsGameBuildStageResult first;
    if (!StageWindowsGameBuild(plan, request, first, error))
    {
        fs::remove_all(root);
        return Fail(error);
    }
    if (first.stagingPath.find(".renegade-staging") == std::string::npos ||
        fs::exists(fs::u8path(first.finalOutputPath)))
    {
        fs::remove_all(root);
        return Fail("Gate 2 did not isolate staging from the final build path");
    }
    if (first.files.size() != 19 ||
        Find(first, "ProofGame.exe") == nullptr ||
        Find(first, "dxcompiler.dll") == nullptr ||
        Find(first, "GameData/project.manifest.json") == nullptr ||
        Find(first, "GameData/content-manifest.json") == nullptr ||
        Find(first, "Engine/runtime-support-manifest.json") == nullptr ||
        Find(first, "Licences/Build-Component-Inventory.txt") == nullptr ||
        Find(first, "build-report.json") == nullptr ||
        Find(first, "package-manifest.json") == nullptr)
    {
        fs::remove_all(root);
        return Fail("Gate 2 staged tree did not contain the exact governed shell");
    }
    if (fs::exists(fs::u8path(first.stagingPath) /
            "GameData/Content/Unused/unused.txt"))
    {
        fs::remove_all(root);
        return Fail("Gate 2 cooked an unused project file outside the Gate 1 plan");
    }
    if (first.projectManifestJson.find(root.generic_u8string()) !=
            std::string::npos ||
        first.contentManifestJson.find(root.generic_u8string()) !=
            std::string::npos ||
        first.runtimeSupportManifestJson.find(root.generic_u8string()) !=
            std::string::npos ||
        first.packageManifestJson.find(root.generic_u8string()) !=
            std::string::npos)
    {
        fs::remove_all(root);
        return Fail("Gate 2 leaked machine-specific absolute paths into manifests");
    }
    if (!ValidateWindowsGameBuildStage(first, error))
    {
        fs::remove_all(root);
        return Fail(error);
    }

    WindowsGameBuildStagingRequest reordered = Request(
        projectRoot, outputRoot, supportRoot, fixtureRoot, "stage-b");
    std::reverse(reordered.runtimeSupportSources.begin(),
        reordered.runtimeSupportSources.end());
    std::reverse(reordered.packageDocuments.begin(),
        reordered.packageDocuments.end());
    WindowsGameBuildStageResult second;
    if (!StageWindowsGameBuild(plan, reordered, second, error))
    {
        fs::remove_all(root);
        return Fail(error);
    }
    if (first.projectManifestJson != second.projectManifestJson ||
        first.contentManifestJson != second.contentManifestJson ||
        first.runtimeSupportManifestJson != second.runtimeSupportManifestJson ||
        first.packageManifestJson != second.packageManifestJson ||
        first.projectManifestSha256 != second.projectManifestSha256 ||
        first.contentManifestSha256 != second.contentManifestSha256 ||
        first.runtimeSupportManifestSha256 != second.runtimeSupportManifestSha256 ||
        first.packageManifestSha256 != second.packageManifestSha256)
    {
        fs::remove_all(root);
        return Fail("unchanged Gate 2 inputs changed normalized manifest bytes");
    }

    WindowsGameBuildStageResult rejected;
    if (StageWindowsGameBuild(plan, reordered, rejected, error))
    {
        fs::remove_all(root);
        return Fail("Gate 2 reused a single-use staging ID");
    }

    auto missingSupport = Request(
        projectRoot, outputRoot, supportRoot, fixtureRoot, "missing-support");
    missingSupport.runtimeSupportSources.pop_back();
    if (StageWindowsGameBuild(plan, missingSupport, rejected, error))
    {
        fs::remove_all(root);
        return Fail("Gate 2 accepted an incomplete Runtime support source set");
    }

    auto missingNotice = Request(
        projectRoot, outputRoot, supportRoot, fixtureRoot, "missing-notice");
    missingNotice.packageDocuments.pop_back();
    if (StageWindowsGameBuild(plan, missingNotice, rejected, error))
    {
        fs::remove_all(root);
        return Fail("Gate 2 accepted a missing required notice input");
    }

    auto collidingNotice = Request(
        projectRoot, outputRoot, supportRoot, fixtureRoot, "notice-collision");
    collidingNotice.packageDocuments.push_back(
        {"LICENCES/WickedEngine-LICENSE.txt",
            (fixtureRoot / "Fixture-Legal-Notice.txt").generic_u8string(),
            "collision", "repo:test-fixture:lp06-gate2-v1"});
    if (StageWindowsGameBuild(plan, collidingNotice, rejected, error))
    {
        fs::remove_all(root);
        return Fail("Gate 2 accepted a Windows case-equivalent notice collision");
    }

    if (!WriteFile(projectRoot / "Content/Scenes/LevelTwo.wiscene",
            "scene=changed-after-plan\n", error))
    {
        fs::remove_all(root);
        return Fail(error);
    }
    auto staleProject = Request(
        projectRoot, outputRoot, supportRoot, fixtureRoot, "stale-project");
    if (StageWindowsGameBuild(plan, staleProject, rejected, error))
    {
        fs::remove_all(root);
        return Fail("Gate 2 copied project bytes that changed after Gate 1 planning");
    }
    if (!WriteFile(projectRoot / "Content/Scenes/LevelTwo.wiscene",
            levelTwoText, error))
    {
        fs::remove_all(root);
        return Fail(error);
    }

    if (!WriteFile(supportRoot / "dxcompiler.dll",
            "changed-dxc-after-plan\n", error))
    {
        fs::remove_all(root);
        return Fail(error);
    }
    auto staleRuntime = Request(
        projectRoot, outputRoot, supportRoot, fixtureRoot, "stale-runtime");
    if (StageWindowsGameBuild(plan, staleRuntime, rejected, error))
    {
        fs::remove_all(root);
        return Fail("Gate 2 copied Runtime support that changed after Gate 1 planning");
    }
    if (!WriteFile(supportRoot / "dxcompiler.dll",
            "dxc-binary-gate2\n", error))
    {
        fs::remove_all(root);
        return Fail(error);
    }

    const fs::path firstRoot = fs::u8path(first.stagingPath);
    if (!WriteFile(firstRoot / "injected-extra.txt", "extra\n", error))
    {
        fs::remove_all(root);
        return Fail(error);
    }
    if (ValidateWindowsGameBuildStage(first, error))
    {
        fs::remove_all(root);
        return Fail("Gate 2 validation accepted an unmanifested extra file");
    }
    fs::remove(firstRoot / "injected-extra.txt");

    const fs::path stagedLevel =
        firstRoot / "GameData/Content/Scenes/LevelOne.wiscene";
    if (!WriteFile(stagedLevel, "tampered\n", error))
    {
        fs::remove_all(root);
        return Fail(error);
    }
    if (ValidateWindowsGameBuildStage(first, error))
    {
        fs::remove_all(root);
        return Fail("Gate 2 validation accepted tampered staged content");
    }
    if (!WriteFile(stagedLevel, levelOneText, error) ||
        !ValidateWindowsGameBuildStage(first, error))
    {
        fs::remove_all(root);
        return Fail("Gate 2 validation did not recover after restoring staged bytes");
    }

    fs::remove(firstRoot / "ReadMe.txt");
    if (ValidateWindowsGameBuildStage(first, error))
    {
        fs::remove_all(root);
        return Fail("Gate 2 validation accepted a missing governed staged file");
    }

    if (fs::exists(fs::u8path(first.finalOutputPath)))
    {
        fs::remove_all(root);
        return Fail("Gate 2 created or promoted an owner-visible final build");
    }

    fs::remove_all(root);
    std::cout << "PASS: LP06 Gate 2 clean loose cooker and governed staging\n";
    return 0;
}
