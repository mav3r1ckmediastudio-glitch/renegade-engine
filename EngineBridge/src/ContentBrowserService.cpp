#include "renegade/bridge/ContentBrowserService.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        std::string ToLower(std::string value)
        {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }
    }

    void ContentBrowserService::Clear() noexcept
    {
        folders_.clear();
        assets_.clear();
        selectedFolder_.clear();
    }

    void ContentBrowserService::Refresh(const std::string& contentRootPath)
    {
        Clear();

        std::error_code exists_error;
        const fs::path root = fs::u8path(contentRootPath);
        if (contentRootPath.empty() || !fs::exists(root, exists_error))
        {
            // Not an error: a brand new project may not have a populated
            // Content directory yet. An empty browser is the honest result.
            return;
        }

        folders_.push_back(ContentFolder{"Content", "", 0});

        std::error_code walk_error;
        for (auto it = fs::recursive_directory_iterator(
                 root,
                 fs::directory_options::skip_permission_denied,
                 walk_error);
             it != fs::recursive_directory_iterator();
             it.increment(walk_error))
        {
            if (walk_error)
            {
                break;
            }

            const fs::path& path = it->path();
            std::error_code relative_error;
            const fs::path relative = fs::relative(path, root, relative_error);
            if (relative_error)
            {
                continue;
            }

            if (it->is_directory())
            {
                ContentFolder folder;
                folder.name = path.filename().generic_u8string();
                folder.relativePath = relative.generic_u8string();
                folder.depth = static_cast<int>(it.depth()) + 1;
                folders_.push_back(std::move(folder));
                continue;
            }

            if (!it->is_regular_file())
            {
                continue;
            }

            ContentAsset asset;
            asset.fileName = path.filename().generic_u8string();
            asset.name = path.stem().generic_u8string();
            const fs::path parent = relative.parent_path();
            asset.relativeFolder = parent.generic_u8string();
            asset.type = ClassifyExtension(ToLower(path.extension().generic_u8string()));
            assets_.push_back(std::move(asset));
        }

        selectedFolder_.clear();
    }

    std::vector<ContentAsset> ContentBrowserService::AssetsInFolder(
        const std::string& relativeFolder) const
    {
        std::vector<ContentAsset> result;
        for (const auto& asset : assets_)
        {
            if (asset.relativeFolder == relativeFolder)
            {
                result.push_back(asset);
            }
        }
        return result;
    }

    void ContentBrowserService::SelectFolder(const std::string& relativeFolder)
    {
        selectedFolder_ = relativeFolder;
    }

    ContentAssetType ContentBrowserService::ClassifyExtension(
        const std::string& extension)
    {
        if (extension == ".wiscene")
        {
            return ContentAssetType::Scene;
        }
        if (extension == ".obj" || extension == ".fbx" || extension == ".gltf" ||
            extension == ".glb" || extension == ".vrm" || extension == ".vrma" ||
            extension == ".ply")
        {
            return ContentAssetType::Mesh;
        }
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
            extension == ".dds" || extension == ".ktx2" || extension == ".tga" ||
            extension == ".hdr" || extension == ".exr")
        {
            return ContentAssetType::Texture;
        }
        if (extension == ".hlsl" || extension == ".hlsli")
        {
            return ContentAssetType::Shader;
        }
        // A bare ".material" convention does not exist upstream; materials
        // today live inside .wiscene, so there is no separate material file
        // to classify. Left here so the type exists once/if that changes.
        return ContentAssetType::Other;
    }

    const char* ContentBrowserService::TypeLabel(const ContentAssetType type)
    {
        switch (type)
        {
        case ContentAssetType::Scene: return "Scene";
        case ContentAssetType::Mesh: return "Mesh";
        case ContentAssetType::Material: return "Material";
        case ContentAssetType::Texture: return "Texture";
        case ContentAssetType::Shader: return "Shader";
        default: return "File";
        }
    }
}
