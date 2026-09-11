#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    enum class SpecialistComponentKind
    {
        HairParticle,
        ForceField,
        Video,
        Spline,
    };

    class CreateSpecialistComponentCommand final : public ICommand
    {
    public:
        CreateSpecialistComponentCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            SpecialistComponentKind kind);
        bool Execute() override;
        void Undo() override;

    private:
        bool Create();
        bool Remove();
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        SpecialistComponentKind kind_ = SpecialistComponentKind::ForceField;
        bool createdMaterial_ = false;
        bool createdTransform_ = false;
    };

    struct HairAtlasRectState
    {
        XMFLOAT4 texMulAdd = XMFLOAT4(1, 1, 0, 0);
        float size = 1.0f;
    };

    struct HairParticleState
    {
        wi::ecs::Entity mesh = wi::ecs::INVALID_ENTITY;
        bool cameraBend = true;
        std::uint32_t strandCount = 0;
        std::uint32_t segmentCount = 1;
        std::uint32_t billboardCount = 1;
        std::uint32_t randomSeed = 1;
        float length = 1.0f;
        float width = 1.0f;
        float stiffness = 0.5f;
        float drag = 0.1f;
        float gravity = 0.0f;
        float randomness = 0.2f;
        float viewDistance = 200.0f;
        float uniformity = 1.0f;
        std::vector<HairAtlasRectState> atlasRects;
    };

    [[nodiscard]] HairParticleState CaptureHairParticle(
        const wi::HairParticleSystem& hair);
    void ApplyHairParticle(
        wi::HairParticleSystem& hair,
        const HairParticleState& state);

    class SetHairParticleStateCommand final : public ICommand
    {
    public:
        SetHairParticleStateCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            HairParticleState state);
        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const HairParticleState& state);
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        HairParticleState before_;
        HairParticleState after_;
    };

    struct ForceFieldState
    {
        wi::scene::ForceFieldComponent::Type type =
            wi::scene::ForceFieldComponent::Type::Point;
        float gravity = 0.0f;
        float range = 10.0f;
    };

    [[nodiscard]] ForceFieldState CaptureForceField(
        const wi::scene::ForceFieldComponent& force) noexcept;
    void ApplyForceField(
        wi::scene::ForceFieldComponent& force,
        const ForceFieldState& state) noexcept;

    class SetForceFieldStateCommand final : public ICommand
    {
    public:
        SetForceFieldStateCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            ForceFieldState state);
        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const ForceFieldState& state);
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        ForceFieldState before_;
        ForceFieldState after_;
    };

    struct VideoAuthoredState
    {
        std::string filename;
        bool looped = false;
    };

    struct VideoInfo
    {
        bool loaded = false;
        bool playing = false;
        float currentTime = 0.0f;
        float duration = 0.0f;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        float framesPerSecond = 0.0f;
        std::string profile;
    };

    [[nodiscard]] VideoAuthoredState CaptureVideoAuthoredState(
        const wi::scene::VideoComponent& video);
    [[nodiscard]] VideoInfo CaptureVideoInfo(
        const wi::scene::VideoComponent& video);
    bool ApplyVideoAuthoredState(
        wi::scene::VideoComponent& video,
        const VideoAuthoredState& state);

    class SetVideoAuthoredStateCommand final : public ICommand
    {
    public:
        SetVideoAuthoredStateCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            VideoAuthoredState state);
        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const VideoAuthoredState& state);
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        VideoAuthoredState before_;
        VideoAuthoredState after_;
    };

    bool PlayVideo(wi::scene::Scene& scene, wi::ecs::Entity entity) noexcept;
    bool PauseVideo(wi::scene::Scene& scene, wi::ecs::Entity entity) noexcept;
    bool StopVideo(wi::scene::Scene& scene, wi::ecs::Entity entity) noexcept;
    bool SeekVideo(wi::scene::Scene& scene, wi::ecs::Entity entity, float seconds) noexcept;

    struct SplineState
    {
        bool looped = false;
        bool filled = false;
        bool drawAligned = false;
        float width = 1.0f;
        float rotationRadians = 0.0f;
        int horizontalSubdivisions = 0;
        int verticalSubdivisions = 0;
        float terrainModifier = 0.0f;
        float terrainTextureFalloff = 0.0f;
        float terrainPushDown = 0.0f;
        wi::scene::MeshComponent::COMPUTE_NORMALS fillNormals =
            wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH;
    };

    [[nodiscard]] SplineState CaptureSpline(
        const wi::scene::SplineComponent& spline) noexcept;
    void ApplySpline(
        wi::scene::SplineComponent& spline,
        const SplineState& state) noexcept;

    class SetSplineStateCommand final : public ICommand
    {
    public:
        SetSplineStateCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            SplineState state);
        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const SplineState& state);
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        SplineState before_;
        SplineState after_;
    };

    class AddSplineNodeCommand final : public ICommand
    {
    public:
        AddSplineNodeCommand(wi::scene::Scene& scene, wi::ecs::Entity splineEntity);
        bool Execute() override;
        void Undo() override;
        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept { return nodeEntity_; }

    private:
        bool AddExistingNode();
        void RemoveNodeReference();
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity splineEntity_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity nodeEntity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };

    class RemoveLastSplineNodeCommand final : public ICommand
    {
    public:
        RemoveLastSplineNodeCommand(wi::scene::Scene& scene, wi::ecs::Entity splineEntity);
        bool Execute() override;
        void Undo() override;

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity splineEntity_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity nodeEntity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool captured_ = false;
    };

    struct GaussianSplatInfo
    {
        bool valid = false;
        std::size_t splatCount = 0;
        int sphericalHarmonicsDegree = 0;
        std::uint64_t cpuMemoryBytes = 0;
        std::uint64_t gpuMemoryBytes = 0;
    };

    [[nodiscard]] GaussianSplatInfo CaptureGaussianSplatInfo(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;

    class ImportGaussianSplatCommand final : public ICommand
    {
    public:
        ImportGaussianSplatCommand(wi::scene::Scene& scene, std::string filename);
        bool Execute() override;
        void Undo() override;
        [[nodiscard]] wi::ecs::Entity ImportedEntity() const noexcept { return entity_; }
        [[nodiscard]] const std::string& Error() const noexcept { return error_; }

    private:
        bool Restore();
        wi::scene::Scene* scene_ = nullptr;
        std::string filename_;
        std::string error_;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };
}
