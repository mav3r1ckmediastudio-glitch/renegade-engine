#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    enum class AssetType : std::uint8_t
    {
        Folder,
        Scene,
        Model,
        Prefab,
        Material,
        Texture,
        Audio,
        Video,
        Vegetation,
        Character,
        Player,
        Weapon,
        Projectile,
        Particle,
        Script,
        UserInterface,
        Data,
        Generated,
        Unknown,
    };

    struct AssetFolderEntry
    {
        std::string name;
        std::string projectRelativePath;
        std::uint32_t depth = 0;
        bool selected = false;
    };

    struct AssetEntry
    {
        std::string name;
        std::string projectRelativePath;
        AssetType type = AssetType::Unknown;
        bool directory = false;
    };

    struct AssetBrowserSnapshot
    {
        std::string projectRoot;
        std::string contentRoot;
        std::string currentFolder = "Content";
        std::vector<AssetFolderEntry> folders;
        std::vector<AssetEntry> assets;
        std::string error;
        bool succeeded = false;
    };

    // Filesystem-backed project browser. The active project's Content folder
    // is authoritative; this service indexes those real local files and never
    // redirects project content into the Renegade installation.
    class AssetBrowserService
    {
    public:
        [[nodiscard]] AssetBrowserSnapshot Scan(
            const std::string& projectRoot,
            const std::string& currentFolder = "Content") const;

        [[nodiscard]] static AssetType Classify(
            const std::string& projectRelativePath) noexcept;
        [[nodiscard]] static const char* TypeLabel(AssetType type) noexcept;
    };
}
