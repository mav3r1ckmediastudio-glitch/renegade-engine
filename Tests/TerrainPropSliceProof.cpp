#include "renegade/bridge/TerrainPropDocument.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/ModelImportCommitService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/PlayerService.h"
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;
    void Check(bool pass, const std::string& message)
    {
        if (!pass) throw std::runtime_error(message);
    }

    void CheckPopulation(const wi::scene::Scene& scene)
    {
        Check(scene.meshes.GetCount() == 4 && scene.materials.GetCount() == 4, "prototype resource counts");
        Check(scene.objects.GetCount() == 128 && scene.rigidbodies.GetCount() == 64, "placement counts");
        std::map<wi::ecs::Entity, size_t> uses;
        std::set<wi::ecs::Entity> materials;
        for (size_t i = 0; i < scene.objects.GetCount(); ++i)
        {
            const auto meshId = scene.objects[i].meshID;
            ++uses[meshId];
            auto* mesh = scene.meshes.GetComponent(meshId);
            Check(mesh && !mesh->subsets.empty(), "missing shared mesh");
            for (const auto& subset : mesh->subsets)
            {
                Check(scene.materials.Contains(subset.materialID), "missing native material");
                materials.insert(subset.materialID);
            }
        }
        Check(uses.size() == 4 && materials.size() == 4, "shared native resource identities");
        for (const auto& entry : uses) Check(entry.second == 32, "each mesh shared by 32 placements");
    }

    void Run(const fs::path& run, const fs::path& fixtures)
    {
        fs::create_directories(run);
        SceneService scenes;
        scenes.GetScene().Entity_CreateTransform("Authored witness");
        const auto starter = (run / "starter.wiscene").generic_u8string();
        {
            wi::Archive archive;
            scenes.GetScene().Serialize(archive);
            Check(archive.SaveFile(starter), "starter archive");
        }
        ProjectService projects;
        projects.Initialize((run / "recents.ini").string());
        Check(projects.CreateProject(run.string(), "TerrainProof", starter), projects.LastError());
        const auto& project = projects.CurrentProject();
        const fs::path root(project.rootPath);
        CommandService commands;
        SelectionService selection;
        SceneDocumentService documents(scenes, selection, commands, projects);
        TerrainPropDocument doc;
        doc.layerId = GenerateStableId();
        TerrainPropPage page;
        const std::array<std::string, 3> names{"Tree", "Bush", "Boulder"};
        for (size_t i = 0; i < names.size(); ++i)
        {
            auto candidate = ModelImportCandidateService().PrepareModel((fixtures / (names[i] + ".glb")).generic_u8string());
            Check(candidate.IsReady(), "prepare custom " + names[i]);
            ModelImportCommitRequest request;
            request.projectRoot = project.rootPath;
            request.projectId = project.projectId;
            request.assetName = names[i];
            auto result = ModelImportCommitService().CommitStaticModel(request, candidate);
            Check(result.succeeded, names[i] + ": " + result.error);
            TerrainPropProfile profile;
            profile.id[15] = static_cast<std::uint8_t>(1 + i);
            profile.modelAssetId = result.assetId;
            if (i == 0) profile.collision = {TerrainPropShape::TrunkCapsule, {0,1.4f,0}, {.23f,1.17f,.23f}};
            if (i == 2) profile.collision = {TerrainPropShape::BoulderBox, {0,.62f,0}, {.8f,.62f,.57f}};
            doc.profiles.push_back(profile);
            page.profiles.push_back(profile.id);
            for (unsigned j = 0; j < 32; ++j)
                page.placements.push_back({doc.AllocatePlacementId(), static_cast<unsigned>(i),
                    {static_cast<float>(i * 8), 0, static_cast<float>(j * 8)}, 0, 1});
        }
        doc.pages.push_back(page);
        auto owner = scenes.GetScene().Entity_CreateTransform("Terrain layer");
        std::string error;
        Check(PublishTerrainPropRevision(root, project.projectId, doc, scenes.GetScene(), owner, error), error);
        const auto destination = (root / "Content/Scenes/PropSlice.wiscene").generic_u8string();
        Check(documents.Save(destination), scenes.LastError());
        Check(documents.Open(destination), scenes.LastError());
        CheckPopulation(scenes.GetScene());
        Check(documents.Save(destination), scenes.LastError());
        auto raw = PrepareWickedSceneOpen(destination);
        Check(raw.IsReady(), raw.Error());
        auto* archive = raw.MutablePreparedScene();
        Check(archive->objects.GetCount() == 0 && archive->meshes.GetCount() == 0 &&
            archive->materials.GetCount() == 0 && archive->rigidbodies.GetCount() == 0 &&
            archive->Entity_FindByName("Authored witness") && archive->Entity_FindByName("Terrain layer"),
            "transient serialization boundary");
        auto& live = scenes.GetScene();
        const auto transient = live.objects.GetEntity(0);
        selection.BindScene(live);
        selection.Select(transient);
        Check(!selection.HasSelection() && !scenes.IsHierarchyVisible(transient), "transient editing boundary");
        Check(live.Entity_Duplicate(transient) == wi::ecs::INVALID_ENTITY, "transient duplicate boundary");
        owner = live.Entity_FindByName("Terrain layer");
        auto revision = live.metadatas.GetComponent(owner)->string_values.get("renegade.terrain_props.revision");
        TerrainPropDocument roundTrip;
        Check(ReadTerrainPropRevision(root, project.projectId, revision, roundTrip, error), error);
        Check(roundTrip.pages[0].placements[95].id == 96 && roundTrip.AllocatePlacementId() == 97, "reopened stable ID");
        auto options = ProjectDocumentTransactionOptions{};
        options.operationHook = [](auto stage, size_t index, const auto&, auto&)
        {
            return stage == ProjectDocumentTransactionStage::AfterReplace && index == 0
                ? ProjectDocumentTransactionHookAction::Fail : ProjectDocumentTransactionHookAction::Continue;
        };
        doc.pages[0].placements[0].scale = 1.5f;
        Check(!PublishTerrainPropRevision(root, project.projectId, doc, live, owner, error, options),
            "injected failure did not roll back");
        Check(live.metadatas.GetComponent(owner)->string_values.get("renegade.terrain_props.revision") == revision,
            "failed publish advanced active revision");
        // Lose the page temporarily. Both open and save must refuse to
        // publish a broken reference, while the existing visible scene survives.
        fs::path pageFile;
        for (const auto& item : fs::directory_iterator(root / "Content/TerrainProps/Revisions"))
            if (item.path().extension() == ".rppg") pageFile = item.path();
        Check(!pageFile.empty(), "missing fixture page");
        fs::path hidden = pageFile;
        hidden += ".held";
        fs::rename(pageFile, hidden);
        const bool refusedOpen = !documents.Open(destination);
        const bool refusedSave = !documents.Save(destination);
        fs::rename(hidden, pageFile);
        Check(refusedOpen && refusedSave, "missing page accepted");
        CheckPopulation(live);
        Check(PublishTerrainPropRevision(root, project.projectId, doc, live, owner, error), error);
        const auto changedScene = (root / "Content/Scenes/Changed.wiscene").generic_u8string();
        Check(documents.Save(changedScene), scenes.LastError());
        Check(documents.Open(changedScene), scenes.LastError());
        const auto editedOwner = live.Entity_FindByName("Terrain layer");
        const auto changed = live.metadatas.GetComponent(editedOwner)->string_values.get("renegade.terrain_props.revision");
        Check(changed != revision && ReadTerrainPropRevision(root, project.projectId, changed, roundTrip, error),
            "Save As lost second revision");
        Check(roundTrip.pages[0].placements[0].id == 1 && roundTrip.pages[0].placements[0].scale == 1.5f,
            "edited placement lost ID or scale");
        Check(documents.Open(destination), scenes.LastError());
        SceneService runtime;
        Check(runtime.LoadScene(destination), runtime.LastError());
        auto& world = runtime.GetScene();
        CheckPopulation(world);
        wi::physics::SetEnabled(true);
        for (int i=0; i<3; ++i) world.Update(1.f/60);
        auto hit = [&](float x, float y)
        {
            wi::primitive::Ray ray(XMFLOAT3(x,y,-3), XMFLOAT3(0,0,1), 0, 6);
            return wi::physics::Intersects(world, ray).entity != wi::ecs::INVALID_ENTITY;
        };
        Check(hit(0,1) && !hit(0,3.5f) && !hit(8,.5f) && hit(16,.5f), "Jolt trunk/canopy/bush/boulder rays");
        std::uint64_t meshBytes = 0;
        unsigned lodCount = 0;
        for (size_t i = 0; i < world.meshes.GetCount(); ++i)
        {
            const auto& mesh = world.meshes[i];
            meshBytes += mesh.generalBuffer.desc.size;
            lodCount += mesh.GetLODCount();
        }
        std::cout << "native_mesh_buffer_bytes=" << meshBytes << " native_mesh_lods=" << lodCount
            << " impostor_component_count=" << world.impostors.GetCount()
            << " impostor_atlas_payload_bytes=" << (world.impostorArray.IsValid() ?
                wi::graphics::ComputeTextureMemorySizeInBytes(world.impostorArray.desc) : 0) << std::endl;

        const auto trunkHit = wi::physics::Intersects(world,
            wi::primitive::Ray(XMFLOAT3(0,1,-3), XMFLOAT3(0,0,1), 0, 6));
        world.Entity_Remove(trunkHit.entity, false);
        world.Update(1.f/60);
        Check(!hit(0,1) && hit(16,.5f), "removed trunk still collides or removal affected boulder");

        // Exercise the pinned native LOD and impostor allocation paths in this
        // disposable runtime scene only. The authored project keeps its original models.
        const auto meshId = world.meshes.GetEntity(0);
        auto& benchmarkMesh = world.meshes[0];
        auto reduced = benchmarkMesh.subsets.front();
        const auto firstBufferBytes = benchmarkMesh.generalBuffer.desc.size;
        const auto firstIndex = reduced.indexOffset;
        reduced.indexOffset = static_cast<uint32_t>(benchmarkMesh.indices.size());
        reduced.indexCount = 3;
        for (unsigned i = 0; i < 3; ++i)
        {
            const auto index = benchmarkMesh.indices[firstIndex + i];
            benchmarkMesh.indices.push_back(index);
        }
        benchmarkMesh.subsets_per_lod = 1;
        benchmarkMesh.subsets.push_back(reduced);
        benchmarkMesh.CreateRenderData();
        world.impostors.Create(meshId);
        world.Update(1.f/60);
        Check(benchmarkMesh.GetLODCount() == 2 && world.impostorArray.IsValid(), "native LOD/impostor probe");
        std::cout << "lod1_buffer_bytes=" << firstBufferBytes
            << " combined_lod_buffer_bytes=" << benchmarkMesh.generalBuffer.desc.size
            << " native_impostor_atlas_bytes=" << wi::graphics::ComputeTextureMemorySizeInBytes(world.impostorArray.desc)
            << " atlas_slices=" << world.impostorArray.desc.array_size << std::endl;

        // Give the standalone player a floor and a repeatable view of all three assets.
        const auto ground = live.Entity_CreateCube("Proof ground");
        auto& floor = *live.transforms.GetComponent(ground);
        floor.Scale(XMFLOAT3(32,.25f,150));
        floor.Translate(XMFLOAT3(8,-.25f,116));
        floor.UpdateTransform();
        live.materials.GetComponent(ground)->baseColor = XMFLOAT4(.19f,.24f,.14f,1);
        live.rigidbodies.Create(ground).mass = 0;
        live.Entity_CreateLight("Proof light", {5,15,-5}, {1,.94f,.84f}, 1600, 80);
        TransformState spawn;
        spawn.translation = {8,.1f,-14};
        CreatePlayerStartCommand createStart(live, spawn);
        Check(createStart.Execute(), "player start");
        Check(documents.Save(projects.StartupScenePath()), scenes.LastError());
        std::cout << "runtime_project=" << project.descriptorPath << std::endl;
        std::cout << "PASS 96 placements, 128 objects, 4 mesh IDs, 4 material IDs, 64 static bodies; "
            << "authored-only WISCENE, rollback, Studio reopen, runtime load and four Jolt rays.\n"
            << destination << std::endl;
    }
}

int RunTerrainPropSlice(const std::filesystem::path& output, const std::filesystem::path& fixtures)
{
    try { Run(output / GenerateStableId(), fixtures); return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << std::endl; return 1; }
}
