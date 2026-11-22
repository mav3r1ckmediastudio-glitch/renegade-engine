#include "renegade/bridge/ZoneService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/ScriptLibraryService.h"
#include "../Runtime/src/RuntimeScriptRuntime.h"
#include "../Runtime/src/RuntimeZoneState.h"
#include "renegade/bridge/StudioSession.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace zones = renegade::bridge::zones;
using namespace renegade::bridge;
using namespace DirectX;

static void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

static void VerifyZoneLibrary(wi::scene::Scene& scene, const StableId& owner)
{
    namespace fs = std::filesystem;
    const auto stock = fs::path(__FILE__).parent_path().parent_path() / "Library/Scripts/RenegadeZones";
    const auto temp = fs::temp_directory_path() / ("renegade-zones-" + GenerateStableId());
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path,ec); } } cleanup{temp};
    for (const auto* project : {"First","Second"})
    {
        const auto root = temp / project;
        fs::create_directories(root);
        for (int index=0;index<int(zones::Behaviour::Count);++index)
        {
            const auto behaviour=zones::Behaviour(index);
            const bool win = behaviour == zones::Behaviour::Win;
            const std::string family=zones::Info(behaviour).family;
            ScriptLibraryAdoptionResult adopted;
            std::string error;
            const bool ok = ScriptLibraryService{}.AdoptEntry(root.generic_u8string(),
                (stock/family/ScriptLibraryPackageFilename).generic_u8string(),family+"Zone.lua",adopted,error);
            Require(ok,error.c_str());
            Require(adopted.projectSourcePath == zones::ScriptPath(behaviour),"Wrong project source path");
            auto doc=CreateScriptDocument(GenerateStableId(),GenerateStableId(),"Content/Scenes/Zone.wiscene","zone-test");
            auto attachment=CreateScriptAttachment(ScriptScope::Entity,owner,adopted.binding);
            Require(AddScriptAttachment(doc,attachment,error),"Could not attach stock script");
            auto dispatch = [&](bool enter) {
                renegade::runtime::RuntimeScriptRuntime lua;
                Require(lua.StartScene(doc,scene,root.generic_u8string(),error),"Could not start zone script");
                Require(lua.EntityOwnersForSource(adopted.projectSourcePath) == std::vector<StableId>{owner},
                    "Lost zone source ownership");
                GameplayEvent edge;
                edge.name=zones::EdgeEvent(behaviour,enter); edge.senderEntityId=edge.targetEntityId=owner;
                edge.payload=enter?"open":"close";
                Require(lua.EnqueueGameplayEvent(edge,error),"Could not queue zone edge");
                lua.Update(0); lua.Update(0);
                Require(lua.DisabledInstanceCount()==0,"Zone script failed");
                return lua.LastEventName();
            };
            doc.attachments.front().enabled = false;
            Require(dispatch(true)==zones::EdgeEvent(behaviour,true),"Disabled zone executed a script");
            doc.attachments.front().enabled = true;
            if (!win && behaviour!=zones::Behaviour::Trigger)
            {
                const std::string expected="zone.apply."+std::string(zones::EdgeEvent(behaviour,true)+5);
                Require(dispatch(true)==expected,"Stock zone did not dispatch the correct effect on entry");
                const bool releases=behaviour==zones::Behaviour::Ambience || behaviour==zones::Behaviour::Environment ||
                    behaviour==zones::Behaviour::Health || behaviour==zones::Behaviour::Message;
                const std::string exit=zones::EdgeEvent(behaviour,false);
                Require(dispatch(false)==(releases ? "zone.apply."+exit.substr(5) : exit),"Incorrect zone release");
                continue;
            }
            if (!win)
            {
                Require(dispatch(true)=="zone.forward.enter" && dispatch(false)=="zone.forward.exit","Wrong stock trigger events");
                continue;
            }
            Require(dispatch(true)=="zone.complete","Fresh project's Win Zone failed");
            if (std::string(project)=="First")
            {
                const auto source = root / adopted.projectSourcePath;
                std::ifstream in(source,std::ios::binary);
                std::string code{std::istreambuf_iterator<char>(in),{}}; in.close();
                code.replace(code.find("zone.complete"),13,"review.alternate");
                std::ofstream(source,std::ios::binary)<<code;
                Require(dispatch(true)=="review.alternate","Project edit was not executed");
            }
        }
    }
}

static void VerifyFirstSave()
{
    namespace fs=std::filesystem;
    const auto temp=fs::temp_directory_path()/("renegade-zone-save-"+GenerateStableId());
    struct Cleanup { fs::path path; ~Cleanup(){std::error_code ec; fs::remove_all(path,ec);} } cleanup{temp};
    fs::create_directories(temp/"Projects");
    const auto model=temp/"Template.wiscene";
    { wi::scene::Scene scene; scene.Entity_CreateTransform("Template"); wi::Archive out(model.generic_u8string(),false,false); scene.Serialize(out); }
    StudioSession session;
    session.Projects().Initialize((temp/"state.ini").generic_u8string());
    Require(session.Projects().CreateProject((temp/"Projects").generic_u8string(),"ZoneTest",model.generic_u8string()),"Create project failed");
    Require(session.LoadScene(session.Projects().StartupScenePath()),"Project did not open");
    session.NewScene();
    auto& scene=session.Scenes().GetScene();
    zones::Create create(scene,{}); Require(create.Execute(),"Unsaved zone creation failed");
    const auto& root=session.Projects().CurrentProject().rootPath;
    auto preset=fs::path(__FILE__).parent_path().parent_path()/"Library/Scripts/RenegadeZones/Message";
    ScriptLibraryAdoptionResult adoption;
    std::string error;
    Require(ScriptLibraryService{}.AdoptEntry(root,(preset/ScriptLibraryPackageFilename).generic_u8string(),"MessageZone.lua",adoption,error),"Could not adopt message");
    Require(session.Scripts().EnsureZoneDocument(error),"Could not initialise unsaved zone document");
    auto a=CreateScriptAttachment(ScriptScope::Entity,create.PersistentId(),adoption.binding);
    Require(AddScriptAttachment(*session.Scripts().Document(),a,error),"Unable to bind unsaved zone");
    const auto path=(fs::u8path(root)/"Content/Scenes/FirstSave.wiscene").generic_u8string();
    Require(session.SaveScene(path) && session.LoadScene(path) && session.Scripts().EnsureCurrent(error),"First save/reopen failed");
    Require(FindScriptAttachment(*session.Scripts().Document(),a.scriptInstanceId)!=nullptr,"First save lost zone script");
}

static void VerifyPalette()
{
    using namespace renegade::runtime;
    wi::scene::Scene authored;
    for (int i=0; i<int(zones::Behaviour::Count); ++i)
    {
        zones::Create create(authored,{float(i),0,0});
        Require(create.Execute(),"Failed to create zone");
        zones::Definition z; zones::Choose(z,zones::Behaviour(i));
        z.primary="first"; z.secondary="second"; z.amount=23; z.seconds=7; z.cooldown=4;
        z.target=GenerateStableId(); z.activation=1; z.repeat=2;
        zones::Write(authored,create.Id(),z);
    }
    wi::Archive saved; authored.Serialize(saved); saved.SetReadModeAndResetPos(true);
    wi::scene::Scene reopened; reopened.Serialize(saved);
    Require(reopened.metadatas.GetCount()==zones::Presets.size(),"Reopened palette lost zones");
    for (size_t i=0; i<zones::Presets.size(); ++i)
    {
        zones::Definition z;
        Require(zones::Read(reopened,reopened.metadatas.GetEntity(i),z) && int(z.behaviour)==int(i) &&
            z.primary=="first" && z.secondary=="second" && z.amount==23 && z.seconds==7 && z.cooldown==4 &&
            !z.target.empty() && z.activation==1 && z.repeat==2,"Reopen lost preset settings");
    }
    zones::Definition z; z.activation=1; z.repeat=2; z.cooldown=3;
    ZoneContact c{};
    Require(SampleZone(c,z,true,false,0)==ZoneEdge::None,"Interaction activated on entry");
    Require(SampleZone(c,z,true,true,0)==ZoneEdge::Enter,"Interaction missed");
    Require(SampleZone(c,z,true,true,0)==ZoneEdge::None,"Held interaction fired twice");
    Require(SampleZone(c,z,false,false,1)==ZoneEdge::Exit,"Lost matching exit");
    Require(SampleZone(c,z,true,true,1)==ZoneEdge::None,"Cooldown not respected");
    Require(SampleZone(c,z,true,true,1)==ZoneEdge::Enter,"Cooldown did not expire");
    z.repeat=1;
    (void)SampleZone(c,z,false,false,4);
    Require(SampleZone(c,z,true,true,0)==ZoneEdge::None,"One-shot fired twice");
    c={}; c.suppressArrival=true; z.activation=0; z.repeat=0;
    Require(SampleZone(c,z,true,false,0)==ZoneEdge::None,"Destination retriggered teleport");
    (void)SampleZone(c,z,false,false,0);
    Require(SampleZone(c,z,true,false,0)==ZoneEdge::Enter,"Teleport did not rearm after leaving");
}

int main(int argc, char** argv)
{
    try
    {
        wi::scene::Scene scene;
        zones::Create create(scene, {0,0,0});
        Require(create.Execute(), "Zone creation failed");
        const auto id = create.Id();
        const auto stable = PersistentEntityId(scene,id);
        Require(!stable.empty() && !scene.objects.Contains(id) && !scene.rigidbodies.Contains(id), "Expected invisible, non-solid zone");
        create.Undo();
        Require(!zones::IsZone(scene,id) && create.Execute() && stable == PersistentEntityId(scene,id), "Create undo/redo lost identity");
        zones::Definition box;
        Require(zones::Read(scene,id,box), "Missing default bounds");
        box.low = {-3,0,-.25f}; box.high = {3,2,.25f};
        box.enter = "open"; box.exit = "close"; box.target = GenerateStableId();
        zones::Edit edit(scene,id,box);
        Require(edit.Execute(), "Trigger edit failed");
        const auto world = XMMatrixRotationY(XM_PIDIV2) * XMMatrixTranslation(10,0,5);
        Require(zones::Contains(box,world,{10,1,7}), "Rotated doorway missed player");
        Require(!zones::Contains(box,world,{12,1,5}), "Adjacent room triggered doorway");
        Require(!zones::Contains(box,world,{10,3,5}), "Upper floor triggered doorway");
        float distance;
        wi::primitive::Ray ray(XMFLOAT3(5,1,6), XMFLOAT3(1,0,0));
        Require(zones::HitDistance(box,world,ray,distance) && std::abs(distance-4.75f)<.001f,
            "A click inside the rotated doorway face missed");
        ray.origin.y=3;
        Require(!zones::HitDistance(box,world,ray,distance), "A click above the doorway selected the zone");
        ray.origin={10,1,6};
        Require(zones::HitDistance(box,world,ray,distance) && std::abs(distance-.25f)<.001f,
            "Picking from inside must use the exit face");
        auto win = box; win.behaviour = zones::Behaviour::Win;
        zones::Edit preset(scene,id,win);
        Require(preset.Execute(), "Win preset failed");
        preset.Undo();
        zones::Definition loaded;
        Require(zones::Read(scene,id,loaded) && loaded.behaviour == zones::Behaviour::Trigger &&
            loaded.target == box.target && loaded.enter == "open" && loaded.exit == "close", "Preset undo lost wiring");
        Require(preset.Execute(), "Preset redo failed");
        wi::Archive saved;
        scene.Serialize(saved);
        saved.SetReadModeAndResetPos(true);
        wi::scene::Scene reopened;
        reopened.Serialize(saved);
        const auto copy = reopened.metadatas.GetEntity(0);
        Require(zones::Read(reopened,copy,loaded) && loaded.behaviour == zones::Behaviour::Win &&
            loaded.low == box.low && loaded.high == box.high && PersistentEntityId(reopened,copy) == stable,
            "WISCENE round trip lost settings");
        VerifyFirstSave();
        VerifyPalette();
        VerifyZoneLibrary(scene,stable);
        if (argc == 2)
        {
            const auto path = std::filesystem::u8path(argv[1]);
            Require(!std::filesystem::exists(path), "Review scene destination must be new");
            CreatePlayerStartCommand start(scene,{});
            Require(start.Execute(), "Review Player Start failed");
            wi::Archive output(path.generic_u8string(),false,false);
            Require(output.IsOpen(), "Unable to create review scene");
            scene.Serialize(output);
        }
        std::cout << "PASS rotated box, upper floor, adjacent room, non-solid anchor, history, preset wiring, WISCENE reopen, face ray picking, 13 presets, WISCENE settings, activation policies, independent projects and Lua behaviour\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
