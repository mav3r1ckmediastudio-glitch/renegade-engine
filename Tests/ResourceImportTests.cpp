#include "renegade/bridge/AssetBrowserService.h"
#include "renegade/bridge/ResourceImportService.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    int failures = 0;

    void Require(const bool condition, const std::string& message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL // " << message << '\n';
        }
    }

    void WriteBytes(
        const fs::path& path,
        const std::vector<std::uint8_t>& bytes)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }

    void WriteText(const fs::path& path, const std::string& text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << text;
    }

    std::vector<std::uint8_t> ReadBytes(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::vector<std::uint8_t>(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
    }

    void InspectRepresentative(
        const fs::path& root,
        const std::string& relativePath,
        const ResourceSourceFormat expectedFormat,
        const ResourceClass expectedClass,
        const AssetType expectedType)
    {
        ResourceSourceInspectionRequest request;
        request.projectRoot = root.generic_u8string();
        request.sourceProjectRelativePath = relativePath;
        request.expectedFormat = expectedFormat;

        const auto result = InspectResourceSource(request);
        Require(result.succeeded, relativePath + " should inspect successfully: " + result.error);
        Require(result.format == expectedFormat, relativePath + " format mismatch");
        Require(result.resourceClass == expectedClass, relativePath + " class mismatch");
        Require(result.assetType == expectedType, relativePath + " AssetType mismatch");
        Require(result.byteCount > 0, relativePath + " should report source bytes");
    }
}

int main()
{
    std::string error;
    Require(
        ValidatePinnedWickedResourceCapabilities(error),
        "Renegade LP08 resource table must exactly match pinned Wicked: " + error);

    const auto& formats = GetSupportedResourceFormats();
    Require(formats.size() == 13, "LP08 pinned resource table should contain 13 formats");

    Require(DetectResourceSourceFormat("thing.PNG") == ResourceSourceFormat::Png,
        "PNG detection should be case-insensitive");
    Require(DetectResourceSourceFormat("thing.ogg") == ResourceSourceFormat::Ogg,
        "OGG should be accepted");
    Require(DetectResourceSourceFormat("thing.mp3") == ResourceSourceFormat::Unknown,
        "MP3 must not be claimed by the pinned Wicked resource contract");
    Require(DetectResourceSourceFormat("thing.ttf") == ResourceSourceFormat::Ttf,
        "TTF should be accepted by the pinned Wicked resource contract");

    Require(AssetBrowserService::Classify("Content/Textures/a.png") == AssetType::Texture,
        "Asset Browser should classify PNG as texture");
    Require(AssetBrowserService::Classify("Content/Audio/a.wav") == AssetType::Audio,
        "Asset Browser should classify WAV as audio");
    Require(AssetBrowserService::Classify("Content/Scripts/a.lua") == AssetType::Script,
        "Asset Browser should classify Lua as script");
    Require(AssetBrowserService::Classify("Content/Video/a.mp4") == AssetType::Video,
        "Asset Browser should classify MP4 as video");
    Require(AssetBrowserService::Classify("Content/Fonts/a.ttf") == AssetType::Font,
        "Asset Browser should classify TTF as font");
    Require(AssetBrowserService::Classify("Content/a.mp3") == AssetType::Unknown,
        "Asset Browser must not advertise unsupported MP3 by extension");
    Require(std::string(AssetBrowserService::TypeLabel(AssetType::Font)) == "FONT",
        "Asset Browser should expose a FONT label");

    const fs::path root = fs::temp_directory_path() / "renegade-lp08-gate1-resource";
    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);
    fs::create_directories(root / "SourceAssets");

    const fs::path png = root / "SourceAssets/Textures/sample.png";
    WriteBytes(png, {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A});
    const auto pngBefore = ReadBytes(png);
    InspectRepresentative(
        root,
        "SourceAssets/Textures/sample.png",
        ResourceSourceFormat::Png,
        ResourceClass::Texture,
        AssetType::Texture);
    Require(ReadBytes(png) == pngBefore, "resource inspection must not mutate PNG source bytes");

    WriteBytes(
        root / "SourceAssets/Audio/sample.wav",
        {'R','I','F','F',0,0,0,0,'W','A','V','E'});
    InspectRepresentative(
        root,
        "SourceAssets/Audio/sample.wav",
        ResourceSourceFormat::Wav,
        ResourceClass::Audio,
        AssetType::Audio);

    WriteText(root / "SourceAssets/Scripts/sample.lua", "return { gate = 1 }\n");
    InspectRepresentative(
        root,
        "SourceAssets/Scripts/sample.lua",
        ResourceSourceFormat::Lua,
        ResourceClass::Script,
        AssetType::Script);

    WriteBytes(
        root / "SourceAssets/Video/sample.mp4",
        {0,0,0,16,'f','t','y','p','i','s','o','m'});
    InspectRepresentative(
        root,
        "SourceAssets/Video/sample.mp4",
        ResourceSourceFormat::Mp4,
        ResourceClass::Video,
        AssetType::Video);

    WriteBytes(
        root / "SourceAssets/Fonts/sample.ttf",
        {0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00});
    InspectRepresentative(
        root,
        "SourceAssets/Fonts/sample.ttf",
        ResourceSourceFormat::Ttf,
        ResourceClass::Font,
        AssetType::Font);

    WriteBytes(
        root / "SourceAssets/Textures/wrong.png",
        {'R','I','F','F',0,0,0,0,'W','A','V','E'});
    ResourceSourceInspectionRequest mismatch;
    mismatch.projectRoot = root.generic_u8string();
    mismatch.sourceProjectRelativePath = "SourceAssets/Textures/wrong.png";
    mismatch.expectedFormat = ResourceSourceFormat::Png;
    const auto mismatchResult = InspectResourceSource(mismatch);
    Require(!mismatchResult.succeeded,
        "reliable signature mismatch must fail closed");

    ResourceSourceInspectionRequest wrongExpected;
    wrongExpected.projectRoot = root.generic_u8string();
    wrongExpected.sourceProjectRelativePath = "SourceAssets/Audio/sample.wav";
    wrongExpected.expectedFormat = ResourceSourceFormat::Ogg;
    const auto wrongExpectedResult = InspectResourceSource(wrongExpected);
    Require(!wrongExpectedResult.succeeded,
        "expected-format mismatch must fail closed");

    ResourceSourceInspectionRequest traversal;
    traversal.projectRoot = root.generic_u8string();
    traversal.sourceProjectRelativePath = "../outside.png";
    const auto traversalResult = InspectResourceSource(traversal);
    Require(!traversalResult.succeeded,
        "project traversal resource source must fail closed");

    fs::remove_all(root, cleanupError);

    if (failures != 0)
    {
        std::cerr << "LP08 GATE 1 RESOURCE SEAM FAIL // " << failures << " checks failed\n";
        return 1;
    }

    std::cout << "LP08 GATE 1 RESOURCE SEAM PASS // formats=" << formats.size()
              << " classes=texture,audio,script,video,font\n";
    return 0;
}
