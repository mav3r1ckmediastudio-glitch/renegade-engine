// Creates a new, disposable review project. Never changes an existing project.
#include "renegade/bridge/DialogueAssetService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/ProjectService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
using namespace renegade::bridge;
int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    namespace fs = std::filesystem;
    auto parent = fs::absolute(fs::u8path(argv[1]));
    if (parent.filename() != "dialogue-proof-20261115" || fs::exists(parent))
    {
        std::cerr << "Requires a fresh dialogue-proof-20261115 directory\n";
        return 1;
    }
    fs::create_directories(parent);
    wi::Application application;
    auto window = CreateWindowExW(0, L"STATIC", L"Dialogue fixture author", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr,
                                  nullptr, GetModuleHandleW(nullptr), nullptr);
    application.allow_hdr = false;
    application.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    auto scene = wi::allocator::make_shared<wi::scene::Scene>();
    auto cube = [&](const char *name, XMFLOAT3 pos, XMFLOAT3 scale) {
        auto e = scene->Entity_CreateCube(name);
        auto *t = scene->transforms.GetComponent(e);
        t->Scale(scale);
        t->Translate(pos);
        t->UpdateTransform();
        return e;
    };
    auto floor = cube("Walkable review platform", {0, -.25f, 0}, {10, .25f, 10});
    auto &body = scene->rigidbodies.Create(floor);
    body.mass = 0;
    body.shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;
    body.box.halfextents = {10, .25f, 10};
    auto npc = cube("Gate Guard - Dialogue Review", {0, 1, 2}, {.35f, .8f, .35f});
    CreateEnvironment(*scene, WeatherState{}, "Environment");
    TransformState player;
    player.translation = {0, 1, 0};
    CreatePlayerStartCommand create(*scene, player);
    if (!create.Execute())
        return 2;
    auto camera = scene->Entity_CreateCamera("Review camera", 1280, 720);
    auto *ct = scene->transforms.GetComponent(camera);
    ct->Translate({0, 2, -5});
    ct->UpdateTransform();
    scene->cameras.GetComponent(camera)->TransformCamera(*ct);
    scene->cameras.GetComponent(camera)->UpdateCamera();
    auto source = parent / "DialogueSource.wiscene";
    {
        wi::Archive archive(source.generic_u8string(), false, false);
        archive.SetCompressionEnabled(true);
        scene->Serialize(archive);
        if (!archive.SaveFile(source.generic_u8string()))
            return 3;
    }
    ProjectService projects;
    projects.Initialize((parent / "review-state.json").generic_u8string());
    if (!projects.CreateProject(parent.generic_u8string(), "DialogueReview", source.generic_u8string()))
    {
        std::cerr << projects.LastError();
        return 4;
    }
    const auto project = projects.CurrentProject();
    auto doc = MakeGuardDialogue(project.projectId, GenerateStableId());
    doc.lines[0].replies[0].text = "I have a permit.";
    doc.lines[0].replies[0].outcomes[0].actions = {{DialogueActionKind::SetFlag, "has_permit", 1}};
    doc.lines[0].replies[0].outcomes[0].next = "opening";
    std::string error;
    if (!SaveDialogueAsset(project.rootPath, doc, error))
    {
        std::cerr << error;
        return 5;
    }
    if (!projects.SetAlwaysInclude({"data:Content/Dialogue/" + doc.id + ".rdialogue"}))
        return 6;
    if (!AssignDialogueCommand(*scene, npc, doc.id).Execute() || !EnsurePersistentEntityIdentities(*scene, error))
        return 7;
    auto path = (fs::u8path(project.rootPath) / project.startupScene).generic_u8string();
    {
        wi::Archive archive(path, false, false);
        archive.SetCompressionEnabled(true);
        scene->Serialize(archive);
        if (!archive.SaveFile(path))
            return 8;
    }
    auto envelope = CreateDocumentEnvelope(project.projectId, "scene", project.startupScene, "dialogue-review-v1");
    if (!WriteDocumentEnvelope(path + ".rmeta", envelope, error))
    {
        std::cerr << error;
        return 9;
    }
    std::cout << project.descriptorPath << '\n';
    std::cout.flush();
    std::_Exit(0);
}
