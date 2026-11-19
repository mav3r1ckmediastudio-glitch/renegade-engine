#include "renegade/bridge/MaterialTextureCook.h"
#include "renegade/bridge/ResourceImportService.h"

#include <DirectXTex.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace renegade::bridge;
using namespace DirectX;

namespace
{
    void Require(bool condition, const std::string& reason)
    {
        if (!condition)
            throw std::runtime_error(reason);
    }

    std::vector<uint8_t> TestImage(unsigned size)
    {
        std::vector<uint8_t> tga(18 + size * size * 4);
        tga[2] = 2;
        tga[12] = tga[14] = static_cast<uint8_t>(size);
        tga[13] = tga[15] = static_cast<uint8_t>(size >> 8);
        tga[16] = 32;
        tga[17] = 0x28;
        for (unsigned y = 0; y < size; ++y)
        {
            for (unsigned x = 0; x < size; ++x)
            {
                auto* bgra = &tga[18 + (y * size + x) * 4];
                bgra[0] = 17;
                bgra[1] = 64;
                bgra[2] = ((x + y) % 2) ? 255 : 0;
                bgra[3] = (x % 16) < 8 ? 255 : 0;
            }
        }
        return tga;
    }

    void CheckResidentDds(const TexMetadata& metadata)
    {
        ScratchImage image;
        Require(SUCCEEDED(image.Initialize(metadata)), "resident DDS fixture");
        Blob blob;
        Require(SUCCEEDED(SaveToDDSMemory(image.GetImages(), image.GetImageCount(), metadata,
            DDS_FLAGS_FORCE_DX10_EXT, blob)), "resident DDS serialization");
        const auto* begin = static_cast<const uint8_t*>(blob.GetBufferPointer());
        std::vector<uint8_t> product;
        std::string error;
        const auto result = CookMaterialTexture({begin, begin + blob.GetBufferSize()},
            ResourceSourceFormat::Dds, {}, product, error);
        Require(result == TextureCookResult::ResidentFallback && product.empty(), "resident DDS eligibility: " + error);
    }

    void CheckRecipe(const std::vector<uint8_t>& input, TextureSemantic semantic,
        DXGI_FORMAT expected, int expectedRed, float cutoff = 0.5f)
    {
        std::string error;
        std::vector<uint8_t> product, reused;
        const MaterialTextureRecipe recipe{semantic, cutoff};
        MaterialTextureRecipe parsed;
        Require(ParseTextureRecipe(TextureRecipeName(recipe), parsed) && parsed.semantic == semantic &&
            (semantic != TextureSemantic::Cutout || parsed.alphaReference == cutoff), "recipe round trip");
        Require(CookMaterialTexture(input, ResourceSourceFormat::Tga, recipe, product, error)
            == TextureCookResult::Cooked, "TGA cook: " + error);
        Require(CookMaterialTexture(product, ResourceSourceFormat::Dds, recipe, reused, error)
            == TextureCookResult::Cooked && product == reused, "DDS reuse: " + error);
        ScratchImage blocks, pixels;
        TexMetadata meta{};
        Require(SUCCEEDED(LoadFromDDSMemory(product.data(), product.size(), DDS_FLAGS_NONE,
            &meta, blocks)), "DDS decode");
        Require(meta.width == 256 && meta.height == 256 && meta.mipLevels == 9 &&
            meta.format == expected, "DDS metadata mismatch");
        Image mip = *blocks.GetImage(1, 0, 0);
        // Read the encoded samples directly to check the difference between
        // gamma-correct colour filtering and raw linear material filtering.
        mip.format = (mip.format == DXGI_FORMAT_BC7_UNORM_SRGB ? DXGI_FORMAT_BC7_UNORM : mip.format);
        Require(SUCCEEDED(Decompress(mip, DXGI_FORMAT_R8G8B8A8_UNORM, pixels)), "mip decode");
        const auto* first = pixels.GetPixels();
        Require(std::abs(int(first[0]) - expectedRed) <= 8 &&
            std::abs(int(first[1]) - 64) <= 8, "mip colour/normal orientation mismatch");
        if (semantic == TextureSemantic::Cutout)
        {
            const auto& image = *pixels.GetImage(0, 0, 0);
            size_t covered = 0;
            for (size_t y = 0; y < image.height; ++y)
                for (size_t x = 0; x < image.width; ++x)
                    covered += image.pixels[y * image.rowPitch + 4 * x + 3] >= std::ceil(cutoff * 255.f);
            Require(std::abs(double(covered) / (image.width * image.height) - 0.5) < 0.03,
                "cutout coverage mismatch");
        }
        product.pop_back();
        Require(CookMaterialTexture(product, ResourceSourceFormat::Dds, recipe, reused, error)
            == TextureCookResult::Failed && reused.empty(), "short DDS accepted");
    }
}

int main()
{
    try
    {
        const auto input = TestImage(256);
        CheckRecipe(input, TextureSemantic::Color, DXGI_FORMAT_BC7_UNORM_SRGB, 188);
        CheckRecipe(input, TextureSemantic::Cutout, DXGI_FORMAT_BC7_UNORM_SRGB, 188);
        CheckRecipe(input, TextureSemantic::Normal, DXGI_FORMAT_BC5_UNORM, 128);
        CheckRecipe(input, TextureSemantic::Linear, DXGI_FORMAT_BC7_UNORM, 128);
        CheckRecipe(input, TextureSemantic::Cutout, DXGI_FORMAT_BC7_UNORM_SRGB, 188, 0.73f);
        TexMetadata metadata{};
        metadata.width = metadata.height = 256;
        metadata.depth = metadata.arraySize = metadata.mipLevels = 1;
        metadata.dimension = TEX_DIMENSION_TEXTURE2D;
        metadata.format = DXGI_FORMAT_BC7_UNORM_SRGB;
        CheckResidentDds(metadata); // A valid single-mip DDS cannot stream.
        metadata.mipLevels = 9;
        metadata.arraySize = 2;
        CheckResidentDds(metadata);
        metadata.arraySize = 1;
        metadata.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        CheckResidentDds(metadata);
        metadata.dimension = TEX_DIMENSION_TEXTURE3D;
        metadata.depth = 4;
        CheckResidentDds(metadata);
        std::vector<uint8_t> output;
        std::string reason;
        Require(CookMaterialTexture(TestImage(16), ResourceSourceFormat::Tga, {}, output, reason)
            == TextureCookResult::ResidentFallback && output.empty(), "small image should stay resident");
        std::cout << "Colour, cutout, normal, linear, DDS reuse and fallback passed.\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
