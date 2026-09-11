#include <cmath>
#include <iostream>
#include <memory>

#include "renegade/bridge/CollisionService.h"
#include "renegade/bridge/TerrainService.h"

namespace
{
    class PreviewCommand final : public renegade::bridge::ICommand
    {
    public:
        explicit PreviewCommand(int& value) : value_(&value) {}
        bool Execute() override
        {
            *value_ = 2;
            return true;
        }
        void Undo() override { *value_ = 1; }
    private:
        int* value_ = nullptr;
    };

    bool NearlyEqual(float left, float right)
    {
        return std::abs(left - right) < 0.0001f;
    }

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    void SetPosition(
        wi::scene::TransformComponent& transform,
        const XMFLOAT3& position)
    {
        transform.ClearTransform();
        transform.Translate(position);
        transform.UpdateTransform();
    }
}

int main()
{
    wi::scene::Scene blankScene;
    if (renegade::bridge::CreateTerrain(
            blankScene,
            renegade::bridge::TerrainState{},
            "Unsafe Terrain") != wi::ecs::INVALID_ENTITY ||
        blankScene.terrains.GetCount() != 0 ||
        blankScene.weathers.GetCount() != 0)
    {
        return Fail("terrain creation accepted a scene without a dedicated Environment");
    }

    wi::scene::Scene scene;
    const auto entity = wi::ecs::CreateEntity();
    auto& terrain = scene.terrains.Create(entity);

    const auto native = renegade::bridge::CaptureTerrain(terrain);
    if (!native.centerToCamera || !native.removeDistantChunks ||
        native.physics || native.visibleChunkRadius != 12 ||
        !NearlyEqual(native.minimumHeight, -60.0f) ||
        !NearlyEqual(native.maximumHeight, 380.0f))
    {
        return Fail("native terrain defaults were not captured");
    }

    const renegade::bridge::TerrainState standard;
    if (standard.visibleChunkRadius != 9 ||
        standard.physicsChunkRadius != 10 ||
        renegade::bridge::TerrainChunkCountPerSide(
            standard.visibleChunkRadius) != 19 ||
        !NearlyEqual(
            renegade::bridge::TerrainWidthMeters(
                standard.visibleChunkRadius,
                standard.chunkScale),
            1254.0f))
    {
        return Fail("standard terrain dimensions were not 19 chunks / 1.254 km");
    }
    renegade::bridge::CommandService commands;
    if (!commands.Execute(
            std::make_unique<renegade::bridge::SetTerrainCommand>(
                scene,
                entity,
                standard)))
    {
        return Fail("standard terrain state did not execute");
    }

    const auto applied = renegade::bridge::CaptureTerrain(terrain);
    if (applied.centerToCamera || applied.removeDistantChunks ||
        !applied.physics || applied.visibleChunkRadius != 9 ||
        applied.physicsChunkRadius != 10 ||
        !NearlyEqual(applied.minimumHeight, -20.0f) ||
        !NearlyEqual(applied.maximumHeight, 120.0f) ||
        !NearlyEqual(applied.chunkScale, 1.0f))
    {
        return Fail("standard terrain state did not apply");
    }
    if (!commands.Undo() || !terrain.IsCenterToCamEnabled() ||
        !NearlyEqual(terrain.topLevel, 380.0f))
    {
        return Fail("terrain Undo did not restore native state");
    }
    if (!commands.Redo() || terrain.IsCenterToCamEnabled() ||
        !NearlyEqual(terrain.topLevel, 120.0f) ||
        terrain.physics_generation != 10)
    {
        return Fail("terrain Redo did not restore authored state/full fixed physics coverage");
    }

    auto unsafe = renegade::bridge::CaptureTerrain(terrain);
    unsafe.visibleChunkRadius = 999;
    unsafe.physicsChunkRadius = -10;
    unsafe.chunkScale = 0.0f;
    unsafe.minimumHeight = 2500.0f;
    unsafe.maximumHeight = -2500.0f;
    unsafe.lowAltitudeBlend = -1.0f;
    unsafe.baseBlend = 2.0f;
    unsafe.slopeBlend = 99.0f;
    unsafe.lodBias = 20.0f;
    renegade::bridge::ApplyTerrain(terrain, unsafe, false);
    const auto safe = renegade::bridge::CaptureTerrain(terrain);
    if (safe.visibleChunkRadius != 16 || safe.physicsChunkRadius != 17 ||
        !NearlyEqual(safe.chunkScale, 0.25f) ||
        !NearlyEqual(safe.minimumHeight, 1999.0f) ||
        !NearlyEqual(safe.maximumHeight, 2000.0f) ||
        !NearlyEqual(safe.lowAltitudeBlend, 0.0f) ||
        !NearlyEqual(safe.baseBlend, 1.0f) ||
        !NearlyEqual(safe.slopeBlend, 1.0f) ||
        !NearlyEqual(safe.lodBias, 4.0f))
    {
        return Fail("terrain safety bounds did not apply");
    }

    renegade::bridge::SetTerrainCommand noOp(scene, entity, safe, safe);
    if (noOp.Execute())
    {
        return Fail("identical terrain state polluted Undo history");
    }

    // Expansion changes only the authored extent. Existing inner chunk data
    // must survive Execute/Undo/Redo without Generation_Restart().
    terrain.generation = renegade::bridge::DefaultTerrainChunkRadius;
    terrain.center_chunk = {0, 0};
    wi::terrain::Chunk innerChunk = {0, 0};
    terrain.chunks[innerChunk].heightmap_data = {123, 456};
    renegade::bridge::CommandService expansionCommands;
    if (!expansionCommands.Execute(
            std::make_unique<renegade::bridge::ExpandTerrainCommand>(
                scene,
                entity)) ||
        terrain.generation != 10 ||
        terrain.physics_generation < 11 ||
        terrain.chunks[innerChunk].heightmap_data !=
            std::vector<std::uint16_t>({123, 456}))
    {
        return Fail("terrain expansion restarted or changed existing chunks");
    }
    wi::terrain::Chunk generatedOuter = {10, 0};
    terrain.chunks[generatedOuter].heightmap_data = {789};
    if (!expansionCommands.Undo() || terrain.generation != 9 ||
        terrain.chunks.find(generatedOuter) != terrain.chunks.end() ||
        terrain.chunks[innerChunk].heightmap_data !=
            std::vector<std::uint16_t>({123, 456}))
    {
        return Fail("terrain expansion Undo did not remove only the outer ring");
    }
    if (!expansionCommands.Redo() || terrain.generation != 10 ||
        terrain.chunks[innerChunk].heightmap_data !=
            std::vector<std::uint16_t>({123, 456}))
    {
        return Fail("terrain expansion Redo did not preserve existing chunks");
    }
    terrain.SetCenterToCamEnabled(true);
    renegade::bridge::ExpandTerrainCommand movingTerrainExpansion(
        scene,
        entity);
    if (movingTerrainExpansion.Execute())
    {
        return Fail("camera-following terrain accepted finite authored expansion");
    }
    terrain.SetCenterToCamEnabled(false);

    renegade::bridge::TerrainMaterialState material;
    renegade::bridge::SetTerrainTextureScale(material, 8.0f);
    if (!NearlyEqual(material.slots[0].texMulAdd.x, 0.25f) ||
        !NearlyEqual(
            renegade::bridge::MakeTerrainMaterialPreset(
                renegade::bridge::TerrainMaterialPreset::CoarseGrass),
            12.0f))
    {
        return Fail("terrain material scale was not mapped to packed UVs");
    }

    int previewValue = 2;
    renegade::bridge::CommandService previewCommands;
    if (!previewCommands.RecordExecuted(
            std::make_unique<PreviewCommand>(previewValue)) ||
        previewValue != 2 ||
        !previewCommands.Undo() || previewValue != 1 ||
        !previewCommands.Redo() || previewValue != 2)
    {
        return Fail("completed preview was not retained for Undo/Redo");
    }

    // Exercise the real Wicked/Jolt contact path with the same hierarchy and
    // local height samples used by generated terrain. This specifically guards
    // the world-zero root transform: the box must stop at Y=0, not fall to the
    // unshifted bottomLevel plane.
    wi::jobsystem::Initialize();
    wi::physics::Initialize();
    wi::physics::SetEnabled(true);
    wi::physics::SetSimulationEnabled(true);
    wi::physics::SetFrameRate(60.0f);
    wi::physics::SetAccuracy(4);
    wi::physics::SetInterpolationEnabled(false);

    wi::scene::Scene physicsScene;
    physicsScene.weather.gravity = XMFLOAT3(0.0f, -10.0f, 0.0f);
    physicsScene.weathers.Create(wi::ecs::CreateEntity());
    const auto terrainEntity = renegade::bridge::CreateTerrain(
        physicsScene,
        renegade::bridge::TerrainState{},
        "Jolt Contact Terrain");
    auto* physicsTerrain = physicsScene.terrains.GetComponent(terrainEntity);
    if (physicsTerrain == nullptr ||
        physicsTerrain->chunkGroupEntity == wi::ecs::INVALID_ENTITY ||
        !physicsScene.transforms.Contains(physicsTerrain->chunkGroupEntity))
    {
        return Fail("generated terrain chunk hierarchy did not inherit its root");
    }

    const wi::ecs::Entity chunk = wi::ecs::CreateEntity();
    physicsScene.transforms.Create(chunk);
    physicsScene.Component_Attach(
        chunk, physicsTerrain->chunkGroupEntity, true);

    const wi::ecs::Entity meshEntity = wi::ecs::CreateEntity();
    auto& mesh = physicsScene.meshes.Create(meshEntity);
    mesh.vertex_positions.reserve(9);
    for (int z = 0; z < 3; ++z)
    {
        for (int x = 0; x < 3; ++x)
        {
            mesh.vertex_positions.emplace_back(
                static_cast<float>(x) - 1.0f,
                physicsTerrain->bottomLevel,
                static_cast<float>(z) - 1.0f);
        }
    }
    mesh.indices = {
        0, 3, 1, 1, 3, 4,
        1, 4, 2, 2, 4, 5,
        3, 6, 4, 4, 6, 7,
        4, 7, 5, 5, 7, 8,
    };
    physicsScene.objects.Create(chunk).meshID = meshEntity;
    auto& heightfield = physicsScene.rigidbodies.Create(chunk);
    heightfield.shape =
        wi::scene::RigidBodyPhysicsComponent::CollisionShape::HEIGHTFIELD;
    heightfield.mass = 0.0f;
    heightfield.friction = 0.8f;

    // Model the creator's grounded-pivot crate: its render vertices occupy
    // Y=0..4 relative to the placed entity. Auto-fit must produce a two-metre
    // half-height at offset Y=2, not a four-metre half-height centred at Y=0.
    const wi::ecs::Entity crateMeshEntity = wi::ecs::CreateEntity();
    auto& crateMesh = physicsScene.meshes.Create(crateMeshEntity);
    crateMesh.vertex_positions = {
        XMFLOAT3(-0.4f, 0.0f, -0.4f), XMFLOAT3(0.4f, 0.0f, -0.4f),
        XMFLOAT3(-0.4f, 4.0f, -0.4f), XMFLOAT3(0.4f, 4.0f, -0.4f),
        XMFLOAT3(-0.4f, 0.0f,  0.4f), XMFLOAT3(0.4f, 0.0f,  0.4f),
        XMFLOAT3(-0.4f, 4.0f,  0.4f), XMFLOAT3(0.4f, 4.0f,  0.4f),
    };

    renegade::bridge::CommandService collisionCommands;
    const auto createDynamicCrate = [&](const float x, const float y)
    {
        const wi::ecs::Entity crate = wi::ecs::CreateEntity();
        auto& transform = physicsScene.transforms.Create(crate);
        SetPosition(transform, XMFLOAT3(x, y, 0.0f));
        physicsScene.objects.Create(crate).meshID = crateMeshEntity;
        renegade::bridge::CollisionState collision;
        collision.mass = 1.0f;
        collision.disableDeactivation = true;
        if (!collisionCommands.Execute(std::make_unique<
                renegade::bridge::CreateCollisionCommand>(
                    physicsScene, crate, collision)))
        {
            return wi::ecs::INVALID_ENTITY;
        }
        return crate;
    };

    const wi::ecs::Entity surfaceCrate = createDynamicCrate(-0.45f, 0.0f);
    const wi::ecs::Entity fallingCrate = createDynamicCrate(0.45f, 5.0f);
    const auto* fittedBody =
        physicsScene.rigidbodies.GetComponent(surfaceCrate);
    if (surfaceCrate == wi::ecs::INVALID_ENTITY ||
        fallingCrate == wi::ecs::INVALID_ENTITY || fittedBody == nullptr ||
        !NearlyEqual(fittedBody->box.halfextents.y, 2.0f) ||
        !NearlyEqual(fittedBody->local_offset.y, 2.0f))
    {
        return Fail("grounded-pivot crate did not receive centred primitive auto-fit");
    }

    wi::jobsystem::context physicsContext;
    for (int frame = 0; frame < 600; ++frame)
    {
        wi::physics::RunPhysicsUpdateSystem(
            physicsContext, physicsScene, 1.0f / 60.0f);
    }
    const auto* surfaceTransform =
        physicsScene.transforms.GetComponent(surfaceCrate);
    const auto* fallingTransform =
        physicsScene.transforms.GetComponent(fallingCrate);
    if (surfaceTransform == nullptr || fallingTransform == nullptr ||
        !std::isfinite(surfaceTransform->translation_local.y) ||
        !std::isfinite(fallingTransform->translation_local.y) ||
        surfaceTransform->translation_local.y < -0.1f ||
        surfaceTransform->translation_local.y > 0.1f ||
        fallingTransform->translation_local.y < -0.1f ||
        fallingTransform->translation_local.y > 0.1f)
    {
        return Fail("grounded or falling dynamic crate did not rest on world-zero terrain");
    }

    std::cout << "PASS: terrain authoring and world-zero Jolt HEIGHTFIELD contact\n";
    return 0;
}
