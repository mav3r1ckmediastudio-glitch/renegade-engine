#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    // Phase 5 Gate 1 treats the stable reusable-asset wrapper as the creator's
    // scene entity. Selecting any imported descendant therefore resolves back
    // to that root for Name, Layer and Metadata authoring.
    [[nodiscard]] wi::ecs::Entity ResolveSceneComponentAuthoringRoot(
        const wi::scene::Scene& scene,
        wi::ecs::Entity selected) noexcept;

    struct SceneLayerMaskState
    {
        std::uint32_t mask = ~0u;
        std::size_t targetCount = 0;
        bool mixed = false;
    };

    // A normal entity contributes one target. A reusable asset contributes its
    // stable root plus every descendant ObjectComponent so render filtering and
    // root-owned systems observe the same creator-authored mask.
    [[nodiscard]] SceneLayerMaskState InspectSceneLayerMask(
        const wi::scene::Scene& scene,
        wi::ecs::Entity selected);

    enum class ObjectParticipationProperty
    {
        Renderable,
        CastShadow,
        Foreground,
        VisibleInMainCamera,
        VisibleInReflections,
        Wetmap,
    };

    struct ObjectParticipationState
    {
        bool value = false;
        std::size_t targetCount = 0;
        bool mixed = false;
    };

    // For a reusable root, object participation is a whole-asset operation over
    // all descendant render ObjectComponents. Ordinary entities target only
    // their own ObjectComponent.
    [[nodiscard]] ObjectParticipationState InspectObjectParticipation(
        const wi::scene::Scene& scene,
        wi::ecs::Entity selected,
        ObjectParticipationProperty property);

    class SetSceneNameCommand final : public ICommand
    {
    public:
        SetSceneNameCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity selected,
            std::string name);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity TargetEntity() const noexcept
        {
            return entity_;
        }

    private:
        bool Apply(const std::string& value);

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        std::string before_;
        std::string after_;
        bool hadName_ = false;
    };

    class SetSceneLayerMaskCommand final : public ICommand
    {
    public:
        SetSceneLayerMaskCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity selected,
            std::uint32_t mask);

        bool Execute() override;
        void Undo() override;

    private:
        struct TargetState
        {
            wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
            std::uint32_t mask = ~0u;
            std::uint32_t hierarchyBind = ~0u;
            bool hadLayer = false;
            bool hadHierarchy = false;
        };

        bool Apply(std::uint32_t mask);

        wi::scene::Scene* scene_ = nullptr;
        std::vector<TargetState> before_;
        std::uint32_t after_ = ~0u;
    };

    class SetMetadataPresetCommand final : public ICommand
    {
    public:
        SetMetadataPresetCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity selected,
            wi::scene::MetadataComponent::Preset preset);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(wi::scene::MetadataComponent::Preset preset);

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::scene::MetadataComponent::Preset before_ =
            wi::scene::MetadataComponent::Preset::Custom;
        wi::scene::MetadataComponent::Preset after_ =
            wi::scene::MetadataComponent::Preset::Custom;
        bool hadMetadata_ = false;
    };

    class SetObjectParticipationCommand final : public ICommand
    {
    public:
        SetObjectParticipationCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity selected,
            ObjectParticipationProperty property,
            bool value);

        bool Execute() override;
        void Undo() override;

    private:
        struct TargetState
        {
            wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
            bool value = false;
        };

        bool Apply(bool value);

        wi::scene::Scene* scene_ = nullptr;
        ObjectParticipationProperty property_ =
            ObjectParticipationProperty::Renderable;
        std::vector<TargetState> before_;
        bool after_ = false;
    };
}
