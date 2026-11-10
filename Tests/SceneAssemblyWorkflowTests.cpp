#include "renegade/bridge/CreatorAssetActionPolicy.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/SceneAssemblyService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
using namespace renegade::bridge;
namespace fs=std::filesystem;
using wi::ecs::Entity;
static void Check(bool condition,const std::string& message) {
    if(!condition){std::cerr<<"FAIL: "<<message<<std::endl;std::exit(1);}
    std::cout<<"PASS: "<<message<<std::endl;
}
static bool Near(float a,float b){return std::abs(a-b)<.001f;}
static XMFLOAT3 World(const wi::scene::Scene& s,Entity e) {
    XMFLOAT3 p;XMStoreFloat3(&p,AssemblyWorldMatrix(s,e).r[3]);return p;
}
static Entity Find(const wi::scene::Scene& s,const std::string& name) {
    for(size_t i=0;i<s.names.GetCount();++i)if(s.names[i].name==name)return s.names.GetEntity(i);
    return wi::ecs::INVALID_ENTITY;
}
static void Save(wi::scene::Scene& s,const fs::path& path) {
    wi::Archive archive(path.generic_u8string(),false,false);
    archive.SetCompressionEnabled(true);s.Serialize(archive);Check(archive.SaveFile(path.generic_u8string()),"native scene saved: "+path.filename().u8string());
}
static LRESULT CALLBACK Proc(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcW(w,m,a,b);}
static int Core() {
    wi::scene::Scene scene;SelectionService selection;CommandService history;
    const auto parent=scene.Entity_CreateTransform("Parent");
    const auto a=scene.Entity_CreateTransform("A"),b=scene.Entity_CreateTransform("B");
    auto* pt=scene.transforms.GetComponent(parent);
    pt->Scale(XMFLOAT3(2,2,2));pt->RotateRollPitchYaw(XMFLOAT3(0,.5f,0));pt->Translate(XMFLOAT3(5,2,-3));pt->UpdateTransform();
    scene.transforms.GetComponent(a)->Translate(XMFLOAT3(1,1,0));scene.transforms.GetComponent(a)->UpdateTransform();
    scene.Component_Attach(a,parent,true);
    scene.transforms.GetComponent(b)->Translate(XMFLOAT3(-2,1,0));scene.transforms.GetComponent(b)->UpdateTransform();
    const auto wa=World(scene,a),wb=World(scene,b);
    selection.Set({a,b,a,wi::ecs::INVALID_ENTITY});Check(selection.Entities().size()==2,"selection deduplicates inputs");
    const auto revision=selection.Revision();selection.Set({a,b});Check(selection.Revision()==revision,"unchanged selection does not invalidate caches");
    auto group=std::make_unique<SceneAssemblyCommand>(scene,selection.Entities(),"Nested parent proof");auto* result=group.get();
    Check(history.Execute(std::move(group)),"group across transformed parents");
    const auto root=result->Root();const auto id=PersistentEntityId(scene,root);
    Check(Near(World(scene,a).x,wa.x)&&Near(World(scene,a).z,wa.z)&&Near(World(scene,b).x,wb.x),"group keeps transformed world positions");
    Check(history.Undo()&&scene.hierarchy.GetComponent(a)->parentID==parent&&Near(World(scene,a).x,wa.x),"Undo restores original parent and local transform");
    Check(history.Redo()&&PersistentEntityId(scene,root)==id,"Redo keeps stable group identity");
    struct Refuse final:ICommand{bool Execute()override{return false;}void Undo()override{}};
    const auto before=CaptureTransform(*scene.transforms.GetComponent(root));
    auto after=before;after.translation.x+=4;
    auto batch=std::make_unique<SelectionBatchCommand>();
    batch->Add(std::make_unique<SetTransformCommand>(scene,root,after));batch->Add(std::make_unique<Refuse>());
    const auto undo=history.UndoCount();
    Check(!history.Execute(std::move(batch))&&history.UndoCount()==undo&&
        Near(scene.transforms.GetComponent(root)->translation_local.x,before.translation.x),"batch rolls back fully when a member refuses");
    const auto child=scene.Entity_CreateTransform("Grandchild");scene.Component_Attach(child,a,true);
    Check(SelectionRoots(scene,{root,child}).size()==1,"non-adjacent selected ancestor prevents double transforms");
    auto unpack=std::make_unique<SceneAssemblyCommand>(scene,root);
    Check(history.Execute(std::move(unpack))&&!scene.transforms.Contains(root)&&Near(World(scene,a).x,wa.x),"unpack preserves transformed pieces");
    Check(history.Undo()&&IsSceneAssembly(scene,root)&&scene.hierarchy.GetComponent(a)->parentID==root,"unpack Undo restores hierarchy");
    selection.Set({root,a});scene.Entity_Remove(root);selection.Prune(scene);Check(!selection.HasSelection(),"deleted selection prunes stale handles");
    wi::scene::Scene multi;CommandService multiHistory;
    auto one=multi.Entity_CreateTransform("One"),two=multi.Entity_CreateTransform("Two");
    auto duplicates=std::make_unique<SelectionBatchCommand>();
    auto first=std::make_unique<DuplicateEntityCommand>(multi,one);auto* firstResult=first.get();
    auto second=std::make_unique<DuplicateEntityCommand>(multi,two);auto* secondResult=second.get();
    duplicates->Add(std::move(first));duplicates->Add(std::move(second));
    Check(multiHistory.Execute(std::move(duplicates))&&multi.transforms.GetCount()==4,"multi duplicate creates exactly one copy of each root");
    const auto firstId=firstResult->DuplicatedEntity(),secondId=secondResult->DuplicatedEntity();
    const auto firstStable=PersistentEntityId(multi,firstId);
    Check(multiHistory.Undo()&&multi.transforms.GetCount()==2,"multi duplicate Undo removes both copies");
    Check(multiHistory.Redo()&&multi.transforms.GetCount()==4&&PersistentEntityId(multi,firstId)==firstStable,"multi duplicate Redo retains both identities");
    auto deletes=std::make_unique<SelectionBatchCommand>();
    deletes->Add(std::make_unique<DeleteEntityCommand>(multi,firstId));
    deletes->Add(std::make_unique<DeleteEntityCommand>(multi,secondId));
    Check(multiHistory.Execute(std::move(deletes))&&multi.transforms.GetCount()==2,"multi delete removes both roots");
    Check(multiHistory.Undo()&&multi.transforms.GetCount()==4&&PersistentEntityId(multi,firstId)==firstStable,"multi delete Undo restores both roots and identities");
    SelectionService authoring;authoring.Set({one,two});CommandService selectionHistory;
    auto selectedGroup=std::make_unique<SceneAssemblyCommand>(multi,authoring.Entities(),"Selected Group");
    auto* selectedGroupResult=selectedGroup.get();
    Check(selectionHistory.Execute(std::make_unique<SelectionHistoryCommand>(authoring,std::move(selectedGroup),
        [selectedGroupResult]{return std::vector<Entity>{selectedGroupResult->Root()};}))&&
        authoring.SelectedEntity()==selectedGroupResult->Root(),"group command selects the created unit");
    Check(selectionHistory.Undo()&&authoring.Entities()==std::vector<Entity>({one,two}),"group Undo reselects the original pieces");
    Check(selectionHistory.Redo()&&authoring.SelectedEntity()==selectedGroupResult->Root(),"group Redo reselects the group");
    const auto selectedRoot=authoring.SelectedEntity();
    auto selectedDelete=std::make_unique<DeleteEntityCommand>(multi,selectedRoot);
    Check(selectionHistory.Execute(std::make_unique<SelectionHistoryCommand>(authoring,std::move(selectedDelete),
        []{return std::vector<Entity>{};}))&&!authoring.HasSelection(),"delete clears removed selection");
    Check(selectionHistory.Undo()&&authoring.SelectedEntity()==selectedRoot&&multi.transforms.Contains(selectedRoot),"delete Undo restores and reselects the group");
    std::cout<<"CORE ASSEMBLY CHECKS PASSED"<<std::endl;return 0;
}
int main(int argc,char** argv) {
    if(argc>1&&std::string(argv[1])=="--core")return Core();
    const fs::path output=argc>1?fs::u8path(argv[1]):fs::temp_directory_path()/("renegade-assembly-"+GenerateStableId());
    fs::create_directories(output);
    WNDCLASSEXW cls={};cls.cbSize=sizeof(cls);cls.lpfnWndProc=Proc;
    cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"RenegadeAssemblyWorkflowTests";RegisterClassExW(&cls);
    HWND window=CreateWindowExW(0,cls.lpszClassName,L"Assembly workflow test",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,cls.hInstance,nullptr);
    wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
    struct Drain{~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}} drain;
    wi::scene::Scene scene;
    auto piece=[&](const char* name,XMFLOAT3 position,XMFLOAT3 scale,XMFLOAT4 color) {
        auto e=scene.Entity_CreateCube(name);auto* t=scene.transforms.GetComponent(e);
        t->Scale(scale);t->Translate(position);t->UpdateTransform();
        auto* m=scene.materials.GetComponent(e);if(m){m->SetBaseColor(color);m->SetRoughness(.85f);}
        return e;
    };
    const auto left=piece("Left Pier",{-2,1.5f,0},{.5f,1.5f,.5f},{.5f,.43f,.32f,1});
    const auto right=piece("Right Pier",{2,1.5f,0},{.5f,1.5f,.5f},{.4f,.45f,.5f,1});
    const auto lintel=piece("Lintel",{0,3.25f,0},{2.5f,.3f,.5f},{.55f,.45f,.3f,1});
    const auto originalLeft=World(scene,left);
    SelectionService selection;selection.Select(left);selection.Toggle(right);selection.Toggle(lintel);
    Check(selection.Entities().size()==3&&selection.SelectedEntity()==lintel,"Shift-toggle selects three pieces");
    selection.Toggle(right);Check(!selection.Contains(right),"Shift-toggle removes one piece");selection.Toggle(right);
    CommandService history;
    auto group=std::make_unique<SceneAssemblyCommand>(scene,selection.Entities(),"Stone Gateway");
    auto* groupResult=group.get();Check(history.Execute(std::move(group)),"group selected pieces");
    const auto root=groupResult->Root();const auto persistent=PersistentEntityId(scene,root);
    Check(IsSceneAssembly(scene,root)&&Near(World(scene,left).x,originalLeft.x),"group preserves world transforms");
    Check(SelectionRoots(scene,{root,left,right}).size()==1,"parent plus child selection transforms once");
    Check(ResolveAssemblySelection(scene,left)==root&&ResolveAssemblySelection(scene,left,root)==left,"whole group versus Edit Pieces selection");
    Check(history.Undo()&&!scene.transforms.Contains(root)&&!scene.hierarchy.Contains(left),"group Undo restores independent objects");
    Check(history.Redo()&&PersistentEntityId(scene,root)==persistent,"group Redo preserves identity");
    auto duplicate=std::make_unique<DuplicateEntityCommand>(scene,root);auto* duplicateResult=duplicate.get();
    Check(history.Execute(std::move(duplicate)),"duplicate complete assembly");
    const auto copy=duplicateResult->DuplicatedEntity();Check(copy!=root&&IsSceneAssembly(scene,copy),"duplicate is separate named assembly");
    auto before=CaptureTransform(*scene.transforms.GetComponent(copy));auto moved=before;moved.translation.x+=8;
    Check(history.Execute(std::make_unique<SetTransformCommand>(scene,copy,moved)),"move duplicate as one unit");
    std::vector<Entity> copyChildren;for(size_t i=0;i<scene.hierarchy.GetCount();++i)
        if(scene.hierarchy[i].parentID==copy)copyChildren.push_back(scene.hierarchy.GetEntity(i));
    Check(copyChildren.size()==3,"duplicate retains three independently editable pieces");
    auto* component=scene.transforms.GetComponent(copyChildren.front());auto componentBefore=CaptureTransform(*component);
    auto componentAfter=componentBefore;componentAfter.translation.y+=1;
    Check(history.Execute(std::make_unique<SetTransformCommand>(scene,copyChildren.front(),componentAfter)),"edit one component");
    Check(Near(scene.transforms.GetComponent(left)->translation_local.x,originalLeft.x-scene.transforms.GetComponent(root)->translation_local.x),"original assembly stays independent");
    Check(history.Undo()&&Near(scene.transforms.GetComponent(copyChildren.front())->translation_local.y,componentBefore.translation.y),"Undo restores edited component");
    // Prove transform batch history, including a non-adjacent ancestor selection.
    auto batch=std::make_unique<SelectionBatchCommand>();
    for(auto e:std::vector<Entity>{root,copy}){auto next=CaptureTransform(*scene.transforms.GetComponent(e));next.translation.z+=2;
        batch->Add(std::make_unique<SetTransformCommand>(scene,e,next));}
    const auto depth=history.UndoCount();Check(history.Execute(std::move(batch))&&history.UndoCount()==depth+1,"multi move records one Undo");
    Check(history.Undo()&&Near(World(scene,root).z,0)&&Near(World(scene,copy).z,0),"multi move Undo restores both");
    const auto project=GenerateStableId();const auto projectRoot=output/"AssemblyProof";
    fs::create_directories(projectRoot);
    AssetRegistry registry;registry.projectId=project;
    Check(WriteAssetRegistry(projectRoot.generic_u8string(),registry).success,"project registry created");
    StableId asset;std::string error;
    Check(SaveSceneAssemblyPrefab(projectRoot.generic_u8string(),project,scene,{root},"Stone Gateway",asset,error),"save prefab: "+error);
    AssetCatalogue catalogue;
    Check(CreatorAssetWorkflowService().BuildCatalogueSnapshot(projectRoot.generic_u8string(),project,catalogue,error),"browser catalogue includes saved prefab: "+error);
    auto card=std::find_if(catalogue.entries.begin(),catalogue.entries.end(),[&](const auto& e){return e.assetId==asset;});
    Check(card!=catalogue.entries.end()&&CanPlaceCreatorModelAsset(*card),"ordinary Asset Browser allows placing the saved scene prefab");
    auto blocked=*card;blocked.state=AssetCatalogueState::Invalid;
    Check(!CanPlaceCreatorScenePrefabAsset(blocked),"invalid prefab remains blocked by placement policy");
    blocked=*card;blocked.importer="untrusted.other";
    Check(!CanPlaceCreatorScenePrefabAsset(blocked),"unrelated product is not admitted as a scene prefab");
    StableId clash;
    Check(!SaveSceneAssemblyPrefab(projectRoot.generic_u8string(),project,scene,{root},"Stone Gateway",clash,error),"existing prefab never overwritten silently");
    auto prepared=ReusableAssetService().PrepareModelAssetPlacement({projectRoot.generic_u8string(),project,asset});
    Check(prepared.IsReady(),"cold prefab preparation: "+prepared.Result().error);
    Check(prepared.PeekScene()->objects.GetCount()==3&&prepared.PeekScene()->materials.GetCount()==3,
        "prefab contains only selected geometry and its materials");
    bool stone=false,blue=false,brown=false;
    for(size_t i=0;i<prepared.PeekScene()->materials.GetCount();++i) {
        const auto c=prepared.PeekScene()->materials[i].baseColor;
        stone=stone||(Near(c.x,.5f)&&Near(c.y,.43f)&&Near(c.z,.32f));
        blue=blue||(Near(c.x,.4f)&&Near(c.y,.45f)&&Near(c.z,.5f));
        brown=brown||(Near(c.x,.55f)&&Near(c.y,.45f)&&Near(c.z,.3f));
    }
    Check(stone&&blue&&brown,"prefab preserves all three authored material colours");
    // A referenced mesh/material can share an entity with an unselected render
    // object. Saving instances must copy its resources without adding the donor.
    wi::scene::Scene shared;
    const auto donor=shared.Entity_CreateCube("Unselected Mesh Donor");
    shared.materials.GetComponent(donor)->SetBaseColor({.6f,.2f,.7f,1});
    auto sharedA=shared.Entity_CreateObject("Shared Piece A"),sharedB=shared.Entity_CreateObject("Shared Piece B");
    shared.objects.GetComponent(sharedA)->meshID=donor;shared.objects.GetComponent(sharedB)->meshID=donor;
    StableId sharedAsset;
    Check(SaveSceneAssemblyPrefab(projectRoot.generic_u8string(),project,shared,{sharedA,sharedB},"Shared Resource Proof",sharedAsset,error),
        "save instances using an external shared mesh/material: "+error);
    auto sharedPrepared=ReusableAssetService().PrepareModelAssetPlacement({projectRoot.generic_u8string(),project,sharedAsset});
    Check(sharedPrepared.IsReady()&&sharedPrepared.PeekScene()->objects.GetCount()==2&&
        sharedPrepared.PeekScene()->meshes.GetCount()==1&&sharedPrepared.PeekScene()->materials.GetCount()==1,
        "shared resource closure excludes the unselected donor object");
    Check(Near(sharedPrepared.PeekScene()->materials[0].baseColor.x,.6f),"shared donor material retained");
    const auto beforeObjects=scene.objects.GetCount();
    auto place=std::make_unique<PlaceReusableModelCommand>(scene,prepared.ReleaseScene(),asset,XMFLOAT3(16,0,0),1.f,"Stone Gateway Prefab");
    auto* placeResult=place.get();Check(history.Execute(std::move(place)),"place another prefab instance through ordinary placement command");
    const auto instance=placeResult->PlacedEntity();Check(scene.objects.GetCount()==beforeObjects+3&&IsSceneAssembly(scene,instance),"placed prefab has exactly its three visible pieces");
    Check(history.Undo()&&scene.objects.GetCount()==beforeObjects,"placement Undo removes whole instance");
    Check(history.Redo()&&scene.objects.GetCount()==beforeObjects+3,"placement Redo restores whole instance");
    const auto instanceId=PersistentEntityId(scene,instance);
    Save(scene,output/"AssemblyRoundtrip.wiscene");
    auto reopened=PrepareWickedSceneOpen((output/"AssemblyRoundtrip.wiscene").generic_u8string());
    Check(reopened.IsReady(),"cold scene reopen");
    auto& cold=*reopened.MutablePreparedScene();
    Entity coldInstance=wi::ecs::INVALID_ENTITY;
    for(size_t i=0;i<cold.transforms.GetCount();++i)if(PersistentEntityId(cold,cold.transforms.GetEntity(i))==instanceId)coldInstance=cold.transforms.GetEntity(i);
    Check(coldInstance!=wi::ecs::INVALID_ENTITY&&IsSceneAssembly(cold,coldInstance),"cold reopen preserves prefab instance identity");
    wi::scene::Scene dragScene;
    auto dragPrepared=ReusableAssetService().PrepareModelAssetPlacement({projectRoot.generic_u8string(),project,asset});
    Check(dragPrepared.IsReady(),"prepare independent native drag template");
    auto nativePayload=dragScene.Instantiate(*dragPrepared.PeekMutableScene(),true);
    auto dragWrapper=dragScene.Entity_CreateTransform("Drag wrapper");
    dragScene.Component_Attach(nativePayload,dragWrapper,true);
    PlaceReusableModelCommand adopt(dragScene,asset,dragWrapper,nativePayload,0,"Drag review");
    Check(adopt.Execute()&&IsSceneAssembly(dragScene,dragWrapper),"native drag adoption exposes editable assembly");
    SceneAssemblyCommand unpackDrag(dragScene,dragWrapper);
    Check(unpackDrag.Execute()&&unpackDrag.Pieces().size()==3&&dragScene.objects.GetCount()==3,
        "native drag adoption unpacks exactly three components without geometry loss");
    unpackDrag.Undo();
    Check(IsSceneAssembly(dragScene,dragWrapper)&&dragScene.objects.GetCount()==3,"native drag Unpack Undo restores assembly");
    Check(unpackDrag.Execute()&&dragScene.objects.GetCount()==3,"native drag Unpack Redo preserves geometry");
    CommandService coldHistory;auto unpack=std::make_unique<SceneAssemblyCommand>(cold,coldInstance);auto* unpackResult=unpack.get();
    const auto count=cold.objects.GetCount();Check(coldHistory.Execute(std::move(unpack)),"unpack cold prefab instance");
    Check(unpackResult->Pieces().size()==3&&!cold.transforms.Contains(coldInstance)&&cold.objects.GetCount()==count,"unpack exposes three pieces with no geometry loss");
    for(auto e:unpackResult->Pieces())Check(!cold.hierarchy.Contains(e),"unpacked piece is independently selectable");
    Check(coldHistory.Undo()&&IsSceneAssembly(cold,coldInstance),"unpack Undo restores prefab");
    Check(coldHistory.Redo()&&cold.objects.GetCount()==count,"unpack Redo preserves all geometry");
    Save(cold,output/"AssemblyUnpacked.wiscene");
    // Leave an ordinary, visible owner-review project with individual source pieces.
    wi::scene::Scene fixture;
    for(auto x:std::vector<float>{-2,2}) {
        auto e=fixture.Entity_CreateCube(x<0?"Left Pier":"Right Pier");auto* t=fixture.transforms.GetComponent(e);
        t->Scale(XMFLOAT3(.5f,1.5f,.5f));t->Translate(XMFLOAT3(x,1.5f,0));t->UpdateTransform();
    }
    auto beam=fixture.Entity_CreateCube("Lintel");fixture.transforms.GetComponent(beam)->Scale(XMFLOAT3(2.5f,.3f,.5f));
    fixture.transforms.GetComponent(beam)->Translate(XMFLOAT3(0,3.25f,0));fixture.transforms.GetComponent(beam)->UpdateTransform();
    CreateEnvironment(fixture,WeatherState{},"Environment");
    auto cam=fixture.Entity_CreateCamera("Review Camera",1280,720);
    auto* ct=fixture.transforms.GetComponent(cam);ct->MatrixTransform(XMMatrixInverse(nullptr,
        XMMatrixLookAtLH(XMVectorSet(8,6,-14,1),XMVectorSet(0,1.5f,0,1),XMVectorSet(0,1,0,0))));ct->UpdateTransform();
    fixture.cameras.GetComponent(cam)->TransformCamera(*ct);fixture.cameras.GetComponent(cam)->UpdateCamera();
    Save(fixture,output/"AssemblySource.wiscene");
    ProjectService projects;projects.Initialize((output/"review-state.json").generic_u8string());
    if(!fs::exists(output/"AssemblyReview"/"AssemblyReview.renegade")) {
        Check(projects.CreateProject(output.generic_u8string(),"AssemblyReview",(output/"AssemblySource.wiscene").generic_u8string()),"create native review project: "+projects.LastError());
        std::cout<<"REVIEW PROJECT: "<<projects.CurrentProject().descriptorPath<<std::endl;
    }
    std::ofstream(output/"workflow-passed.txt")<<"select -> group -> duplicate -> edit component -> undo -> save prefab -> place -> save/reopen -> unpack: PASS\n";
    std::cout<<"ASSEMBLY WORKFLOW PASSED"<<std::endl;
    return 0;
}
