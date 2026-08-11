#include "renegade/bridge/ReusableAssetService.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr const char* ProjectId = "11111111-1111-4111-8111-111111111111";
    constexpr const char* SourceId = "22222222-2222-4222-8222-222222222222";
    constexpr const char* AssetId = "33333333-3333-4333-8333-333333333333";
    constexpr const char* OtherProjectId = "44444444-4444-4444-8444-444444444444";

    bool Check(bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "ReusableAssetTests FAIL: " << message << '\n';
        return false;
    }

    std::string HashPayload(const std::vector<std::uint8_t>& payload)
    {
        constexpr std::uint64_t Offset = 1469598103934665603ull;
        constexpr std::uint64_t Prime = 1099511628211ull;
        std::uint64_t hash = Offset;
        for (const auto value : payload)
        {
            hash ^= value;
            hash *= Prime;
        }
        constexpr char Hex[] = "0123456789abcdef";
        std::string text = "fnv1a64:";
        for (int shift = 60; shift >= 0; shift -= 4)
            text.push_back(Hex[(hash >> shift) & 0x0f]);
        return text;
    }

    renegade::bridge::ReusableModelAssetDocument MakeDocument()
    {
        using namespace renegade::bridge;
        ReusableModelAssetDocument document;
        document.manifest.projectId = ProjectId;
        document.manifest.assetId = AssetId;
        document.manifest.sourceAssetId = SourceId;
        document.manifest.sourceFormat = "fbx";
        document.manifest.importer = "wicked.ufbx";
        document.manifest.importerVersion = 1;
        document.manifest.settingsJson =
            "{\"options\":{},\"source_format\":\"fbx\"}";
        document.payload = {0x57, 0x49, 0x53, 0x43, 0x45, 0x4e, 0x45, 0x01};
        document.manifest.payloadHash = HashPayload(document.payload);
        return document;
    }

    bool WriteText(const fs::path& path, const std::string& value)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(value.data(), static_cast<std::streamsize>(value.size()));
        return static_cast<bool>(stream);
    }

    bool WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& value)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!value.empty())
            stream.write(reinterpret_cast<const char*>(value.data()),
                static_cast<std::streamsize>(value.size()));
        return static_cast<bool>(stream);
    }
}

int main()
{
    using namespace renegade::bridge;
    bool passed = true;
    std::string error;

    const auto document = MakeDocument();
    passed &= Check(ValidateReusableModelAssetDocument(document, error),
        "valid model document rejected: " + error);

    std::vector<std::uint8_t> first;
    std::vector<std::uint8_t> second;
    passed &= Check(SerializeReusableModelAssetDocument(document, first, error),
        "serialize failed: " + error);
    passed &= Check(SerializeReusableModelAssetDocument(document, second, error),
        "repeat serialize failed: " + error);
    passed &= Check(first == second,
        "canonical RAsset serialization is not byte deterministic");

    ReusableModelAssetDocument reopened;
    passed &= Check(DeserializeReusableModelAssetDocument(first, reopened, error),
        "deserialize failed: " + error);
    passed &= Check(reopened.manifest.projectId == ProjectId &&
            reopened.manifest.assetId == AssetId &&
            reopened.manifest.sourceAssetId == SourceId &&
            reopened.manifest.sourceFormat == "fbx" &&
            reopened.manifest.importer == "wicked.ufbx" &&
            reopened.payload == document.payload,
        "round-trip changed RAsset identity/recipe/payload");

    auto corruptPayload = first;
    corruptPayload.back() ^= 0xffu;
    passed &= Check(!DeserializeReusableModelAssetDocument(
            corruptPayload, reopened, error),
        "payload corruption was accepted");

    auto wrongMagic = first;
    wrongMagic.front() = 'X';
    passed &= Check(!DeserializeReusableModelAssetDocument(
            wrongMagic, reopened, error),
        "wrong RAsset magic was accepted");

    auto unsupportedVersion = first;
    const std::string versionNeedle = "\"schema_version\":1";
    const auto versionAt = std::search(
        unsupportedVersion.begin(), unsupportedVersion.end(),
        versionNeedle.begin(), versionNeedle.end());
    passed &= Check(versionAt != unsupportedVersion.end(),
        "could not locate schema version in canonical manifest");
    if (versionAt != unsupportedVersion.end())
    {
        *(versionAt + static_cast<std::ptrdiff_t>(versionNeedle.size() - 1)) = '2';
        passed &= Check(!DeserializeReusableModelAssetDocument(
                unsupportedVersion, reopened, error),
            "unsupported RAsset schema version was accepted");
    }

    auto contradictoryRecipe = document;
    contradictoryRecipe.manifest.settingsJson =
        "{\"options\":{},\"source_format\":\"glb\"}";
    passed &= Check(!ValidateReusableModelAssetDocument(
            contradictoryRecipe, error),
        "manifest/recipe source-format contradiction was accepted");

    const fs::path root = fs::temp_directory_path() /
        "renegade-lp07-gate3-headless";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "Content" / "Models", ec);
    fs::create_directories(root / "SourceAssets" / "Models", ec);
    fs::create_directories(root / "Intermediate" / "Transactions", ec);
    passed &= Check(!ec, "could not create headless project fixture");

    const fs::path rassetPath = root / "Content" / "Models" / "fixture.rasset";
    passed &= Check(WriteBytes(rassetPath, first), "could not write RAsset fixture");
    passed &= Check(ReadReusableModelAssetDocument(
            rassetPath.generic_u8string(), reopened, error),
        "file RAsset reopen failed: " + error);

    ReusableAssetService service;
    ReusableModelImportRequest request;
    request.projectRoot = root.generic_u8string();
    request.projectId = ProjectId;
    request.sourceProjectRelativePath = "Content/Models/source.fbx";
    request.assetProjectRelativePath = "Content/Models/new.rasset";
    const auto outsideSource = service.ImportModelAsset(request);
    passed &= Check(!outsideSource.succeeded &&
            outsideSource.error.find("SourceAssets") != std::string::npos,
        "source outside SourceAssets did not fail closed before conversion");

    passed &= Check(WriteText(root / "SourceAssets" / "Models" / "source.fbx", "not-a-real-fbx"),
        "could not write source fixture");
    request.sourceProjectRelativePath = "SourceAssets/Models/source.fbx";
    request.assetProjectRelativePath = "Content/Models/fixture.rasset";
    const auto existingProduct = service.ImportModelAsset(request);
    passed &= Check(!existingProduct.succeeded &&
            existingProduct.error.find("already exists") != std::string::npos,
        "existing RAsset destination was not rejected before conversion");
    std::vector<std::uint8_t> preserved;
    {
        std::ifstream stream(rassetPath, std::ios::binary);
        preserved.assign(std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
    }
    passed &= Check(preserved == first,
        "rejected replacement modified the last-good RAsset bytes");

    request.assetProjectRelativePath = "Content/Models/new.rasset";
    request.settingsJson = "{\"z\":1,\"a\":2}";
    const auto nonCanonicalSettings = service.ImportModelAsset(request);
    passed &= Check(!nonCanonicalSettings.succeeded &&
            nonCanonicalSettings.error.find("canonical") != std::string::npos,
        "non-canonical import settings were accepted");

    AssetRegistry foreignRegistry;
    foreignRegistry.projectId = OtherProjectId;
    std::string foreignJson;
    passed &= Check(SerializeAssetRegistry(foreignRegistry, foreignJson, error),
        "could not serialize cross-project registry fixture: " + error);
    passed &= Check(WriteText(root / AssetRegistryDocumentName, foreignJson),
        "could not write cross-project registry fixture");
    request.settingsJson = "{}";
    const auto crossProject = service.ImportModelAsset(request);
    passed &= Check(!crossProject.succeeded &&
            crossProject.error.find("another project") != std::string::npos,
        "cross-project registry did not fail closed before conversion");

    fs::remove_all(root, ec);
    if (!passed)
        return 1;

    std::cout << "LP07_GATE3_HEADLESS_PASS\n";
    return 0;
}
