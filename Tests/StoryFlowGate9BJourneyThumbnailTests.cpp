#include "renegade/bridge/StoryFlowJourneyThumbnailService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using renegade::bridge::StoryFlowJourneyThumbnailService;

    constexpr const char* LevelNodeId =
        "44444444-4444-4444-8444-444444444444";

    struct TemporaryDirectory
    {
        fs::path path;
        ~TemporaryDirectory()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    };

    int Fail(const char* message)
    {
        std::cerr << "RenegadeStoryFlowGate9BJourneyThumbnailTests: "
                  << message << '\n';
        return 1;
    }

    bool WriteBytes(const fs::path& path, const std::string& value)
    {
        std::error_code error;
        fs::create_directories(path.parent_path(), error);
        if (error) return false;
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(value.data(), static_cast<std::streamsize>(value.size()));
        return static_cast<bool>(stream);
    }

    std::string ReadBytes(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::string(
            (std::istreambuf_iterator<char>(stream)),
            std::istreambuf_iterator<char>());
    }
}

int main()
{
    using namespace renegade::bridge;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade-story-flow-gate9b-" + std::to_string(unique))
    };

    const fs::path projectRoot = temporary.path / "Project";
    const fs::path flowPath =
        projectRoot / "Content" / "Flow" / "Main.renegade-flow";
    if (!WriteBytes(projectRoot / "Project.renegade", "format = renegade-project\n") ||
        !WriteBytes(flowPath, "{}"))
    {
        return Fail("fixture could not be created");
    }

    std::string resolvedRoot;
    std::string error;
    if (!StoryFlowJourneyThumbnailService::ResolveProjectRootFromFlowPath(
            flowPath.generic_u8string(), resolvedRoot, error) ||
        fs::u8path(resolvedRoot).lexically_normal() !=
            projectRoot.lexically_normal())
    {
        return Fail("project root was not recovered from the Story Flow path");
    }

    if (!StoryFlowJourneyThumbnailService::IsSupportedExtension(".PNG") ||
        !StoryFlowJourneyThumbnailService::IsSupportedExtension(".jpeg") ||
        StoryFlowJourneyThumbnailService::IsSupportedExtension(".gif"))
    {
        return Fail("supported image extension policy is wrong");
    }

    StoryFlowJourneyThumbnailService service;
    const fs::path sourcePng = temporary.path / "picked" / "first.PNG";
    if (!WriteBytes(sourcePng, "png-first"))
        return Fail("PNG source fixture could not be written");

    auto imported = service.Import(
        projectRoot.generic_u8string(),
        LevelNodeId,
        sourcePng.generic_u8string());
    if (!imported.succeeded ||
        imported.relativePath !=
            std::string("Content/StoryFlow/Thumbnails/") + LevelNodeId + ".png" ||
        ReadBytes(fs::u8path(imported.resolvedPath)) != "png-first")
    {
        return Fail("first thumbnail import did not use the stable-ID governed slot");
    }

    std::string relativePath;
    std::string resolvedPath;
    if (!service.ResolveManaged(
            projectRoot.generic_u8string(), LevelNodeId,
            relativePath, resolvedPath, error) ||
        relativePath != imported.relativePath ||
        fs::u8path(resolvedPath).lexically_normal() !=
            fs::u8path(imported.resolvedPath).lexically_normal())
    {
        return Fail("managed thumbnail did not resolve after import/reopen");
    }

    const fs::path sourcePngReplacement =
        temporary.path / "picked" / "replacement.png";
    if (!WriteBytes(sourcePngReplacement, "png-replacement"))
        return Fail("replacement PNG fixture could not be written");
    imported = service.Import(
        projectRoot.generic_u8string(),
        LevelNodeId,
        sourcePngReplacement.generic_u8string());
    if (!imported.succeeded ||
        ReadBytes(fs::u8path(imported.resolvedPath)) != "png-replacement")
    {
        return Fail("same-format thumbnail replacement did not replace the managed resource");
    }

    const fs::path sourceJpg = temporary.path / "picked" / "replacement.JPG";
    if (!WriteBytes(sourceJpg, "jpg-replacement"))
        return Fail("JPG replacement fixture could not be written");
    imported = service.Import(
        projectRoot.generic_u8string(),
        LevelNodeId,
        sourceJpg.generic_u8string());
    if (!imported.succeeded ||
        imported.relativePath !=
            std::string("Content/StoryFlow/Thumbnails/") + LevelNodeId + ".jpg" ||
        fs::exists(projectRoot / "Content" / "StoryFlow" / "Thumbnails" /
            fs::u8path(std::string(LevelNodeId) + ".png")) ||
        ReadBytes(fs::u8path(imported.resolvedPath)) != "jpg-replacement")
    {
        return Fail("format-changing replacement did not leave one governed slot");
    }

    const fs::path unsupported = temporary.path / "picked" / "bad.gif";
    if (!WriteBytes(unsupported, "gif"))
        return Fail("unsupported fixture could not be written");
    const auto rejected = service.Import(
        projectRoot.generic_u8string(),
        LevelNodeId,
        unsupported.generic_u8string());
    if (rejected.succeeded)
        return Fail("unsupported thumbnail extension was accepted");

    // Fail closed if filesystem corruption/manual copying creates two managed
    // variants for the same stable node. Journey View must never silently pick.
    const fs::path duplicate = projectRoot / "Content" / "StoryFlow" /
        "Thumbnails" / fs::u8path(std::string(LevelNodeId) + ".png");
    if (!WriteBytes(duplicate, "duplicate"))
        return Fail("ambiguous-slot fixture could not be created");
    relativePath.clear();
    resolvedPath.clear();
    if (service.ResolveManaged(
            projectRoot.generic_u8string(), LevelNodeId,
            relativePath, resolvedPath, error))
    {
        return Fail("ambiguous managed thumbnail slot did not fail closed");
    }

    std::cout << "Story Flow Gate 9B Journey thumbnail proof passed.\n";
    return 0;
}
