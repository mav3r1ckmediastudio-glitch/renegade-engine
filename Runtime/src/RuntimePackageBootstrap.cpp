#include "RuntimePackageBootstrap.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/PackageIntegrityService.h"
#include "renegade/bridge/ProjectService.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "json.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace renegade::runtime
{
    namespace
    {
        namespace fs = std::filesystem;

        RuntimeBootstrapResult RejectPackage(
            RuntimeBootstrapResult result,
            const RuntimeBootstrapCode code,
            std::string message)
        {
            result.succeeded = false;
            result.code = code;
            result.message = std::move(message);
            result.projectDescriptorPath.clear();
            result.project = {};
            result.startupScenePath.clear();
            result.startupFlowPath.clear();
            result.startupScreenPath.clear();
            return result;
        }

        bool IsSafePackagePath(const std::string& value)
        {
            if (value.empty() || value.find('\\') != std::string::npos)
                return false;
            const fs::path path = fs::u8path(value);
            if (path.empty() || path.is_absolute() ||
                path.lexically_normal().generic_u8string() != value)
            {
                return false;
            }
            return std::none_of(
                path.begin(),
                path.end(),
                [](const fs::path& component)
                {
                    return component.empty() ||
                        component == "." ||
                        component == "..";
                });
        }

#if defined(_WIN32)
        bool ComponentEqual(
            const fs::path& left,
            const fs::path& right)
        {
            const std::wstring leftText = left.native();
            const std::wstring rightText = right.native();
            return CompareStringOrdinal(
                leftText.c_str(),
                static_cast<int>(leftText.size()),
                rightText.c_str(),
                static_cast<int>(rightText.size()),
                TRUE) == CSTR_EQUAL;
        }
#else
        bool ComponentEqual(
            const fs::path& left,
            const fs::path& right)
        {
            return left == right;
        }
#endif

        bool IsWithinRoot(
            const fs::path& root,
            const fs::path& candidate)
        {
            auto rootPart = root.begin();
            auto candidatePart = candidate.begin();
            for (; rootPart != root.end(); ++rootPart, ++candidatePart)
            {
                if (candidatePart == candidate.end() ||
                    !ComponentEqual(*rootPart, *candidatePart))
                {
                    return false;
                }
            }
            return true;
        }

        bool IsRegularNonSymlink(
            const fs::path& path)
        {
            std::error_code ec;
            const fs::file_status linkStatus = fs::symlink_status(path, ec);
            if (ec || fs::is_symlink(linkStatus))
                return false;
            return fs::is_regular_file(path, ec) && !ec;
        }

        bool ReadManifest(
            const fs::path& path,
            nlohmann::json& manifest,
            std::string& error)
        {
            if (!IsRegularNonSymlink(path))
            {
                error = "Packaged Runtime project manifest is missing or symlinked.";
                return false;
            }
            std::ifstream input(path, std::ios::binary);
            if (!input)
            {
                error = "Packaged Runtime could not read GameData/project.manifest.json.";
                return false;
            }
            try
            {
                input >> manifest;
            }
            catch (const std::exception& exception)
            {
                error = std::string(
                    "Packaged Runtime project manifest is invalid JSON: ") +
                    exception.what();
                return false;
            }
            if (!manifest.is_object())
            {
                error = "Packaged Runtime project manifest is not a JSON object.";
                return false;
            }
            return true;
        }

        bool RequiredIdentityField(
            const nlohmann::json& manifest,
            const char* key)
        {
            return manifest.contains(key) &&
                manifest[key].is_string() &&
                !manifest[key].get<std::string>().empty();
        }

        bool ExecutableNameEqual(
            const std::string& expected,
            const std::string& actual)
        {
#if defined(_WIN32)
            const fs::path expectedPath = fs::u8path(expected);
            const fs::path actualPath = fs::u8path(actual);
            return ComponentEqual(
                expectedPath.filename(),
                actualPath.filename());
#else
            return expected == actual;
#endif
        }
    }

    RuntimeBootstrapResult ResolveRuntimeLaunch(
        const std::vector<std::string>& arguments,
        const std::string& executablePath)
    {
        RuntimeBootstrapResult result =
            ParseRuntimeLaunchArguments(arguments);
        if (result.succeeded)
        {
            // Explicit --project remains authoritative for Test Level and tools.
            return result;
        }
        if (result.code != RuntimeBootstrapCode::MissingProjectArgument)
        {
            // Malformed or duplicate explicit arguments never fall through to
            // package discovery because that would hide a caller error.
            return result;
        }

        result.packageRelativeLaunch = true;

        try
        {
            if (executablePath.empty())
            {
                return RejectPackage(
                    std::move(result),
                    RuntimeBootstrapCode::InvalidArguments,
                    "Packaged Runtime could not resolve its executable path.");
            }

            std::error_code ec;
            const fs::path executable =
                fs::absolute(fs::u8path(executablePath), ec).lexically_normal();
            if (ec || !IsRegularNonSymlink(executable))
            {
                return RejectPackage(
                    std::move(result),
                    RuntimeBootstrapCode::InvalidArguments,
                    "Packaged Runtime executable path is missing or invalid.");
            }

            const fs::path packageRoot =
                fs::weakly_canonical(executable.parent_path(), ec);
            if (ec || packageRoot.empty())
            {
                return RejectPackage(
                    std::move(result),
                    RuntimeBootstrapCode::InvalidArguments,
                    "Packaged Runtime could not resolve its package root.");
            }
            result.packageRootPath = packageRoot.generic_u8string();

            bridge::WindowsGamePackageIntegrityResult integrity;
            std::string integrityError;
            if (!bridge::ValidateWindowsGamePackage(
                    result.packageRootPath,
                    integrity,
                    integrityError))
            {
                result.packageIntegrityStatus = "FAIL";
                result.packageIntegrityCode =
                    bridge::WindowsGamePackageIntegrityCodeName(integrity.code);
                return RejectPackage(
                    std::move(result),
                    RuntimeBootstrapCode::PackageIntegrityFailed,
                    "Packaged Runtime integrity validation failed: " +
                        integrityError);
            }
            result.packageIntegrityStatus = "PASS";
            result.packageIntegrityCode =
                bridge::WindowsGamePackageIntegrityCodeName(integrity.code);
            result.packageManifestSha256 = integrity.packageManifestSha256;

            const fs::path manifestPath =
                packageRoot / "GameData" / "project.manifest.json";
            const fs::path canonicalManifest =
                fs::weakly_canonical(manifestPath, ec);
            if (ec || !IsWithinRoot(packageRoot, canonicalManifest))
            {
                return RejectPackage(
                    std::move(result),
                    RuntimeBootstrapCode::InvalidArguments,
                    "Packaged Runtime project manifest escapes the package root.");
            }

            nlohmann::json manifest;
            std::string manifestError;
            if (!ReadManifest(canonicalManifest, manifest, manifestError))
            {
                return RejectPackage(
                    std::move(result),
                    RuntimeBootstrapCode::InvalidArguments,
                    std::move(manifestError));
            }

            if (manifest.value("format", std::string{}) !=
                    "renegade-project-package-manifest" ||
                manifest.value("schema_version", 0) != 2 ||
                manifest.value("bootstrap_mode", std::string{}) !=
                    "package_relative")
            {
                return RejectPackage(
                    std::move(result),
                    RuntimeBootstrapCode::InvalidArguments,
                    "Packaged Runtime project manifest is not a Gate 3 "
                    "package-relative bootstrap manifest.");
            }

            const std::string projectId =
                manifest.value("project_id", std::string{});
            const std::string saveDataId =
                manifest.value("save_data_id", std::string{});
            const std::string executableName =
                manifest.value("executable", std::string{});
            const std::string projectDocument =
                manifest.value("project_document", std::string{});
            if (!bridge::IsValidStableId(projectId) ||
                !bridge::IsValidStableId(saveDataId) ||
                !RequiredIdentityField(manifest, "public_version") ||
                !RequiredIdentityField(manifest, "internal_build_id") ||
                !RequiredIdentityField(manifest, "build_timestamp_utc") ||
                !RequiredIdentityField(manifest, "developer_publisher") ||
                !RequiredIdentityField(manifest, "application_manifest_policy") ||
                manifest.value("icon_resource", false) != true)
            {
                return RejectPackage(
                    std::move(result),
                    RuntimeBootstrapCode::InvalidArguments,
                    "Packaged Runtime project manifest is missing required "
                    "Gate 3 identity.");
            }

            if (!ExecutableNameEqual(
                    executableName,
                    executable.filename().generic_u8string()))
            {
                return RejectPackage(
                    std::move(result),
                    RuntimeBootstrapCode::InvalidArguments,
                    "Packaged Runtime executable identity does not match "
                    "GameData/project.manifest.json.");
            }
            if (!IsSafePackagePath(projectDocument) ||
                projectDocument.rfind("GameData/", 0) != 0)
            {
                return RejectPackage(
                    std::move(result),
                    RuntimeBootstrapCode::InvalidArguments,
                    "Packaged Runtime project document path is unsafe.");
            }

            const fs::path descriptor =
                fs::weakly_canonical(
                    packageRoot / fs::u8path(projectDocument),
                    ec);
            if (ec ||
                !IsWithinRoot(packageRoot, descriptor) ||
                !IsRegularNonSymlink(descriptor))
            {
                return RejectPackage(
                    std::move(result),
                    RuntimeBootstrapCode::InvalidArguments,
                    "Packaged Runtime project document is missing, symlinked, "
                    "or outside the package root.");
            }

            bridge::ProjectService projects;
            bridge::ProjectMetadata metadata;
            std::string projectError;
            if (!projects.InspectProject(
                    descriptor.generic_u8string(),
                    metadata,
                    projectError))
            {
                return RejectPackage(
                    std::move(result),
                    RuntimeBootstrapCode::InvalidArguments,
                    "Packaged Runtime rejected its project document: " +
                        projectError);
            }
            if (metadata.projectId != projectId)
            {
                return RejectPackage(
                    std::move(result),
                    RuntimeBootstrapCode::InvalidArguments,
                    "Packaged Runtime project ID does not match the package "
                    "manifest.");
            }

            result.succeeded = true;
            result.code = RuntimeBootstrapCode::Success;
            result.message =
                "Runtime package-relative project bootstrap resolved.";
            result.projectDescriptorPath = descriptor.generic_u8string();
            return result;
        }
        catch (const std::exception& exception)
        {
            return RejectPackage(
                std::move(result),
                RuntimeBootstrapCode::InvalidArguments,
                std::string("Packaged Runtime bootstrap failed: ") +
                    exception.what());
        }
    }
}
