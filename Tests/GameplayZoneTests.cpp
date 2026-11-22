#include "renegade/bridge/ZoneService.h"
#include "renegade/bridge/PlayerService.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace zones = renegade::bridge::zones;
using namespace renegade::bridge;
using namespace DirectX;

static void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
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
        std::cout << "PASS rotated box, upper floor, adjacent room, non-solid anchor, history, preset wiring, WISCENE reopen\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
