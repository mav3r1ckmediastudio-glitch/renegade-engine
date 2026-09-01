#include "renegade/bridge/GameplayInputService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    namespace fs = std::filesystem;

    int failures = 0;

    void Check(const bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    }

    fs::path TestRoot()
    {
        const auto stamp = std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();
        return fs::temp_directory_path() /
            ("renegade-phase6-gate2-" + std::to_string(stamp));
    }

    const renegade::bridge::GameplayActionBinding& Binding(
        const renegade::bridge::GameplayInputMap& map,
        const renegade::bridge::GameplayAction action)
    {
        return map.bindings[static_cast<std::size_t>(action)];
    }
}

int main()
{
    using namespace renegade::bridge;

    const fs::path root = TestRoot();
    std::error_code ec;
    fs::create_directories(root / "Content" / "Data", ec);
    Check(!ec, "could not create test project root");

    GameplayInputMap map;
    bool created = false;
    std::string error;
    Check(EnsureGameplayInputMap(
            root.generic_u8string(), map, created, error),
        "default gameplay input map was not created: " + error);
    Check(created, "first ensure did not report map creation");
    Check(fs::is_regular_file(
            root / "Content" / "Data" / "GameplayInput.renegade-input"),
        "input-map file was not created in Content/Data");

    Check(Binding(map, GameplayAction::MoveForward).keyboard == "W",
        "move-forward default is not W");
    Check(Binding(map, GameplayAction::LookYaw).mouse == "MOUSE_X",
        "look-yaw default is not mouse X");
    Check(Binding(map, GameplayAction::Jump).gamepad == "BUTTON_2",
        "jump gamepad default was not retained from Gate 1");
    Check(Binding(map, GameplayAction::Pause).keyboard == "ESCAPE",
        "pause default is not Escape");
    Check(Binding(map, GameplayAction::Reset).keyboard == "R",
        "reset default is not R");

    auto rebound = map;
    rebound.bindings[static_cast<std::size_t>(GameplayAction::MoveForward)].keyboard = "I";
    rebound.bindings[static_cast<std::size_t>(GameplayAction::MoveBackward)].keyboard = "K";
    Check(WriteGameplayInputMap(root.generic_u8string(), rebound, error),
        "rebound input map did not persist: " + error);

    GameplayInputMap reopened;
    Check(ReadGameplayInputMap(root.generic_u8string(), reopened, error),
        "persisted input map did not reopen: " + error);
    Check(Binding(reopened, GameplayAction::MoveForward).keyboard == "I" &&
            Binding(reopened, GameplayAction::MoveBackward).keyboard == "K",
        "keyboard rebinds did not round-trip");

    created = true;
    GameplayInputMap ensured;
    Check(EnsureGameplayInputMap(
            root.generic_u8string(), ensured, created, error),
        "second ensure rejected a valid persisted input map: " + error);
    Check(!created, "second ensure rewrote an existing input map");
    Check(Binding(ensured, GameplayAction::MoveForward).keyboard == "I",
        "ensure replaced a creator-authored binding with defaults");

    auto invalid = reopened;
    invalid.bindings[static_cast<std::size_t>(GameplayAction::Jump)].keyboard =
        "CTRL+ALT+J";
    Check(!ValidateGameplayInputMap(invalid, error),
        "unsupported binding token was accepted");
    Check(!WriteGameplayInputMap(root.generic_u8string(), invalid, error),
        "invalid input map was written to disk");

    GameplayAction parsed{};
    Check(TryParseGameplayAction("pause", parsed) &&
            parsed == GameplayAction::Pause,
        "stable pause action ID did not parse");
    Check(std::string(GameplayActionId(GameplayAction::Reset)) == "reset",
        "stable reset action ID changed");

    fs::remove_all(root, ec);
    if (failures != 0)
    {
        std::cerr << failures << " Phase 6 Gate 2 test(s) failed.\n";
        return 1;
    }

    std::cout << "Phase 6 Gate 2 gameplay input persistence tests passed.\n";
    return 0;
}
