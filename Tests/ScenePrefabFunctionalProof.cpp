#include "renegade/bridge/SceneAssemblyService.h"
#include "renegade/bridge/SceneAssemblyScriptService.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/LightPresetService.h"
#include "renegade/bridge/ParticleTextureLibraryService.h"
#include "renegade/bridge/MaterialService.h"
#include "renegade/bridge/WindowsGameBuildProjectService.h"
#include "renegade/bridge/BuildStageService.h"
#include "renegade/bridge/BuildIdentityService.h"
#include "renegade/bridge/PackageIntegrityService.h"
#include "renegade/bridge/ReusableAssetRuntimeService.h"
#include "../Studio/src/CharacterPrefabStudioIntegration.h"
#include "../Runtime/src/RuntimeScriptRuntime.h"
#include <Windows.h>
#include <wiConfig.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <set>
using namespace renegade::bridge;
using wi::ecs::Entity;
namespace fs=std::filesystem;
static void Check(bool ok,const std::string& label,const std::string* error=nullptr) {
    if(!ok){std::cerr<<"FAIL: "<<label<<(error?*error:std::string{})<<std::endl;std::exit(1);}std::cout<<"PASS: "<<label<<std::endl;
}
static bool Near(float a,float b){return std::abs(a-b)<.001f;}
static Entity Find(const wi::scene::Scene& s,const std::string& name,Entity root=wi::ecs::INVALID_ENTITY) {
    for(size_t i=0;i<s.names.GetCount();++i){auto e=s.names.GetEntity(i);if(s.names[i].name==name&&
        (root==wi::ecs::INVALID_ENTITY||s.Entity_IsDescendant(e,root)))return e;}return wi::ecs::INVALID_ENTITY;
}
static Entity ById(const wi::scene::Scene& s,const StableId& id) {
    for(size_t i=0;i<s.metadatas.GetCount();++i){auto e=s.metadatas.GetEntity(i);if(PersistentEntityId(s,e)==id)return e;}return wi::ecs::INVALID_ENTITY;
}
static Entity LightIn(const wi::scene::Scene& s,Entity root) {
    for(size_t i=0;i<s.lights.GetCount();++i){auto e=s.lights.GetEntity(i);if(s.Entity_IsDescendant(e,root))return e;}return wi::ecs::INVALID_ENTITY;
}
static void NativeSave(wi::scene::Scene& s,const fs::path& path) {
    wi::Archive a(path.generic_u8string(),false,false);a.SetCompressionEnabled(true);s.Serialize(a);
    Check(a.SaveFile(path.generic_u8string()),"native fixture saved: "+path.filename().u8string());
}
static LRESULT CALLBACK Proc(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcW(w,m,a,b);}
static void CheckLua(const ScriptDocument& doc,wi::scene::Scene& scene,const std::string& projectRoot) {
    std::string error;std::vector<float> before;
    for(size_t i=0;i<scene.lights.GetCount();++i)if(scene.lights[i].GetType()==wi::scene::LightComponent::POINT)before.push_back(scene.lights[i].intensity);
    renegade::runtime::RuntimeScriptRuntime lua;
    Check(lua.StartScene(doc,scene,projectRoot,error),"start actual governed Runtime scripts: "+error,&error);
    Check(lua.ActiveInstanceCount()==4,"both placed prefabs start their two independent behaviours");
    bool changed=false;
    for(int frame=0;frame<120;++frame){lua.Update(1.f/60.f);size_t n=0;
        for(size_t i=0;i<scene.lights.GetCount();++i)if(scene.lights[i].GetType()==wi::scene::LightComponent::POINT)
            changed|=!Near(scene.lights[i].intensity,before[n++]);}
    for(const auto& d:lua.Diagnostics())std::cerr<<d.sourcePath<<": "<<d.message<<std::endl;
    Check(lua.DisabledInstanceCount()==0&&changed,"light behaviours animate and component references remain valid");
    lua.StopScene();size_t n=0;
    for(size_t i=0;i<scene.lights.GetCount();++i)if(scene.lights[i].GetType()==wi::scene::LightComponent::POINT)
        Check(Near(scene.lights[i].intensity,before[n++]),"script stop restores the instance's authored light");
}
static ProjectMetadata CompleteFlow(ProjectMetadata project,const ScriptDocument& scripts) {
    FlowDocument flow;flow.envelope=CreateDocumentEnvelope(project.projectId,StoryFlowDocumentType,"Content/Flow/Main.renegade-flow","prefab-functional-proof");
    FlowNode start;start.id=GenerateStableId();start.kind=FlowNodeKind::GameStart;start.name="Game Start";
    FlowNode level;level.id=GenerateStableId();level.kind=FlowNodeKind::Level;level.name="Brazier Review";level.sceneAssetId=scripts.sceneDocumentId;level.scenePathHint="Content/Scenes/Main.wiscene";
    FlowNode complete;complete.id=GenerateStableId();complete.kind=FlowNodeKind::CompleteGame;complete.name="Complete";
    flow.startNodeId=start.id;flow.nodes={start,level,complete};
    FlowRoute first;first.id=GenerateStableId();first.sourceNodeId=start.id;first.outcome=GameStartOutcome;first.destinationNodeId=level.id;first.destinationEntry="start";
    FlowRoute last;last.id=GenerateStableId();last.sourceNodeId=level.id;last.outcome="level.complete";last.destinationNodeId=complete.id;
    flow.routes={first,last};std::string error;fs::create_directories(fs::u8path(project.rootPath)/"Content/Flow");
    Check(WriteFlowDocument((fs::u8path(project.rootPath)/flow.envelope.pathHint).generic_u8string(),flow,error),"save bounded Runtime review route: "+error,&error);
    project.startupFlow=flow.envelope.pathHint;project.startupFlowId=flow.envelope.documentId;
    wi::config::File file;Check(file.Open(project.descriptorPath),"open private fixture descriptor");
    file.Set("startup_flow",project.startupFlow);file.Set("startup_flow_id",project.startupFlowId);file.Commit();return project;
}
static WindowsGameBuildStageResult StageFixture(const ProjectMetadata& project,const fs::path& binaries,
    const fs::path& output,const std::string& revision,const std::string& wicked) {
    std::string error;WindowsGameBuildProjectState state;
    Check(PrepareWindowsGameBuildProjectState(project,state,error),"prepare real dependency closure for scripted prefab package: "+error,&error);
    std::vector<WindowsRuntimeSupportInput> support;WindowsGameBuildStagingRequest staging;
    auto add=[&](const fs::path& source,const std::string& destination) {
        WindowsGamePackageFileDigest digest;Check(DigestWindowsGamePackageFile(source.generic_u8string(),digest,error),"hash Runtime support: "+destination);
        const std::string logical=destination=="BrazierProof.exe"?"renegade-runtime":
            destination=="dxcompiler.dll"?"directx-shader-compiler":"prefab-support-"+std::to_string(support.size());
        support.push_back({logical,destination,digest.byteCount,digest.sha256,"repo:"+revision});
        staging.runtimeSupportSources.push_back({destination,source.generic_u8string()});
    };
    add(binaries/"RenegadeRuntime.exe","BrazierProof.exe");
    for(const auto& entry:fs::recursive_directory_iterator(binaries))if(entry.is_regular_file()&&entry.path().extension()==".dll")
        add(entry.path(),entry.path().lexically_relative(binaries).generic_u8string());
    for(const auto& entry:fs::recursive_directory_iterator(binaries/"Content"))
        if(entry.is_regular_file()&&entry.path().extension()==".cso")add(entry.path(),entry.path().lexically_relative(binaries).generic_u8string());
    WindowsGameBuildRequest request;request.gameName="BrazierProof";request.executableBaseName="BrazierProof";
    request.publicVersion="0.1.0-prefab-proof";request.saveDataId=project.projectId;
    WindowsGameBuildPlan plan;Check(CreateWindowsGameBuildPlan(project,state.dependencyGraph,state.assetRegistry,request,support,plan,error),"plan private native package: "+error,&error);
    const auto notice=output/"Acceptance-Only.txt";{std::ofstream f(notice);f<<"Private local acceptance fixture. Not a distribution build.\n";}
    for(const auto& destination:{"ReadMe.txt","Licences/Renegade-Licence-or-Notice.txt","Licences/WickedEngine-LICENSE.txt",
        "Licences/WickedEngine-third_party_software.txt","Licences/DirectXShaderCompiler-LICENSE.txt","Licences/DirectXShaderCompiler-ThirdPartyNotices.txt"})
        staging.packageDocuments.push_back({destination,notice.generic_u8string(),"private-acceptance-fixture","repo:"+revision});
    staging.projectRootPath=project.rootPath;staging.outputParentPath=(output/"Packages").generic_u8string();
    staging.stagingId=GenerateStableId();staging.renegadeRevision=revision;staging.wickedRevision=wicked;
    WindowsGameBuildStageResult stage;Check(StageWindowsGameBuild(plan,staging,stage,error),"stage Runtime, sources, textures and shaders through native package services: "+error,&error);
    const auto icon=output/"PrivateProof.ico";
    {
        std::vector<unsigned char> bytes;
        auto word=[&](unsigned n){bytes.push_back(n&255);bytes.push_back((n>>8)&255);};
        auto dword=[&](unsigned n){word(n&65535);word(n>>16);};
        word(0);word(1);word(1);bytes.insert(bytes.end(),{1,1,0,0});
        word(1);word(32);dword(48);dword(22);
        dword(40);dword(1);dword(2);word(1);word(32);
        dword(0);dword(4);dword(0);dword(0);dword(0);dword(0);
        bytes.insert(bytes.end(),{210,130,60,255,0,0,0,0});
        std::ofstream f(icon,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        Check(bool(f),"write disposable proof executable icon");
    }
    WindowsGameExecutableIdentityRequest identity;
    identity.developerPublisher="Renegade Engine";identity.description="Private functional prefab acceptance fixture";
    identity.copyrightNotice="Private local proof";identity.internalBuildId="scene-prefab-functional-proof";
    identity.buildTimestampUtc="2026-11-10T16:00:00Z";identity.iconSourcePath=icon.generic_u8string();
    WindowsGameExecutableIdentityResult identified;
    Check(ApplyWindowsGameExecutableIdentity(plan,identity,stage,identified,error),"apply native package-relative bootstrap identity: "+error,&error);
    Check(ValidateWindowsGameBuildStage(stage,error),"private package hashes and closure validate: "+error,&error);
    std::ofstream(output/"package-path.txt")<<stage.stagingPath<<'\n';return stage;
}
int main(int argc,char** argv) {
    if(argc<2)return 2;const auto output=fs::absolute(fs::u8path(argv[1]));fs::create_directories(output);
    const auto binaries=fs::absolute(fs::u8path(argv[0])).parent_path();
    const std::string revision=argc>2?argv[2]:"local-development",wicked=argc>3?argv[3]:"local-development";
    WNDCLASSEXW cls={};cls.cbSize=sizeof(cls);cls.lpfnWndProc=Proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"RenegadeFunctionalPrefabProof";RegisterClassExW(&cls);
    HWND window=CreateWindowExW(0,cls.lpszClassName,L"Functional prefab proof",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,cls.hInstance,nullptr);
    wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
    struct Drain{~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}} drain;
    wi::scene::Scene seed;
    auto cube=[&](const char* name,XMFLOAT3 pos,XMFLOAT3 scale,XMFLOAT4 colour) {
        auto e=seed.Entity_CreateCube(name);auto* t=seed.transforms.GetComponent(e);t->Scale(scale);t->Translate(pos);t->UpdateTransform();
        seed.materials.GetComponent(e)->SetBaseColor(colour);return e;
    };
    cube("Review floor",{0,-.15f,0},{8,.15f,6},{.25f,.27f,.3f,1});
    auto stand=cube("Brazier stand",{-3,1,0},{.4f,1,.4f},{.09f,.11f,.13f,1});
    seed.materials.GetComponent(stand)->shaderType=wi::scene::MaterialComponent::SHADERTYPE_UNLIT;
    auto bowl=cube("Brazier bowl",{-3,2,0},{.8f,.15f,.8f},{.4f,.25f,.12f,1});
    seed.materials.GetComponent(bowl)->roughness=.38f;seed.materials.GetComponent(bowl)->metalness=.7f;
    Check(CreateEnvironment(seed,WeatherState{},"Environment")!=wi::ecs::INVALID_ENTITY,"create native review environment");
    auto camera=seed.Entity_CreateCamera("Review Camera",1280,720);auto* ct=seed.transforms.GetComponent(camera);
    ct->MatrixTransform(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(8,5,-12,1),XMVectorSet(0,1.3f,0,1),XMVectorSet(0,1,0,0))));ct->UpdateTransform();
    seed.cameras.GetComponent(camera)->TransformCamera(*ct);seed.cameras.GetComponent(camera)->UpdateCamera();
    NativeSave(seed,output/"BrazierSource.wiscene");
    StudioSession session;session.Projects().Initialize((output/"editor-state.json").generic_u8string());
    Check(session.Projects().CreateProject(output.generic_u8string(),"BrazierReview",(output/"BrazierSource.wiscene").generic_u8string()),"create isolated functional review project");
    const auto root=fs::u8path(session.Projects().PendingProject().rootPath),scenePath=root/"Content/Scenes/Main.wiscene";
    Check(session.Projects().CommitPendingProject()&&session.LoadScene(scenePath.generic_u8string())&&session.SaveScene(scenePath.generic_u8string()),"adopt and establish the saved Level and scripting companion");
    auto project=session.Projects().CurrentProject();auto& scene=session.Scenes().GetScene();auto& history=session.Commands();std::string error;
    Check(session.Scripts().EnsureCurrent(error),"prepare native Studio script document");auto* scripts=session.Scripts().Document();Check(scripts!=nullptr,"script document available");
    renegade::studio::InstallCharacterPrefabStudioIntegration();
    stand=Find(scene,"Brazier stand");bowl=Find(scene,"Brazier bowl");
    Check(EnsureLightBehaviourSource(root.generic_u8string(),error),"adopt governed light source: "+error,&error);
    LightPreset light;light.name="Brazier light";light.light=MakeNewLightState(wi::scene::LightComponent::POINT);
    light.light.color={1,.58f,.22f};light.light.intensity=24;light.light.range=7;light.light.castShadow=true;light.behaviour=LightBehaviour::Flame;
    auto makeLight=std::make_unique<LightSetupCommand>(scene,scripts,light,wi::ecs::INVALID_ENTITY,XMFLOAT3{-3,2.4f,0});auto* made=makeLight.get();
    Check(history.Execute(std::move(makeLight)),"create the real flame light and its governed behaviour");const auto lamp=made->Entity();
    std::vector<ParticleTextureEntry> textures;Check(ReadIncludedParticleTextures(InstalledParticleTextureDirectory(),textures,error),"read included texture library: "+error,&error);
    auto texture=[&](const std::string& id,const std::string& search) {
        auto found=std::find_if(textures.begin(),textures.end(),[&](const auto& t){return t.id==id||(!search.empty()&&t.name.find(search)!=std::string::npos);});
        Check(found!=textures.end(),"find actual fixture texture: "+id);auto entry=*found;
        Check(AdoptParticleTexture(root.generic_u8string(),project.projectId,entry,error),"adopt project-owned texture: "+entry.name+" "+error,&error);return entry;
    };
    ParticleEffect effect;effect.name="Brazier fire";
    const std::vector<ParticleTextureEntry> images={texture("supplied-candle",""),texture("supplied-smoke",""),texture("","Ember fleck")};
    for(size_t i=0;i<3;++i) {
        ParticleEffectLayer layer;layer.id=GenerateStableId();layer.name=i==0?"Flame":i==1?"Smoke":"Embers";layer.emitter=MakeNewParticleEmitterState();
        layer.emitter.size=i==0?.9f:i==1?.45f:.045f;layer.emitter.emitCount=i==1?8:12;layer.emitter.life=i==1?2.3f:1.8f;
        layer.emitter.velocity={0,i==1?.7f:1.f,0};layer.emitter.gravity={0,0,0};
        ApplyParticleTextureToLayer(layer,images[i],MaterialTextureSlot::BaseColor);
        layer.material.emissiveStrength=i==0?3.f:0.f;layer.material.baseColor.w=i==1?.22f:1.f;
        if(i==0)layer.playback=ParticlePlayback::StationaryLoop;
        if(i==1){layer.emitter.shaderType=wi::EmittedParticleSystem::SOFT_LIGHTING;layer.lifetime.enabled=true;layer.lifetime.keys={{0,.5f,{1,1,1,0}},{.2f,1,{1,1,1,.45f}},{1,2.5f,{1,1,1,0}}};}
        if(i==2){layer.shape.type=ParticleShapeType::Cone;layer.shape.radius=.3f;layer.shape.angle=12;layer.sizeVariation=.2f;}
        effect.layers.push_back(layer);
    }
    ParticleMovementField field;field.id=GenerateStableId();field.name="Warm updraft";field.type=ParticleFieldType::Updraft;field.strength=.15f;effect.fields.push_back(field);
    auto makeEffect=std::make_unique<ParticleEffectCommand>(scene,root.generic_u8string(),project.projectId,effect,wi::ecs::INVALID_ENTITY,XMFLOAT3{-3,2.4f,0});auto* emitted=makeEffect.get();
    Check(history.Execute(std::move(makeEffect)),"create complete three-layer effect");const auto fire=emitted->Entity();
    const auto expectedEffect=CaptureParticleEffect(scene,fire);Check(expectedEffect.layers.size()==3&&scene.sprites.GetCount()==1,"effect has all emitters and its stationary flame sprite");
    session.Selection().Set({stand,bowl,lamp,fire});auto grouping=std::make_unique<SceneAssemblyCommand>(scene,session.Selection().Entities(),"Brazier");auto* grouped=grouping.get();
    Check(history.Execute(std::move(grouping)),"group meshes, light and complete effect through the normal command");const auto sourceRoot=grouped->Root();
    const auto sourcePivot=scene.transforms.GetComponent(sourceRoot)->translation_local;
    const auto controlPath=root/"Content/Scripts/Prefabs/BrazierController.lua";fs::create_directories(controlPath.parent_path());
    {std::ofstream f(controlPath);f<<R"LUA(if renegade.metadata then
 renegade.metadata({schema_version=1,name="Brazier Controller",category="Prefabs",role="SCRIPT",properties={
 {name="lamp",type="entity"},{name="effect",type="entity"}}})
end
local clock,opacity
return {
 on_start=function(ctx)
  clock=0;assert(renegade.light.get_state(ctx.properties.lamp))
  opacity=assert(renegade.particles.get_layer(ctx.properties.effect,"Smoke")).opacity
 end,
 on_update=function(ctx,dt)
  clock=clock+dt
  local lamp=assert(renegade.light.get_state(ctx.properties.lamp))
  assert(renegade.particles.set_layer(ctx.properties.effect,"Smoke",{opacity=.18+.04*math.sin(clock*2+lamp.seed*8)}))
 end,
 on_stop=function(ctx)
  if renegade.entity.is_valid(ctx.properties.effect) then renegade.particles.set_layer(ctx.properties.effect,"Smoke",{opacity=opacity}) end
 end
}
)LUA";}
    ScriptSourceBinding binding;binding.sourceId=GenerateStableId();binding.sourcePath="Content/Scripts/Prefabs/BrazierController.lua";
    auto controller=CreateScriptAttachment(ScriptScope::Entity,PersistentEntityId(scene,sourceRoot),binding);
    for(auto pair:{std::pair<const char*,Entity>{"lamp",lamp},{"effect",fire}}) {ScriptPropertyValue p;p.name=pair.first;p.type=ScriptPropertyType::EntityReference;p.referenceId=PersistentEntityId(scene,pair.second);controller.properties.push_back(p);}
    Check(AddScriptAttachment(*scripts,controller,error)&&ValidateScriptDocumentAgainstScene(*scripts,scene,error),"configure root behaviour with real light/effect references: "+error,&error);
    auto duplicate=std::make_unique<DuplicateEntityCommand>(scene,sourceRoot);auto* copied=duplicate.get();
    Check(history.Execute(std::move(duplicate))&&scripts->attachments.size()==4,"duplicate copies root and child behaviours atomically");const auto copy=copied->DuplicatedEntity();
    const auto copyLamp=LightIn(scene,copy);auto before=CaptureTransform(*scene.transforms.GetComponent(copyLamp));auto changed=before;changed.translation.y+=.3f;
    Check(history.Execute(std::make_unique<SetTransformCommand>(scene,copyLamp,changed))&&history.Undo()&&Near(scene.transforms.GetComponent(copyLamp)->translation_local.y,before.translation.y),"edit one light component and Undo restores its local transform");
    StableId asset;Check(SaveSceneAssemblyPrefab(root.generic_u8string(),project.projectId,scene,{copy},"Working Brazier",asset,error,scripts),"save complete scripted prefab: "+error,&error);
    auto prepared=ReusableAssetService().PrepareModelAssetPlacement({root.generic_u8string(),project.projectId,asset});Check(prepared.IsReady(),"cold reopen complete prefab: "+prepared.Result().error);
    const auto& payload=*prepared.PeekScene();
    Check(payload.objects.GetCount()==2&&payload.lights.GetCount()==1&&payload.emitters.GetCount()==3&&payload.sprites.GetCount()==1&&HasSceneAssemblyScriptTemplate(payload,payload.transforms.GetEntity(0)),"payload retains geometry, light, emitters, sprite and behaviour templates");
    auto payloadStand=Find(payload,"Brazier stand"),payloadBowl=Find(payload,"Brazier bowl");
    Check(!HasMaterialStateChange(CaptureMaterial(*scene.materials.GetComponent(stand)),CaptureMaterial(*payload.materials.GetComponent(payloadStand)))&&
        !HasMaterialStateChange(CaptureMaterial(*scene.materials.GetComponent(bowl)),CaptureMaterial(*payload.materials.GetComponent(payloadBowl))),"PBR/unlit shaders and authored material settings survive cold prefab load");
    Entity payloadEffect=wi::ecs::INVALID_ENTITY;for(size_t i=0;i<payload.metadatas.GetCount();++i)if(payload.metadatas[i].string_values.has(ParticleEffectMetadataKey))payloadEffect=payload.metadatas.GetEntity(i);
    const auto coldEffect=CaptureParticleEffect(payload,payloadEffect);
    Check(coldEffect.layers.size()==3&&coldEffect.fields.size()==1&&coldEffect.layers[0].playback==ParticlePlayback::StationaryLoop&&coldEffect.layers[1].lifetime.enabled&&
        coldEffect.layers[2].shape.type==ParticleShapeType::Cone&&coldEffect.layers[1].emitter.shaderType==wi::EmittedParticleSystem::SOFT_LIGHTING,"layered particle playback, curves, fields, shapes and shader selection survive");
    for(size_t i=0;i<3;++i)Check(coldEffect.layers[i].texture==expectedEffect.layers[i].texture,"each layer retains its governed texture identity");
    StableId effectOnlyAsset;
    Check(SaveSceneAssemblyPrefab(root.generic_u8string(),project.projectId,scene,{lamp,fire},"Light and Effect Only",effectOnlyAsset,error,scripts),"save light/effect-only prefab without geometry: "+error,&error);
    auto effectOnly=ReusableAssetService().PrepareModelAssetPlacement({root.generic_u8string(),project.projectId,effectOnlyAsset});
    Check(effectOnly.IsReady(),"cold prepare component-only prefab: "+effectOnly.Result().error);
    Check(effectOnly.PeekScene()->objects.GetCount()==0&&effectOnly.PeekScene()->lights.GetCount()==1&&effectOnly.PeekScene()->emitters.GetCount()==3,
        "no artificial mesh is required for reusable lights and effects");
    const auto scriptedBeforeEffectOnly=scripts->attachments.size();
    Check(history.Execute(std::make_unique<PlaceReusableModelCommand>(scene,effectOnly.ReleaseScene(),effectOnlyAsset,XMFLOAT3{20,3,0},1.f,"Light/effect only"))&&
        scripts->attachments.size()==scriptedBeforeEffectOnly+1,"place component-only prefab with its light behaviour");
    Check(history.Undo()&&scripts->attachments.size()==scriptedBeforeEffectOnly,"component-only placement Undo restores the scene and behaviour graph");
    Check(history.Execute(MakeDeleteSceneHierarchyCommand(scene,scripts,copy))&&scripts->attachments.size()==2,"delete copied assembly removes its complete behaviour graph");
    Check(history.Undo()&&scripts->attachments.size()==4&&history.Redo()&&scripts->attachments.size()==2,"assembly delete Undo/Redo preserves graph identity");
    auto placeOne=std::make_unique<PlaceReusableModelCommand>(scene,prepared.ReleaseScene(),asset,XMFLOAT3{-3,sourcePivot.y,0},1.f,"Working Brazier");auto* firstPlaced=placeOne.get();
    Check(history.Execute(std::move(placeOne))&&scripts->attachments.size()==4,"place a complete independent instance through the real Studio hook");const auto instanceOne=firstPlaced->PlacedEntity();
    Check(history.Undo()&&scripts->attachments.size()==2&&history.Redo()&&scripts->attachments.size()==4,"placement Undo/Redo owns all behaviour attachments");
    auto another=ReusableAssetService().PrepareModelAssetPlacement({root.generic_u8string(),project.projectId,asset});
    auto placeTwo=std::make_unique<PlaceReusableModelCommand>(scene,another.ReleaseScene(),asset,XMFLOAT3{3,sourcePivot.y,0},1.f,"Working Brazier");auto* secondPlaced=placeTwo.get();
    Check(history.Execute(std::move(placeTwo))&&scripts->attachments.size()==6,"second placement receives another independent behaviour graph");const auto instanceTwo=secondPlaced->PlacedEntity();
    Check(history.Execute(MakeDeleteSceneHierarchyCommand(scene,scripts,sourceRoot))&&scripts->attachments.size()==4,"remove original donors while both placed instances remain complete");
    const auto instanceId=PersistentEntityId(scene,instanceOne),otherId=PersistentEntityId(scene,instanceTwo);
    const auto secondLight=LightIn(scene,instanceTwo);
    const auto secondLightId=PersistentEntityId(scene,secondLight);
    auto override=CaptureLightPreset(scene,secondLight,scripts);override.light.intensity=31;override.speed=2;
    Check(history.Execute(std::make_unique<LightSetupCommand>(scene,scripts,override,secondLight))&&history.Undo()&&Near(scene.lights.GetComponent(secondLight)->intensity,24)&&history.Redo(),"per-instance light parameter and script-property edits have reliable Undo/Redo");
    Check(Near(scene.lights.GetComponent(LightIn(scene,instanceOne))->intensity,24)&&Near(scene.lights.GetComponent(secondLight)->intensity,31),"editing one instance leaves the other unchanged");
    CheckLua(*scripts,scene,root.generic_u8string());
    Check(session.SaveScene(scenePath.generic_u8string())&&session.LoadScene(scenePath.generic_u8string())&&session.Scripts().EnsureCurrent(error),"save and cold reopen both full instances through StudioSession");scripts=session.Scripts().Document();
    auto& cold=session.Scenes().GetScene();auto coldOne=ById(cold,instanceId);
    Check(coldOne!=wi::ecs::INVALID_ENTITY&&ById(cold,otherId)!=wi::ecs::INVALID_ENTITY&&scripts->attachments.size()==4,"cold scene retains independent instance and script identities");
    CheckLua(*scripts,cold,root.generic_u8string());
    auto unpack=std::make_unique<SceneAssemblyCommand>(cold,coldOne);auto* unpacked=unpack.get();
    Check(session.Commands().Execute(std::move(unpack))&&unpacked->Pieces().size()==5&&scripts->attachments.size()==4,"one Unpack retains meshes, light, effect and named root behaviour object");
    Check(session.SaveScene(scenePath.generic_u8string()),"save unpacked functional assembly");
    {SceneService reopened;Check(reopened.LoadScene(scenePath.generic_u8string()),"cold reopen unpacked native scene");
        ScriptDocument loaded;Check(ReadScriptDocument(ScriptDocumentPathForScene(scenePath.generic_u8string()),project.projectId,scripts->sceneDocumentId,loaded,error),"cold read unpacked behaviour companion: "+error,&error);
        CheckLua(loaded,reopened.GetScene(),root.generic_u8string());}
    Check(session.Commands().Undo()&&IsSceneAssembly(cold,coldOne)&&session.SaveScene(scenePath.generic_u8string()),"Unpack Undo restores the complete selected unit for review");
    project=CompleteFlow(project,*scripts);
    const auto stage=StageFixture(project,binaries,output,revision,wicked);
    const auto stagedScene=std::find_if(stage.files.begin(),stage.files.end(),[](const auto& file){return fs::u8path(file.destinationPath).filename()=="Main.wiscene";});
    Check(stagedScene!=stage.files.end(),"native scene is in the governed package closure");
    SceneService packageScene;Check(packageScene.LoadScene((fs::u8path(stage.stagingPath)/fs::u8path(stagedScene->destinationPath)).generic_u8string()),"cold load staged native scene");
    PackagedReusableAssetRefreshResult refresh;
    Check(RefreshPackagedReusableAssetInstances(packageScene.GetScene(),stage.stagingPath,project.projectId,refresh,error),"validate packaged reusable assets without replacing edited scene copies: "+error,&error);
    Check(refresh.preservedSceneAssemblyCount==2&&refresh.refreshedInstanceCount==0&&ById(packageScene.GetScene(),instanceId)!=wi::ecs::INVALID_ENTITY&&
        ById(packageScene.GetScene(),secondLightId)!=wi::ecs::INVALID_ENTITY&&Near(packageScene.GetScene().lights.GetComponent(ById(packageScene.GetScene(),secondLightId))->intensity,31),"package load preserves instance edits and script owner identities");
    Check(ValidateScriptDocumentAgainstScene(*scripts,packageScene.GetScene(),error),"saved script references still resolve after package refresh: "+error,&error);
    std::ofstream(output/"review-project.txt")<<project.descriptorPath<<'\n';
    std::cout<<"REVIEW PROJECT: "<<project.descriptorPath<<std::endl;
    std::cout<<"PRIVATE PACKAGE: "<<stage.stagingPath<<std::endl;
    std::cout<<"FUNCTIONAL SCENE PREFAB WORKFLOW PASSED"<<std::endl;return 0;
}
