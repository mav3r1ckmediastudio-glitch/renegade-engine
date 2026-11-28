#pragma once
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/ZoneService.h"
inline int MakeHurtReview(const std::string& destination,const std::string& art)
{
    using namespace renegade::bridge;namespace fs=std::filesystem;
    const auto parent=fs::u8path(destination);if(fs::exists(parent)){std::cerr<<"Use a new disposable review directory.";return 2;}
    fs::create_directories(parent);
    ProjectService projects;if(!projects.CreateStoryFlowProject(destination,"Hurt Feedback Review")){std::cerr<<projects.LastError();return 3;}
    auto project=projects.CurrentProject();const auto root=fs::u8path(project.rootPath);std::string error;
    fs::create_directories(root/"Content/UI/Map");fs::copy_file(fs::u8path(art),root/"Content/UI/Map/World.png");
    const auto path=root/"Content/Scenes/Starter.wiscene";
    wi::scene::Scene scene;{wi::Archive file(path.generic_u8string(),true,false);scene.Serialize(file);}
    TransformState start;start.translation={0,2,0};CreatePlayerStartCommand player(scene,start);if(!player.Execute())return 4;
    const auto cube=[&](const char* name,XMFLOAT3 position,XMFLOAT3 half,XMFLOAT4 colour){
        const auto material=scene.Entity_CreateMaterial(std::string(name)+" material");auto* surface=scene.materials.GetComponent(material);surface->baseColor=colour;surface->roughness=.9f;
        const auto meshId=wi::ecs::CreateEntity();auto& mesh=scene.meshes.Create(meshId);
        mesh.vertex_positions={{-half.x,-half.y,-half.z},{half.x,-half.y,-half.z},{half.x,half.y,-half.z},{-half.x,half.y,-half.z},
            {-half.x,-half.y,half.z},{half.x,-half.y,half.z},{half.x,half.y,half.z},{-half.x,half.y,half.z}};
        mesh.vertex_normals.resize(8,{0,1,0});mesh.indices={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,3,7,6,3,6,2,0,4,7,0,7,3,1,2,6,1,6,5};
        mesh.subsets.resize(1);mesh.subsets[0].materialID=material;mesh.subsets[0].indexCount=36;
        const auto object=scene.Entity_CreateObject(name);scene.objects.GetComponent(object)->meshID=meshId;
        auto* transform=scene.transforms.GetComponent(object);transform->Translate(position);transform->UpdateTransform();
        auto& body=scene.rigidbodies.Create(object);body.mass=0;body.shape=wi::scene::RigidBodyPhysicsComponent::BOX;body.box.halfextents=half;
        if(!AssignNewPersistentEntityId(scene,object,error))throw std::runtime_error(error);return object;
    };
    cube("Map review ground",{0,-.1f,0},{40,.1f,40},{.19f,.32f,.22f,1});
    cube("Town hall",{8,1,8},{2,1,3},{.6f,.35f,.14f,1});cube("Vendor stall",{-8,1,7},{2,1,2},{.12f,.45f,.65f,1});
    cube("Ruins",{12,1,-10},{3,1,2},{.45f,.45f,.5f,1});
    const auto enemy=cube("Enemy map review",{-10,1,-8},{.5f,1,.5f},{.8f,.08f,.05f,1});
    CharacterAuthoringSettings settings;settings.factionId="Enemy";settings.combatStyle=CombatStyle::Melee;settings.role=CharacterRole::Passive;settings.autonomous=false;
    MakeCharacterCommand make(scene,enemy,settings);if(!make.Execute()){std::cerr<<"Cannot author enemy Character.";return 5;}
    const auto zone=[&](const char* name,const char* type,XMFLOAT3 position){
        zones::Create create(scene,position,zones::Behaviour::Location);if(!create.Execute())return false;
        auto* n=scene.names.GetComponent(create.Id());if(n)n->name=name;zones::Definition definition;
        if(!zones::Read(scene,create.Id(),definition))return false;definition.location.type=type;definition.location.announcement.enabled=false;
        zones::Edit edit(scene,create.Id(),definition);return edit.Execute();
    };
    if(!zone("Town hall","town",{8,0,8})||!zone("Quest giver","questgiver",{5,0,12})||!zone("Vendor","vendor",{-8,0,7}))return 6;
    {wi::Archive file(path.generic_u8string(),false,false);scene.Serialize(file);if(!file.SaveFile(path.generic_u8string()))return 7;}
    ScreenDocument hud;if(!ReadScreenDocument((root/fs::u8path(project.defaultHud)).generic_u8string(),project.projectId,hud,error))return 8;
    auto minimap=MakeWorldMinimapWidget();minimap.navigation->minimap->range=30;
    minimap.navigation->minimap->worldMapImage="Content/UI/Map/World.png";
    minimap.navigation->minimap->west=-40;minimap.navigation->minimap->east=40;minimap.navigation->minimap->south=-40;minimap.navigation->minimap->north=40;
    auto hurt=MakePlayerHurtWidget();
    ScreenWidget readout;readout.id=GenerateStableId();readout.name="Health";readout.kind=ScreenWidgetKind::Text;readout.text="E: sample hit (health unchanged) | Configure effects in HUD > Player hurt feedback";readout.rect={36,630,1100,44};readout.style=MakeScreenWidgetStyleTemplate(readout.kind,56);
    hud.widgets={minimap,readout,hurt};hud.actions.clear();hud.focusOrder.clear();
    if(!WriteScreenDocument((root/fs::u8path(project.defaultHud)).generic_u8string(),hud,error)){std::cerr<<error;return 9;}
    // Keep this dedicated proof direct-to-Level so no owner menu/route is edited.
    DocumentEnvelope sceneEnvelope;if(!ReadDocumentEnvelope(path.generic_u8string()+".rmeta",sceneEnvelope,error))return 10;
    fs::create_directories(root/"Content/Scripts");
    std::ofstream(root/"Content/Scripts/hurt_review.lua")<<R"LUA(
return {on_update=function(self,dt)
 if renegade.input.was_pressed("interact") then assert(renegade.hurt.hit(25,90)) end

end}
)LUA";
    auto scripts=CreateScriptDocument(project.projectId,sceneEnvelope.documentId,"Content/Scenes/Starter.wiscene","hurt-review");
    ScriptSourceBinding script;script.sourceId=GenerateStableId();script.sourcePath="Content/Scripts/hurt_review.lua";script.apiVersion=1;
    auto attachment=CreateScriptAttachment(ScriptScope::Level,{},script);attachment.presentation=ScriptPresentation::GlobalScript;scripts.attachments.push_back(attachment);
    if(!WriteScriptDocument(ScriptDocumentPathForScene(path.generic_u8string()),scripts,error))return 11;
    const auto descriptor=fs::u8path(project.descriptorPath);std::ifstream input(descriptor);std::string wire((std::istreambuf_iterator<char>(input)),{});
    const auto startup=wire.find("startup_scene =");if(startup!=wire.npos){const auto end=wire.find('\n',startup);wire.replace(startup,end-startup,"startup_scene = Content/Scenes/Starter.wiscene");}
    for(const char* key:{"startup_flow_id","startup_flow","startup_screen_id","startup_screen"}){
        const std::string prefix=std::string(key)+" =";const auto at=wire.find(prefix);if(at!=wire.npos){const auto end=wire.find('\n',at);wire.replace(at,end-at,prefix+" ");}
    }
    std::ofstream(descriptor,std::ios::trunc)<<wire;
    std::cout<<"HURT_REVIEW_READY "<<project.descriptorPath<<'\n';return 0;
}
