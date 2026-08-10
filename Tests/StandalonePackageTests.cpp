#include "RuntimeBootstrap.h"
#include "RuntimeFlow.h"

#include "renegade/bridge/BuildIdentityService.h"
#include "renegade/bridge/BuildStageService.h"
#include "renegade/bridge/BuildVerificationService.h"
#include "renegade/bridge/PackageIntegrityService.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade;
    using namespace renegade::bridge;

    constexpr const char* ProjectId =
        "61111111-1111-4111-8111-111111111111";
    constexpr const char* SaveDataId =
        "69999999-9999-4999-8999-999999999999";

    struct SourceFile
    {
        std::string path;
        StableId assetId;
        DependencyClass dependencyClass = DependencyClass::Data;
    };

    int Fail(const fs::path& root, const std::string& message)
    {
        std::error_code ignored;
        fs::remove_all(root, ignored);
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    bool ReadFile(
        const fs::path& path,
        std::string& bytes,
        std::string& error)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = "could not read Gate 4 fixture: " + path.generic_u8string();
            return false;
        }
        bytes.assign(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
        if (input.bad())
        {
            error = "could not complete Gate 4 fixture read: " +
                path.generic_u8string();
            return false;
        }
        error.clear();
        return true;
    }

    bool WriteFile(
        const fs::path& path,
        const std::string& bytes,
        std::string& error)
    {
        std::error_code ec;
        if (!path.parent_path().empty())
            fs::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error = "could not create Gate 4 fixture directory";
            return false;
        }
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "could not create Gate 4 fixture file: " +
                path.generic_u8string();
            return false;
        }
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output)
        {
            error = "could not write Gate 4 fixture file: " +
                path.generic_u8string();
            return false;
        }
        error.clear();
        return true;
    }

    bool CopyBytes(
        const fs::path& source,
        const fs::path& destination,
        std::string& bytes,
        std::string& error)
    {
        return ReadFile(source, bytes, error) &&
            WriteFile(destination, bytes, error);
    }

    std::string Fnv1a64(const std::string& bytes)
    {
        std::uint64_t hash = 1469598103934665603ull;
        for (const unsigned char value : bytes)
        {
            hash ^= value;
            hash *= 1099511628211ull;
        }
        std::ostringstream stream;
        stream << "fnv1a64:" << std::hex << std::setfill('0')
               << std::setw(16) << hash;
        return stream.str();
    }

    void AppendWord(
        std::vector<std::uint8_t>& bytes,
        const std::uint16_t value)
    {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
        bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    }

    void AppendDword(
        std::vector<std::uint8_t>& bytes,
        const std::uint32_t value)
    {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
        bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
        bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
        bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
    }

    bool WriteIcon(const fs::path& path, std::string& error)
    {
        std::vector<std::uint8_t> bytes;
        AppendWord(bytes, 0);
        AppendWord(bytes, 1);
        AppendWord(bytes, 1);
        bytes.push_back(1);
        bytes.push_back(1);
        bytes.push_back(0);
        bytes.push_back(0);
        AppendWord(bytes, 1);
        AppendWord(bytes, 32);
        AppendDword(bytes, 48);
        AppendDword(bytes, 22);
        AppendDword(bytes, 40);
        AppendDword(bytes, 1);
        AppendDword(bytes, 2);
        AppendWord(bytes, 1);
        AppendWord(bytes, 32);
        AppendDword(bytes, 0);
        AppendDword(bytes, 4);
        AppendDword(bytes, 0);
        AppendDword(bytes, 0);
        AppendDword(bytes, 0);
        AppendDword(bytes, 0);
        bytes.push_back(0x20);
        bytes.push_back(0x80);
        bytes.push_back(0xff);
        bytes.push_back(0xff);
        AppendDword(bytes, 0);
        return WriteFile(
            path,
            std::string(
                reinterpret_cast<const char*>(bytes.data()),
                bytes.size()),
            error);
    }

    std::vector<WindowsPackageDocumentInput> Documents(
        const fs::path& fixtureRoot)
    {
        const std::string readme =
            (fixtureRoot / "ReadMe.txt").generic_u8string();
        const std::string legal =
            (fixtureRoot / "Fixture-Legal-Notice.txt").generic_u8string();
        return {
            {"ReadMe.txt", readme, "gate4-test-readme",
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

    WindowsGameBuildFile ProjectFile(
        const SourceFile& source,
        const std::string& bytes)
    {
        WindowsGameBuildFile file;
        file.kind = WindowsGameBuildFileKind::ProjectContent;
        file.destinationPath = "GameData/" + source.path;
        file.projectRelativeSourcePath = source.path;
        file.assetId = source.assetId;
        file.dependencyClass = source.dependencyClass;
        file.requirement = DependencyRequirement::Required;
        file.sourceContentHash = Fnv1a64(bytes);
        file.provenance = {"lp06:gate4:isolated-standalone-fixture"};
        return file;
    }

    WindowsGameBuildFile RuntimeFile(
        std::string destination,
        std::string logicalName,
        const WindowsGamePackageFileDigest& digest)
    {
        WindowsGameBuildFile file;
        file.kind = WindowsGameBuildFileKind::RuntimeSupport;
        file.destinationPath = std::move(destination);
        file.runtimeSupportName = std::move(logicalName);
        file.byteCount = digest.byteCount;
        file.sha256 = digest.sha256;
        file.provenance = {"lp06:gate4:actual-runtime"};
        return file;
    }

    std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty())
            return {};
        const int required = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);
        if (required <= 0)
            return std::wstring(value.begin(), value.end());
        std::wstring result(static_cast<std::size_t>(required), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            required);
        return result;
    }

    std::wstring Quote(const std::string& value)
    {
        return L"\"" + Utf8ToWide(value) + L"\"";
    }

    bool RunProcess(
        const fs::path& executable,
        const std::vector<std::string>& arguments,
        const fs::path& workingDirectory,
        const DWORD timeoutMs,
        DWORD& exitCode,
        std::string& error)
    {
        std::wstring command = Quote(executable.generic_u8string());
        for (const std::string& argument : arguments)
        {
            command += L" ";
            command += Quote(argument);
        }
        std::vector<wchar_t> mutableCommand(command.begin(), command.end());
        mutableCommand.push_back(L'\0');

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION process{};
        const std::wstring cwd = workingDirectory.wstring();
        if (!CreateProcessW(
                nullptr,
                mutableCommand.data(),
                nullptr,
                nullptr,
                FALSE,
                0,
                nullptr,
                cwd.c_str(),
                &startup,
                &process))
        {
            error = "CreateProcessW failed with error " +
                std::to_string(GetLastError());
            return false;
        }

        const DWORD wait = WaitForSingleObject(process.hProcess, timeoutMs);
        if (wait != WAIT_OBJECT_0)
        {
            TerminateProcess(process.hProcess, 0xEEu);
            WaitForSingleObject(process.hProcess, 5000);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            error = wait == WAIT_TIMEOUT
                ? "packaged Runtime timed out"
                : "WaitForSingleObject failed";
            return false;
        }
        if (!GetExitCodeProcess(process.hProcess, &exitCode))
        {
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            error = "could not read packaged Runtime exit code";
            return false;
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        error.clear();
        return true;
    }

    bool ReadEvidence(
        const fs::path& path,
        std::map<std::string, std::string>& values,
        std::string& error)
    {
        std::string text;
        if (!ReadFile(path, text, error))
            return false;
        values.clear();
        std::size_t begin = 0;
        while (begin <= text.size())
        {
            const std::size_t end = text.find('\n', begin);
            std::string line = text.substr(
                begin,
                end == std::string::npos ? std::string::npos : end - begin);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!line.empty())
            {
                const std::size_t equals = line.find('=');
                if (equals == std::string::npos || equals == 0)
                {
                    error = "Runtime evidence contains malformed line";
                    return false;
                }
                values[line.substr(0, equals)] = line.substr(equals + 1);
            }
            if (end == std::string::npos)
                break;
            begin = end + 1;
        }
        error.clear();
        return true;
    }

    bool HasAppLocalVcRuntime(const fs::path& packageRoot)
    {
        std::error_code ec;
        for (fs::directory_iterator iterator(packageRoot, ec), end;
             !ec && iterator != end;
             iterator.increment(ec))
        {
            if (!iterator->is_regular_file())
                continue;
            std::string name = iterator->path().filename().generic_u8string();
            std::transform(
                name.begin(),
                name.end(),
                name.begin(),
                [](const unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
            if (name == "ucrtbase.dll" ||
                name.rfind("vcruntime", 0) == 0 ||
                name.rfind("msvcp", 0) == 0 ||
                name.rfind("concrt", 0) == 0)
            {
                return true;
            }
        }
        return false;
    }

    bool CopyTree(
        const fs::path& source,
        const fs::path& destination,
        std::string& error)
    {
        std::error_code ec;
        fs::create_directories(destination.parent_path(), ec);
        if (ec)
        {
            error = "could not create package variant parent";
            return false;
        }
        fs::copy(source, destination, fs::copy_options::recursive, ec);
        if (ec)
        {
            error = "could not copy package variant: " + ec.message();
            return false;
        }
        error.clear();
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc != 7)
    {
        std::cerr <<
            "Gate 4 expects Runtime, dxcompiler, LP03 fixture root, cube WISCENE, "
            "Gate2 package-doc fixture root, and repo root arguments.\n";
        return 2;
    }

    const fs::path runtimePath = fs::absolute(fs::u8path(argv[1]));
    const fs::path dxcPath = fs::absolute(fs::u8path(argv[2]));
    const fs::path lp03Root = fs::absolute(fs::u8path(argv[3]));
    const fs::path cubePath = fs::absolute(fs::u8path(argv[4]));
    const fs::path packageDocRoot = fs::absolute(fs::u8path(argv[5]));
    const fs::path repoRoot = fs::absolute(fs::u8path(argv[6]));
    const auto nonce = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path(u8"Renegade LP06 Gate4 Standalone Ω " +
            std::to_string(nonce));
    const fs::path projectRoot = root / "Disposable Source Project";
    const fs::path outputRoot = root / "Build Output";
    const fs::path supportRoot = root / "Identity Support";
    const fs::path movedPackage =
        root / fs::u8path(u8"Detached Consumer Location Ω") /
        "Proof Game Windows Build";
    const fs::path unrelatedCwd = root / "Unrelated Current Directory";
    const fs::path localAppData = root / "Consumer Local App Data";
    const fs::path iconPath = supportRoot / "ProofGame.ico";
    std::string error;

    if (!fs::is_regular_file(runtimePath) ||
        !fs::is_regular_file(dxcPath) ||
        !fs::is_directory(lp03Root) ||
        !fs::is_regular_file(cubePath) ||
        !fs::is_directory(packageDocRoot))
    {
        return Fail(root, "required real Runtime/Gate 4 fixture input is missing");
    }

    const std::vector<SourceFile> sources = {
        {"ScreenProject.renegade",
            "71000000-0000-4000-8000-000000000001",
            DependencyClass::ProjectDocument},
        {"Content/Flow/Main.renegade-flow",
            "71000000-0000-4000-8000-000000000002",
            DependencyClass::StoryFlowDocument},
        {"Content/UI/Main.renegade-screen",
            "71000000-0000-4000-8000-000000000003",
            DependencyClass::RuntimeScreenDocument},
        {"Content/UI/renegade-runtime-background.png",
            "71000000-0000-4000-8000-000000000004",
            DependencyClass::Texture},
        {"Content/Scenes/LevelOne.wiscene",
            "71000000-0000-4000-8000-000000000005",
            DependencyClass::Scene},
        {"Content/Scenes/LevelOne.wiscene.rmeta",
            "71000000-0000-4000-8000-000000000006",
            DependencyClass::GeneratedData},
        {"Content/Scenes/LevelTwo.wiscene",
            "71000000-0000-4000-8000-000000000007",
            DependencyClass::Scene},
        {"Content/Scenes/LevelTwo.wiscene.rmeta",
            "71000000-0000-4000-8000-000000000008",
            DependencyClass::GeneratedData},
    };

    std::map<std::string, std::string> sourceBytes;
    for (const SourceFile& source : sources)
    {
        const bool isScene =
            source.path == "Content/Scenes/LevelOne.wiscene" ||
            source.path == "Content/Scenes/LevelTwo.wiscene";
        const fs::path sourcePath = isScene
            ? cubePath
            : lp03Root / fs::u8path(source.path);
        std::string bytes;
        if (!CopyBytes(
                sourcePath,
                projectRoot / fs::u8path(source.path),
                bytes,
                error))
        {
            return Fail(root, error);
        }
        sourceBytes.emplace(source.path, std::move(bytes));
    }
    if (!WriteIcon(iconPath, error))
        return Fail(root, error);

    WindowsGamePackageFileDigest runtimeDigest;
    WindowsGamePackageFileDigest dxcDigest;
    if (!DigestWindowsGamePackageFile(
            runtimePath.generic_u8string(), runtimeDigest, error) ||
        !DigestWindowsGamePackageFile(
            dxcPath.generic_u8string(), dxcDigest, error))
    {
        return Fail(root, error);
    }

    WindowsGameBuildPlan plan;
    plan.projectId = ProjectId;
    plan.gameName = "Proof Game";
    plan.executableFileName = "ProofGame.exe";
    plan.buildFolderName = "Proof Game Windows Build";
    plan.publicVersion = "0.1.0-gate4";
    plan.saveDataId = SaveDataId;
    for (const SourceFile& source : sources)
        plan.files.push_back(ProjectFile(source, sourceBytes.at(source.path)));
    plan.files.push_back(RuntimeFile(
        "ProofGame.exe", "renegade-runtime", runtimeDigest));
    plan.files.push_back(RuntimeFile(
        "dxcompiler.dll", "directx-shader-compiler", dxcDigest));

    WindowsGameBuildStagingRequest stageRequest;
    stageRequest.projectRootPath = projectRoot.generic_u8string();
    stageRequest.outputParentPath = outputRoot.generic_u8string();
    stageRequest.stagingId = "gate4-isolated";
    stageRequest.renegadeRevision =
        "6db154978e6e4c9dfa8afcf67b0927d850993676";
    stageRequest.wickedRevision =
        "3a800b7134aafe58461093c8abb2e274d4e64033";
    stageRequest.runtimeSupportSources = {
        {"ProofGame.exe", runtimePath.generic_u8string()},
        {"dxcompiler.dll", dxcPath.generic_u8string()},
    };
    stageRequest.packageDocuments = Documents(packageDocRoot);

    WindowsGameBuildStageResult stage;
    if (!StageWindowsGameBuild(plan, stageRequest, stage, error))
        return Fail(root, error);

    WindowsGameExecutableIdentityRequest identityRequest;
    identityRequest.developerPublisher = "Maverick Media Studio";
    identityRequest.description = "Proof Game Gate 4 standalone";
    identityRequest.copyrightNotice =
        "Copyright 2026 Maverick Media Studio";
    identityRequest.internalBuildId = "6db15497-gate4-proof";
    identityRequest.buildTimestampUtc = "2026-08-10T09:00:00Z";
    identityRequest.iconSourcePath = iconPath.generic_u8string();
    WindowsGameExecutableIdentityResult identity;
    if (!ApplyWindowsGameExecutableIdentity(
            plan, identityRequest, stage, identity, error))
    {
        return Fail(root, error);
    }
    if (!ValidateWindowsGameBuildStage(stage, error))
        return Fail(root, error);

    auto testAll = runtime::ParseRuntimeLaunchArguments({
        "--project",
        (projectRoot / "ScreenProject.renegade").generic_u8string(),
        "--flow-outcome=level.complete",
        "--flow-outcome=level.complete",
    });
    testAll = runtime::ResolveRuntimeProject(std::move(testAll));
    bridge::SceneService testAllScenes;
    runtime::RuntimeFlowController testAllFlow;
    testAll = runtime::LoadRuntimeProjectFlow(
        testAllScenes, testAllFlow, std::move(testAll));
    if (!testAll.succeeded ||
        testAll.flowTerminalAction != FlowTerminalAction::CompleteGame ||
        testAll.flowTrace.size() != 4)
    {
        return Fail(root,
            "explicit-project Test All flow did not reach Complete Game");
    }
    const std::vector<std::string> expectedTrace = testAll.flowTrace;

    std::error_code ec;
    fs::create_directories(movedPackage.parent_path(), ec);
    if (ec)
        return Fail(root, "could not create moved-package destination");
    fs::rename(fs::u8path(stage.stagingPath), movedPackage, ec);
    if (ec)
        return Fail(root, "could not move staged package: " + ec.message());
    fs::remove_all(projectRoot, ec);
    if (ec || fs::exists(projectRoot))
        return Fail(root, "could not remove source project before isolated launch");
    fs::create_directories(unrelatedCwd, ec);
    if (ec)
        return Fail(root, "could not create unrelated current directory");
    fs::create_directories(localAppData, ec);
    if (ec)
        return Fail(root, "could not create isolated LocalAppData directory");

    WindowsGamePackageIntegrityResult movedIntegrity;
    if (!ValidateWindowsGamePackage(
            movedPackage.generic_u8string(), movedIntegrity, error))
    {
        return Fail(root, "moved package failed exact validation: " + error);
    }
    if (HasAppLocalVcRuntime(movedPackage))
    {
        return Fail(root,
            "Gate 4 package unexpectedly carries app-local Microsoft VC Runtime DLLs");
    }
    if (fs::exists(fs::u8path(stage.finalOutputPath)))
        return Fail(root, "Gate 4 created an owner-visible final build before Gate 5");

    const std::wstring localAppDataWide = localAppData.wstring();
    if (!SetEnvironmentVariableW(L"LOCALAPPDATA", localAppDataWide.c_str()))
        return Fail(root, "could not isolate packaged Runtime LOCALAPPDATA");

    const fs::path namedExecutable = movedPackage / "ProofGame.exe";
    DWORD exitCode = 0;
    if (!RunProcess(
            namedExecutable,
            {"dx12",
             "--flow-outcome=level.complete",
             "--flow-outcome=level.complete",
             "--renegade-smoke-autoplay",
             "--renegade-smoke-exit"},
            unrelatedCwd,
            120000,
            exitCode,
            error))
    {
        return Fail(root,
            "real isolated DX12 packaged Runtime failed: " + error);
    }
    if (exitCode != 0)
    {
        return Fail(root,
            "real isolated DX12 packaged Runtime returned " +
            std::to_string(exitCode));
    }

    const fs::path evidencePath =
        localAppData / "RenegadeEngine" / ProjectId /
        "Logs/RuntimeBootstrap.log";
    WindowsGameBuildVerificationRequest verificationRequest;
    verificationRequest.packageRootPath = movedPackage.generic_u8string();
    verificationRequest.runtimeEvidencePath = evidencePath.generic_u8string();
    verificationRequest.expectedGraphicsBackend = "DX12";
    verificationRequest.expectedFlowTrace = expectedTrace;
    WindowsGameBuildVerificationResult verified;
    if (!RecordWindowsGameBuildVerification(
            verificationRequest, verified, error))
    {
        return Fail(root,
            "Gate 4 rejected real Runtime evidence: " + error);
    }
    if (!verified.succeeded ||
        verified.runtimeEvidenceSha256.empty() ||
        verified.buildReportSha256.empty() ||
        verified.packageManifestSha256.empty() ||
        fs::exists(fs::u8path(stage.finalOutputPath)))
    {
        return Fail(root,
            "Gate 4 verification did not remain staged/unpromoted");
    }

    std::string reportText;
    if (!ReadFile(movedPackage / "build-report.json", reportText, error) ||
        reportText.find("isolated_smoke_passed_not_promoted") ==
            std::string::npos ||
        reportText.find("passed_gate4") == std::string::npos ||
        reportText.find(WindowsVcRuntimePrerequisitePolicy) ==
            std::string::npos)
    {
        return Fail(root,
            "Gate 4 build report did not record isolated smoke proof");
    }

    std::string packageManifestText;
    if (!ReadFile(
            movedPackage / "package-manifest.json",
            packageManifestText,
            error))
    {
        return Fail(root, error);
    }
    const std::string repoText = repoRoot.generic_u8string();
    const std::string sourceText = projectRoot.generic_u8string();
    if (packageManifestText.find(repoText) != std::string::npos ||
        packageManifestText.find(sourceText) != std::string::npos ||
        reportText.find(repoText) != std::string::npos ||
        reportText.find(sourceText) != std::string::npos)
    {
        return Fail(root,
            "Gate 4 normalized package evidence leaked repository/source paths");
    }

    if (!RunProcess(
            namedExecutable,
            {"vulkan", "--renegade-capability-probe"},
            unrelatedCwd,
            30000,
            exitCode,
            error))
    {
        return Fail(root,
            "Vulkan capability probe process failed: " + error);
    }
    if (exitCode != 0 &&
        exitCode != static_cast<DWORD>(
            runtime::RuntimeBootstrapCode::GraphicsPrerequisiteMissing))
    {
        return Fail(root,
            "Vulkan capability probe returned unexpected code " +
            std::to_string(exitCode));
    }

    std::map<std::string, std::string> capabilityEvidence;
    if (!ReadEvidence(evidencePath, capabilityEvidence, error) ||
        capabilityEvidence["graphics_backend_requested"] != "Vulkan" ||
        (capabilityEvidence["graphics_capability"] !=
             "VULKAN_LOADER_AVAILABLE" &&
         capabilityEvidence["graphics_capability"] !=
             "VULKAN_LOADER_MISSING"))
    {
        return Fail(root, "Vulkan capability evidence was not explicit");
    }

    const fs::path variantsRoot = root / "Corrupt Package Variants";
    const std::array<std::string, 3> variantNames = {
        "Tampered", "Missing", "Unmanifested"};
    for (const std::string& variantName : variantNames)
    {
        const fs::path variant = variantsRoot / variantName;
        if (!CopyTree(movedPackage, variant, error))
            return Fail(root, error);

        if (variantName == "Tampered")
        {
            if (!WriteFile(
                    variant / "GameData/Content/Flow/Main.renegade-flow",
                    "tampered-gate4\n",
                    error))
            {
                return Fail(root, error);
            }
        }
        else if (variantName == "Missing")
        {
            fs::remove(
                variant / "GameData/Content/Scenes/LevelTwo.wiscene",
                ec);
            if (ec)
                return Fail(root, "could not remove missing-content fixture");
        }
        else
        {
            if (!WriteFile(
                    variant / "GameData/Content/injected-extra.txt",
                    "unmanifested\n",
                    error))
            {
                return Fail(root, error);
            }
        }

        const fs::path variantExe = variant / "ProofGame.exe";
        if (!RunProcess(
                variantExe,
                {"dx12", "--renegade-capability-probe"},
                unrelatedCwd,
                30000,
                exitCode,
                error))
        {
            return Fail(root,
                variantName + " package process failed: " + error);
        }
        if (exitCode != static_cast<DWORD>(
                runtime::RuntimeBootstrapCode::PackageIntegrityFailed))
        {
            return Fail(root,
                variantName +
                " package did not fail with PACKAGE_INTEGRITY_FAILED");
        }

        const fs::path failureEvidence =
            localAppData / "RenegadeEngine" / "unknown-package" /
            "Logs/RuntimeBootstrap.log";
        std::map<std::string, std::string> failedEvidence;
        if (!ReadEvidence(failureEvidence, failedEvidence, error) ||
            failedEvidence["code"] != "PACKAGE_INTEGRITY_FAILED" ||
            failedEvidence["package_integrity"] != "FAIL")
        {
            return Fail(root,
                variantName +
                " package did not emit clear integrity evidence");
        }
    }

    std::error_code ignored;
    fs::remove_all(root, ignored);
    std::cout
        << "PASS: LP06 Gate 4 real moved-package DX12 smoke, Test All parity, "
           "Vulkan capability and integrity failure proof\n";
    return 0;
}
