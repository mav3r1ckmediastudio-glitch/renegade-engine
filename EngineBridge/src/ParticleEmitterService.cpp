#include "renegade/bridge/ParticleEmitterService.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/MaterialService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>

namespace
{
    constexpr float Epsilon = 0.00001f;

    bool NearlyEqual(float a, float b) noexcept
    {
        return std::abs(a - b) <= Epsilon;
    }

    float FiniteOr(float value, float fallback) noexcept
    {
        return std::isfinite(value) ? value : fallback;
    }

    bool EntityExists(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return false;
        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

    bool WouldCreateCycle(
        const wi::scene::Scene& scene,
        wi::ecs::Entity child,
        wi::ecs::Entity parent) noexcept
    {
        const std::size_t maximumDepth = scene.hierarchy.GetCount() + 1;
        for (std::size_t depth = 0;
            parent != wi::ecs::INVALID_ENTITY && depth <= maximumDepth;
            ++depth)
        {
            if (parent == child)
                return true;
            const auto* hierarchy = scene.hierarchy.GetComponent(parent);
            if (hierarchy == nullptr ||
                hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                hierarchy->parentID == parent)
                break;
            parent = hierarchy->parentID;
        }
        return false;
    }

    wi::ecs::Entity CreatorAttachmentEntity(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept
    {
        wi::ecs::Entity current = entity;
        const std::size_t maximumDepth = scene.hierarchy.GetCount() + 1;
        for (std::size_t depth = 0;
            current != wi::ecs::INVALID_ENTITY && depth <= maximumDepth;
            ++depth)
        {
            const auto* metadata = scene.metadatas.GetComponent(current);
            if (metadata != nullptr && metadata->string_values.has(
                    renegade::bridge::ReusableAssetInstanceIdMetadataKey))
                return current;

            const auto* hierarchy = scene.hierarchy.GetComponent(current);
            if (hierarchy == nullptr ||
                hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                hierarchy->parentID == current)
                break;
            current = hierarchy->parentID;
        }
        return entity;
    }

    std::string CreatorAttachmentName(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity)
    {
        const auto* name = scene.names.GetComponent(entity);
        if (name != nullptr && !name->name.empty())
            return name->name;
        return "Unnamed Entity";
    }

    bool Same3(const XMFLOAT3& a, const XMFLOAT3& b) noexcept
    {
        return NearlyEqual(a.x, b.x) && NearlyEqual(a.y, b.y) &&
            NearlyEqual(a.z, b.z);
    }

    bool Same4(const XMFLOAT4& a, const XMFLOAT4& b) noexcept
    {
        return NearlyEqual(a.x, b.x) && NearlyEqual(a.y, b.y) &&
            NearlyEqual(a.z, b.z) && NearlyEqual(a.w, b.w);
    }

    void ApplyExactLocalTransform(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const renegade::bridge::TransformState& state) noexcept
    {
        auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
            return;
        transform->translation_local = state.translation;
        transform->rotation_local = state.rotation;
        transform->scale_local = state.scale;
        transform->SetDirty();
        transform->UpdateTransform();

        const auto* hierarchy = scene.hierarchy.GetComponent(entity);
        if (hierarchy != nullptr &&
            hierarchy->parentID != wi::ecs::INVALID_ENTITY)
        {
            const auto* parent = scene.transforms.GetComponent(hierarchy->parentID);
            if (parent != nullptr)
                transform->UpdateTransform_Parented(*parent);
        }
    }
}

namespace renegade::bridge
{
    ParticleEmitterState CaptureParticleEmitter(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept
    {
        ParticleEmitterState state;
        const auto* emitter = scene.emitters.GetComponent(entity);
        if (emitter == nullptr)
            return state;

        state.shaderType = emitter->shaderType;
        state.meshId = emitter->meshID;
        state.maxParticles = emitter->GetMaxParticleCount();
        state.fixedTimestep = emitter->FIXED_TIMESTEP;
        state.size = emitter->size;
        state.randomFactor = emitter->random_factor;
        state.normalFactor = emitter->normal_factor;
        state.emitCount = emitter->count;
        state.life = emitter->life;
        state.randomLife = emitter->random_life;
        state.scaleX = emitter->scaleX;
        state.scaleY = emitter->scaleY;
        state.rotation = emitter->rotation;
        state.motionBlurAmount = emitter->motionBlurAmount;
        state.mass = emitter->mass;
        state.randomColor = emitter->random_color;
        state.opacityPeakStart = emitter->opacityCurveControlPeakStart;
        state.opacityPeakEnd = emitter->opacityCurveControlPeakEnd;
        state.burstOnCreate = emitter->burst_on_create;
        state.velocity = emitter->velocity;
        state.gravity = emitter->gravity;
        state.drag = emitter->drag;
        state.restitution = emitter->restitution;
        state.sphH = emitter->SPH_h;
        state.sphK = emitter->SPH_K;
        state.sphP0 = emitter->SPH_p0;
        state.sphE = emitter->SPH_e;
        state.framesX = emitter->framesX;
        state.framesY = emitter->framesY;
        state.frameCount = emitter->frameCount;
        state.frameStart = emitter->frameStart;
        state.frameRate = emitter->frameRate;
        state.paused = emitter->IsPaused();
        state.sorted = emitter->IsSorted();
        state.depthCollision = emitter->IsDepthCollisionEnabled();
        state.sph = emitter->IsSPHEnabled();
        state.volume = emitter->IsVolumeEnabled();
        state.frameBlending = emitter->IsFrameBlendingEnabled();
        state.collidersDisabled = emitter->IsCollidersDisabled();
        state.takeColorFromMesh = emitter->IsTakeColorFromMesh();

        if (const auto* material = scene.materials.GetComponent(entity))
        {
            const auto materialState = CaptureMaterial(*material);
            state.color = materialState.baseColor;
            state.emissiveColor = materialState.emissiveColor;
            state.emissiveStrength = materialState.emissiveStrength;
        }
        return state;
    }

    ParticleEmitterState SanitizeParticleEmitterState(
        const ParticleEmitterState& state) noexcept
    {
        ParticleEmitterState result = state;
        if (result.shaderType >= wi::EmittedParticleSystem::PARTICLESHADERTYPE_COUNT)
            result.shaderType = wi::EmittedParticleSystem::SOFT;

        result.maxParticles = std::clamp<std::uint32_t>(
            result.maxParticles, 100u, 1000000u);
        result.fixedTimestep = std::clamp(
            FiniteOr(result.fixedTimestep, -1.0f), -1.0f, 0.1f);
        result.size = std::clamp(FiniteOr(result.size, 1.0f), 0.001f, 100.0f);
        result.randomFactor = std::clamp(
            FiniteOr(result.randomFactor, 1.0f), 0.0f, 1.0f);
        result.normalFactor = std::clamp(
            FiniteOr(result.normalFactor, 1.0f), -10.0f, 10.0f);
        result.emitCount = std::clamp(
            FiniteOr(result.emitCount, 0.0f), 0.0f, 10000.0f);
        result.life = std::clamp(FiniteOr(result.life, 1.0f), 0.001f, 600.0f);
        result.randomLife = std::clamp(
            FiniteOr(result.randomLife, 1.0f), 0.0f, 10.0f);
        result.scaleX = std::clamp(FiniteOr(result.scaleX, 1.0f), 0.001f, 100.0f);
        result.scaleY = std::clamp(FiniteOr(result.scaleY, 1.0f), 0.001f, 100.0f);
        result.rotation = std::clamp(
            FiniteOr(result.rotation, 0.0f), -XM_2PI, XM_2PI);
        result.motionBlurAmount = std::clamp(
            FiniteOr(result.motionBlurAmount, 0.0f), 0.0f, 1.0f);
        result.mass = std::clamp(FiniteOr(result.mass, 1.0f), 0.001f, 1000.0f);
        result.randomColor = std::clamp(
            FiniteOr(result.randomColor, 0.0f), 0.0f, 2.0f);
        result.opacityPeakStart = std::clamp(
            FiniteOr(result.opacityPeakStart, 0.1f), 0.0f, 1.0f);
        result.opacityPeakEnd = std::clamp(
            FiniteOr(result.opacityPeakEnd, 0.5f), result.opacityPeakStart, 1.0f);
        result.burstOnCreate = std::clamp(result.burstOnCreate, 0, 100000);

        const auto sanitizeVector = [](XMFLOAT3& value, float limit)
        {
            value.x = std::clamp(FiniteOr(value.x, 0.0f), -limit, limit);
            value.y = std::clamp(FiniteOr(value.y, 0.0f), -limit, limit);
            value.z = std::clamp(FiniteOr(value.z, 0.0f), -limit, limit);
        };
        sanitizeVector(result.velocity, 10000.0f);
        sanitizeVector(result.gravity, 10000.0f);
        result.drag = std::clamp(FiniteOr(result.drag, 1.0f), 0.0f, 1.0f);
        result.restitution = std::clamp(
            FiniteOr(result.restitution, 0.98f), 0.0f, 1.0f);

        result.sphH = std::clamp(FiniteOr(result.sphH, 1.0f), 0.001f, 100.0f);
        result.sphK = std::clamp(FiniteOr(result.sphK, 250.0f), 0.0f, 10000.0f);
        result.sphP0 = std::clamp(FiniteOr(result.sphP0, 1.0f), 0.001f, 1000.0f);
        result.sphE = std::clamp(FiniteOr(result.sphE, 0.018f), 0.0f, 100.0f);

        result.framesX = std::clamp<std::uint32_t>(result.framesX, 1u, 1024u);
        result.framesY = std::clamp<std::uint32_t>(result.framesY, 1u, 1024u);
        const auto cells64 = static_cast<std::uint64_t>(result.framesX) * result.framesY;
        const auto cells = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(cells64, 1048576ull));
        result.frameCount = std::clamp<std::uint32_t>(
            result.frameCount, 1u, std::max(1u, cells));
        result.frameStart = std::min(result.frameStart, result.frameCount - 1u);
        result.frameRate = std::clamp(
            FiniteOr(result.frameRate, 0.0f), 0.0f, 240.0f);

        result.color.x = std::clamp(FiniteOr(result.color.x, 1.0f), 0.0f, 1.0f);
        result.color.y = std::clamp(FiniteOr(result.color.y, 1.0f), 0.0f, 1.0f);
        result.color.z = std::clamp(FiniteOr(result.color.z, 1.0f), 0.0f, 1.0f);
        result.color.w = std::clamp(FiniteOr(result.color.w, 1.0f), 0.0f, 1.0f);
        result.emissiveColor.x = std::clamp(FiniteOr(result.emissiveColor.x, 0.0f), 0.0f, 1.0f);
        result.emissiveColor.y = std::clamp(FiniteOr(result.emissiveColor.y, 0.0f), 0.0f, 1.0f);
        result.emissiveColor.z = std::clamp(FiniteOr(result.emissiveColor.z, 0.0f), 0.0f, 1.0f);
        result.emissiveStrength = std::clamp(
            FiniteOr(result.emissiveStrength, 0.0f), 0.0f, 100.0f);
        return result;
    }

    bool HasParticleEmitterStateChange(
        const ParticleEmitterState& before,
        const ParticleEmitterState& after) noexcept
    {
        const auto a = SanitizeParticleEmitterState(before);
        const auto b = SanitizeParticleEmitterState(after);
        return a.shaderType != b.shaderType || a.meshId != b.meshId ||
            a.maxParticles != b.maxParticles ||
            !NearlyEqual(a.fixedTimestep, b.fixedTimestep) ||
            !NearlyEqual(a.size, b.size) ||
            !NearlyEqual(a.randomFactor, b.randomFactor) ||
            !NearlyEqual(a.normalFactor, b.normalFactor) ||
            !NearlyEqual(a.emitCount, b.emitCount) ||
            !NearlyEqual(a.life, b.life) ||
            !NearlyEqual(a.randomLife, b.randomLife) ||
            !NearlyEqual(a.scaleX, b.scaleX) || !NearlyEqual(a.scaleY, b.scaleY) ||
            !NearlyEqual(a.rotation, b.rotation) ||
            !NearlyEqual(a.motionBlurAmount, b.motionBlurAmount) ||
            !NearlyEqual(a.mass, b.mass) ||
            !NearlyEqual(a.randomColor, b.randomColor) ||
            !NearlyEqual(a.opacityPeakStart, b.opacityPeakStart) ||
            !NearlyEqual(a.opacityPeakEnd, b.opacityPeakEnd) ||
            a.burstOnCreate != b.burstOnCreate ||
            !Same3(a.velocity, b.velocity) || !Same3(a.gravity, b.gravity) ||
            !NearlyEqual(a.drag, b.drag) || !NearlyEqual(a.restitution, b.restitution) ||
            !NearlyEqual(a.sphH, b.sphH) || !NearlyEqual(a.sphK, b.sphK) ||
            !NearlyEqual(a.sphP0, b.sphP0) || !NearlyEqual(a.sphE, b.sphE) ||
            a.framesX != b.framesX || a.framesY != b.framesY ||
            a.frameCount != b.frameCount || a.frameStart != b.frameStart ||
            !NearlyEqual(a.frameRate, b.frameRate) ||
            a.paused != b.paused || a.sorted != b.sorted ||
            a.depthCollision != b.depthCollision || a.sph != b.sph ||
            a.volume != b.volume || a.frameBlending != b.frameBlending ||
            a.collidersDisabled != b.collidersDisabled ||
            a.takeColorFromMesh != b.takeColorFromMesh ||
            !Same4(a.color, b.color) || !Same3(a.emissiveColor, b.emissiveColor) ||
            !NearlyEqual(a.emissiveStrength, b.emissiveStrength);
    }

    void ApplyParticleEmitter(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const ParticleEmitterState& state) noexcept
    {
        auto* emitter = scene.emitters.GetComponent(entity);
        if (emitter == nullptr)
            return;
        const auto safe = SanitizeParticleEmitterState(state);
        const bool opacityChanged =
            !NearlyEqual(emitter->opacityCurveControlPeakStart, safe.opacityPeakStart) ||
            !NearlyEqual(emitter->opacityCurveControlPeakEnd, safe.opacityPeakEnd);

        emitter->shaderType = safe.shaderType;
        emitter->meshID = safe.meshId;
        if (emitter->GetMaxParticleCount() != safe.maxParticles)
            emitter->SetMaxParticleCount(safe.maxParticles);
        emitter->FIXED_TIMESTEP = safe.fixedTimestep;
        emitter->size = safe.size;
        emitter->random_factor = safe.randomFactor;
        emitter->normal_factor = safe.normalFactor;
        emitter->count = safe.emitCount;
        emitter->life = safe.life;
        emitter->random_life = safe.randomLife;
        emitter->scaleX = safe.scaleX;
        emitter->scaleY = safe.scaleY;
        emitter->rotation = safe.rotation;
        emitter->motionBlurAmount = safe.motionBlurAmount;
        emitter->mass = safe.mass;
        emitter->random_color = safe.randomColor;
        emitter->burst_on_create = safe.burstOnCreate;
        emitter->velocity = safe.velocity;
        emitter->gravity = safe.gravity;
        emitter->drag = safe.drag;
        emitter->restitution = safe.restitution;
        emitter->SPH_h = safe.sphH;
        emitter->SPH_K = safe.sphK;
        emitter->SPH_p0 = safe.sphP0;
        emitter->SPH_e = safe.sphE;
        emitter->framesX = safe.framesX;
        emitter->framesY = safe.framesY;
        emitter->frameCount = safe.frameCount;
        emitter->frameStart = safe.frameStart;
        emitter->frameRate = safe.frameRate;
        if (opacityChanged)
            emitter->SetOpacityCurveControl(safe.opacityPeakStart, safe.opacityPeakEnd);
        emitter->SetPaused(safe.paused);
        emitter->SetSorted(safe.sorted);
        emitter->SetDepthCollisionEnabled(safe.depthCollision);
        emitter->SetSPHEnabled(safe.sph);
        emitter->SetVolumeEnabled(safe.volume);
        emitter->SetFrameBlendingEnabled(safe.frameBlending);
        emitter->SetCollidersDisabled(safe.collidersDisabled);
        emitter->SetTakeColorFromMesh(safe.takeColorFromMesh);

        if (auto* material = scene.materials.GetComponent(entity))
        {
            auto materialState = CaptureMaterial(*material);
            materialState.baseColor = safe.color;
            materialState.emissiveColor = safe.emissiveColor;
            materialState.emissiveStrength = safe.emissiveStrength;
            ApplyMaterial(*material, materialState);
        }
    }

    ParticleEmitterState MakeNewParticleEmitterState() noexcept
    {
        ParticleEmitterState state;
        state.emitCount = 20.0f;
        state.life = 2.0f;
        state.size = 0.2f;
        state.randomFactor = 0.35f;
        state.randomLife = 0.25f;
        state.normalFactor = 0.0f;
        state.velocity = XMFLOAT3(0.0f, 0.75f, 0.0f);
        state.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.9f);
        return state;
    }

    bool IsParticleEmitter(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept
    {
        return entity != wi::ecs::INVALID_ENTITY &&
            scene.emitters.GetComponent(entity) != nullptr &&
            scene.transforms.GetComponent(entity) != nullptr;
    }

    wi::ecs::Entity ParticleEmitterParent(
        const wi::scene::Scene& scene,
        wi::ecs::Entity emitter) noexcept
    {
        const auto* hierarchy = scene.hierarchy.GetComponent(emitter);
        return hierarchy != nullptr ? hierarchy->parentID : wi::ecs::INVALID_ENTITY;
    }

    std::vector<ParticleAttachmentCandidate> CollectParticleAttachmentCandidates(
        const wi::scene::Scene& scene,
        wi::ecs::Entity emitter)
    {
        std::vector<ParticleAttachmentCandidate> result;
        std::unordered_set<wi::ecs::Entity> seen;
        result.reserve(scene.transforms.GetCount());
        for (std::size_t index = 0; index < scene.transforms.GetCount(); ++index)
        {
            const auto raw = scene.transforms.GetEntity(index);
            const auto entity = CreatorAttachmentEntity(scene, raw);
            if (entity == wi::ecs::INVALID_ENTITY || entity == emitter ||
                !seen.insert(entity).second ||
                scene.transforms.GetComponent(entity) == nullptr ||
                WouldCreateCycle(scene, emitter, entity))
                continue;

            const auto* name = scene.names.GetComponent(entity);
            if (name != nullptr && name->name.rfind("__renegade_internal_", 0) == 0)
                continue;
            result.push_back({entity, CreatorAttachmentName(scene, entity)});
        }
        std::sort(result.begin(), result.end(),
            [](const ParticleAttachmentCandidate& a, const ParticleAttachmentCandidate& b)
            {
                if (a.name != b.name)
                    return a.name < b.name;
                return a.entity < b.entity;
            });
        return result;
    }

    CreateParticleEmitterCommand::CreateParticleEmitterCommand(
        wi::scene::Scene& scene,
        const XMFLOAT3& position,
        ParticleEmitterState state)
        : scene_(&scene), position_(position),
          state_(SanitizeParticleEmitterState(state))
    {
    }

    bool CreateParticleEmitterCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;
        if (hasSnapshot_)
        {
            if (EntityExists(*scene_, entity_))
                return false;
            snapshot_.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer serializer;
            serializer.allow_remap = false;
            return scene_->Entity_Serialize(snapshot_, serializer) == entity_;
        }

        entity_ = scene_->Entity_CreateEmitter(MakeUniqueName(), position_);
        if (!IsParticleEmitter(*scene_, entity_) ||
            scene_->materials.GetComponent(entity_) == nullptr)
        {
            if (entity_ != wi::ecs::INVALID_ENTITY)
                scene_->Entity_Remove(entity_);
            entity_ = wi::ecs::INVALID_ENTITY;
            return false;
        }

        std::string error;
        if (!AssignNewPersistentEntityId(*scene_, entity_, error))
        {
            scene_->Entity_Remove(entity_);
            entity_ = wi::ecs::INVALID_ENTITY;
            return false;
        }

        ApplyParticleEmitter(*scene_, entity_, state_);
        snapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer serializer;
        scene_->Entity_Serialize(snapshot_, serializer, entity_);
        hasSnapshot_ = true;
        return true;
    }

    void CreateParticleEmitterCommand::Undo()
    {
        if (scene_ != nullptr && EntityExists(*scene_, entity_))
            scene_->Entity_Remove(entity_);
    }

    wi::ecs::Entity CreateParticleEmitterCommand::CreatedEntity() const noexcept
    {
        return entity_;
    }

    std::string CreateParticleEmitterCommand::MakeUniqueName() const
    {
        const std::string base = "Particle Emitter";
        std::string candidate = base;
        int suffix = 2;
        for (;;)
        {
            bool collision = false;
            for (std::size_t i = 0; i < scene_->names.GetCount(); ++i)
            {
                if (scene_->names[i].name == candidate)
                {
                    collision = true;
                    break;
                }
            }
            if (!collision)
                return candidate;
            candidate = base + " " + std::to_string(suffix++);
        }
    }

    SetParticleEmitterCommand::SetParticleEmitterCommand(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const ParticleEmitterState& after)
        : scene_(&scene), entity_(entity),
          before_(CaptureParticleEmitter(scene, entity)),
          after_(SanitizeParticleEmitterState(after))
    {
    }

    SetParticleEmitterCommand::SetParticleEmitterCommand(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const ParticleEmitterState& before,
        const ParticleEmitterState& after)
        : scene_(&scene), entity_(entity),
          before_(SanitizeParticleEmitterState(before)),
          after_(SanitizeParticleEmitterState(after))
    {
    }

    bool SetParticleEmitterCommand::Execute()
    {
        return HasParticleEmitterStateChange(before_, after_) && Apply(after_);
    }

    void SetParticleEmitterCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool SetParticleEmitterCommand::Apply(const ParticleEmitterState& state) noexcept
    {
        if (scene_ == nullptr || !IsParticleEmitter(*scene_, entity_))
            return false;
        ApplyParticleEmitter(*scene_, entity_, state);
        return true;
    }

    SetParticleEmitterParentCommand::SetParticleEmitterParentCommand(
        wi::scene::Scene& scene,
        wi::ecs::Entity emitter,
        wi::ecs::Entity parent)
        : scene_(&scene), emitter_(emitter), requestedParent_(parent)
    {
    }

    bool SetParticleEmitterParentCommand::Execute()
    {
        if (scene_ == nullptr || !IsParticleEmitter(*scene_, emitter_))
            return false;
        if (requestedParent_ != wi::ecs::INVALID_ENTITY &&
            (!EntityExists(*scene_, requestedParent_) ||
             scene_->transforms.GetComponent(requestedParent_) == nullptr ||
             WouldCreateCycle(*scene_, emitter_, requestedParent_)))
            return false;

        if (!captured_)
        {
            beforeParent_ = ParticleEmitterParent(*scene_, emitter_);
            if (beforeParent_ == requestedParent_)
                return false;
            const auto* transform = scene_->transforms.GetComponent(emitter_);
            if (transform == nullptr)
                return false;
            beforeLocal_ = CaptureTransform(*transform);
            if (!Apply(requestedParent_, nullptr))
                return false;
            transform = scene_->transforms.GetComponent(emitter_);
            if (transform == nullptr)
                return false;
            afterLocal_ = CaptureTransform(*transform);
            captured_ = true;
            return true;
        }
        return Apply(requestedParent_, &afterLocal_);
    }

    void SetParticleEmitterParentCommand::Undo()
    {
        if (captured_)
            (void)Apply(beforeParent_, &beforeLocal_);
    }

    bool SetParticleEmitterParentCommand::Apply(
        wi::ecs::Entity parent,
        const TransformState* exactLocal) noexcept
    {
        if (scene_ == nullptr || !IsParticleEmitter(*scene_, emitter_))
            return false;
        if (parent == wi::ecs::INVALID_ENTITY)
            scene_->Component_Detach(emitter_);
        else
            scene_->Component_Attach(emitter_, parent, exactLocal != nullptr);
        if (exactLocal != nullptr)
            ApplyExactLocalTransform(*scene_, emitter_, *exactLocal);
        return true;
    }
}
