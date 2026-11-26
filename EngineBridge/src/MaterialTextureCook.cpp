#include "renegade/bridge/MaterialTextureCook.h"
#include "renegade/bridge/ResourceImportService.h"

#include <DirectXTex.h>
#include <algorithm>
#include <cmath>
#include <objbase.h>

namespace renegade::bridge
{
    using namespace DirectX;

    namespace
    {
        struct ComApartment
        {
            HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            ~ComApartment() { if (SUCCEEDED(result)) CoUninitialize(); }
        };

        DXGI_FORMAT ProductFormat(TextureSemantic semantic)
        {
            switch (semantic)
            {
            case TextureSemantic::Color:
            case TextureSemantic::Cutout: return DXGI_FORMAT_BC7_UNORM_SRGB;
            case TextureSemantic::Normal: return DXGI_FORMAT_BC5_UNORM;
            case TextureSemantic::Linear: return DXGI_FORMAT_BC7_UNORM;
            }
            return DXGI_FORMAT_UNKNOWN;
        }

        bool CanStream(const TexMetadata& m)
        {
            // Pinned Wicked restores sizes by doubling the resident dimensions.
            // Restrict to reversible reductions and products above its 64 KiB floor.
            if (m.dimension != TEX_DIMENSION_TEXTURE2D || m.arraySize != 1 ||
                m.depth != 1 || m.IsCubemap() || m.width > 16384 || m.height > 16384 ||
                m.width < 4 || m.height < 4 || (m.width & (m.width - 1)) ||
                (m.height & (m.height - 1)))
                return false;
            size_t w = m.width, h = m.height;
            if (w * h * 4 / 3 <= 65536)
                return false;
            while (w * h * 4 / 3 > 65536)
            {
                if (w <= 1 || h <= 1)
                    return false;
                w /= 2;
                h /= 2;
            }
            return true;
        }

        TextureCookResult Fail(const char* stage, HRESULT hr, std::string& diagnostic)
        {
            diagnostic = std::string(stage) + " failed (HRESULT " +
                std::to_string(static_cast<unsigned long>(hr)) + ").";
            return TextureCookResult::Failed;
        }
    }

    bool EncodeMaterialTgaAsPng(const uint8_t* data, size_t size, std::vector<uint8_t>& png)
    {
        ComApartment apartment;
        ScratchImage decoded;
        if (FAILED(LoadFromTGAMemory(data, size, TGA_FLAGS_NONE, nullptr, decoded))) return false;
        Blob encoded;
        if (FAILED(SaveToWICMemory(*decoded.GetImage(0,0,0), WIC_FLAGS_NONE,
                GetWICCodec(WIC_CODEC_PNG), encoded))) return false;
        const auto* first = static_cast<const uint8_t*>(encoded.GetBufferPointer());
        png.assign(first, first + encoded.GetBufferSize());
        return true;
    }

    bool ValidateMaterialTextureProduct(const std::vector<std::uint8_t>& bytes,
        TextureSemantic semantic, std::string& error)
    {
        TexMetadata info{};
        if (bytes.size() < 128 || FAILED(GetMetadataFromDDSMemory(bytes.data(), bytes.size(),
                DDS_FLAGS_NONE, info)) || !CanStream(info) || info.format != ProductFormat(semantic))
        {
            error = "DDS layout or format is incompatible with the native texture recipe.";
            return false;
        }
        const bool dx10 = bytes[84] == 'D' && bytes[85] == 'X' && bytes[86] == '1' && bytes[87] == '0';
        size_t expected = dx10 ? 148 : 128;
        size_t w = info.width, h = info.height, levels = 0;
        for (;;)
        {
            size_t pitch = 0, slice = 0;
            if (FAILED(ComputePitch(info.format, w, h, pitch, slice)) ||
                expected > bytes.size() || slice > bytes.size() - expected)
            {
                error = "DDS mip payload is truncated.";
                return false;
            }
            expected += slice;
            ++levels;
            if (w == 1 && h == 1) break;
            w = (std::max)(size_t(1), w / 2);
            h = (std::max)(size_t(1), h / 2);
        }
        if (levels != info.mipLevels || expected != bytes.size())
        {
            error = "DDS mip chain does not exactly cover the governed span.";
            return false;
        }
        return true;
    }

    TextureCookResult CookMaterialTexture(
        const std::vector<std::uint8_t>& source, ResourceSourceFormat format,
        const MaterialTextureRecipe& recipe, std::vector<std::uint8_t>& dds,
        std::string& diagnostic)
    {
        dds.clear();
        diagnostic.clear();
        if (source.empty() || ProductFormat(recipe.semantic) == DXGI_FORMAT_UNKNOWN ||
            !std::isfinite(recipe.alphaReference) || recipe.alphaReference < 0 ||
            recipe.alphaReference > 1)
            return Fail("Texture recipe validation", E_INVALIDARG, diagnostic);
        const bool isDds = format == ResourceSourceFormat::Dds;
        const bool isTga = format == ResourceSourceFormat::Tga;
        const bool isWic = format == ResourceSourceFormat::Png || format == ResourceSourceFormat::Jpg ||
            format == ResourceSourceFormat::Jpeg || format == ResourceSourceFormat::Bmp;
        if (!isDds && !isTga && !isWic)
        {
            diagnostic = "Source format uses resident decoding.";
            return TextureCookResult::ResidentFallback;
        }
        ComApartment com;
        if (FAILED(com.result) && com.result != RPC_E_CHANGED_MODE)
            return Fail("Image codec initialization", com.result, diagnostic);

        TexMetadata meta{};
        const auto* data = source.data();
        const size_t length = source.size();
        const auto wicFlags = WIC_FLAGS_FORCE_RGB | WIC_FLAGS_IGNORE_SRGB;
        HRESULT hr = isDds ? GetMetadataFromDDSMemory(data, length, DDS_FLAGS_NONE, meta) :
            isTga ? GetMetadataFromTGAMemory(data, length, TGA_FLAGS_IGNORE_SRGB, meta) :
            GetMetadataFromWICMemory(data, length, wicFlags, meta);
        if (FAILED(hr))
            return Fail("Texture header", hr, diagnostic);
        if (!CanStream(meta))
        {
            diagnostic = "Texture layout or size uses resident decoding.";
            return TextureCookResult::ResidentFallback;
        }
        const DXGI_FORMAT target = ProductFormat(recipe.semantic);
        if (isDds)
        {
            size_t levels = 1;
            for (size_t dim = (std::max)(meta.width, meta.height); dim > 1; dim /= 2)
                ++levels;
            if (meta.mipLevels != levels || meta.format != target)
            {
                diagnostic = "DDS mip chain or compression does not match the material recipe.";
                return TextureCookResult::ResidentFallback;
            }
            if (!ValidateMaterialTextureProduct(source, recipe.semantic, diagnostic))
                return TextureCookResult::Failed;
            dds = source;
            return TextureCookResult::Cooked;
        }

        ScratchImage decoded;
        hr = isTga ? LoadFromTGAMemory(data, length, TGA_FLAGS_IGNORE_SRGB, nullptr, decoded) :
            LoadFromWICMemory(data, length, wicFlags, nullptr, decoded);
        if (FAILED(hr))
            return Fail("Texture decode", hr, diagnostic);
        Image base = *decoded.GetImage(0, 0, 0);
        const bool color = recipe.semantic == TextureSemantic::Color ||
            recipe.semantic == TextureSemantic::Cutout;
        if (color)
            base.format = MakeSRGB(base.format);
        const auto filter = TEX_FILTER_BOX | TEX_FILTER_FORCE_NON_WIC |
            (color ? TEX_FILTER_SRGB : TEX_FILTER_DEFAULT);
        ScratchImage mips;
        hr = GenerateMipMaps(base, filter, 0, mips);
        if (FAILED(hr))
            return Fail("Texture mip generation", hr, diagnostic);
        ScratchImage coverage;
        const ScratchImage* chain = &mips;
        if (recipe.semantic == TextureSemantic::Cutout && !decoded.IsAlphaAllOpaque())
        {
            hr = coverage.Initialize(mips.GetMetadata());
            if (SUCCEEDED(hr))
                hr = ScaleMipMapsAlphaForCoverage(mips.GetImages(), mips.GetImageCount(),
                    mips.GetMetadata(), 0, recipe.alphaReference, coverage);
            if (FAILED(hr))
                return Fail("Foliage alpha coverage", hr, diagnostic);
            chain = &coverage;
        }
        ScratchImage blocks;
        const auto flags = TEX_COMPRESS_BC7_QUICK | (color ? TEX_COMPRESS_SRGB : TEX_COMPRESS_DEFAULT);
        hr = Compress(chain->GetImages(), chain->GetImageCount(), chain->GetMetadata(),
            target, flags, TEX_THRESHOLD_DEFAULT, blocks);
        if (FAILED(hr))
            return Fail("Texture block compression", hr, diagnostic);
        Blob blob;
        hr = SaveToDDSMemory(blocks.GetImages(), blocks.GetImageCount(), blocks.GetMetadata(),
            DDS_FLAGS_FORCE_DX10_EXT, blob);
        if (FAILED(hr))
            return Fail("DDS serialization", hr, diagnostic);
        const auto* begin = static_cast<const std::uint8_t*>(blob.GetBufferPointer());
        dds.assign(begin, begin + blob.GetBufferSize());
        return TextureCookResult::Cooked;
    }
}
