#pragma once

#include <cstddef>
#include <memory>
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

    // A curated view of WeatherComponent.
    //
    // WeatherComponent carries well over a hundred values once the two
    // volumetric cloud layers are counted. Exposing them raw would be
    // unusable, and capturing them all for Undo would make every slider drag
    // copy kilobytes. This is the set a creator actually reaches for; anything
    // absent here is left untouched by ApplyWeather, so authored values that
    // Renegade does not yet surface are never destroyed by an edit.
    struct WeatherState
    {
        enum class SkyMode
        {
            Realistic,            // physically based atmosphere
            RealisticWithClouds,  // atmosphere plus volumetric clouds
            Skybox,               // skyMap texture, cheapest
        };

        SkyMode skyMode = SkyMode::Realistic;
        bool aerialPerspective = true;

        float skyExposure = 1.0f;
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

    // Applies only the fields WeatherState covers. Everything else on the
    // component - atmosphere parameters, sky map name, wind, ocean, rain - is
    // deliberately left as authored.
    void ApplyWeather(
        wi::scene::WeatherComponent& weather,
        const WeatherState& state) noexcept;

    class ICommand
    {
    public:
        virtual ~ICommand() = default;
        virtual bool Execute() = 0;
        virtual void Undo() = 0;
    };

    class CommandService
    {
    public:
        bool Execute(std::unique_ptr<ICommand> command);
        bool Undo();
        bool Redo();
        void Clear() noexcept;

        [[nodiscard]] bool CanUndo() const noexcept;
        [[nodiscard]] bool CanRedo() const noexcept;
        [[nodiscard]] std::size_t UndoCount() const noexcept;
        [[nodiscard]] std::size_t RedoCount() const noexcept;

    private:
        std::vector<std::unique_ptr<ICommand>> undoStack_;
        std::vector<std::unique_ptr<ICommand>> redoStack_;
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
        bool Apply(const TransformState& transform);

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
