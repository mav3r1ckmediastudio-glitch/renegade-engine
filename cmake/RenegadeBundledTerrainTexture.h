#pragma once
#include "wiHelper.h"
#include <algorithm>
#include <string>

namespace renegade::terrain
{
    // Old WISCENE archives store paths relative to their authoring Studio.
    // Resolve only the reserved stock paths, before the native material upload.
    inline std::string MissingStockGrassReplacement(const std::string& name)
    {
        if (name.find("default_grass_") == std::string::npos)
            return {};
        std::string normalized = name;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        const std::string filename = wi::helper::GetFileNameFromPath(normalized);
        if (filename != "default_grass_basecolor.tga" && filename != "default_grass_normal.tga" &&
            filename != "default_grass_surface.tga")
            return {};
        const std::string relative = "Content/terrain/default_grass/" + filename;
        if (normalized.size() < relative.size())
            return {};
        const auto offset = normalized.size() - relative.size();
        if ((offset && normalized[offset - 1] != '/') || normalized.compare(offset, relative.size(), relative) != 0 ||
            wi::helper::FileExists(name))
            return {};
        const auto local = wi::helper::GetDirectoryFromPath(wi::helper::GetExecutablePath()) + relative;
        return wi::helper::FileExists(local) ? local : std::string{};
    }
}
