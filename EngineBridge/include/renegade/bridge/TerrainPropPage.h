#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct TerrainPropPlacement
    {
        std::uint64_t id = 0; // Unique within the owning terrain layer.
        std::uint32_t profileIndex = 0;
        std::array<float, 3> position{};
        float yawRadians = 0;
        float scale = 1;
        std::uint64_t candidateKey = 0; // Zero identifies migrated v1/manual placements.
    };

    struct TerrainPropPage
    {
        std::int32_t cellX = 0, cellZ = 0;
        std::vector<std::array<std::uint8_t, 16>> profiles;
        std::vector<TerrainPropPlacement> placements;
    };

    // Failure leaves the caller's output intact.
    bool ValidateTerrainPropPage(const TerrainPropPage&, std::string& error);
    bool EncodeTerrainPropPage(const TerrainPropPage&, std::vector<std::uint8_t>&, std::string& error);
    bool DecodeTerrainPropPage(const std::vector<std::uint8_t>&, TerrainPropPage&, std::string& error);
}
