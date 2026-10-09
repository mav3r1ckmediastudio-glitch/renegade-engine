#pragma once

#include <cstdint>
#include <string_view>

namespace renegade::bridge
{
    enum class ImpactSurfaceType : std::uint8_t
    {
        Default = 0,
        Metal,
        Wood,
        Concrete,
        Stone,
        Dirt,
        Glass,
        Water,
        Character,
    };

    inline constexpr const char* ImpactSurfaceMetadataKey =
        "renegade.impact_surface";

    inline constexpr const char* ImpactSurfaceTypeToken(
        const ImpactSurfaceType type) noexcept
    {
        switch (type)
        {
        case ImpactSurfaceType::Metal: return "metal";
        case ImpactSurfaceType::Wood: return "wood";
        case ImpactSurfaceType::Concrete: return "concrete";
        case ImpactSurfaceType::Stone: return "stone";
        case ImpactSurfaceType::Dirt: return "dirt";
        case ImpactSurfaceType::Glass: return "glass";
        case ImpactSurfaceType::Water: return "water";
        case ImpactSurfaceType::Character: return "character";
        default: return "default";
        }
    }

    inline constexpr const char* ImpactSurfaceTypeName(
        const ImpactSurfaceType type) noexcept
    {
        switch (type)
        {
        case ImpactSurfaceType::Metal: return "Metal";
        case ImpactSurfaceType::Wood: return "Wood";
        case ImpactSurfaceType::Concrete: return "Concrete";
        case ImpactSurfaceType::Stone: return "Stone";
        case ImpactSurfaceType::Dirt: return "Dirt / Ground";
        case ImpactSurfaceType::Glass: return "Glass";
        case ImpactSurfaceType::Water: return "Water";
        case ImpactSurfaceType::Character: return "Character";
        default: return "Default";
        }
    }

    inline constexpr bool ParseImpactSurfaceType(
        const std::string_view token,
        ImpactSurfaceType& type) noexcept
    {
        if (token == "metal") type = ImpactSurfaceType::Metal;
        else if (token == "wood") type = ImpactSurfaceType::Wood;
        else if (token == "concrete") type = ImpactSurfaceType::Concrete;
        else if (token == "stone") type = ImpactSurfaceType::Stone;
        else if (token == "dirt") type = ImpactSurfaceType::Dirt;
        else if (token == "glass") type = ImpactSurfaceType::Glass;
        else if (token == "water") type = ImpactSurfaceType::Water;
        else if (token == "character") type = ImpactSurfaceType::Character;
        else if (token == "default" || token.empty()) type = ImpactSurfaceType::Default;
        else return false;
        return true;
    }
}
