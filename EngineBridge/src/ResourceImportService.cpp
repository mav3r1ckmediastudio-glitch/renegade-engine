#include "renegade/bridge/ResourceImportService.h"

#include <WickedEngine.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>

namespace
{
    namespace fs = std::filesystem;
    using renegade::bridge::ResourceClass;
    using renegade::bridge::ResourceSourceFormat;

    std::string Upper(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](const unsigned char character)
            {
                return static_cast<char>(std::toupper(character));
            });
        return value;
    }

    bool IsSafeProjectRelativePath(const fs::path& path)
    {
        if (path.empty() || path.is_absolute())
        {
            return false;
        }
        return std::none_of(
            path.begin(),
            path.end(),
            [](const fs::path& part)
            {
                return part == "..";
            });
    }

    bool IsWithin(const fs::path& child, const fs::path& parent)
    {
        auto childPart = child.begin();
        for (auto parentPart = parent.begin();
            parentPart != parent.end();
            ++parentPart, ++childPart)
        {
            if (childPart == child.end() || *childPart != *parentPart)
            {
                return false;
            }
        }
        return true;
    }

    bool StartsWith(
        const std::vector<std::uint8_t>& bytes,
        const std::initializer_list<std::uint8_t> signature)
    {
        if (bytes.size() < signature.size())
        {
            return false;
        }
        return std::equal(signature.begin(), signature.end(), bytes.begin());
    }

    bool HasAsciiAt(
        const std::vector<std::uint8_t>& bytes,
        const std::size_t offset,
        const char* signature,
        const std::size_t size)
    {
        return bytes.size() >= offset + size &&
            std::equal(
                signature,
                signature + size,
                bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                [](const char left, const std::uint8_t right)
                {
                    return static_cast<std::uint8_t>(left) == right;
                });
    }

    bool ValidateSignature(
        const ResourceSourceFormat format,
        const std::vector<std::uint8_t>& bytes,
        bool& signatureChecked)
    {
        signatureChecked = true;
        switch (format)
        {
        case ResourceSourceFormat::Jpg:
        case ResourceSourceFormat::Jpeg:
            return StartsWith(bytes, {0xFF, 0xD8, 0xFF});
        case ResourceSourceFormat::Png:
            return StartsWith(bytes, {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A});
        case ResourceSourceFormat::Bmp:
            return HasAsciiAt(bytes, 0, "BM", 2);
        case ResourceSourceFormat::Dds:
            return HasAsciiAt(bytes, 0, "DDS ", 4);
        case ResourceSourceFormat::Hdr:
            return HasAsciiAt(bytes, 0, "#?RADIANCE", 10) ||
                HasAsciiAt(bytes, 0, "#?RGBE", 6);
        case ResourceSourceFormat::Wav:
            return HasAsciiAt(bytes, 0, "RIFF", 4) &&
                HasAsciiAt(bytes, 8, "WAVE", 4);
        case ResourceSourceFormat::Ogg:
            return HasAsciiAt(bytes, 0, "OggS", 4);
        case ResourceSourceFormat::Mp4:
            return HasAsciiAt(bytes, 4, "ftyp", 4);
        case ResourceSourceFormat::H264:
            return StartsWith(bytes, {0x00, 0x00, 0x01}) ||
                StartsWith(bytes, {0x00, 0x00, 0x00, 0x01});
        case ResourceSourceFormat::Ttf:
            return StartsWith(bytes, {0x00, 0x01, 0x00, 0x00}) ||
                HasAsciiAt(bytes, 0, "true", 4) ||
                HasAsciiAt(bytes, 0, "typ1", 4) ||
                HasAsciiAt(bytes, 0, "OTTO", 4);
        case ResourceSourceFormat::Tga:
        case ResourceSourceFormat::Lua:
            // TGA has no dependable universal magic value, and Lua is source
            // text rather than a binary container. Extension/capability checks
            // remain authoritative for these Gate 1 formats.
            signatureChecked = false;
            return !bytes.empty();
        case ResourceSourceFormat::Unknown:
        default:
            signatureChecked = false;
            return false;
        }
    }

    std::set<std::string> NormalizedSet(const wi::vector<std::string>& values)
    {
        std::set<std::string> result;
        for (const auto& value : values)
        {
            result.insert(Upper(value));
        }
        return result;
    }

    std::set<std::string> ExpectedSet(const ResourceClass resourceClass)
    {
        std::set<std::string> result;
        for (const auto& capability :
            renegade::bridge::GetSupportedResourceFormats())
        {
            if (capability.resourceClass == resourceClass)
            {
                result.insert(Upper(capability.wickedExtension));
            }
        }
        return result;
    }

    std::string Join(const std::set<std::string>& values)
    {
        std::ostringstream stream;
        bool first = true;
        for (const auto& value : values)
        {
            if (!first)
            {
                stream << ',';
            }
            first = false;
            stream << value;
        }
        return stream.str();
    }

    bool CompareCapabilitySet(
        const char* label,
        const ResourceClass resourceClass,
        const wi::vector<std::string>& wickedValues,
        std::string& error)
    {
        const auto expected = ExpectedSet(resourceClass);
        const auto actual = NormalizedSet(wickedValues);
        if (expected == actual)
        {
            return true;
        }
        error = std::string("Pinned Wicked ") + label +
            " capability set differs from Renegade's accepted LP08 table. expected=" +
            Join(expected) + " actual=" + Join(actual);
        return false;
    }
}

namespace renegade::bridge
{
    const std::vector<ResourceFormatCapability>&
    GetSupportedResourceFormats() noexcept
    {
        static const std::vector<ResourceFormatCapability> capabilities = {
            {ResourceSourceFormat::Jpg, ResourceClass::Texture, ".jpg", "JPG"},
            {ResourceSourceFormat::Jpeg, ResourceClass::Texture, ".jpeg", "JPEG"},
            {ResourceSourceFormat::Png, ResourceClass::Texture, ".png", "PNG"},
            {ResourceSourceFormat::Bmp, ResourceClass::Texture, ".bmp", "BMP"},
            {ResourceSourceFormat::Dds, ResourceClass::Texture, ".dds", "DDS"},
            {ResourceSourceFormat::Tga, ResourceClass::Texture, ".tga", "TGA"},
            {ResourceSourceFormat::Hdr, ResourceClass::Texture, ".hdr", "HDR"},
            {ResourceSourceFormat::Wav, ResourceClass::Audio, ".wav", "WAV"},
            {ResourceSourceFormat::Ogg, ResourceClass::Audio, ".ogg", "OGG"},
            {ResourceSourceFormat::Lua, ResourceClass::Script, ".lua", "LUA"},
            {ResourceSourceFormat::Mp4, ResourceClass::Video, ".mp4", "MP4"},
            {ResourceSourceFormat::H264, ResourceClass::Video, ".h264", "H264"},
            {ResourceSourceFormat::Ttf, ResourceClass::Font, ".ttf", "TTF"},
        };
        return capabilities;
    }

    ResourceSourceFormat DetectResourceSourceFormat(
        const std::string& path) noexcept
    {
        try
        {
            const std::string extension = Upper(
                fs::u8path(path).extension().u8string());
            for (const auto& capability : GetSupportedResourceFormats())
            {
                if (extension == Upper(capability.extension))
                {
                    return capability.format;
                }
            }
        }
        catch (...)
        {
        }
        return ResourceSourceFormat::Unknown;
    }

    ResourceClass ClassifyResourceSourceFormat(
        const ResourceSourceFormat format) noexcept
    {
        for (const auto& capability : GetSupportedResourceFormats())
        {
            if (capability.format == format)
            {
                return capability.resourceClass;
            }
        }
        return ResourceClass::Unknown;
    }

    AssetType ResourceClassAssetType(const ResourceClass resourceClass) noexcept
    {
        switch (resourceClass)
        {
        case ResourceClass::Texture: return AssetType::Texture;
        case ResourceClass::Audio: return AssetType::Audio;
        case ResourceClass::Script: return AssetType::Script;
        case ResourceClass::Video: return AssetType::Video;
        case ResourceClass::Font: return AssetType::Font;
        case ResourceClass::Unknown:
        default:
            return AssetType::Unknown;
        }
    }

    const char* ResourceSourceFormatLabel(
        const ResourceSourceFormat format) noexcept
    {
        for (const auto& capability : GetSupportedResourceFormats())
        {
            if (capability.format == format)
            {
                return capability.wickedExtension;
            }
        }
        return "UNKNOWN";
    }

    const char* ResourceClassLabel(const ResourceClass resourceClass) noexcept
    {
        switch (resourceClass)
        {
        case ResourceClass::Texture: return "TEXTURE";
        case ResourceClass::Audio: return "AUDIO";
        case ResourceClass::Script: return "SCRIPT";
        case ResourceClass::Video: return "VIDEO";
        case ResourceClass::Font: return "FONT";
        case ResourceClass::Unknown:
        default:
            return "RESOURCE";
        }
    }

    bool ValidatePinnedWickedResourceCapabilities(std::string& error)
    {
        error.clear();
        if (!CompareCapabilitySet(
                "image",
                ResourceClass::Texture,
                wi::resourcemanager::GetSupportedImageExtensions(),
                error) ||
            !CompareCapabilitySet(
                "audio",
                ResourceClass::Audio,
                wi::resourcemanager::GetSupportedSoundExtensions(),
                error) ||
            !CompareCapabilitySet(
                "script",
                ResourceClass::Script,
                wi::resourcemanager::GetSupportedScriptExtensions(),
                error) ||
            !CompareCapabilitySet(
                "video",
                ResourceClass::Video,
                wi::resourcemanager::GetSupportedVideoExtensions(),
                error) ||
            !CompareCapabilitySet(
                "font",
                ResourceClass::Font,
                wi::resourcemanager::GetSupportedFontStyleExtensions(),
                error))
        {
            return false;
        }
        return true;
    }

    ResourceSourceInspectionResult InspectResourceSource(
        const ResourceSourceInspectionRequest& request)
    {
        ResourceSourceInspectionResult result;
        result.sourceProjectRelativePath = request.sourceProjectRelativePath;

        if (request.projectRoot.empty())
        {
            result.error = "Resource inspection requires an active project root.";
            return result;
        }

        try
        {
            const fs::path relative = fs::u8path(
                request.sourceProjectRelativePath).lexically_normal();
            if (!IsSafeProjectRelativePath(relative))
            {
                result.error =
                    "Resource source path must be project-relative and contained.";
                return result;
            }

            const fs::path root = fs::weakly_canonical(
                fs::absolute(fs::u8path(request.projectRoot)));
            const fs::path source = fs::weakly_canonical(root / relative);
            if (!IsWithin(source, root) || !fs::is_regular_file(source))
            {
                result.error =
                    "Resource source is missing or resolves outside the project.";
                return result;
            }

            result.format = DetectResourceSourceFormat(
                relative.generic_u8string());
            result.resourceClass = ClassifyResourceSourceFormat(result.format);
            result.assetType = ResourceClassAssetType(result.resourceClass);
            if (result.format == ResourceSourceFormat::Unknown ||
                result.resourceClass == ResourceClass::Unknown)
            {
                result.error = "Unsupported resource source format.";
                return result;
            }
            if (request.expectedFormat != ResourceSourceFormat::Unknown &&
                request.expectedFormat != result.format)
            {
                result.error =
                    "Resource source extension does not match the expected format.";
                return result;
            }

            std::ifstream stream(source, std::ios::binary);
            if (!stream)
            {
                result.error = "Could not open resource source for inspection.";
                return result;
            }
            const std::vector<std::uint8_t> bytes{
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>()};
            result.byteCount = bytes.size();
            if (bytes.empty())
            {
                result.error = "Resource source is empty.";
                return result;
            }

            if (!ValidateSignature(
                    result.format,
                    bytes,
                    result.signatureChecked))
            {
                result.error =
                    "Resource source signature does not match its declared format.";
                return result;
            }

            result.succeeded = true;
            return result;
        }
        catch (const std::exception& exception)
        {
            result.error = std::string("Resource inspection failed: ") +
                exception.what();
            return result;
        }
    }
}
