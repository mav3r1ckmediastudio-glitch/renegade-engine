#include "renegade/bridge/DialogueService.h"
#include <iostream>
#include <stdexcept>
using namespace renegade::bridge;
int checks = 0;
void Check(bool v, const char *label)
{
    ++checks;
    if (!v)
        throw std::runtime_error(label);
}
int main()
{
    try
    {
        std::string error;
        auto d = MakeGuardDialogue("project", "guard");
        Check(ValidateDialogue(d, error), "valid guard");
        DialogueDocument copy;
        Check(DecodeDialogue(EncodeDialogue(d), copy, error), "save reopen");
        Check(EncodeDialogue(copy) == EncodeDialogue(d), "lossless codec");
        DialogueSession s;
        DialogueMemory m;
        Check(s.Start(copy, m, error), "start");
        Check(s.View().replies.size() == 3, "three replies");
        Check(s.Choose("gate", error) && s.View().text == "Come back with a permit.", "fallback");
        Check(s.Continue(error) && s.View().replies.size() == 3, "reconnect opening");
        m.values["has_permit"] = 1;
        Check(s.Choose("gate", error) && s.View().text == "Everything seems in order.", "permit branch");
        Check(m.values["access_granted"] == 1, "action mutates memory");
        Check(s.Continue(error) && !s.Active(), "end");
        d.lines[0].replies[0].once = true;
        Check(s.Start(d, m, error) && s.Choose("work", error), "once choose");
        Check(s.Start(d, m, error) && s.View().replies.size() == 2, "once hidden");
        Check(!s.Choose("work", error), "hidden rejected");
        DialogueMemory restored;
        Check(DecodeDialogueMemory(EncodeDialogueMemory(m), restored, error), "memory roundtrip");
        Check(restored.values == m.values && restored.usedReplies == m.usedReplies, "choices persist");
        d.lines[0].replies[1].outcomes[0].conditions = {{DialogueConditionKind::HasItem, "permit", 2}};
        s.SetItemQuery([](auto &id, int count) { return id == "permit" && count <= 2; });
        Check(s.Start(d, m, error) && s.Choose("gate", error) && s.View().text == "Everything seems in order.",
              "inventory condition");
        auto bad = d;
        bad.entry = "missing";
        Check(!ValidateDialogue(bad, error), "entry rejected");
        bad = d;
        bad.lines[1].id = "opening";
        Check(!ValidateDialogue(bad, error), "duplicate rejected");
        bad = d;
        bad.lines[0].replies[0].outcomes[0].next = "missing";
        Check(!ValidateDialogue(bad, error), "broken target");
        bad = d;
        bad.lines[0].replies[1].outcomes.pop_back();
        Check(!ValidateDialogue(bad, error), "missing otherwise");
        bad = d;
        bad.lines[0].replies[1].outcomes[0].conditions.clear();
        Check(!ValidateDialogue(bad, error), "shadowed outcomes");
        Check(!DecodeDialogue(EncodeDialogue(d) + "junk", copy, error), "trailing bytes");
        Check(!DecodeDialogue("renegade-dialogue 99", copy, error), "future schema");
        auto before = EncodeDialogue(copy);
        Check(!DecodeDialogue("broken", copy, error) && before == EncodeDialogue(copy), "decode transactional");
        auto &outcome = d.lines[0].replies[1].outcomes[0];
        outcome.actions.push_back({DialogueActionKind::Event, "open_gate", 1});
        m.values.erase("access_granted");
        Check(s.Start(d, m, error), "event start");
        Check(!s.Choose("gate", error) && s.View().text == "What business have you here?" &&
                  m.values.count("access_granted") == 0,
              "missing event handler no partial state");
        s.SetEventSink([](auto &, auto &e) {
            e = "Rejected";
            return false;
        });
        Check(!s.Choose("gate", error) && m.values.count("access_granted") == 0, "reject no partial state");
        int events = 0;
        s.SetEventSink([&](auto &a, auto &) {
            events += int(a.size());
            return true;
        });
        Check(s.Choose("gate", error) && events == 1 && m.values["access_granted"] == 1, "accepted event advances");
        Check(!s.Choose("gate", error) && events == 1, "no duplicate event");
        auto old = restored;
        Check(!DecodeDialogueMemory("renegade-dialogue-memory 1 1 \"bad key\" 1 0", restored, error) &&
                  restored.values == old.values,
              "bad memory no mutation");
        auto manyEvents = d;
        manyEvents.lines[0].replies[1].outcomes[0].actions.push_back({DialogueActionKind::Event, "second", 1});
        Check(!ValidateDialogue(manyEvents, error), "event batch rejected before save");
        DialogueMemory longMemory;
        longMemory.usedReplies.push_back(std::string(128, 'a') + ":" + std::string(128, 'b'));
        Check(DecodeDialogueMemory(EncodeDialogueMemory(longMemory), restored, error),
              "maximum composite reply identity persists");
        std::cout << checks << " dialogue checks passed\n";
        return 0;
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
