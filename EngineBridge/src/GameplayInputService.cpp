#include "renegade/bridge/GameplayInputService.h"

#include "renegade/bridge/ProjectDocumentTransaction.h"

#include <WickedEngine.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using renegade::bridge::GameplayAction;
    using renegade::bridge::GameplayActionBinding;
    using renegade::bridge::GameplayInputMap;

    std::string Trim(std::string value)
    {
        const auto whitespace = [](const unsigned char value)
        {
            return std::isspace(value) != 0;
        };
        value.erase(
            value.begin(),
            std::find_if(value.begin(), value.end(),
                [&](const char c) { return !whitespace(c); }));
        value.erase(
            std::find_if(value.rbegin(), value.rend(),
                [&](const char c) { return !whitespace(c); }).base(),
            value.end());
        return value;
    }

    bool SupportedKeyboardToken(const std::string& token) noexcept
    {
        if (token.empty())
            return true;
        if (token.size() == 1)
        {
            const unsigned char c = static_cast<unsigned char>(token.front());
            return std::isalnum(c) != 0;
        }
        return token == "SPACE" || token == "LSHIFT" || token == "ESCAPE";
    }

    bool SupportedMouseToken(const std::string& token) noexcept
    {
        return token.empty() || token == "MOUSE_X" || token == "MOUSE_Y";
    }

    bool SupportedGamepadToken(const std::string& token) noexcept
    {
        return token.empty() ||
            token == "LEFT_X_POS" || token == "LEFT_X_NEG" ||
            token == "LEFT_Y_POS" || token == "LEFT_Y_NEG" ||
            token == "RIGHT_X" || token == "RIGHT_Y" ||
            token == "BUTTON_2" || token == "BUTTON_7";
    }

    bool KeyboardDown(const std::string& token) noexcept
    {
        if (token.empty())
            return false;
        if (token.size() == 1)
        {
            return wi::input::Down(
                static_cast<wi::input::BUTTON>(
                    static_cast<unsigned char>(token.front())));
        }
        if (token == "SPACE")
            return wi::input::Down(wi::input::KEYBOARD_BUTTON_SPACE);
        if (token == "LSHIFT")
            return wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT);
        if (token == "ESCAPE")
            return wi::input::Down(wi::input::KEYBOARD_BUTTON_ESCAPE);
        return false;
    }

    bool KeyboardPress(const std::string& token) noexcept
    {
        if (token.empty())
            return false;
        if (token.size() == 1)
        {
            return wi::input::Press(
                static_cast<wi::input::BUTTON>(
                    static_cast<unsigned char>(token.front())));
        }
        if (token == "SPACE")
            return wi::input::Press(wi::input::KEYBOARD_BUTTON_SPACE);
        if (token == "LSHIFT")
            return wi::input::Press(wi::input::KEYBOARD_BUTTON_LSHIFT);
        if (token == "ESCAPE")
            return wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE);
        return false;
    }

    float MouseAxis(const std::string& token) noexcept
    {
        if (token.empty())
            return 0.0f;
        const auto& mouse = wi::input::GetMouseState();
        if (token == "MOUSE_X")
            return mouse.delta_position.x;
        if (token == "MOUSE_Y")
            return mouse.delta_position.y;
        return 0.0f;
    }

    float GamepadAxis(const std::string& token) noexcept
    {
        if (token.empty())
            return 0.0f;
        const XMFLOAT4 left = wi::input::GetAnalog(
            wi::input::GAMEPAD_ANALOG_THUMBSTICK_L);
        const XMFLOAT4 right = wi::input::GetAnalog(
            wi::input::GAMEPAD_ANALOG_THUMBSTICK_R);
        if (token == "LEFT_X_POS") return std::max(0.0f, left.x);
        if (token == "LEFT_X_NEG") return std::max(0.0f, -left.x);
        if (token == "LEFT_Y_POS") return std::max(0.0f, left.y);
        if (token == "LEFT_Y_NEG") return std::max(0.0f, -left.y);
        if (token == "RIGHT_X") return right.x;
        if (token == "RIGHT_Y") return right.y;
        return 0.0f;
    }

    bool GamepadDown(const std::string& token) noexcept
    {
        if (token == "BUTTON_2")
            return wi::input::Down(wi::input::GAMEPAD_BUTTON_2);
        if (token == "BUTTON_7")
            return wi::input::Down(wi::input::GAMEPAD_BUTTON_7);
        return false;
    }

    bool GamepadPress(const std::string& token) noexcept
    {
        if (token == "BUTTON_2")
            return wi::input::Press(wi::input::GAMEPAD_BUTTON_2);
        if (token == "BUTTON_7")
            return wi::input::Press(wi::input::GAMEPAD_BUTTON_7);
        return false;
    }

    const GameplayActionBinding& Binding(
        const GameplayInputMap& map,
        const GameplayAction action) noexcept
    {
        return map.bindings[static_cast<std::size_t>(action)];
    }

    float MovementValue(const GameplayActionBinding& binding) noexcept
    {
        return (KeyboardDown(binding.keyboard) ? 1.0f : 0.0f) +
            GamepadAxis(binding.gamepad);
    }

    bool Pressed(const GameplayActionBinding& binding) noexcept
    {
        return KeyboardPress(binding.keyboard) ||
            GamepadPress(binding.gamepad);
    }

    bool Down(const GameplayActionBinding& binding) noexcept
    {
        return KeyboardDown(binding.keyboard) ||
            GamepadDown(binding.gamepad);
    }

    std::string Serialize(const GameplayInputMap& map)
    {
        std::ostringstream stream;
        stream << "format = "
               << renegade::bridge::GameplayInputDocumentFormat << '\n';
        stream << "version = " << map.formatVersion << "\n\n";
        for (const auto& binding : map.bindings)
        {
            stream << "[action."
                   << renegade::bridge::GameplayActionId(binding.action)
                   << "]\n";
            stream << "keyboard = " << binding.keyboard << '\n';
            stream << "mouse = " << binding.mouse << '\n';
            stream << "gamepad = " << binding.gamepad << "\n\n";
        }
        stream << "[settings]\n";
        stream << "mouse_look_scale = " << map.mouseLookScale << '\n';
        stream << "gamepad_look_radians_per_second = "
               << map.gamepadLookRadiansPerSecond << '\n';
        return stream.str();
    }

    bool ParseFloat(
        const std::string& value,
        float& parsed) noexcept
    {
        try
        {
            std::size_t consumed = 0;
            const float result = std::stof(value, &consumed);
            if (consumed != value.size() || !std::isfinite(result))
                return false;
            parsed = result;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}

namespace renegade::bridge
{
    const char* GameplayActionId(const GameplayAction action) noexcept
    {
        switch (action)
        {
        case GameplayAction::MoveForward: return "move_forward";
        case GameplayAction::MoveBackward: return "move_backward";
        case GameplayAction::MoveLeft: return "move_left";
        case GameplayAction::MoveRight: return "move_right";
        case GameplayAction::LookYaw: return "look_yaw";
        case GameplayAction::LookPitch: return "look_pitch";
        case GameplayAction::Jump: return "jump";
        case GameplayAction::Sprint: return "sprint";
        case GameplayAction::Pause: return "pause";
        case GameplayAction::Reset: return "reset";
        case GameplayAction::Count: break;
        }
        return "unknown";
    }

    bool TryParseGameplayAction(
        const std::string& id,
        GameplayAction& action) noexcept
    {
        for (std::size_t index = 0;
            index < static_cast<std::size_t>(GameplayAction::Count);
            ++index)
        {
            const auto candidate = static_cast<GameplayAction>(index);
            if (id == GameplayActionId(candidate))
            {
                action = candidate;
                return true;
            }
        }
        return false;
    }

    GameplayInputMap MakeDefaultGameplayInputMap()
    {
        GameplayInputMap map;
        map.bindings = {{
            {GameplayAction::MoveForward, "W", "", "LEFT_Y_POS"},
            {GameplayAction::MoveBackward, "S", "", "LEFT_Y_NEG"},
            {GameplayAction::MoveLeft, "A", "", "LEFT_X_NEG"},
            {GameplayAction::MoveRight, "D", "", "LEFT_X_POS"},
            {GameplayAction::LookYaw, "", "MOUSE_X", "RIGHT_X"},
            {GameplayAction::LookPitch, "", "MOUSE_Y", "RIGHT_Y"},
            {GameplayAction::Jump, "SPACE", "", "BUTTON_2"},
            {GameplayAction::Sprint, "LSHIFT", "", "BUTTON_7"},
            {GameplayAction::Pause, "ESCAPE", "", ""},
            {GameplayAction::Reset, "R", "", ""},
        }};
        return map;
    }

    bool ValidateGameplayInputMap(
        const GameplayInputMap& map,
        std::string& error) noexcept
    {
        if (map.formatVersion != GameplayInputDocumentVersion)
        {
            error = "Unsupported gameplay input-map version.";
            return false;
        }
        if (!std::isfinite(map.mouseLookScale) || map.mouseLookScale <= 0.0f ||
            map.mouseLookScale > 0.1f ||
            !std::isfinite(map.gamepadLookRadiansPerSecond) ||
            map.gamepadLookRadiansPerSecond <= 0.0f ||
            map.gamepadLookRadiansPerSecond > 20.0f)
        {
            error = "Gameplay input-map sensitivity settings are invalid.";
            return false;
        }

        std::array<bool, static_cast<std::size_t>(GameplayAction::Count)> seen{};
        for (const auto& binding : map.bindings)
        {
            const auto index = static_cast<std::size_t>(binding.action);
            if (index >= seen.size() || seen[index])
            {
                error = "Gameplay input-map contains duplicate or invalid actions.";
                return false;
            }
            seen[index] = true;
            if (!SupportedKeyboardToken(binding.keyboard) ||
                !SupportedMouseToken(binding.mouse) ||
                !SupportedGamepadToken(binding.gamepad))
            {
                error = "Gameplay input-map contains an unsupported binding for action '" +
                    std::string(GameplayActionId(binding.action)) + "'.";
                return false;
            }
            if (binding.keyboard.empty() && binding.mouse.empty() &&
                binding.gamepad.empty())
            {
                error = "Gameplay input-map action '" +
                    std::string(GameplayActionId(binding.action)) +
                    "' has no binding.";
                return false;
            }
        }
        if (std::any_of(seen.begin(), seen.end(), [](const bool value) { return !value; }))
        {
            error = "Gameplay input-map is missing a required action.";
            return false;
        }
        error.clear();
        return true;
    }

    std::string GameplayInputDocumentPath(const std::string& projectRoot)
    {
        if (projectRoot.empty())
            return {};
        return (fs::u8path(projectRoot) /
            fs::u8path(GameplayInputDocumentRelativePath))
            .lexically_normal()
            .generic_u8string();
    }

    bool ReadGameplayInputMapFile(
        const std::string& path,
        GameplayInputMap& map,
        std::string& error)
    {
        std::ifstream stream(fs::u8path(path));
        if (!stream)
        {
            error = "Gameplay input-map does not exist: " + path;
            return false;
        }

        GameplayInputMap parsed = MakeDefaultGameplayInputMap();
        std::array<bool, static_cast<std::size_t>(GameplayAction::Count)> seen{};
        bool formatSeen = false;
        bool versionSeen = false;
        std::string section;
        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(stream, line))
        {
            ++lineNumber;
            line = Trim(std::move(line));
            if (line.empty() || line.front() == '#' || line.front() == ';')
                continue;
            if (line.front() == '[' && line.back() == ']')
            {
                section = Trim(line.substr(1, line.size() - 2));
                continue;
            }
            const auto equals = line.find('=');
            if (equals == std::string::npos)
            {
                error = "Gameplay input-map has malformed line " +
                    std::to_string(lineNumber) + ".";
                return false;
            }
            const std::string key = Trim(line.substr(0, equals));
            const std::string value = Trim(line.substr(equals + 1));
            if (section.empty())
            {
                if (key == "format")
                {
                    if (value != GameplayInputDocumentFormat)
                    {
                        error = "Selected file is not a Renegade gameplay input-map.";
                        return false;
                    }
                    formatSeen = true;
                }
                else if (key == "version")
                {
                    if (value != std::to_string(GameplayInputDocumentVersion))
                    {
                        error = "Unsupported gameplay input-map version: " + value;
                        return false;
                    }
                    versionSeen = true;
                    parsed.formatVersion = GameplayInputDocumentVersion;
                }
                else
                {
                    error = "Gameplay input-map has an unknown root key: " + key;
                    return false;
                }
                continue;
            }
            if (section == "settings")
            {
                if (key == "mouse_look_scale")
                {
                    if (!ParseFloat(value, parsed.mouseLookScale))
                    {
                        error = "Gameplay input-map has invalid mouse look scale.";
                        return false;
                    }
                }
                else if (key == "gamepad_look_radians_per_second")
                {
                    if (!ParseFloat(value, parsed.gamepadLookRadiansPerSecond))
                    {
                        error = "Gameplay input-map has invalid gamepad look speed.";
                        return false;
                    }
                }
                else
                {
                    error = "Gameplay input-map has an unknown settings key: " + key;
                    return false;
                }
                continue;
            }
            constexpr const char* prefix = "action.";
            if (section.rfind(prefix, 0) != 0)
            {
                error = "Gameplay input-map has an unknown section: " + section;
                return false;
            }
            GameplayAction action{};
            if (!TryParseGameplayAction(section.substr(7), action))
            {
                error = "Gameplay input-map has an unknown action section: " + section;
                return false;
            }
            const auto index = static_cast<std::size_t>(action);
            if (!seen[index])
            {
                parsed.bindings[index] = {action, "", "", ""};
                seen[index] = true;
            }
            auto& binding = parsed.bindings[index];
            if (key == "keyboard") binding.keyboard = value;
            else if (key == "mouse") binding.mouse = value;
            else if (key == "gamepad") binding.gamepad = value;
            else
            {
                error = "Gameplay input-map has an unknown action key: " + key;
                return false;
            }
        }
        if (!stream.eof())
        {
            error = "Could not read complete gameplay input-map: " + path;
            return false;
        }
        if (!formatSeen || !versionSeen ||
            std::any_of(seen.begin(), seen.end(), [](const bool value) { return !value; }))
        {
            error = "Gameplay input-map is incomplete.";
            return false;
        }
        if (!ValidateGameplayInputMap(parsed, error))
            return false;
        map = std::move(parsed);
        error.clear();
        return true;
    }

    bool ReadGameplayInputMap(
        const std::string& projectRoot,
        GameplayInputMap& map,
        std::string& error)
    {
        const std::string path = GameplayInputDocumentPath(projectRoot);
        if (path.empty())
        {
            error = "Gameplay input-map requires a project root.";
            return false;
        }
        return ReadGameplayInputMapFile(path, map, error);
    }

    bool WriteGameplayInputMap(
        const std::string& projectRoot,
        const GameplayInputMap& map,
        std::string& error)
    {
        if (!ValidateGameplayInputMap(map, error))
            return false;
        if (projectRoot.empty())
        {
            error = "Gameplay input-map requires a project root.";
            return false;
        }

        try
        {
            const fs::path root = fs::absolute(fs::u8path(projectRoot)).lexically_normal();
            const fs::path destination =
                root / fs::u8path(GameplayInputDocumentRelativePath);
            std::error_code ec;
            fs::create_directories(destination.parent_path(), ec);
            if (ec)
            {
                error = "Could not create gameplay input-map directory: " +
                    ec.message();
                return false;
            }
            const fs::path journalDirectory = root / "Intermediate" / "Transactions";
            fs::create_directories(journalDirectory, ec);
            if (ec)
            {
                error = "Could not create gameplay input-map transaction directory: " +
                    ec.message();
                return false;
            }

            const std::string text = Serialize(map);
            ProjectDocumentWrite document;
            document.destinationPath = destination.generic_u8string();
            document.content.assign(text.begin(), text.end());
            document.validator = [](const std::string& stagedPath, std::string& validationError)
            {
                GameplayInputMap verified;
                return ReadGameplayInputMapFile(stagedPath, verified, validationError);
            };

            ProjectDocumentTransactionOptions options;
            options.allowedRoot = root.generic_u8string();
            options.journalDirectory = journalDirectory.generic_u8string();
            const auto result = ProjectDocumentTransaction{}.Execute(
                {std::move(document)}, std::move(options));
            if (!result.success || !result.committed)
            {
                error = "Could not commit gameplay input-map";
                if (!result.code.empty()) error += " [" + result.code + "]";
                if (!result.message.empty()) error += ": " + result.message;
                return false;
            }
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = std::string("Could not write gameplay input-map: ") +
                exception.what();
            return false;
        }
    }

    bool EnsureGameplayInputMap(
        const std::string& projectRoot,
        GameplayInputMap& map,
        bool& created,
        std::string& error)
    {
        created = false;
        const std::string path = GameplayInputDocumentPath(projectRoot);
        if (path.empty())
        {
            error = "Gameplay input-map requires a project root.";
            return false;
        }
        std::error_code ec;
        if (fs::exists(fs::u8path(path), ec))
        {
            if (ec || !fs::is_regular_file(fs::u8path(path), ec) || ec)
            {
                error = "Gameplay input-map path is not a regular file: " + path;
                return false;
            }
            return ReadGameplayInputMapFile(path, map, error);
        }
        if (ec)
        {
            error = "Could not inspect gameplay input-map path: " + ec.message();
            return false;
        }

        map = MakeDefaultGameplayInputMap();
        if (!WriteGameplayInputMap(projectRoot, map, error))
            return false;
        created = true;
        error.clear();
        return true;
    }

    GameplayInputFrame CaptureGameplayInput(
        const GameplayInputMap& map,
        const float dt) noexcept
    {
        GameplayInputFrame frame;
        const float safeDt = std::clamp(dt, 0.0f, 0.1f);

        frame.player.moveForward =
            MovementValue(Binding(map, GameplayAction::MoveForward)) -
            MovementValue(Binding(map, GameplayAction::MoveBackward));
        frame.player.moveRight =
            MovementValue(Binding(map, GameplayAction::MoveRight)) -
            MovementValue(Binding(map, GameplayAction::MoveLeft));

        const auto& yaw = Binding(map, GameplayAction::LookYaw);
        const auto& pitch = Binding(map, GameplayAction::LookPitch);
        frame.player.lookYaw =
            MouseAxis(yaw.mouse) * map.mouseLookScale +
            GamepadAxis(yaw.gamepad) * map.gamepadLookRadiansPerSecond * safeDt;
        frame.player.lookPitch =
            MouseAxis(pitch.mouse) * map.mouseLookScale -
            GamepadAxis(pitch.gamepad) * map.gamepadLookRadiansPerSecond * safeDt;
        frame.player.jumpPressed = Pressed(Binding(map, GameplayAction::Jump));
        frame.player.sprintDown = Down(Binding(map, GameplayAction::Sprint));
        frame.pausePressed = Pressed(Binding(map, GameplayAction::Pause));
        frame.resetPressed = Pressed(Binding(map, GameplayAction::Reset));
        return frame;
    }
}
