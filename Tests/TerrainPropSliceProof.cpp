#include "renegade/bridge/TerrainPropDocument.h"
#include "renegade/bridge/TestLevelSnapshotService.h"
#include "renegade/bridge/TerrainPropCollisionWorld.h"
#include "renegade/bridge/PhysicsService.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/ModelImportCommitService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/ResourceAssetCacheIdentityService.h"
#include <iterator>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>

int MeasureTerrainPropResidency(const std::filesystem::path&, wi::scene::MaterialComponent*);

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
        const auto collision=TerrainPropCollisionWorld::Find(scene);
        Check(scene.objects.GetCount()==128&&scene.rigidbodies.GetCount()==(collision?0:64),"placement counts");
        if(collision)Check(collision->Index()->Count()==64&&collision->Index()->ShapeCount()==2,
            "complete authored collision index with shared shapes and no eager map-wide bodies");
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

        page.placements[0].candidateKey = 123;
        doc.pages.push_back(page);
        auto& authoring = doc.authoring;
        authoring.recipe.seed = 987654321;
        authoring.recipe.cellSize = 64;
        authoring.recipe.candidateStep = 2;
        authoring.recipe.mix.name = "Woodland \"test\" mix";
        authoring.recipe.mix.entries.push_back({doc.profiles[0].id, .03f, 3, .8f, 1.2f});
        authoring.savedMixes.push_back(authoring.recipe.mix);
        authoring.bindings = {{123,1}, {456,97}};
        authoring.tombstones = {456};
        authoring.clearings = {{50,50,4}};
        authoring.overrides.push_back({123,1,doc.profiles[0].id,
            page.placements[0].position,page.placements[0].yawRadians,page.placements[0].scale});
        doc.nextPlacementId = 98;
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
        Check(roundTrip.pages[0].placements[95].id == 96 && roundTrip.AllocatePlacementId() == 98, "reopened stable ID");

        Check(roundTrip.authoring.recipe.seed == 987654321 && roundTrip.authoring.recipe.cellSize == 64 &&
            roundTrip.authoring.recipe.candidateStep == 2 &&
            roundTrip.authoring.recipe.mix.name == authoring.recipe.mix.name &&
            roundTrip.authoring.savedMixes.size() == 1 &&
            roundTrip.authoring.savedMixes[0].entries[0].densityPerSquareMetre == .03f &&
            roundTrip.authoring.bindings.size() == 2 && roundTrip.authoring.tombstones[0] == 456 &&
            roundTrip.authoring.clearings[0].radius == 4 && roundTrip.authoring.overrides[0].placementId == 1 &&
            roundTrip.pages[0].placements[0].candidateKey == 123,
            "authoring recipe, mix, exclusions, overrides and candidate keys did not persist");
        const auto revisionStore = root / "Content/TerrainProps/Revisions";
        std::ifstream manifestFile(revisionStore / (revision + ".rprops"), std::ios::binary);
        std::string manifestText{std::istreambuf_iterator<char>(manifestFile), std::istreambuf_iterator<char>()};
        auto rejectManifest = [&](std::string text)
        {
            std::vector<std::uint8_t> content(text.begin(), text.end());
            std::string hash;
            Check(ComputePayloadSha256(content, hash, error), error);
            {
                std::ofstream file(revisionStore / (hash + ".rprops"), std::ios::binary);
                file << text;
            }
            TerrainPropDocument unchanged = roundTrip;
            Check(!ReadTerrainPropRevision(root, project.projectId, hash, unchanged, error) &&
                unchanged.authoring.recipe.seed == roundTrip.authoring.recipe.seed &&
                unchanged.nextPlacementId == roundTrip.nextPlacementId,
                "invalid manifest accepted or changed caller state");
        };
        auto negativeSeed = manifestText;
        negativeSeed.replace(negativeSeed.find("AUTHORING 1 987654321"), 21, "AUTHORING 1 -1");
        rejectManifest(negativeSeed);
        auto overflowingSeed = manifestText;
        overflowingSeed.replace(overflowingSeed.find("987654321"), 9, "18446744073709551616");
        rejectManifest(overflowingSeed);
        rejectManifest(manifestText + "unexpected trailing data");
        // Construct actual v1 profile rows, rather than merely relabelling
        // v3 friction/surface fields as the older format.
        std::istringstream currentManifest(manifestText);
        std::string manifestLine, legacyText;
        Check(bool(std::getline(currentManifest, manifestLine)), "legacy manifest header");
        manifestLine[7]='1';legacyText=manifestLine+'\n';
        for(size_t i=0;i<doc.profiles.size();++i)
        {
            Check(bool(std::getline(currentManifest,manifestLine)), "legacy profile row");
            std::istringstream row(manifestLine);std::string field;
            for(int column=0;column<9;++column)
            {
                Check(bool(row>>field),"legacy profile field");
                if(column)legacyText+=' ';
                legacyText+=field;
            }
            legacyText+='\n';
        }
        for(size_t i=0;i<doc.pages.size();++i)
        {
            Check(bool(std::getline(currentManifest,manifestLine)),"legacy page digest");
            legacyText+=manifestLine+'\n';
        }
        std::vector<std::uint8_t> legacyPageBytes;
        auto legacyPage = doc.pages[0];
        legacyPage.placements[0].candidateKey = 0;
        Check(EncodeTerrainPropPage(legacyPage, legacyPageBytes, error), error);
        for (size_t n = legacyPage.placements.size(); n-- > 0;)
        {
            const auto offset = 24 + legacyPage.profiles.size() * 16 + n * 40;
            legacyPageBytes.erase(legacyPageBytes.begin() + offset + 32, legacyPageBytes.begin() + offset + 40);
        }
        legacyPageBytes[4] = 1;
        std::string legacyPageDigest;
        Check(ComputePayloadSha256(legacyPageBytes, legacyPageDigest, error), error);
        const auto lastLine = legacyText.rfind('\n', legacyText.size() - 2) + 1;
        legacyText.replace(lastLine, 64, legacyPageDigest);
        {
            std::ofstream file(revisionStore / (legacyPageDigest + ".rppg"), std::ios::binary);
            file.write(reinterpret_cast<const char*>(legacyPageBytes.data()), legacyPageBytes.size());
        }
        std::vector<std::uint8_t> legacyManifest(legacyText.begin(), legacyText.end());
        std::string legacyDigest;
        Check(ComputePayloadSha256(legacyManifest, legacyDigest, error), error);
        {
            std::ofstream file(revisionStore / (legacyDigest + ".rprops"), std::ios::binary);
            file << legacyText;
        }
        TerrainPropDocument migrated;
        Check(ReadTerrainPropRevision(root, project.projectId, legacyDigest, migrated, error), error);
        Check(migrated.authoring.bindings.empty() && migrated.authoring.recipe.mix.entries.empty() &&
            migrated.pages[0].placements[0].id == 1 && migrated.pages[0].placements[0].candidateKey == 0 &&
            migrated.nextPlacementId == 98, "v1 manifest/page migration changed IDs or counter");
        auto invalid = doc;
        invalid.authoring.bindings[0].placementId = 98;
        Check(!PublishTerrainPropRevision(root, project.projectId, invalid, live, owner, error) &&
            live.metadatas.GetComponent(owner)->string_values.get("renegade.terrain_props.revision") == revision,
            "invalid binding advanced revision");
        invalid = doc;
        invalid.authoring.recipe.mix.entries[0].profileId = {{99}};
        Check(!PublishTerrainPropRevision(root, project.projectId, invalid, live, owner, error),
            "unknown recipe profile accepted");

        auto options = ProjectDocumentTransactionOptions{};
        options.operationHook = [](auto stage, size_t index, const auto&, auto&)
        {
            return stage == ProjectDocumentTransactionStage::AfterReplace && index == 0
                ? ProjectDocumentTransactionHookAction::Fail : ProjectDocumentTransactionHookAction::Continue;
        };
        doc.pages[0].placements[0].scale = 1.5f;
        doc.authoring.overrides[0].scale = 1.5f;
        Check(!PublishTerrainPropRevision(root, project.projectId, doc, live, owner, error, options),
            "injected failure did not roll back");
        Check(live.metadatas.GetComponent(owner)->string_values.get("renegade.terrain_props.revision") == revision,
            "failed publish advanced active revision");
        // Lose the page temporarily. Both open and save must refuse to
        // publish a broken reference, while the existing visible scene survives.
        const auto activePageLine = manifestText.rfind('\n', manifestText.find("AUTHORING 1 ") - 2) + 1;
        fs::path pageFile = revisionStore / (manifestText.substr(activePageLine, 64) + ".rppg");
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
        const auto collision=runtime.TerrainPropCollisions();
        Check(collision&&collision->Update(1.f/60,{{{8,1,0},{0,0,0},1}}),
            "runtime near collision admission");
        for (int i=0; i<3; ++i) world.Update(1.f/60);
        Check(collision->Stats().resident<collision->Index()->Count(),
            "distant placement bodies were eagerly allocated");
        const auto distant=Raycast(world,wi::primitive::Ray(XMFLOAT3{16,.5f,245},XMFLOAT3{0,0,1},0,6));
        Check(distant.IsValid()&&distant.terrainPropPlacement!=0&&
            distant.surface==ImpactSurfaceType::Stone,"nonresident all-map gameplay ray");
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

        const auto& firstMesh = *world.meshes.GetComponent(world.objects[0].meshID);
        auto* sharedMaterial = world.materials.GetComponent(firstMesh.subsets.front().materialID);
        Check(sharedMaterial && MeasureTerrainPropResidency(run / "native-texture", sharedMaterial) == 0,
            "residency through shared placement material");
        const auto trunkHit = wi::physics::Intersects(world,
            wi::primitive::Ray(XMFLOAT3(0,1,-3), XMFLOAT3(0,0,1), 0, 6));
        world.Entity_Remove(trunkHit.entity, false);
        world.Update(1.f/60);
        Check(!hit(0,1) && hit(16,.5f), "removed trunk still collides or removal affected boulder");
        Check(collision->Update(1.f/60,{{{8,1,0},{0,0,0},1}}),"restore authored native collision");
        world.Update(1.f/60);
        Check(hit(0,1),"authored collision did not recover a removed transient native body");

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

        // A Test Game copy must survive relocation away from the source registry.
        // Reopening under the authoring root alone could mask a missing companion.
        TestLevelSnapshotService snapshots(scenes, commands);
        for (const bool deferred : {false, true})
        {
            TestLevelSnapshot snapshot;
            if (deferred)
            {
                Check(snapshots.CreateDeferred(project, snapshot, error), error);
                Check(snapshots.FinalizeDeferredScene(snapshot, error, false), error);
            }
            else Check(snapshots.Create(project, snapshot, error), error);

            const auto isolated = run / (deferred ? "PlayDeferred" : "PlayImmediate");
            fs::rename(snapshot.sessionDirectory, isolated);
            SceneService player;
            Check(player.LoadScene((isolated / "Content/Scenes/TestLevel.wiscene").string()),
                player.LastError());
            CheckPopulation(player.GetScene());
            fs::rename(isolated, snapshot.sessionDirectory);
            Check(snapshots.Cleanup(snapshot, error), error);
        }

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
        std::cout << "PASS 96 placements, 128 objects, 4 mesh IDs, 4 material IDs, 64 indexed collision placements; "
            << "authored-only WISCENE, rollback, Studio reopen, bounded Runtime bodies and all-map/native rays.\n"
            << destination << std::endl;
    }
}

int RunTerrainPropSlice(const std::filesystem::path& output, const std::filesystem::path& fixtures)
{
    try { Run(output / GenerateStableId(), fixtures); return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << std::endl; return 1; }
}
