#pragma once
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/DialogueService.h"
namespace renegade::bridge
{
inline constexpr const char *DialogueMetadataKey = "renegade.dialogue.asset";
bool ResolveDialogueVoicePath(const std::string &root, const std::string &relative, std::string &absolute, std::string &error);
// Adopt a creator's local recording into this project without overwriting clips.
bool ImportUiClickSound(const std::string& root,const std::string& source,std::string& relative,std::string& error);
bool ImportDialogueVoice(const std::string &root, const std::string &source, std::string &relative, std::string &error);
bool SnapshotDialogueVoices(const std::string &sourceRoot, const std::string &destinationRoot, const DialogueDocument &, std::string &error);
std::string DialogueAssetPath(const std::string &root, const std::string &id);
bool SaveDialogueAsset(const std::string &root, const DialogueDocument &, std::string &error);
bool LoadDialogueAsset(const std::string &root, const std::string &project, const std::string &id, DialogueDocument &,
                       std::string &error);
std::vector<DialogueDocument> ListDialogueAssets(const std::string &root, const std::string &project,
                                                 std::string &error);
std::string AssignedDialogue(const wi::scene::Scene &, wi::ecs::Entity);
class AssignDialogueCommand final : public ICommand
{
  public:
    AssignDialogueCommand(wi::scene::Scene &, wi::ecs::Entity, std::string id);
    bool Execute() override;
    void Undo() override;

  private:
    void Apply(const std::string &);
    wi::scene::Scene *scene_;
    wi::ecs::Entity entity_;
    std::string before_, after_;
};
} // namespace renegade::bridge
