#pragma once
// UI-independent conversation authority shared by authoring preview and Runtime.
#include <functional>
#include <map>
#include <string>
#include <vector>
namespace renegade::bridge
{
enum class DialogueConditionKind
{
    FlagEquals,
    ValueAtLeast,
    HasItem
};
struct DialogueCondition
{
    DialogueConditionKind kind = DialogueConditionKind::FlagEquals;
    std::string key;
    int value = 1;
};
enum class DialogueActionKind
{
    SetFlag,
    SetValue,
    Event
};
struct DialogueAction
{
    DialogueActionKind kind = DialogueActionKind::SetFlag;
    std::string key;
    int value = 1;
};
struct DialogueOutcome
{
    std::vector<DialogueCondition> conditions;
    bool matchAll = true;
    std::vector<DialogueAction> actions;
    std::string next;
};
struct DialogueReply
{
    std::string id, text;
    bool once = false;
    std::vector<DialogueCondition> conditions;
    std::vector<DialogueOutcome> outcomes;
};
struct DialogueLine
{
    std::string id, speaker = "NPC", text;
    std::vector<DialogueReply> replies;
    std::string next;
    std::string voice; // Optional project-relative audio clip.
};
struct DialogueTheme
{
    float textSize = 22;
    float panelOpacity = 1.f;
    unsigned accent = 0xde5b1d;
};
struct DialogueDocument
{
    std::string projectId, id, name = "New Conversation", entry;
    DialogueTheme theme;
    std::vector<DialogueLine> lines;
};
struct DialogueMemory
{
    std::map<std::string, int> values;
    std::vector<std::string> usedReplies;
};
struct DialogueView
{
    std::string speaker, text, voice, voiceKey;
    std::vector<std::pair<std::string, std::string>> replies;
    bool active = false;
    DialogueTheme theme;
};
bool IsDialogueVoicePath(const std::string &);
bool ValidateDialogue(const DialogueDocument &, std::string &error);
std::string EncodeDialogue(const DialogueDocument &);
bool DecodeDialogue(const std::string &, DialogueDocument &, std::string &error);
std::string EncodeDialogueMemory(const DialogueMemory &);
bool DecodeDialogueMemory(const std::string &, DialogueMemory &, std::string &error);
DialogueDocument MakeGuardDialogue(const std::string &project, const std::string &id);
class DialogueSession
{
  public:
    using ItemQuery = std::function<bool(const std::string &, int)>;
    // Event batches must be accepted atomically. No state/advance on rejection.
    using EventSink = std::function<bool(const std::vector<DialogueAction> &, std::string &)>;
    bool Start(const DialogueDocument &, DialogueMemory &, std::string &error);
    void Close() noexcept
    {
        current_.clear();
    }
    bool Active() const noexcept
    {
        return !current_.empty();
    }
    DialogueView View() const;
    bool Choose(const std::string &replyId, std::string &error);
    bool Continue(std::string &error);
    void SetItemQuery(ItemQuery query)
    {
        items_ = std::move(query);
    }
    void SetEventSink(EventSink sink)
    {
        events_ = std::move(sink);
    }
    bool Matches(const std::vector<DialogueCondition> &, bool all = true) const;

  private:
    const DialogueLine *Line() const;
    DialogueDocument document_;
    DialogueMemory *memory_ = nullptr;
    std::string current_;
    unsigned long long visit_ = 0;
    ItemQuery items_;
    EventSink events_;
};
} // namespace renegade::bridge