#include "renegade/bridge/DialogueAssetService.h"
#include "renegade/bridge/IdentityService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace renegade::bridge;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << "Dialogue asset failure at " << __LINE__ << ": " << #x << '\n';                               \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
int main()
{
    std::string error;
    auto project = GenerateStableId(), id = GenerateStableId();
    auto root = std::filesystem::temp_directory_path() / ("renegade-dialogue-" + GenerateStableId());
    std::filesystem::create_directories(root);
    auto d = MakeGuardDialogue(project, id);
    CHECK(SaveDialogueAsset(root.generic_u8string(), d, error));
    DialogueDocument reopened;
    CHECK(LoadDialogueAsset(root.generic_u8string(), project, id, reopened, error));
    CHECK(EncodeDialogue(d) == EncodeDialogue(reopened));
    CHECK(!LoadDialogueAsset(root.generic_u8string(), GenerateStableId(), id, reopened, error));
    auto invalid = d;
    invalid.lines[0].replies[0].outcomes[0].next = "missing";
    CHECK(!SaveDialogueAsset(root.generic_u8string(), invalid, error));
    CHECK(LoadDialogueAsset(root.generic_u8string(), project, id, reopened, error));
    CHECK(EncodeDialogue(d) == EncodeDialogue(reopened));
    CHECK(!LoadDialogueAsset(root.generic_u8string(), project, "../escape", reopened, error));
    const auto voiceFolder = root / "Content/Audio/Dialogue";
    std::filesystem::create_directories(voiceFolder);
    const auto voiceFile = voiceFolder / "guard.wav";
    { std::ofstream out(voiceFile, std::ios::binary); out << "test audio bytes"; }
    d.lines[0].voice = "Content/Audio/Dialogue/guard.wav";
    CHECK(SaveDialogueAsset(root.generic_u8string(), d, error));
    CHECK(LoadDialogueAsset(root.generic_u8string(), project, id, reopened, error));
    CHECK(reopened.lines[0].voice == d.lines[0].voice);
    std::string resolved;
    CHECK(ResolveDialogueVoicePath(root.generic_u8string(), d.lines[0].voice, resolved, error));
    CHECK(!ResolveDialogueVoicePath(root.generic_u8string(), "Content/../outside.wav", resolved, error));
    const auto snapshot = root / "Snapshot";
    CHECK(SnapshotDialogueVoices(root.generic_u8string(), snapshot.generic_u8string(), d, error));
    CHECK(std::filesystem::file_size(snapshot / d.lines[0].voice) == std::filesystem::file_size(voiceFile));
    std::filesystem::remove(voiceFile);
    CHECK(!SnapshotDialogueVoices(root.generic_u8string(), snapshot.generic_u8string(), d, error));
    auto scene = wi::allocator::make_shared<wi::scene::Scene>();
    auto npc = scene->Entity_CreateTransform("Gate Guard");
    scene->metadatas.Create(npc).string_values.set("creator.note", "Preserve this");
    CommandService history;
    CHECK(history.Execute(std::make_unique<AssignDialogueCommand>(*scene, npc, id)));
    CHECK(AssignedDialogue(*scene, npc) == id);
    CHECK(history.Undo());
    CHECK(AssignedDialogue(*scene, npc).empty());
    CHECK(history.Redo());
    CHECK(AssignedDialogue(*scene, npc) == id);
    CHECK(scene->metadatas.GetComponent(npc)->string_values.get("creator.note") == "Preserve this");
    wi::Archive archive;
    scene->Serialize(archive);
    archive.SetReadModeAndResetPos(true);
    auto other = wi::allocator::make_shared<wi::scene::Scene>();
    other->Serialize(archive);
    CHECK(AssignedDialogue(*other, other->Entity_FindByName("Gate Guard")) == id);
    std::filesystem::remove_all(root);
    std::cout << "Dialogue asset transaction, binding Undo/Redo and native scene reopen passed\n";
}