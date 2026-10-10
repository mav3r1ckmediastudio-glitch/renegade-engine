// Explicit local fixture preparation; never runs as a CTest or at application startup.
#include "renegade/bridge/FirstPersonAssemblyService.h"
#include "renegade/bridge/ProjectileAssetService.h"
#include "renegade/bridge/EquipmentAssetService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include <WickedEngine.h>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace renegade::bridge;
namespace fs=std::filesystem;
LRESULT CALLBACK ProjectilePlaygroundWindow(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcW(h,m,w,l);}
int main(int argc,char** argv) {
    if(argc!=2 && argc!=3){std::cerr<<"Usage: ProjectilePlayground <disposable Projectile Playground.renegade>\n";return 2;}
    ProjectMetadata project;std::string error;
    if(!ProjectService().InspectProject(argv[1],project,error)||project.name!="Projectile Playground") {
        std::cerr<<"Refusing to modify a project without the explicit Projectile Playground fixture name: "<<error;return 2;
    }
    const auto fail=[&](const std::string& message){std::cerr<<message<<" / "<<error<<"\n";return 1;};
    const auto mark=[](const char* message){std::cerr<<"[p3] "<<message<<"\n"<<std::flush;};
    mark("project accepted");
    WNDCLASSEXW windowClass={};windowClass.cbSize=sizeof(windowClass);windowClass.lpfnWndProc=ProjectilePlaygroundWindow;
    windowClass.hInstance=GetModuleHandleW(nullptr);windowClass.lpszClassName=L"RenegadeProjectilePlayground";
    RegisterClassExW(&windowClass);
    HWND window=CreateWindowExW(0,windowClass.lpszClassName,L"Projectile Playground",WS_OVERLAPPEDWINDOW,
        0,0,64,64,nullptr,nullptr,windowClass.hInstance,nullptr);
    if(!window)return fail("Create fixture GPU window");
    wi::Application app;app.allow_hdr=false;app.SetWindow(window);
    mark("graphics application created");
    wi::initializer::InitializeComponentsImmediate();
    mark("wicked initialized");
    SceneService scenes;
    const auto scenePath=(fs::u8path(project.rootPath)/"Content/Scenes/ArmsPlayground.wiscene").generic_u8string();
    mark("loading scene");
    if(!scenes.LoadScene(scenePath))return fail("Load fixture scene");
    mark("scene loaded");
    auto& scene=scenes.GetScene();auto start=ResolvePlayerStart(scene);
    if(start.resolution!=PlayerStartResolution::Success)return fail("Resolve Player Start");
    if(argc==3) {
        std::ifstream manifest(fs::u8path(project.rootPath)/"Playground-Assets.txt");
        std::string label,id,selected;const std::string mode=argv[2];
        const std::string wanted=mode=="both"?"Both:":mode=="disappear"?"Disappear:":"Alternate:";
        while(manifest>>label>>id)if(label==wanted){selected=id;break;}
        EquipmentAssetDocument variant;
        if(selected.empty() || !LoadEquipmentAsset(project.rootPath,project.projectId,selected,variant,error))
            return fail("Load saved fixture variant");
        if(mode=="timing" || mode=="visual") {
            variant.equipment.name=mode=="visual"?"Shotgun - Slow flame inspection":"Shotgun - Observable release timing";
            for(auto& binding:variant.equipment.projectiles)binding.releaseSeconds=mode=="visual"?0:.35f;
            if(mode=="visual") {
                ProjectileAssetDocument projectile;
                if(!LoadProjectileAsset(project.rootPath,project.projectId,variant.equipment.projectiles.front().projectileAssetId,projectile,error))
                    return fail("Load visual proof projectile");
                projectile.name="Flaming Arrow - Slow visual proof";projectile.speedMetresPerSecond=3;
                projectile.gravityScale=0;projectile.lifetimeSeconds=10;
                for(auto& layer:projectile.flightEffects){layer.sizeMetres=.18f;layer.particlesPerSecond=120;layer.particleLifeSeconds=.5f;}
                auto savedProjectile=SaveProjectileAsset(project.rootPath,project.projectId,projectile);
                if(!savedProjectile.succeeded){error=savedProjectile.error;return fail("Save visual proof projectile");}
                variant.equipment.projectiles.front().projectileAssetId=savedProjectile.document.assetId;
            }
            auto saved=SaveEquipmentAsset(project.rootPath,project.projectId,variant.equipment);
            if(!saved.succeeded){error=saved.error;return fail("Save timing fixture");}
            selected=saved.document.equipment.assetId;
        } else if(mode!="both" && mode!="disappear" && mode!="alternate")return fail("Unknown fixture mode");
        if(start.start.settings.primaryEquipmentAssetId==selected) {
            wi::graphics::GetDevice()->WaitForGPU();DestroyWindow(window);
            std::cout<<"Saved fixture variant already assigned: "<<mode<<"\n";return 0;
        }
        auto settings=start.start.settings;settings.primaryEquipmentAssetId=selected;
        if(!SetPlayerControllerSettingsCommand(scene,start.start.entity,settings).Execute())return fail("Assign variant");
        wi::Archive archive;scene.Serialize(archive);
        if(!archive.SaveFile(scenePath))return fail("Save variant scene");
        archive.Close();wi::graphics::GetDevice()->WaitForGPU();DestroyWindow(window);
        std::cout<<"Assigned saved fixture variant: "<<mode<<"\n";return 0;
    }
    EquipmentAssetDocument original;
    if(!LoadEquipmentAsset(project.rootPath,project.projectId,start.start.settings.primaryEquipmentAssetId,original,error))
        return fail("Load source equipment");
    FirstPersonAssemblyService assemblies;FirstPersonAssemblySettings assembly;
    if(!assemblies.ReadSettings(project.rootPath,project.projectId,original.equipment.presentationAssetId,assembly,error))
        return fail("Read assembly");
    if(assembly.launchSockets.empty())return fail("Source fixture needs the saved Muzzle");
    auto left=assembly.launchSockets.front();left.name="PSP_Left";left.position.x-=.018f;
    auto right=assembly.launchSockets.front();right.name="PSP_Right";right.position.x+=.018f;
    assembly.launchSockets.push_back(left);assembly.launchSockets.push_back(right);
    AssetRegistry registry;
    if(!ReadAssetRegistry(project.rootPath,project.projectId,registry,error))return fail("Registry");
    std::string expected;
    for(const auto& record:registry.records)
        if(record.assetId==original.equipment.presentationAssetId)expected=record.contentHash;
    mark("updating assembly sockets");
    if(!assemblies.Update(project.rootPath,project.projectId,original.equipment.presentationAssetId,expected,assembly,{},error))
        return fail("Persist both PSPs");
    mark("assembly sockets persisted");
    auto list=ListProjectileAssets(project.rootPath,project.projectId,error);
    auto arrow=std::find_if(list.begin(),list.end(),[](const auto& p){return p.name=="Arrow";});
    if(arrow==list.end())return fail("Find supplied Arrow");
    auto flaming=*arrow;flaming.name="Flaming Arrow - Stick";flaming.speedMetresPerSecond=20;
    flaming.flightEffects={{ProjectileEffectKind::Flame,{0,0,.3f},.1f,90,.3f},
                          {ProjectileEffectKind::Smoke,{0,0,.3f},.1f,90,.3f}};
    flaming.impactEffect=ProjectileEffectKind::Sparks;flaming.stickOnImpact=true;
    flaming.stuckLifetimeSeconds=30;flaming.embedDepthMetres=.05f;
    auto saved=SaveProjectileAsset(project.rootPath,project.projectId,flaming);
    if(!saved.succeeded){error=saved.error;return fail("Save flaming arrow");}
    const auto stick=saved.document.assetId;
    flaming.name="Flaming Arrow - Disappear";flaming.stickOnImpact=false;
    saved=SaveProjectileAsset(project.rootPath,project.projectId,flaming);
    if(!saved.succeeded){error=saved.error;return fail("Save disappearing arrow");}
    const auto disappear=saved.document.assetId;
    auto weapon=original.equipment;weapon.name="Shotgun - Alternating flaming arrows";
    weapon.projectiles={{EquipmentAction::PrimaryUse,stick,"PSP_Left",.10f,1,"PSP_Right"}};
    auto equipment=SaveEquipmentAsset(project.rootPath,project.projectId,weapon);
    if(!equipment.succeeded){error=equipment.error;return fail("Save alternating weapon");}
    const auto alternate=equipment.document.equipment.assetId;
    weapon.name="Shotgun - Both PSPs";
    weapon.projectiles={{EquipmentAction::PrimaryUse,stick,"PSP_Left",.10f,2,"PSP_Right"}};
    equipment=SaveEquipmentAsset(project.rootPath,project.projectId,weapon);
    if(!equipment.succeeded){error=equipment.error;return fail("Save simultaneous weapon");}
    const auto both=equipment.document.equipment.assetId;
    weapon.name="Shotgun - Disappear on impact";
    weapon.projectiles={{EquipmentAction::PrimaryUse,disappear,"PSP_Left",0,0,{}}};
    equipment=SaveEquipmentAsset(project.rootPath,project.projectId,weapon);
    if(!equipment.succeeded){error=equipment.error;return fail("Save disappear weapon");}
    const auto vanish=equipment.document.equipment.assetId;
    auto settings=start.start.settings;settings.primaryEquipmentAssetId=alternate;
    if(!SetPlayerControllerSettingsCommand(scene,start.start.entity,settings).Execute())return fail("Assign default equipment");
    // A large target makes straight-on and ground-contact inspection simple.
    auto target=scene.Entity_CreateCube("Projectile test wall");
    auto& t=*scene.transforms.GetComponent(target);t.Scale(XMFLOAT3{3,2,.2f});t.Translate(XMFLOAT3{0,2,8});t.UpdateTransform();
    const auto mesh=scene.objects.GetComponent(target)->meshID;
    auto* material=scene.materials.GetComponent(scene.meshes.GetComponent(mesh)->subsets[0].materialID);
    if(material){material->baseColor={.3f,.36f,.43f,1};material->SetDirty();}
    if(!AssignPersistentEntityId(scene,target,GenerateStableId(),error))return fail("Target identity");
    wi::Archive archive;scene.Serialize(archive);
    if(!archive.SaveFile(scenePath))return fail("Save fixture scene");
    archive.Close();
    FlowDocument flow;
    const auto flowPath=(fs::u8path(project.rootPath)/"Content/StoryFlow/Main.renegade-flow").generic_u8string();
    if(!ReadFlowDocument(flowPath,project.projectId,flow,error))return fail("Read fixture StoryFlow");
    auto level=std::find_if(flow.nodes.begin(),flow.nodes.end(),[](const auto& n){return n.scenePathHint=="Content/Scenes/ArmsPlayground.wiscene";});
    if(level==flow.nodes.end())return fail("Find target Level");
    auto main=*level;FlowNode begin{flow.startNodeId,FlowNodeKind::GameStart,"Game Start"};
    FlowNode complete{GenerateStableId(),FlowNodeKind::CompleteGame,"Complete Game"};
    flow.nodes={begin,main,complete};
    flow.routes={{GenerateStableId(),begin.id,GameStartOutcome,main.id,"default",0,{}},
                 {GenerateStableId(),main.id,"level.complete",complete.id,{},0,{}}};
    if(!WriteFlowDocument(flowPath,flow,error))return fail("Write fixture route");
    std::ofstream manifest(fs::u8path(project.rootPath)/"Playground-Assets.txt");
    manifest<<"Alternate: "<<alternate<<"\nBoth: "<<both<<"\nDisappear: "<<vanish
        <<"\nStick projectile: "<<stick<<"\nDisappear projectile: "<<disappear<<"\n";
    wi::graphics::GetDevice()->WaitForGPU();
    DestroyWindow(window);
    std::cout<<"Prepared saved two-PSP / timed release / layered effects / both impact modes fixture.\n";
    return 0;
}
