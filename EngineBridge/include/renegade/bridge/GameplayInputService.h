#pragma once

#include "renegade/bridge/PlayerService.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace renegade::bridge
{
    inline constexpr const char* GameplayInputDocumentRelativePath =
        "Content/Data/GameplayInput.renegade-input";
    inline constexpr const char* GameplayInputDocumentFormat =
        "renegade-input-map";
    inline constexpr std::uint32_t GameplayInputDocumentVersion = 1;

    enum class GameplayAction : std::uint8_t
    {
        MoveForward,
        MoveBackward,
        MoveLeft,
        MoveRight,
        LookYaw,
        LookPitch,
        Jump,
        Sprint,
        Pause,
        Reset,
        Count,
    };

    struct GameplayActionBinding
    {
        GameplayAction action = GameplayAction::MoveForward;
        std::string keyboard;
        std::string mouse;
        std::string gamepad;
    };

    struct GameplayInputMap
    {
        std::uint32_t formatVersion = GameplayInputDocumentVersion;
        std::array<GameplayActionBinding,
            static_cast<std::size_t>(GameplayAction::Count)> bindings{};
        float mouseLookScale = 0.0017f;
        float gamepadLookRadiansPerSecond = 2.5f;
    };

    struct GameplayInputFrame
    {
        PlayerInputFrame player;
        bool pausePressed = false;
        bool resetPressed = false;
    };

    [[nodiscard]] const char* GameplayActionId(GameplayAction action) noexcept;
    [[nodiscard]] bool TryParseGameplayAction(
        const std::string& id,
        GameplayAction& action) noexcept;
    [[nodiscard]] GameplayInputMap MakeDefaultGameplayInputMap();
    [[nodiscard]] bool ValidateGameplayInputMap(
        const GameplayInputMap& map,
        std::string& error) noexcept;

    [[nodiscard]] std::string GameplayInputDocumentPath(
        const std::string& projectRoot);
    [[nodiscard]] bool ReadGameplayInputMapFile(
        const std::string& path,
        GameplayInputMap& map,
        std::string& error);
    [[nodiscard]] bool ReadGameplayInputMap(
        const std::string& projectRoot,
        GameplayInputMap& map,
        std::string& error);
    [[nodiscard]] bool WriteGameplayInputMap(
        const std::string& projectRoot,
        const GameplayInputMap& map,
        std::string& error);
    [[nodiscard]] bool EnsureGameplayInputMap(
        const std::string& projectRoot,
        GameplayInputMap& map,
        bool& created,
        std::string& error);

    // Gate 2 owns raw gameplay-device polling. PlayerService remains action-shaped
    // and receives only PlayerInputFrame values produced here.
    [[nodiscard]] GameplayInputFrame CaptureGameplayInput(
        const GameplayInputMap& map,
        float dt) noexcept;
}
