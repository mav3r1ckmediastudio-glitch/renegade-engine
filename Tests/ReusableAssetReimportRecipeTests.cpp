#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/CreatorModelMaterialPreparationService.h"
#include "renegade/bridge/CreatorSurfaceBuilderService.h"
#include "renegade/bridge/ReusableAssetService.h"

#include "Utility/stb_image.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr const char* ProjectId = "88888888-8888-4888-8888-888888888888";
    constexpr const char* SourceId = "99999999-9999-4999-8999-999999999999";
    constexpr const char* ProductId = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    constexpr const char* TextureA = "11111111-1111-4111-8111-111111111111";
    constexpr const char* TextureB = "22222222-2222-4222-8222-222222222222";
    constexpr const char* TextureC = "33333333-3333-4333-8333-333333333333";
    constexpr const char* TextureD = "44444444-4444-4444-8444-444444444444";
    constexpr const char* TextureE = "55555555-5555-4555-8555-555555555555";
    constexpr std::uint64_t FnvOffset = 1469598103934665603ull;
    constexpr std::uint64_t FnvPrime = 1099511628211ull;

    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "CREATOR WORKFLOW HARDENING FAIL // " << message << '\n';
        return false;
    }

    std::string HashBytes(const std::vector<std::uint8_t>& bytes)
    {
        std::uint64_t hash = FnvOffset;
        for (const auto value : bytes)
        {
            hash ^= value;
            hash *= FnvPrime;
        }
        std::ostringstream stream;
        stream << "fnv1a64:" << std::hex << std::setfill('0')
               << std::setw(16) << hash;
        return stream.str();
    }

    bool WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;
        if (!bytes.empty())
        {
            output.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        return static_cast<bool>(output);
    }

    bool ReadBytes(const fs::path& path, std::vector<std::uint8_t>& bytes)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return false;
        bytes.assign(std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
        return input.good() || input.eof();
    }

    bool WritePgm(const fs::path& path, const int width, const int height,
        const std::vector<std::uint8_t>& pixels)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output || pixels.size() != static_cast<std::size_t>(width * height))
            return false;
        output << "P5\n" << width << ' ' << height << "\n255\n";
        output.write(reinterpret_cast<const char*>(pixels.data()),
            static_cast<std::streamsize>(pixels.size()));
        return static_cast<bool>(output);
    }

    bool Touch(const fs::path& path)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        return static_cast<bool>(output);
    }

    bool TestCreatorRecipeContract()
    {
        using namespace renegade::bridge;

        CreatorModelImportRecipe recipe;
        recipe.transform.authored = true;
        recipe.transform.positionX = 1.0f;
        recipe.transform.positionY = -2.0f;
        recipe.transform.positionZ = 3.0f;
        recipe.transform.rotationXDegrees = 10.0f;
        recipe.transform.rotationYDegrees = 20.0f;
        recipe.transform.rotationZDegrees = 30.0f;
        recipe.transform.scaleX = 0.5f;
        recipe.transform.scaleY = 0.75f;
        recipe.transform.scaleZ = 1.25f;
        CreatorMaterialImportRecipe material;
        material.materialIndex = 2;
        material.baseColorAssetId = TextureA;
        material.normalAssetId = TextureB;
        material.surfaceAssetId = TextureC;
        material.emissiveAssetId = TextureD;
        material.occlusionAssetId = TextureE;
        material.hasScalarSettings = true;
        material.roughness = 0.82f;
        material.metalness = 0.07f;
        material.reflectance = 0.04f;
        material.normalStrength = 1.25f;
        material.aoStrength = 0.65f;
        material.emissiveStrength = 2.5f;
        recipe.materials.push_back(material);

        CreatorAnimationImportRecipe walk;
        walk.sourceAnimationIndex = 0;
        walk.name = "Walk";
        walk.start = 2.0f;
        walk.end = 18.0f;
        walk.enabled = true;
        recipe.animations.push_back(walk);

        CreatorAnimationImportRecipe unused = walk;
        unused.sourceAnimationIndex = 1;
        unused.name = "Unused";
        unused.start = 20.0f;
        unused.end = 30.0f;
        unused.enabled = false;
        recipe.animations.push_back(unused);

        std::string encoded;
        std::string error;
        if (!Require(SerializeCreatorModelImportOptions(recipe, encoded, error),
                "supported creator recipe did not serialize: " + error))
            return false;

        CreatorModelImportRecipe decoded;
        if (!Require(ParseCreatorModelImportOptions(encoded, decoded, error),
                "serialized creator recipe did not parse: " + error) ||
            !Require(decoded.materials.size() == 1 && decoded.animations.size() == 2,
                "creator recipe did not round-trip material/animation counts") ||
            !Require(decoded.transform.authored &&
                decoded.transform.positionX == 1.0f &&
                decoded.transform.positionY == -2.0f &&
                decoded.transform.positionZ == 3.0f &&
                decoded.transform.rotationXDegrees == 10.0f &&
                decoded.transform.rotationYDegrees == 20.0f &&
                decoded.transform.rotationZDegrees == 30.0f &&
                decoded.transform.scaleX == 0.5f &&
                decoded.transform.scaleY == 0.75f &&
                decoded.transform.scaleZ == 1.25f,
                "creator authored transform did not round-trip") ||
            !Require(decoded.materials[0].materialIndex == 2 &&
                decoded.materials[0].baseColorAssetId == TextureA &&
                decoded.materials[0].normalAssetId == TextureB &&
                decoded.materials[0].surfaceAssetId == TextureC &&
                decoded.materials[0].emissiveAssetId == TextureD &&
                decoded.materials[0].occlusionAssetId == TextureE &&
                decoded.materials[0].hasScalarSettings &&
                decoded.materials[0].roughness == 0.82f &&
                decoded.materials[0].metalness == 0.07f &&
                decoded.materials[0].reflectance == 0.04f &&
                decoded.materials[0].normalStrength == 1.25f &&
                decoded.materials[0].aoStrength == 0.65f &&
                decoded.materials[0].emissiveStrength == 2.5f,
                "creator material stable IDs did not round-trip") ||
            !Require(decoded.animations[0].name == "Walk" &&
                decoded.animations[0].start == 2.0f &&
                decoded.animations[0].end == 18.0f &&
                decoded.animations[0].enabled &&
                !decoded.animations[1].enabled,
                "creator animation clip data did not round-trip"))
            return false;

        CreatorModelImportRecipe invalid;
        if (!Require(!ParseCreatorModelImportOptions(
                "{\"unsupported\":true}", invalid, error) &&
                error.find("unsupported key: unsupported") != std::string::npos,
                "unknown creator option was not rejected by name") ||
            !Require(!ParseCreatorModelImportOptions(
                "{\"materials\":[{\"material_index\":0},{\"material_index\":0}]}",
                invalid, error) && error.find("duplicate material_index") != std::string::npos,
                "duplicate material indices were not rejected") ||
            !Require(!ParseCreatorModelImportOptions(
                "{\"materials\":[{\"material_index\":0,\"normal_asset_id\":\"not-a-stable-id\"}]}",
                invalid, error) && error.find("not a valid stable asset ID") != std::string::npos,
                "invalid governed texture stable ID was not rejected") ||
            !Require(!ParseCreatorModelImportOptions(
                "{\"animations\":[{\"enabled\":true,\"end\":1.0,\"name\":\"Bad\",\"source_animation_index\":0,\"start\":2.0}]}",
                invalid, error) && error.find("invalid start/end range") != std::string::npos,
                "invalid animation clip range was not rejected") ||
            !Require(!ParseCreatorModelImportOptions(
                "{\"transform\":{\"position\":[0,0,0],\"rotation_degrees\":[0,0,0],\"scale\":[0,1,1]}}",
                invalid, error) && error.find("outside its supported range") != std::string::npos,
                "invalid creator transform scale was not rejected"))
            return false;

        wi::scene::Scene transformed;
        const auto sourceRoot = wi::ecs::CreateEntity();
        transformed.names.Create(sourceRoot).name = "Source Root";
        transformed.transforms.Create(sourceRoot);
        CreatorModelImportRecipe transformOnly;
        transformOnly.transform = recipe.transform;
        if (!Require(ApplyCreatorModelImportRecipe(
                transformed, "", ProjectId, transformOnly, error),
                "creator transform recipe did not apply: " + error) ||
            !Require(HasCreatorAuthoredTransform(transformed),
                "creator transform marker was not present after apply"))
            return false;

        return true;
    }

    bool TestCreatorMaterialDetection(const fs::path& root)
    {
        using namespace renegade::bridge;

        const fs::path source = root / "DetectedModel.fbx";
        if (!Touch(source) || !Touch(root / "declared.png") ||
            !Touch(root / "Body_color.png") ||
            !Touch(root / "Body_normal.png") ||
            !Touch(root / "Body_roughness.png") ||
            !Touch(root / "Body_metalness.png") ||
            !Touch(root / "Body_ao.png") ||
            !Touch(root / "Body_emissive.png") ||
            !Touch(root / "Trim_surface.png"))
            return Require(false, "could not create material-detection fixtures");

        wi::scene::Scene scene;
        const auto bodyEntity = wi::ecs::CreateEntity();
        auto& body = scene.materials.Create(bodyEntity);
        scene.names.Create(bodyEntity).name = "Body";
        body.textures[wi::scene::MaterialComponent::BASECOLORMAP].name = "declared.png";

        const auto trimEntity = wi::ecs::CreateEntity();
        scene.materials.Create(trimEntity);
        scene.names.Create(trimEntity).name = "Trim";

        const auto plainEntity = wi::ecs::CreateEntity();
        auto& plain = scene.materials.Create(plainEntity);
        plain.roughness = 0.05f;
        plain.metalness = 1.0f;
        plain.reflectance = 1.0f;
        scene.names.Create(plainEntity).name = "Plain";

        const auto detected = DetectCreatorModelMaterials(scene, source.generic_u8string());
        if (!Require(detected.succeeded && detected.materials.size() == 3,
                "multi-material suffix detection failed: " + detected.error))
            return false;

        const auto& bodyDetected = detected.materials[0];
        const auto& trimDetected = detected.materials[1];
        return Require(bodyDetected.baseColor.overridden &&
                fs::u8path(bodyDetected.baseColor.path).filename() == "declared.png",
                "declared/imported base-color binding did not win over suffix detection") &&
            Require(bodyDetected.normal.overridden &&
                fs::u8path(bodyDetected.normal.path).filename() == "Body_normal.png",
                "_normal suffix was not detected") &&
            Require(bodyDetected.roughness.overridden && bodyDetected.metalness.overridden &&
                bodyDetected.occlusion.overridden,
                "roughness/metalness/AO component maps were not detected") &&
            Require(bodyDetected.emissive.overridden &&
                fs::u8path(bodyDetected.emissive.path).filename() == "Body_emissive.png",
                "_emissive suffix was not detected") &&
            Require(trimDetected.surface.overridden &&
                fs::u8path(trimDetected.surface.path).filename() == "Trim_surface.png",
                "per-material supplied Surface map was not detected independently") &&
            Require(detected.materials[2].roughnessValue == 0.75f &&
                detected.materials[2].metalnessValue == 0.0f &&
                detected.materials[2].reflectanceValue == 0.04f,
                "no-Surface material did not receive neutral dielectric fallback values");
    }

    bool TestCreatorSurfaceBuilder(const fs::path& root)
    {
        using namespace renegade::bridge;

        const fs::path roughness = root / "roughness.pgm";
        const fs::path metalness = root / "metalness.pgm";
        const fs::path ao = root / "ao.pgm";
        const fs::path output = root / "surface.png";
        if (!WritePgm(roughness, 1, 1, {64}) ||
            !WritePgm(metalness, 1, 1, {192}) ||
            !WritePgm(ao, 1, 1, {32}))
            return Require(false, "could not create Surface Builder fixtures");

        CreatorSurfaceBuildRequest request;
        request.roughnessPath = roughness.generic_u8string();
        request.metalnessPath = metalness.generic_u8string();
        request.occlusionPath = ao.generic_u8string();
        request.outputPath = output.generic_u8string();
        request.reflectance = 211;
        const auto built = BuildCreatorSurfaceMap(request);
        if (!Require(built.succeeded && built.width == 1 && built.height == 1,
                "Surface Builder failed valid component maps: " + built.error))
            return false;

        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load(
            output.generic_u8string().c_str(), &width, &height, &channels, 4);
        const bool packedCorrectly = pixels != nullptr && width == 1 && height == 1 &&
            pixels[0] == 32 && pixels[1] == 64 && pixels[2] == 192 && pixels[3] == 211;
        if (pixels != nullptr)
            stbi_image_free(pixels);
        if (!Require(packedCorrectly,
                "Surface Builder did not pack R=AO G=roughness B=metalness A=reflectance"))
            return false;

        CreatorSurfaceBuildRequest defaults;
        defaults.roughnessPath = roughness.generic_u8string();
        defaults.outputPath = (root / "surface-defaults.png").generic_u8string();
        defaults.defaultMetalness = 7;
        defaults.defaultOcclusion = 231;
        defaults.reflectance = 199;
        const auto defaultBuilt = BuildCreatorSurfaceMap(defaults);
        if (!Require(defaultBuilt.succeeded,
                "Surface Builder rejected valid missing-channel defaults: " + defaultBuilt.error))
            return false;
        pixels = stbi_load(defaults.outputPath.c_str(), &width, &height, &channels, 4);
        const bool defaultsCorrect = pixels != nullptr && pixels[0] == 231 &&
            pixels[1] == 64 && pixels[2] == 7 && pixels[3] == 199;
        if (pixels != nullptr)
            stbi_image_free(pixels);
        if (!Require(defaultsCorrect,
                "Surface Builder missing-channel defaults were not deterministic"))
            return false;

        CreatorSurfaceBuildRequest neutral;
        neutral.roughnessPath = roughness.generic_u8string();
        neutral.outputPath = (root / "surface-neutral.png").generic_u8string();
        const auto neutralBuilt = BuildCreatorSurfaceMap(neutral);
        if (!Require(neutralBuilt.succeeded,
                "Surface Builder rejected neutral dielectric defaults: " + neutralBuilt.error))
            return false;
        pixels = stbi_load(neutral.outputPath.c_str(), &width, &height, &channels, 4);
        const bool neutralReflectance = pixels != nullptr && pixels[3] == 10;
        if (pixels != nullptr)
            stbi_image_free(pixels);
        if (!Require(neutralReflectance,
                "Surface Builder defaulted reflectance to a glossy full-white channel"))
            return false;

        const fs::path mismatched = root / "mismatch.pgm";
        if (!WritePgm(mismatched, 2, 1, {1, 2}))
            return Require(false, "could not create mismatched Surface Builder fixture");
        CreatorSurfaceBuildRequest mismatch;
        mismatch.roughnessPath = roughness.generic_u8string();
        mismatch.metalnessPath = mismatched.generic_u8string();
        mismatch.outputPath = (root / "surface-mismatch.png").generic_u8string();
        const auto mismatchResult = BuildCreatorSurfaceMap(mismatch);
        if (!Require(!mismatchResult.succeeded &&
                mismatchResult.error.find("matching dimensions") != std::string::npos,
                "Surface Builder did not reject mismatched component dimensions"))
            return false;

        const fs::path corrupt = root / "corrupt.png";
        if (!WriteBytes(corrupt, {'n','o','t','-','a','n','-','i','m','a','g','e'}))
            return Require(false, "could not create corrupt texture fixture");
        CreatorSurfaceBuildRequest corruptRequest;
        corruptRequest.roughnessPath = corrupt.generic_u8string();
        corruptRequest.outputPath = (root / "surface-corrupt.png").generic_u8string();
        const auto corruptResult = BuildCreatorSurfaceMap(corruptRequest);
        return Require(!corruptResult.succeeded &&
                corruptResult.error.find("Could not decode PBR source image") != std::string::npos,
                "Surface Builder did not fail closed on corrupt source image");
    }

    bool TestCreatorPreviewSurfaceCacheIdentity(const fs::path& root)
    {
        using namespace renegade::bridge;

        const fs::path first = root / "preview-first.pgm";
        const fs::path second = root / "preview-second.pgm";
        if (!WritePgm(first, 1, 1, {32}) ||
            !WritePgm(second, 1, 1, {224}))
        {
            return Require(false,
                "could not create preview Surface cache fixtures");
        }

        CreatorMaterialSourceOverride settings;
        const std::string firstRevision =
            CreatorMaterialPreviewSurfaceRevision(
                first.generic_u8string(), {}, {}, {}, settings);
        const std::string secondRevision =
            CreatorMaterialPreviewSurfaceRevision(
                second.generic_u8string(), {}, {}, {}, settings);
        settings.roughnessValue = 0.25f;
        const std::string scalarRevision =
            CreatorMaterialPreviewSurfaceRevision(
                second.generic_u8string(), {}, {}, {}, settings);

        return Require(!firstRevision.empty() &&
                firstRevision != secondRevision,
                "a replacement Surface source reused the cached preview identity") &&
            Require(secondRevision != scalarRevision,
                "changed PBR values reused the cached preview Surface identity");
    }
}

int main()
{
    using namespace renegade::bridge;

    const fs::path root = fs::temp_directory_path() /
        "renegade-lp07-gate4-recipe";
    const fs::path sourcePath = root / "SourceAssets" / "Models" / "fixture.fbx";
    const fs::path productPath = root / "Content" / "Models" / "fixture.rasset";

    std::error_code ec;
    fs::remove_all(root, ec);
    ec.clear();
    fs::create_directories(sourcePath.parent_path(), ec);
    fs::create_directories(productPath.parent_path(), ec);
    fs::create_directories(root / "Intermediate" / "Transactions", ec);
    const fs::path hardeningRoot = root / "Hardening";
    fs::create_directories(hardeningRoot, ec);
    if (!Require(!ec, "could not create project fixture"))
        return 1;

    if (!TestCreatorRecipeContract() ||
        !TestCreatorMaterialDetection(hardeningRoot) ||
        !TestCreatorSurfaceBuilder(hardeningRoot) ||
        !TestCreatorPreviewSurfaceCacheIdentity(hardeningRoot))
    {
        fs::remove_all(root, ec);
        return 1;
    }

    const std::vector<std::uint8_t> sourceBytes = {
        'n', 'o', 't', '-', 'a', '-', 'r', 'e', 'a', 'l', '-', 'f', 'b', 'x'
    };
    if (!Require(WriteBytes(sourcePath, sourceBytes),
            "could not write source fixture"))
        return 1;

    ReusableAssetService service;
    ReusableModelImportRequest unsupportedImport;
    unsupportedImport.projectRoot = root.generic_u8string();
    unsupportedImport.projectId = ProjectId;
    unsupportedImport.sourceProjectRelativePath = "SourceAssets/Models/fixture.fbx";
    unsupportedImport.assetProjectRelativePath = "Content/Models/rejected.rasset";
    unsupportedImport.settingsJson = "{\"unsupported\":true}";
    const auto rejectedImport = service.ImportModelAsset(unsupportedImport);
    if (!Require(!rejectedImport.succeeded &&
            rejectedImport.error.find("unsupported key: unsupported") !=
                std::string::npos,
            "new import did not reject an unknown creator-option key before conversion: " +
                rejectedImport.error) ||
        !Require(!fs::exists(root / "Content" / "Models" / "rejected.rasset") &&
            !fs::exists(root / AssetRegistryDocumentName),
            "rejected new import created persistent product/registry state"))
        return 1;

    ReusableModelAssetDocument asset;
    asset.manifest.projectId = ProjectId;
    asset.manifest.assetId = ProductId;
    asset.manifest.sourceAssetId = SourceId;
    asset.manifest.sourceFormat = "fbx";
    asset.manifest.importer = "wicked.ufbx";
    asset.manifest.importerVersion = 1;
    asset.manifest.settingsSchema = ReusableModelImportSettingsSchema;
    asset.manifest.settingsVersion = 1;
    asset.manifest.settingsJson =
        "{\"options\":{\"unsupported\":true},\"source_format\":\"fbx\"}";
    asset.payload = {'W', 'I', 'S', 'C', 'E', 'N', 'E', 1};
    asset.manifest.payloadHash = HashBytes(asset.payload);

    std::string error;
    std::vector<std::uint8_t> productBytes;
    if (!Require(SerializeReusableModelAssetDocument(
            asset, productBytes, error),
            "could not create stored version-1 recipe fixture: " + error) ||
        !Require(WriteBytes(productPath, productBytes),
            "could not write product fixture"))
        return 1;

    AssetRegistry registry;
    registry.projectId = ProjectId;

    AssetRecord source;
    source.assetId = SourceId;
    source.dependencyNodeId = std::string("lp07.source:") + SourceId;
    source.projectRelativePath = "SourceAssets/Models/fixture.fbx";
    source.dependencyClass = DependencyClass::ImportedContent;
    source.requirement = DependencyRequirement::EditorOnly;
    source.provider = "lp07.source_asset";
    source.providerVersion = 1;
    source.contentHash = HashBytes(sourceBytes);
    registry.records.push_back(source);

    AssetRecord product;
    product.assetId = ProductId;
    product.dependencyNodeId = std::string("lp07.rasset:") + ProductId;
    product.projectRelativePath = "Content/Models/fixture.rasset";
    product.dependencyClass = DependencyClass::ImportedContent;
    product.requirement = DependencyRequirement::Required;
    product.provider = "lp07.rasset";
    product.providerVersion = 1;
    product.contentHash = HashBytes(productBytes);
    registry.records.push_back(product);

    ImportedProductRecord provenance;
    provenance.sourceAssetId = SourceId;
    provenance.productAssetId = ProductId;
    provenance.importer = "wicked.ufbx";
    provenance.importerVersion = 1;
    provenance.settingsSchema = ReusableModelImportSettingsSchema;
    provenance.settingsVersion = 1;
    provenance.settingsJson = asset.manifest.settingsJson;
    provenance.sourceContentHashAtImport = source.contentHash;
    provenance.productContentHashAtImport = product.contentHash;

    if (!Require(SetImportedProductRecords(
            registry, {provenance}, error),
            "could not create imported-product provenance: " + error))
        return 1;

    AssetRegistryPersistenceOptions persistence;
    persistence.transactionId = "lp07-gate4-recipe-setup";
    const auto written = WriteAssetRegistry(
        root.generic_u8string(), registry, std::move(persistence));
    if (!Require(written.success && written.committed,
            "could not persist registry fixture: " + written.message))
        return 1;

    std::vector<std::uint8_t> productBefore;
    std::vector<std::uint8_t> registryBefore;
    if (!Require(ReadBytes(productPath, productBefore) &&
            ReadBytes(root / AssetRegistryDocumentName, registryBefore),
            "could not capture authoritative bytes before rejection"))
        return 1;

    ReusableModelReimportRequest request;
    request.projectRoot = root.generic_u8string();
    request.projectId = ProjectId;
    request.assetId = ProductId;
    const auto result = service.ReimportModelAsset(request);

    std::vector<std::uint8_t> productAfter;
    std::vector<std::uint8_t> registryAfter;
    const bool passed =
        Require(!result.succeeded,
            "stored recipe with an unknown creator-option key unexpectedly reimported") &&
        Require(result.error.find("unsupported key: unsupported") != std::string::npos,
            "stored creator recipe did not reject the unknown key at the recipe boundary: " +
                result.error) &&
        Require(ReadBytes(productPath, productAfter) &&
            ReadBytes(root / AssetRegistryDocumentName, registryAfter),
            "could not reopen authoritative bytes after rejection") &&
        Require(productAfter == productBefore && registryAfter == registryBefore,
            "recipe rejection mutated authoritative product/registry bytes");

    fs::remove_all(root, ec);
    if (!passed)
        return 1;

    std::cout << "CREATOR MODEL WORKFLOW HARDENING PASS\n";
    return 0;
}
