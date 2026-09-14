#include "renegade/bridge/CreatorExternalAnimationImportService.h"

#include "ModelImporter.h"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace renegade::bridge
{
    namespace
    {
        constexpr std::uint64_t FnvOffset = 14695981039346656037ull;
        constexpr std::uint64_t FnvPrime = 1099511628211ull;

        struct QueueState
        {
            std::string projectRoot;
            std::vector<CreatorExternalAnimationClip> clips;
            std::string error;
        };

        struct GltfDependency
        {
            fs::path relativePath;
            fs::path sourcePath;
        };

        QueueState g_queue;
        std::mutex g_queueMutex;

        std::string Trim(std::string value)
        {
            const auto notSpace = [](const unsigned char ch)
            {
                return std::isspace(ch) == 0;
            };
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
            value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
            return value;
        }

        std::string LowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](const unsigned char ch)
                {
                    return static_cast<char>(std::tolower(ch));
                });
            return value;
        }

        std::string SafeStem(std::string stem)
        {
            if (stem.empty())
                stem = "animation";
            for (char& ch : stem)
            {
                const unsigned char value = static_cast<unsigned char>(ch);
                if (!(std::isalnum(value) || ch == '-' || ch == '_'))
                    ch = '_';
            }
            return stem;
        }

        void HashAppendText(std::uint64_t& hash, const std::string& text)
        {
            for (const unsigned char byte : text)
            {
                hash ^= byte;
                hash *= FnvPrime;
            }
            hash ^= 0xffu;
            hash *= FnvPrime;
        }

        bool HashAppendFile(
            const fs::path& path,
            std::uint64_t& hash,
            std::string& error)
        {
            std::ifstream input(path, std::ios::binary);
            if (!input)
            {
                error = "Could not read Character animation source for governed staging: " +
                    path.generic_u8string();
                return false;
            }

            char buffer[64 * 1024];
            while (input)
            {
                input.read(buffer, sizeof(buffer));
                const auto count = input.gcount();
                for (std::streamsize i = 0; i < count; ++i)
                {
                    hash ^= static_cast<unsigned char>(buffer[i]);
                    hash *= FnvPrime;
                }
            }
            if (!input.eof())
            {
                error = "Could not finish reading Character animation source for governed staging: " +
                    path.generic_u8string();
                return false;
            }
            return true;
        }

        std::string HashText(const std::uint64_t hash)
        {
            std::ostringstream stream;
            stream << std::hex << std::setfill('0') << std::setw(16) << hash;
            return stream.str();
        }

        bool IsSafeRelativeDependency(const fs::path& path)
        {
            if (path.empty() || path.is_absolute() || path.has_root_name() ||
                path.has_root_directory())
            {
                return false;
            }
            for (const auto& component : path)
            {
                if (component == "..")
                    return false;
            }
            return true;
        }

        bool CollectGltfDependencies(
            const fs::path& source,
            std::vector<GltfDependency>& dependencies,
            std::string& error)
        {
            dependencies.clear();
            if (LowerAscii(source.extension().generic_u8string()) != ".gltf")
                return true;

            nlohmann::json document;
            try
            {
                std::ifstream input(source);
                if (!input)
                {
                    error = "Could not read glTF Character animation source dependencies: " +
                        source.generic_u8string();
                    return false;
                }
                input >> document;
            }
            catch (const nlohmann::json::exception& exception)
            {
                error = std::string("Could not parse glTF Character animation source dependencies: ") +
                    exception.what();
                return false;
            }

            std::set<std::string> seen;
            const auto collectUris = [&](const char* key) -> bool
            {
                const auto found = document.find(key);
                if (found == document.end())
                    return true;
                if (!found->is_array())
                {
                    error = std::string("glTF Character animation '") + key +
                        "' member must be an array.";
                    return false;
                }

                for (const auto& entry : *found)
                {
                    if (!entry.is_object())
                        continue;
                    const auto uriIt = entry.find("uri");
                    if (uriIt == entry.end())
                        continue;
                    if (!uriIt->is_string())
                    {
                        error = "glTF Character animation dependency URI must be a string.";
                        return false;
                    }

                    const std::string uri = uriIt->get<std::string>();
                    if (uri.empty() || uri.rfind("data:", 0) == 0 ||
                        uri.find("://") != std::string::npos)
                    {
                        continue;
                    }

                    const fs::path relative = fs::u8path(uri).lexically_normal();
                    if (!IsSafeRelativeDependency(relative))
                    {
                        error = "glTF Character animation dependency escapes its source folder and cannot be retained safely: " +
                            uri;
                        return false;
                    }

                    const fs::path dependency = source.parent_path() / relative;
                    std::error_code ec;
                    if (!fs::is_regular_file(dependency, ec) || ec)
                    {
                        error = "glTF Character animation dependency is missing: " +
                            dependency.generic_u8string();
                        return false;
                    }

                    const std::string keyText = relative.generic_u8string();
                    if (seen.insert(keyText).second)
                        dependencies.push_back({relative, dependency});
                }
                return true;
            };

            if (!collectUris("buffers") || !collectUris("images"))
                return false;

            std::sort(dependencies.begin(), dependencies.end(),
                [](const GltfDependency& lhs, const GltfDependency& rhs)
                {
                    return lhs.relativePath.generic_u8string() <
                        rhs.relativePath.generic_u8string();
                });
            return true;
        }

        bool HashSourceSnapshot(
            const fs::path& source,
            const std::vector<GltfDependency>& dependencies,
            std::uint64_t& hash,
            std::string& error)
        {
            hash = FnvOffset;
            HashAppendText(hash, source.filename().generic_u8string());
            if (!HashAppendFile(source, hash, error))
                return false;

            for (const auto& dependency : dependencies)
            {
                HashAppendText(hash, dependency.relativePath.generic_u8string());
                if (!HashAppendFile(dependency.sourcePath, hash, error))
                    return false;
            }
            return true;
        }

        bool CopyGltfDependencies(
            const std::vector<GltfDependency>& dependencies,
            const fs::path& destinationDirectory,
            std::string& error)
        {
            for (const auto& dependency : dependencies)
            {
                const fs::path destination =
                    destinationDirectory / dependency.relativePath;
                std::error_code ec;
                fs::create_directories(destination.parent_path(), ec);
                if (ec)
                {
                    error = "Could not create retained glTF Character animation dependency folder: " +
                        ec.message();
                    return false;
                }
                fs::copy_file(
                    dependency.sourcePath,
                    destination,
                    fs::copy_options::overwrite_existing,
                    ec);
                if (ec)
                {
                    error = "Could not retain glTF Character animation dependency " +
                        dependency.relativePath.generic_u8string() + ": " + ec.message();
                    return false;
                }
            }
            return true;
        }

        bool LoadAnimationSource(
            const std::string& sourcePath,
            const HumanoidAnimationSourceFormat format,
            wi::scene::Scene& scene,
            std::string& error)
        {
            try
            {
                switch (format)
                {
                case HumanoidAnimationSourceFormat::Wiscene:
                    wi::scene::LoadModel(scene, sourcePath);
                    break;
                case HumanoidAnimationSourceFormat::Fbx:
                    ImportModel_FBX(sourcePath, scene);
                    break;
                case HumanoidAnimationSourceFormat::Gltf:
                case HumanoidAnimationSourceFormat::Glb:
                case HumanoidAnimationSourceFormat::Vrm:
                case HumanoidAnimationSourceFormat::Vrma:
                    ImportModel_GLTF(sourcePath, scene);
                    break;
                default:
                    error = "Unsupported Character animation source: " + sourcePath;
                    return false;
                }
            }
            catch (const std::exception& exception)
            {
                error = std::string("Character animation source import failed: ") +
                    exception.what();
                return false;
            }
            catch (...)
            {
                error = "Character animation source import failed with an unknown error.";
                return false;
            }

            if (scene.animations.GetCount() == 0)
            {
                error = std::string(HumanoidAnimationSourceFormatName(format)) +
                    " source contains no native Wicked animation clips.";
                return false;
            }
            return true;
        }

        bool SameSource(
            const CreatorExternalAnimationClip& clip,
            const std::string& sourcePath)
        {
            std::error_code ec;
            const fs::path lhs =
                fs::weakly_canonical(fs::u8path(clip.localSourcePath), ec);
            if (ec)
                return clip.localSourcePath == sourcePath;
            ec.clear();
            const fs::path rhs =
                fs::weakly_canonical(fs::u8path(sourcePath), ec);
            return !ec && lhs == rhs;
        }

        bool RetainSource(
            const fs::path& projectRoot,
            const std::string& localSourcePath,
            std::string& projectRelativePath,
            std::string& error)
        {
            const fs::path source = fs::u8path(localSourcePath);
            std::error_code ec;
            if (!fs::is_regular_file(source, ec) || ec)
            {
                error = "Character animation source no longer exists: " +
                    localSourcePath;
                return false;
            }

            std::vector<GltfDependency> dependencies;
            if (!CollectGltfDependencies(source, dependencies, error))
                return false;

            std::uint64_t hash = 0;
            if (!HashSourceSnapshot(source, dependencies, hash, error))
                return false;

            const std::string folderName =
                SafeStem(source.stem().generic_u8string()) + "_" + HashText(hash);
            const fs::path snapshotDirectory =
                projectRoot / "SourceAssets" / "Animations" / "Snapshots" /
                fs::u8path(folderName);
            fs::create_directories(snapshotDirectory, ec);
            if (ec)
            {
                error = "Could not create governed Character animation source folder: " +
                    ec.message();
                return false;
            }

            const fs::path retainedSource = snapshotDirectory / source.filename();
            if (!fs::is_regular_file(retainedSource, ec) || ec)
            {
                ec.clear();
                fs::copy_file(source, retainedSource, fs::copy_options::none, ec);
                if (ec)
                {
                    error = "Could not retain selected Character animation source: " +
                        ec.message();
                    return false;
                }
            }

            if (!CopyGltfDependencies(dependencies, snapshotDirectory, error))
                return false;

            projectRelativePath =
                fs::relative(retainedSource, projectRoot, ec).generic_u8string();
            if (ec || projectRelativePath.empty())
            {
                error = "Could not make retained Character animation source project-relative.";
                return false;
            }
            return true;
        }

        bool ValidateClip(
            const CreatorExternalAnimationClip& clip,
            std::string& error)
        {
            if (clip.localSourcePath.empty())
            {
                error = "External Character animation clip is missing its selected source file.";
                return false;
            }
            if (Trim(clip.name).empty())
            {
                error = "External Character animation clip name cannot be empty.";
                return false;
            }
            if (!std::isfinite(clip.start) || !std::isfinite(clip.end) ||
                clip.end < clip.start)
            {
                error = "External Character animation clip has an invalid source range.";
                return false;
            }
            return true;
        }
    }

    void BeginCreatorExternalAnimationImportSession(const std::string& projectRoot)
    {
        std::scoped_lock lock(g_queueMutex);
        g_queue = {};
        g_queue.projectRoot = projectRoot;
    }

    void ClearCreatorExternalAnimationImportSession() noexcept
    {
        std::scoped_lock lock(g_queueMutex);
        g_queue = {};
    }

    bool QueueCreatorExternalAnimationSource(
        const std::string& sourcePath,
        std::string& error)
    {
        error.clear();
        if (sourcePath.empty())
        {
            error = "Choose an animation source file first.";
            return false;
        }

        {
            std::scoped_lock lock(g_queueMutex);
            if (g_queue.projectRoot.empty())
            {
                error = "Character animation import session is not active.";
                return false;
            }
            if (std::any_of(g_queue.clips.begin(), g_queue.clips.end(),
                [&](const CreatorExternalAnimationClip& clip)
                {
                    return SameSource(clip, sourcePath);
                }))
            {
                return true;
            }
        }

        std::error_code ec;
        if (!fs::is_regular_file(fs::u8path(sourcePath), ec) || ec)
        {
            error = "Character animation source does not exist: " + sourcePath;
            return false;
        }

        const auto format = ClassifyHumanoidAnimationSource(sourcePath);
        if (format == HumanoidAnimationSourceFormat::Unknown)
        {
            error = "Unsupported Character animation source: " + sourcePath;
            return false;
        }

        wi::scene::Scene sourceScene;
        if (!LoadAnimationSource(sourcePath, format, sourceScene, error))
            return false;

        const fs::path source = fs::u8path(sourcePath);
        const std::string displayName = source.filename().generic_u8string();
        const std::string sourceStem = source.stem().generic_u8string();
        const std::size_t count = sourceScene.animations.GetCount();
        std::vector<CreatorExternalAnimationClip> discovered;
        discovered.reserve(count);

        for (std::size_t i = 0; i < count; ++i)
        {
            const auto entity = sourceScene.animations.GetEntity(i);
            const auto* animation = sourceScene.animations.GetComponent(entity);
            if (animation == nullptr)
                continue;

            const auto* sourceName = sourceScene.names.GetComponent(entity);
            const std::string actionName = sourceName != nullptr
                ? Trim(sourceName->name)
                : std::string{};

            CreatorExternalAnimationClip clip;
            clip.localSourcePath = sourcePath;
            clip.sourceDisplayName = displayName;
            clip.sourceFormat = format;
            clip.sourceAnimationIndex = static_cast<std::uint32_t>(i);
            clip.sourceActionName = actionName;
            if (count == 1)
            {
                // Mixamo and similar one-action-per-file workflows get the
                // useful filename as their creator-facing default clip name.
                clip.name = sourceStem.empty() ? "Animation" : sourceStem;
            }
            else if (!actionName.empty())
            {
                clip.name = actionName;
            }
            else
            {
                clip.name = (sourceStem.empty() ? "Animation" : sourceStem) +
                    " " + std::to_string(i + 1);
            }
            clip.start = animation->start;
            clip.end = animation->end;
            clip.enabled = true;
            discovered.push_back(std::move(clip));
        }

        if (discovered.empty())
        {
            error = "Character animation source did not expose any usable native clips.";
            return false;
        }

        std::scoped_lock lock(g_queueMutex);
        if (std::any_of(g_queue.clips.begin(), g_queue.clips.end(),
            [&](const CreatorExternalAnimationClip& clip)
            {
                return SameSource(clip, sourcePath);
            }))
        {
            return true;
        }
        g_queue.clips.insert(
            g_queue.clips.end(),
            std::make_move_iterator(discovered.begin()),
            std::make_move_iterator(discovered.end()));
        g_queue.error.clear();
        return true;
    }

    CreatorExternalAnimationQueueSnapshot CaptureCreatorExternalAnimationQueue()
    {
        std::scoped_lock lock(g_queueMutex);
        CreatorExternalAnimationQueueSnapshot snapshot;
        snapshot.projectRoot = g_queue.projectRoot;
        snapshot.clips = g_queue.clips;
        snapshot.error = g_queue.error;
        return snapshot;
    }

    bool RenameCreatorExternalAnimationClip(
        const std::size_t index,
        const std::string& name,
        std::string& error)
    {
        const std::string trimmed = Trim(name);
        if (trimmed.empty())
        {
            error = "External animation clip name cannot be empty.";
            return false;
        }

        std::scoped_lock lock(g_queueMutex);
        if (index >= g_queue.clips.size())
        {
            error = "External animation clip selection is no longer valid.";
            return false;
        }
        g_queue.clips[index].name = trimmed;
        error.clear();
        return true;
    }

    bool SetCreatorExternalAnimationClipEnabled(
        const std::size_t index,
        const bool enabled,
        std::string& error)
    {
        std::scoped_lock lock(g_queueMutex);
        if (index >= g_queue.clips.size())
        {
            error = "External animation clip selection is no longer valid.";
            return false;
        }
        g_queue.clips[index].enabled = enabled;
        error.clear();
        return true;
    }

    bool RemoveCreatorExternalAnimationClip(
        const std::size_t index,
        std::string& error)
    {
        std::scoped_lock lock(g_queueMutex);
        if (index >= g_queue.clips.size())
        {
            error = "External animation clip selection is no longer valid.";
            return false;
        }
        g_queue.clips.erase(
            g_queue.clips.begin() + static_cast<std::ptrdiff_t>(index));
        error.clear();
        return true;
    }

    bool StageCreatorExternalAnimationClipsForRecipe(
        const std::string& projectRoot,
        const std::vector<CreatorExternalAnimationClip>& clips,
        std::vector<CreatorExternalAnimationImportRecipe>& recipe,
        std::string& error)
    {
        recipe.clear();
        error.clear();
        if (clips.empty())
            return true;
        if (projectRoot.empty())
        {
            error = "Character animation staging requires an active project root.";
            return false;
        }

        const fs::path root = fs::u8path(projectRoot);
        std::error_code ec;
        if (!fs::is_directory(root, ec) || ec)
        {
            error = "Character animation staging project root is unavailable.";
            return false;
        }

        std::unordered_map<std::string, std::string> retainedByLocalSource;
        recipe.reserve(clips.size());
        for (const auto& clip : clips)
        {
            if (!ValidateClip(clip, error))
                return false;

            auto found = retainedByLocalSource.find(clip.localSourcePath);
            if (found == retainedByLocalSource.end())
            {
                std::string retained;
                if (!RetainSource(root, clip.localSourcePath, retained, error))
                    return false;
                found = retainedByLocalSource.emplace(
                    clip.localSourcePath, std::move(retained)).first;
            }

            CreatorExternalAnimationImportRecipe item;
            item.sourceProjectRelativePath = found->second;
            item.sourceAnimationIndex = clip.sourceAnimationIndex;
            item.name = Trim(clip.name);
            item.start = clip.start;
            item.end = clip.end;
            item.enabled = clip.enabled;
            recipe.push_back(std::move(item));
        }
        return true;
    }

    bool StageCreatorExternalAnimationsForRecipe(
        const std::string& projectRoot,
        std::vector<CreatorExternalAnimationImportRecipe>& recipe,
        std::string& error)
    {
        CreatorExternalAnimationQueueSnapshot snapshot;
        {
            std::scoped_lock lock(g_queueMutex);
            snapshot.projectRoot = g_queue.projectRoot;
            snapshot.clips = g_queue.clips;
            snapshot.error = g_queue.error;
        }

        if (snapshot.clips.empty())
        {
            recipe.clear();
            error.clear();
            return true;
        }
        if (snapshot.projectRoot.empty() || projectRoot.empty() ||
            snapshot.projectRoot != projectRoot)
        {
            recipe.clear();
            error = "Character animation import session belongs to a different or inactive project.";
            return false;
        }
        return StageCreatorExternalAnimationClipsForRecipe(
            projectRoot, snapshot.clips, recipe, error);
    }
}
