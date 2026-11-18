#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    enum class ResourceSourceFormat : std::uint8_t;
    enum class TextureSemantic { Color, Cutout, Normal, Linear };
    enum class TextureCookResult { Cooked, ResidentFallback, Failed };

    struct MaterialTextureRecipe
    {
        static constexpr unsigned Version = 1;
        TextureSemantic semantic = TextureSemantic::Color;
        float alphaReference = 0.5f;
    };

    inline std::string TextureRecipeName(const MaterialTextureRecipe& recipe)
    {
        switch (recipe.semantic)
        {
        case TextureSemantic::Color: return "color";
        case TextureSemantic::Normal: return "normal";
        case TextureSemantic::Linear: return "linear";
        case TextureSemantic::Cutout:
            char text[32];
            const auto end = std::to_chars(text, text + sizeof(text), recipe.alphaReference);
            if (end.ec != std::errc{}) return {};
            return "cutout-" + std::string(text, end.ptr);
        }
        return {};
    }

    inline bool ParseTextureRecipe(const std::string& value, MaterialTextureRecipe& recipe)
    {
        if (value == "color") recipe.semantic = TextureSemantic::Color;
        else if (value == "normal") recipe.semantic = TextureSemantic::Normal;
        else if (value == "linear") recipe.semantic = TextureSemantic::Linear;
        else if (value.compare(0, 7, "cutout-") == 0)
        {
            recipe.semantic = TextureSemantic::Cutout;
            const char* end = value.data() + value.size();
            const auto parsed = std::from_chars(value.data() + 7, end, recipe.alphaReference);
            return parsed.ec == std::errc{} && parsed.ptr == end && std::isfinite(recipe.alphaReference) &&
                recipe.alphaReference >= 0 && recipe.alphaReference <= 1 && TextureRecipeName(recipe) == value;
        }
        else return false;
        return true;
    }

    bool ValidateMaterialTextureProduct(const std::vector<std::uint8_t>& bytes,
        TextureSemantic semantic, std::string& error);

    TextureCookResult CookMaterialTexture(
        const std::vector<std::uint8_t>& source, ResourceSourceFormat format,
        const MaterialTextureRecipe& recipe, std::vector<std::uint8_t>& dds,
        std::string& diagnostic);
}
