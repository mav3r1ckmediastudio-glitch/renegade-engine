#include "DialogueEditorWindow.h"
#include "StudioTheme.h"
#include "renegade/bridge/DialogueAssetService.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/screen/DialoguePanel.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <set>
#include <filesystem>
namespace renegade::studio
{
using namespace bridge;
namespace
{
// Reuse Studio's native typeface and palette; no private turquoise theme.
using theme::Accent;
using theme::Panel;
using theme::Text;
constexpr auto Inset = theme::Idle;
void Rect(float x, float y, float w, float h, wi::Color c, wi::graphics::CommandList cmd)
{
    wi::image::Params p(x, y, w, h, c);
    wi::image::Draw(nullptr, p, cmd);
}
class DialogueButton : public wi::gui::Button
{
  public:
    bool primary = false, accented = false;
    void Render(const wi::Canvas &, wi::graphics::CommandList cmd) const override
    {
        if (!IsVisible())
            return;
        const bool engaged = GetState() == wi::gui::FOCUS || GetState() == wi::gui::ACTIVE;
        Rect(translation.x, translation.y, scale.x, scale.y, engaged || accented || primary ? Accent : theme::Border, cmd);
        Rect(translation.x + 1, translation.y + 1, scale.x - 2, scale.y - 2,
             primary ? Accent : theme::Focus, cmd);
        wi::font::Params fp(translation.x + scale.x * .5f, translation.y + scale.y * .5f, theme::ControlSize, wi::font::WIFALIGN_CENTER,
                            wi::font::WIFALIGN_CENTER,
                            !IsEnabled() ? wi::Color(125, 130, 132, 255)
                            : primary    ? theme::Text
                                         : Text);
        fp.bolden = .12f;
        wi::font::Draw(GetText(), fp, cmd);
    }
};
class DialogueCheckBox : public wi::gui::CheckBox
{
  public:
    void Render(const wi::Canvas &, wi::graphics::CommandList cmd) const override
    {
        if (!IsVisible())
            return;
        Rect(translation.x, translation.y, 18, 18, GetState() == wi::gui::FOCUS ? Accent : theme::Border,
             cmd);
        Rect(translation.x + 1, translation.y + 1, 16, 16, Inset, cmd);
        if (GetCheck())
            Rect(translation.x + 4, translation.y + 4, 10, 10, Accent, cmd);
        wi::font::Params fp(translation.x + 27, translation.y + 9, 13, wi::font::WIFALIGN_LEFT,
                            wi::font::WIFALIGN_CENTER, IsEnabled() ? Text : wi::Color(120, 128, 129, 255));
        wi::font::Draw(GetName(), fp, cmd);
    }
};
// Native selection/scroll ownership, with script-sized rows rather than the
// upstream TreeList's fixed 20-pixel hierarchy geometry. Render and hit-testing
// share exactly the same visible-row list and row height.
class DialogueOutline : public wi::gui::TreeList
{
  public:
    float uiScale = 1;
    std::vector<int> VisibleRows() const
    {
        std::vector<int> rows;
        int level = 0;
        bool open = true;
        for (size_t n = 0; n < items.size(); ++n)
        {
            if (!open && items[n].level > level)
                continue;
            rows.push_back(int(n));
            level = items[n].level;
            open = items[n].open;
        }
        return rows;
    }
    float RowHeight() const { return 52 * uiScale; }
    void Update(const wi::Canvas &canvas, float dt) override
    {
        if (!IsVisible()) return;
        Widget::Update(canvas, dt);
        const auto rows = VisibleRows();
        scrollbar.SetListLength(float(rows.size()) * RowHeight());
        scrollbar.SetOverScroll(0);
        scrollbar.SetSize({10 * uiScale, scale.y});
        scrollbar.SetPos({translation.x + scale.x - 10 * uiScale, translation.y});
        scrollbar.Update(canvas, dt);
        if (!IsEnabled() || dt <= 0) return;
        auto pointer = GetPointerHitbox();
        const bool inside = hitBox.intersects(pointer);
        if (state == wi::gui::ACTIVE || state == wi::gui::DEACTIVATING) Deactivate();
        state = inside ? wi::gui::FOCUS : wi::gui::IDLE;
        const float wheel = wi::input::GetPointer().z;
        if (inside && wheel != 0 && scrollbar.IsScrollbarRequired())
            scrollbar.Scroll(wheel * RowHeight());
        item_highlight = -1;
        if (!inside || scrollbar.GetState() > wi::gui::IDLE ||
            pointer.pos.x >= translation.x + scale.x - 12 * uiScale) return;
        const int visible = int((pointer.pos.y - translation.y - scrollbar.GetOffset()) / RowHeight());
        if (visible < 0 || size_t(visible) >= rows.size()) return;
        const int index = rows[visible];
        item_highlight = index;
        if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
        {
            Activate();
            const float left = translation.x + std::min(items[index].level, 5) * 24 * uiScale;
            if (pointer.pos.x < left + 24 * uiScale && DoesItemHaveChildren(index))
                items[index].open = !items[index].open;
            else
            {
                for (auto &item : items) item.selected = false;
                items[index].selected = true;
                wi::gui::EventArgs args;
                args.iValue = index;
                args.userdata = items[index].userdata;
                if (onSelect) onSelect(args);
            }
        }
    }
    void Render(const wi::Canvas &canvas, wi::graphics::CommandList cmd) const override
    {
        if (!IsVisible()) return;
        ApplyScissor(canvas, scissorRect, cmd);
        Rect(translation.x, translation.y, scale.x, scale.y, Panel, cmd);
        const auto rows = VisibleRows();
        for (size_t n = 0; n < rows.size(); ++n)
        {
            const int index = rows[n];
            const auto &item = items[index];
            const float y = translation.y + scrollbar.GetOffset() + float(n) * RowHeight();
            if (y + RowHeight() < translation.y || y > translation.y + scale.y) continue;
            const float left = translation.x + std::min(item.level, 5) * 24 * uiScale;
            const float width = translation.x + scale.x - left - 16 * uiScale;
            const bool reply = item.name.rfind("REPLY:", 0) == 0;
            const auto colon = item.name.find(": ");
            const bool spoken = !reply && colon != std::string::npos &&
                                item.name.rfind("Continue to:", 0) != 0 &&
                                item.name.rfind("Condition:", 0) != 0 &&
                                item.name.rfind("Action:", 0) != 0 &&
                                item.name.rfind("Show if:", 0) != 0;
            const auto fill = item.selected ? theme::Selected :
                index == item_highlight ? theme::Active : theme::Focus;
            Rect(left + 24 * uiScale, y + 3 * uiScale, width - 24 * uiScale,
                 RowHeight() - 6 * uiScale, item.selected ? Accent : theme::Border, cmd);
            Rect(left + 25 * uiScale, y + 4 * uiScale, width - 26 * uiScale,
                 RowHeight() - 8 * uiScale, fill, cmd);
            auto draw = [&](std::string label, float x, float top, int size, wi::Color color, float bold = 0.f) {
                wi::font::Params fp(x, top, size);
                fp.scaling = uiScale;
                fp.color = color;
                fp.bolden = bold;
                const float available = translation.x + scale.x - x - 28 * uiScale;
                if (wi::font::TextWidth(label, fp) > available)
                {
                    std::vector<size_t> ends{0};
                    for (size_t k = 1; k < label.size(); ++k)
                        if ((static_cast<unsigned char>(label[k]) & 0xc0) != 0x80) ends.push_back(k);
                    size_t low = 0, high = ends.size() - 1;
                    while (low < high)
                    {
                        const auto mid = (low + high + 1) / 2;
                        if (wi::font::TextWidth(label.substr(0, ends[mid]) + "...", fp) <= available) low = mid;
                        else high = mid - 1;
                    }
                    label.resize(ends[low]); label += "...";
                }
                wi::font::Draw(label, fp, cmd);
            };
            if (DoesItemHaveChildren(index))
                draw(item.open ? "v" : ">", left + 5 * uiScale, y + 17 * uiScale, 14, theme::Muted);
            const float x = left + 38 * uiScale;
            if (spoken)
            {
                draw(item.name.substr(0, colon), x, y + 7 * uiScale, theme::CaptionSize, Accent, .15f);
                draw(item.name.substr(colon + 2), x, y + 24 * uiScale, theme::ScriptSize, Text, .12f);
            }
            else
                draw(reply ? item.name.substr(7) : item.name, x, y + 17 * uiScale,
                     reply ? theme::ScriptSize : theme::ControlSize, Text, reply ? .12f : 0.f);
        }
        ApplyScissor(canvas, scissorRect, cmd);
        scrollbar.Render(canvas, cmd);
    }
};
class DialogueText : public wi::gui::TextInputField
{
  public:
    void Render(const wi::Canvas &canvas, wi::graphics::CommandList cmd) const override
    {
        if (!IsVisible()) return;
        if (GetState() == wi::gui::ACTIVE)
        {
            // Preserve native editing, selection, clipboard and IME behaviour.
            wi::gui::TextInputField::Render(canvas, cmd);
            return;
        }
        ApplyScissor(canvas, scissorRect, cmd);
        Rect(translation.x, translation.y, scale.x, scale.y,
             GetState() == wi::gui::FOCUS ? Accent : theme::Border, cmd);
        Rect(translation.x + 1, translation.y + 1, scale.x - 2, scale.y - 2, Inset, cmd);
        auto fp = font.params;
        fp.position = {translation.x + 12, translation.y + 12, 0};
        fp.h_align = wi::font::WIFALIGN_LEFT;
        fp.v_align = wi::font::WIFALIGN_TOP;
        fp.h_wrap = scale.x - 24;
        fp.color = Text;
        wi::font::Draw(font.GetTextA(), fp, cmd);
    }
};
} // namespace
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
    bool advanced = false, sync = false, refreshPreview = false, selectionPlaced = false;
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
    float uiScale = 1, canvasWidth = 0, canvasHeight = 0, canvasDpi = 0;
    std::vector<DialogueDocument> assets;
    std::vector<DialogueDocument> undo, redo;
    DialogueButton save, newDoc, load, discard, assign, addLine, addReply, remove, undoButton, redoButton, play,
        addOutcome, addCondition, addAction;
    DialogueButton advancedBox;
    DialogueCheckBox onceBox, permitBox, matchAllBox;
    wi::gui::ComboBox library, next, kind, voice;
    std::vector<std::string> voicePaths;
    wi::gui::TextInputField name, speaker, key, value, textSize, opacity;
    DialogueText text;
    DialogueOutline tree;
    wi::gui::Label help, selectionLabel, previewHeading, previewHint, appearanceHeading, conversationHeading, textHeading, destinationHeading, ruleSummary, voiceHeading;
    DialogueButton showAdvanced, audition;
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
    void Button(DialogueButton &b, const char *label, float x, float y, float w, std::function<void()> fn)
    {
        b.Create(label);
        Add(b, x, y, w, 32);
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
        item.selected = !selectionPlaced && ref.l == selected.l && ref.r == selected.r && ref.o == selected.o &&
                        ref.c == selected.c && ref.a == selected.a;
        selectionPlaced = selectionPlaced || item.selected;
        const auto key = doc.id + "/" + (ref.l >= 0 && size_t(ref.l) < doc.lines.size() ? doc.lines[ref.l].id : "") +
                         "/" + std::to_string(ref.r) + "/" + std::to_string(ref.o) + "/" + std::to_string(ref.c) + "/" +
                         std::to_string(ref.a) + "/" + std::to_string(depth);
        auto found = expanded.find(key);
        item.open = found == expanded.end() ? (advanced || depth == 0) : found->second;
        rowKeys.push_back(key);
        refs.push_back(ref);
        tree.AddItem(item);
    }
    void Rebuild()
    {
        for (size_t n = 0; n < rowKeys.size() && n < size_t(tree.GetItemCount()); ++n)
            expanded[rowKeys[n]] = tree.GetItem(int(n)).open;
        tree.ClearItems();
        selectionPlaced = false;
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
                if (advanced && (conditions || actions))
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
        advancedBox.primary = advanced;
        advancedBox.SetText(advanced ? "Advanced: On" : "Advanced");
        auto *l = L();
        auto *r = R();
        auto *o = O();
        auto *c = C();
        auto *a = A();
        name.SetValue(doc.name);
        textSize.SetValue(doc.theme.textSize);
        opacity.SetValue(doc.theme.panelOpacity);
        opacity.SetVisible(advanced && r && !c && !a);
        textSize.SetVisible(advanced && r && !c && !a);
        appearanceHeading.SetVisible(advanced && r && !c && !a);
        permitBox.SetVisible(advanced);
        speaker.SetValue(l ? l->speaker : "");
        int voiceIndex = 0;
        if (l) for (size_t n = 0; n < voicePaths.size(); ++n)
            if (voicePaths[n] == l->voice) voiceIndex = int(n + 1);
        voice.SetSelectedWithoutCallback(voiceIndex);
        voice.SetVisible(l && !r);
        voiceHeading.SetVisible(l && !r);
        audition.SetVisible(l && !r);
        audition.SetEnabled(l && !l->voice.empty());
        text.SetValue(r && !o ? r->text : l ? l->text : "");
        textHeading.SetVisible(l && !o && !c && !a);
        destinationHeading.SetVisible((o || r && r->outcomes.size() == 1 || l && !r) && !c && !a);
        ruleSummary.SetText(r && r->outcomes.size() > 1 ? "Conditional replies are preserved." : "");
        ruleSummary.SetVisible(!advanced && r && r->outcomes.size() > 1);
        showAdvanced.SetVisible(!advanced && r && r->outcomes.size() > 1);
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
        addOutcome.SetVisible(advanced && r && !c && !a);
        addCondition.SetVisible(advanced && r && !c && !a);
        addAction.SetVisible(advanced && o && !c && !a);
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
        for (const auto &line : doc.lines)
        {
            if (line.voice.empty()) continue;
            const auto audioDeclaration = "audio:" + line.voice;
            if (std::find(include.begin(), include.end(), audioDeclaration) == include.end())
                include.push_back(audioDeclaration);
        }
        if (std::find(include.begin(), include.end(), declaration) == include.end())
            include.push_back(declaration);
        if (!session->Projects().SetAlwaysInclude(include))
        {
            status = "Dialogue saved; packaging registration failed: " + session->Projects().LastError();
            return;
        }
        saved = EncodeDialogue(doc);
        status = "Saved conversation. Save the scene after assigning an NPC.";
        ReloadLibrary();
        RefreshFields();
    }
    void ReloadLibrary()
    {
        assets = ListDialogueAssets(root, project, status);
        voicePaths.clear();
        voice.ClearItems();
        voice.AddItem("No voice clip");
        namespace fs = std::filesystem;
        std::error_code ec;
        const auto audioFolder = fs::u8path(root) / "Content/Audio";
        if (fs::exists(audioFolder, ec))
            for (fs::recursive_directory_iterator it(audioFolder, fs::directory_options::skip_permission_denied, ec), end;
                 it != end && !ec && voicePaths.size() < 512; it.increment(ec))
            {
                if (!it->is_regular_file(ec)) continue;
                auto rel = it->path().lexically_relative(fs::u8path(root)).generic_u8string();
                std::string absolute, ignored;
                if (IsDialogueVoicePath(rel) && ResolveDialogueVoicePath(root, rel, absolute, ignored))
                    voicePaths.push_back(rel);
            }
        std::sort(voicePaths.begin(), voicePaths.end());
        for (const auto &path : voicePaths) voice.AddItem(fs::u8path(path).stem().u8string());
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
        auto heading = [this](wi::gui::Label &label, const char *title, float x, float y, float w) {
            label.Create(title);
            label.SetText(title);
            Add(label, x, y, w, 24);
        };
        heading(previewHeading, "In-game preview", 32, 575, 340);
        heading(previewHint, "Try the conversation as your player will see it.", 32, 606, 650);
        heading(conversationHeading, "Conversation", 32, 126, 740);
        heading(textHeading, "Dialogue text", 890, 207, 360);
        heading(destinationHeading, "Continue to", 890, 334, 360);
        heading(ruleSummary, "", 890, 441, 360);
        heading(voiceHeading, "Voice clip", 890, 411, 360);
        voice.Create("Voice clip");
        voice.SetText("");
        Add(voice, 890, 438, 380, 30);
        voice.OnSelect([this](auto &e) {
            if (sync || !L() || R()) return;
            Before();
            L()->voice = e.iValue > 0 && size_t(e.iValue) <= voicePaths.size() ? voicePaths[e.iValue - 1] : "";
            RefreshFields();
        });
        Button(audition, "Preview line", 1100, 485, 170, [this] {
            if (!L()) return;
            auto selectedDocument = doc;
            selectedDocument.entry = L()->id;
            panel.StopVoice();
            if (preview.Start(selectedDocument, memory, status)) refreshPreview = true;
        });
        Button(showAdvanced, "Show in Advanced", 1075, 478, 195, [this] { advanced = true; Changed(); });
        heading(appearanceHeading, "Presentation", 890, 473, 350);
        Button(save, "Save", 1192, 20, 100, [this] { Save(); });
        save.primary = true;
        Button(newDoc, "New", 280, 20, 90, [this] {
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
        Button(load, "Open", 380, 20, 90, [this] {
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
        library.SetText("");
        Add(library, 24, 20, 242, 30);
        Button(discard, "Discard", 480, 20, 90, [this] {
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
        Button(advancedBox, "Advanced", 1030, 20, 140, [this] {
            advanced = !advanced;
            if (!advanced) selected.o = selected.c = selected.a = -1;
            Changed();
        });
        advancedBox.accented = true;
        Button(undoButton, "Undo", 824, 20, 85, [this] {
            if (undo.empty())
                return;
            redo.push_back(doc);
            doc = undo.back();
            undo.pop_back();
            selected = {0};
            Changed();
        });
        Button(redoButton, "Redo", 919, 20, 85, [this] {
            if (redo.empty())
                return;
            undo.push_back(doc);
            doc = redo.back();
            redo.pop_back();
            selected = {0};
            Changed();
        });
        Button(assign, "Assign to selected NPC", 1072, 76, 220, [this] {
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
        Add(name, 88, 76, 730, 30);
        name.OnInput([this](auto &e) {
            if (sync)
                return;
            Before();
            doc.name = e.sValue;
            Rebuild();
        });
        Button(addLine, "+ Add NPC line", 32, 510, 155, [this] {
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
        Button(addReply, "+ Add reply", 199, 510, 140, [this] {
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
        Button(remove, "Delete selected", 686, 510, 145, [this] {
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
        Add(textSize, 1010, 500, 260, 28);
        textSize.OnInput([this](auto &e) {
            if (sync)
                return;
            float parsed = 0;
            const auto result = std::from_chars(e.sValue.data(), e.sValue.data() + e.sValue.size(), parsed);
            if (result.ec != std::errc{} || result.ptr != e.sValue.data() + e.sValue.size() || !std::isfinite(parsed))
                return;
            Before();
            doc.theme.textSize = std::clamp(parsed, 14.f, 48.f);
            if (preview.Active())
                Start();
        });
        opacity.Create("Dialogue opacity");
        opacity.SetDescription("Opacity: ");
        Add(opacity, 1010, 534, 260, 28);
        opacity.OnInput([this](auto &e) {
            if (sync)
                return;
            float parsed = 0;
            const auto result = std::from_chars(e.sValue.data(), e.sValue.data() + e.sValue.size(), parsed);
            if (result.ec != std::errc{} || result.ptr != e.sValue.data() + e.sValue.size() || !std::isfinite(parsed))
                return;
            Before();
            doc.theme.panelOpacity = std::clamp(parsed, .2f, 1.f);
            if (preview.Active())
                Start();
        });
        matchAllBox.Create("Match all conditions");
        Add(matchAllBox, 890, 405, 22, 22);
        matchAllBox.OnClick([this](auto &e) {
            if (sync || !O())
                return;
            Before();
            O()->matchAll = e.bValue;
            Rebuild();
        });
        tree.Create("Conversation outline");
        Add(tree, 32, 164, 800, 330);
        tree.OnSelect([this](auto &e) {
            if (e.userdata < refs.size())
            {
                selected = refs[e.userdata];
                RefreshFields();
            }
        });
        selectionLabel.Create("Selection");
        Add(selectionLabel, 890, 126, 380, 28);
        speaker.Create("Speaker");
        speaker.SetDescription("Speaker: ");
        Add(speaker, 970, 168, 300, 28);
        speaker.OnInput([this](auto &e) {
            if (sync || !L())
                return;
            Before();
            L()->speaker = e.sValue;
            Rebuild();
        });
        text.Create("Dialogue text");
        text.SetDescription("");
        Add(text, 890, 238, 380, 82);
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
        next.SetText("");
        next.SetTooltip("Continue to this named section, or end the conversation.");
        Add(next, 890, 368, 380, 30);
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
        Add(onceBox, 890, 405, 22, 22);
        onceBox.OnClick([this](auto &e) {
            if (sync || !R())
                return;
            Before();
            R()->once = e.bValue;
            Rebuild();
        });
        kind.Create("Rule type");
        kind.SetText("");
        Add(kind, 890, 168, 380, 30);
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
        Add(key, 945, 238, 325, 30);
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
        Add(value, 965, 284, 305, 30);
        value.OnInput([this](auto &e) {
            if (sync)
                return;
            int parsed = 0;
            const auto result = std::from_chars(e.sValue.data(), e.sValue.data() + e.sValue.size(), parsed);
            if (result.ec != std::errc{} || result.ptr != e.sValue.data() + e.sValue.size())
            {
                status = "Enter a whole number for this rule.";
                return;
            }
            if (C())
            {
                Before();
                C()->value = parsed;
            }
            else if (A())
            {
                Before();
                A()->value = parsed;
            }
            Rebuild();
        });
        Button(addOutcome, "+ Outcome", 890, 438, 110, [this] {
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
        Button(addCondition, "+ Condition", 1010, 438, 130, [this] {
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
        Button(addAction, "+ Action", 1150, 438, 120, [this] {
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
        Button(play, "Restart", 1180, 575, 110, [this] { Start(); });
        permitBox.Create("Test: has_permit");
        Add(permitBox, 950, 580, 22, 22);
        permitBox.OnClick([this](auto &) { Start(); });
        for (auto *input :
             std::vector<wi::gui::TextInputField *>{&name, &speaker, &text, &key, &value, &textSize, &opacity})
            input->SetCancelInputEnabled(false);
        help.Create("Dialogue status");
        help.font.params.size = 13;
        Add(help, 24, 839, 1270, 20);
    }
    void FitToCanvas(wi::RenderPath2D &path)
    {
        const float width = float(path.GetLogicalWidth());
        const float height = float(path.GetLogicalHeight());
        const float dpi = path.GetDPIScaling();
        if (width < 64 || height < 100 ||
            (width == canvasWidth && height == canvasHeight && dpi == canvasDpi)) return;
        canvasWidth = width;
        canvasHeight = height;
        canvasDpi = dpi;
        uiScale = std::clamp(std::min((width - 32) / 1320.f, (height - 64) / 900.f), .4f, 1.f);
        owner->SetSize({1320 * uiScale, 900 * uiScale});
        owner->SetPos({std::max(8.f, (width - 1320 * uiScale) * .5f), 24});
        for (auto &item : layout)
        {
            item.widget->SetPos({item.pos.x * uiScale, item.pos.y * uiScale});
            item.widget->SetSize({item.size.x * uiScale, item.size.y * uiScale});
        }
        tree.uiScale = uiScale;
    }
    void ApplyTheme()
    {
        wi::gui::Theme theme;
        theme.image.background = false;
        theme.image.color = Panel;
        theme.font.color = Text;
        theme.font.shadow_color = wi::Color::Transparent();
        theme.shadow = 0;
        theme.tooltipImage.background = false;
        theme.tooltipImage.color = theme::Focus;
        theme.tooltipFont.color = Text;
        owner->SetTheme(theme);
        owner->SetColor(theme::Idle, wi::gui::IDLE);
        owner->SetColor(theme::Focus, wi::gui::FOCUS);
        owner->SetColor(theme::Active, wi::gui::ACTIVE);
        // The workspace paints its opaque base and section fills before native
        // controls. Decoration widgets would compete in Window's focus ordering.
        owner->SetColor(wi::Color::Transparent(), wi::gui::WIDGET_ID_WINDOW_BASE);
        owner->SetColor(Panel, wi::gui::WIDGET_ID_COMBO_DROPDOWN);
        owner->font.params.size = 16;
        owner->font.params.bolden = .15f;
        for (auto *label : {&help, &selectionLabel, &previewHeading, &previewHint, &appearanceHeading, &conversationHeading, &textHeading, &destinationHeading, &ruleSummary, &voiceHeading})
        {
            label->SetColor(wi::Color::Transparent());
            label->font.params.size = label == &help || label == &previewHint ? 13 : 16;
            label->font.params.bolden = label == &help || label == &previewHint ? 0 : .15f;
        }
        previewHint.font.params.color = theme::Muted;
        for (auto *input : std::vector<wi::gui::TextInputField *>{&name, &speaker, &text, &key, &value, &textSize, &opacity})
        {
            input->font.params.size = input->font_description.params.size = 14;
            input->SetColor(theme::Idle, wi::gui::IDLE);
        }
        for (auto *combo : {&library, &next, &kind, &voice})
        {
            combo->font.params.size = combo->selected_font.params.size = 14;
        }
        tree.font.params.size = theme::ScriptSize;
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
    SetSize({1320, 900});
    SetControlSize(28);
    impl_->Create();
    impl_->ApplyTheme();
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
    if (IsVisible() || !s.Projects().HasProject())
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
    i.canvasWidth = i.canvasHeight = i.canvasDpi = 0;
    i.FitToCanvas(path);
    SetVisible(true);
    i.ApplyTheme();
    i.RefreshFields();
    i.Start();
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
    i.FitToCanvas(path);
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
        auto viewport = ScreenRect{GetPos().x + 24 * i.uiScale, GetPos().y + control_size + 640 * i.uiScale,
                                   1270 * i.uiScale, 190 * i.uiScale};
        if (i.preview.Active())
        {
            if (i.panel.Show(i.preview.View(), i.project, i.root, path, viewport, i.status))
                i.panel.EmbedInWindow(path, *this);
        }
        else
            i.panel.Hide(path);
    }
    else if (i.preview.Active())
        i.panel.Layout(path, {GetPos().x + 24 * i.uiScale, GetPos().y + control_size + 640 * i.uiScale,
                              1270 * i.uiScale, 190 * i.uiScale});
    i.previewHint.SetText(i.preview.Active() ? "Try the conversation as your player will see it."
                                            : "Conversation ended. Restart to try it again.");
    i.help.SetText((i.Dirty() ? "UNSAVED  |  " : "") + i.status);
    i.undoButton.SetEnabled(!i.undo.empty());
    i.redoButton.SetEnabled(!i.redo.empty());
}
void DialogueEditorWindow::Render(const wi::Canvas &canvas, wi::graphics::CommandList cmd) const
{
    if (!IsVisible())
        return;
    ApplyScissor(canvas, scissorRect, cmd);
    Rect(translation.x, translation.y, scale.x, scale.y, Panel, cmd);
    Rect(translation.x, translation.y, scale.x, control_size, theme::Idle, cmd);
    const float s = impl_->uiScale;
    auto section = [&](float x, float y, float w, float h) {
        Rect(translation.x + x * s, translation.y + control_size + y * s, w * s, h * s, Inset, cmd);
    };
    section(18, 116, 830, 438);
    section(874, 116, 420, 438);
    section(18, 566, 1276, 266);
    wi::gui::Window::Render(canvas, cmd);
}
} // namespace renegade::studio