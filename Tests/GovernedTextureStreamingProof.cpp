#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ModelImportCommitService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/TerrainPropProjection.h"
#include "renegade/bridge/ResourceAssetRuntimeService.h"

#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/ReusableAssetDependencyService.h"
#include "renegade/bridge/ResourceAssetDependencyService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/TerrainPropDocument.h"

#include <WickedEngine.h>
#include <Windows.h>
#include <Psapi.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>

namespace
{
    using namespace renegade::bridge;
    namespace fs = std::filesystem;
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
        const fs::path run = fs::absolute(directory) / fs::u8path(GenerateStableId());
        fs::create_directories(run);
        const auto starter = (run / "starter.wiscene").generic_u8string();
        {
            wi::scene::Scene blank;
            wi::Archive archive;
            blank.Serialize(archive);
            Check(archive.SaveFile(starter), "write starter scene");
        }
        ProjectService projects;
        projects.Initialize((run / "recents.ini").string());
        Check(projects.CreateProject(run.string(), "TerrainProof", starter), projects.LastError());
        const auto project = projects.CurrentProject();
        const fs::path root = fs::u8path(project.rootPath);
        const auto source = run / "wood.tga";
        std::string error;
        WriteImage(source, 192);
        auto imported = CreatorTextureWorkflowService().ImportTexture(project.rootPath, project.projectId,
            source.generic_u8string(), "color");
        Check(imported.succeeded, imported.error);
        std::vector<uint8_t> newPixels;
        {
            PROCESS_MEMORY_COUNTERS beforeBind{};
            Check(GetProcessMemoryInfo(GetCurrentProcess(), &beforeBind, sizeof(beforeBind)) != 0, "pre-bind memory");
            const auto bindStarted = std::chrono::steady_clock::now();
            PreparedMaterialTextureAsset old;
            Check(PrepareMaterialTextureAsset(project.rootPath, project.projectId, imported.assetId, old, error), error);
            wi::scene::Scene scene;
            const auto entity = scene.Entity_CreateMaterial("Boulder surface");
            Check(ApplyPreparedMaterialTextureAsset(scene, entity, MaterialTextureSlot::BaseColor, old, {}, error), error);
            auto& slot = scene.materials.GetComponent(entity)->textures[wi::scene::MaterialComponent::BASECOLORMAP];
            wi::Resource original = slot.resource;
            const auto loadTime = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - bindStarted);
            std::cout << "T1_first_bind adapter=" << wi::graphics::GetDevice()->GetAdapterName()
                << " elapsed_ms=" << loadTime.count() << " source_bytes=" << old.payload.size()
                << " prior_working_set=" << beforeBind.WorkingSetSize
                << " prior_peak_working_set=" << beforeBind.PeakWorkingSetSize << std::endl;
            Report("T1_after_bind", original, old);
            const auto firstRead = CaptureMaterialTextureReads(old, "color").initialBytes;
            auto candidate = ModelImportCandidateService().PrepareModel(boulder.generic_u8string());
            Check(candidate.IsReady(), "prepare boulder model");
            auto& candidateScene = *candidate.PeekMutableScene();
            Check(candidateScene.materials.GetCount() == 1, "expected one boulder material");
            candidateScene.materials[0].SetAlphaRef(1.0f);
            Check(ApplyPreparedMaterialTextureAsset(candidateScene, candidateScene.materials.GetEntity(0),
                MaterialTextureSlot::BaseColor, old, {}, error), error);
            Check(CaptureMaterialTextureReads(old, "color").initialBytes == firstRead, "repeated material bind reread DDS");
            ModelImportCommitRequest modelRequest;
            modelRequest.projectRoot = project.rootPath;
            modelRequest.projectId = project.projectId;
            modelRequest.assetName = "StreamedBoulder";
            auto committed = ModelImportCommitService().CommitStaticModel(modelRequest, candidate);
            Check(committed.succeeded, committed.error);
            auto model = ReusableAssetService().PrepareModelAssetPlacement(
                {project.rootPath, project.projectId, committed.assetId});
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
            {
                SceneService authored;
                CommandService history;
                SelectionService selection;
                SceneDocumentService documents(authored, selection, history, projects);
                auto& native = authored.GetScene();
                TerrainPropDocument layer;
                layer.layerId = GenerateStableId();
                layer.nextPlacementId = 33;
                layer.profiles.push_back({profile, committed.assetId, {}});
                layer.pages.push_back(page);
                const auto owner = native.Entity_CreateTransform("T1 boulder layer");
                Check(PublishTerrainPropRevision(root, project.projectId, layer, native, owner, error), error);
                TransformState spawn;
                spawn.translation = {0, 1, -3};
                CreatePlayerStartCommand player(native, spawn);
                Check(player.Execute(), "author player start");
                Check(documents.Save(projects.StartupScenePath()), authored.LastError());
                const auto saveAs = (root / "Content/Scenes/Boulders.wiscene").generic_u8string();
                Check(documents.Save(saveAs), authored.LastError());
                Check(documents.Open(saveAs), authored.LastError());
                Check(native.objects.GetCount() == 32 && native.materials.GetCount() == 1 &&
                    native.materials[0].textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.internal_state.get() ==
                        original.internal_state.get(), "Save As lost shared governed prop material");

                ReusableAssetDependencyProvider provider(project.projectId);
                DependencyNode sceneNode;
                sceneNode.dependencyClass = DependencyClass::Scene;
                sceneNode.projectRelativePath = "Content/Scenes/Boulders.wiscene";
                std::set<std::string> files;
                const auto emit = [&](const DependencyCandidate& c) { files.insert(c.declaredPath); };
                Check(provider.Discover({project.rootPath, &sceneNode}, emit, {}, error), error);
                Check(files.count(committed.assetProjectRelativePath) == 1 && files.size() == 3,
                    "export must discover exactly the referenced prop revision, page and model");
                DependencyNode modelNode;
                modelNode.dependencyClass = DependencyClass::ImportedContent;
                modelNode.projectRelativePath = committed.assetProjectRelativePath;
                ResourceAssetDependencyProvider textures(project.projectId);
                Check(textures.Discover({project.rootPath, &modelNode}, emit, {}, error), error);
                Check(files.count(imported.assetProjectRelativePath) == 1, "export omitted nested governed texture");
            }
            const unsigned cold = original.GetTexture().desc.width;
            Check(cold < Size && original.GetTextureFullMipCount() == 10 && original.GetFileData().empty(), "cold DDS contract");
            Report("T1_cold", original, old);
            Reach(original, Size, Size);
            Report("T1_near", original, old);
            const auto originalPixels = Readback(original);
            Reach(original, 0, cold);
            Report("T1_far", original, old);
            Check(CaptureMaterialTextureReads(old, "color").mipReads >= 2, "no observed backing reads");

            const fs::path moved = root.parent_path() / fs::u8path(GenerateStableId());
            fs::copy(root, moved, fs::copy_options::recursive);
            PreparedMaterialTextureAsset movedTexture;
            Check(PrepareMaterialTextureAsset(moved.generic_u8string(), project.projectId, imported.assetId, movedTexture, error), error);
            auto movedResource = LoadMaterialTextureForSemantic(movedTexture, "color", error);
            Check(movedResource.IsValid() && movedResource.internal_state.get() != original.internal_state.get(),
                "relocated project reused its original backing");
            Reach(movedResource, Size, Size);
            Check(Readback(movedResource) == originalPixels, "copied project mismatch");

            WriteImage(root / Source, 24);
            AssetCatalogue catalogue;
            Check(CreatorAssetWorkflowService().BuildCatalogue(project.rootPath, project.projectId, catalogue, error), error);
            auto replay = ResourceAssetService().ReimportResourceAsset({project.rootPath, project.projectId, imported.assetId});
            Check(replay.succeeded, replay.error);
            PreparedMaterialTextureAsset replacement;
            Check(PrepareMaterialTextureAsset(project.rootPath, project.projectId, imported.assetId, replacement, error), error);
            auto current = LoadMaterialTextureForSemantic(replacement, "color", error);
            Check(current.IsValid() && current.internal_state.get() != original.internal_state.get(), "new texture generation");
            Reach(current, Size, Size);
            newPixels = Readback(current);
            Check(newPixels != originalPixels, "reimport did not replace visual content");
            Reach(original, Size, Size);
            Check(Readback(original) == originalPixels, "old generation read replacement bytes");

            const fs::path package = root.parent_path() / fs::u8path(GenerateStableId());
            const fs::path file = package / "GameData/Content/Textures/wood.rasset";
            fs::create_directories(file.parent_path());
            fs::copy_file(root / Product, file);
            std::ofstream(package / "GameData/content-manifest.json")
                << "{\"format\":\"renegade-content-manifest\",\"schema_version\":1,\"project_id\":\"" << project.projectId
                << "\",\"files\":[{\"asset_id\":\"" << imported.assetId
                << "\",\"path\":\"GameData/Content/Textures/wood.rasset\",\"source_hash\":\"" << replay.productHash << "\"}]}";
            PackagedMaterialTextureRefreshResult packaged;
            Check(RefreshPackagedMaterialTextureAssets(scene, package.generic_u8string(), project.projectId, packaged, error), error);
            Check(packaged.refreshedBindingCount == 1 && slot.resource.internal_state.get() != current.internal_state.get(),
                "standalone package resolver did not acquire its own backing");
            Reach(slot.resource, Size, Size);
            Check(Readback(slot.resource) == newPixels, "packaged DDS readback mismatch");
        }
        // Move the complete closed project, then reopen with the original path absent.
        const fs::path relocated = root.parent_path() / fs::u8path(GenerateStableId());
        // Wicked's job vector keeps its strong references until the next
        // normal streaming update after the outstanding worker finishes.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        std::error_code moveError;
        do
        {
            wi::resourcemanager::UpdateStreamingResources(1.f / 60);
            fs::rename(root, relocated, moveError);
            if (!moveError) break;
            if (moveError.value() != ERROR_ACCESS_DENIED && moveError.value() != ERROR_SHARING_VIOLATION)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } while (std::chrono::steady_clock::now() < deadline);
        Check(!moveError, "moving closed project after native job release: " + moveError.message());
        PreparedMaterialTextureAsset reopened;
        Check(PrepareMaterialTextureAsset(relocated.generic_u8string(), project.projectId, imported.assetId, reopened, error), error);
        auto reopenedResource = LoadMaterialTextureForSemantic(reopened, "color", error);
        Check(reopenedResource.IsValid(), "reopen relocated product");
        Reach(reopenedResource, Size, Size);
        Check(Readback(reopenedResource) == newPixels, "relocated reimported texture pixels");
        std::cout << "T1_project=" << (relocated / fs::u8path(project.descriptorPath).filename()).generic_u8string()
            << " texture=" << imported.assetId << std::endl;
        std::cout << "T1_NATIVE_PASS" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "T1 native proof: " << e.what() << std::endl;
        return 1;
    }
}
