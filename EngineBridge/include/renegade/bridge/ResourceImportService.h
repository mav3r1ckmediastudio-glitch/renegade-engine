#pragma once

#include "renegade/bridge/AssetBrowserService.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    enum class ResourceClass : std::uint8_t
    {
        Unknown,
        Texture,
        Audio,
        Script,
        Video,
        Font,
    };

    enum class ResourceSourceFormat : std::uint8_t
    {
        Unknown,
        Jpg,
        Jpeg,
        Png,
        Bmp,
        Dds,
        Tga,
        Hdr,
        Wav,
        Ogg,
        Lua,
        Mp4,
        H264,
        Ttf,
    };

    struct ResourceFormatCapability
    {
        ResourceSourceFormat format = ResourceSourceFormat::Unknown;
        ResourceClass resourceClass = ResourceClass::Unknown;
        const char* extension = "";
        const char* wickedExtension = "";
    };

    struct ResourceSourceInspectionRequest
    {
        std::string projectRoot;
        std::string sourceProjectRelativePath;
        ResourceSourceFormat expectedFormat = ResourceSourceFormat::Unknown;
    };

    struct ResourceSourceInspectionResult
    {
        bool succeeded = false;
        ResourceSourceFormat format = ResourceSourceFormat::Unknown;
        ResourceClass resourceClass = ResourceClass::Unknown;
        AssetType assetType = AssetType::Unknown;
        std::string sourceProjectRelativePath;
        std::size_t byteCount = 0;
        bool signatureChecked = false;
        std::string error;
    };

    // Fixed Renegade capability vocabulary for the exact pinned Wicked resource
    // manager. Ordering is deterministic and independent of Wicked's internal
    // unordered map iteration order.
    [[nodiscard]] const std::vector<ResourceFormatCapability>&
    GetSupportedResourceFormats() noexcept;

    [[nodiscard]] ResourceSourceFormat DetectResourceSourceFormat(
        const std::string& path) noexcept;
    [[nodiscard]] ResourceClass ClassifyResourceSourceFormat(
        ResourceSourceFormat format) noexcept;
    [[nodiscard]] AssetType ResourceClassAssetType(
        ResourceClass resourceClass) noexcept;
    [[nodiscard]] const char* ResourceSourceFormatLabel(
        ResourceSourceFormat format) noexcept;
    [[nodiscard]] const char* ResourceClassLabel(
        ResourceClass resourceClass) noexcept;

    // Compares Renegade's fixed capability table against the exact extension
    // sets advertised by the pinned Wicked wi::resourcemanager implementation.
    // A future Wicked pin change therefore fails proof instead of silently
    // changing creator-facing support claims.
    [[nodiscard]] bool ValidatePinnedWickedResourceCapabilities(
        std::string& error);

    // Gate 1 source inspection is read-only. It validates project containment,
    // supported extension/expected format, reads the source without mutation,
    // and performs lightweight signature checks where a stable signature exists.
    [[nodiscard]] ResourceSourceInspectionResult InspectResourceSource(
        const ResourceSourceInspectionRequest& request);
}
