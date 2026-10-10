#include "RuntimeProjectileWorld.h"

#include <iostream>
#include <memory>

using namespace renegade;

int main()
{
    // Exercise the real Scene::IntersectsAll and native BVH/primitive path.
    // Populate its CPU query cache without a renderer or advancing gameplay.
    auto scene = std::make_unique<wi::scene::Scene>();
    std::string error;
    const auto owner = scene->Entity_CreateTransform("Projectile Owner");
    const auto target = scene->Entity_CreateTransform("Projectile Target");
    const auto targetChild = scene->Entity_CreateTransform("Target Collider");
    scene->hierarchy.Create(targetChild).parentID = target;
    const auto wall = scene->Entity_CreateTransform("World Blocker");
    const auto ownerId = runtime::RuntimePlayerKnowledgeId;
    const auto targetId = bridge::GenerateStableId();
    const auto sourceId = bridge::GenerateStableId();
    if (!bridge::AssignPersistentEntityId(*scene, target, targetId, error))
        return 1;
    std::vector<wi::scene::ColliderComponent> cache;
    std::vector<wi::primitive::AABB> bounds;
    auto addSphere = [&](wi::ecs::Entity entity, float x, float radius) {
        auto& collider = scene->colliders.Create(entity);
        collider.shape = wi::scene::ColliderComponent::Shape::Sphere;
        collider.radius = radius;
        collider.sphere = wi::primitive::Sphere({x,0,0}, radius);
        cache.push_back(collider);
        bounds.emplace_back(XMFLOAT3{x-radius,-radius,-radius},
                            XMFLOAT3{x+radius,radius,radius});
    };
    // More than six owner child hits must not hide an actual world blocker.
    for (int i = 0; i < 10; ++i)
    {
        const auto child = scene->Entity_CreateTransform("Owner Collider");
        scene->hierarchy.Create(child).parentID = owner;
        addSphere(child, 0.2f + 0.1f * i, 0.02f);
    }
    addSphere(targetChild, 10, 1);
    addSphere(wall, 5, 0.25f);
    scene->colliders_cpu = cache.data();
    scene->collider_bvh.Build(bounds.data(), static_cast<std::uint32_t>(bounds.size()));

    runtime::RuntimeCharacterSystemState characters;
    runtime::RuntimeCharacterRecord character;
    character.entity = target;
    character.stableEntityId = targetId;
    character.authoring.factionId = "Enemy";
    character.authoring.combatStyle = bridge::CombatStyle::Melee;
    character.tuning = bridge::ResolveCharacterTuning(character.authoring);
    characters.characters.push_back(character);
    auto& native = scene->characters.Create(target);
    native.health = 100;
    native.SetActive(true);
    runtime::RuntimeCombatState combat;
    if (!runtime::InitializeRuntimeCombat(*scene, characters, combat, error))
    {
        std::cerr << error;
        return 1;
    }
    runtime::RuntimeCharacterPerceptionState perception;
    runtime::CharacterCognitionRecord cognition;
    cognition.characterId = targetId;
    perception.characters.push_back(cognition);

    bridge::ProjectileLaunch launch;
    launch.source = {ownerId, sourceId, "Player", {1,2,3}, {4,5,6}};
    launch.definition.acceleration = {};
    launch.velocity = {100,0,0};
    bridge::ProjectileSimulation simulation;
    std::uint64_t id;
    auto fail = [](const char* reason) { std::cerr << reason << '\n'; return 1; };
    if (!simulation.Launch(launch, id, error))
        return fail("launch");
    const runtime::ProjectileOwnerBinding binding{ownerId, owner};
    const auto query = [&](const bridge::ProjectileRecord& record,
                           const bridge::ProjectileVector& from,
                           const bridge::ProjectileVector& to) {
        return runtime::QueryProjectileSceneSegment(*scene, characters, binding, record, from, to);
    };
    auto hit = query(simulation.Records()[0], {0,0,0}, {20,0,0});
    if (hit.status != bridge::ProjectileQueryStatus::Hit ||
        std::abs(hit.contact.position.x - 4.75f) > 0.001f ||
        !hit.contact.targetSubjectId.empty())
        return fail("world blocker must precede Character and exclude owner children");
    // A projectile's render-only hierarchy must never become world cover.
    const std::vector<wi::ecs::Entity> visualRoots{wall};
    runtime::ProjectileOwnerBinding withVisuals{ownerId,owner,&visualRoots};
    const auto visualHit=runtime::QueryProjectileSceneSegment(*scene,characters,
        withVisuals,simulation.Records()[0],{0,0,0},{20,0,0});
    if(visualHit.status!=bridge::ProjectileQueryStatus::Hit ||
        visualHit.contact.targetSubjectId!=targetId)
        return fail("projectile visual blocks world ray");
    if(runtime::QueryProjectileSceneSegment(*scene,characters,withVisuals,
        simulation.Records()[0],{5,0,0},{5.1f,0,0}).status!=bridge::ProjectileQueryStatus::Miss)
        return fail("projectile visual blocks origin overlap");
    std::vector<bridge::ProjectileImpact> impacts;
    if (!simulation.Update(0.2f, query, impacts, error) || impacts.size() != 1)
        return fail("native simulation wall impact");
    bridge::GameplayEventService events;
    const runtime::CombatEventEmitter emitter = [&](bridge::GameplayEvent event,
                                                   std::string& eventError) {
        return events.Enqueue(std::move(event), eventError);
    };
    auto damage = runtime::ApplyProjectileCharacterImpact(
        *scene, characters, perception, combat, impacts[0], emitter);
    if (damage.damageApplied || native.health != 100 || events.Size() != 0)
        return fail("wall incorrectly damages Character behind it");

    // Disable wall's query layer in the native cache; same target now receives hit.
    cache.back().layerMask = 0;
    if (!simulation.Launch(launch, id, error) ||
        !simulation.Update(0.2f, query, impacts, error) ||
        impacts.size() != 1 || impacts[0].contact.targetSubjectId != targetId ||
        impacts[0].contact.surfaceType != bridge::ImpactSurfaceType::Character)
        return fail("Character child target resolution/classification");
    damage = runtime::ApplyProjectileCharacterImpact(
        *scene, characters, perception, combat, impacts[0], emitter);
    if (!damage.damageApplied || native.health != 90 ||
        runtime::FindCharacterCombat(combat, targetId)->health != 90 ||
        perception.damageStimuli != 1)
        return fail("attributed Character health routing");
    const auto* memory = runtime::FindCharacterMemory(perception.characters[0], ownerId);
    if (memory == nullptr || memory->source != runtime::KnowledgeSource::DamagedBy ||
        memory->lastKnownPosition.x != 1 || memory->lastKnownVelocity.x != 4)
        return fail("legitimate source knowledge");
    bridge::GameplayEvent event;
    if (!events.TryDequeue(event) || event.name != "ai.damage" ||
        event.senderEntityId != ownerId || event.targetEntityId != targetId)
        return fail("existing governed damage event");
    if (!simulation.Update(0.2f, query, impacts, error) ||
        !impacts.empty() || native.health != 90)
        return fail("duplicate hit");

    runtime::ProjectileOwnerBinding invalid{"different-owner", owner};
    bridge::ProjectileRecord record{id, launch, 0};
    if (runtime::QueryProjectileSceneSegment(*scene, characters, invalid,
        record, {0,0,0}, {20,0,0}).status != bridge::ProjectileQueryStatus::Blocked)
        return fail("owner binding mismatch");
    if (query(record, {0,0,0}, {2,0,0}).status != bridge::ProjectileQueryStatus::Miss)
        return fail("owner-only ray");
    if (query(record, {0,0,0}, {8,0,0}).status != bridge::ProjectileQueryStatus::Miss)
        return fail("segment bounds");
    bridge::ProjectileImpact malformed;
    malformed.contact.targetSubjectId = targetId;
    malformed.source = launch.source;
    malformed.source.sourceAssetId = "invalid";
    malformed.damage = 10;
    damage = runtime::ApplyProjectileCharacterImpact(
        *scene, characters, perception, combat, malformed, emitter);
    if (damage.damageApplied || damage.error.empty() || native.health != 90)
        return fail("malformed source mutates health");
    malformed.source.sourceAssetId = sourceId;
    malformed.source.ownerSubjectId = targetId;
    damage = runtime::ApplyProjectileCharacterImpact(
        *scene, characters, perception, combat, malformed, emitter);
    if (damage.damageApplied || native.health != 90)
        return fail("self damage");
    malformed.source.ownerSubjectId = ownerId;
    malformed.source.factionId = "bad\nfaction";
    damage = runtime::ApplyProjectileCharacterImpact(
        *scene, characters, perception, combat, malformed, emitter);
    if (damage.damageApplied || native.health != 90 || perception.damageStimuli != 1)
        return fail("invalid faction mutates health or knowledge");

    cache.back().layerMask = ~0u;
    hit = query(record, {5,0,0}, {5.1f,0,0});
    if (hit.status != bridge::ProjectileQueryStatus::Hit || hit.contact.fraction != 0)
        return fail("origin inside collider must not travel through cover");
    cache.back().layerMask = 0;

    // Existing damage path remains death authority.
    malformed.source.factionId = "Player";
    malformed.damage = 100;
    damage = runtime::ApplyProjectileCharacterImpact(
        *scene, characters, perception, combat, malformed, emitter);
    if (!damage.damageApplied || native.health != 0 ||
        !runtime::FindCharacterCombat(combat, targetId)->dead)
        return fail("existing death routing");
    damage = runtime::ApplyProjectileCharacterImpact(
        *scene, characters, perception, combat, malformed, emitter);
    if (damage.damageApplied)
        return fail("dead target damaged again");

    const auto material = scene->Entity_CreateTransform("Surface Material");
    scene->materials.Create(material);
    const auto materialId = bridge::GenerateStableId();
    if (!bridge::AssignPersistentEntityId(*scene, material, materialId, error))
        return fail("material identity");
    const auto meshEntity = scene->Entity_CreateTransform("Surface Mesh");
    scene->meshes.Create(meshEntity).subsets.resize(1);
    scene->meshes.GetComponent(meshEntity)->subsets[0].materialID = material;
    scene->objects.Create(meshEntity).meshID = meshEntity;
    wi::scene::Scene::RayIntersectionResult surfaceHit;
    surfaceHit.entity = meshEntity;
    surfaceHit.subsetIndex = 0;
    if (runtime::ProjectileContactSurface(*scene, surfaceHit) != materialId)
        return fail("material subset identity");
    if (!bridge::ApplyMaterialImpactSurface(
            *scene, material, bridge::ImpactSurfaceType::Metal) ||
        bridge::CaptureMaterialImpactSurface(*scene, material) !=
            bridge::ImpactSurfaceType::Metal ||
        runtime::ProjectileContactSurfaceType(
            *scene, characters, surfaceHit, {}) !=
            bridge::ImpactSurfaceType::Metal)
        return fail("authored impact surface classification");
    bridge::SetMaterialImpactSurfaceCommand surfaceCommand(
        *scene, material, bridge::ImpactSurfaceType::Wood);
    if (!surfaceCommand.Execute() ||
        bridge::CaptureMaterialImpactSurface(*scene, material) !=
            bridge::ImpactSurfaceType::Wood)
        return fail("impact surface command execute");
    surfaceCommand.Undo();
    if (bridge::CaptureMaterialImpactSurface(*scene, material) !=
        bridge::ImpactSurfaceType::Metal)
        return fail("impact surface command undo");
    // Two instances share one mesh and material: authoring one must not leak.
    const auto secondObject = scene->Entity_CreateTransform("Shared mesh instance");
    scene->objects.Create(secondObject).meshID = meshEntity;
    bridge::SetObjectImpactSurfaceCommand objectSurface(
        *scene, meshEntity, bridge::ImpactSurfaceType::Wood);
    if (!objectSurface.Execute() ||
        runtime::ProjectileContactSurfaceType(*scene, characters, surfaceHit, {}) !=
            bridge::ImpactSurfaceType::Wood ||
        bridge::ResolveObjectImpactSurface(*scene, secondObject, 0) !=
            bridge::ImpactSurfaceType::Metal ||
        scene->meshes.GetComponent(meshEntity)->subsets[0].materialID != material ||
        bridge::CaptureMaterialImpactSurface(*scene, material) !=
            bridge::ImpactSurfaceType::Metal)
        return fail("object surface leaks across shared mesh/material");
    objectSurface.Undo();
    if (bridge::HasObjectImpactSurface(*scene, meshEntity) ||
        bridge::ResolveObjectImpactSurface(*scene, meshEntity, 0) !=
            bridge::ImpactSurfaceType::Metal)
        return fail("undo does not restore absence/inherited surface");
    if (!objectSurface.Execute())
        return fail("object surface redo");
    bridge::SetObjectImpactSurfaceCommand genericSurface(
        *scene, meshEntity, bridge::ImpactSurfaceType::Default);
    if (!genericSurface.Execute() ||
        bridge::ResolveObjectImpactSurface(*scene, meshEntity, 0) !=
            bridge::ImpactSurfaceType::Default)
        return fail("explicit Default must not inherit Metal");
    genericSurface.Undo();
    if (bridge::ResolveObjectImpactSurface(*scene, meshEntity, 0) !=
            bridge::ImpactSurfaceType::Wood ||
        bridge::ApplyObjectImpactSurface(
            *scene, meshEntity, bridge::ImpactSurfaceType::Character) ||
        bridge::ApplyObjectImpactSurface(
            *scene, material, bridge::ImpactSurfaceType::Metal))
        return fail("object surface undo/authoring validation");
    surfaceHit.subsetIndex = -1;
    if (runtime::ProjectileContactSurfaceType(*scene, characters, surfaceHit, {}) !=
            bridge::ImpactSurfaceType::Wood)
        return fail("object surface must work without a mesh subset");
    if (runtime::ProjectileContactSurfaceType(*scene, characters, surfaceHit, targetId) !=
            bridge::ImpactSurfaceType::Character)
        return fail("governed Character must retain automatic classification");

    const auto assetRoot = scene->Entity_CreateTransform("Imported asset root");
    scene->Component_Attach(secondObject, assetRoot);
    bridge::SetObjectImpactSurfaceCommand assetSurface(
        *scene, assetRoot, bridge::ImpactSurfaceType::Stone);
    if (!assetSurface.Execute() ||
        bridge::ResolveObjectImpactSurface(*scene, secondObject, 0) !=
            bridge::ImpactSurfaceType::Stone ||
        bridge::ResolveObjectImpactSurface(*scene, meshEntity, 0) !=
            bridge::ImpactSurfaceType::Wood)
        return fail("asset root surface must affect only its own descendants");
    if (!bridge::ApplyObjectImpactSurface(
            *scene, secondObject, bridge::ImpactSurfaceType::Glass) ||
        bridge::ResolveObjectImpactSurface(*scene, secondObject, 0) !=
            bridge::ImpactSurfaceType::Glass)
        return fail("nearest selected object surface precedence");
    assetSurface.Undo();
    if (bridge::HasObjectImpactSurface(*scene, assetRoot))
        return fail("asset root undo must restore absence");

    // Native WISCENE archive roundtrip, not a custom persistence substitute.
    {
        auto authored = wi::allocator::make_shared<wi::scene::Scene>();
        const auto persistedObject = authored->Entity_CreateTransform("Surface roundtrip");
        authored->objects.Create(persistedObject);
        const auto persistedId = bridge::GenerateStableId();
        if (!bridge::AssignPersistentEntityId(*authored, persistedObject, persistedId, error) ||
            !bridge::ApplyObjectImpactSurface(
                *authored, persistedObject, bridge::ImpactSurfaceType::Glass))
            return fail("roundtrip source");
        wi::Archive archive;
        authored->Serialize(archive);
        archive.SetReadModeAndResetPos(true);
        auto reopened = wi::allocator::make_shared<wi::scene::Scene>();
        reopened->Serialize(archive);
        bool found = false;
        for (std::size_t i = 0; i < reopened->objects.GetCount(); ++i)
        {
            const auto entity = reopened->objects.GetEntity(i);
            if (bridge::PersistentEntityId(*reopened, entity) == persistedId)
            {
                found = bridge::ResolveObjectImpactSurface(*reopened, entity) ==
                    bridge::ImpactSurfaceType::Glass;
            }
        }
        if (!found)
            return fail("object surface lost in native scene save/reload");
    }
    surfaceHit.subsetIndex = 99;
    if (!runtime::ProjectileContactSurface(*scene, surfaceHit).empty())
        return fail("invalid subset fallback");

    scene->colliders_cpu = nullptr; // Cache belongs to this fixture.
    // The native Scene was created before the temporary collider cache/bounds.
    // Destroy it while those fixture buffers are still alive, rather than
    // relying on reverse-order local teardown after the buffers are freed.
    scene.reset();
    std::cout << "ProjectileWorldTests passed\n";
    return 0;
}
