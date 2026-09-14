#include "renegade/bridge/CreatorExternalAnimationImportService.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "CW-02 EXTERNAL ANIMATION INGESTION FAIL // " << message << '\n';
        return false;
    }

    bool WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
            return false;
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;
        if (!bytes.empty())
        {
            output.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        return static_cast<bool>(output);
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
        output << text;
        return static_cast<bool>(output);
    }

    std::vector<std::uint8_t> ReadBytes(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return {};
        return {
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
    }

    renegade::bridge::CreatorExternalAnimationClip MakeClip(
        const fs::path& source,
        const std::uint32_t sourceIndex,
        std::string name,
        const float start,
        const float end)
    {
        renegade::bridge::CreatorExternalAnimationClip clip;
        clip.localSourcePath = source.generic_u8string();
        clip.sourceDisplayName = source.filename().generic_u8string();
        clip.sourceAnimationIndex = sourceIndex;
        clip.name = std::move(name);
        clip.start = start;
        clip.end = end;
        clip.enabled = true;
        return clip;
    }

    bool TestSharedImmutableSnapshot(const fs::path& root)
    {
        using namespace renegade::bridge;

        const fs::path project = root / "Project";
        const fs::path incoming = root / "Incoming";
        std::error_code ec;
        fs::create_directories(project, ec);
        fs::create_directories(incoming, ec);
        if (ec)
            return Require(false, "could not create staging fixture folders");

        const fs::path source = incoming / "Mixamo Walk.fbx";
        const std::vector<std::uint8_t> firstBytes = {0x46, 0x42, 0x58, 0x01};
        if (!WriteBytes(source, firstBytes))
            return Require(false, "could not create external FBX fixture");

        std::vector<CreatorExternalAnimationClip> clips;
        clips.push_back(MakeClip(source, 0, "Walk", 0.0f, 1.0f));
        clips.push_back(MakeClip(source, 1, "Walk Alt", 1.0f, 2.0f));
        clips[1].enabled = false;

        std::vector<CreatorExternalAnimationImportRecipe> recipe;
        std::string error;
        if (!Require(StageCreatorExternalAnimationClipsForRecipe(
                project.generic_u8string(), clips, recipe, error),
                "FBX source staging failed: " + error) ||
            !Require(recipe.size() == 2,
                "two source actions did not produce two durable recipe rows") ||
            !Require(recipe[0].sourceProjectRelativePath ==
                    recipe[1].sourceProjectRelativePath,
                "actions from the same source did not share one retained snapshot") ||
            !Require(recipe[0].sourceProjectRelativePath.rfind(
                    "SourceAssets/Animations/Snapshots/", 0) == 0,
                "retained source escaped the governed Character animation tree") ||
            !Require(recipe[0].sourceAnimationIndex == 0 &&
                    recipe[1].sourceAnimationIndex == 1,
                "source action indices were not preserved") ||
            !Require(recipe[0].name == "Walk" &&
                    recipe[1].name == "Walk Alt" && !recipe[1].enabled,
                "creator clip names/include state were not preserved"))
        {
            return false;
        }

        const fs::path retained = project / fs::u8path(
            recipe[0].sourceProjectRelativePath);
        if (!Require(fs::is_regular_file(retained),
                "governed source snapshot was not written") ||
            !Require(ReadBytes(retained) == firstBytes,
                "governed source snapshot bytes differ from selected source"))
        {
            return false;
        }

        const std::string firstSnapshot = recipe[0].sourceProjectRelativePath;
        const std::vector<std::uint8_t> changedBytes = {0x46, 0x42, 0x58, 0x02};
        if (!WriteBytes(source, changedBytes))
            return Require(false, "could not revise external FBX fixture");

        recipe.clear();
        if (!Require(StageCreatorExternalAnimationClipsForRecipe(
                project.generic_u8string(), clips, recipe, error),
                "revised FBX source staging failed: " + error) ||
            !Require(!recipe.empty() &&
                    recipe[0].sourceProjectRelativePath != firstSnapshot,
                "changed source bytes reused an old immutable snapshot identity"))
        {
            return false;
        }
        return true;
    }

    bool TestGltfDependencySnapshotIdentity(const fs::path& root)
    {
        using namespace renegade::bridge;

        const fs::path project = root / "GltfProject";
        const fs::path incoming = root / "GltfIncoming";
        std::error_code ec;
        fs::create_directories(project, ec);
        fs::create_directories(incoming, ec);
        if (ec)
            return Require(false, "could not create glTF staging fixture folders");

        const fs::path source = incoming / "Run.gltf";
        const fs::path buffer = incoming / "Run.bin";
        const fs::path image = incoming / "Textures" / "Run.png";
        const std::string gltf =
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"uri\":\"Run.bin\",\"byteLength\":4}],"
            "\"images\":[{\"uri\":\"Textures/Run.png\"}]}";
        if (!WriteText(source, gltf) ||
            !WriteBytes(buffer, {1, 2, 3, 4}) ||
            !WriteBytes(image, {9, 8, 7, 6}))
        {
            return Require(false, "could not create glTF dependency fixtures");
        }

        std::vector<CreatorExternalAnimationClip> clips = {
            MakeClip(source, 0, "Run", 0.0f, 1.0f)};
        std::vector<CreatorExternalAnimationImportRecipe> recipe;
        std::string error;
        if (!Require(StageCreatorExternalAnimationClipsForRecipe(
                project.generic_u8string(), clips, recipe, error),
                "glTF source staging failed: " + error) ||
            !Require(recipe.size() == 1,
                "glTF source did not produce one durable recipe row"))
        {
            return false;
        }

        const fs::path retainedSource = project / fs::u8path(
            recipe[0].sourceProjectRelativePath);
        const fs::path retainedDirectory = retainedSource.parent_path();
        if (!Require(fs::is_regular_file(retainedDirectory / "Run.bin"),
                "glTF buffer dependency was not retained") ||
            !Require(fs::is_regular_file(retainedDirectory / "Textures" / "Run.png"),
                "glTF image dependency was not retained"))
        {
            return false;
        }

        const std::string firstSnapshot = recipe[0].sourceProjectRelativePath;
        if (!WriteBytes(buffer, {1, 2, 3, 5}))
            return Require(false, "could not revise glTF buffer dependency");

        recipe.clear();
        if (!Require(StageCreatorExternalAnimationClipsForRecipe(
                project.generic_u8string(), clips, recipe, error),
                "glTF dependency revision staging failed: " + error) ||
            !Require(!recipe.empty() &&
                    recipe[0].sourceProjectRelativePath != firstSnapshot,
                "changed glTF dependency did not create a new snapshot identity"))
        {
            return false;
        }

        const fs::path unsafeSource = incoming / "Unsafe.gltf";
        if (!WriteText(unsafeSource,
                "{\"asset\":{\"version\":\"2.0\"},"
                "\"buffers\":[{\"uri\":\"../outside.bin\",\"byteLength\":4}]}"))
        {
            return Require(false, "could not create unsafe glTF fixture");
        }
        clips = {MakeClip(unsafeSource, 0, "Unsafe", 0.0f, 1.0f)};
        recipe.clear();
        if (!Require(!StageCreatorExternalAnimationClipsForRecipe(
                project.generic_u8string(), clips, recipe, error) &&
                error.find("escapes its source folder") != std::string::npos,
                "glTF traversal dependency was not rejected"))
        {
            return false;
        }
        return true;
    }

    bool TestQueueSessionIsolation(const fs::path& root)
    {
        using namespace renegade::bridge;
        const fs::path projectA = root / "QueueA";
        const fs::path projectB = root / "QueueB";
        std::error_code ec;
        fs::create_directories(projectA, ec);
        fs::create_directories(projectB, ec);
        if (ec)
            return Require(false, "could not create queue fixture projects");

        BeginCreatorExternalAnimationImportSession(projectA.generic_u8string());
        const auto snapshot = CaptureCreatorExternalAnimationQueue();
        if (!Require(snapshot.projectRoot == projectA.generic_u8string() &&
                snapshot.clips.empty(),
                "new importer session did not start empty for its project"))
        {
            ClearCreatorExternalAnimationImportSession();
            return false;
        }

        std::vector<CreatorExternalAnimationImportRecipe> recipe;
        std::string error;
        if (!Require(StageCreatorExternalAnimationsForRecipe(
                projectA.generic_u8string(), recipe, error) && recipe.empty(),
                "empty active queue should stage as an empty recipe"))
        {
            ClearCreatorExternalAnimationImportSession();
            return false;
        }

        BeginCreatorExternalAnimationImportSession(projectB.generic_u8string());
        if (!Require(CaptureCreatorExternalAnimationQueue().projectRoot ==
                projectB.generic_u8string(),
                "starting a new importer session did not replace old project state"))
        {
            ClearCreatorExternalAnimationImportSession();
            return false;
        }
        ClearCreatorExternalAnimationImportSession();
        return Require(
            CaptureCreatorExternalAnimationQueue().projectRoot.empty(),
            "clearing importer session did not remove transient queue ownership");
    }
}

int main()
{
    const auto token = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-cw02-animation-ingestion-" + std::to_string(token));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    if (ec)
    {
        std::cerr << "CW-02 EXTERNAL ANIMATION INGESTION FAIL // could not create temp root\n";
        return 1;
    }

    const bool passed =
        TestSharedImmutableSnapshot(root) &&
        TestGltfDependencySnapshotIdentity(root) &&
        TestQueueSessionIsolation(root);

    fs::remove_all(root, ec);
    if (!passed)
        return 1;

    std::cout << "CW-02 external animation ingestion contract PASS\n";
    return 0;
}
