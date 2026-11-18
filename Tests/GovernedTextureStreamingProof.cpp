#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ModelImportCommitService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/TerrainPropProjection.h"
#include "renegade/bridge/ResourceAssetRuntimeService.h"

#include <WickedEngine.h>
#include <Windows.h>
#include <Psapi.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
    using namespace renegade::bridge;
    namespace fs = std::filesystem;
    constexpr const char* Project = "71111111-1111-4111-8111-111111111111";
    constexpr const char* Source = "SourceAssets/Textures/wood.tga";
    constexpr const char* Product = "Content/Textures/wood.rasset";
    constexpr unsigned Size = 512;

    void Check(bool ok, const std::string& reason)
    {
        if (!ok) throw std::runtime_error(reason);
    }

    void WriteImage(const fs::path& file, unsigned char red)
    {
        std::vector<uint8_t> data(18 + Size * Size * 4, 0);
        data[2] = 2;
        data[13] = data[15] = Size >> 8;
        data[16] = 32;
        data[17] = 0x28;
        for (size_t i = 18; i < data.size(); i += 4)
        {
            data[i] = 11; data[i + 1] = 76; data[i + 2] = red; data[i + 3] = 255;
        }
        std::ofstream stream(file, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(data.data()), data.size());
        stream.close();
        Check(static_cast<bool>(stream), "writing TGA fixture");
    }

    void Reach(wi::Resource& texture, unsigned request, unsigned expected)
    {
        const auto stop = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (texture.GetTexture().desc.width != expected && std::chrono::steady_clock::now() < stop)
        {
            texture.StreamingRequestResolution(request);
            wi::resourcemanager::UpdateStreamingResources(1.f / 60);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        Check(texture.GetTexture().desc.width == expected, "residency did not reach requested size");
    }

    void Report(const char* phase, const wi::Resource& resource, const PreparedMaterialTextureAsset& prepared)
    {
        PROCESS_MEMORY_COUNTERS counters{};
        Check(GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) != 0, "process memory");
        const auto stats = CaptureMaterialTextureReads(prepared, "color");
        const auto gpu = wi::graphics::GetDevice()->GetMemoryUsage();
        const auto desc = resource.GetTexture().desc;
        std::cout << phase << " width=" << desc.width << " mips=" << desc.mip_levels
            << " resident_bytes=" << wi::graphics::ComputeTextureMemorySizeInBytes(desc)
            << " initial_dds_read=" << stats.initialBytes << " mip_reads=" << stats.mipReads
            << " mip_read_bytes=" << stats.mipBytes << " working_set=" << counters.WorkingSetSize
            << " peak_working_set=" << counters.PeakWorkingSetSize << " device_usage=" << gpu.usage
            << " device_budget=" << gpu.budget << std::endl;
    }

    std::vector<uint8_t> Readback(const wi::Resource& resource)
    {
        wi::vector<uint8_t> bytes;
        Check(wi::helper::saveTextureToMemory(resource.GetTexture(), bytes) && !bytes.empty(), "DDS GPU readback");
        return {bytes.begin(), bytes.end()};
    }
}

int RunGovernedTextureProof(const std::filesystem::path& directory, const std::filesystem::path& boulder)
{
    using namespace renegade::bridge;
    try
    {
        const fs::path root = directory / fs::u8path(GenerateStableId());
        fs::create_directories((root / Source).parent_path());
        fs::create_directories((root / Product).parent_path());
        fs::create_directories(root / "Intermediate/Transactions");
        AssetRegistry registry;
        registry.projectId = Project;
        registry.schemaVersion = AssetRegistry::CurrentSchemaVersion;
        std::string json, error;
        Check(SerializeAssetRegistry(registry, json, error), error);
        std::ofstream(root / AssetRegistryDocumentName) << json;
        WriteImage(root / Source, 192);
        ResourceAssetImportRequest request;
        request.projectId = Project;
        request.projectRoot = root.generic_u8string();
        request.sourceProjectRelativePath = Source;
        request.assetProjectRelativePath = Product;
        request.expectedFormat = ResourceSourceFormat::Tga;
        request.materialTextureSemantic = "color";
        auto imported = ResourceAssetService().ImportResourceAsset(request);
        Check(imported.succeeded, imported.error);
        PreparedMaterialTextureAsset old;
        Check(PrepareMaterialTextureAsset(request.projectRoot, Project, imported.assetId, old, error), error);
        wi::scene::Scene scene;
        const auto entity = scene.Entity_CreateMaterial("Boulder surface");
        Check(ApplyPreparedMaterialTextureAsset(scene, entity, MaterialTextureSlot::BaseColor, old, {}, error), error);
        auto& slot = scene.materials.GetComponent(entity)->textures[wi::scene::MaterialComponent::BASECOLORMAP];
        wi::Resource original = slot.resource;
        auto candidate = ModelImportCandidateService().PrepareModel(boulder.generic_u8string());
        Check(candidate.IsReady(), "prepare boulder model");
        auto& candidateScene = *candidate.PeekMutableScene();
        Check(candidateScene.materials.GetCount() == 1, "expected one boulder material");
        candidateScene.materials[0].SetAlphaRef(1.0f);
        Check(ApplyPreparedMaterialTextureAsset(candidateScene, candidateScene.materials.GetEntity(0),
            MaterialTextureSlot::BaseColor, old, {}, error), error);
        ModelImportCommitRequest modelRequest;
        modelRequest.projectRoot = request.projectRoot;
        modelRequest.projectId = Project;
        modelRequest.assetName = "StreamedBoulder";
        auto committed = ModelImportCommitService().CommitStaticModel(modelRequest, candidate);
        Check(committed.succeeded, committed.error);
        auto model = ReusableAssetService().PrepareModelAssetPlacement(
            {request.projectRoot, Project, committed.assetId});
        Check(model.IsReady(), model.Result().error);
        wi::scene::Scene terrain;
        TerrainPropProjection projection(terrain, false);
        TerrainPropProfileId profile{};
        profile.back() = 1;
        Check(projection.Register(profile, *model.PeekScene(), {}, error), error);
        TerrainPropPage page;
        page.profiles.push_back(profile);
        for (unsigned j = 0; j < 32; ++j)
            page.placements.push_back({j + 1, 0, {float(j), 0, 0}, 0, 1.f + float(j % 4)});
        Check(projection.Place(page, error), error);
        Check(terrain.objects.GetCount() == 32 && terrain.materials.GetCount() == 1 &&
            terrain.meshes.GetCount() == 1, "boulder placements duplicated shared resources");
        const auto& projected = terrain.materials[0].textures[wi::scene::MaterialComponent::BASECOLORMAP];
        Check(projected.resource.internal_state.get() == original.internal_state.get(),
            "boulder placement lost governed texture binding");
        const unsigned cold = original.GetTexture().desc.width;
        Check(cold < Size && original.GetTextureFullMipCount() == 10 && original.GetFileData().empty(), "cold DDS contract");
        Report("T1_cold", original, old);
        Reach(original, Size, Size);
        Report("T1_near", original, old);
        const auto originalPixels = Readback(original);
        Reach(original, 0, cold);
        Report("T1_far", original, old);
        Check(CaptureMaterialTextureReads(old, "color").mipReads >= 2, "no observed backing reads");

        WriteImage(root / Source, 24);
        AssetCatalogue catalogue;
        Check(CreatorAssetWorkflowService().BuildCatalogue(request.projectRoot, Project, catalogue, error), error);
        auto replay = ResourceAssetService().ReimportResourceAsset({request.projectRoot, Project, imported.assetId});
        Check(replay.succeeded, replay.error);
        PreparedMaterialTextureAsset replacement;
        Check(PrepareMaterialTextureAsset(request.projectRoot, Project, imported.assetId, replacement, error), error);
        auto current = LoadMaterialTextureForSemantic(replacement, "color", error);
        Check(current.IsValid() && current.internal_state.get() != original.internal_state.get(), "new texture generation");
        Reach(current, Size, Size);
        const auto newPixels = Readback(current);
        Check(newPixels != originalPixels, "reimport did not replace visual content");
        Reach(original, Size, Size);
        Check(Readback(original) == originalPixels, "old generation read replacement bytes");

        const fs::path moved = root.parent_path() / fs::u8path(GenerateStableId());
        fs::copy(root, moved, fs::copy_options::recursive);
        PreparedMaterialTextureAsset movedTexture;
        Check(PrepareMaterialTextureAsset(moved.generic_u8string(), Project, imported.assetId, movedTexture, error), error);
        auto movedResource = LoadMaterialTextureForSemantic(movedTexture, "color", error);
        Check(movedResource.IsValid() && movedResource.internal_state.get() != current.internal_state.get(),
            "relocated project reused its original backing");
        Reach(movedResource, Size, Size);
        Check(Readback(movedResource) == newPixels, "relocated project mismatch");

        const fs::path package = root.parent_path() / fs::u8path(GenerateStableId());
        const fs::path file = package / "GameData/Content/Textures/wood.rasset";
        fs::create_directories(file.parent_path());
        fs::copy_file(root / Product, file);
        std::ofstream(package / "GameData/content-manifest.json")
            << "{\"format\":\"renegade-content-manifest\",\"schema_version\":1,\"project_id\":\"" << Project
            << "\",\"files\":[{\"asset_id\":\"" << imported.assetId
            << "\",\"path\":\"GameData/Content/Textures/wood.rasset\",\"source_hash\":\"" << replay.productHash << "\"}]}";
        PackagedMaterialTextureRefreshResult packaged;
        Check(RefreshPackagedMaterialTextureAssets(scene, package.generic_u8string(), Project, packaged, error), error);
        Check(packaged.refreshedBindingCount == 1 && slot.resource.internal_state.get() != current.internal_state.get(),
            "standalone package resolver did not acquire its own backing");
        Reach(slot.resource, Size, Size);
        Check(Readback(slot.resource) == newPixels, "packaged DDS readback mismatch");
        std::cout << "T1_NATIVE_PASS" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "T1 native proof: " << e.what() << std::endl;
        return 1;
    }
}
