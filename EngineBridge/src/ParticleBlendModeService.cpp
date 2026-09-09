#include "renegade/bridge/ParticleBlendModeService.h"

namespace renegade::bridge
{
    wi::enums::BLENDMODE CaptureParticleBlendMode(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity emitter) noexcept
    {
        const auto* material = scene.materials.GetComponent(emitter);
        if (material == nullptr)
            return wi::enums::BLENDMODE_ALPHA;
        return material->GetBlendMode();
    }

    bool IsSupportedParticleBlendMode(
        const wi::enums::BLENDMODE blendMode) noexcept
    {
        return blendMode >= wi::enums::BLENDMODE_OPAQUE &&
            blendMode < wi::enums::BLENDMODE_COUNT;
    }

    SetParticleBlendModeCommand::SetParticleBlendModeCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity emitter,
        const wi::enums::BLENDMODE blendMode)
        : scene_(&scene), emitter_(emitter)
    {
        if (const auto* material = scene.materials.GetComponent(emitter))
        {
            before_ = CaptureMaterial(*material);
            after_ = before_;
            after_.blendMode = IsSupportedParticleBlendMode(blendMode)
                ? blendMode
                : wi::enums::BLENDMODE_ALPHA;
            after_ = SanitizeMaterialState(after_);
        }
    }

    bool SetParticleBlendModeCommand::Execute()
    {
        return HasMaterialStateChange(before_, after_) && Apply(after_);
    }

    void SetParticleBlendModeCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool SetParticleBlendModeCommand::Apply(const MaterialState& state) noexcept
    {
        if (scene_ == nullptr || emitter_ == wi::ecs::INVALID_ENTITY ||
            scene_->emitters.GetComponent(emitter_) == nullptr)
            return false;

        auto* material = scene_->materials.GetComponent(emitter_);
        if (material == nullptr)
            return false;

        ApplyMaterial(*material, state);
        return true;
    }
}
