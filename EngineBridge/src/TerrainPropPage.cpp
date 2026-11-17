#include "renegade/bridge/TerrainPropPage.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <unordered_set>
#include <utility>

namespace renegade::bridge
{
    namespace
    {
        constexpr std::size_t HeaderBytes = 24;
        constexpr std::size_t RecordBytes = 32;
        constexpr std::size_t MaxProfiles = 65536;
        constexpr std::size_t MaxPlacements = 1000000;

        template<typename T>
        void Append(std::vector<std::uint8_t>& bytes, T value)
        {
            // The supported native target is little-endian Windows x64.
            auto first = reinterpret_cast<const std::uint8_t*>(&value);
            bytes.insert(bytes.end(), first, first + sizeof(T));
        }

        template<typename T>
        T Read(const std::uint8_t*& cursor)
        {
            T value;
            std::memcpy(&value, cursor, sizeof(value));
            cursor += sizeof(value);
            return value;
        }

    }

    bool ValidateTerrainPropPage(const TerrainPropPage& page, std::string& error)
    {
        error.clear();
        if (page.profiles.size() > MaxProfiles || page.placements.size() > MaxPlacements)
        {
            error = "Terrain prop page exceeds its format limits.";
            return false;
        }
        std::set<std::array<std::uint8_t, 16>> profiles;
        for (const auto& profile : page.profiles)
        {
            if (profile == std::array<std::uint8_t, 16>{} || !profiles.insert(profile).second)
            {
                error = "Missing or duplicate profile identity.";
                return false;
            }
        }
        std::unordered_set<std::uint64_t> ids;
        for (const auto& placement : page.placements)
        {
            if (!placement.id || !ids.insert(placement.id).second ||
                placement.profileIndex >= profiles.size() ||
                !std::all_of(placement.position.begin(), placement.position.end(),
                    [](float coordinate) { return std::isfinite(coordinate); }) ||
                !std::isfinite(placement.yawRadians) || !std::isfinite(placement.scale) ||
                placement.scale <= 0)
            {
                error = "Invalid terrain prop identity, profile or transform.";
                return false;
            }
        }
        return true;
    }

    bool EncodeTerrainPropPage(const TerrainPropPage& page, std::vector<std::uint8_t>& out, std::string& error)
    {
        error.clear();
        if (!ValidateTerrainPropPage(page, error)) return false;
        std::vector<std::uint8_t> bytes{'R','P','P','G'};
        bytes.reserve(HeaderBytes + page.profiles.size() * 16 + page.placements.size() * RecordBytes);
        Append(bytes, std::uint32_t{1});
        Append(bytes, page.cellX);
        Append(bytes, page.cellZ);
        Append(bytes, static_cast<std::uint32_t>(page.profiles.size()));
        Append(bytes, static_cast<std::uint32_t>(page.placements.size()));
        for (const auto& profile : page.profiles)
            bytes.insert(bytes.end(), profile.begin(), profile.end());
        for (const auto& p : page.placements)
        {
            Append(bytes, p.id);
            Append(bytes, p.profileIndex);
            for (float coordinate : p.position) Append(bytes, coordinate);
            Append(bytes, p.yawRadians);
            Append(bytes, p.scale);
        }
        out.swap(bytes);
        return true;
    }

    bool DecodeTerrainPropPage(const std::vector<std::uint8_t>& bytes, TerrainPropPage& out, std::string& error)
    {
        error.clear();
        if (bytes.size() < HeaderBytes || std::memcmp(bytes.data(), "RPPG", 4))
        {
            error = "Invalid terrain prop page header.";
            return false;
        }
        const auto* cursor = bytes.data() + 4;
        if (Read<std::uint32_t>(cursor) != 1)
        {
            error = "Unsupported terrain prop page version.";
            return false;
        }
        TerrainPropPage page;
        page.cellX = Read<std::int32_t>(cursor);
        page.cellZ = Read<std::int32_t>(cursor);
        const auto profiles = Read<std::uint32_t>(cursor);
        const auto placements = Read<std::uint32_t>(cursor);
        if (profiles > MaxProfiles || placements > MaxPlacements ||
            bytes.size() != HeaderBytes + profiles * 16ull + placements * RecordBytes)
        {
            error = "Invalid terrain prop page length.";
            return false;
        }
        page.profiles.resize(profiles);
        for (auto& profile : page.profiles)
        {
            std::copy_n(cursor, profile.size(), profile.begin());
            cursor += profile.size();
        }
        page.placements.resize(placements);
        for (auto& p : page.placements)
        {
            p.id = Read<std::uint64_t>(cursor);
            p.profileIndex = Read<std::uint32_t>(cursor);
            for (auto& coordinate : p.position) coordinate = Read<float>(cursor);
            p.yawRadians = Read<float>(cursor);
            p.scale = Read<float>(cursor);
        }
        if (!ValidateTerrainPropPage(page, error)) return false;
        out = std::move(page);
        return true;
    }
}
