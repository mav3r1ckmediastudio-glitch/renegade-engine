#pragma once

#include "renegade/bridge/AssetBrowserService.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace renegade::bridge
{
    enum class ScreenCreatorResourceKind
    {
        Image,
        Font,
    };

    struct ScreenCreatorResourceChoice
    {
        std::string projectRelativePath;
        AssetType assetType = AssetType::Unknown;
    };

    struct ScreenCreatorResourceCatalogue
    {
        bool succeeded = false;
        std::vector<ScreenCreatorResourceChoice> choices;
        std::string error;
    };

    // Deterministic creator-facing projection over the existing Renegade Asset
    // Browser seam. No arbitrary absolute-path picker is introduced: every
    // choice is a supported resource already inside the active project Content
    // tree. The current Screen renderer consumes project-relative texture/font
    // paths, so only renderer-compatible source resources are surfaced here.
    [[nodiscard]] inline ScreenCreatorResourceCatalogue
    EnumerateScreenCreatorResources(
        const std::string& projectRoot,
        const ScreenCreatorResourceKind kind)
    {
        ScreenCreatorResourceCatalogue result;
        AssetBrowserService browser;
        const AssetBrowserSnapshot root = browser.Scan(projectRoot, "Content");
        if (!root.succeeded)
        {
            result.error = root.error;
            return result;
        }

        const AssetType requestedType = kind == ScreenCreatorResourceKind::Image
            ? AssetType::Texture : AssetType::Font;
        std::set<std::string> paths;
        const auto collect = [&](const AssetBrowserSnapshot& snapshot)
        {
            if (!snapshot.succeeded) return;
            for (const auto& asset : snapshot.assets)
            {
                if (asset.directory || asset.type != requestedType) continue;
                const auto path = std::filesystem::u8path(
                    asset.projectRelativePath).lexically_normal();
                const std::string extension = path.extension().u8string();
                if (extension == ".rasset" || extension == ".RASSET")
                    continue;
                paths.insert(path.generic_u8string());
            }
        };

        collect(root);
        for (const auto& folder : root.folders)
        {
            if (folder.projectRelativePath == "Content") continue;
            collect(browser.Scan(projectRoot, folder.projectRelativePath));
        }

        result.choices.reserve(paths.size());
        for (const auto& path : paths)
        {
            result.choices.push_back({path, requestedType});
        }
        result.succeeded = true;
        return result;
    }
}
