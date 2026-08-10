#include "RuntimePackageBootstrap.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::runtime;

    constexpr const char* ProjectId =
        "11111111-1111-4111-8111-111111111111";
    constexpr const char* ExplicitProjectId =
        "33333333-3333-4333-8333-333333333333";
    constexpr const char* SaveDataId =
        "22222222-2222-4222-8222-222222222222";

    int Fail(const std::string& message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    bool WriteFile(
        const fs::path& path,
        const std::string& contents,
        std::string& error)
    {
        std::error_code ec;
        if (!path.parent_path().empty())
        {
            fs::create_directories(path.parent_path(), ec);
            if (ec)
            {
                error = "could not create fixture directory";
                return false;
            }
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

    std::string Project(
        const std::string& id,
        const std::string& name)
    {
        return
            "format = renegade-project\n"
            "version = 1\n\n"
            "[project]\n"
            "project_id = " + id + "\n"
            "name = " + name + "\n"
            "startup_scene = Content/Scenes/Main.wiscene\n";
    }

    std::string Manifest(
        const std::string& executable,
        const std::string& projectId,
        const std::string& projectDocument = "GameData/ProofGame.renegade",
        const bool includeBuildId = true)
    {
        std::string text =
            "{\"format\":\"renegade-project-package-manifest\","
            "\"schema_version\":2,"
            "\"bootstrap_mode\":\"package_relative\","
            "\"project_id\":\"" + projectId + "\","
            "\"game_name\":\"Proof Game\","
            "\"executable\":\"" + executable + "\","
            "\"public_version\":\"0.1.0-gate3\","
            "\"save_data_id\":\"" + std::string(SaveDataId) + "\","
            "\"developer_publisher\":\"Maverick Media Studio\","
            "\"description\":\"Proof Game\","
            "\"copyright\":\"Copyright 2026\","
            "\"build_timestamp_utc\":\"2026-08-10T00:00:00Z\","
            "\"application_manifest_policy\":"
                "\"asInvoker+PerMonitorV2+longPathAware+utf8\","
            "\"icon_resource\":true,"
            "\"project_document\":\"" + projectDocument + "\"";
        if (includeBuildId)
            text += ",\"internal_build_id\":\"gate3-proof-build\"";
        text += "}";
        return text;
    }
}

int main()
{
    const auto nonce = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path originalCwd = fs::current_path();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path(u8"Renegade LP06 Gate3 Bootstrap Ω " +
            std::to_string(nonce));
    const fs::path packageRoot = root / "Package With Spaces";
    const fs::path executable = packageRoot / "ProofGame.exe";
    const fs::path descriptor = packageRoot / "GameData/ProofGame.renegade";
    const fs::path manifest = packageRoot / "GameData/project.manifest.json";
    const fs::path scene =
        packageRoot / "GameData/Content/Scenes/Main.wiscene";
    const fs::path unrelated = root / "Unrelated Working Directory";
    const fs::path explicitRoot = root / "Explicit Project";
    const fs::path explicitDescriptor = explicitRoot / "Explicit.renegade";
    const fs::path explicitScene =
        explicitRoot / "Content/Scenes/Main.wiscene";
    std::string error;

    if (!WriteFile(executable, "named-runtime-fixture\n", error) ||
        !WriteFile(descriptor, Project(ProjectId, "Packaged Project"), error) ||
        !WriteFile(scene, "scene=package\n", error) ||
        !WriteFile(manifest, Manifest("ProofGame.exe", ProjectId), error) ||
        !WriteFile(
            explicitDescriptor,
            Project(ExplicitProjectId, "Explicit Project"),
            error) ||
        !WriteFile(explicitScene, "scene=explicit\n", error))
    {
        fs::remove_all(root);
        return Fail(error);
    }

    std::error_code ec;
    fs::create_directories(unrelated, ec);
    if (ec)
    {
        fs::remove_all(root);
        return Fail("could not create unrelated working directory");
    }
    fs::current_path(unrelated, ec);
    if (ec)
    {
        fs::remove_all(root);
        return Fail("could not switch to unrelated working directory");
    }

    RuntimeBootstrapResult packaged = ResolveRuntimeLaunch(
        {}, executable.generic_u8string());
    if (!packaged.succeeded ||
        fs::path(packaged.projectDescriptorPath) !=
            fs::weakly_canonical(descriptor))
    {
        fs::current_path(originalCwd);
        fs::remove_all(root);
        return Fail("zero-argument package launch did not resolve from executable root");
    }
    packaged = ResolveRuntimeProject(std::move(packaged));
    if (!packaged.succeeded ||
        packaged.project.projectId != ProjectId ||
        packaged.startupScenePath.empty())
    {
        fs::current_path(originalCwd);
        fs::remove_all(root);
        return Fail("resolved packaged project did not pass normal Runtime validation");
    }

    RuntimeBootstrapResult explicitLaunch = ResolveRuntimeLaunch(
        {"--project", explicitDescriptor.generic_u8string()},
        executable.generic_u8string());
    explicitLaunch = ResolveRuntimeProject(std::move(explicitLaunch));
    if (!explicitLaunch.succeeded ||
        explicitLaunch.project.projectId != ExplicitProjectId)
    {
        fs::current_path(originalCwd);
        fs::remove_all(root);
        return Fail("explicit --project did not remain authoritative over package fallback");
    }

    const RuntimeBootstrapResult malformedExplicit = ResolveRuntimeLaunch(
        {"--project"}, executable.generic_u8string());
    if (malformedExplicit.succeeded ||
        malformedExplicit.code != RuntimeBootstrapCode::InvalidArguments)
    {
        fs::current_path(originalCwd);
        fs::remove_all(root);
        return Fail("malformed explicit --project incorrectly fell back to package bootstrap");
    }

    if (!WriteFile(manifest, Manifest("DifferentGame.exe", ProjectId), error))
    {
        fs::current_path(originalCwd);
        fs::remove_all(root);
        return Fail(error);
    }
    if (ResolveRuntimeLaunch({}, executable.generic_u8string()).succeeded)
    {
        fs::current_path(originalCwd);
        fs::remove_all(root);
        return Fail("package bootstrap accepted executable identity mismatch");
    }

    if (!WriteFile(
            manifest,
            Manifest("ProofGame.exe", ProjectId, "GameData/../outside.renegade"),
            error))
    {
        fs::current_path(originalCwd);
        fs::remove_all(root);
        return Fail(error);
    }
    if (ResolveRuntimeLaunch({}, executable.generic_u8string()).succeeded)
    {
        fs::current_path(originalCwd);
        fs::remove_all(root);
        return Fail("package bootstrap accepted escaping project_document path");
    }

    if (!WriteFile(
            manifest,
            Manifest("ProofGame.exe", ExplicitProjectId),
            error))
    {
        fs::current_path(originalCwd);
        fs::remove_all(root);
        return Fail(error);
    }
    if (ResolveRuntimeLaunch({}, executable.generic_u8string()).succeeded)
    {
        fs::current_path(originalCwd);
        fs::remove_all(root);
        return Fail("package bootstrap accepted project ID mismatch");
    }

    if (!WriteFile(
            manifest,
            Manifest("ProofGame.exe", ProjectId, "GameData/ProofGame.renegade", false),
            error))
    {
        fs::current_path(originalCwd);
        fs::remove_all(root);
        return Fail(error);
    }
    if (ResolveRuntimeLaunch({}, executable.generic_u8string()).succeeded)
    {
        fs::current_path(originalCwd);
        fs::remove_all(root);
        return Fail("package bootstrap accepted missing Gate 3 build identity");
    }

    fs::current_path(originalCwd, ec);
    fs::remove_all(root);
    if (ec)
        return Fail("could not restore original working directory");

    std::cout << "PASS: LP06 Gate 3 package-relative Runtime bootstrap\n";
    return 0;
}
