#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    // Appearance-only subset of Wicked's native HairParticleSystem. This is
    // intentionally isolated from VegetationService so creator-facing tuning
    // cannot rewrite the owner-approved paint/live-mask path.
    struct VegetationAppearanceSettings
    {
        float length = 1.0f;
        float width = 1.0f;
        float randomness = 0.2f;
        std::uint32_t randomSeed = 1;
        float uniformity = 1.0f;
    };

    [[nodiscard]] inline VegetationAppearanceSettings
    CaptureVegetationAppearanceSettings(
        const wi::terrain::Terrain& terrain) noexcept
    {
        VegetationAppearanceSettings settings;
        settings.length = terrain.grass_properties.length;
        settings.width = terrain.grass_properties.width;
        settings.randomness = terrain.grass_properties.randomness;
        settings.randomSeed = terrain.grass_properties.randomSeed;
        settings.uniformity = terrain.grass_properties.uniformity;
        return settings;
    }

    [[nodiscard]] inline bool VegetationAppearanceSettingsEqual(
        const VegetationAppearanceSettings& left,
        const VegetationAppearanceSettings& right) noexcept
    {
        constexpr float epsilon = 0.0001f;
        return std::abs(left.length - right.length) <= epsilon &&
            std::abs(left.width - right.width) <= epsilon &&
            std::abs(left.randomness - right.randomness) <= epsilon &&
            left.randomSeed == right.randomSeed &&
            std::abs(left.uniformity - right.uniformity) <= epsilon;
    }

    inline void ApplyAppearanceToHair(
        wi::HairParticleSystem& hair,
        const VegetationAppearanceSettings& settings)
    {
        hair.length = settings.length;
        hair.width = settings.width;
        hair.randomness = settings.randomness;
        hair.randomSeed = settings.randomSeed;
        hair.uniformity = settings.uniformity;
        hair.regenerate_frame = true;
        hair.SetDirty();
    }

    inline bool ApplyVegetationAppearanceSettings(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        VegetationAppearanceSettings settings)
    {
        settings.length = std::clamp(settings.length, 0.0f, 4.0f);
        settings.width = std::clamp(settings.width, 0.0f, 2.0f);
        settings.randomness = std::clamp(settings.randomness, 0.0f, 1.0f);
        settings.randomSeed = std::clamp<std::uint32_t>(
            settings.randomSeed, 1u, 12345u);
        settings.uniformity = std::clamp(settings.uniformity, 0.01f, 2.0f);

        const auto before = CaptureVegetationAppearanceSettings(terrain);
        if (VegetationAppearanceSettingsEqual(before, settings))
            return false;

        ApplyAppearanceToHair(terrain.grass_properties, settings);

        if (terrain.grassEntity != wi::ecs::INVALID_ENTITY)
        {
            if (auto* master = scene.hairs.GetComponent(terrain.grassEntity))
                ApplyAppearanceToHair(*master, settings);
        }

        for (auto& entry : terrain.chunks)
        {
            auto& chunk = entry.second;
            ApplyAppearanceToHair(chunk.grass, settings);

            if (chunk.grass_entity == wi::ecs::INVALID_ENTITY)
                continue;
            if (auto* live = scene.hairs.GetComponent(chunk.grass_entity))
                ApplyAppearanceToHair(*live, settings);
        }

        return true;
    }

    class SetVegetationAppearanceSettingsCommand final : public ICommand
    {
    public:
        SetVegetationAppearanceSettingsCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity terrainEntity,
            const VegetationAppearanceSettings before,
            const VegetationAppearanceSettings after)
            : scene_(&scene)
            , terrainEntity_(terrainEntity)
            , before_(before)
            , after_(after)
        {
        }

        bool Execute() override
        {
            return Apply(after_);
        }

        void Undo() override
        {
            Apply(before_);
        }

    private:
        bool Apply(const VegetationAppearanceSettings& settings)
        {
            if (scene_ == nullptr)
                return false;
            auto* terrain = scene_->terrains.GetComponent(terrainEntity_);
            return terrain != nullptr &&
                ApplyVegetationAppearanceSettings(*scene_, *terrain, settings);
        }

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity terrainEntity_ = wi::ecs::INVALID_ENTITY;
        VegetationAppearanceSettings before_;
        VegetationAppearanceSettings after_;
    };
}
