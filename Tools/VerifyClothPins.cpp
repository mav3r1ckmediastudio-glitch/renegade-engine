#include "renegade/bridge/SoftBodyPhysicsService.h"
#include <Windows.h>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    HWND window = CreateWindowExW(0, L"STATIC", L"Cloth pin proof", WS_OVERLAPPEDWINDOW,
        0, 0, 960, 640, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    wi::Application app;
    app.allow_hdr = false;
    app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    struct Drain {
        ~Drain() {
            while (wi::renderer::IsPipelineCreationActive()) Sleep(10);
            wi::graphics::GetDevice()->WaitForGPU();
        }
    } drain;
    wi::physics::SetEnabled(true);
    wi::physics::SetSimulationEnabled(false);

    wi::scene::Scene scene;
    const auto meshId = scene.Entity_CreateMesh("Cloth proof mesh");
    const auto objectId = scene.Entity_CreateObject("Cloth proof object");
    const auto materialId = scene.Entity_CreateMaterial("Cloth proof material");
    scene.materials.GetComponent(materialId)->SetBaseColor({0.84f, 0.15f, 0.08f, 1.0f});
    scene.objects.GetComponent(objectId)->meshID = meshId;
    auto* transform = scene.transforms.GetComponent(objectId);
    transform->Translate(XMFLOAT3(0, 4, 0));
    auto& mesh = *scene.meshes.GetComponent(meshId);

    constexpr uint32_t side = 9;
    wi::vector<uint32_t> pins;
    for (uint32_t y = 0; y < side; ++y) {
        for (uint32_t x = 0; x < side; ++x) {
            mesh.vertex_positions.push_back({float(x) * 0.25f, 0, float(y) * 0.25f});
            mesh.vertex_normals.push_back({0, 1, 0});
            if (y == 0) pins.push_back(x);
            if (x + 1 == side || y + 1 == side) continue;
            const uint32_t i = y * side + x;
            mesh.indices.insert(mesh.indices.end(), {i, i + side, i + 1, i + 1, i + side, i + side + 1});
        }
    }
    auto& subset = mesh.subsets.emplace_back();
    subset.materialID = materialId;
    subset.indexCount = static_cast<uint32_t>(mesh.indices.size());
    mesh.CreateRenderData();

    using namespace renegade::bridge;
    if (!CreateSoftBodyPhysicsCommand(scene, objectId, {}).Execute() ||
        !SetSoftBodyPinsCommand(scene, objectId, pins).Execute())
        return 3;
    wi::Archive archive;
    scene.Serialize(archive);
    if (!archive.SaveFile(argv[1])) return 4;
    archive.SetReadModeAndResetPos(true);
    wi::scene::Scene reopened;
    reopened.Serialize(archive);
    const auto reopenObject = reopened.Entity_FindByName("Cloth proof object");
    const auto reopenMesh = ResolveSoftBodyMeshEntity(reopened, reopenObject);
    wi::vector<uint32_t> actualPins;
    if (!GetSoftBodyPinnedVertices(reopened, reopenObject, actualPins) || actualPins != pins)
        return 5;

    auto advance = [&] {
        reopened.Update(1.f / 60.f);
        wi::graphics::GetDevice()->SubmitCommandLists();
    };
    advance();
    advance();
    auto* body = reopened.softbodies.GetComponent(reopenMesh);
    if (!body || !HasLiveSoftBodyPhysics(reopened, reopenObject)) return 6;
    const auto nodeFor = [&](uint32_t graphicsIndex) {
        const auto& map = body->physicsToGraphicsVertexMapping;
        return static_cast<uint32_t>(std::find(map.begin(), map.end(), graphicsIndex) - map.begin());
    };
    const auto anchored = nodeFor(4);
    const auto free = nodeFor(side * (side - 1) + 4);
    XMFLOAT3 anchorStart, freeStart, anchorEnd, freeEnd;
    if (!GetSoftBodyNodePosition(reopened, reopenObject, anchored, anchorStart) ||
        !GetSoftBodyNodePosition(reopened, reopenObject, free, freeStart)) return 7;
    wi::physics::SetSimulationEnabled(true);
    for (int i = 0; i < 90; ++i) advance();
    if (!GetSoftBodyNodePosition(reopened, reopenObject, anchored, anchorEnd) ||
        !GetSoftBodyNodePosition(reopened, reopenObject, free, freeEnd)) return 8;
    std::cout << "Pinned Y: " << anchorStart.y << " -> " << anchorEnd.y
        << ", free Y: " << freeStart.y << " -> " << freeEnd.y << '\n';
    if (std::abs(anchorEnd.y - anchorStart.y) > 0.005f || freeStart.y - freeEnd.y < 0.35f)
        return 9;
    std::cout << "PASS: reopened cloth held its Jolt pins while unpinned vertices fell" << std::endl;
    return 0;
}
