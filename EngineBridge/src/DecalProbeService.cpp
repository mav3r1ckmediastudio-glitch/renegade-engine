#include "renegade/bridge/DecalProbeService.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace
{
    constexpr float Epsilon = 0.00001f;
    constexpr std::array<std::uint32_t, 7> ProbeResolutions = {
        32u, 64u, 128u, 256u, 512u, 1024u, 2048u
    };

    bool NearlyEqual(const float left, const float right) noexcept
    {
        return std::abs(left - right) <= Epsilon;
    }

    bool EntityExists(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return false;
        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

    void ApplyTransformState(
        wi::scene::TransformComponent& transform,
        const renegade::bridge::TransformState& state) noexcept
    {
        transform.translation_local = state.translation;
        transform.rotation_local = state.rotation;
        transform.scale_local = state.scale;
        transform.SetDirty();
        transform.UpdateTransform();
    }

    std::uint32_t NearestProbeResolution(const std::uint32_t value) noexcept
    {
        std::uint32_t best = ProbeResolutions.front();
        std::uint32_t bestDistance =
            value > best ? value - best : best - value;
        for (const auto candidate : ProbeResolutions)
        {
            const std::uint32_t distance =
                value > candidate ? value - candidate : candidate - value;
            if (distance < bestDistance)
            {
                best = candidate;
                bestDistance = distance;
            }
        }
        return best;
    }

    std::string MakeUniqueName(
        const wi::scene::Scene& scene,
        const std::string& base)
    {
        std::string candidate = base;
        int suffix = 2;
        bool collision = true;
        while (collision)
        {
            collision = false;
            for (std::size_t index = 0; index < scene.names.GetCount(); ++index)
            {
                if (scene.names[index].name == candidate)
                {
                    collision = true;
                    candidate = base + " " + std::to_string(suffix++);
                    break;
                }
            }
        }
        return candidate;
    }
}

namespace renegade::bridge
{
    DecalState CaptureDecal(
        const wi::scene::DecalComponent& decal) noexcept
    {
        DecalState state;
        state.baseColorOnlyAlpha = decal.IsBaseColorOnlyAlpha();
        state.slopeBlendPower = decal.slopeBlendPower;
        return state;
    }

    DecalState SanitizeDecalState(const DecalState& state) noexcept
    {
        DecalState result = state;
        result.slopeBlendPower = std::clamp(result.slopeBlendPower, 0.0f, 8.0f);
        return result;
    }

    bool HasDecalStateChange(
        const DecalState& before,
        const DecalState& after) noexcept
    {
        const auto left = SanitizeDecalState(before);
        const auto right = SanitizeDecalState(after);
        return left.baseColorOnlyAlpha != right.baseColorOnlyAlpha ||
            !NearlyEqual(left.slopeBlendPower, right.slopeBlendPower);
    }

    void ApplyDecal(
        wi::scene::DecalComponent& decal,
        const DecalState& state) noexcept
    {
        const auto safe = SanitizeDecalState(state);
        decal.SetBaseColorOnlyAlpha(safe.baseColorOnlyAlpha);
        decal.slopeBlendPower = safe.slopeBlendPower;
    }

    EnvironmentProbeState CaptureEnvironmentProbe(
        const wi::scene::EnvironmentProbeComponent& probe) noexcept
    {
        EnvironmentProbeState state;
        state.realTime = probe.IsRealTime();
        state.msaa = probe.IsMSAA();
        state.resolution = probe.resolution;
        state.updateInterval = probe.GetRealtimeUpdateInterval();
        state.viewDistance = probe.view_distance;
        return state;
    }

    EnvironmentProbeState SanitizeEnvironmentProbeState(
        const EnvironmentProbeState& state) noexcept
    {
        EnvironmentProbeState result = state;
        result.resolution = NearestProbeResolution(result.resolution);
        result.updateInterval = std::clamp(result.updateInterval, 0.0f, 60.0f);
        if (result.viewDistance < 0.0f)
            result.viewDistance = -1.0f;
        else
            result.viewDistance = std::clamp(result.viewDistance, 0.0f, 1000000.0f);
        return result;
    }

    bool HasEnvironmentProbeStateChange(
        const EnvironmentProbeState& before,
        const EnvironmentProbeState& after) noexcept
    {
        const auto left = SanitizeEnvironmentProbeState(before);
        const auto right = SanitizeEnvironmentProbeState(after);
        return left.realTime != right.realTime ||
            left.msaa != right.msaa ||
            left.resolution != right.resolution ||
            !NearlyEqual(left.updateInterval, right.updateInterval) ||
            !NearlyEqual(left.viewDistance, right.viewDistance);
    }

    void ApplyEnvironmentProbe(
        wi::scene::EnvironmentProbeComponent& probe,
        const EnvironmentProbeState& state) noexcept
    {
        const auto safe = SanitizeEnvironmentProbeState(state);
        const bool changed = HasEnvironmentProbeStateChange(
            CaptureEnvironmentProbe(probe), safe);
        const bool resolutionChanged = probe.resolution != safe.resolution;
        const bool msaaChanged = probe.IsMSAA() != safe.msaa;

        probe.SetRealTime(safe.realTime);
        probe.SetUpdateInterval(safe.updateInterval);
        probe.view_distance = safe.viewDistance;

        if (resolutionChanged)
        {
            probe.resolution = safe.resolution;
            probe.first_render = true;
        }
        if (msaaChanged)
            probe.SetMSAA(safe.msaa);
        else if (changed)
            probe.SetDirty();
    }

    bool RefreshEnvironmentProbe(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        auto* probe = scene.probes.GetComponent(entity);
        if (probe == nullptr)
            return false;
        probe->first_render = true;
        probe->SetDirty();
        return true;
    }

    CreateDecalCommand::CreateDecalCommand(
        wi::scene::Scene& scene,
        const DecalState& state,
        const TransformState& transform)
        : scene_(&scene)
        , state_(SanitizeDecalState(state))
        , transform_(transform)
    {
    }

    bool CreateDecalCommand::Execute()
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

        entity_ = scene_->Entity_CreateDecal(MakeUniqueName(), "", "");
        auto* decal = scene_->decals.GetComponent(entity_);
        auto* transform = scene_->transforms.GetComponent(entity_);
        auto* material = scene_->materials.GetComponent(entity_);
        if (entity_ == wi::ecs::INVALID_ENTITY || decal == nullptr ||
            transform == nullptr || material == nullptr)
        {
            if (entity_ != wi::ecs::INVALID_ENTITY)
                scene_->Entity_Remove(entity_);
            entity_ = wi::ecs::INVALID_ENTITY;
            return false;
        }

        ApplyDecal(*decal, state_);
        ApplyTransformState(*transform, transform_);

        snapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer serializer;
        scene_->Entity_Serialize(snapshot_, serializer, entity_);
        hasSnapshot_ = true;
        return true;
    }

    void CreateDecalCommand::Undo()
    {
        if (scene_ != nullptr && EntityExists(*scene_, entity_))
            scene_->Entity_Remove(entity_);
    }

    wi::ecs::Entity CreateDecalCommand::CreatedEntity() const noexcept
    {
        return entity_;
    }

    std::string CreateDecalCommand::MakeUniqueName() const
    {
        return ::MakeUniqueName(*scene_, "Decal");
    }

    SetDecalCommand::SetDecalCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const DecalState& state)
        : scene_(&scene)
        , entity_(entity)
        , after_(SanitizeDecalState(state))
    {
        if (const auto* existing = scene.decals.GetComponent(entity))
            before_ = CaptureDecal(*existing);
    }

    SetDecalCommand::SetDecalCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const DecalState& before,
        const DecalState& after)
        : scene_(&scene)
        , entity_(entity)
        , before_(SanitizeDecalState(before))
        , after_(SanitizeDecalState(after))
    {
    }

    bool SetDecalCommand::Execute()
    {
        return HasDecalStateChange(before_, after_) && Apply(after_);
    }

    void SetDecalCommand::Undo()
    {
        Apply(before_);
    }

    bool SetDecalCommand::Apply(const DecalState& state) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* decal = scene_->decals.GetComponent(entity_);
        if (decal == nullptr)
            return false;
        ApplyDecal(*decal, state);
        return true;
    }

    CreateEnvironmentProbeCommand::CreateEnvironmentProbeCommand(
        wi::scene::Scene& scene,
        const EnvironmentProbeState& state,
        const TransformState& transform)
        : scene_(&scene)
        , state_(SanitizeEnvironmentProbeState(state))
        , transform_(transform)
    {
    }

    bool CreateEnvironmentProbeCommand::Execute()
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

        entity_ = scene_->Entity_CreateEnvironmentProbe(
            MakeUniqueName(), transform_.translation);
        auto* probe = scene_->probes.GetComponent(entity_);
        auto* transform = scene_->transforms.GetComponent(entity_);
        if (entity_ == wi::ecs::INVALID_ENTITY || probe == nullptr ||
            transform == nullptr)
        {
            if (entity_ != wi::ecs::INVALID_ENTITY)
                scene_->Entity_Remove(entity_);
            entity_ = wi::ecs::INVALID_ENTITY;
            return false;
        }

        ApplyEnvironmentProbe(*probe, state_);
        ApplyTransformState(*transform, transform_);

        snapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer serializer;
        scene_->Entity_Serialize(snapshot_, serializer, entity_);
        hasSnapshot_ = true;
        return true;
    }

    void CreateEnvironmentProbeCommand::Undo()
    {
        if (scene_ != nullptr && EntityExists(*scene_, entity_))
            scene_->Entity_Remove(entity_);
    }

    wi::ecs::Entity CreateEnvironmentProbeCommand::CreatedEntity() const noexcept
    {
        return entity_;
    }

    std::string CreateEnvironmentProbeCommand::MakeUniqueName() const
    {
        return ::MakeUniqueName(*scene_, "Environment Probe");
    }

    SetEnvironmentProbeCommand::SetEnvironmentProbeCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const EnvironmentProbeState& state)
        : scene_(&scene)
        , entity_(entity)
        , after_(SanitizeEnvironmentProbeState(state))
    {
        if (const auto* existing = scene.probes.GetComponent(entity))
            before_ = CaptureEnvironmentProbe(*existing);
    }

    SetEnvironmentProbeCommand::SetEnvironmentProbeCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const EnvironmentProbeState& before,
        const EnvironmentProbeState& after)
        : scene_(&scene)
        , entity_(entity)
        , before_(SanitizeEnvironmentProbeState(before))
        , after_(SanitizeEnvironmentProbeState(after))
    {
    }

    bool SetEnvironmentProbeCommand::Execute()
    {
        return HasEnvironmentProbeStateChange(before_, after_) && Apply(after_);
    }

    void SetEnvironmentProbeCommand::Undo()
    {
        Apply(before_);
    }

    bool SetEnvironmentProbeCommand::Apply(
        const EnvironmentProbeState& state) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* probe = scene_->probes.GetComponent(entity_);
        if (probe == nullptr)
            return false;
        ApplyEnvironmentProbe(*probe, state);
        return true;
    }
}
