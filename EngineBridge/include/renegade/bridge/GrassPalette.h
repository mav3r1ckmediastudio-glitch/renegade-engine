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

    // Retired stock category values remain for source compatibility; their queries return empty.
    enum class PlantGroup { All, Grass, Freshwater, Marine, Alien, Custom };
    inline constexpr char CustomPlantDirectory[] = "Content/Vegetation/Palette/Custom";

    std::vector<std::filesystem::path> GrassPaletteEntries(const std::filesystem::path& project,
        PlantGroup group = PlantGroup::All);
    bool ImportGrassSheet(const std::filesystem::path& project, const std::filesystem::path& source,
        unsigned columns, unsigned rows, std::string& error);
    bool BuildGrassAtlas(const std::filesystem::path& project,
        const std::vector<std::filesystem::path>& selected, GrassAtlas& atlas, std::string& error);
}
