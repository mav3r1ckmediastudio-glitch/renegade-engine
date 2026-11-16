#pragma once
#include "renegade/bridge/DialogueService.h"
#include "renegade/bridge/ScreenService.h"
namespace renegade::bridge {
inline constexpr const char* DialogueDisplaySettingsPath = "Content/Gameplay/Dialogue.renegade-ui";
struct DialogueDisplayAssignment { StableId screenId; std::string path; };
ScreenDocument MakeDialogueDisplayTemplate(const StableId& project, const std::string& path);
bool ValidateDialogueDisplay(const ScreenDocument&, const StableId& project, std::string& error);
bool ReadDialogueDisplayAssignment(const std::string& root, const StableId& project,
                                  DialogueDisplayAssignment&, std::string& error);
bool SetDialogueDisplayAssignment(const std::string& root, const StableId& project,
                                 const DialogueDisplayAssignment&, std::string& error);
bool LoadDialogueDisplay(const std::string& root, const StableId& project, ScreenDocument&, std::string& error);
bool SnapshotDialogueDisplay(const std::string& root, const std::string& destination,
                             const StableId& project, std::string& error);
// Populate an immutable authoring template; styles/geometry and stable IDs stay intact.
bool BuildDialogueDisplay(const ScreenDocument&, const DialogueView&, size_t page,
                         ScreenDocument&, std::vector<StableId>& controls,
                         std::vector<std::string>& actions, std::string& error);
}
