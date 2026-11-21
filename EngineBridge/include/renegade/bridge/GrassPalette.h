#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct GrassAtlas
    {
        std::string path;
        unsigned frames = 0;
    };

    std::vector<std::filesystem::path> GrassPaletteEntries(const std::filesystem::path& project);
    bool ImportGrassSheet(const std::filesystem::path& project, const std::filesystem::path& source,
        unsigned columns, unsigned rows, std::string& error);
    bool BuildGrassAtlas(const std::filesystem::path& project,
        const std::vector<std::filesystem::path>& selected, GrassAtlas& atlas, std::string& error);
}
