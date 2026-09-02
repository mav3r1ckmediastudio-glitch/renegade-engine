#pragma once

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/GameplayInputService.h"
#include "renegade/bridge/PlayerService.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

struct lua_State;

namespace renegade::bridge
{
    inline constexpr const char* GameplayScriptMetadataKey =
        "renegade.gameplay.script";
    inline constexpr const char* GameplayScriptMetadataVersion = "1";
    inline constexpr const char* GameplayScriptEnabledMetadataKey =
        "renegade.gameplay.script_enabled";
    inline constexpr const char* GameplayScriptPathMetadataKey =
        "renegade.gameplay.script_path";

    struct GameplayScriptState
    {
        std::string projectRelativePath;
        bool enabled = true;
    };

    struct GameplayScriptDiagnostic
    {
        std::string entityId;
        std::string scriptPath;
        std::string callback;
        std::string message;
    };

    [[nodiscard]] bool IsGameplayScript(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;
    [[nodiscard]] GameplayScriptState CaptureGameplayScript(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;
    void PrepareGameplayScriptsForRuntime(
        wi::scene::Scene& scene) noexcept;

    // Resolve only project-owned Content/Scripts/*.lua files. Runtime and
    // packaging use the project-relative path as authority; absolute host
    // paths are never serialized into the WISCENE attachment.
    [[nodiscard]] bool ResolveGameplayScriptPath(
        const std::string& projectRoot,
        const std::string& projectRelativePath,
        std::string& absolutePath,
        std::string& error);
    [[nodiscard]] bool ValidateGameplayScriptSyntax(
        const std::string& projectRoot,
        const std::string& projectRelativePath,
        lua_State* state,
        std::string& error);

    // Studio's creator-facing import seam. An external Lua file is copied to
    // Content/Scripts under a collision-free name. A file already inside that
    // governed folder is retained in place.
    [[nodiscard]] bool ImportGameplayScript(
        const std::string& projectRoot,
        const std::string& sourcePath,
        std::string& projectRelativePath,
        std::string& error);

    class CreateGameplayScriptCommand final : public ICommand
    {
    public:
        CreateGameplayScriptCommand(
            wi::scene::Scene& scene,
            std::string projectRoot,
            GameplayScriptState state);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept;

    private:
        wi::scene::Scene* scene_ = nullptr;
        std::string projectRoot_;
        GameplayScriptState state_;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };

    class SetGameplayScriptEnabledCommand final : public ICommand
    {
    public:
        SetGameplayScriptEnabledCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            bool enabled);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(bool enabled) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        bool before_ = true;
        bool after_ = true;
    };

    // Deterministic Runtime owner for governed script callbacks. Wicked keeps
    // ownership of its one Lua VM; Renegade stores only registry references to
    // returned script tables and never exposes an engine pointer to Lua.
    class GameplayScriptRuntime
    {
    public:
        GameplayScriptRuntime();
        ~GameplayScriptRuntime();
        GameplayScriptRuntime(const GameplayScriptRuntime&) = delete;
        GameplayScriptRuntime& operator=(const GameplayScriptRuntime&) = delete;

        [[nodiscard]] bool Start(
            wi::scene::Scene& scene,
            std::string projectRoot,
            const GameplayInputFrame& input,
            const RuntimePlayerState& player,
            std::string& error);
        [[nodiscard]] bool Start(
            wi::scene::Scene& scene,
            std::string projectRoot,
            const GameplayInputFrame& input,
            const RuntimePlayerState& player,
            lua_State* state,
            std::string& error);
        void Update(
            float dt,
            const GameplayInputFrame& input,
            const RuntimePlayerState& player) noexcept;
        void Pause() noexcept;
        void Resume() noexcept;
        void Reset() noexcept;
        void Stop() noexcept;

        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] bool IsPaused() const noexcept;
        [[nodiscard]] std::size_t ActiveScriptCount() const noexcept;
        [[nodiscard]] const std::vector<GameplayScriptDiagnostic>&
            Diagnostics() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
