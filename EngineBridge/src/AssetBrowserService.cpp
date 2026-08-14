#include "renegade/bridge/AssetBrowserService.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <iterator>
#include <system_error>
#include <utility>

namespace
{
    namespace fs = std::filesystem;

    std::string Lower(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](const unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return value;
    }

    bool IsSafeProjectRelativePath(const fs::path& path)
    {
        if (path.empty() || path.is_absolute())
        {
            return false;
        }
        return std::none_of(
            path.begin(),
            path.end(),
            [](const fs::path& part)
            {
                return part == "..";
            });
    }

    bool IsWithin(const fs::path& child, const fs::path& parent)
    {
        auto childPart = child.begin();
        for (auto parentPart = parent.begin();
            parentPart != parent.end();
            ++parentPart, ++childPart)
        {
            if (childPart == child.end() || *childPart != *parentPart)
            {
                return false;
            }
        }
        return true;
    }

    std::uint32_t PathDepth(const fs::path& path)
    {
        return static_cast<std::uint32_t>(
            std::distance(path.begin(), path.end()));
    }

    std::string ContentCategory(const std::string& relativePath)
    {
        const fs::path path = fs::u8path(relativePath);
        bool foundContent = false;
        for (const auto& part : path)
        {
            const std::string lower = Lower(part.u8string());
            if (foundContent)
            {
                return lower;
            }
            foundContent = lower == "content";
        }
        return {};
    }

    bool CaseInsensitiveNameLess(
        const renegade::bridge::AssetEntry& left,
        const renegade::bridge::AssetEntry& right)
    {
        if (left.directory != right.directory)
        {
            return left.directory;
        }
        return Lower(left.name) < Lower(right.name);
    }

    bool IsResourceCategory(const std::string& category)
    {
        return category == "textures" ||
            category == "audio" ||
            category == "video" ||
            category == "fonts" ||
            category == "scripts";
    }

    renegade::bridge::AssetType ResourceCategoryType(
        const std::string& category)
    {
        using renegade::bridge::AssetType;
        if (category == "textures") return AssetType::Texture;
        if (category == "audio") return AssetType::Audio;
        if (category == "video") return AssetType::Video;
        if (category == "fonts") return AssetType::Font;
        if (category == "scripts") return AssetType::Script;
        return AssetType::Unknown;
    }
}

namespace renegade::bridge
{
    AssetBrowserSnapshot AssetBrowserService::Scan(
        const std::string& projectRoot,
        const std::string& currentFolder) const
    {
        AssetBrowserSnapshot result;
        result.projectRoot = projectRoot;

        if (projectRoot.empty())
        {
            result.error =
                "Open or create a Renegade project before browsing assets.";
            return result;
        }

        try
        {
            const fs::path root =
                fs::absolute(fs::u8path(projectRoot)).lexically_normal();
            const fs::path content = (root / "Content").lexically_normal();
            result.projectRoot = root.generic_u8string();
            result.contentRoot = content.generic_u8string();

            if (!fs::is_directory(root))
            {
                result.error = "Project root does not exist: " +
                    root.generic_u8string();
                return result;
            }
            if (!fs::is_directory(content))
            {
                result.error = "Project Content folder does not exist: " +
                    content.generic_u8string();
                return result;
            }

            const fs::path requested = currentFolder.empty()
                ? fs::path("Content")
                : fs::u8path(currentFolder).lexically_normal();
            if (!IsSafeProjectRelativePath(requested))
            {
                result.error =
                    "Asset Browser paths must remain inside project Content.";
                return result;
            }

            std::error_code canonicalError;
            const fs::path canonicalContent =
                fs::weakly_canonical(content, canonicalError);
            fs::path selected =
                fs::weakly_canonical(root / requested, canonicalError);
            if (canonicalError || !fs::is_directory(selected) ||
                !IsWithin(selected, canonicalContent))
            {
                selected = canonicalContent;
            }

            result.currentFolder =
                fs::relative(selected, root).generic_u8string();

            AssetFolderEntry contentEntry;
            contentEntry.name = "Content";
            contentEntry.projectRelativePath = "Content";
            contentEntry.depth = 0;
            contentEntry.selected = selected == canonicalContent;
            result.folders.push_back(std::move(contentEntry));

            std::error_code iterationError;
            fs::recursive_directory_iterator iterator(
                canonicalContent,
                fs::directory_options::skip_permission_denied,
                iterationError);
            const fs::recursive_directory_iterator end;
            for (; iterator != end; iterator.increment(iterationError))
            {
                if (iterationError)
                {
                    iterationError.clear();
                    continue;
                }
                if (!iterator->is_directory(iterationError))
                {
                    continue;
                }

                const fs::path folder = iterator->path().lexically_normal();
                AssetFolderEntry entry;
                entry.name = folder.filename().u8string();
                entry.projectRelativePath =
                    fs::relative(folder, root).generic_u8string();
                entry.depth = PathDepth(
                    fs::relative(folder, canonicalContent));
                entry.selected = folder == selected;
                result.folders.push_back(std::move(entry));
            }

            std::sort(
                result.folders.begin() + 1,
                result.folders.end(),
                [](const AssetFolderEntry& left,
                    const AssetFolderEntry& right)
                {
                    return Lower(left.projectRelativePath) <
                        Lower(right.projectRelativePath);
                });

            fs::directory_iterator childIterator(
                selected,
                fs::directory_options::skip_permission_denied,
                iterationError);
            const fs::directory_iterator childEnd;
            for (; childIterator != childEnd;
                childIterator.increment(iterationError))
            {
                if (iterationError)
                {
                    iterationError.clear();
                    continue;
                }

                const bool directory =
                    childIterator->is_directory(iterationError);
                const bool regular =
                    childIterator->is_regular_file(iterationError);
                if (!directory && !regular)
                {
                    continue;
                }

                const fs::path path = childIterator->path().lexically_normal();
                const std::string filename = path.filename().u8string();
                if (!directory && filename.size() >= 5 &&
                    Lower(filename.substr(filename.size() - 5)) == ".meta")
                {
                    continue;
                }
                const std::string lowerFilename = Lower(filename);
                constexpr const char* ThumbnailSuffix = ".thumbnail.png";
                if (!directory &&
                    lowerFilename.size() >= std::char_traits<char>::length(ThumbnailSuffix) &&
                    lowerFilename.compare(
                        lowerFilename.size() - std::char_traits<char>::length(ThumbnailSuffix),
                        std::char_traits<char>::length(ThumbnailSuffix),
                        ThumbnailSuffix) == 0)
                {
                    continue;
                }

                AssetEntry entry;
                entry.name = filename;
                entry.projectRelativePath =
                    fs::relative(path, root).generic_u8string();
                entry.directory = directory;
                entry.type = directory
                    ? AssetType::Folder
                    : Classify(entry.projectRelativePath);
                result.assets.push_back(std::move(entry));
            }

            std::sort(
                result.assets.begin(),
                result.assets.end(),
                CaseInsensitiveNameLess);
            result.succeeded = true;
            return result;
        }
        catch (const std::exception& exception)
        {
            result.error =
                std::string("Could not index project Content: ") +
                exception.what();
            return result;
        }
    }

    AssetType AssetBrowserService::Classify(
        const std::string& projectRelativePath) noexcept
    {
        const std::string extension = Lower(
            fs::u8path(projectRelativePath).extension().u8string());

        // LP08 resource support is extension/capability driven. A file does not
        // become a supported texture/audio/video/script/font merely because it
        // was dropped into a similarly named Content folder.
        if (extension == ".png" || extension == ".jpg" ||
            extension == ".jpeg" || extension == ".dds" ||
            extension == ".tga" || extension == ".bmp" ||
            extension == ".hdr")
        {
            return AssetType::Texture;
        }
        if (extension == ".wav" || extension == ".ogg")
        {
            return AssetType::Audio;
        }
        if (extension == ".mp4" || extension == ".h264")
        {
            return AssetType::Video;
        }
        if (extension == ".ttf")
        {
            return AssetType::Font;
        }
        if (extension == ".lua")
        {
            return AssetType::Script;
        }

        const std::string category = ContentCategory(projectRelativePath);
        if (IsResourceCategory(category))
        {
            // Governed products use .rasset and may be projected by their
            // canonical resource folder until catalogue metadata owns the type.
            // Any other unrecognised extension remains honestly unsupported.
            return extension == ".rasset"
                ? ResourceCategoryType(category)
                : AssetType::Unknown;
        }

        if (category == "scenes") return AssetType::Scene;
        if (category == "models") return AssetType::Model;
        if (category == "prefabs") return AssetType::Prefab;
        if (category == "materials") return AssetType::Material;
        if (category == "vegetation") return AssetType::Vegetation;
        if (category == "characters") return AssetType::Character;
        if (category == "player") return AssetType::Player;
        if (category == "weapons") return AssetType::Weapon;
        if (category == "projectiles") return AssetType::Projectile;
        if (category == "particles") return AssetType::Particle;
        if (category == "ui") return AssetType::UserInterface;
        if (category == "data") return AssetType::Data;
        if (category == "generated") return AssetType::Generated;

        if (extension == ".wiscene") return AssetType::Scene;
        if (extension == ".glb" || extension == ".gltf" ||
            extension == ".fbx" || extension == ".obj" ||
            extension == ".ply" || extension == ".vrm")
        {
            return AssetType::Model;
        }
        if (extension == ".ini" || extension == ".material")
        {
            return AssetType::Material;
        }
        return AssetType::Unknown;
    }

    const char* AssetBrowserService::TypeLabel(const AssetType type) noexcept
    {
        switch (type)
        {
        case AssetType::Folder: return "FOLDER";
        case AssetType::Scene: return "SCENE";
        case AssetType::Model: return "MODEL";
        case AssetType::Prefab: return "PREFAB";
        case AssetType::Material: return "MATERIAL";
        case AssetType::Texture: return "TEXTURE";
        case AssetType::Audio: return "AUDIO";
        case AssetType::Video: return "VIDEO";
        case AssetType::Font: return "FONT";
        case AssetType::Vegetation: return "VEGETATION";
        case AssetType::Character: return "CHARACTER";
        case AssetType::Player: return "PLAYER";
        case AssetType::Weapon: return "WEAPON";
        case AssetType::Projectile: return "PROJECTILE";
        case AssetType::Particle: return "PARTICLE";
        case AssetType::Script: return "SCRIPT";
        case AssetType::UserInterface: return "UI";
        case AssetType::Data: return "DATA";
        case AssetType::Generated: return "GENERATED";
        case AssetType::Unknown:
        default:
            return "ASSET";
        }
    }
}
