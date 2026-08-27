#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    struct TgaImage
    {
        std::uint16_t width = 0;
        std::uint16_t height = 0;
        std::uint8_t descriptor = 0;
        std::vector<std::array<std::uint8_t, 4>> pixels;
    };

    std::uint16_t ReadU16(const std::array<std::uint8_t, 18>& header,
        const std::size_t offset)
    {
        return static_cast<std::uint16_t>(
            header[offset] |
            (static_cast<std::uint16_t>(header[offset + 1]) << 8u));
    }

    std::array<std::uint8_t, 4> ReadPixel(
        std::istream& stream,
        const std::uint8_t bitsPerPixel)
    {
        std::array<std::uint8_t, 4> pixel = {0, 0, 0, 255};
        if (bitsPerPixel == 8)
        {
            stream.read(reinterpret_cast<char*>(&pixel[0]), 1);
            pixel[1] = pixel[0];
            pixel[2] = pixel[0];
        }
        else if (bitsPerPixel == 24 || bitsPerPixel == 32)
        {
            stream.read(
                reinterpret_cast<char*>(pixel.data()),
                bitsPerPixel / 8);
        }
        else
        {
            throw std::runtime_error("unsupported TGA pixel depth");
        }
        if (!stream)
        {
            throw std::runtime_error("truncated TGA pixel data");
        }
        return pixel;
    }

    TgaImage LoadTga(const std::string& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("cannot open " + path);
        }

        std::array<std::uint8_t, 18> header = {};
        stream.read(reinterpret_cast<char*>(header.data()), header.size());
        if (!stream)
        {
            throw std::runtime_error("invalid TGA header in " + path);
        }
        if (header[1] != 0)
        {
            throw std::runtime_error("colour-mapped TGA is unsupported: " + path);
        }

        const std::uint8_t imageType = header[2];
        const bool runLengthEncoded = imageType == 10 || imageType == 11;
        if (imageType != 2 && imageType != 3 && !runLengthEncoded)
        {
            throw std::runtime_error("unsupported TGA image type in " + path);
        }

        TgaImage image;
        image.width = ReadU16(header, 12);
        image.height = ReadU16(header, 14);
        image.descriptor = header[17];
        if (image.width == 0 || image.height == 0)
        {
            throw std::runtime_error("empty TGA image: " + path);
        }

        stream.seekg(header[0], std::ios::cur);
        const std::size_t pixelCount =
            static_cast<std::size_t>(image.width) * image.height;
        image.pixels.reserve(pixelCount);
        if (!runLengthEncoded)
        {
            while (image.pixels.size() < pixelCount)
            {
                image.pixels.push_back(ReadPixel(stream, header[16]));
            }
        }
        else
        {
            while (image.pixels.size() < pixelCount)
            {
                std::uint8_t packet = 0;
                stream.read(reinterpret_cast<char*>(&packet), 1);
                if (!stream)
                {
                    throw std::runtime_error("truncated TGA RLE packet: " + path);
                }
                const std::size_t count = (packet & 0x7Fu) + 1u;
                if (image.pixels.size() + count > pixelCount)
                {
                    throw std::runtime_error("invalid TGA RLE packet: " + path);
                }
                if ((packet & 0x80u) != 0)
                {
                    const auto pixel = ReadPixel(stream, header[16]);
                    image.pixels.insert(image.pixels.end(), count, pixel);
                }
                else
                {
                    for (std::size_t i = 0; i < count; ++i)
                    {
                        image.pixels.push_back(ReadPixel(stream, header[16]));
                    }
                }
            }
        }
        return image;
    }

    void WriteHeader(std::ostream& stream, const TgaImage& image)
    {
        std::array<std::uint8_t, 18> header = {};
        header[2] = 2;
        header[12] = static_cast<std::uint8_t>(image.width & 0xFFu);
        header[13] = static_cast<std::uint8_t>(image.width >> 8u);
        header[14] = static_cast<std::uint8_t>(image.height & 0xFFu);
        header[15] = static_cast<std::uint8_t>(image.height >> 8u);
        header[16] = 32;
        header[17] = static_cast<std::uint8_t>(
            (image.descriptor & 0x30u) | 8u);
        stream.write(reinterpret_cast<const char*>(header.data()), header.size());
    }

    std::size_t TiledSourceIndex(
        const TgaImage& image,
        const std::size_t outputIndex,
        const std::size_t tileCount)
    {
        const std::size_t x = outputIndex % image.width;
        const std::size_t y = outputIndex / image.width;
        return ((y * tileCount) % image.height) * image.width +
            ((x * tileCount) % image.width);
    }

    void WriteTiledTga(
        const std::string& path,
        const TgaImage& image,
        const std::size_t tileCount)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("cannot write " + path);
        }
        WriteHeader(stream, image);
        for (std::size_t index = 0; index < image.pixels.size(); ++index)
        {
            const auto& pixel = image.pixels[
                TiledSourceIndex(image, index, tileCount)];
            stream.write(
                reinterpret_cast<const char*>(pixel.data()),
                pixel.size());
        }
        if (!stream)
        {
            throw std::runtime_error("failed while writing " + path);
        }
    }

    void WriteSurfaceTga(
        const std::string& path,
        const TgaImage& ao,
        const TgaImage& roughness,
        const std::size_t tileCount)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("cannot write " + path);
        }
        WriteHeader(stream, ao);

        for (std::size_t index = 0; index < ao.pixels.size(); ++index)
        {
            const auto aoIndex = TiledSourceIndex(ao, index, tileCount);
            const auto roughnessIndex = TiledSourceIndex(
                roughness,
                index,
                tileCount);
            // TGA stores BGRA. Wicked's metallic/roughness surface layout is
            // R=AO, G=roughness, B=metalness, A=reflectance.
            const std::array<std::uint8_t, 4> pixel = {
                0,
                roughness.pixels[roughnessIndex][2],
                ao.pixels[aoIndex][2],
                255,
            };
            stream.write(reinterpret_cast<const char*>(pixel.data()), pixel.size());
        }
        if (!stream)
        {
            throw std::runtime_error("failed while writing " + path);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc != 9)
    {
        std::cerr << "Usage: RenegadeTerrainSurfacePacker <ao.tga> "
                     "<roughness.tga> <basecolor.tga> <normal.tga> "
                     "<output-basecolor.tga> <output-normal.tga> "
                     "<output-surface.tga> <tile-count>\n";
        return 2;
    }
    try
    {
        const TgaImage ao = LoadTga(argv[1]);
        const TgaImage roughness = LoadTga(argv[2]);
        const TgaImage baseColor = LoadTga(argv[3]);
        const TgaImage normal = LoadTga(argv[4]);
        const std::size_t tileCount = std::stoul(argv[8]);
        if (tileCount == 0)
        {
            throw std::runtime_error("tile count must be greater than zero");
        }
        if (ao.width != roughness.width || ao.height != roughness.height ||
            (ao.descriptor & 0x30u) != (roughness.descriptor & 0x30u))
        {
            throw std::runtime_error(
                "AO and roughness TGA dimensions/orientation do not match");
        }
        if (baseColor.width != normal.width ||
            baseColor.height != normal.height ||
            (baseColor.descriptor & 0x30u) != (normal.descriptor & 0x30u))
        {
            throw std::runtime_error(
                "base-colour and normal TGA dimensions/orientation do not match");
        }
        WriteTiledTga(argv[5], baseColor, tileCount);
        WriteTiledTga(argv[6], normal, tileCount);
        WriteSurfaceTga(argv[7], ao, roughness, tileCount);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Terrain surface packing failed: " << error.what() << '\n';
        return 1;
    }
}
