#include "renegade/bridge/CommandService.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    constexpr float transformEpsilon = 0.00001f;

    bool NearlyEqual(const float before, const float after) noexcept
    {
        return std::abs(after - before) <= transformEpsilon;
    }

    bool IsMeaningfulTranslation(
        const XMFLOAT3& before,
        const XMFLOAT3& after) noexcept
    {
        return !NearlyEqual(after.x, before.x) ||
            !NearlyEqual(after.y, before.y) ||
            !NearlyEqual(after.z, before.z);
    }

    bool IsMeaningfulTransform(
        const renegade::bridge::TransformState& before,
        const renegade::bridge::TransformState& after) noexcept
    {
        return IsMeaningfulTranslation(
                before.translation,
                after.translation) ||
            !NearlyEqual(before.rotation.x, after.rotation.x) ||
            !NearlyEqual(before.rotation.y, after.rotation.y) ||
            !NearlyEqual(before.rotation.z, after.rotation.z) ||
            !NearlyEqual(before.rotation.w, after.rotation.w) ||
            !NearlyEqual(before.scale.x, after.scale.x) ||
            !NearlyEqual(before.scale.y, after.scale.y) ||
            !NearlyEqual(before.scale.z, after.scale.z);
    }

    bool IsMeaningfulWeather(
        const renegade::bridge::WeatherState& before,
        const renegade::bridge::WeatherState& after) noexcept
    {
        return before.skyMode != after.skyMode ||
            before.aerialPerspective != after.aerialPerspective ||
            !NearlyEqual(before.skyExposure, after.skyExposure) ||
            !NearlyEqual(before.ambientIntensity, after.ambientIntensity) ||
            !NearlyEqual(before.fogStart, after.fogStart) ||
            !NearlyEqual(before.fogDensity, after.fogDensity) ||
            before.heightFog != after.heightFog ||
            !NearlyEqual(before.fogHeightStart, after.fogHeightStart) ||
            !NearlyEqual(before.fogHeightEnd, after.fogHeightEnd) ||
            !NearlyEqual(before.cloudCoverage, after.cloudCoverage) ||
            !NearlyEqual(before.cloudStartHeight, after.cloudStartHeight) ||
            !NearlyEqual(before.cloudThickness, after.cloudThickness) ||
            before.cloudsCastShadow != after.cloudsCastShadow;
    }

    bool EntityExists(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }

        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }
}

namespace renegade::bridge
{
    TransformState CaptureTransform(
        const wi::scene::TransformComponent& transform) noexcept
    {
        TransformState state;
        state.translation = transform.translation_local;
        state.rotation = transform.rotation_local;
        state.scale = transform.scale_local;
        return state;
    }

    WeatherState CaptureWeather(
        const wi::scene::WeatherComponent& weather) noexcept
    {
        WeatherState state;

        if (weather.IsVolumetricClouds())
        {
            state.skyMode = WeatherState::SkyMode::RealisticWithClouds;
        }
        else if (weather.IsRealisticSky())
        {
            state.skyMode = WeatherState::SkyMode::Realistic;
        }
        else
        {
            state.skyMode = WeatherState::SkyMode::Skybox;
        }

        state.aerialPerspective = weather.IsRealisticSkyAerialPerspective();
        state.skyExposure = weather.skyExposure;

        // Ambient is authored as a neutral scalar. Renegade keeps the cool
        // tint it generates and scales it, rather than making the creator
        // balance three channels by hand.
        state.ambientIntensity = std::max(
            weather.ambient.x,
            std::max(weather.ambient.y, weather.ambient.z));

        state.fogStart = weather.fogStart;
        state.fogDensity = weather.fogDensity;
        state.heightFog = weather.IsHeightFog();
        state.fogHeightStart = weather.fogHeightStart;
        state.fogHeightEnd = weather.fogHeightEnd;

        state.cloudCoverage =
            weather.volumetricCloudParameters.layerFirst.coverageAmount;
        state.cloudStartHeight =
            weather.volumetricCloudParameters.cloudStartHeight;
        state.cloudThickness =
            weather.volumetricCloudParameters.cloudThickness;
        state.cloudsCastShadow = weather.IsVolumetricCloudsCastShadow();

        return state;
    }

    void ApplyWeather(
        wi::scene::WeatherComponent& weather,
        const WeatherState& state) noexcept
    {
        const bool realisticSky =
            state.skyMode != WeatherState::SkyMode::Skybox;
        const bool clouds =
            state.skyMode == WeatherState::SkyMode::RealisticWithClouds;

        weather.SetRealisticSky(realisticSky);
        weather.SetRealisticSkyAerialPerspective(
            realisticSky && state.aerialPerspective);
        weather.SetVolumetricClouds(clouds);
        weather.SetVolumetricCloudsCastShadow(clouds && state.cloudsCastShadow);

        weather.skyExposure = state.skyExposure;

        // Preserve the authored ambient hue, rescaled to the requested
        // intensity. Falling back to a neutral grey keeps a fully black
        // ambient recoverable.
        const float previousPeak = std::max(
            weather.ambient.x,
            std::max(weather.ambient.y, weather.ambient.z));
        if (previousPeak > 1e-6f)
        {
            const float scale = state.ambientIntensity / previousPeak;
            weather.ambient.x *= scale;
            weather.ambient.y *= scale;
            weather.ambient.z *= scale;
        }
        else
        {
            weather.ambient = XMFLOAT3(
                state.ambientIntensity,
                state.ambientIntensity,
                state.ambientIntensity);
        }

        weather.fogStart = state.fogStart;
        weather.fogDensity = state.fogDensity;
        weather.SetHeightFog(state.heightFog);
        weather.fogHeightStart = state.fogHeightStart;
        weather.fogHeightEnd = state.fogHeightEnd;

        // The curated coverage control authors the primary layer only. The
        // advanced second layer is deliberately left untouched until Renegade
        // exposes it, matching the preservation contract of WeatherState.
        weather.volumetricCloudParameters.layerFirst.coverageAmount =
            state.cloudCoverage;
        weather.volumetricCloudParameters.cloudStartHeight =
            state.cloudStartHeight;
        weather.volumetricCloudParameters.cloudThickness =
            state.cloudThickness;
    }

    WeatherState MakeWeatherPreset(
        const WeatherState& current,
        const WeatherPreset preset) noexcept
    {
        WeatherState result = current;
        switch (preset)
        {
        case WeatherPreset::Clear:
            result.skyMode = WeatherState::SkyMode::Realistic;
            result.skyExposure = 1.0f;
            result.ambientIntensity = 0.10f;
            result.fogStart = 100.0f;
            result.fogDensity = 0.002f;
            result.heightFog = false;
            result.cloudCoverage = 0.05f;
            result.cloudsCastShadow = false;
            break;
        case WeatherPreset::Scattered:
            result.skyMode = WeatherState::SkyMode::RealisticWithClouds;
            result.skyExposure = 0.95f;
            result.ambientIntensity = 0.085f;
            result.fogStart = 70.0f;
            result.fogDensity = 0.004f;
            result.cloudCoverage = 0.35f;
            result.cloudStartHeight = 1800.0f;
            result.cloudThickness = 3500.0f;
            result.cloudsCastShadow = true;
            break;
        case WeatherPreset::Overcast:
            result.skyMode = WeatherState::SkyMode::RealisticWithClouds;
            result.skyExposure = 0.75f;
            result.ambientIntensity = 0.065f;
            result.fogStart = 35.0f;
            result.fogDensity = 0.012f;
            result.heightFog = true;
            result.fogHeightStart = -1.0f;
            result.fogHeightEnd = 8.0f;
            result.cloudCoverage = 0.78f;
            result.cloudStartHeight = 1200.0f;
            result.cloudThickness = 5000.0f;
            result.cloudsCastShadow = true;
            break;
        case WeatherPreset::Storm:
            result.skyMode = WeatherState::SkyMode::RealisticWithClouds;
            result.skyExposure = 0.55f;
            result.ambientIntensity = 0.035f;
            result.fogStart = 18.0f;
            result.fogDensity = 0.025f;
            result.heightFog = true;
            result.fogHeightStart = -2.0f;
            result.fogHeightEnd = 12.0f;
            result.cloudCoverage = 0.95f;
            result.cloudStartHeight = 750.0f;
            result.cloudThickness = 6500.0f;
            result.cloudsCastShadow = true;
            break;
        }
        return result;
    }

    SetWeatherCommand::SetWeatherCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const WeatherState& weather)
        : scene_(&scene)
        , entity_(entity)
        , after_(weather)
    {
        const auto* existing = scene.weathers.GetComponent(entity);
        before_ = existing == nullptr
            ? WeatherState{}
            : CaptureWeather(*existing);
    }

    SetWeatherCommand::SetWeatherCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const WeatherState& before,
        const WeatherState& after)
        : scene_(&scene)
        , entity_(entity)
        , before_(before)
        , after_(after)
    {
    }

    bool SetWeatherCommand::Execute()
    {
        if (!IsMeaningfulWeather(before_, after_))
        {
            return false;
        }
        return Apply(after_);
    }

    void SetWeatherCommand::Undo()
    {
        Apply(before_);
    }

    bool SetWeatherCommand::Apply(const WeatherState& state)
    {
        if (scene_ == nullptr)
        {
            return false;
        }

        auto* weather = scene_->weathers.GetComponent(entity_);
        if (weather == nullptr)
        {
            return false;
        }

        ApplyWeather(*weather, state);

        // Scene::weather is a resolved runtime copy that RunWeatherUpdateSystem
        // refreshes from weathers[0] each frame. Updating it here means the
        // change is visible on the very next frame rather than one late.
        if (scene_->weathers.GetCount() > 0 &&
            scene_->weathers.GetEntity(0) == entity_)
        {
            scene_->weather = *weather;
        }

        return true;
    }

    bool CommandService::Execute(std::unique_ptr<ICommand> command)
    {
        if (command == nullptr || !command->Execute())
        {
            return false;
        }

        undoStack_.push_back(std::move(command));
        redoStack_.clear();
        return true;
    }

    bool CommandService::Undo()
    {
        if (undoStack_.empty())
        {
            return false;
        }

        auto command = std::move(undoStack_.back());
        undoStack_.pop_back();
        command->Undo();
        redoStack_.push_back(std::move(command));
        return true;
    }

    bool CommandService::Redo()
    {
        if (redoStack_.empty())
        {
            return false;
        }

        auto command = std::move(redoStack_.back());
        redoStack_.pop_back();
        if (!command->Execute())
        {
            redoStack_.push_back(std::move(command));
            return false;
        }

        undoStack_.push_back(std::move(command));
        return true;
    }

    void CommandService::Clear() noexcept
    {
        undoStack_.clear();
        redoStack_.clear();
    }

    bool CommandService::CanUndo() const noexcept
    {
        return !undoStack_.empty();
    }

    bool CommandService::CanRedo() const noexcept
    {
        return !redoStack_.empty();
    }

    std::size_t CommandService::UndoCount() const noexcept
    {
        return undoStack_.size();
    }

    std::size_t CommandService::RedoCount() const noexcept
    {
        return redoStack_.size();
    }

    SetTranslationCommand::SetTranslationCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& translation)
        : scene_(&scene)
        , entity_(entity)
        , after_(translation)
    {
        const auto* transform = scene.transforms.GetComponent(entity);
        before_ = transform == nullptr ? XMFLOAT3{} : transform->translation_local;
    }

    SetTranslationCommand::SetTranslationCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& before,
        const XMFLOAT3& after)
        : scene_(&scene)
        , entity_(entity)
        , before_(before)
        , after_(after)
    {
    }

    bool SetTranslationCommand::Execute()
    {
        if (!IsMeaningfulTranslation(before_, after_))
        {
            return false;
        }
        return Apply(after_);
    }

    void SetTranslationCommand::Undo()
    {
        Apply(before_);
    }

    bool SetTranslationCommand::Apply(const XMFLOAT3& translation)
    {
        auto* transform = scene_->transforms.GetComponent(entity_);
        if (transform == nullptr)
        {
            return false;
        }

        transform->translation_local = translation;
        transform->SetDirty();
        transform->UpdateTransform();
        return true;
    }

    SetTransformCommand::SetTransformCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const TransformState& transform)
        : scene_(&scene)
        , entity_(entity)
        , after_(transform)
    {
        const auto* existing = scene.transforms.GetComponent(entity);
        before_ = existing == nullptr
            ? TransformState{}
            : CaptureTransform(*existing);
    }

    SetTransformCommand::SetTransformCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const TransformState& before,
        const TransformState& after)
        : scene_(&scene)
        , entity_(entity)
        , before_(before)
        , after_(after)
    {
    }

    bool SetTransformCommand::Execute()
    {
        if (!IsMeaningfulTransform(before_, after_))
        {
            return false;
        }
        return Apply(after_);
    }

    void SetTransformCommand::Undo()
    {
        Apply(before_);
    }

    bool SetTransformCommand::Apply(const TransformState& transformState)
    {
        auto* transform = scene_->transforms.GetComponent(entity_);
        if (transform == nullptr)
        {
            return false;
        }

        transform->translation_local = transformState.translation;
        transform->rotation_local = transformState.rotation;
        transform->scale_local = transformState.scale;
        transform->SetDirty();
        transform->UpdateTransform();
        return true;
    }

    DuplicateEntityCommand::DuplicateEntityCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity source)
        : scene_(&scene)
        , source_(source)
    {
    }

    bool DuplicateEntityCommand::Execute()
    {
        if (!hasSnapshot_)
        {
            if (!EntityExists(*scene_, source_))
            {
                return false;
            }

            duplicate_ = scene_->Entity_Duplicate(source_);
            if (duplicate_ == wi::ecs::INVALID_ENTITY)
            {
                return false;
            }

            if (auto* name = scene_->names.GetComponent(duplicate_))
            {
                name->name += " Copy";
            }

            snapshot_.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(snapshot_, serializer, duplicate_);
            hasSnapshot_ = true;
            return true;
        }

        if (EntityExists(*scene_, duplicate_))
        {
            return false;
        }

        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        const auto restored =
            scene_->Entity_Serialize(snapshot_, serializer);
        return restored == duplicate_;
    }

    void DuplicateEntityCommand::Undo()
    {
        if (EntityExists(*scene_, duplicate_))
        {
            scene_->Entity_Remove(duplicate_);
        }
    }

    wi::ecs::Entity DuplicateEntityCommand::DuplicatedEntity() const noexcept
    {
        return duplicate_;
    }

    DeleteEntityCommand::DeleteEntityCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
        : scene_(&scene)
        , entity_(entity)
    {
    }

    bool DeleteEntityCommand::Execute()
    {
        if (!EntityExists(*scene_, entity_))
        {
            return false;
        }

        if (!hasSnapshot_)
        {
            snapshot_.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(snapshot_, serializer, entity_);
            hasSnapshot_ = true;
        }

        scene_->Entity_Remove(entity_);
        return true;
    }

    void DeleteEntityCommand::Undo()
    {
        if (!hasSnapshot_ || EntityExists(*scene_, entity_))
        {
            return;
        }

        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        scene_->Entity_Serialize(snapshot_, serializer);
    }
}
