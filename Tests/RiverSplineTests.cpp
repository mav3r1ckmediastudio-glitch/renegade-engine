#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "renegade/bridge/RiverSplineService.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/SpecialistComponentService.h"
#include "renegade/bridge/IdentityService.h"

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    bool VerifyNativeRiver(const wi::scene::Scene& scene,
        wi::ecs::Entity root, std::size_t nodes)
    {
        const auto* spline = scene.splines.GetComponent(root);
        const auto* material = scene.materials.GetComponent(root);
        if (!spline || !material || !material->IsCastingShadow() ||
            material->shaderType !=
                wi::scene::MaterialComponent::SHADERTYPE_WATER ||
            !material->HasPlanarReflection() ||
            material->textures[
                wi::scene::MaterialComponent::NORMALMAP].name.find(
                    "waterNormalMap.jpg") == std::string::npos ||
            material->texAnimFrameRate <= 0.0f ||
            std::abs(material->texAnimDirection.y) < 0.0001f ||
            spline->mesh_generation_subdivision <= 0 ||
            spline->mesh_generation_vertical_subdivision != 0 ||
            spline->IsLooped() || spline->IsFilled() ||
            std::abs(spline->width - 4.0f) > 0.001f ||
            spline->terrain_modifier_amount != 0 ||
            spline->spline_node_entities.size() != nodes)
            return false;
        for (const auto node : spline->spline_node_entities)
        {
            if (!scene.Entity_IsDescendant(node, root) ||
                !scene.transforms.Contains(node))
                return false;
        }
        return true;
    }
}

int main()
{
    // Native WATER orientation is an EXPLICIT opt-in only. Reject flat rivers,
    // waterfalls, and unexpected descendants without touching authoring state.
    {
        using namespace renegade::bridge;
        wi::scene::Scene untouched;
        RiverSplineOptions opt;
        const std::vector<XMFLOAT3> flat={{0,12,0},{0,12,8},{0,12,16}};
        CreateRiverSplineCommand createFlat(untouched,flat,opt);
        if(!createFlat.Execute())return Fail("flat rejection fixture creation");
        const auto flatID=createFlat.CreatedEntity();
        const auto* flatRoot=untouched.transforms.GetComponent(flatID);
        const auto before=CaptureTransform(*flatRoot);
        CommandService invalidAlign;
        if(invalidAlign.Execute(std::make_unique<
            OrientNearPlanarRiverWaterPlaneCommand>(untouched,flatID)) ||
            invalidAlign.CanUndo())
            return Fail("flat river must never be auto-oriented");
        const auto after=CaptureTransform(*untouched.transforms.GetComponent(flatID));
        if(after.rotation.x!=before.rotation.x ||
           after.translation.y!=before.translation.y)
            return Fail("failed flat alignment changed root transform");

        wi::scene::Scene cliffs;
        const std::vector<XMFLOAT3> badGrade={
            {0,42,0},{0,40,5},{0,25,10},{0,23,15},{0,20,20}};
        CreateRiverSplineCommand createCliff(cliffs,badGrade,opt);
        if(!createCliff.Execute())return Fail("nonplanar fixture creation");
        const auto cliffID=createCliff.CreatedEntity();
        CommandService failClosed;
        if(failClosed.Execute(std::make_unique<
            OrientNearPlanarRiverWaterPlaneCommand>(cliffs,cliffID)) ||
           failClosed.CanUndo())
            return Fail("waterfall / strongly variable grade must be refused");
        const auto extra=cliffs.Entity_CreateTransform("Additional authored child");
        cliffs.Component_Attach(extra,cliffID);
        if(failClosed.Execute(std::make_unique<
            OrientNearPlanarRiverWaterPlaneCommand>(cliffs,cliffID)))
            return Fail("extra hierarchy child must fail closed");
        std::cout<<"RIVER_NATIVE_REFLECTION_ALIGN_GUARDS=PASS\\n";
    }
    {
        using namespace renegade::bridge;
        wi::scene::Scene terrainScene;
        const auto tid=terrainScene.Entity_CreateTransform("Fit planning terrain");
        auto& terrain=terrainScene.terrains.Create(tid);
        terrain.scene=&terrainScene;terrain.terrainEntity=tid;terrain.generation=1;
        terrain.bottomLevel=-100;terrain.topLevel=100;
        terrain.SetCenterToCamEnabled(false);terrain.SetRemovalEnabled(false);
        for(int cz=-1;cz<=1;++cz) for(int cx=-1;cx<=1;++cx)
        {
            const auto id=terrainScene.Entity_CreateTransform("Plan grid");
            auto& mesh=terrainScene.meshes.Create(id);
            for(int z=0;z<wi::terrain::chunk_width;++z)
                for(int x=0;x<wi::terrain::chunk_width;++x)
                {
                    const float wx=float(cx*66+x-33),wz=float(cz*66+z-33);
                    mesh.vertex_positions.push_back({wx,20.f-.4f*wx+.3f*wz,wz});
                }
            wi::terrain::Chunk coordinate;coordinate.x=cx;coordinate.z=cz;
            terrain.chunks[coordinate].entity=id;
        }
        RiverSplineOptions options;options.widthMeters=4;
        CreateRiverSplineCommand create(terrainScene,{{-16,26.4f,0},{0,20,2},{16,13.6f,0}},options);
        if(!create.Execute())return Fail("mountain planning river create");
        const auto rid=create.CreatedEntity();
        MountainStreamFit fit;std::string error;
        if(!PlanMountainStream(terrainScene,tid,rid,fit,error))
        {std::cerr<<error<<'\n';return Fail("cross-slope downhill planning");}
        if(fit.points.size()<=3 || fit.maximumCutMeters>4 || fit.lengthMeters<32)
            return Fail("adaptive mountain plan bounds");
        for(std::size_t i=1;i<fit.points.size();++i)
            if(fit.points[i].y>fit.points[i-1].y+.003f)
                return Fail("mountain plan uphill water");
        const auto unchanged=terrainScene.meshes[0].vertex_positions[0].y;
        const auto previous=fit.points;
        auto* root=terrainScene.transforms.GetComponent(rid);
        root->scale_local.x=2;root->SetDirty();
        if(PlanMountainStream(terrainScene,tid,rid,fit,error) || fit.points.size()!=previous.size())
            return Fail("scaled mountain root rejection leaves output unchanged");
        root->scale_local.x=1;root->SetDirty();
        const auto last=terrainScene.splines.GetComponent(rid)->spline_node_entities.back();
        auto* node=terrainScene.transforms.GetComponent(last);
        node->translation_local=terrainScene.transforms.GetComponent(
            terrainScene.splines.GetComponent(rid)->spline_node_entities.front())->translation_local;
        node->SetDirty();
        if(PlanMountainStream(terrainScene,tid,rid,fit,error))
            return Fail("mountain cusp/zero-distance rejection");
        if(terrainScene.meshes[0].vertex_positions[0].y!=unchanged ||
            HasRiverTerrainLayers(terrainScene,tid))
            return Fail("planning must never carve or publish metadata");
        // A cliff is sampled from native height vertices, not source node Y.
        node->translation_local={16,13.6f,0};node->SetDirty();
        for(std::size_t m=0;m<terrainScene.meshes.GetCount();++m)
            for(auto& p:terrainScene.meshes[m].vertex_positions)
                p.y=p.x<0?40.f:10.f;
        if(PlanMountainStream(terrainScene,tid,rid,fit,error))
            return Fail("cliff must be rejected rather than stretched");
        std::cout<<"MOUNTAIN_PLAN_CROSS_SLOPE_ADAPTIVE_DOWNHILL_CLIFF_SCALE_CUSP=PASS\n";
    }

    {
        wi::scene::Scene legacy;
        const auto river=legacy.Entity_CreateTransform("Legacy caustics migration");
        legacy.splines.Create(river);
        auto& material=legacy.materials.Create(river);
        material.shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
        material.SetCastShadow(false);
        std::string error;
        if(!renegade::bridge::AssignNewPersistentEntityId(legacy,river,error))
            return Fail("legacy identity setup");
        const auto id=renegade::bridge::PersistentEntityId(legacy,river);
        renegade::bridge::RestoreRiverCaustics(legacy);
        if(!material.IsCastingShadow()||renegade::bridge::PersistentEntityId(legacy,river)!=id)
            return Fail("native caustics migration must retain river UUID");
        material.SetCastShadow(false);
        renegade::bridge::RestoreRiverCaustics(legacy);
        if(material.IsCastingShadow())
            return Fail("caustics migration must preserve later authored shadow choice");
    }
    // Riverbed inspector readback is read-only and keyed by stable UUID.
    // Synthetic metadata tests do not need a GPU terrain regeneration.
    {
        using namespace renegade::bridge;
        wi::scene::Scene readback;
        const auto terrain=readback.Entity_CreateTransform("Terrain - inspector proof");
        readback.terrains.Create(terrain);
        const auto stream=readback.Entity_CreateTransform("Linked stream");
        readback.splines.Create(stream);
        readback.materials.Create(stream).shaderType =
            wi::scene::MaterialComponent::SHADERTYPE_WATER;
        std::string error;
        if (!AssignNewPersistentEntityId(readback,stream,error))
            return Fail("readback river stable ID fixture");
        RiverTerrainLayerSettings settings;
        if (!ReadRiverTerrainLayerSettings(readback,terrain,stream,settings,error) ||
            settings.linked || settings.depthMeters!=DefaultRiverBedDepthMeters ||
            settings.bankWidthMeters!=DefaultRiverBankWidthMeters)
            return Fail("unlinked river inspector must read stock bed defaults");
        readback.metadatas.Create(terrain);
        // ComponentManager storage can reallocate when a second river gains
        // identity metadata. Resolve the terrain component for every write.
        const auto setLayerMetadata=[&](const std::string& payload) {
            readback.metadatas.GetComponent(terrain)->string_values.set(
                RiverTerrainLayersMetadataKey,payload);
        };
        const std::string riverId=PersistentEntityId(readback,stream);
        const auto payload=[&](const std::string& layers) {
            return std::string("{\"version\":1,\"chunks\":[],\"layers\":") + layers + "}";
        };
        const auto layer=[&](float depth,float bank) {
            return "[{\"river\":\""+riverId+"\",\"depth\":"+
                std::to_string(depth)+",\"bank\":"+std::to_string(bank)+"}]";
        };
        setLayerMetadata(payload(layer(2.35f,3.5f)));
        if (!ReadRiverTerrainLayerSettings(readback,terrain,stream,settings,error) ||
            !settings.linked || std::abs(settings.depthMeters-2.35f)>.0001f ||
            std::abs(settings.bankWidthMeters-3.5f)>.0001f)
            return Fail("linked river inspector must read persisted depth/bank");
        const auto previous=settings;
        // Two selected rivers must never borrow one another's saved settings.
        const auto second=readback.Entity_CreateTransform("Second linked stream");
        readback.splines.Create(second);
        readback.materials.Create(second).shaderType =
            wi::scene::MaterialComponent::SHADERTYPE_WATER;
        if (!AssignNewPersistentEntityId(readback,second,error))
            return Fail("second readback river identity fixture");
        const auto secondId=PersistentEntityId(readback,second);
        const auto linkedItem=layer(2.35f,3.5f).substr(1,layer(2.35f,3.5f).size()-2);
        const std::string otherItem="{\"river\":\""+secondId+
            "\",\"depth\":0.75,\"bank\":4.25}";
        setLayerMetadata(
            payload("["+linkedItem+","+otherItem+"]"));
        RiverTerrainLayerSettings secondSettings;
        if (!ReadRiverTerrainLayerSettings(readback,terrain,second,secondSettings,error) ||
            !secondSettings.linked ||
            std::abs(secondSettings.depthMeters-0.75f)>.0001f ||
            std::abs(secondSettings.bankWidthMeters-4.25f)>.0001f ||
            !ReadRiverTerrainLayerSettings(readback,terrain,stream,settings,error) ||
            std::abs(settings.depthMeters-2.35f)>.0001f ||
            std::abs(settings.bankWidthMeters-3.5f)>.0001f)
            return Fail("river selection readback must isolate multiple channel settings");
        setLayerMetadata(payload("[]"));
        if (!ReadRiverTerrainLayerSettings(readback,terrain,stream,settings,error) ||
            settings.linked || settings.depthMeters!=DefaultRiverBedDepthMeters)
            return Fail("unlinked river inspector must reset displayed stock bed");
        setLayerMetadata(
            payload(layer(2.35f,3.5f)));
        if (!ReadRiverTerrainLayerSettings(readback,terrain,stream,settings,error) ||
            !settings.linked || settings.depthMeters!=previous.depthMeters)
            return Fail("river bed inspector Undo/Redo metadata readback");
        const auto invalidBefore=settings;
        setLayerMetadata("broken JSON");
        if (ReadRiverTerrainLayerSettings(readback,terrain,stream,settings,error) ||
            settings.linked!=invalidBefore.linked ||
            settings.depthMeters!=invalidBefore.depthMeters)
            return Fail("invalid riverbed metadata must not corrupt inspector values");
        const std::string duplicated=layer(2.35f,3.5f);
        const std::string item=duplicated.substr(1,duplicated.size()-2);
        setLayerMetadata(
            payload("["+item+","+item+"]"));
        if (ReadRiverTerrainLayerSettings(readback,terrain,stream,settings,error))
            return Fail("duplicate riverbed UUID should fail closed");
    }

    using renegade::bridge::RiverBedEndDistance;
    if(RiverBedEndDistance(0,0,20,4)!=4 ||
       RiverBedEndDistance(0,20,20,4)!=4 ||
       RiverBedEndDistance(0,10,20,4)!=0 ||
       RiverBedEndDistance(9,2,20,4)<=3 ||
       RiverBedEndDistance(0,.5f,1,4)>=4)
        return Fail("rounded bed endings must stay inside native open ribbon");

    using namespace renegade::bridge;
    const std::vector<XMFLOAT3> points = {
        {1.0f, 3.0f, 1.0f},
        {6.0f, 2.0f, 4.0f},
        {12.0f, 1.0f, 7.0f}
    };

    if (!IsRiverSplineStrokeValid(points) ||
        IsRiverSplineStrokeValid({}) ||
        IsRiverSplineStrokeValid({points.front()}) ||
        IsRiverSplineStrokeValid({points.front(), points.front()}) ||
        IsRiverSplineStrokeValid({
            points.front(),
            XMFLOAT3(std::numeric_limits<float>::quiet_NaN(), 0, 0)}))
        return Fail("river input validity contract");

    wi::scene::Scene scene;
    CommandService commands;
    auto river = std::make_unique<CreateRiverSplineCommand>(scene, points);
    auto* command = river.get();
    if (!commands.Execute(std::move(river)))
        return Fail("creating river through command service");
    const auto root = command->CreatedEntity();
    if (root == wi::ecs::INVALID_ENTITY ||
        !VerifyNativeRiver(scene, root, points.size()))
        return Fail("native spline/material/scene hierarchy");
    const auto riverIdentity = PersistentEntityId(scene,root);
    if (!IsValidStableId(riverIdentity)) return Fail("river requires a persistent layer identity");
    // Owner-approved pale blue must be the initial native WATER material,
    // not a screenshot-only UI preset. Clarity 0.874 = native alpha 0.126.
    const auto* createdWater = scene.materials.GetComponent(root);
    const RiverWaterMaterialState stockWater;
    const auto matchesStock = [&stockWater](const wi::scene::MaterialComponent* water)
    {
        if (!water) return false;
        const auto& c = water->baseColor, d = stockWater.tintOpacity;
        return std::abs(c.x - .031f) < .0001f &&
            std::abs(c.y - .044f) < .0001f &&
            std::abs(c.z - .047f) < .0001f &&
            std::abs(1.0f - c.w - .874f) < .0001f &&
            std::abs(c.x - d.x) < .0001f &&
            std::abs(c.y - d.y) < .0001f &&
            std::abs(c.z - d.z) < .0001f &&
            std::abs(c.w - d.w) < .0001f &&
            std::abs(water->texAnimDirection.y - stockWater.flowUvV) < .00001f;
    };
    if (!matchesStock(createdWater) ||
        DefaultRiverBedDepthMeters != 1.35f ||
        DefaultRiverBankWidthMeters != 2.0f)
        return Fail("fresh river stock colour, clarity, flow and bed defaults");
    wi::Archive defaultArchive;
    scene.Serialize(defaultArchive);
    defaultArchive.SetReadModeAndResetPos(true);
    wi::scene::Scene stockReopened;
    stockReopened.Serialize(defaultArchive);
    if (!matchesStock(stockReopened.materials.GetComponent(
            stockReopened.Entity_FindByName("River Water"))))
        return Fail("fresh stock river must survive native WISCENE roundtrip");
    const auto* first = scene.transforms.GetComponent(
        scene.splines.GetComponent(root)->spline_node_entities.front());
    if (!first || std::abs(first->translation_local.y - 2.85f) > 0.002f)
        return Fail("water elevation offset");

    // Native WATER supports >1 fog coefficients even though the original
    // Clarity UI only exposed 0..1. Verify strong shallow-water murkiness is
    // safe in the authoritative Undo stack and WISCENE material archive.
    {
        RiverWaterMaterialState dense;
        if(!CaptureRiverWaterMaterialState(scene,root,dense))
            return Fail("capture native water before strong murkiness");
        const auto original=dense.tintOpacity.w;
        dense.tintOpacity.w=8.0f;
        CommandService murkCommands;
        if(!murkCommands.Execute(
                std::make_unique<SetRiverWaterMaterialCommand>(scene,root,dense)))
            return Fail("native strong water murkiness was rejected");
        const auto* murky=scene.materials.GetComponent(root);
        if(!murky || std::abs(murky->baseColor.w-8.f)>.001f ||
           murky->IsCustomShader() ||
           murky->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER)
            return Fail("strong fog must keep original WATER material");
        wi::Archive murkyArchive;
        scene.Serialize(murkyArchive);
        murkyArchive.SetReadModeAndResetPos(true);
        wi::scene::Scene murkyReopened;
        murkyReopened.Serialize(murkyArchive);
        const auto murkyEntity=murkyReopened.Entity_FindByName("River Water");
        const auto* reopenedMaterial=murkyReopened.materials.GetComponent(murkyEntity);
        if(!reopenedMaterial ||
           std::abs(reopenedMaterial->baseColor.w-8.f)>.001f ||
           reopenedMaterial->IsCustomShader())
            return Fail("native fog coefficient 8 did not survive WISCENE");
        if(!murkCommands.Undo() ||
           std::abs(scene.materials.GetComponent(root)->baseColor.w-original)>.001f)
            return Fail("Undo must restore exact owner-approved fog");
        if(!murkCommands.Redo() ||
           std::abs(scene.materials.GetComponent(root)->baseColor.w-8.f)>.001f)
            return Fail("Redo must restore strong murkiness");
        if(!murkCommands.Undo())
            return Fail("Undo must restore stock appearance after Redo");
        dense.tintOpacity.w=16.01f;
        CommandService invalidMurk;
        if(invalidMurk.Execute(
            std::make_unique<SetRiverWaterMaterialCommand>(scene,root,dense)))
            return Fail("out-of-range fog density accepted");
        std::cout<<"RIVER_STRONG_NATIVE_MURK_UNDO_ARCHIVE=PASS\\n";
    }
    // Editor centreline must follow current transforms before a Scene update,
    // including a parent translation; it must not mutate native spline caches.
    const auto beforeSamples = SampleSplineWorldPoints(scene, root, 24);
    if (beforeSamples.size() != 24 ||
        !SampleSplineWorldPoints(scene, root, 1).empty())
        return Fail("shared spline sampling bounds");
    auto* rootTransform = scene.transforms.GetComponent(root);
    rootTransform->translation_local.x += 7.0f;
    rootTransform->scale_local.x = 2.0f;
    rootTransform->SetDirty();
    const auto movedSamples = SampleSplineWorldPoints(scene, root, 24);
    if (movedSamples.size() != beforeSamples.size())
        return Fail("sampling edited parent");
    for (std::size_t i = 0; i < movedSamples.size(); ++i)
        if (std::abs(movedSamples[i].x - beforeSamples[i].x * 2.0f - 7.0f) > 0.002f)
            return Fail("shared spline ignored live parent transform");
    rootTransform->translation_local.x -= 7.0f;
    rootTransform->scale_local.x = 1.0f;
    rootTransform->SetDirty();

    if (!commands.Undo() || scene.splines.Contains(root) ||
        scene.names.Contains(root))
        return Fail("Undo must remove whole water spline");
    if (!commands.Redo() || !VerifyNativeRiver(scene, root, points.size()) ||
        !matchesStock(scene.materials.GetComponent(root)) ||
        PersistentEntityId(scene,root) != riverIdentity)
        return Fail("Redo must restore stock WATER appearance, identity and child nodes");
    if (!commands.Undo())
        return Fail("second Undo");
    if (!commands.Redo() || !VerifyNativeRiver(scene, root, points.size()) ||
        !matchesStock(scene.materials.GetComponent(root)))
        return Fail("second Redo must preserve stock WATER appearance");

    // River editor changes must not touch water normal-map tiling, texture
    // identity, refraction or terrain. Test native bridge command Undo/Redo.
    RiverWaterMaterialState waterState;
    if (!CaptureRiverWaterMaterialState(scene, root, waterState))
        return Fail("capture of WATER spline material");
    const auto originalWater = waterState;
    auto* nativeWater = scene.materials.GetComponent(root);
    if (nativeWater->IsCustomShader()) return Fail("new rivers must use native WATER");
    nativeWater->SetCustomShaderID(123); // Simulate a rejected candidate's saved ID.
    if (!BindRiverSurfaceShader(scene, root) || nativeWater->IsCustomShader() ||
        nativeWater->baseColor.x != originalWater.tintOpacity.x ||
        nativeWater->normalMapStrength != originalWater.normalStrength ||
        nativeWater->texAnimDirection.y != originalWater.flowUvV)
        return Fail("native WATER restore must clear only experimental custom binding");
    const auto nativeTiling = scene.materials.GetComponent(root)->texMulAdd;
    const auto nativeNormal = scene.materials.GetComponent(root)->textures[
        wi::scene::MaterialComponent::NORMALMAP].name;
    waterState.tintOpacity = XMFLOAT4(.23f,.34f,.45f,.56f);
    waterState.flowUvV = .004f;
    waterState.flowUvU = -.002f;
    waterState.normalStrength = 2.4f;
    CommandService waterHistory;
    if (!waterHistory.Execute(std::make_unique<SetRiverWaterMaterialCommand>(
            scene, root, waterState)))
        return Fail("river material command execute");
    auto* waterMaterial = scene.materials.GetComponent(root);
    if (std::abs(waterMaterial->baseColor.w-.56f)>.0001f ||
        std::abs(waterMaterial->texAnimDirection.y-.004f)>.00001f ||
        std::abs(waterMaterial->texAnimDirection.x+.002f)>.00001f ||
        std::abs(waterMaterial->normalMapStrength-2.4f)>.0001f ||
        waterMaterial->texMulAdd.x!=nativeTiling.x ||
        waterMaterial->texMulAdd.y!=nativeTiling.y ||
        waterMaterial->textures[wi::scene::MaterialComponent::NORMALMAP].name!=nativeNormal)
        return Fail("material command modified unrelated native UV or texture");
    if (!waterHistory.Undo() ||
        std::abs(waterMaterial->baseColor.w-originalWater.tintOpacity.w)>.0001f ||
        std::abs(waterMaterial->texAnimDirection.y-originalWater.flowUvV)>.00001f ||
        std::abs(waterMaterial->texAnimDirection.x-originalWater.flowUvU)>.00001f ||
        std::abs(waterMaterial->normalMapStrength-originalWater.normalStrength)>.0001f)
        return Fail("river water material Undo");
    if (!waterHistory.Redo() ||
        std::abs(waterMaterial->baseColor.w-.56f)>.0001f ||
        std::abs(waterMaterial->texAnimDirection.y-.004f)>.00001f ||
        std::abs(waterMaterial->texAnimDirection.x+.002f)>.00001f ||
        std::abs(waterMaterial->normalMapStrength-2.4f)>.0001f)
        return Fail("river water material Redo");
    RiverWaterMaterialState invalidWater=waterState;
    invalidWater.tintOpacity.w=std::numeric_limits<float>::quiet_NaN();
    CommandService invalidWaterHistory;
    if (invalidWaterHistory.Execute(std::make_unique<SetRiverWaterMaterialCommand>(
            scene,root,invalidWater)) || invalidWaterHistory.UndoCount()!=0)
        return Fail("nonfinite river appearance must refuse mutation");

    invalidWater=waterState;invalidWater.normalStrength=4;
    if(invalidWaterHistory.Execute(std::make_unique<SetRiverWaterMaterialCommand>(scene,root,invalidWater)))
        return Fail("invalid normal strength accepted");
    invalidWater=waterState;invalidWater.flowUvU=std::numeric_limits<float>::infinity();
    if(invalidWaterHistory.Execute(std::make_unique<SetRiverWaterMaterialCommand>(scene,root,invalidWater)))
        return Fail("nonfinite U scroll accepted");
    wi::Archive opticsArchive;scene.Serialize(opticsArchive);opticsArchive.SetReadModeAndResetPos(true);
    wi::scene::Scene opticsReload;opticsReload.Serialize(opticsArchive);
    RiverWaterMaterialState optics;
    if(!CaptureRiverWaterMaterialState(opticsReload,opticsReload.Entity_FindByName("River Water"),optics) ||
        std::abs(optics.flowUvU+.002f)>.00001f || std::abs(optics.normalStrength-2.4f)>.0001f)
        return Fail("direction and ripple strength archive persistence");

    // Immediate command publication must retain parent space for the native
    // translator and invalidate mesh generation even after dirty bits clear.
    auto* spline = scene.splines.GetComponent(root);
    const auto point = spline->spline_node_entities[1];
    auto* parent = scene.transforms.GetComponent(root);
    parent->translation_local.z = 18.0f;
    parent->SetDirty(); parent->UpdateTransform();
    const auto pointBefore = CaptureTransform(*scene.transforms.GetComponent(point));
    auto pointAfter = pointBefore; pointAfter.translation.z += 5.0f;
    CommandService pointCommands;
    spline->prev_mesh_generation_nodes = 3;
    if (!pointCommands.Execute(std::make_unique<SetTransformCommand>(scene,point,pointBefore,pointAfter)) ||
        spline->prev_mesh_generation_nodes != -1 ||
        std::abs(scene.transforms.GetComponent(point)->GetPosition().z - pointAfter.translation.z - 18.0f) > 0.001f)
        return Fail("point command must publish world pose and invalidate native mesh");
    spline->prev_mesh_generation_nodes = 3;
    if (!pointCommands.Undo() || spline->prev_mesh_generation_nodes != -1 ||
        std::abs(scene.transforms.GetComponent(point)->GetPosition().z - pointBefore.translation.z - 18.0f) > 0.001f)
        return Fail("point Undo must publish parent world pose and invalidate native mesh");
    if (!pointCommands.Redo()) return Fail("point Redo");
    pointCommands.Undo();

    // Native material settings used by the River inspector must survive
    // serialization independent of scene defaults or an editor session.
    auto* editedWater = scene.materials.GetComponent(root);
    editedWater->SetBaseColor(XMFLOAT4(.26f, .31f, .44f, .52f));
    editedWater->texAnimDirection.y = .00325f; // authored reverse flow
    editedWater->SetDirty();
    // A WISCENE round trip must retain the water material, spline,
    // original node hierarchy and authored positions.
    wi::Archive sceneArchive;
    scene.Serialize(sceneArchive);
    sceneArchive.SetReadModeAndResetPos(true);
    wi::scene::Scene reopened;
    reopened.Serialize(sceneArchive);
    const auto reopenedRoot = reopened.Entity_FindByName("River Water");
    if (reopenedRoot == wi::ecs::INVALID_ENTITY ||
        !VerifyNativeRiver(reopened, reopenedRoot, points.size()) ||
        PersistentEntityId(reopened,reopenedRoot) != riverIdentity)
        return Fail("native WISCENE spline/water-material round trip");
    const auto* reopenedWater = reopened.materials.GetComponent(reopenedRoot);
    if (!reopenedWater ||
        std::abs(reopenedWater->baseColor.x - .26f) > .0001f ||
        std::abs(reopenedWater->baseColor.y - .31f) > .0001f ||
        std::abs(reopenedWater->baseColor.z - .44f) > .0001f ||
        std::abs(reopenedWater->baseColor.w - .52f) > .0001f ||
        std::abs(reopenedWater->texAnimDirection.y - .00325f) > .000001f)
        return Fail("river colour/opacity/reverse-speed WISCENE persistence");

    CommandService invalid;
    if (invalid.Execute(std::make_unique<CreateRiverSplineCommand>(
            scene, std::vector<XMFLOAT3>{points.front()})) ||
        invalid.IsDirty() || invalid.UndoCount() != 0)
        return Fail("invalid stroke must not mutate scene/history");

    std::cout << "PASS: river spline creation, water material, nodes, "
                 "validation and recursive Undo/Redo\n";
    return 0;
}
