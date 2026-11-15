#include "DialogueEditorWindow.h"
#include "RenegadeStudioChrome.h"
#include "renegade/bridge/DialogueAssetService.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/screen/DialoguePanel.h"
#include <algorithm>
#include <set>
namespace renegade::studio
{
using namespace bridge;
struct DialogueEditorWindow::Impl
{
    DialogueEditorWindow *owner;
    StudioSession *session = nullptr;
    wi::RenderPath2D *path = nullptr;
    DialogueDocument doc;
    DialogueMemory memory;
    DialogueSession preview;
    screen::DialoguePanel panel;
    std::string project, root, saved, status;
    bool advanced = false, sync = false, refreshPreview = false;
    struct Ref
    {
        int l = -1, r = -1, o = -1, c = -1, a = -1;
    };
    Ref selected;
    std::vector<Ref> refs;
    std::vector<std::string> rowKeys;
    std::map<std::string, bool> expanded;
    struct LayoutItem
    {
        wi::gui::Widget *widget;
        XMFLOAT2 pos, size;
    };
    std::vector<LayoutItem> layout;
    float uiScale = 1;
    std::vector<DialogueDocument> assets;
    std::vector<DialogueDocument> undo, redo;
    RenegadeButton save, newDoc, load, discard, assign, addLine, addReply, remove, undoButton, redoButton, play,
        addOutcome, addCondition, addAction;
    RenegadeCheckBox advancedBox, onceBox, permitBox, matchAllBox;
    RenegadeComboBox library, next, kind;
    RenegadeTextInputField name, speaker, text, key, value, textSize, opacity;
    wi::gui::TreeList tree;
    wi::gui::Label help, selectionLabel;
    explicit Impl(DialogueEditorWindow *w) : owner(w)
    {
    }
    template <class W> void Add(W &w, float x, float y, float width, float height)
    {
        w.SetPos({x, y});
        w.SetSize({width, height});
        owner->AddWidget(&w);
        layout.push_back({&w, {x, y}, {width, height}});
    }
    void Button(RenegadeButton &b, const char *label, float x, float y, float w, std::function<void()> fn)
    {
        b.Create(label);
        b.SetRenderTextSize(13);
        Add(b, x, y, w, 30);
        b.OnClick([fn](auto &) { fn(); });
    }
    bool Dirty() const
    {
        return EncodeDialogue(doc) != saved;
    }
    void Before()
    {
        undo.push_back(doc);
        if (undo.size() > 128)
            undo.erase(undo.begin());
        redo.clear();
    }
    DialogueLine *L()
    {
        return selected.l >= 0 && size_t(selected.l) < doc.lines.size() ? &doc.lines[selected.l] : nullptr;
    }
    DialogueReply *R()
    {
        auto *l = L();
        return l && selected.r >= 0 && size_t(selected.r) < l->replies.size() ? &l->replies[selected.r] : nullptr;
    }
    DialogueOutcome *O()
    {
        auto *r = R();
        return r && selected.o >= 0 && size_t(selected.o) < r->outcomes.size() ? &r->outcomes[selected.o] : nullptr;
    }
    DialogueCondition *C()
    {
        if (selected.c < 0)
            return nullptr;
        auto *o = O();
        auto *r = R();
        auto *cs = o ? &o->conditions : r ? &r->conditions : nullptr;
        return cs && size_t(selected.c) < cs->size() ? &(*cs)[selected.c] : nullptr;
    }
    DialogueAction *A()
    {
        auto *o = O();
        return o && selected.a >= 0 && size_t(selected.a) < o->actions.size() ? &o->actions[selected.a] : nullptr;
    }
    void Changed()
    {
        Rebuild();
        RefreshFields();
    }
    void Row(std::string label, int depth, Ref ref)
    {
        wi::gui::TreeList::Item item;
        item.name = std::move(label);
        item.level = depth;
        item.userdata = refs.size();
        const auto key = doc.id + "/" + (ref.l >= 0 && size_t(ref.l) < doc.lines.size() ? doc.lines[ref.l].id : "") +
                         "/" + std::to_string(ref.r) + "/" + std::to_string(ref.o) + "/" + std::to_string(ref.c) + "/" +
                         std::to_string(ref.a) + "/" + std::to_string(depth);
        auto found = expanded.find(key);
        item.open = found == expanded.end() ? true : found->second;
        rowKeys.push_back(key);
        refs.push_back(ref);
        tree.AddItem(item);
    }
    void Rebuild()
    {
        for (size_t n = 0; n < rowKeys.size() && n < size_t(tree.GetItemCount()); ++n)
            expanded[rowKeys[n]] = tree.GetItem(int(n)).open;
        tree.ClearItems();
        refs.clear();
        rowKeys.clear();
        std::set<std::string> seen;
        std::function<void(int, int)> line = [&](int index, int depth) {
            if (depth > 24)
                return;
            auto &l = doc.lines[index];
            if (!seen.insert(l.id).second)
            {
                Row("Continue to: " + l.speaker + " - " + l.text.substr(0, 40), depth, {index});
                return;
            }
            Row(l.speaker + ": " + l.text, depth, {index});
            auto destination = [&](const std::string &id, int level) {
                for (size_t k = 0; k < doc.lines.size(); ++k)
                    if (doc.lines[k].id == id)
                    {
                        line(int(k), level);
                        return;
                    }
                if (id.empty())
                    Row("End conversation", level, {index});
            };
            for (size_t j = 0; j < l.replies.size(); ++j)
            {
                auto &r = l.replies[j];
                size_t conditions = r.conditions.size(), actions = 0;
                for (auto &o : r.outcomes)
                {
                    conditions += o.conditions.size();
                    actions += o.actions.size();
                }
                auto label = "REPLY: " + r.text;
                if (conditions || actions)
                    label +=
                        "  [" + std::to_string(conditions) + " conditions / " + std::to_string(actions) + " actions]";
                Row(label, depth + 1, {index, int(j)});
                if (advanced)
                    for (size_t c = 0; c < r.conditions.size(); ++c)
                        Row("Show if: " + r.conditions[c].key, depth + 2, {index, int(j), -1, int(c)});
                for (size_t q = 0; q < r.outcomes.size(); ++q)
                {
                    auto &o = r.outcomes[q];
                    if (advanced)
                    {
                        Row(o.conditions.empty() ? "Otherwise / Continue" : "IF conditions match", depth + 2,
                            {index, int(j), int(q)});
                        for (size_t c = 0; c < o.conditions.size(); ++c)
                            Row("Condition: " + o.conditions[c].key, depth + 3, {index, int(j), int(q), int(c)});
                        for (size_t a = 0; a < o.actions.size(); ++a)
                            Row("Action: " + o.actions[a].key, depth + 3, {index, int(j), int(q), -1, int(a)});
                    }
                    destination(o.next, depth + (advanced ? 3 : 2));
                }
            }
            if (!l.next.empty())
                destination(l.next, depth + 1);
        };
        for (size_t i = 0; i < doc.lines.size(); ++i)
            if (doc.lines[i].id == doc.entry)
                line(int(i), 0);
        for (size_t i = 0; i < doc.lines.size(); ++i)
            if (!seen.count(doc.lines[i].id))
            {
                Row("Shared / unlinked section", 0, {int(i)});
                line(int(i), 1);
            }
    }
    void RefreshFields()
    {
        sync = true;
        auto *l = L();
        auto *r = R();
        auto *o = O();
        auto *c = C();
        auto *a = A();
        name.SetValue(doc.name);
        textSize.SetValue(doc.theme.textSize);
        opacity.SetValue(doc.theme.panelOpacity);
        speaker.SetValue(l ? l->speaker : "");
        text.SetValue(r && !o ? r->text : l ? l->text : "");
        selectionLabel.SetText(c ? "CONDITION" : a ? "ACTION" : o ? "OUTCOME" : r ? "PLAYER REPLY" : "NPC LINE");
        speaker.SetVisible(l && !r);
        text.SetVisible(l && !o && !c && !a);
        next.ClearItems();
        next.AddItem("End conversation");
        for (auto &line : doc.lines)
            next.AddItem((line.id == doc.entry ? std::string("Opening") : line.speaker) + ": " +
                         line.text.substr(0, 40));
        std::string destination = o                              ? o->next
                                  : r && r->outcomes.size() == 1 ? r->outcomes[0].next
                                  : l && !r                      ? l->next
                                                                 : "";
        int idx = 0;
        for (size_t i = 0; i < doc.lines.size(); ++i)
            if (doc.lines[i].id == destination)
                idx = int(i + 1);
        next.SetSelected(idx);
        next.SetVisible((o || r && r->outcomes.size() == 1 || l && !r) && !c && !a);
        matchAllBox.SetVisible(advanced && o && !c && !a);
        matchAllBox.SetCheck(o ? o->matchAll : true);
        onceBox.SetVisible(advanced && r && !o);
        onceBox.SetCheck(r ? r->once : false);
        kind.ClearItems();
        if (c)
        {
            kind.AddItem("Flag equals");
            kind.AddItem("Value at least");
            kind.AddItem("Has inventory item");
            kind.SetSelected(int(c->kind));
        }
        else if (a)
        {
            kind.AddItem("Set flag");
            kind.AddItem("Set value");
            kind.AddItem("Gameplay event (script)");
            kind.SetSelected(int(a->kind));
        }
        kind.SetVisible(advanced && (c || a));
        key.SetVisible(advanced && (c || a));
        value.SetVisible(advanced && (c || a));
        key.SetValue(c ? c->key : a ? a->key : "");
        value.SetValue(c ? c->value : a ? a->value : 1);
        addOutcome.SetVisible(advanced);
        addCondition.SetVisible(advanced);
        addAction.SetVisible(advanced);
        sync = false;
    }
    void Start()
    {
        memory = {};
        memory.values["has_permit"] = permitBox.GetCheck() ? 1 : 0;
        preview.SetItemQuery([this](auto &key, int count) {
            auto i = memory.values.find("item." + key);
            return i != memory.values.end() && i->second >= count;
        });
        preview.SetEventSink([this](auto &actions, std::string &) {
            status = "Preview event: " + actions.front().key;
            return true;
        });
        if (preview.Start(doc, memory, status))
            refreshPreview = true;
    }
    void Save()
    {
        if (!SaveDialogueAsset(root, doc, status))
            return;
        auto declaration = "data:Content/Dialogue/" + doc.id + ".rdialogue";
        auto include = session->Projects().CurrentProject().alwaysInclude;
        if (std::find(include.begin(), include.end(), declaration) == include.end())
        {
            include.push_back(declaration);
            if (!session->Projects().SetAlwaysInclude(include))
            {
                status = "Dialogue saved; packaging registration failed: " + session->Projects().LastError();
                return;
            }
        }
        saved = EncodeDialogue(doc);
        status = "Saved conversation. Save the scene after assigning an NPC.";
        ReloadLibrary();
    }
    void ReloadLibrary()
    {
        assets = ListDialogueAssets(root, project, status);
        library.ClearItems();
        for (auto &d : assets)
            library.AddItem(d.name);
    }
    void SelectDocument(DialogueDocument d)
    {
        doc = std::move(d);
        selected = {0};
        saved = EncodeDialogue(doc);
        undo.clear();
        redo.clear();
        Changed();
        preview.Close();
        refreshPreview = true;
    }
    void Create()
    {
        Button(save, "Save", 1010, 34, 110, [this] { Save(); });
        save.SetPrimaryStyle(true);
        Button(newDoc, "New", 18, 34, 70, [this] {
            if (Dirty())
            {
                status = "Save or discard current edits first.";
                return;
            }
            auto d = MakeGuardDialogue(project, GenerateStableId());
            d.name = "New Conversation";
            SelectDocument(d);
            saved.clear();
        });
        Button(load, "Open", 305, 34, 70, [this] {
            if (Dirty())
            {
                status = "Save or discard current edits first.";
                return;
            }
            int i = library.GetSelected();
            if (i >= 0 && size_t(i) < assets.size())
                SelectDocument(assets[i]);
        });
        library.Create("Conversations");
        Add(library, 98, 34, 197, 30);
        Button(discard, "Discard", 385, 34, 80, [this] {
            DialogueDocument d;
            if (LoadDialogueAsset(root, project, doc.id, d, status))
                SelectDocument(d);
            else
            {
                // An unsaved first draft has no disk version to restore.
                preview.Close();
                refreshPreview = true;
                saved = EncodeDialogue(doc);
                owner->SetVisible(false);
            }
        });
        advancedBox.Create("Advanced");
        Add(advancedBox, 490, 40, 22, 22);
        advancedBox.OnClick([this](auto &e) {
            advanced = e.bValue;
            Changed();
        });
        Button(undoButton, "Undo", 640, 34, 70, [this] {
            if (undo.empty())
                return;
            redo.push_back(doc);
            doc = undo.back();
            undo.pop_back();
            selected = {0};
            Changed();
        });
        Button(redoButton, "Redo", 720, 34, 70, [this] {
            if (redo.empty())
                return;
            undo.push_back(doc);
            doc = redo.back();
            redo.pop_back();
            selected = {0};
            Changed();
        });
        Button(assign, "Assign to selected NPC", 800, 34, 200, [this] {
            if (Dirty())
            {
                status = "Save the conversation before assigning it.";
                return;
            }
            auto entity = session->Selection().SelectedEntity();
            if (session->Commands().Execute(
                    std::make_unique<AssignDialogueCommand>(session->Scenes().GetScene(), entity, doc.id)))
                status = "Assigned. Save the scene to keep this NPC binding.";
            else
                status = "Select an NPC or object with a transform first.";
        });
        name.Create("Conversation name");
        name.SetDescription("Name: ");
        Add(name, 80, 80, 480, 28);
        name.OnInput([this](auto &e) {
            if (sync)
                return;
            Before();
            doc.name = e.sValue;
            Rebuild();
        });
        Button(addLine, "+ NPC Line", 18, 122, 125, [this] {
            auto *l = L();
            if (!l)
                return;
            Before();
            DialogueLine n;
            n.id = GenerateStableId();
            n.speaker = l->speaker;
            n.text = "New dialogue line";
            auto *o = O();
            if (!o && R() && R()->outcomes.size() == 1)
                o = &R()->outcomes[0];
            if (o)
                o->next = n.id;
            else if (!l->replies.empty())
            {
                status = "Select an outcome to add its response.";
                undo.pop_back();
                return;
            }
            else
                l->next = n.id;
            doc.lines.push_back(n);
            selected = {int(doc.lines.size() - 1)};
            Changed();
        });
        Button(addReply, "+ Player Reply", 153, 122, 145, [this] {
            auto *l = L();
            if (!l)
                return;
            Before();
            l->next.clear();
            DialogueReply r;
            r.id = GenerateStableId();
            r.text = "New player reply";
            r.outcomes.push_back({});
            l->replies.push_back(r);
            selected.r = int(l->replies.size() - 1);
            selected.o = selected.c = selected.a = -1;
            Changed();
        });
        Button(remove, "Delete selected", 308, 122, 140, [this] {
            auto *l = L();
            if (!l)
                return;
            Before();
            auto *r = R();
            auto *o = O();
            if (C())
            {
                auto &cs = o ? o->conditions : r->conditions;
                cs.erase(cs.begin() + selected.c);
            }
            else if (A())
                o->actions.erase(o->actions.begin() + selected.a);
            else if (o)
            {
                if (r->outcomes.size() > 1)
                    r->outcomes.erase(r->outcomes.begin() + selected.o);
            }
            else if (r)
                l->replies.erase(l->replies.begin() + selected.r);
            else if (l->id != doc.entry)
            {
                auto id = l->id;
                doc.lines.erase(doc.lines.begin() + selected.l);
                for (auto &x : doc.lines)
                {
                    if (x.next == id)
                        x.next.clear();
                    for (auto &y : x.replies)
                        for (auto &z : y.outcomes)
                            if (z.next == id)
                                z.next.clear();
                }
            }
            selected = {0};
            Changed();
        });
        textSize.Create("Dialogue font size");
        textSize.SetDescription("Text size: ");
        Add(textSize, 840, 390, 260, 28);
        textSize.OnInputAccepted([this](auto &e) {
            if (sync)
                return;
            Before();
            doc.theme.textSize = std::clamp(e.fValue, 14.f, 48.f);
            if (preview.Active())
                Start();
        });
        opacity.Create("Dialogue opacity");
        opacity.SetDescription("Opacity: ");
        Add(opacity, 840, 430, 260, 28);
        opacity.OnInputAccepted([this](auto &e) {
            if (sync)
                return;
            Before();
            doc.theme.panelOpacity = std::clamp(e.fValue, .2f, 1.f);
            if (preview.Active())
                Start();
        });
        matchAllBox.Create("Match all conditions");
        Add(matchAllBox, 750, 288, 22, 22);
        matchAllBox.OnClick([this](auto &e) {
            if (sync || !O())
                return;
            Before();
            O()->matchAll = e.bValue;
            Rebuild();
        });
        tree.Create("Conversation outline");
        Add(tree, 18, 164, 690, 302);
        tree.OnSelect([this](auto &e) {
            if (e.userdata < refs.size())
            {
                selected = refs[e.userdata];
                RefreshFields();
            }
        });
        selectionLabel.Create("Selection");
        Add(selectionLabel, 735, 122, 370, 28);
        speaker.Create("Speaker");
        speaker.SetDescription("Speaker: ");
        Add(speaker, 815, 164, 285, 28);
        speaker.OnInput([this](auto &e) {
            if (sync || !L())
                return;
            Before();
            L()->speaker = e.sValue;
            Rebuild();
        });
        text.Create("Dialogue text");
        text.SetDescription("Text: ");
        Add(text, 780, 205, 320, 30);
        text.OnInput([this](auto &e) {
            if (sync || !L())
                return;
            Before();
            if (R())
                R()->text = e.sValue;
            else
                L()->text = e.sValue;
            Rebuild();
        });
        next.Create("Continue to");
        next.SetTooltip("Continue to this named section, or end the conversation.");
        Add(next, 815, 248, 285, 28);
        next.OnSelect([this](auto &e) {
            if (sync)
                return;
            auto id = e.iValue > 0 && size_t(e.iValue) <= doc.lines.size() ? doc.lines[e.iValue - 1].id : "";
            if (O())
            {
                Before();
                O()->next = id;
            }
            else if (R() && R()->outcomes.size() == 1)
            {
                Before();
                R()->outcomes[0].next = id;
            }
            else if (L() && !R() && L()->replies.empty())
            {
                Before();
                L()->next = id;
            }
            Rebuild();
        });
        onceBox.Create("Show reply only once");
        Add(onceBox, 750, 288, 22, 22);
        onceBox.OnClick([this](auto &e) {
            if (sync || !R())
                return;
            Before();
            R()->once = e.bValue;
            Rebuild();
        });
        kind.Create("Rule type");
        Add(kind, 750, 164, 350, 28);
        kind.OnSelect([this](auto &e) {
            if (sync)
                return;
            if (C())
            {
                Before();
                C()->kind = DialogueConditionKind(e.iValue);
            }
            else if (A())
            {
                Before();
                A()->kind = DialogueActionKind(e.iValue);
            }
            Rebuild();
        });
        key.Create("Key");
        key.SetDescription("Key: ");
        Add(key, 795, 205, 305, 30);
        key.OnInput([this](auto &e) {
            if (sync)
                return;
            if (C())
            {
                Before();
                C()->key = e.sValue;
            }
            else if (A())
            {
                Before();
                A()->key = e.sValue;
            }
            Rebuild();
        });
        value.Create("Value");
        value.SetDescription("Value: ");
        Add(value, 815, 248, 285, 28);
        value.OnInput([this](auto &e) {
            if (sync)
                return;
            if (C())
            {
                Before();
                C()->value = e.iValue;
            }
            else if (A())
            {
                Before();
                A()->value = e.iValue;
            }
            Rebuild();
        });
        Button(addOutcome, "+ Outcome", 735, 330, 120, [this] {
            auto *r = R();
            if (!r)
                return;
            Before();
            DialogueOutcome o;
            o.conditions.push_back({DialogueConditionKind::FlagEquals, "has_permit", 1});
            r->outcomes.insert(r->outcomes.end() - 1, o);
            selected.o = int(r->outcomes.size() - 2);
            selected.c = selected.a = -1;
            Changed();
        });
        Button(addCondition, "+ Condition", 865, 330, 135, [this] {
            auto *r = R();
            if (!r)
                return;
            Before();
            auto *o = O();
            auto &cs = o ? o->conditions : r->conditions;
            cs.push_back({DialogueConditionKind::FlagEquals, "flag", 1});
            selected.c = int(cs.size() - 1);
            selected.a = -1;
            Changed();
        });
        Button(addAction, "+ Action", 1010, 330, 100, [this] {
            auto *o = O();
            if (!o)
            {
                status = "Select an outcome first.";
                return;
            }
            Before();
            o->actions.push_back({DialogueActionKind::SetFlag, "flag", 1});
            selected.a = int(o->actions.size() - 1);
            selected.c = -1;
            Changed();
        });
        Button(play, "Preview from start", 18, 480, 190, [this] { Start(); });
        permitBox.Create("Test: has_permit");
        Add(permitBox, 250, 486, 22, 22);
        permitBox.OnClick([this](auto &) { Start(); });
        for (auto *input :
             std::vector<RenegadeTextInputField *>{&name, &speaker, &text, &key, &value, &textSize, &opacity})
            input->SetCancelInputEnabled(false);
        help.Create("Dialogue status");
        help.font.params.size = 13;
        Add(help, 18, 520, 1100, 38);
    }
};
DialogueEditorWindow::DialogueEditorWindow() : impl_(std::make_unique<Impl>(this))
{
}
DialogueEditorWindow::~DialogueEditorWindow()
{
    if (impl_ && impl_->path)
        impl_->panel.Hide(*impl_->path);
}
void DialogueEditorWindow::Create()
{
    wi::gui::Window::Create("Dialogue Editor", WindowControls::CLOSE | WindowControls::MOVE);
    SetSize({1150, 860});
    SetColor(wi::Color(31, 34, 36, 255));
    impl_->Create();
    SetVisible(false);
    OnClose([this](auto &) {
        if (impl_->Dirty())
        {
            SetVisible(true);
            impl_->RefreshFields();
            impl_->status = "Save or discard your conversation before closing.";
        }
    });
}
void DialogueEditorWindow::Open(StudioSession &s, wi::RenderPath2D &path)
{
    if (!s.Projects().HasProject())
        return;
    auto &i = *impl_;
    i.session = &s;
    i.path = &path;
    i.project = s.Projects().CurrentProject().projectId;
    i.root = s.Projects().CurrentProject().rootPath;
    i.ReloadLibrary();
    auto id = AssignedDialogue(s.Scenes().GetScene(), s.Selection().SelectedEntity());
    DialogueDocument d;
    if (!id.empty() && LoadDialogueAsset(i.root, i.project, id, d, i.status))
        i.SelectDocument(d);
    else if (!i.assets.empty())
        i.SelectDocument(i.assets.front());
    else
    {
        i.SelectDocument(MakeGuardDialogue(i.project, GenerateStableId()));
        i.saved.clear();
    }
    i.uiScale =
        std::clamp(std::min((path.GetLogicalWidth() - 32) / 1150.f, (path.GetLogicalHeight() - 64) / 860.f), .65f, 1.f);
    SetSize({1150 * i.uiScale, 860 * i.uiScale});
    SetPos({std::max(8.f, (path.GetLogicalWidth() - 1150 * i.uiScale) * .5f), 24});
    for (auto &item : i.layout)
    {
        item.widget->SetPos({item.pos.x * i.uiScale, item.pos.y * i.uiScale});
        item.widget->SetSize({item.size.x * i.uiScale, item.size.y * i.uiScale});
    }
    SetVisible(true);
    i.RefreshFields();
}
void DialogueEditorWindow::Tick(StudioSession &s, wi::RenderPath2D &path)
{
    auto &i = *impl_;
    if (!IsVisible() || !s.Projects().HasProject() || s.Projects().CurrentProject().projectId != i.project)
    {
        i.panel.Hide(path);
        if (IsVisible())
            SetVisible(false);
        return;
    }
    auto action = i.panel.TakeAction();
    if (!action.empty())
    {
        if (action == "@close")
            i.preview.Close();
        else if (action == "@continue")
            i.preview.Continue(i.status);
        else if (action != "@refresh")
            i.preview.Choose(action, i.status);
        i.refreshPreview = true;
    }
    if (i.refreshPreview)
    {
        i.refreshPreview = false;
        auto viewport =
            ScreenRect{GetPos().x + 18 * i.uiScale, GetPos().y + 570 * i.uiScale, 1110 * i.uiScale, 260 * i.uiScale};
        if (i.preview.Active())
        {
            if (i.panel.Show(i.preview.View(), i.project, i.root, path, viewport, i.status))
                i.panel.EmbedInWindow(path, *this);
        }
        else
            i.panel.Hide(path);
    }
    else if (i.preview.Active())
        i.panel.Layout(path,
                       {GetPos().x + 18 * i.uiScale, GetPos().y + 570 * i.uiScale, 1110 * i.uiScale, 260 * i.uiScale});
    i.help.SetText((i.Dirty() ? "UNSAVED  |  " : "") + i.status);
    i.undoButton.SetEnabled(!i.undo.empty());
    i.redoButton.SetEnabled(!i.redo.empty());
}
} // namespace renegade::studio
