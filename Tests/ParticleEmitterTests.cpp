#include "renegade/bridge/ParticleEmitterService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"

#include <cmath>
#include <iostream>

namespace
{
    int failures = 0;
    void Check(bool condition, const char* message)
    {
        if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    }
    bool Near(float a, float b) { return std::abs(a - b) < 0.0001f; }
    bool Near3(const XMFLOAT3& a, const XMFLOAT3& b)
    { return Near(a.x,b.x) && Near(a.y,b.y) && Near(a.z,b.z); }
}

int main()
{
    using namespace renegade::bridge;

    {
        ParticleEmitterState unsafe;
        unsafe.maxParticles = 1;
        unsafe.fixedTimestep = 9.0f;
        unsafe.size = -10.0f;
        unsafe.emitCount = -4.0f;
        unsafe.life = 0.0f;
        unsafe.randomFactor = 4.0f;
        unsafe.drag = -3.0f;
        unsafe.restitution = 7.0f;
        unsafe.framesX = 0;
        unsafe.framesY = 5000;
        unsafe.frameCount = 9999999;
        unsafe.frameStart = 9999999;
        unsafe.frameRate = 900.0f;
        unsafe.opacityPeakStart = 0.8f;
        unsafe.opacityPeakEnd = 0.2f;
        const auto safe = SanitizeParticleEmitterState(unsafe);
        Check(safe.maxParticles == 100u, "particle budget clamp");
        Check(Near(safe.fixedTimestep, 0.1f), "fixed timestep clamp");
        Check(Near(safe.size, 0.001f), "size clamp");
        Check(Near(safe.emitCount, 0.0f), "emit clamp");
        Check(Near(safe.life, 0.001f), "life clamp");
        Check(safe.framesX == 1u && safe.framesY == 1024u, "sprite dimensions clamp");
        Check(safe.frameCount == 1024u && safe.frameStart == 1023u, "sprite frame clamp");
        Check(Near(safe.frameRate, 240.0f), "sprite FPS clamp");
        Check(Near(safe.opacityPeakStart, 0.8f) && Near(safe.opacityPeakEnd, 0.8f),
            "opacity curve ordering");
    }

    {
        wi::scene::Scene scene;
        const XMFLOAT3 position(7.0f, 2.0f, -3.0f);
        CreateParticleEmitterCommand command(scene, position);
        Check(command.Execute(), "create native emitter");
        const auto entity = command.CreatedEntity();
        Check(entity != wi::ecs::INVALID_ENTITY, "created entity valid");
        Check(IsParticleEmitter(scene, entity), "native emitter component present");
        Check(scene.materials.GetComponent(entity) != nullptr, "native emitter material present");
        const auto* transform = scene.transforms.GetComponent(entity);
        Check(transform != nullptr && Near3(transform->GetPosition(), position), "creation position");
        const auto identity = PersistentEntityId(scene, entity);
        Check(IsValidStableId(identity), "persistent identity assigned immediately");
        const auto state = CaptureParticleEmitter(scene, entity);
        Check(Near(state.emitCount, 20.0f) && Near(state.life, 2.0f), "creator defaults");
        command.Undo();
        Check(!IsParticleEmitter(scene, entity), "create undo");
        Check(command.Execute(), "create redo snapshot");
        Check(PersistentEntityId(scene, entity) == identity, "redo preserves identity");
    }

    {
        wi::scene::Scene scene;
        CreateParticleEmitterCommand create(scene, XMFLOAT3(0,1,0));
        Check(create.Execute(), "state fixture create");
        const auto entity = create.CreatedEntity();
        const auto before = CaptureParticleEmitter(scene, entity);
        auto after = before;
        after.shaderType = wi::EmittedParticleSystem::SOFT_LIGHTING;
        after.emitCount = 73.0f;
        after.life = 4.5f;
        after.framesX = 4; after.framesY = 4; after.frameCount = 12;
        after.frameStart = 3; after.frameRate = 18.0f;
        after.velocity = XMFLOAT3(1,2,3);
        after.gravity = XMFLOAT3(0,-9.8f,0);
        after.sorted = true; after.depthCollision = true;
        after.volume = true; after.frameBlending = true;
        after.color = XMFLOAT4(0.2f,0.4f,0.8f,0.75f);
        after.emissiveStrength = 2.0f;
        SetParticleEmitterCommand command(scene, entity, before, after);
        Check(command.Execute(), "state command execute");
        const auto applied = CaptureParticleEmitter(scene, entity);
        Check(applied.shaderType == wi::EmittedParticleSystem::SOFT_LIGHTING, "shader state");
        Check(applied.framesX == 4u && applied.framesY == 4u &&
            applied.frameCount == 12u && applied.frameStart == 3u &&
            Near(applied.frameRate,18.0f), "spritesheet state");
        Check(Near3(applied.velocity,XMFLOAT3(1,2,3)) &&
            Near3(applied.gravity,XMFLOAT3(0,-9.8f,0)), "motion state");
        Check(applied.sorted && applied.depthCollision && applied.volume && applied.frameBlending,
            "native emitter flags");
        command.Undo();
        const auto undone = CaptureParticleEmitter(scene, entity);
        Check(Near(undone.emitCount,before.emitCount) && undone.shaderType == before.shaderType,
            "state undo");
    }

    {
        wi::scene::Scene scene;
        const auto parent = scene.Entity_CreateTransform("Moving Barrel");
        auto* parentTransform = scene.transforms.GetComponent(parent);
        parentTransform->Translate(XMFLOAT3(10,0,0));
        parentTransform->UpdateTransform();
        const XMFLOAT3 world(13,2,-1);
        CreateParticleEmitterCommand create(scene, world);
        Check(create.Execute(), "attachment fixture create");
        const auto emitter = create.CreatedEntity();
        SetParticleEmitterParentCommand attach(scene, emitter, parent);
        Check(attach.Execute(), "native hierarchy attach");
        Check(ParticleEmitterParent(scene, emitter) == parent, "attachment parent");
        auto* transform = scene.transforms.GetComponent(emitter);
        Check(transform != nullptr && Near3(transform->GetPosition(), world),
            "attach preserves world position");
        const auto local = transform != nullptr ? CaptureTransform(*transform) : TransformState{};
        Check(Near3(local.translation,XMFLOAT3(3,2,-1)), "world to local offset");
        attach.Undo();
        transform = scene.transforms.GetComponent(emitter);
        Check(ParticleEmitterParent(scene, emitter) == wi::ecs::INVALID_ENTITY &&
            transform != nullptr && Near3(transform->GetPosition(),world), "attachment undo");
        Check(attach.Execute(), "attachment redo");
        transform = scene.transforms.GetComponent(emitter);
        Check(transform != nullptr && Near3(CaptureTransform(*transform).translation,local.translation),
            "attachment redo exact local offset");
    }

    {
        wi::scene::Scene scene;
        const auto wrapper = scene.Entity_CreateTransform("Crate 002");
        const auto child = scene.Entity_CreateTransform("730");
        scene.Component_Attach(child, wrapper, true);
        auto& metadata = scene.metadatas.Create(wrapper);
        metadata.string_values.set(ReusableAssetInstanceIdMetadataKey, GenerateStableId());
        CreateParticleEmitterCommand create(scene, XMFLOAT3(0,0,0));
        Check(create.Execute(), "picker fixture create");
        const auto candidates = CollectParticleAttachmentCandidates(scene, create.CreatedEntity());
        int wrapperCount = 0; bool leakedChild = false;
        for (const auto& candidate : candidates)
        {
            if (candidate.entity == wrapper && candidate.name == "Crate 002") ++wrapperCount;
            if (candidate.name == "730") leakedChild = true;
        }
        Check(wrapperCount == 1, "creator-facing reusable wrapper dedupe");
        Check(!leakedChild, "numeric Wicked child hidden from attachment picker");
    }

    if (failures != 0) return 1;
    std::cout << "PASS: native Wicked particle emitter authoring contract\n";
    return 0;
}
