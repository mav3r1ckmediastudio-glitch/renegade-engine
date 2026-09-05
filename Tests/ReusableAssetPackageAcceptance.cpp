#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/ImportService.h"
#include "renegade/bridge/PackageIntegrityService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/WindowsGameBuildProjectService.h"
#include "renegade/bridge/WindowsGameBuildWorkflow.h"

#include <WickedEngine.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    using namespace renegade::bridge;

    constexpr wchar_t WindowClassName[] =
        L"RenegadeLP07Gate6PackageAcceptanceWindow";
    constexpr const char* ProjectId =
        "61111111-1111-4111-8111-111111111111";
    constexpr const char* GameName = "Gate6Proof";
#ifdef _DEBUG
    constexpr DWORD RuntimeTimeoutMs = 300000;
#else
    constexpr DWORD RuntimeTimeoutMs = 120000;
#endif

    LRESULT CALLBACK WindowProc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        if (message == WM_CLOSE)
        {
            DestroyWindow(window);
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "LP07 GATE 6 PACKAGE ACCEPTANCE FAIL // "
                  << message << '\n';
        return false;
    }

    bool ReadBytes(const fs::path& path, std::vector<std::uint8_t>& bytes)
    {
        bytes.clear();
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return false;
        bytes.assign(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
        return input.good() || input.eof();
    }

    bool ReadText(const fs::path& path, std::string& text)
    {
        text.clear();
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return false;
        text.assign(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
        return input.good() || input.eof();
    }

    bool WriteText(const fs::path& path, const std::string& text)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
            return false;
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(output);
    }

    bool CopyTreeContents(
        const fs::path& source,
        const fs::path& destination,
        std::string& error)
    {
        std::error_code ec;
        fs::create_directories(destination, ec);
        if (ec)
        {
            error = "could not create project fixture root: " + ec.message();
            return false;
        }
        for (fs::recursive_directory_iterator it(source, ec), end;
            !ec && it != end; it.increment(ec))
        {
            const fs::path relative = it->path().lexically_relative(source);
            const fs::path target = destination / relative;
            if (it->is_directory(ec))
            {
                if (ec) break;
                fs::create_directories(target, ec);
            }
            else if (it->is_regular_file(ec))
            {
                if (ec) break;
                fs::create_directories(target.parent_path(), ec);
                if (ec) break;
                fs::copy_file(
                    it->path(), target,
                    fs::copy_options::overwrite_existing, ec);
            }
            if (ec) break;
        }
        if (ec)
        {
            error = "could not copy project fixture tree: " + ec.message();
            return false;
        }
        error.clear();
        return true;
    }

    const AssetCatalogueEntry* FindCatalogueEntry(
        const AssetCatalogue& catalogue,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            catalogue.entries.begin(), catalogue.entries.end(),
            [&assetId](const AssetCatalogueEntry& entry)
            {
                return entry.registered && entry.assetId == assetId;
            });
        return found == catalogue.entries.end() ? nullptr : &*found;
    }

    bool CopyFileOver(
        const fs::path& source,
        const fs::path& destination,
        std::string& error)
    {
        std::error_code ec;
        fs::copy_file(
            source, destination,
            fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            error = "could not replace retained source fixture: " + ec.message();
            return false;
        }
        error.clear();
        return true;
    }

    void WriteU16(std::ofstream& output, const std::uint16_t value)
    {
        const std::array<unsigned char, 2> bytes = {
            static_cast<unsigned char>(value & 0xffu),
            static_cast<unsigned char>((value >> 8u) & 0xffu),
        };
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }

    void WriteU32(std::ofstream& output, const std::uint32_t value)
    {
        const std::array<unsigned char, 4> bytes = {
            static_cast<unsigned char>(value & 0xffu),
            static_cast<unsigned char>((value >> 8u) & 0xffu),
            static_cast<unsigned char>((value >> 16u) & 0xffu),
            static_cast<unsigned char>((value >> 24u) & 0xffu),
        };
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }

    bool WriteIcon(const fs::path& path, std::string& error)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error = "could not create icon directory";
            return false;
        }
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "could not create Gate 6 icon";
            return false;
        }
        WriteU16(output, 0);
        WriteU16(output, 1);
        WriteU16(output, 1);
        output.put(1);
        output.put(1);
        output.put(0);
        output.put(0);
        WriteU16(output, 1);
        WriteU16(output, 32);
        WriteU32(output, 48);
        WriteU32(output, 22);
        WriteU32(output, 40);
        WriteU32(output, 1);
        WriteU32(output, 2);
        WriteU16(output, 1);
        WriteU16(output, 32);
        WriteU32(output, 0);
        WriteU32(output, 4);
        WriteU32(output, 0);
        WriteU32(output, 0);
        WriteU32(output, 0);
        WriteU32(output, 0);
        output.put(static_cast<char>(0x20));
        output.put(static_cast<char>(0x80));
        output.put(static_cast<char>(0xff));
        output.put(static_cast<char>(0xff));
        WriteU32(output, 0);
        output.close();
        if (!output)
        {
            error = "could not finish Gate 6 icon";
            return false;
        }
        error.clear();
        return true;
    }

    std::vector<WindowsPackageDocumentInput> PackageDocuments(
        const fs::path& fixtureRoot)
    {
        const std::string readme =
            (fixtureRoot / "ReadMe.txt").generic_u8string();
        const std::string legal =
            (fixtureRoot / "Fixture-Legal-Notice.txt").generic_u8string();
        return {
            {"ReadMe.txt", readme, "gate6-test-readme",
                "repo:test-fixture:lp07-gate6-v1"},
            {"Licences/Renegade-Licence-or-Notice.txt", legal,
                "renegade-test-policy", "repo:test-fixture:lp07-gate6-v1"},
            {"Licences/WickedEngine-LICENSE.txt", legal,
                "wicked-test-notice", "repo:test-fixture:lp07-gate6-v1"},
            {"Licences/WickedEngine-third_party_software.txt", legal,
                "wicked-third-party-test-notice",
                "repo:test-fixture:lp07-gate6-v1"},
            {"Licences/DirectXShaderCompiler-LICENSE.txt", legal,
                "dxc-test-notice", "repo:test-fixture:lp07-gate6-v1"},
            {"Licences/DirectXShaderCompiler-ThirdPartyNotices.txt", legal,
                "dxc-third-party-test-notice",
                "repo:test-fixture:lp07-gate6-v1"},
        };
    }

    bool AddRuntimeSupport(
        const std::string& logicalName,
        const std::string& destination,
        const fs::path& source,
        const std::string& provenance,
        std::vector<WindowsRuntimeSupportInput>& planInputs,
        std::vector<WindowsRuntimeSupportSource>& stageInputs,
        std::string& error)
    {
        WindowsGamePackageFileDigest digest;
        if (!DigestWindowsGamePackageFile(
                source.generic_u8string(), digest, error))
            return false;
        planInputs.push_back({
            logicalName,
            destination,
            digest.byteCount,
            digest.sha256,
            provenance,
        });
        stageInputs.push_back({destination, source.generic_u8string()});
        return true;
    }

    bool MakeWorkflowRequest(
        const ProjectMetadata& project,
        const WindowsGameBuildProjectState& state,
        const fs::path& runtimePath,
        const fs::path& dxcPath,
        const fs::path& packageDocRoot,
        const fs::path& outputParent,
        const fs::path& iconPath,
        const std::string& renegadeRevision,
        const std::string& wickedRevision,
        const std::string& stagingId,
        WindowsGameBuildWorkflowRequest& request,
        std::string& error)
    {
        request = {};
        request.project = project;
        request.dependencyGraph = state.dependencyGraph;
        request.assetRegistry = state.assetRegistry;
        request.build.gameName = GameName;
        request.build.executableBaseName = GameName;
        request.build.publicVersion = "0.1.0-lp07-gate6";
        request.build.saveDataId = project.projectId;
        request.build.platform = "windows-x64";
        request.build.configuration = "Release";

        if (!AddRuntimeSupport(
                "renegade-runtime",
                std::string(GameName) + ".exe",
                runtimePath,
                "repo:" + renegadeRevision,
                request.runtimeSupport,
                request.staging.runtimeSupportSources,
                error) ||
            !AddRuntimeSupport(
                "directx-shader-compiler",
                "dxcompiler.dll",
                dxcPath,
                "pinned:" + wickedRevision,
                request.runtimeSupport,
                request.staging.runtimeSupportSources,
                error))
        {
            return false;
        }

        request.staging.projectRootPath = project.rootPath;
        request.staging.outputParentPath = outputParent.generic_u8string();
        request.staging.stagingId = stagingId;
        request.staging.renegadeRevision = renegadeRevision;
        request.staging.wickedRevision = wickedRevision;
        request.staging.packageDocuments = PackageDocuments(packageDocRoot);

        request.identity.developerPublisher = "Maverick Media Studio";
        request.identity.description = "LP07 Gate 6 reusable asset package proof";
        request.identity.copyrightNotice =
            "Copyright 2026 Maverick Media Studio";
        request.identity.internalBuildId =
            "lp07-gate6-" + renegadeRevision.substr(0, 12) + "-" + stagingId;
        request.identity.buildTimestampUtc = "2026-08-12T08:00:00Z";
        request.identity.iconSourcePath = iconPath.generic_u8string();

        request.verification.expectedGraphicsBackend = "DX12";
        request.verification.windowsPrerequisitePolicy =
            WindowsVcRuntimePrerequisitePolicy;
        request.verification.expectedFlowTrace = state.expectedFlowTrace;
        error.clear();
        return true;
    }

    std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty())
            return {};
        const int required = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()),
            nullptr, 0);
        if (required <= 0)
            return std::wstring(value.begin(), value.end());
        std::wstring result(static_cast<std::size_t>(required), L'\0');
        MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()),
            result.data(), required);
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

        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        if (job == nullptr)
        {
            error = "could not create packaged Runtime job object";
            return false;
        }
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(
                job, JobObjectExtendedLimitInformation,
                &limits, sizeof(limits)))
        {
            CloseHandle(job);
            error = "could not configure packaged Runtime job object";
            return false;
        }

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION process{};
        const std::wstring cwd = workingDirectory.wstring();
        if (!CreateProcessW(
                executable.wstring().c_str(),
                mutableCommand.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                nullptr,
                cwd.c_str(),
                &startup,
                &process))
        {
            const DWORD code = GetLastError();
            CloseHandle(job);
            error = "could not launch packaged named Runtime (Win32 " +
                std::to_string(code) + ")";
            return false;
        }
        if (!AssignProcessToJobObject(job, process.hProcess))
        {
            TerminateProcess(process.hProcess, 1);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            CloseHandle(job);
            error = "could not contain packaged Runtime process";
            return false;
        }
        if (ResumeThread(process.hThread) == static_cast<DWORD>(-1))
        {
            TerminateJobObject(job, 1);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            CloseHandle(job);
            error = "could not start packaged Runtime process";
            return false;
        }

        const DWORD wait = WaitForSingleObject(
            process.hProcess, RuntimeTimeoutMs);
        if (wait != WAIT_OBJECT_0)
        {
            TerminateJobObject(job, 1);
            WaitForSingleObject(process.hProcess, 5000);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            CloseHandle(job);
            error = wait == WAIT_TIMEOUT
                ? "packaged Runtime timed out"
                : "packaged Runtime wait failed";
            return false;
        }
        if (!GetExitCodeProcess(process.hProcess, &exitCode))
        {
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            CloseHandle(job);
            error = "could not read packaged Runtime exit code";
            return false;
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(job);
        error.clear();
        return true;
    }

    std::vector<std::string> SmokeArguments(const std::size_t completionCount)
    {
        std::vector<std::string> arguments = {"dx12"};
        for (std::size_t index = 0; index < completionCount; ++index)
            arguments.push_back("--flow-outcome=level.complete");
        arguments.push_back("--renegade-smoke-autoplay");
        arguments.push_back("--renegade-smoke-exit");
        return arguments;
    }

    bool ValidateRuntimeEvidence(
        const fs::path& evidencePath,
        const StableId& assetId,
        const std::string& currentPayloadHash,
        const std::string& previousPayloadHash,
        std::string& error)
    {
        std::string text;
        if (!ReadText(evidencePath, text))
        {
            error = "could not read packaged Runtime Gate 6 evidence";
            return false;
        }
        if (text.find("status=PASS") == std::string::npos ||
            text.find("package_relative_launch=true") == std::string::npos ||
            text.find("reusable_asset_instances_discovered=1") ==
                std::string::npos ||
            text.find("reusable_asset_instances_refreshed=1") ==
                std::string::npos ||
            text.find("asset_id=" + assetId) == std::string::npos ||
            text.find("payload_hash=" + currentPayloadHash) ==
                std::string::npos ||
            (!previousPayloadHash.empty() &&
                previousPayloadHash != currentPayloadHash &&
                text.find("payload_hash=" + previousPayloadHash) !=
                    std::string::npos))
        {
            error =
                "packaged Runtime evidence did not prove the current reimported RAsset payload";
            return false;
        }
        error.clear();
        return true;
    }

    bool GraphContainsAssetPath(
        const DependencyGraph& graph,
        const std::string& path,
        const DependencyRequirement requirement)
    {
        return std::any_of(
            graph.nodes.begin(), graph.nodes.end(),
            [&path, requirement](const DependencyNode& node)
            {
                return node.projectRelativePath == path &&
                    node.requirement == requirement;
            });
    }

    bool PlanContainsAsset(
        const WindowsGameBuildPlan& plan,
        const StableId& assetId,
        const std::string& path)
    {
        return std::any_of(
            plan.files.begin(), plan.files.end(),
            [&assetId, &path](const WindowsGameBuildFile& file)
            {
                return file.kind == WindowsGameBuildFileKind::ProjectContent &&
                    file.assetId == assetId &&
                    file.projectRelativeSourcePath == path;
            });
    }

    bool RunLifecycle(
        const fs::path& root,
        const fs::path& staticFixture,
        const fs::path& animatedFixture,
        const fs::path& runtimePath,
        const fs::path& dxcPath,
        const fs::path& lp03FixtureRoot,
        const fs::path& packageDocRoot,
        const std::string& renegadeRevision,
        const std::string& wickedRevision)
    {
        std::error_code ec;
        fs::remove_all(root, ec);
        ec.clear();
        const fs::path projectRoot = root / "Source Project";
        const fs::path outputParent = root / "Build Output";
        const fs::path localAppData = root / "LocalAppData";
        const fs::path detachedCwd = root / "Detached CWD";
        const fs::path iconPath = root / "Build Inputs" / "Gate6Proof.ico";
        std::string error;

        if (!CopyTreeContents(lp03FixtureRoot, projectRoot, error))
            return Require(false, error);
        fs::create_directories(projectRoot / "Intermediate" / "Transactions", ec);
        fs::create_directories(detachedCwd, ec);
        fs::create_directories(localAppData, ec);
        if (!Require(!ec, "could not create Gate 6 project/runtime working directories") ||
            !Require(WriteIcon(iconPath, error), error))
            return false;

        // Keep LP03's accepted flow/screen/scene identities but give this
        // disposable owner build a deterministic short public game name.
        const std::string descriptor =
            "format = renegade-project\n"
            "version = 1\n\n"
            "[project]\n"
            "project_id = 61111111-1111-4111-8111-111111111111\n"
            "name = Gate6Proof\n"
            "startup_scene = Content/Scenes/LevelOne.wiscene\n"
            "startup_flow_id = 62222222-2222-4222-8222-222222222222\n"
            "startup_flow = Content/Flow/Main.renegade-flow\n"
            "startup_screen_id = 63333333-3333-4333-8333-333333333333\n"
            "startup_screen = Content/UI/Main.renegade-screen\n";
        const fs::path descriptorPath = projectRoot / "ScreenProject.renegade";
        if (!Require(WriteText(descriptorPath, descriptor),
                "could not write Gate 6 project descriptor"))
            return false;

        CreatorAssetWorkflowService workflow;
        const CreatorModelImportResult imported = workflow.ImportModel(
            projectRoot.generic_u8string(), ProjectId,
            animatedFixture.generic_u8string());
        if (!Require(imported.succeeded && imported.asset.succeeded,
                "creator import failed: " + imported.error) ||
            !Require(imported.asset.modelMetadata.known &&
                    imported.asset.modelMetadata.skinned &&
                    imported.asset.modelMetadata.animated,
                "initial representative FBX did not retain skinned/animated metadata"))
            return false;

        const StableId productId = imported.asset.assetId;
        const StableId sourceId = imported.asset.sourceAssetId;
        const fs::path retainedSource =
            projectRoot / fs::u8path(imported.stagedSourceProjectRelativePath);
        const fs::path productPath =
            projectRoot / fs::u8path(imported.assetProjectRelativePath);
        if (!Require(IsValidStableId(productId) && IsValidStableId(sourceId),
                "creator import did not create stable source/product IDs") ||
            !Require(fs::is_regular_file(retainedSource) &&
                    fs::is_regular_file(productPath),
                "creator import did not retain source and governed RAsset"))
            return false;

        if (!Require(workflow.SetCreatorTags(
                projectRoot.generic_u8string(), ProjectId, productId,
                {"Gate6", "Packaged"}, error),
                "could not persist creator tags: " + error))
            return false;
        AssetCatalogue catalogue;
        if (!Require(workflow.BuildCatalogue(
                projectRoot.generic_u8string(), ProjectId, catalogue, error),
                "could not build creator catalogue: " + error))
            return false;
        const AssetCatalogueEntry* entry =
            FindCatalogueEntry(catalogue, productId);
        if (!Require(entry != nullptr &&
                entry->state == AssetCatalogueState::Current &&
                entry->model.skinned && entry->model.animated,
                "creator catalogue did not expose the imported current model"))
            return false;

        ReusableModelAssetDocument initialDocument;
        if (!Require(ReadReusableModelAssetDocument(
                productPath.generic_u8string(), initialDocument, error),
                "could not read initial RAsset: " + error))
            return false;
        const std::string initialPayloadHash =
            initialDocument.manifest.payloadHash;

        // Save a scene containing the INITIAL payload. This WISCENE is never
        // resaved after the later reimport. Runtime must therefore prove it
        // follows the stable product ID to the packaged current payload.
        SceneService scenes;
        SelectionService selection;
        CommandService commands;
        ProjectService documentProjects;
        SceneDocumentService documents(
            scenes, selection, commands, documentProjects);
        scenes.NewScene();
        auto prepared = workflow.PrepareModelPlacement(
            projectRoot.generic_u8string(), ProjectId, productId);
        if (!Require(prepared.IsReady(),
                "could not prepare initial RAsset placement: " +
                    prepared.Result().error))
            return false;
        const wi::scene::Scene* preparedScene = prepared.PeekScene();
        const float scale = ImportService::ResolveScaleFactor(
            ModelScaleMode::Automatic, *preparedScene);
        auto placement = std::make_unique<PlaceReusableModelCommand>(
            scenes.GetScene(), prepared.ReleaseScene(), productId,
            XMFLOAT3(4.0f, 0.0f, 0.0f), scale);
        auto* placed = placement.get();
        if (!Require(commands.Execute(std::move(placement)),
                "stable reusable asset placement command failed"))
            return false;
        const wi::ecs::Entity savedWrapper = placed->PlacedEntity();
        const fs::path levelOne =
            projectRoot / "Content" / "Scenes" / "LevelOne.wiscene";
        if (!Require(documents.Save(levelOne.generic_u8string()),
                "could not save Gate 6 LevelOne scene: " + scenes.LastError()))
            return false;
        documents.NewScene();
        if (!Require(documents.Open(levelOne.generic_u8string()),
                "could not reopen Gate 6 LevelOne scene: " + scenes.LastError()))
            return false;
        std::vector<ReusableAssetInstanceRecord> savedInstances;
        if (!Require(InspectReusableAssetInstances(
                scenes.GetScene(), savedInstances, error) &&
                savedInstances.size() == 1 &&
                savedInstances.front().assetId == productId,
                "saved/reopened scene lost reusable product stable identity: " + error))
            return false;
        const auto* savedTransform =
            scenes.GetScene().transforms.GetComponent(savedInstances.front().instanceRoot);
        if (!Require(savedTransform != nullptr &&
                savedTransform->translation_local.x == 4.0f,
                "saved reusable wrapper lost creator-authored transform") ||
            !Require(savedWrapper != wi::ecs::INVALID_ENTITY,
                "initial reusable wrapper was invalid before save"))
            return false;

        std::vector<std::uint8_t> productBeforeFailure;
        if (!Require(ReadBytes(productPath, productBeforeFailure),
                "could not capture last-good RAsset bytes"))
            return false;

        // Failed explicit reimport: Creator workflow first refreshes the source
        // hash to the malformed disk state, but the governed product bytes must
        // remain exactly the prior last-good product.
        if (!Require(WriteText(retainedSource, "not-a-valid-fbx\n"),
                "could not create malformed retained-source fixture"))
            return false;
        const ReusableModelReimportResult failedReimport =
            workflow.ReimportModel(
                projectRoot.generic_u8string(), ProjectId, productId);
        std::vector<std::uint8_t> productAfterFailure;
        if (!Require(!failedReimport.succeeded,
                "malformed retained FBX unexpectedly reimported") ||
            !Require(ReadBytes(productPath, productAfterFailure) &&
                    productAfterFailure == productBeforeFailure,
                "failed reimport replaced the last-good RAsset product"))
            return false;

        if (!Require(CopyFileOver(staticFixture, retainedSource, error), error))
            return false;

        ProjectService inspector;
        ProjectMetadata project;
        if (!Require(inspector.InspectProject(
                descriptorPath.generic_u8string(), project, error),
                "could not inspect Gate 6 build project: " + error))
            return false;

        // The source is now a different valid FBX but the product is still the
        // initial import. Build preparation must hash the editor-only source,
        // retain its LC01 stable ID without packaging it, and plan creation must
        // fail closed on stale imported-product provenance.
        WindowsGameBuildProjectState staleState;
        if (!Require(PrepareWindowsGameBuildProjectState(
                project, staleState, error),
                "could not prepare stale build state: " + error) ||
            !Require(GraphContainsAssetPath(
                    staleState.dependencyGraph,
                    imported.assetProjectRelativePath,
                    DependencyRequirement::Required),
                "saved scene did not make the RAsset reachable through LP05") ||
            !Require(GraphContainsAssetPath(
                    staleState.dependencyGraph,
                    imported.stagedSourceProjectRelativePath,
                    DependencyRequirement::EditorOnly),
                "build freshness did not retain the authoritative editor-only source"))
            return false;

        WindowsGameBuildWorkflowRequest staleRequest;
        if (!Require(MakeWorkflowRequest(
                project, staleState, runtimePath, dxcPath, packageDocRoot,
                outputParent, iconPath, renegadeRevision, wickedRevision,
                "lp07-gate6-stale", staleRequest, error),
                "could not compose stale owner build request: " + error))
            return false;

        const fs::path previousFinal =
            outputParent / (std::string(GameName) + " Windows Build");
        const fs::path sentinel = previousFinal / "last-good-sentinel.txt";
        if (!Require(WriteText(sentinel, "last-good-build\n"),
                "could not create last-good build preservation sentinel"))
            return false;
        bool staleSmokeCalled = false;
        WindowsGameBuildWorkflowResult staleBuild;
        const bool staleBuilt = BuildWindowsGame(
            staleRequest,
            [&staleSmokeCalled](
                const WindowsGameBuildPlan&,
                const WindowsGameBuildStageResult&,
                std::string&,
                std::string& smokeError)
            {
                staleSmokeCalled = true;
                smokeError = "stale build reached smoke unexpectedly";
                return false;
            },
            staleBuild,
            error);
        std::string sentinelText;
        if (!Require(!staleBuilt && !staleSmokeCalled &&
                error.find("stale imported product") != std::string::npos,
                "stale source did not fail at imported-product build freshness: " +
                    error) ||
            !Require(ReadText(sentinel, sentinelText) &&
                    sentinelText == "last-good-build\n",
                "failed stale build changed the owner-visible last-good output"))
            return false;
        fs::remove_all(previousFinal, ec);
        ec.clear();

        const ReusableModelReimportResult reimported =
            workflow.ReimportModel(
                projectRoot.generic_u8string(), ProjectId, productId);
        if (!Require(reimported.succeeded &&
                reimported.assetId == productId &&
                reimported.sourceAssetId == sourceId,
                "explicit reimport failed or changed stable identity: " +
                    reimported.error))
            return false;

        ReusableModelAssetDocument currentDocument;
        if (!Require(ReadReusableModelAssetDocument(
                productPath.generic_u8string(), currentDocument, error),
                "could not read reimported RAsset: " + error))
            return false;
        const std::string currentPayloadHash =
            currentDocument.manifest.payloadHash;
        if (!Require(!currentPayloadHash.empty() &&
                currentPayloadHash != initialPayloadHash,
                "representative explicit reimport did not change payload hash"))
            return false;

        if (!Require(workflow.BuildCatalogue(
                projectRoot.generic_u8string(), ProjectId, catalogue, error),
                "could not refresh catalogue after reimport: " + error))
            return false;
        entry = FindCatalogueEntry(catalogue, productId);
        if (!Require(entry != nullptr &&
                entry->state == AssetCatalogueState::Current &&
                entry->creatorTags ==
                    std::vector<std::string>({"gate6", "packaged"}),
                "successful reimport did not return Current/preserve creator tags"))
            return false;

        // Do NOT resave LevelOne here. Its baked child payload is still the
        // initial animated import; only its stable wrapper ID can lead Runtime
        // to the current static reimported product.
        WindowsGameBuildProjectState currentState;
        if (!Require(PrepareWindowsGameBuildProjectState(
                project, currentState, error),
                "could not prepare current Gate 6 build state: " + error))
            return false;

        WindowsGameBuildWorkflowRequest currentRequest;
        if (!Require(MakeWorkflowRequest(
                project, currentState, runtimePath, dxcPath, packageDocRoot,
                outputParent, iconPath, renegadeRevision, wickedRevision,
                "lp07-gate6-current", currentRequest, error),
                "could not compose current owner build request: " + error))
            return false;

        const std::wstring localAppDataWide = localAppData.wstring();
        if (!Require(SetEnvironmentVariableW(
                L"LOCALAPPDATA", localAppDataWide.c_str()) != FALSE,
                "could not isolate Gate 6 Runtime LOCALAPPDATA"))
            return false;
        const fs::path evidencePath =
            localAppData / "RenegadeEngine" / ProjectId /
            "Logs" / "RuntimeBootstrap.log";
        const std::size_t completionCount = currentState.levelCompletionCount;
        if (!Require(completionCount > 0,
                "owner build produced no deterministic Story Flow completion count"))
            return false;

        WindowsGameBuildWorkflowResult built;
        const WindowsGameBuildSmokeRunner smoke =
            [&](const WindowsGameBuildPlan& plan,
                const WindowsGameBuildStageResult& stage,
                std::string& runtimeEvidencePath,
                std::string& smokeError)
            {
                if (!PlanContainsAsset(
                        plan, productId, imported.assetProjectRelativePath))
                {
                    smokeError =
                        "LP06 plan omitted the reachable stable RAsset product";
                    return false;
                }
                if (PlanContainsAsset(
                        plan, sourceId, imported.stagedSourceProjectRelativePath) ||
                    fs::exists(fs::u8path(stage.stagingPath) /
                        "GameData" /
                        fs::u8path(imported.stagedSourceProjectRelativePath)))
                {
                    smokeError =
                        "editor-only FBX source leaked into the Runtime package";
                    return false;
                }

                std::error_code removeError;
                fs::remove(evidencePath, removeError);
                DWORD exitCode = 1;
                const fs::path executable =
                    fs::u8path(stage.stagingPath) / plan.executableFileName;
                if (!RunProcess(
                        executable,
                        SmokeArguments(completionCount),
                        detachedCwd,
                        exitCode,
                        smokeError))
                    return false;
                if (exitCode != 0)
                {
                    smokeError =
                        "staged named Runtime returned " +
                        std::to_string(exitCode);
                    return false;
                }
                if (!ValidateRuntimeEvidence(
                        evidencePath,
                        productId,
                        currentPayloadHash,
                        initialPayloadHash,
                        smokeError))
                    return false;
                runtimeEvidencePath = evidencePath.generic_u8string();
                return true;
            };

        if (!Require(BuildWindowsGame(
                currentRequest, smoke, built, error),
                "current owner Windows build failed: " + error) ||
            !Require(built.promotion.succeeded &&
                    !built.finalOutputPath.empty(),
                "current owner Windows build was not safely promoted"))
            return false;

        const fs::path finalPackage = fs::u8path(built.finalOutputPath);
        WindowsGamePackageIntegrityResult integrity;
        if (!Require(ValidateWindowsGamePackage(
                finalPackage.generic_u8string(), integrity, error),
                "promoted Gate 6 package failed integrity validation: " + error))
            return false;

        std::string contentManifest;
        if (!Require(ReadText(
                finalPackage / "GameData" / "content-manifest.json",
                contentManifest),
                "could not read promoted content manifest") ||
            !Require(contentManifest.find(productId) != std::string::npos &&
                    contentManifest.find(imported.assetProjectRelativePath) !=
                        std::string::npos,
                "promoted content manifest omitted stable RAsset identity/path") ||
            !Require(contentManifest.find(imported.stagedSourceProjectRelativePath) ==
                    std::string::npos &&
                    !fs::exists(finalPackage / "GameData" /
                        fs::u8path(imported.stagedSourceProjectRelativePath)),
                "promoted package contains editor-only retained source"))
            return false;

        // Final acceptance is a second DIRECT launch of the promoted named
        // executable, from an unrelated CWD after the source project has been
        // removed. This is independent of the pre-promotion workflow smoke.
        fs::remove(evidencePath, ec);
        ec.clear();
        fs::remove_all(projectRoot, ec);
        if (!Require(!ec && !fs::exists(projectRoot),
                "could not remove source project before final isolated launch"))
            return false;
        DWORD finalExitCode = 1;
        const fs::path finalExecutable =
            finalPackage / (std::string(GameName) + ".exe");
        if (!Require(RunProcess(
                finalExecutable,
                SmokeArguments(completionCount),
                detachedCwd,
                finalExitCode,
                error),
                "final named packaged Runtime process failed: " + error) ||
            !Require(finalExitCode == 0,
                "final named packaged Runtime returned " +
                    std::to_string(finalExitCode)) ||
            !Require(ValidateRuntimeEvidence(
                evidencePath,
                productId,
                currentPayloadHash,
                initialPayloadHash,
                error),
                "final named packaged Runtime evidence failed: " + error))
            return false;

        if (!Require(
                ImportService::IsModelSourceFormatSupported(ModelSourceFormat::Gltf) &&
                ImportService::IsModelSourceFormatSupported(ModelSourceFormat::Glb),
                "GLB/GLTF support regressed during Gate 6"))
            return false;

        fs::remove_all(root, ec);
        std::cout
            << "LP07 GATE 6 PACKAGE ACCEPTANCE PASS // asset_id="
            << productId
            << " old_payload=" << initialPayloadHash
            << " current_payload=" << currentPayloadHash
            << " named_executable=" << GameName << ".exe\n";
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc != 9)
    {
        std::cerr
            << "Usage: RenegadeReusableAssetPackageAcceptance "
            << "<static.fbx> <animated.fbx> <runtime.exe> <dxcompiler.dll> "
            << "<LP03 fixture root> <package docs root> <renegade sha> <wicked sha>\n";
        return 2;
    }

    const fs::path staticFixture =
        fs::weakly_canonical(fs::u8path(argv[1]));
    const fs::path animatedFixture =
        fs::weakly_canonical(fs::u8path(argv[2]));
    const fs::path runtimePath =
        fs::weakly_canonical(fs::u8path(argv[3]));
    const fs::path dxcPath =
        fs::weakly_canonical(fs::u8path(argv[4]));
    const fs::path lp03FixtureRoot =
        fs::weakly_canonical(fs::u8path(argv[5]));
    const fs::path packageDocRoot =
        fs::weakly_canonical(fs::u8path(argv[6]));
    const std::string renegadeRevision = argv[7];
    const std::string wickedRevision = argv[8];

    if (!Require(fs::is_regular_file(staticFixture), "static FBX fixture missing") ||
        !Require(fs::is_regular_file(animatedFixture), "animated FBX fixture missing") ||
        !Require(fs::is_regular_file(runtimePath), "Release Runtime executable missing") ||
        !Require(fs::is_regular_file(dxcPath), "dxcompiler.dll missing") ||
        !Require(fs::is_directory(lp03FixtureRoot), "LP03 fixture root missing") ||
        !Require(fs::is_directory(packageDocRoot), "package-doc fixture root missing") ||
        !Require(renegadeRevision.size() == 40 && wickedRevision.size() == 40,
            "exact source revisions were not supplied"))
        return 3;

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = WindowClassName;
    RegisterClassExW(&windowClass);
    const HWND window = CreateWindowExW(
        0,
        WindowClassName,
        L"Renegade LP07 Gate 6 Package Acceptance",
        WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64,
        nullptr, nullptr, instance, nullptr);
    if (!Require(window != nullptr, "could not create Gate 6 graphics proof window"))
        return 4;

    int exitCode = 0;
    {
        wi::Application application;
        application.allow_hdr = false;
        application.SetWindow(window);
        if (!Require(wi::graphics::GetDevice() != nullptr,
                "Wicked graphics device was not initialized"))
        {
            exitCode = 5;
        }
        else
        {
            const auto nonce = std::chrono::high_resolution_clock::now()
                .time_since_epoch().count();
            const fs::path root = fs::temp_directory_path() /
                fs::u8path(u8"Renegade LP07 Gate6 Package Ω " +
                    std::to_string(nonce));
            if (!RunLifecycle(
                    root,
                    staticFixture,
                    animatedFixture,
                    runtimePath,
                    dxcPath,
                    lp03FixtureRoot,
                    packageDocRoot,
                    renegadeRevision,
                    wickedRevision))
            {
                exitCode = 6;
            }
        }
    }

    if (IsWindow(window))
        DestroyWindow(window);
    UnregisterClassW(WindowClassName, instance);
    return exitCode;
}
