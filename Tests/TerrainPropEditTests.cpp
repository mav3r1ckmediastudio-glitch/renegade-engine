#include "renegade/bridge/TerrainPropEditService.h"
#include "renegade/bridge/ModelImportCommitService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"
#include <Windows.h>
#include <iostream>
#include <set>
#include <stdexcept>

namespace
{
    using namespace renegade::bridge;
    namespace fs=std::filesystem;
    void Check(bool condition,const std::string& text)
    { if(!condition) throw std::runtime_error(text); }
    std::set<wi::ecs::Entity> Objects(const wi::scene::Scene& scene)
    {
        std::set<wi::ecs::Entity> result;
        for(size_t i=0;i<scene.objects.GetCount();++i) result.insert(scene.objects.GetEntity(i));
        return result;
    }
    void Run(const fs::path& run,const fs::path& fixture)
    {
        fs::create_directories(run);
        SceneService scenes;
        auto& scene=scenes.GetScene();
        scene.Entity_CreateTransform("Authored witness");
        auto starter=run/"starter.wiscene";
        {wi::Archive archive;scene.Serialize(archive);Check(archive.SaveFile(starter.string()),"starter");}
        ProjectService projects;
        projects.Initialize((run/"recent.ini").string());
        Check(projects.CreateProject(run.string(),"EditProof",starter.string()),projects.LastError());
        const auto project=projects.CurrentProject();
        CommandService commands;
        SelectionService selection;
        SceneDocumentService documents(scenes,selection,commands,projects);
        const auto owner=scene.Entity_CreateTransform("Terrain layer");
        auto edit=std::make_shared<TerrainPropEditService>(scene,fs::u8path(project.rootPath),project.projectId,owner);
        TerrainPropDocument doc;
        std::string error;
        Check(edit->Read(doc,error),error);
        auto candidate=ModelImportCandidateService().PrepareModel(fixture.string());
        Check(candidate.IsReady(),"fixture preparation");
        ModelImportCommitRequest request;
        request.projectRoot=project.rootPath;request.projectId=project.projectId;request.assetName="Tree";
        auto imported=ModelImportCommitService().CommitStaticModel(request,candidate);
        Check(imported.succeeded,imported.error);
        TerrainPropProfile profile;
        profile.id[0]=1;profile.modelAssetId=imported.assetId;
        doc.profiles.push_back(profile);
        TerrainPropPage a,b;
        a.profiles.push_back(profile.id);b.profiles=a.profiles;b.cellX=1;
        TerrainPropPlacement p;
        p.id=doc.AllocatePlacementId();p.position={1,0,1};a.placements.push_back(p);
        p.id=doc.AllocatePlacementId();p.position={33,0,1};b.placements.push_back(p);
        doc.pages={a,b};
        Check(commands.Execute(edit->MakeCommand(doc,"")),edit->LastError());
        Check(commands.UndoCount()==1 && scene.objects.GetCount()==4,"single Apply history/projection");
        auto stable=Objects(scene);
        auto revision=edit->Revision();
        std::string originalHash,counterHash,changedHash;
        Check(ComputeTerrainPropDocumentContentDigest(doc,originalHash,error),error);
        auto counterOnly=doc;counterOnly.nextPlacementId+=100;
        Check(ComputeTerrainPropDocumentContentDigest(counterOnly,counterHash,error) &&
            counterHash==originalHash,"content token included ID high-water");
        auto changed=doc;
        changed.pages[0].placements[0].position[1]=2;
        changed.pages[0].placements[0].scale=1.5f;
        p.id=changed.AllocatePlacementId();p.position={34,0,2};
        changed.pages[1].placements.push_back(p);
        Check(ComputeTerrainPropDocumentContentDigest(changed,changedHash,error) &&
            changedHash!=originalHash,"content token ignored changed page");
        auto recipeOnly=doc;recipeOnly.authoring.recipe.seed+=1;
        Check(ComputeTerrainPropDocumentContentDigest(recipeOnly,changedHash,error) &&
            changedHash!=originalHash,"content token ignored changed recipe");
        ProjectDocumentTransactionOptions options;
        options.operationHook=[](auto stage,size_t,const std::string&,std::string& message)
        {
            if(stage==ProjectDocumentTransactionStage::Replace)
            {message="injected P2 failure";return ProjectDocumentTransactionHookAction::Fail;}
            return ProjectDocumentTransactionHookAction::Continue;
        };
        edit->SetTransactionOptions(options);
        Check(!commands.Execute(edit->MakeCommand(changed,revision)),"failed publish accepted");
        Check(edit->Revision()==revision && Objects(scene)==stable && commands.UndoCount()==1,
            "failed Execute changed display/revision/history");
        edit->SetTransactionOptions({});
        int polls=0;
        Check(!commands.Execute(edit->MakeCommand(changed,revision,[&]{return ++polls>=2;})),
            "late cancellation accepted");
        Check(Objects(scene)==stable && edit->Revision()==revision && commands.UndoCount()==1,
            "cancelled staging leaked");
        Check(commands.Execute(edit->MakeCommand(changed,revision)),edit->LastError());
        Check(scene.objects.GetCount()==6 && commands.UndoCount()==2,"adjacent-cell delta");
        auto changedObjects=Objects(scene);
        auto changedRevision=edit->Revision();
        edit->SetTransactionOptions(options);
        Check(!commands.Undo(),"failed Undo accepted");
        Check(commands.UndoCount()==2 && commands.RedoCount()==0 && Objects(scene)==changedObjects &&
            edit->Revision()==changedRevision,"failed Undo lost display/history");
        edit->SetTransactionOptions({});
        Check(commands.Undo(),edit->LastError());
        TerrainPropDocument undone;
        Check(edit->Read(undone,error),error);
        Check(undone.nextPlacementId==4 && undone.pages[0].placements[0].id==1 &&
            undone.pages[0].placements[0].scale==1,"Undo high-water/stable identity");
        edit->SetTransactionOptions(options);
        auto undoObjects=Objects(scene);
        Check(!commands.Redo(),"failed Redo accepted");
        Check(commands.UndoCount()==1 && commands.RedoCount()==1 && Objects(scene)==undoObjects,
            "failed Redo changed history/display");
        edit->SetTransactionOptions({});
        Check(commands.Redo(),edit->LastError());
        // Edit one cell only. Adjacent cell's exact native object identities survive.
        TerrainPropDocument current;
        Check(edit->Read(current,error),error);
        std::set<wi::ecs::Entity> adjacent;
        for(size_t i=0;i<scene.objects.GetCount();++i)
        {
            auto e=scene.objects.GetEntity(i);
            auto* t=scene.transforms.GetComponent(e);
            if(t && t->translation_local.x>30) adjacent.insert(e);
        }
        current.pages[0].placements[0].position[2]=3;
        auto stale=edit->Revision();
        Check(commands.Execute(edit->MakeCommand(current,stale)),edit->LastError());
        for(auto e:adjacent) Check(scene.objects.Contains(e),"unchanged adjacent cell was rebuilt");
        Check(!commands.Execute(edit->MakeCommand(changed,stale)),"stale revision overwrote authoring");
        const auto first=fs::u8path(project.rootPath)/"Content/Scenes/First.wiscene";
        const auto second=fs::u8path(project.rootPath)/"Content/Scenes/Second.wiscene";
        Check(documents.Save(first.string()),scenes.LastError());
        Check(documents.Save(second.string()),scenes.LastError());
        Check(commands.Undo() && commands.Undo() && commands.Undo(),
            "multi-command Undo chain with monotonic high-water");
        Check(edit->Read(current,error),error);
        Check(current.pages.empty() && current.nextPlacementId==4,"oldest Undo reused IDs");
        Check(commands.Redo() && commands.Redo() && commands.Redo(),
            "multi-command Redo chain with monotonic high-water");
        Check(commands.Undo(),"undo before branched edit");
        Check(edit->Read(current,error),error);
        TerrainPropPlacement fresh;
        fresh.id=current.AllocatePlacementId();fresh.position={2,0,4};
        current.pages[0].placements.push_back(fresh);
        Check(fresh.id==4 && commands.Execute(edit->MakeCommand(current,edit->Revision())),
            "branched authoring reused a deleted placement ID");
        Check(commands.RedoCount()==0,"branched edit retained redo");
        Check(documents.Save(second.string()),scenes.LastError());
        const auto finalRevision=edit->Revision();
        // Release retained projection before replacing its scene/document.
        commands.Clear();edit.reset();
        Check(documents.Open(second.string()),scenes.LastError());
        const auto reopenedOwner=scene.Entity_FindByName("Terrain layer");
        auto reopened=std::make_shared<TerrainPropEditService>(scene,fs::u8path(project.rootPath),
            project.projectId,reopenedOwner);
        Check(reopened->Read(current,error),error);
        Check(current.nextPlacementId==5 && reopened->Revision()==finalRevision &&
            scene.objects.GetCount()==8,"save/reopen lost high-water or projection");
        SceneService runtime;
        Check(runtime.LoadScene(second.string()),runtime.LastError());
        Check(runtime.GetScene().objects.GetCount()==8,"separate Runtime loader projection");
        Check(documents.Open(first.string()),scenes.LastError());
        Check(scene.objects.GetCount()==6,"Save As source lost P2 placements");
        Check(documents.Open(second.string()),scenes.LastError());
        Check(scene.objects.GetCount()==8,"Save As reopen lost P2 placements");
        const auto lifecycleOwner=scene.Entity_FindByName("Terrain layer");
        auto lifecycle=std::make_shared<TerrainPropEditService>(scene,fs::u8path(project.rootPath),
            project.projectId,lifecycleOwner);
        Check(lifecycle->Read(current,error) && lifecycle->Reconcile(error),error);
        const auto fallback=scene.Entity_CreateTransform("Fallback terrain");
        wi::ecs::Entity resolved=wi::ecs::INVALID_ENTITY;
        Check(ResolveTerrainPropAuthoringOwner(scene,project.projectId,fallback,resolved,error) &&
            resolved==lifecycleOwner,"P1 separate authored owner was not resolved");
        const auto duplicate=scene.Entity_CreateTransform("Ambiguous layer");
        auto& duplicateMetadata=scene.metadatas.Create(duplicate);
        duplicateMetadata.string_values.set("renegade.terrain_props.project",project.projectId);
        duplicateMetadata.string_values.set("renegade.terrain_props.revision",lifecycle->Revision());
        Check(!ResolveTerrainPropAuthoringOwner(scene,project.projectId,fallback,resolved,error),
            "ambiguous layer selection silently chose one");
        scene.Entity_Remove(duplicate,false);
        const auto beforeDeleteRevision=lifecycle->Revision();
        const auto beforeDeleteLayer=current.layerId;
        Check(commands.Execute(std::make_unique<DeleteEntityCommand>(scene,lifecycleOwner)),
            "delete authored layer");
        Check(lifecycle->Reconcile(error),error);
        Check(scene.objects.GetCount()==0 && scene.meshes.GetCount()==0 &&
            scene.materials.GetCount()==0 && scene.rigidbodies.GetCount()==0,
            "deleted layer left projected objects/resources/physics");
        Check(!lifecycle->Read(current,error),"deleted owner accepted authoring");
        Check(commands.Undo(),"undo authored layer deletion");
        Check(lifecycle->Reconcile(error),error);
        Check(lifecycle->Read(current,error) && current.layerId==beforeDeleteLayer &&
            lifecycle->Revision()==beforeDeleteRevision && current.nextPlacementId==5 &&
            scene.objects.GetCount()==8,"Undo owner did not restore authoritative layer");
        const auto validObjects=Objects(scene);
        scene.metadatas.GetComponent(lifecycleOwner)->string_values.set(
            "renegade.terrain_props.revision",std::string(64,'0'));
        Check(!lifecycle->Reconcile(error) && Objects(scene)==validObjects,
            "invalid authoritative revision replaced live display");
        scene.metadatas.GetComponent(lifecycleOwner)->string_values.set(
            "renegade.terrain_props.revision",beforeDeleteRevision);
        Check(lifecycle->Reconcile(error),error);
        std::cout<<"PASS P2 native changed-cell projection, cancellation, failed Execute/Undo/Redo, "
            "stable IDs, high-water, stale revision, Save As/reopen\n";
    }
}
int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    const HWND window=CreateWindowExW(0,L"STATIC",L"Terrain prop authoring test",
        WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return 3;
    wi::Application application;application.allow_hdr=false;application.SetWindow(window);
    if(!wi::graphics::GetDevice())return 4;
    wi::initializer::InitializeComponentsImmediate();
    struct Drain
    {
        ~Drain()
        {
            while(wi::renderer::IsPipelineCreationActive())Sleep(5);
            wi::jobsystem::ShutDown();
            wi::graphics::GetDevice()->WaitForGPU();
        }
    } drain;
    try {Run(fs::u8path(argv[1])/GenerateStableId(),fs::u8path(argv[2]));return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
}

