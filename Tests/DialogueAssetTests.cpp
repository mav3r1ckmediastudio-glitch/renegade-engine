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
    // Creator recordings are copied into the project; collisions never replace
    // an earlier clip, and a project-owned recording is reused in place.
    const auto recording = root / "my-recording.wav";
    {
        std::ofstream out(recording, std::ios::binary);
        auto le = [&](unsigned value, unsigned bytes) {
            for (unsigned n = 0; n < bytes; ++n) out.put(char((value >> (8 * n)) & 255));
        };
        out << "RIFF"; le(38, 4); out << "WAVEfmt "; le(16, 4); le(1, 2); le(1, 2);
        le(44100, 4); le(88200, 4); le(2, 2); le(16, 2); out << "data"; le(2, 4); le(0, 2);
    }
    std::string imported, secondImport, reused;
    CHECK(ImportDialogueVoice(root.generic_u8string(), recording.generic_u8string(), imported, error));
    CHECK(imported.rfind("Content/Audio/Dialogue/my-recording-", 0) == 0);
    CHECK(ResolveDialogueVoicePath(root.generic_u8string(), imported, resolved, error));
    CHECK(std::filesystem::file_size(resolved) == std::filesystem::file_size(recording));
    CHECK(ImportDialogueVoice(root.generic_u8string(), recording.generic_u8string(), secondImport, error));
    CHECK(secondImport != imported && std::filesystem::exists(root / imported));
    CHECK(ImportDialogueVoice(root.generic_u8string(), (root / imported).generic_u8string(), reused, error));
    CHECK(reused == imported);
    CHECK(!ImportDialogueVoice(root.generic_u8string(), (root / "missing.wav").generic_u8string(), reused, error));
    const auto corrupt = root / "broken.wav";
    { std::ofstream out(corrupt); out << "not audio"; }
    CHECK(!ImportDialogueVoice(root.generic_u8string(), corrupt.generic_u8string(), reused, error));
    const auto unsupported = root / "recording.mp3";
    std::filesystem::copy_file(recording, unsupported);
    CHECK(!ImportDialogueVoice(root.generic_u8string(), unsupported.generic_u8string(), reused, error));
    d.lines[0].voice = imported;
    CHECK(SaveDialogueAsset(root.generic_u8string(), d, error));
    CHECK(LoadDialogueAsset(root.generic_u8string(), project, id, reopened, error));
    CHECK(reopened.lines[0].voice == imported);
    CHECK(SnapshotDialogueVoices(root.generic_u8string(), snapshot.generic_u8string(), reopened, error));
    CHECK(std::filesystem::exists(snapshot / imported));
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
