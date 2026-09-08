#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    // Creator-facing view of Wicked's native EmittedParticleSystem. This is
    // deliberately an authoring wrapper around the pinned Wicked emitter;
    // Renegade does not own a competing particle simulation or serialization.
    struct ParticleEmitterState
    {
        wi::EmittedParticleSystem::PARTICLESHADERTYPE shaderType =
            wi::EmittedParticleSystem::SOFT;
        wi::ecs::Entity meshId = wi::ecs::INVALID_ENTITY;
        std::uint32_t maxParticles = 1000;

        float fixedTimestep = -1.0f;
        float size = 1.0f;
        float randomFactor = 1.0f;
        float normalFactor = 1.0f;
        float emitCount = 0.0f;
        float life = 1.0f;
        float randomLife = 1.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float rotation = 0.0f;
        float motionBlurAmount = 0.0f;
        float mass = 1.0f;
        float randomColor = 0.0f;
        float opacityPeakStart = 0.1f;
        float opacityPeakEnd = 0.5f;
        int burstOnCreate = 0;

        XMFLOAT3 velocity = {};
        XMFLOAT3 gravity = {};
        float drag = 1.0f;
        float restitution = 0.98f;

        float sphH = 1.0f;
        float sphK = 250.0f;
        float sphP0 = 1.0f;
        float sphE = 0.018f;

        std::uint32_t framesX = 1;
        std::uint32_t framesY = 1;
        std::uint32_t frameCount = 1;
        std::uint32_t frameStart = 0;
        float frameRate = 0.0f;

        bool paused = false;
        bool sorted = false;
        bool depthCollision = false;
        bool sph = false;
        bool volume = false;
        bool frameBlending = false;
        bool collidersDisabled = false;
        bool takeColorFromMesh = false;

        XMFLOAT4 color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        XMFLOAT3 emissiveColor = XMFLOAT3(0.0f, 0.0f, 0.0f);
        float emissiveStrength = 0.0f;
    };

    [[nodiscard]] ParticleEmitterState CaptureParticleEmitter(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;
    [[nodiscard]] ParticleEmitterState SanitizeParticleEmitterState(
        const ParticleEmitterState& state) noexcept;
    [[nodiscard]] bool HasParticleEmitterStateChange(
        const ParticleEmitterState& before,
        const ParticleEmitterState& after) noexcept;
    void ApplyParticleEmitter(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const ParticleEmitterState& state) noexcept;

    [[nodiscard]] ParticleEmitterState MakeNewParticleEmitterState() noexcept;
    [[nodiscard]] bool IsParticleEmitter(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;

    struct ParticleAttachmentCandidate
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        std::string name;
    };

    [[nodiscard]] std::vector<ParticleAttachmentCandidate>
    CollectParticleAttachmentCandidates(
        const wi::scene::Scene& scene,
        wi::ecs::Entity emitter);
    [[nodiscard]] wi::ecs::Entity ParticleEmitterParent(
        const wi::scene::Scene& scene,
        wi::ecs::Entity emitter) noexcept;

    class CreateParticleEmitterCommand final : public ICommand
    {
    public:
        CreateParticleEmitterCommand(
            wi::scene::Scene& scene,
            const XMFLOAT3& position,
            ParticleEmitterState state = MakeNewParticleEmitterState());
        bool Execute() override;
        void Undo() override;
        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept;
    private:
        [[nodiscard]] std::string MakeUniqueName() const;
        wi::scene::Scene* scene_ = nullptr;
        XMFLOAT3 position_ = {};
        ParticleEmitterState state_;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };

    class SetParticleEmitterCommand final : public ICommand
    {
    public:
        SetParticleEmitterCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const ParticleEmitterState& after);
        SetParticleEmitterCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const ParticleEmitterState& before,
            const ParticleEmitterState& after);
        bool Execute() override;
        void Undo() override;
    private:
        bool Apply(const ParticleEmitterState& state) noexcept;
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        ParticleEmitterState before_;
        ParticleEmitterState after_;
    };

    class SetParticleEmitterParentCommand final : public ICommand
    {
    public:
        SetParticleEmitterParentCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity emitter,
            wi::ecs::Entity parent);
        bool Execute() override;
        void Undo() override;
    private:
        bool Apply(wi::ecs::Entity parent, const TransformState* exactLocal) noexcept;
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity emitter_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity requestedParent_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity beforeParent_ = wi::ecs::INVALID_ENTITY;
        TransformState beforeLocal_;
        TransformState afterLocal_;
        bool captured_ = false;
    };
}
