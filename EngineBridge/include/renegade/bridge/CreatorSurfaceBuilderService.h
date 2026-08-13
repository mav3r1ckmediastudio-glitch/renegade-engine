#pragma once

#include <cstdint>
#include <string>

namespace renegade::bridge
{
    struct CreatorSurfaceBuildRequest
    {
        // Individual creator-facing PBR sources. Empty paths use the supplied
        // defaults for that channel. At least one source image is required.
        std::string roughnessPath;
        std::string metalnessPath;
        std::string occlusionPath;
        std::string outputPath;

        std::uint8_t defaultRoughness = 128;
        std::uint8_t defaultMetalness = 0;
        std::uint8_t defaultOcclusion = 255;
        std::uint8_t reflectance = 255;
    };

    struct CreatorSurfaceBuildResult
    {
        bool succeeded = false;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::string outputPath;
        std::string error;
    };

    // Packs creator-friendly separate PBR maps into Wicked's native surface
    // layout: R=AO, G=roughness, B=metalness, A=reflectance. Input images are
    // decoded through the stb_image implementation already pinned by Wicked.
    // Sources must have matching dimensions; the importer can report a clear
    // error rather than silently resampling authored data.
    [[nodiscard]] CreatorSurfaceBuildResult BuildCreatorSurfaceMap(
        const CreatorSurfaceBuildRequest& request);
}
