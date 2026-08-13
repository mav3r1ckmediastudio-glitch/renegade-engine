#include "renegade/bridge/CreatorSurfaceBuilderService.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <memory>
#include <vector>

#include "Utility/stb_image.h"
#include "Utility/stb_image_write.h"

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        struct Image
        {
            int width = 0;
            int height = 0;
            std::unique_ptr<unsigned char, void(*)(void*)> pixels{nullptr, stbi_image_free};
        };

        bool LoadGray(const std::string& path, Image& image, std::string& error)
        {
            image = {};
            if (path.empty())
                return true;
            int channels = 0;
            unsigned char* pixels = stbi_load(path.c_str(), &image.width, &image.height, &channels, 1);
            if (pixels == nullptr || image.width <= 0 || image.height <= 0)
            {
                error = "Could not decode PBR source image: " + path;
                if (const char* reason = stbi_failure_reason(); reason != nullptr)
                    error += " (" + std::string(reason) + ")";
                return false;
            }
            image.pixels.reset(pixels);
            return true;
        }

        bool ResolveDimensions(
            const Image& roughness,
            const Image& metalness,
            const Image& occlusion,
            int& width,
            int& height,
            std::string& error)
        {
            width = 0;
            height = 0;
            for (const Image* image : {&roughness, &metalness, &occlusion})
            {
                if (!image->pixels)
                    continue;
                if (width == 0)
                {
                    width = image->width;
                    height = image->height;
                    continue;
                }
                if (image->width != width || image->height != height)
                {
                    error = "Roughness, metalness and AO source maps must have matching dimensions.";
                    return false;
                }
            }
            if (width <= 0 || height <= 0)
            {
                error = "Surface Builder requires at least one roughness, metalness or AO source map.";
                return false;
            }
            return true;
        }

        unsigned char SampleOrDefault(
            const Image& image,
            const std::size_t index,
            const std::uint8_t fallback)
        {
            return image.pixels ? image.pixels.get()[index] : fallback;
        }
    }

    CreatorSurfaceBuildResult BuildCreatorSurfaceMap(
        const CreatorSurfaceBuildRequest& request)
    {
        CreatorSurfaceBuildResult result;
        if (request.outputPath.empty())
        {
            result.error = "Surface Builder requires an output path.";
            return result;
        }

        Image roughness;
        Image metalness;
        Image occlusion;
        if (!LoadGray(request.roughnessPath, roughness, result.error) ||
            !LoadGray(request.metalnessPath, metalness, result.error) ||
            !LoadGray(request.occlusionPath, occlusion, result.error))
            return result;

        int width = 0;
        int height = 0;
        if (!ResolveDimensions(roughness, metalness, occlusion, width, height, result.error))
            return result;

        const std::uint64_t pixelCount64 =
            static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
        if (pixelCount64 > (std::numeric_limits<std::size_t>::max)() / 4u)
        {
            result.error = "Surface Builder image dimensions are too large.";
            return result;
        }
        const std::size_t pixelCount = static_cast<std::size_t>(pixelCount64);
        std::vector<unsigned char> rgba(pixelCount * 4u);
        for (std::size_t index = 0; index < pixelCount; ++index)
        {
            rgba[index * 4u + 0u] = SampleOrDefault(
                occlusion, index, request.defaultOcclusion);
            rgba[index * 4u + 1u] = SampleOrDefault(
                roughness, index, request.defaultRoughness);
            rgba[index * 4u + 2u] = SampleOrDefault(
                metalness, index, request.defaultMetalness);
            rgba[index * 4u + 3u] = request.reflectance;
        }

        std::error_code ec;
        const fs::path output = fs::u8path(request.outputPath);
        if (!output.parent_path().empty())
        {
            fs::create_directories(output.parent_path(), ec);
            if (ec)
            {
                result.error = "Could not create Surface Builder output directory: " + ec.message();
                return result;
            }
        }
        if (stbi_write_png(
                request.outputPath.c_str(), width, height, 4,
                rgba.data(), width * 4) == 0)
        {
            result.error = "Could not write generated Wicked surface map: " + request.outputPath;
            return result;
        }

        result.succeeded = true;
        result.width = static_cast<std::uint32_t>(width);
        result.height = static_cast<std::uint32_t>(height);
        result.outputPath = request.outputPath;
        result.error.clear();
        return result;
    }
}
