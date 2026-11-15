#pragma once
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/DialogueService.h"
namespace renegade::bridge
{
inline constexpr const char *DialogueMetadataKey = "renegade.dialogue.asset";
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
