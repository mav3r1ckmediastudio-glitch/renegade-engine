#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "renegade/bridge/AssetBrowserService.h"

namespace
{
    namespace fs = std::filesystem;

    int Fail(const fs::path& root, const char* message)
    {
        std::error_code ignored;
        fs::remove_all(root, ignored);
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    void Touch(const fs::path& path)
    {
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file.put('\0');
    }

    bool ContainsFolder(
        const renegade::bridge::AssetBrowserSnapshot& snapshot,
        const std::string& path)
    {
        return std::any_of(
            snapshot.folders.begin(),
            snapshot.folders.end(),
            [&path](const renegade::bridge::AssetFolderEntry& folder)
            {
                return folder.projectRelativePath == path;
            });
    }

    const renegade::bridge::AssetEntry* FindAsset(
        const renegade::bridge::AssetBrowserSnapshot& snapshot,
        const std::string& name)
    {
        const auto found = std::find_if(
            snapshot.assets.begin(),
            snapshot.assets.end(),
            [&name](const renegade::bridge::AssetEntry& asset)
            {
                return asset.name == name;
            });
        return found == snapshot.assets.end() ? nullptr : &*found;
    }
}

int main()
{
    using renegade::bridge::AssetBrowserService;
    using renegade::bridge::AssetType;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-asset-browser-" + std::to_string(unique));

    Touch(root / "Content/Scenes/Main.wiscene");
    Touch(root / "Content/Models/Props/Crate.wiscene");
    Touch(root / "Content/Models/Props/Crate.thumbnail.png");
    Touch(root / "Content/Materials/Water/RiverWater.ini");
    Touch(root / "Content/Audio/SFX/impact.wav");
    Touch(root / "Content/Scripts/gameplay.lua");
    Touch(root / "Saved/Thumbnails/ignored.png");

    const AssetBrowserService browser;
    const auto models = browser.Scan(
        root.generic_u8string(),
        "Content/Models");
    if (!models.succeeded)
    {
        return Fail(root, "Content/Models scan failed");
    }
    if (models.currentFolder != "Content/Models")
    {
        return Fail(root, "current folder was not project-relative");
    }
    if (!ContainsFolder(models, "Content") ||
        !ContainsFolder(models, "Content/Models") ||
        !ContainsFolder(models, "Content/Models/Props") ||
        ContainsFolder(models, "Saved/Thumbnails"))
    {
        return Fail(root, "folder tree escaped Content or omitted a folder");
    }

    const auto* props = FindAsset(models, "Props");
    if (props == nullptr || !props->directory ||
        props->type != AssetType::Folder)
    {
        return Fail(root, "child folder was not represented as a card");
    }

    const auto crateFolder = browser.Scan(
        root.generic_u8string(),
        "Content/Models/Props");
    const auto* crate = FindAsset(crateFolder, "Crate.wiscene");
    if (crate == nullptr || crate->directory ||
        crate->type != AssetType::Model)
    {
        return Fail(root, "model WISCENE classification failed");
    }
    if (FindAsset(crateFolder, "Crate.thumbnail.png") != nullptr)
    {
        return Fail(root, "model thumbnail sidecar leaked into asset cards");
    }

    const auto unsafe = browser.Scan(
        root.generic_u8string(),
        "../Saved");
    if (unsafe.succeeded || unsafe.error.empty())
    {
        return Fail(root, "path traversal outside Content was accepted");
    }

    if (AssetBrowserService::Classify(
            "Content/Weapons/Sword/Sword.wiscene") != AssetType::Weapon ||
        AssetBrowserService::Classify(
            "Content/Projectiles/Arrow/Arrow.wiscene") !=
            AssetType::Projectile ||
        AssetBrowserService::Classify(
            "Content/Audio/Music/theme.ogg") != AssetType::Audio ||
        AssetBrowserService::Classify(
            "Content/Scripts/door.lua") != AssetType::Script)
    {
        return Fail(root, "typed project category classification failed");
    }

    std::error_code ignored;
    fs::remove_all(root, ignored);
    std::cout
        << "PASS: project-local Content indexing, safety, folders, and types\n";
    return 0;
}
