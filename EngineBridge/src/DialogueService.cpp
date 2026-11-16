#include "renegade/bridge/DialogueService.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <set>
#include <sstream>
namespace renegade::bridge
{
namespace
{
bool Token(const std::string &s, size_t maximum = 128)
{
    return !s.empty() && s.size() <= maximum &&
           s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.:") ==
               std::string::npos;
}
bool Text(const std::string &s)
{
    return s.size() <= 8192 && s.find('\0') == std::string::npos;
}
void Conditions(std::ostream &o, const std::vector<DialogueCondition> &cs)
{
    o << cs.size() << ' ';
    for (auto &c : cs)
        o << int(c.kind) << ' ' << std::quoted(c.key) << ' ' << c.value << ' ';
}
bool Conditions(std::istream &in, std::vector<DialogueCondition> &cs)
{
    size_t n;
    if (!(in >> n) || n > 64)
        return false;
    for (size_t i = 0; i < n; ++i)
    {
        DialogueCondition c;
        int k;
        if (!(in >> k >> std::quoted(c.key) >> c.value) || k < 0 || k > 2)
            return false;
        c.kind = DialogueConditionKind(k);
        cs.push_back(c);
    }
    return true;
}
} // namespace
bool IsDialogueVoicePath(const std::string &path)
{
    if (path.empty()) return true;
    if (path.size() > 1024 || path.rfind("Content/", 0) != 0 ||
        path.find_first_of("\\:\n\r\t") != std::string::npos || path.find('\0') != std::string::npos)
        return false;
    std::istringstream parts(path);
    std::string part;
    while (std::getline(parts, part, '/'))
        if (part.empty() || part == "." || part == "..") return false;
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return false;
    auto ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return ext == ".wav" || ext == ".ogg";
}
bool ValidateDialogue(const DialogueDocument &d, std::string &e)
{
    auto fail = [&](const char *s) {
        e = s;
        return false;
    };
    if (!Token(d.projectId) || !Token(d.id) || d.name.empty() || !Text(d.name) || d.lines.empty() ||
        d.lines.size() > 4096)
        return fail("Invalid conversation identity, name or line count.");
    if (!std::isfinite(d.theme.textSize) || d.theme.textSize < 14 || d.theme.textSize > 48 ||
        !std::isfinite(d.theme.panelOpacity) || d.theme.panelOpacity < .2f || d.theme.panelOpacity > 1 ||
        d.theme.accent > 0xffffff)
        return fail("Invalid dialogue appearance.");
    std::set<std::string> lines, ids;
    for (auto &l : d.lines)
        if (!Token(l.id) || !lines.insert(l.id).second || !ids.insert(l.id).second || l.speaker.empty() ||
            !Text(l.speaker) || !IsDialogueVoicePath(l.voice) || l.text.empty() || !Text(l.text) || l.replies.size() > 64)
            return fail("Invalid or duplicate line; each line needs a speaker and text.");
    if (!lines.count(d.entry))
        return fail("Opening line is missing.");
    auto target = [&](auto &n) { return n.empty() || lines.count(n); };
    auto cond = [](auto &cs) {
        if (cs.size() > 64)
            return false;
        for (auto &c : cs)
            if (int(c.kind) < 0 || int(c.kind) > 2 || !Token(c.key) ||
                (c.kind == DialogueConditionKind::HasItem && c.value < 1))
                return false;
        return true;
    };
    for (auto &l : d.lines)
    {
        if (!target(l.next) || (!l.next.empty() && !l.replies.empty()))
            return fail("A line must use replies or a continuation, with valid destinations.");
        for (auto &r : l.replies)
        {
            if (!Token(r.id) || !ids.insert(r.id).second || r.text.empty() || !Text(r.text) || !cond(r.conditions) ||
                r.outcomes.empty() || r.outcomes.size() > 64)
                return fail("Invalid reply or conditions.");
            bool fallback = false;
            for (auto &o : r.outcomes)
            {
                if (fallback)
                    return fail("Otherwise must be the final outcome.");
                fallback = o.conditions.empty();
                if (!target(o.next) || !cond(o.conditions) || o.actions.size() > 64)
                    return fail("Invalid outcome destination or rules.");
                size_t events = 0;
                for (auto &a : o.actions)
                {
                    if (int(a.kind) < 0 || int(a.kind) > 2 || !Token(a.key))
                        return fail("Invalid action.");
                    if (a.kind == DialogueActionKind::Event)
                        ++events;
                }
                if (events > 1)
                    return fail("Use one gameplay event per outcome.");
            }
            if (!fallback)
                return fail("Add an Otherwise outcome so the reply always has a result.");
        }
    }
    e.clear();
    return true;
}
std::string EncodeDialogue(const DialogueDocument &d)
{
    std::ostringstream o;
    o << "renegade-dialogue 2 " << std::quoted(d.projectId) << ' ' << std::quoted(d.id) << ' ' << std::quoted(d.name)
      << ' ' << std::quoted(d.entry) << '\n'
      << std::setprecision(9) << d.theme.textSize << ' ' << d.theme.panelOpacity << ' ' << d.theme.accent << ' '
      << d.lines.size() << '\n';
    for (auto &l : d.lines)
    {
        o << std::quoted(l.id) << ' ' << std::quoted(l.speaker) << ' ' << std::quoted(l.text) << ' '
          << std::quoted(l.next) << ' ' << std::quoted(l.voice) << ' ' << l.replies.size() << '\n';
        for (auto &r : l.replies)
        {
            o << std::quoted(r.id) << ' ' << std::quoted(r.text) << ' ' << r.once << ' ';
            Conditions(o, r.conditions);
            o << r.outcomes.size() << '\n';
            for (auto &v : r.outcomes)
            {
                o << v.matchAll << ' ';
                Conditions(o, v.conditions);
                o << std::quoted(v.next) << ' ' << v.actions.size() << ' ';
                for (auto &a : v.actions)
                    o << int(a.kind) << ' ' << std::quoted(a.key) << ' ' << a.value << ' ';
                o << '\n';
            }
        }
    }
    return o.str();
}
bool DecodeDialogue(const std::string &bytes, DialogueDocument &out, std::string &e)
{
    e = "Invalid or unsupported dialogue file.";
    if (bytes.size() > 8 * 1024 * 1024)
        return false;
    std::istringstream in(bytes);
    DialogueDocument d;
    std::string magic;
    unsigned version;
    size_t count;
    if (!(in >> magic >> version >> std::quoted(d.projectId) >> std::quoted(d.id) >> std::quoted(d.name) >>
          std::quoted(d.entry) >> d.theme.textSize >> d.theme.panelOpacity >> d.theme.accent >> count) ||
        magic != "renegade-dialogue" || (version != 1 && version != 2) || count > 4096)
        return false;
    for (size_t i = 0; i < count; ++i)
    {
        DialogueLine l;
        size_t n;
        if (!(in >> std::quoted(l.id) >> std::quoted(l.speaker) >> std::quoted(l.text) >> std::quoted(l.next)) ||
            (version >= 2 && !(in >> std::quoted(l.voice))) || !(in >> n) || n > 64)
            return false;
        for (size_t j = 0; j < n; ++j)
        {
            DialogueReply r;
            size_t m;
            if (!(in >> std::quoted(r.id) >> std::quoted(r.text) >> r.once) || !Conditions(in, r.conditions) ||
                !(in >> m) || m > 64)
                return false;
            for (size_t k = 0; k < m; ++k)
            {
                DialogueOutcome o;
                size_t a;
                if (!(in >> o.matchAll) || !Conditions(in, o.conditions) || !(in >> std::quoted(o.next) >> a) || a > 64)
                    return false;
                for (size_t q = 0; q < a; ++q)
                {
                    DialogueAction ac;
                    int kind;
                    if (!(in >> kind >> std::quoted(ac.key) >> ac.value) || kind < 0 || kind > 2)
                        return false;
                    ac.kind = DialogueActionKind(kind);
                    o.actions.push_back(ac);
                }
                r.outcomes.push_back(o);
            }
            l.replies.push_back(r);
        }
        d.lines.push_back(l);
    }
    in >> std::ws;
    if (!in.eof() || !ValidateDialogue(d, e))
        return false;
    out = std::move(d);
    e.clear();
    return true;
}
std::string EncodeDialogueMemory(const DialogueMemory &m)
{
    std::ostringstream o;
    o << "renegade-dialogue-memory 1 " << m.values.size() << ' ';
    for (auto &v : m.values)
        o << std::quoted(v.first) << ' ' << v.second << ' ';
    o << m.usedReplies.size() << ' ';
    for (auto &v : m.usedReplies)
        o << std::quoted(v) << ' ';
    return o.str();
}
bool DecodeDialogueMemory(const std::string &b, DialogueMemory &out, std::string &e)
{
    e = "Invalid dialogue memory.";
    if (b.size() > 4 * 1024 * 1024)
        return false;
    std::istringstream in(b);
    DialogueMemory m;
    std::string magic, k;
    unsigned v;
    size_t n;
    int value;
    if (!(in >> magic >> v >> n) || magic != "renegade-dialogue-memory" || v != 1 || n > 16384)
        return false;
    for (size_t i = 0; i < n; ++i)
    {
        if (!(in >> std::quoted(k) >> value) || !Token(k) || !m.values.emplace(k, value).second)
            return false;
    }
    if (!(in >> n) || n > 16384)
        return false;
    std::set<std::string> ids;
    for (size_t i = 0; i < n; ++i)
    {
        if (!(in >> std::quoted(k)) || !Token(k, 257) || !ids.insert(k).second)
            return false;
        m.usedReplies.push_back(k);
    }
    in >> std::ws;
    if (!in.eof())
        return false;
    out = std::move(m);
    e.clear();
    return true;
}
const DialogueLine *DialogueSession::Line() const
{
    for (auto &l : document_.lines)
        if (l.id == current_)
            return &l;
    return nullptr;
}
bool DialogueSession::Start(const DialogueDocument &d, DialogueMemory &m, std::string &e)
{
    if (!ValidateDialogue(d, e))
        return false;
    document_ = d;
    memory_ = &m;
    current_ = d.entry;
    ++visit_;
    return true;
}
bool DialogueSession::Matches(const std::vector<DialogueCondition> &cs, bool all) const
{
    if (cs.empty())
        return true;
    for (auto &c : cs)
    {
        int value = 0;
        if (memory_)
        {
            auto it = memory_->values.find(c.key);
            if (it != memory_->values.end())
                value = it->second;
        }
        bool pass = c.kind == DialogueConditionKind::HasItem        ? items_ && items_(c.key, c.value)
                    : c.kind == DialogueConditionKind::ValueAtLeast ? value >= c.value
                                                                    : value == c.value;
        if (all && !pass)
            return false;
        if (!all && pass)
            return true;
    }
    return all;
}
DialogueView DialogueSession::View() const
{
    DialogueView v;
    v.theme = document_.theme;
    if (auto *l = Line())
    {
        v.active = true;
        v.speaker = l->speaker;
        v.text = l->text;
        v.voice = l->voice;
        v.voiceKey = document_.id + ":" + l->id + ":" + std::to_string(visit_);
        for (auto &r : l->replies)
        {
            auto key = document_.id + ":" + r.id;
            if (r.once && memory_ &&
                std::find(memory_->usedReplies.begin(), memory_->usedReplies.end(), key) != memory_->usedReplies.end())
                continue;
            if (Matches(r.conditions))
                v.replies.emplace_back(r.id, r.text);
        }
    }
    return v;
}
bool DialogueSession::Continue(std::string &e)
{
    auto *l = Line();
    if (!l || !View().replies.empty())
    {
        e = "Choose an available reply.";
        return false;
    }
    current_ = l->next;
    ++visit_;
    e.clear();
    return true;
}
bool DialogueSession::Choose(const std::string &id, std::string &e)
{
    auto view = View();
    if (std::none_of(view.replies.begin(), view.replies.end(), [&](auto &r) { return r.first == id; }))
    {
        e = "Reply is no longer available.";
        return false;
    }
    auto *l = Line();
    for (auto &r : l->replies)
        if (r.id == id)
        {
            for (auto &o : r.outcomes)
                if (Matches(o.conditions, o.matchAll))
                {
                    std::vector<DialogueAction> batch;
                    for (auto &a : o.actions)
                        if (a.kind == DialogueActionKind::Event)
                            batch.push_back(a);
                    if (!batch.empty() && (!events_ || !events_(batch, e)))
                    {
                        if (e.empty())
                            e = "No gameplay handler is registered for this action.";
                        return false;
                    }
                    for (auto &a : o.actions)
                        if (a.kind != DialogueActionKind::Event)
                            memory_->values[a.key] = a.value;
                    if (r.once)
                        memory_->usedReplies.push_back(document_.id + ":" + r.id);
                    current_ = o.next;
                    ++visit_;
                    e.clear();
                    return true;
                }
        }
    e = "No matching outcome.";
    return false;
}
DialogueDocument MakeGuardDialogue(const std::string &p, const std::string &id)
{
    DialogueDocument d;
    d.projectId = p;
    d.id = id;
    d.name = "Gate Guard";
    d.entry = "opening";
    DialogueLine l;
    l.id = "opening";
    l.speaker = "Guard";
    l.text = "What business have you here?";
    DialogueReply work;
    work.id = "work";
    work.text = "I am looking for work.";
    work.outcomes.push_back({{}, true, {}, "captain"});
    DialogueReply gate;
    gate.id = "gate";
    gate.text = "Let me through.";
    gate.outcomes.push_back({{{DialogueConditionKind::FlagEquals, "has_permit", 1}},
                             true,
                             {{DialogueActionKind::SetFlag, "access_granted", 1}},
                             "permit"});
    gate.outcomes.push_back({{}, true, {}, "refused"});
    DialogueReply bye;
    bye.id = "bye";
    bye.text = "Never mind.";
    bye.outcomes.push_back({});
    l.replies = {work, gate, bye};
    d.lines.push_back(l);
    for (auto x :
         std::vector<std::pair<std::string, std::string>>{{"captain", "Speak to the captain in the courtyard."},
                                                          {"permit", "Everything seems in order."},
                                                          {"refused", "Come back with a permit."}})
    {
        DialogueLine a;
        a.id = x.first;
        a.speaker = "Guard";
        a.text = x.second;
        if (a.id == "refused")
            a.next = "opening";
        d.lines.push_back(a);
    }
    return d;
}
} // namespace renegade::bridge