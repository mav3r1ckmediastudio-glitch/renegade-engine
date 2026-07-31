#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace renegade::bridge
{
    // A real (not mocked) view of a project's Content directory, refreshed on
    // demand. This is intentionally read-only: it lists what is on disk, it
    // does not import, convert or track assets. Asset import is a separate,
    // not-yet-started capability (see docs/PHASE3_STUDIO_SHELL_REBUILD.md).
    struct ContentFolder
    {
        std::string name;           // display name, e.g. "Scenes"
        std::string relativePath;   // path relative to Content/, "" for root
        int depth = 0;
    };

    enum class ContentAssetType
    {
        Scene,
        Mesh,
        Material,
        Texture,
        Shader,
        Other,
    };

    struct ContentAsset
    {
        std::string name;            // file name without extension
        std::string fileName;        // file name with extension
        std::string relativeFolder;  // folder this asset lives in, relative to Content/
        ContentAssetType type = ContentAssetType::Other;
    };

    class ContentBrowserService
    {
    public:
        // Rescans contentRootPath (a project's "Content" directory) from
        // disk. Safe to call with a path that does not exist yet (empty
        // result rather than an error): a freshly created project may not
        // have every folder until something is saved into it.
        void Refresh(const std::string& contentRootPath);

        void Clear() noexcept;

        [[nodiscard]] const std::vector<ContentFolder>& Folders() const noexcept
        {
            return folders_;
        }

        [[nodiscard]] std::vector<ContentAsset> AssetsInFolder(
            const std::string& relativeFolder) const;

        [[nodiscard]] const std::string& SelectedFolder() const noexcept
        {
            return selectedFolder_;
        }

        void SelectFolder(const std::string& relativeFolder);

        [[nodiscard]] static ContentAssetType ClassifyExtension(
            const std::string& extension);
        [[nodiscard]] static const char* TypeLabel(ContentAssetType type);

    private:
        std::vector<ContentFolder> folders_;
        std::vector<ContentAsset> assets_;
        std::string selectedFolder_;
    };
}
