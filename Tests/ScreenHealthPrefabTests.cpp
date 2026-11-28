#include "renegade/bridge/ScreenPrefabService.h"
#include "renegade/bridge/ScreenSimulation.h"
#include "renegade/bridge/ScreenLiquidMaskService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/PlayerService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace renegade::bridge;
namespace fs=std::filesystem;
static ScreenDocument Vial(const StableId& project)
{
    ScreenDocument doc;doc.usage=ScreenUsage::Overlay;
    doc.envelope=CreateDocumentEnvelope(project,RuntimeScreenDocumentType,"Content/UI/Vial.renegade-screen","animated-vial-v1");
    ScreenWidget root;root.id=GenerateStableId();root.kind=ScreenWidgetKind::Image;root.name="Animated vial";
    root.rect={0,0,288,576};root.enabled=false;root.style=MakeScreenWidgetStyleTemplate(root.kind,576);root.container.padding=0;
    ScreenWidget liquid=root;liquid.id=GenerateStableId();liquid.parentId=root.id;liquid.kind=ScreenWidgetKind::Progress;liquid.name="Liquid";
    liquid.fill=1;liquid.fillDirection=ScreenFillDirection::BottomToTop;liquid.liquid.emplace();
    liquid.liquid->mask="Content/UI/Vial/interior_mask.png";liquid.liquid->left=87.f/288;liquid.liquid->right=201.f/288;
    liquid.liquid->top=125.f/576;liquid.liquid->bottom=518.f/576;
    for(auto* v:{&liquid.style.normal,&liquid.style.hover,&liquid.style.pressed,&liquid.style.focused,&liquid.style.disabled})v->imageTint={250,60,42,255};
    liquid.layoutMode=ScreenLayoutMode::Anchored;liquid.anchors={0,0,1,1,0,0,0,0};
    ScreenBinding binding;binding.property=ScreenBindingProperty::Fill;binding.operation=ScreenBindingOperation::Ratio;
    binding.source.attributeId="health.current";binding.denominator.attributeId="health.maximum";liquid.bindings={binding};
    ScreenWidget frame=root;frame.id=GenerateStableId();frame.parentId=root.id;frame.name="Glass and metal";
    frame.resourcePath="Content/UI/Vial/static_art.png";frame.layoutMode=ScreenLayoutMode::Anchored;frame.anchors=liquid.anchors;
    doc.widgets={root,liquid,frame};return doc;
}
int main(int argc,char** argv)
{
    const auto temp=fs::temp_directory_path()/fs::u8path("RenegadeHealthPrefab-"+GenerateStableId());
    struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}} cleanup{temp};
    const auto source=temp/"Source";fs::create_directories(source/"Content/UI/Vial");
    auto doc=Vial(GenerateStableId());std::string error;int failures=0;
    const auto check=[&](bool ok,const char* why){if(!ok){++failures;std::cerr<<why<<": "<<error<<'\n';}};
    if(argc==3&&std::string(argv[1])=="--hud-review")
    {
        const auto parent=fs::u8path(argv[2]);
        if(fs::exists(parent)){std::cerr<<"Use a new disposable fixture directory.";return 2;}
        fs::create_directories(parent);
        ProjectService projects;
        if(!projects.CreateStoryFlowProject(parent.generic_u8string(),"HUD Components Review"))
        {std::cerr<<projects.LastError();return 3;}
        const auto project=projects.CurrentProject();const auto root=fs::u8path(project.rootPath);
        const auto scenePath=(root/"Content/Scenes/Starter.wiscene").generic_u8string();
        wi::scene::Scene scene;
        {wi::Archive archive(scenePath,true,false);if(!archive.IsOpen())return 4;scene.Serialize(archive);}
        TransformState start;start.translation={0,2,0};CreatePlayerStartCommand player(scene,start);
        if(!player.Execute())return 5;
        const auto cube=[&](const char* name,XMFLOAT3 position,XMFLOAT3 half,XMFLOAT4 colour)
        {
            const auto material=scene.Entity_CreateMaterial(std::string(name)+" material");
            auto* surface=scene.materials.GetComponent(material);surface->baseColor=colour;surface->roughness=.85f;
            const auto meshId=wi::ecs::CreateEntity();auto& mesh=scene.meshes.Create(meshId);
            mesh.vertex_positions={{-half.x,-half.y,-half.z},{half.x,-half.y,-half.z},{half.x,half.y,-half.z},{-half.x,half.y,-half.z},
                {-half.x,-half.y,half.z},{half.x,-half.y,half.z},{half.x,half.y,half.z},{-half.x,half.y,half.z}};
            mesh.vertex_normals.resize(8,{0,1,0});
            mesh.indices={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,3,7,6,3,6,2,0,4,7,0,7,3,1,2,6,1,6,5};
            mesh.subsets.resize(1);mesh.subsets[0].materialID=material;mesh.subsets[0].indexOffset=0;mesh.subsets[0].indexCount=36;
            const auto object=scene.Entity_CreateObject(name);scene.objects.GetComponent(object)->meshID=meshId;
            auto* transform=scene.transforms.GetComponent(object);transform->Translate(position);transform->UpdateTransform();
            auto& body=scene.rigidbodies.Create(object);body.mass=0;body.shape=wi::scene::RigidBodyPhysicsComponent::BOX;body.box.halfextents=half;
            return AssignNewPersistentEntityId(scene,object,error);
        };
        if(!cube("HUD review floor",{0,-.1f,0},{20,.1f,20},{.18f,.23f,.28f,1})||
            !cube("HUD review marker",{0,1,8},{1,1,1},{.8f,.3f,.08f,1}))return 6;
        {wi::Archive archive(scenePath,false,false);scene.Serialize(archive);if(!archive.SaveFile(scenePath))return 7;}
        ScreenDocument hud;
        if(!ReadScreenDocument((root/fs::u8path(project.defaultHud)).generic_u8string(),project.projectId,hud,error))return 8;
        hud.widgets.erase(std::remove_if(hud.widgets.begin(),hud.widgets.end(),[](const auto& w){return w.kind==ScreenWidgetKind::Progress||w.name.find(" Label")!=std::string::npos;}),hud.widgets.end());
        for(auto& w:hud.widgets)if(w.name=="Pause Hint")w.anchors={.7f,.03f,.96f,.07f,0,0,0,0};
        if(!WriteScreenDocument((root/fs::u8path(project.defaultHud)).generic_u8string(),hud,error))return 9;
        std::cout<<"HUD_REVIEW_READY "<<project.descriptorPath<<'\n';return 0;
    }
    if(argc==4&&std::string(argv[1])=="--author-vial")
    {
        const auto art=fs::u8path(argv[2]);
        fs::copy_file(art/"static_art.png",source/"Content/UI/Vial/static_art.png");
        fs::copy_file(art/"interior_mask.png",source/"Content/UI/Vial/interior_mask.png");
        check(WriteScreenDocument((source/doc.envelope.pathHint).generic_u8string(),doc,error),"Author vial Screen");
        ScreenAuthoringSession session;check(session.Open((source/doc.envelope.pathHint).generic_u8string(),doc.envelope.projectId,error),"Open vial source");
        check(PublishScreenWidgetPrefab(source.generic_u8string(),session,doc.widgets.front().id,argv[3],error),"Publish built-in vial");
        if(!failures)std::cout<<"PASS: published supplied-art animated vial prefab\n";return failures?1:0;
    }
    {std::ofstream file(source/"Content/UI/Vial/static_art.png",std::ios::binary);file<<"unaltered glass fixture";}
    {std::ofstream file(source/"Content/UI/Vial/interior_mask.png",std::ios::binary);file<<"interior mask fixture";}
    {std::ofstream file(source/"Content/UI/Vial/change.wav",std::ios::binary);file<<"sound fixture";}
    doc.widgets[1].liquid->decreaseSound="Content/UI/Vial/change.wav";
    check(WriteScreenDocument((source/doc.envelope.pathHint).generic_u8string(),doc,error),"Write layered vial");
    ScreenAuthoringSession sourceSession;
    check(sourceSession.Open((source/doc.envelope.pathHint).generic_u8string(),doc.envelope.projectId,error),"Open source vial");
    const auto package=temp/"VialPackage";
    check(PublishScreenWidgetPrefab(source.generic_u8string(),sourceSession,doc.widgets.front().id,package.generic_u8string(),error),"Publish layered prefab");
    const auto target=temp/"Target";fs::create_directories(target/"Content/UI");
    ScreenDocument hud;hud.usage=ScreenUsage::Hud;
    hud.envelope=CreateDocumentEnvelope(GenerateStableId(),RuntimeScreenDocumentType,"Content/UI/Hud.renegade-screen","health-prefab-test");
    check(WriteScreenDocument((target/hud.envelope.pathHint).generic_u8string(),hud,error),"Write target HUD");
    ScreenAuthoringSession session;check(session.Open((target/hud.envelope.pathHint).generic_u8string(),hud.envelope.projectId,error),"Open target HUD");
    std::vector<StableId> health,mana;
    check(InsertScreenWidgetPrefab(target.generic_u8string(),session,package.generic_u8string(),40,30,health,error),"Insert health instance");
    check(InsertScreenWidgetPrefab(target.generic_u8string(),session,package.generic_u8string(),400,30,mana,error),"Insert mana instance");
    if(health.size()!=1||mana.size()!=1)return 1;
    check(health.front()!=mana.front()&&session.Document().widgets.size()==6,"Independent hierarchy identities");
    check(session.Undo(error)&&session.Document().widgets.size()==3&&session.Redo(error)&&session.Document().widgets.size()==6,"One-step prefab insert Undo/Redo");
    StableId healthLiquid,manaLiquid;
    for(const auto& w:session.Document().widgets)if(w.liquid)(w.parentId==health.front()?healthLiquid:manaLiquid)=w.id;
    const auto configure=[&](const StableId& id,const char* name,ScreenColor colour){
        const auto* w=session.FindWidget(id);if(!w)return false;
        auto binding=w->bindings.front();binding.source.attributeId=std::string(name)+".current";binding.denominator.attributeId=std::string(name)+".maximum";
        return session.ConfigureLiquidWidget(id,*w->liquid,colour,{binding},error);
    };
    check(configure(healthLiquid,"health",{250,60,42,255})&&configure(manaLiquid,"mana",{55,130,255,255}),"Set independent colour/binding parameters");
    ScreenSimulation simulation;
    for(const auto& sample:std::vector<std::pair<const char*,double>>{{"health.current",100},{"health.maximum",100},{"mana.current",35},{"mana.maximum",100}})
        check(simulation.SetSample({ScreenBindingScope::PossessedPlayer,{},sample.first},sample.second,error),"Set explicit Preview sample");
    ScreenBindingFrame frame;check(simulation.Evaluate(session.Document(),frame,error)&&frame.ready,"Evaluate independent Preview bindings");
    const auto resolved=[&](const StableId& id){const auto w=std::find_if(frame.widgets.begin(),frame.widgets.end(),[&](const auto& item){return item.id==id;});return w==frame.widgets.end()?-1.f:w->fill;};
    check(std::abs(resolved(healthLiquid)-1.f)<.001f&&std::abs(resolved(manaLiquid)-.35f)<.001f,"Resolved liquid fractions crossed instances");
    check(simulation.SetSample({ScreenBindingScope::PossessedPlayer,{},"mana.current"},0.0,error)&&
        simulation.Evaluate(session.Document(),frame,error)&&resolved(manaLiquid)==0&&resolved(healthLiquid)==1,"Draining mana changed health");
    check(simulation.SetSample({ScreenBindingScope::PossessedPlayer,{},"mana.current"},35.0,error),"Restore mana sample");
    // The same source query semantics drive both Studio simulation and Runtime.
    const auto h=session.FindWidget(healthLiquid),m=session.FindWidget(manaLiquid);
    const auto hs=simulation.Sample(h->bindings.front().source),ms=simulation.Sample(m->bindings.front().source);
    check(hs&&ms&&std::get<double>(hs->value)==100&&std::get<double>(ms->value)==35&&
        h->style.normal.imageTint.red==250&&m->style.normal.imageTint.blue==255,"Health and mana samples/colours crossed instances");
    for(const auto& w:session.Document().widgets)if(!w.resourcePath.empty())
        check(w.style.normal.imageTint.red==255&&w.style.normal.imageTint.green==255&&w.style.normal.imageTint.blue==255,"Liquid recolour altered glass/frame tint");
    check(session.Undo(error)&&session.Redo(error),"Liquid parameter Undo/Redo");
    check(session.Save(error),"Save configured instances");
    const auto republished=temp/"Republished";
    check(PublishScreenWidgetPrefab(target.generic_u8string(),session,mana.front(),republished.generic_u8string(),error),"Republish imported hierarchy");
    ScreenDocument compact;
    check(InspectPortableScreenPackage(republished.generic_u8string(),compact,error),"Read republished hierarchy");
    for(const auto& w:compact.widgets)if(w.liquid)
        check(w.liquid->mask.rfind("Content/UI/Assets/",0)==0&&w.liquid->decreaseSound.rfind("Content/UI/Assets/",0)==0,"Republish accumulated source directories");
    std::vector<StableId> roundTrip;
    check(InsertScreenWidgetPrefab(target.generic_u8string(),session,republished.generic_u8string(),700,30,roundTrip,error)&&
        roundTrip.size()==1&&session.Document().widgets.size()==9&&session.Undo(error)&&session.Document().widgets.size()==6,"Republished prefab reuse and Undo");
    check(session.Save(error),"Save after prefab reuse Undo");
    fs::rename(source,temp/"SourceOffline");fs::rename(package,temp/"PackageOffline");
    check(session.Reload(error)&&session.Document().widgets.size()==6,"Reopen without original project/package");
    for(const auto& w:session.Document().widgets)
    {
        if(w.liquid)
        {
            check(fs::is_regular_file(target/w.liquid->mask)&&fs::is_regular_file(target/w.liquid->decreaseSound)&&
                w.liquid->bubbles==5&&w.liquid->animated,"Prefab mask/sound/animation not self-contained");
            auto invalid=*w.liquid;invalid.top=invalid.bottom;
            check(!session.ConfigureLiquidWidget(w.id,invalid,w.style.normal.imageTint,w.bindings,error),"Invalid liquid extent accepted");
            invalid=*w.liquid;invalid.wavePixels=std::numeric_limits<float>::quiet_NaN();
            check(!session.ConfigureLiquidWidget(w.id,invalid,w.style.normal.imageTint,w.bindings,error),"Non-finite liquid setting accepted");
            invalid=*w.liquid;invalid.mask="../escape.png";
            check(!session.ConfigureLiquidWidget(w.id,invalid,w.style.normal.imageTint,w.bindings,error),"Unsafe liquid mask accepted");
        }
    }
    check(session.Save(error)&&session.Reload(error),"Retain validated liquid settings after rejected edits");
    const auto sourceTree=fs::path(__FILE__).parent_path().parent_path();
    fs::copy_file(sourceTree/"Tests/assets/ui/liquid/alpha-mask.png",target/"Content/UI/alpha-mask.png");
    fs::copy_file(sourceTree/"Tests/assets/ui/liquid/empty-mask.png",target/"Content/UI/empty-mask.png");
    ScreenLiquid generic;generic.wavePixels=2;generic.bubbles=0;
    check(InspectScreenLiquidMask(target.generic_u8string(),"Content/UI/alpha-mask.png",generic,error)&&
        generic.left==.25f&&generic.top==.25f&&generic.right==.75f&&generic.bottom==.75f,"Transparent mask auto-bounds");
    const auto fitted=generic;
    check(!InspectScreenLiquidMask(target.generic_u8string(),"Content/UI/empty-mask.png",generic,error)&&generic.mask==fitted.mask&&generic.left==fitted.left,"Empty mask rejected without mutation");
    check(!InspectScreenLiquidMask(target.generic_u8string(),"../escape.png",generic,error),"Unsafe mask path rejected");
    check(session.ConfigureProgressFill(healthLiquid,fitted,{40,220,80,255},{},ScreenFillDirection::LeftToRight,error),"Convert ordinary fill with a custom mask and no gameplay binding");
    check(session.FindWidget(healthLiquid)->bindings.empty()&&session.Undo(error)&&session.Redo(error)&&session.Save(error)&&session.Reload(error),"Generic fill history/save/reload");
    check(session.FindWidget(healthLiquid)->liquid&&session.FindWidget(healthLiquid)->fillDirection==ScreenFillDirection::LeftToRight&&session.FindWidget(healthLiquid)->bindings.empty(),"Generic masked fill persisted");
    check(session.ConfigureProgressFill(healthLiquid,std::nullopt,{255,255,255,255},{},ScreenFillDirection::TopToBottom,error)&&!session.FindWidget(healthLiquid)->liquid&&session.Undo(error)&&session.FindWidget(healthLiquid)->liquid,"Remove liquid effect and Undo");
    ScreenDocument stock;
    check(InspectPortableScreenPackage((sourceTree/"Studio/assets/ui/prefabs/LiquidFill").generic_u8string(),stock,error)&&stock.widgets.size()==1&&stock.widgets.front().liquid&&stock.widgets.front().bindings.empty(),"Stock generic Liquid Fill is an editable unbound Progress layer");
    if(!failures)std::cout<<"PASS: layered health/mana prefab portability, parameters, independent Preview, history and invalid settings\n";
    return failures?1:0;
}
