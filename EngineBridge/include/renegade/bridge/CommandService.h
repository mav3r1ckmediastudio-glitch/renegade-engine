#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <WickedEngine.h>

namespace renegade::bridge
{
    struct TransformState
    {
        XMFLOAT3 translation = {};
        XMFLOAT4 rotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        XMFLOAT3 scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
    };

    [[nodiscard]] TransformState CaptureTransform(
        const wi::scene::TransformComponent& transform) noexcept;

    struct WeatherState
    {
        enum class SkyMode
        {
            Realistic,
            RealisticWithClouds,
            Skybox,
        };

        SkyMode skyMode = SkyMode::Realistic;
        bool aerialPerspective = true;
        float skyExposure = 1.0f;
        float stars = 0.0f;
        float ambientIntensity = 0.0f;
        float fogStart = 100.0f;
        float fogDensity = 0.0f;
        bool heightFog = false;
        float fogHeightStart = 1.0f;
        float fogHeightEnd = 3.0f;
        float cloudCoverage = 1.0f;
        float cloudStartHeight = 1500.0f;
        float cloudThickness = 5000.0f;
        bool cloudsCastShadow = false;
    };

    [[nodiscard]] WeatherState CaptureWeather(
        const wi::scene::WeatherComponent& weather) noexcept;

    void ApplyWeather(
        wi::scene::WeatherComponent& weather,
        const WeatherState& state) noexcept;

    enum class WeatherPreset
    {
        Clear,
        Scattered,
        Overcast,
        Storm,
    };

    [[nodiscard]] WeatherState MakeWeatherPreset(
        const WeatherState& current,
        WeatherPreset preset) noexcept;

    [[nodiscard]] wi::ecs::Entity CreateEnvironment(
        wi::scene::Scene& scene,
        const WeatherState& weather,
        const char* name = "Environment",
        wi::ecs::Entity* createdSun = nullptr);

    class ICommand
    {
    public:
        virtual ~ICommand() = default;
        virtual bool Execute() = 0;
        virtual void Undo() = 0;
    };

    // Some creator-owned authoring state deliberately lives outside WISCENE.
    // The scene duplicate command remains the one Undo/Redo authority, while a
    // registered companion hook can atomically duplicate/restore that external
    // state (currently .rscripts) using the new persistent entity identities.
    struct DuplicateEntityCompanionCallbacks
    {
        std::function<void()> undo;
        std::function<bool()> redo;
    };

    using DuplicateEntityCompanionFactory = std::function<bool(
        wi::scene::Scene& scene,
        wi::ecs::Entity source,
        wi::ecs::Entity duplicate,
        DuplicateEntityCompanionCallbacks& callbacks,
        std::string& error)>;

    void SetDuplicateEntityCompanionFactory(
        DuplicateEntityCompanionFactory factory);
    void ClearDuplicateEntityCompanionFactory() noexcept;

    class CommandService
    {
    public:
        bool Execute(std::unique_ptr<ICommand> command);
        bool RecordExecuted(std::unique_ptr<ICommand> command);
        bool Undo();
        bool Redo();
        void Clear() noexcept;
        void MarkSaved() noexcept;
        void MarkUnsaved() noexcept
        {
            savedStateReachable_ = false;
        }

        [[nodiscard]] bool CanUndo() const noexcept;
        [[nodiscard]] bool CanRedo() const noexcept;
        [[nodiscard]] bool IsDirty() const noexcept;
        [[nodiscard]] std::size_t UndoCount() const noexcept;
        [[nodiscard]] std::size_t RedoCount() const noexcept;

    private:
        std::vector<std::unique_ptr<ICommand>> undoStack_;
        std::vector<std::unique_ptr<ICommand>> redoStack_;
        std::size_t savedDepth_ = 0;
        bool savedStateReachable_ = true;
    };

    class SetTranslationCommand final : public ICommand
    {
    public:
        SetTranslationCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const XMFLOAT3& translation);
        SetTranslationCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const XMFLOAT3& before,
            const XMFLOAT3& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const XMFLOAT3& translation);
        wi::scene::Scene* scene_;
        wi::ecs::Entity entity_;
        XMFLOAT3 before_;
        XMFLOAT3 after_;
    };

    class SetTransformCommand final : public ICommand
    {
    public:
        SetTransformCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const TransformState& transform);
        SetTransformCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const TransformState& before,
            const TransformState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(
            const TransformState& transform,
            const XMFLOAT3& expectedPreviousScale);
        wi::scene::Scene* scene_;
        wi::ecs::Entity entity_;
        TransformState before_;
        TransformState after_;
    };

    class SetWeatherCommand final : public ICommand
    {
    public:
        SetWeatherCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const WeatherState& weather);
        SetWeatherCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const WeatherState& before,
            const WeatherState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const WeatherState& state);
        wi::scene::Scene* scene_;
        wi::ecs::Entity entity_;
        WeatherState before_;
        WeatherState after_;
    };

    class CreateEnvironmentCommand final : public ICommand
    {
    public:
        CreateEnvironmentCommand(
            wi::scene::Scene& scene,
            const WeatherState& weather,
            const char* name = "Environment");

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept;

    private:
        wi::scene::Scene* scene_ = nullptr;
        WeatherState weather_;
        std::string name_;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::scene::WeatherComponent resolvedWeatherBefore_;
        wi::Archive snapshot_;
        wi::ecs::Entity createdSun_ = wi::ecs::INVALID_ENTITY;
        wi::Archive sunSnapshot_;
        bool hasSnapshot_ = false;
    };

    class DuplicateEntityCommand final : public ICommand
    {
    public:
        DuplicateEntityCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity source);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity DuplicatedEntity() const noexcept;

    private:
        wi::scene::Scene* scene_;
        wi::ecs::Entity source_;
        wi::ecs::Entity duplicate_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        DuplicateEntityCompanionCallbacks companion_;
        bool companionActive_ = false;
        bool hasSnapshot_ = false;
    };

    class DeleteEntityCommand final : public ICommand
    {
    public:
        DeleteEntityCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity);

        bool Execute() override;
        void Undo() override;

    private:
        wi::scene::Scene* scene_;
        wi::ecs::Entity entity_;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };
}
