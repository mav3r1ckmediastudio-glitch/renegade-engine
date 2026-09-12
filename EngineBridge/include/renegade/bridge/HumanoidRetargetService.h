#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    using HumanoidBone = wi::scene::HumanoidComponent::HumanoidBone;
    inline constexpr std::size_t HumanoidBoneCount =
        static_cast<std::size_t>(HumanoidBone::Count);

    struct HumanoidMappingState
    {
        bool componentExists = false;
        std::array<wi::ecs::Entity, HumanoidBoneCount> bones{};

        HumanoidMappingState() noexcept
        {
            bones.fill(wi::ecs::INVALID_ENTITY);
        }
    };

    struct HumanoidAutoMapResult
    {
        HumanoidMappingState mapping;
        std::size_t mappedBones = 0;
        bool valid = false;
        std::string error;
    };

    enum class HumanoidAnimationSourceFormat
    {
        Unknown,
        Wiscene,
        Fbx,
        Gltf,
        Glb,
        Vrm,
        Vrma,
    };

    struct HumanoidRetargetResult
    {
        bool succeeded = false;
        HumanoidAnimationSourceFormat sourceFormat =
            HumanoidAnimationSourceFormat::Unknown;
        std::string sourcePath;
        std::string error;
        std::vector<wi::ecs::Entity> createdAnimations;
    };

    // Baked retarget results must be command-owned after the first import.
    // Redo restores these snapshots instead of reopening an external source file.
    // Native ownership is persisted as parent identity and rebuilt through
    // Scene::Component_Attach(), matching Wicked's original retarget path.
    struct RetargetAnimationSnapshot
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        std::string name;
        wi::scene::AnimationComponent animation;
        wi::ecs::Entity parent = wi::ecs::INVALID_ENTITY;
    };

    struct RetargetAnimationDataSnapshot
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        wi::scene::AnimationDataComponent data;
        wi::ecs::Entity parent = wi::ecs::INVALID_ENTITY;
    };

    [[nodiscard]] const char* HumanoidBoneName(HumanoidBone bone) noexcept;

    [[nodiscard]] wi::ecs::Entity FindHumanoidRigEntity(
        const wi::scene::Scene& scene,
        wi::ecs::Entity selected) noexcept;

    [[nodiscard]] std::vector<wi::ecs::Entity> CollectArmatureBones(
        const wi::scene::Scene& scene,
        wi::ecs::Entity rigEntity);

    [[nodiscard]] HumanoidMappingState CaptureHumanoidMapping(
        const wi::scene::Scene& scene,
        wi::ecs::Entity rigEntity) noexcept;

    [[nodiscard]] bool IsHumanoidMappingValid(
        const HumanoidMappingState& mapping) noexcept;

    [[nodiscard]] HumanoidAutoMapResult BuildAutoHumanoidMapping(
        const wi::scene::Scene& scene,
        wi::ecs::Entity rigEntity);

    class SetHumanoidMappingCommand final : public ICommand
    {
    public:
        SetHumanoidMappingCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity rigEntity,
            const HumanoidMappingState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const HumanoidMappingState& mapping) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity rigEntity_ = wi::ecs::INVALID_ENTITY;
        HumanoidMappingState before_;
        HumanoidMappingState after_;
    };

    class SetHumanoidBoneCommand final : public ICommand
    {
    public:
        SetHumanoidBoneCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity rigEntity,
            HumanoidBone bone,
            wi::ecs::Entity mappedEntity);

        bool Execute() override;
        void Undo() override;

    private:
        SetHumanoidMappingCommand mappingCommand_;
    };

    [[nodiscard]] HumanoidAnimationSourceFormat ClassifyHumanoidAnimationSource(
        const std::string& path) noexcept;

    [[nodiscard]] const char* HumanoidAnimationSourceFormatName(
        HumanoidAnimationSourceFormat format) noexcept;

    class RetargetHumanoidAnimationsCommand final : public ICommand
    {
    public:
        RetargetHumanoidAnimationsCommand(
            wi::scene::Scene& destinationScene,
            wi::ecs::Entity destinationHumanoid,
            std::string sourcePath);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] const HumanoidRetargetResult& Result() const noexcept
        {
            return result_;
        }

    private:
        bool LoadSource(wi::scene::Scene& sourceScene);
        bool CapturePreparedResult();
        bool RestorePreparedResult();
        void RemoveCreated() noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity destinationHumanoid_ = wi::ecs::INVALID_ENTITY;
        std::string sourcePath_;
        HumanoidRetargetResult result_;
        bool prepared_ = false;
        std::vector<RetargetAnimationSnapshot> animationSnapshots_;
        std::vector<RetargetAnimationDataSnapshot> dataSnapshots_;
    };
}
