#include "WindowsGameBuildController.h"

#include "renegade/bridge/PackageIntegrityService.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/WindowsGameBuildProjectService.h"
#include "renegade/bridge/WindowsGameBuildWorkflow.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifndef RENEGADE_SOURCE_REVISION
#error RENEGADE_SOURCE_REVISION must be supplied by the Renegade Studio build.
#endif

#ifndef RENEGADE_WICKED_REVISION
#error RENEGADE_WICKED_REVISION must be supplied by the Renegade Studio build.
#endif

namespace renegade::studio
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr const char* DeveloperPublisher = "Maverick Media Studio";
        constexpr DWORD SmokeTimeoutMilliseconds = 120000;

        fs::path StudioDirectory()
        {
            std::wstring path(32768, L'\0');
            const DWORD count = GetModuleFileNameW(
                nullptr, path.data(), static_cast<DWORD>(path.size()));
            if (count == 0 || count >= path.size())
                return {};
            path.resize(count);
            return fs::path(path).parent_path();
        }

        bool IsRegularFile(const fs::path& path)
        {
            std::error_code ec;
            return fs::is_regular_file(path, ec) && !ec &&
                !fs::is_symlink(path, ec) && !ec;
        }

        bool FindFirstRegularFile(
            const std::vector<fs::path>& candidates,
            fs::path& result)
        {
            for (const fs::path& candidate : candidates)
            {
                if (IsRegularFile(candidate))
                {
                    result = candidate;
                    return true;
                }
            }
            result.clear();
            return false;
        }

        std::string UtcTimestamp()
        {
            const std::time_t now = std::time(nullptr);
            std::tm utc{};
            if (gmtime_s(&utc, &now) != 0)
                return {};
            std::ostringstream stream;
            stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
            return stream.str();
        }

        std::string UniqueStagingId()
        {
            const auto ticks = static_cast<std::uint64_t>(
                std::chrono::high_resolution_clock::now()
                    .time_since_epoch().count());
            std::ostringstream stream;
            stream << "studio-" << std::hex << std::nouppercase << ticks
                << "-" << GetCurrentProcessId();
            return stream.str();
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

        bool WriteDefaultGameIcon(
            const fs::path& projectRoot,
            fs::path& iconPath,
            std::string& error)
        {
            iconPath = projectRoot / "Intermediate" / "Build" /
                "RenegadeDefaultGame.ico";
            std::error_code ec;
            fs::create_directories(iconPath.parent_path(), ec);
            if (ec)
            {
                error = "Build Windows Game could not create the governed icon directory.";
                return false;
            }

            std::ofstream output(iconPath, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                error = "Build Windows Game could not create the governed default icon.";
                return false;
            }

            // Deterministic one-image ICO containing a 32-bit 1x1 DIB. The
            // default is intentionally simple; project-selectable branding is
            // a later authoring concern, not a Gate 5 packaging semantic.
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

            const std::array<unsigned char, 8> image = {
                29, 91, 210, 255,
                0, 0, 0, 0,
            };
            output.write(
                reinterpret_cast<const char*>(image.data()),
                static_cast<std::streamsize>(image.size()));
            output.close();
            if (!output)
            {
                error = "Build Windows Game could not finish the governed default icon.";
                return false;
            }
            error.clear();
            return true;
        }

        bool AddRuntimeSupport(
            const std::string& logicalName,
            const std::string& destination,
            const fs::path& source,
            const std::string& provenance,
            std::vector<bridge::WindowsRuntimeSupportInput>& planInputs,
            std::vector<bridge::WindowsRuntimeSupportSource>& stageInputs,
            std::string& error)
        {
            bridge::WindowsGamePackageFileDigest digest;
            if (!bridge::DigestWindowsGamePackageFile(
                    source.generic_u8string(), digest, error))
            {
                error = "Build Windows Game could not hash Runtime support " +
                    destination + ": " + error;
                return false;
            }

            bridge::WindowsRuntimeSupportInput plan;
            plan.logicalName = logicalName;
            plan.destinationPath = destination;
            plan.byteCount = digest.byteCount;
            plan.sha256 = digest.sha256;
            plan.provenance = provenance;
            planInputs.push_back(std::move(plan));

            bridge::WindowsRuntimeSupportSource stage;
            stage.destinationPath = destination;
            stage.sourcePath = source.generic_u8string();
            stageInputs.push_back(std::move(stage));
            return true;
        }

        bool AddPackageDocument(
            const fs::path& source,
            std::string destination,
            std::string component,
            std::string provenance,
            std::vector<bridge::WindowsPackageDocumentInput>& documents,
            std::string& error)
        {
            if (!IsRegularFile(source))
            {
                error = "Build Windows Game is missing governed package input: " +
                    source.generic_u8string();
                return false;
            }
            bridge::WindowsPackageDocumentInput document;
            document.destinationPath = std::move(destination);
            document.sourcePath = source.generic_u8string();
            document.component = std::move(component);
            document.provenance = std::move(provenance);
            documents.push_back(std::move(document));
            return true;
        }

        std::wstring QuoteArgument(const std::wstring& value)
        {
            if (value.find_first_of(L" \t\"") == std::wstring::npos)
                return value;
            std::wstring quoted = L"\"";
            std::size_t slashes = 0;
            for (const wchar_t character : value)
            {
                if (character == L'\\')
                {
                    ++slashes;
                    continue;
                }
                if (character == L'\"')
                {
                    quoted.append(slashes * 2 + 1, L'\\');
                    quoted.push_back(L'\"');
                    slashes = 0;
                    continue;
                }
                quoted.append(slashes, L'\\');
                slashes = 0;
                quoted.push_back(character);
            }
            quoted.append(slashes * 2, L'\\');
            quoted.push_back(L'\"');
            return quoted;
        }

        fs::path RuntimeEvidencePath(const std::string& identity)
        {
            const DWORD required = GetEnvironmentVariableW(
                L"LOCALAPPDATA", nullptr, 0);
            if (required <= 1)
                return {};
            std::wstring localAppData(static_cast<std::size_t>(required), L'\0');
            const DWORD written = GetEnvironmentVariableW(
                L"LOCALAPPDATA", localAppData.data(), required);
            if (written == 0 || written >= required)
                return {};
            localAppData.resize(written);
            return fs::path(localAppData) /
                "RenegadeEngine" /
                fs::u8path(identity) /
                "Logs" /
                "RuntimeBootstrap.log";
        }

        bool RunStandaloneSmoke(
            const bridge::WindowsGameBuildPlan& plan,
            const bridge::WindowsGameBuildStageResult& stage,
            const std::size_t completionCount,
            const std::string& stagingId,
            std::string& runtimeEvidencePath,
            std::string& error)
        {
            runtimeEvidencePath.clear();
            const fs::path executable = fs::u8path(stage.stagingPath) /
                fs::u8path(plan.executableFileName);
            if (!IsRegularFile(executable))
            {
                error = "Build Windows Game smoke cannot find the staged named executable.";
                return false;
            }
            if (completionCount == 0)
            {
                error = "Build Windows Game smoke requires at least one deterministic Level completion.";
                return false;
            }

            const fs::path evidence = RuntimeEvidencePath(plan.saveDataId);
            if (evidence.empty())
            {
                error = "Build Windows Game smoke cannot resolve LOCALAPPDATA evidence storage.";
                return false;
            }
            std::error_code ec;
            const bool evidenceExists = fs::exists(evidence, ec);
            if (ec)
            {
                error = "Build Windows Game smoke could not inspect prior Runtime evidence.";
                return false;
            }
            if (evidenceExists)
            {
                const bool removed = fs::remove(evidence, ec);
                if (ec || !removed)
                {
                    error = "Build Windows Game smoke could not remove stale Runtime evidence.";
                    return false;
                }
            }

            ec.clear();
            const fs::path workingDirectory = fs::temp_directory_path(ec) /
                "RenegadeBuildSmoke" / fs::u8path(stagingId);
            if (ec)
            {
                error = "Build Windows Game smoke cannot resolve a detached working directory.";
                return false;
            }
            fs::create_directories(workingDirectory, ec);
            if (ec)
            {
                error = "Build Windows Game smoke cannot create its detached working directory.";
                return false;
            }

            std::wstring command = QuoteArgument(executable.wstring());
            command += L" dx12";
            for (std::size_t index = 0; index < completionCount; ++index)
                command += L" --flow-outcome=level.complete";
            command += L" --renegade-smoke-autoplay --renegade-smoke-exit";
            std::vector<wchar_t> commandBuffer(command.begin(), command.end());
            commandBuffer.push_back(L'\0');

            HANDLE job = CreateJobObjectW(nullptr, nullptr);
            if (job == nullptr)
            {
                error = "Build Windows Game smoke could not create a Runtime job object.";
                return false;
            }
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if (!SetInformationJobObject(
                    job,
                    JobObjectExtendedLimitInformation,
                    &limits,
                    sizeof(limits)))
            {
                CloseHandle(job);
                error = "Build Windows Game smoke could not configure its Runtime job object.";
                return false;
            }

            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            const std::wstring cwd = workingDirectory.wstring();
            if (!CreateProcessW(
                    executable.wstring().c_str(),
                    commandBuffer.data(),
                    nullptr,
                    nullptr,
                    FALSE,
                    CREATE_UNICODE_ENVIRONMENT,
                    nullptr,
                    cwd.c_str(),
                    &startup,
                    &process))
            {
                const DWORD launchError = GetLastError();
                CloseHandle(job);
                error = "Build Windows Game smoke could not launch the staged named executable (Win32 " +
                    std::to_string(launchError) + ").";
                return false;
            }

            const bool assigned =
                AssignProcessToJobObject(job, process.hProcess) != FALSE;
            if (!assigned)
            {
                TerminateProcess(process.hProcess, 1);
                WaitForSingleObject(process.hProcess, 5000);
                CloseHandle(process.hThread);
                CloseHandle(process.hProcess);
                CloseHandle(job);
                error = "Build Windows Game smoke could not contain the Runtime process.";
                return false;
            }

            const DWORD wait = WaitForSingleObject(
                process.hProcess, SmokeTimeoutMilliseconds);
            if (wait != WAIT_OBJECT_0)
            {
                TerminateJobObject(job, 1);
                WaitForSingleObject(process.hProcess, 5000);
                CloseHandle(process.hThread);
                CloseHandle(process.hProcess);
                CloseHandle(job);
                error = wait == WAIT_TIMEOUT
                    ? "Build Windows Game smoke timed out before deterministic completion."
                    : "Build Windows Game smoke failed while waiting for Runtime completion.";
                return false;
            }

            DWORD exitCode = 1;
            const bool readExit =
                GetExitCodeProcess(process.hProcess, &exitCode) != FALSE;
            const DWORD exitReadError = readExit ? ERROR_SUCCESS : GetLastError();
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            CloseHandle(job);
            if (!readExit || exitCode != 0)
            {
                error = readExit
                    ? "Build Windows Game smoke Runtime exited with code " +
                        std::to_string(exitCode) + "."
                    : "Build Windows Game smoke could not read the Runtime exit code (Win32 " +
                        std::to_string(exitReadError) + ").";
                return false;
            }
            if (!IsRegularFile(evidence))
            {
                error = "Build Windows Game smoke completed without fresh Runtime evidence.";
                return false;
            }

            runtimeEvidencePath = evidence.generic_u8string();
            error.clear();
            return true;
        }
    }

    WindowsGameBuildUiResult BuildActiveWindowsGame()
    {
        WindowsGameBuildUiResult ui;

#if !defined(NDEBUG)
        ui.message =
            "Build Windows Game requires a Release Renegade Studio build; Debug remains regression-only.";
        return ui;
#else
        bridge::StudioSession* session = bridge::StudioSession::Current();
        if (session == nullptr || !session->Projects().HasProject())
        {
            ui.message = "Build Windows Game requires an active Renegade project.";
            return ui;
        }

        const bridge::ProjectMetadata project = session->Projects().CurrentProject();
        bridge::WindowsGameBuildProjectState projectState;
        std::string error;
        if (!bridge::PrepareWindowsGameBuildProjectState(
                project, projectState, error))
        {
            ui.message = std::move(error);
            return ui;
        }

        const fs::path studioDirectory = StudioDirectory();
        if (studioDirectory.empty())
        {
            ui.message = "Build Windows Game could not resolve the Studio installation directory.";
            return ui;
        }

        fs::path runtimeExecutable;
        if (!FindFirstRegularFile(
                {
                    studioDirectory / "Runtime" / "RenegadeRuntime.exe",
                    studioDirectory.parent_path().parent_path() /
                        "Runtime" / "Release" / "RenegadeRuntime.exe",
                },
                runtimeExecutable))
        {
            ui.message =
                "Build Windows Game requires the Release RenegadeRuntime.exe beside the Studio package or build tree.";
            return ui;
        }

        fs::path dxCompiler;
        if (!FindFirstRegularFile(
                {
                    studioDirectory / "dxcompiler.dll",
                    runtimeExecutable.parent_path() / "dxcompiler.dll",
                },
                dxCompiler))
        {
            ui.message = "Build Windows Game could not find the governed dxcompiler.dll Runtime support.";
            return ui;
        }

        const std::string renegadeRevision = RENEGADE_SOURCE_REVISION;
        const std::string wickedRevision = RENEGADE_WICKED_REVISION;
        const std::string stagingId = UniqueStagingId();
        const std::string timestamp = UtcTimestamp();
        if (timestamp.empty())
        {
            ui.message = "Build Windows Game could not produce a UTC build timestamp.";
            return ui;
        }

        bridge::WindowsGameBuildWorkflowRequest request;
        request.project = project;
        request.dependencyGraph = std::move(projectState.dependencyGraph);
        request.assetRegistry = std::move(projectState.assetRegistry);
        request.build.gameName = project.name;
        request.build.executableBaseName = project.name;
        request.build.publicVersion = "0.1.0";
        request.build.saveDataId = project.projectId;
        request.build.platform = "windows-x64";
        request.build.configuration = "Release";

        const std::string namedExecutable = project.name + ".exe";
        if (!AddRuntimeSupport(
                "renegade-runtime",
                namedExecutable,
                runtimeExecutable,
                "repo:" + renegadeRevision,
                request.runtimeSupport,
                request.staging.runtimeSupportSources,
                error) ||
            !AddRuntimeSupport(
                "directx-shader-compiler",
                "dxcompiler.dll",
                dxCompiler,
                "pinned:" + wickedRevision,
                request.runtimeSupport,
                request.staging.runtimeSupportSources,
                error))
        {
            ui.message = std::move(error);
            return ui;
        }

        request.staging.projectRootPath = project.rootPath;
        request.staging.outputParentPath =
            (fs::u8path(project.rootPath) / "Builds" / "Windows")
                .generic_u8string();
        request.staging.stagingId = stagingId;
        request.staging.renegadeRevision = renegadeRevision;
        request.staging.wickedRevision = wickedRevision;

        const fs::path buildInputs = studioDirectory / "BuildInputs";
        if (!AddPackageDocument(
                buildInputs / "ReadMe.txt",
                "ReadMe.txt",
                "renegade-build-readme",
                "repo:" + renegadeRevision,
                request.staging.packageDocuments,
                error) ||
            !AddPackageDocument(
                buildInputs / "Renegade-Licence-or-Notice.txt",
                "Licences/Renegade-Licence-or-Notice.txt",
                "renegade",
                "repo:" + renegadeRevision,
                request.staging.packageDocuments,
                error) ||
            !AddPackageDocument(
                buildInputs / "WickedEngine-LICENSE.txt",
                "Licences/WickedEngine-LICENSE.txt",
                "wicked-engine",
                "pinned:" + wickedRevision,
                request.staging.packageDocuments,
                error) ||
            !AddPackageDocument(
                buildInputs / "WickedEngine-third_party_software.txt",
                "Licences/WickedEngine-third_party_software.txt",
                "wicked-engine-third-party",
                "pinned:" + wickedRevision,
                request.staging.packageDocuments,
                error) ||
            !AddPackageDocument(
                buildInputs / "DirectXShaderCompiler-LICENSE.txt",
                "Licences/DirectXShaderCompiler-LICENSE.txt",
                "directx-shader-compiler",
                "pinned:" + wickedRevision,
                request.staging.packageDocuments,
                error) ||
            !AddPackageDocument(
                buildInputs / "DirectXShaderCompiler-ThirdPartyNotices.txt",
                "Licences/DirectXShaderCompiler-ThirdPartyNotices.txt",
                "directx-shader-compiler-third-party",
                "pinned:" + wickedRevision,
                request.staging.packageDocuments,
                error))
        {
            ui.message = std::move(error);
            return ui;
        }

        fs::path iconPath;
        if (!WriteDefaultGameIcon(
                fs::u8path(project.rootPath), iconPath, error))
        {
            ui.message = std::move(error);
            return ui;
        }

        request.identity.developerPublisher = DeveloperPublisher;
        request.identity.description = project.name + " standalone game";
        request.identity.copyrightNotice =
            "Copyright 2026 Maverick Media Studio";
        request.identity.internalBuildId =
            "gate5-" + renegadeRevision.substr(0, 12) + "-" + stagingId;
        request.identity.buildTimestampUtc = timestamp;
        request.identity.iconSourcePath = iconPath.generic_u8string();

        request.verification.expectedGraphicsBackend = "DX12";
        request.verification.windowsPrerequisitePolicy =
            bridge::WindowsVcRuntimePrerequisitePolicy;
        request.verification.expectedFlowTrace = projectState.expectedFlowTrace;

        const std::size_t completionCount = projectState.levelCompletionCount;
        bridge::WindowsGameBuildWorkflowResult result;
        const bridge::WindowsGameBuildSmokeRunner smoke =
            [completionCount, stagingId](
                const bridge::WindowsGameBuildPlan& plan,
                const bridge::WindowsGameBuildStageResult& stage,
                std::string& evidencePath,
                std::string& smokeError)
            {
                return RunStandaloneSmoke(
                    plan,
                    stage,
                    completionCount,
                    stagingId,
                    evidencePath,
                    smokeError);
            };

        if (!bridge::BuildWindowsGame(
                request, smoke, result, error))
        {
            ui.message = std::move(error);
            return ui;
        }

        ui.succeeded = true;
        ui.finalOutputPath = result.finalOutputPath;
        ui.message = "Windows game build promoted successfully: " +
            result.finalOutputPath;
        return ui;
#endif
    }
}
