#include "DialogueEditorWindow.h"
#include "StudioTheme.h"
#include "StudioButtonVisual.h"
#include "renegade/bridge/DialogueAssetService.h"
#include "renegade/bridge/DialogueDisplayService.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/screen/DialoguePanel.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <set>
#include <filesystem>
#ifdef _WIN32
#include <shellapi.h>
#endif
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
class DialogueButton : public ReleaseButton
{
  public:
    bool primary = false, accented = false;
    void Render(const wi::Canvas &canvas, wi::graphics::CommandList cmd) const override
    {
        if (!IsVisible()) return;
        ApplyScissor(canvas,scissorRect,cmd);
        const bool enabled=IsEnabled();
        theme::DrawButton(translation.x,translation.y,scale.x,scale.y,GetText(),
            {enabled,enabled&&GetState()==wi::gui::FOCUS,
             enabled&&(GetState()==wi::gui::ACTIVE||GetState()==wi::gui::DEACTIVATING),primary,accented},
            theme::ControlSize,cmd);
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
    std::vector<std::string> hints;
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
            else if (reply)
            {
                draw(item.name.substr(7), x, y + 8 * uiScale, theme::ScriptSize, Text, .12f);
                if (size_t(index) < hints.size())
                    draw(hints[index], x, y + 32 * uiScale, theme::CaptionSize, theme::Muted);
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
    std::string project, root, saved, status, previewResult;
    std::shared_ptr<int> lifetime = std::make_shared<int>(0);
    std::uint64_t generation = 0, targetRevision = ~std::uint64_t(0), editRevision = 0, previewRevision = 0;
    std::string previewError;
    std::vector<SceneEntity> targets;
    bool testGameRequested = false;
    bool advanced = false, sync = false, refreshPreview = false, selectionPlaced = false;
    bool expandAdvancedRules = false;
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
    wi::gui::ComboBox library, next, kind, voice, target;
    wi::gui::ComboBox display;
    DialogueButton editDisplay;
    std::vector<DialogueDisplayAssignment> displays;
    DialogueDisplayAssignment selectedDisplay, displayRequest;
    bool resumeAfterDisplay = false;
    std::vector<std::string> voicePaths;
    wi::gui::TextInputField name, speaker, key, value, textSize, opacity;
    DialogueText text;
    DialogueOutline tree;
    wi::gui::Label help, selectionLabel, previewHeading, previewHint, appearanceHeading, conversationHeading, textHeading, destinationHeading, ruleSummary, voiceHeading;
    wi::gui::Label workflow, voiceHint, gameHeading, gameHint, exampleNote;
    DialogueButton showAdvanced, audition, importVoice, voiceFolder, refreshVoices, testGame;
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
        b.OnClick([fn](auto &) {
            // Pinned native Button revisits DEACTIVATING while held. Act on
            // release once, matching the shared Screen renderer's gesture rule.
            if (!wi::input::Down(wi::input::MOUSE_BUTTON_LEFT)) fn();
        });
    }
    bool Dirty() const
    {
        return EncodeDialogue(doc) != saved;
    }
    void Before()
    {
        ++editRevision;
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
        ++editRevision;
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
        std::string hint;
        if (ref.l >= 0 && ref.r >= 0 && ref.o < 0 && ref.c < 0 && ref.a < 0)
        {
            const auto &reply = doc.lines[ref.l].replies[ref.r];
            if (reply.outcomes.size() != 1) hint = "Conditional responses - Show in Advanced";
            else if (reply.outcomes.front().next.empty()) hint = "Ends conversation - add a response to continue";
            else
                for (const auto &line : doc.lines)
                    if (line.id == reply.outcomes.front().next) { hint = "Then: " + line.text; break; }
        }
        tree.hints.push_back(std::move(hint));
    }
    void Rebuild()
    {
        if (expandAdvancedRules)
        {
            expanded.clear();
            expandAdvancedRules = false;
        }
        else
            for (size_t n = 0; n < rowKeys.size() && n < size_t(tree.GetItemCount()); ++n)
                expanded[rowKeys[n]] = tree.GetItem(int(n)).open;
        tree.ClearItems();
        tree.hints.clear();
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
                Row("Unused NPC line - connect from a reply's Continue to", 0, {int(i)});
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
        workflow.SetText(!advanced ? "Select a line to edit. Add replies, then select a reply and Add response."
            : !r ? "Advanced is on. Select or add a player reply to edit conditions and alternative outcomes."
            : o ? "Advanced: edit this outcome's conditions and destination. Add actions with + Action."
                : "Advanced: add conditions or outcomes to this reply. Select an outcome below it to add actions.");
        name.SetValue(doc.name);
        textSize.SetValue(doc.theme.textSize);
        opacity.SetValue(doc.theme.panelOpacity);
        textSize.SetEnabled(selectedDisplay.screenId.empty());
        opacity.SetEnabled(selectedDisplay.screenId.empty());
        textSize.SetTooltip(selectedDisplay.screenId.empty()?"Set the built-in dialogue text size.":"This custom display owns its fonts and sizes. Use Edit display, then STYLE / FONT.");
        opacity.SetTooltip(selectedDisplay.screenId.empty()?"Set the built-in dialogue panel opacity.":"This custom display owns its backgrounds and opacity. Use Edit display.");
        opacity.SetVisible(advanced && r && !c && !a);
        textSize.SetVisible(advanced && r && !c && !a);
        appearanceHeading.SetVisible(advanced && r && !c && !a);
        bool usesPermit = false;
        for (const auto &line : doc.lines)
            for (const auto &reply : line.replies)
            {
                for (const auto &condition : reply.conditions) usesPermit |= condition.key == "has_permit";
                for (const auto &outcome : reply.outcomes)
                    for (const auto &condition : outcome.conditions) usesPermit |= condition.key == "has_permit";
            }
        permitBox.SetVisible(advanced && usesPermit);
        speaker.SetValue(l ? l->speaker : "");
        int voiceIndex = 0;
        if (l) for (size_t n = 0; n < voicePaths.size(); ++n)
            if (voicePaths[n] == l->voice) voiceIndex = int(n + 1);
        voice.SetSelectedWithoutCallback(voiceIndex);
        voice.SetVisible(l && !r);
        voiceHeading.SetVisible(l && !r);
        importVoice.SetVisible(l && !r);
        voiceFolder.SetVisible(l && !r);
        refreshVoices.SetVisible(l && !r);
        addLine.SetText(r ? "+ Add response" : "+ Add NPC line");
        audition.SetVisible(l && !r);
        audition.SetEnabled(l && !l->voice.empty());
        text.SetValue(r && !o ? r->text : l ? l->text : "");
        textHeading.SetVisible(l && !o && !c && !a);
        destinationHeading.SetVisible((o || r && r->outcomes.size() == 1 || l && !r && l->replies.empty()) && !c && !a);
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
        next.SetVisible((o || r && r->outcomes.size() == 1 || l && !r && l->replies.empty()) && !c && !a);
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
        previewResult.clear();
        previewError.clear();
        previewRevision = editRevision;
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
    void RefreshTargets()
    {
        const auto previous = target.GetSelected();
        auto keep = previous > 0 && size_t(previous) <= targets.size() ? targets[previous - 1].entity
            : session->Selection().SelectedEntity();
        targets.clear();
        target.ClearItems();
        target.AddItem("Choose an NPC or scene object...");
        for (const auto &item : session->Scenes().ListEntities())
            if (item.hasTransform && item.logicalAsset && !IsPlayerStart(session->Scenes().GetScene(), item.entity) &&
                (item.category == SceneEntityCategory::Characters || item.category == SceneEntityCategory::Models ||
                 item.category == SceneEntityCategory::Other))
            {
                targets.push_back(item);
                target.AddItem(item.name + "  (#" + std::to_string(item.entity) + ")");
            }
        int selectedIndex = 0;
        for (size_t n = 0; n < targets.size(); ++n)
            if (targets[n].entity == keep) selectedIndex = int(n + 1);
        if (!selectedIndex)
            for (size_t n = 0; n < targets.size(); ++n)
                if (AssignedDialogue(session->Scenes().GetScene(), targets[n].entity) == doc.id)
                { selectedIndex = int(n + 1); break; }
        target.SetSelectedWithoutCallback(selectedIndex);
        targetRevision = session->Scenes().Revision();
    }
    bool Attach()
    {
        const int index = target.GetSelected();
        if (index <= 0 || size_t(index) > targets.size())
        { status = "Choose the NPC or object that should speak this conversation."; return false; }
        if (!Save()) return false;
        const auto entity = targets[index - 1].entity;
        if (AssignedDialogue(session->Scenes().GetScene(), entity) != doc.id &&
            !session->Commands().Execute(std::make_unique<AssignDialogueCommand>(session->Scenes().GetScene(), entity, doc.id)))
        { status = "This object is no longer available. Choose another NPC."; return false; }
        if (session->Scenes().CurrentPath().empty())
        { status = "Attached in memory. Save your level once in the main editor to keep the NPC binding."; return false; }
        if (!session->SaveScene(session->Scenes().CurrentPath()))
        { status = "NPC attached, but saving the level failed: " + session->Scenes().LastError(); return false; }
        status = "Conversation and level saved. Walk within 3 m of the NPC and press Interact (E by default).";
        return true;
    }
    void ImportVoice()
    {
        if (!L() || R()) return;
        const auto lineId = L()->id, documentId = doc.id;
        const auto token = generation;
        std::weak_ptr<int> alive = lifetime;
        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "Dialogue voice clip (WAV or OGG)";
        params.extensions = {"wav", "ogg"};
        params.multiselect = false;
        wi::helper::FileDialog(params, [this, alive, token, lineId, documentId](const std::string &file) {
            if (file.empty() || alive.expired()) return;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this, alive, token, lineId, documentId, file](uint64_t) {
                    if (alive.expired() || token != generation || !owner->IsVisible() || doc.id != documentId) return;
                    auto line = std::find_if(doc.lines.begin(), doc.lines.end(), [&](auto &l) { return l.id == lineId; });
                    if (line == doc.lines.end()) return;
                    std::string clip;
                    if (!ImportDialogueVoice(root, file, clip, status)) return;
                    Before();
                    line->voice = clip;
                    selected = {int(line - doc.lines.begin())};
                    ReloadLibrary();
                    Changed();
                    status = "Voice clip copied into this project and attached to the selected NPC line. Save to keep it.";
                });
        });
    }
    bool Save()
    {
        if (!SaveDialogueAsset(root, doc, status))
            return false;
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
            return false;
        }
        saved = EncodeDialogue(doc);
        status = "Conversation saved. Choose an NPC below, then Save & attach.";
        ReloadLibrary();
        RefreshFields();
        return true;
    }
    bool UseDisplay(const DialogueDisplayAssignment& assignment)
    {
        if (!SetDialogueDisplayAssignment(root,project,assignment,status)) return false;
        auto include=session->Projects().CurrentProject().alwaysInclude;
        if(!selectedDisplay.path.empty() && selectedDisplay.path!=assignment.path)
            include.erase(std::remove(include.begin(),include.end(),"runtime_screen_document:"+selectedDisplay.path),include.end());
        auto add=[&](std::string value){if(std::find(include.begin(),include.end(),value)==include.end())include.push_back(std::move(value));};
        add(std::string("data:")+DialogueDisplaySettingsPath);
        if(!assignment.path.empty())add("runtime_screen_document:"+assignment.path);
        if(!session->Projects().SetAlwaysInclude(include)){status=session->Projects().LastError();return false;}
        selectedDisplay=assignment;
        if(!panel.ReloadDisplay(root,project,status))return false;
        refreshPreview=true;status="Project dialogue display saved. Preview and the next Test Game use this display.";
        return true;
    }
    void ReloadDisplays()
    {
        const bool previous=sync;sync=true;displays.clear();display.ClearItems();
        display.AddItem("Built-in display");display.AddItem("Create a new display...");
        std::string error;
        if(!ReadDialogueDisplayAssignment(root,project,selectedDisplay,error))status=error;
        namespace fs=std::filesystem;std::error_code ec;
        auto folder=fs::u8path(root)/"Content/UI";
        if(fs::exists(folder,ec))for(fs::recursive_directory_iterator it(folder,ec),end;it!=end&&!ec;it.increment(ec)){
            if(!it->is_regular_file(ec)||it->path().extension()!=".renegade-screen")continue;
            ScreenDocument screen;std::string ignored;
            if(!ReadScreenDocument(it->path().generic_u8string(),project,screen,ignored)||
                std::none_of(screen.widgets.begin(),screen.widgets.end(),[](const auto& w){return !w.dialogueRole.empty();}))continue;
            displays.push_back({screen.envelope.documentId,it->path().lexically_relative(fs::u8path(root)).generic_u8string()});
            display.AddItem(it->path().stem().u8string());
        }
        int selection=0;
        for(size_t n=0;n<displays.size();++n)if(displays[n].screenId==selectedDisplay.screenId)selection=int(n)+2;
        display.SetSelected(selection);
        editDisplay.SetText(selectedDisplay.screenId.empty()?"Customise display":"Edit display");
        if(!panel.ReloadDisplay(root,project,error))status="Display needs repair: "+error;
        sync=previous;
    }
    void EditDisplay(bool create=false)
    {
        if(create||selectedDisplay.screenId.empty()){
            const auto relative="Content/UI/Dialogue/Dialogue-"+GenerateStableId()+".renegade-screen";
            auto screen=MakeDialogueDisplayTemplate(project,relative);
            if(!WriteScreenDocument((std::filesystem::u8path(root)/relative).generic_u8string(),screen,status)||
                !UseDisplay({screen.envelope.documentId,relative}))return;
            ReloadDisplays();
        }
        displayRequest=selectedDisplay;resumeAfterDisplay=true;
        panel.Hide(*path);owner->SetVisible(false);
    }
    void ReloadLibrary()
    {
        assets = ListDialogueAssets(root, project, status);
        voicePaths.clear();
        voice.ClearItems();
        voice.AddItem("No voice (text only)");
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
        for (size_t n = 0; n < assets.size(); ++n)
            if (assets[n].id == doc.id) library.SetSelectedWithoutCallback(int(n));
    }
    void SelectDocument(DialogueDocument d)
    {
        doc = std::move(d);
        selected = {0};
        saved = EncodeDialogue(doc);
        undo.clear();
        redo.clear();
        Changed();
        Start();
    }
    void Create()
    {
        auto heading = [this](wi::gui::Label &label, const char *title, float x, float y, float w) {
            label.Create(title);
            label.SetText(title);
            Add(label, x, y, w, 24);
        };
        heading(previewHeading, "2. Try the replies", 32, 575, 340);
        heading(previewHint, "Try the conversation as your player will see it.", 32, 606, 650);
        heading(workflow, "Select a line to edit. Add replies, then select a reply and Add response.", 32, 118, 800);
        exampleNote.Create("Example voice recordings");
        exampleNote.SetText("Included voice clips are examples. Add your own WAV/OGG recordings for spoken dialogue; lines without a clip display as text only.");
        Add(exampleNote, 740, 600, 550, 34);
        heading(conversationHeading, "1. Write the conversation", 32, 146, 740);
        heading(textHeading, "Dialogue text", 890, 207, 360);
        heading(destinationHeading, "Continue to", 890, 334, 360);
        heading(ruleSummary, "", 890, 441, 360);
        heading(voiceHeading, "Voice (optional WAV / OGG)", 890, 411, 380);
        voice.Create("Voice clip");
        voice.SetText("");
        Add(voice, 890, 438, 380, 30);
        voice.OnSelect([this](auto &e) {
            if (sync || !L() || R()) return;
            Before();
            L()->voice = e.iValue > 0 && size_t(e.iValue) <= voicePaths.size() ? voicePaths[e.iValue - 1] : "";
            RefreshFields();
        });
        Button(importVoice, "Add voice clip...", 890, 478, 180, [this] { ImportVoice(); });
        Button(audition, "Play this line", 1080, 478, 190, [this] {
            if (!L()) return;
            auto selectedDocument = doc;
            selectedDocument.entry = L()->id;
            panel.StopVoice();
            previewResult.clear();
            if (preview.Start(selectedDocument, memory, status))
            { previewRevision = editRevision; previewError.clear(); refreshPreview = true; }
        });
        Button(voiceFolder, "Open voice folder", 890, 518, 180, [this] {
            namespace fs = std::filesystem;
            std::error_code ec;
            const auto base = fs::weakly_canonical(fs::u8path(root), ec);
            const auto folder = fs::weakly_canonical(base / "Content/Audio/Dialogue", ec);
            const auto relative = folder.lexically_relative(base);
            if (ec || relative.empty() || *relative.begin() == "..") { status = "The voice folder is outside this project."; return; }
            fs::create_directories(folder, ec);
            if (ec) { status = "Could not create the project voice folder: " + ec.message(); return; }
#ifdef _WIN32
            ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
            status = "Add WAV/OGG recordings to Content/Audio/Dialogue, then Refresh clips. New lines can remain text-only.";
        });
        Button(refreshVoices, "Refresh clips", 1080, 518, 190, [this] { ReloadLibrary(); RefreshFields(); status = "Project WAV/OGG list refreshed."; });
        Button(showAdvanced, "Show in Advanced", 1075, 478, 195, [this] { advanced = true; expandAdvancedRules = true; Changed(); });
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
            d.lines.clear();
            d.entry = "opening";
            DialogueLine opening;
            opening.id = d.entry;
            opening.text = "Your opening line";
            d.lines.push_back(std::move(opening));
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
            if (advanced)
            {
                expandAdvancedRules = true;
                if (L() && !R() && !L()->replies.empty())
                {
                    selected.r = 0;
                    selected.o = selected.c = selected.a = -1;
                    status = "Advanced enabled: showing the first player reply's rules. Select an outcome to edit its actions.";
                }
                else status = "Advanced enabled. Select a player reply or its outcome to edit rules.";
            }
            else selected.o = selected.c = selected.a = -1;
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
        heading(gameHeading, "3. Attach to an NPC", 32, 853, 230);
        target.Create("NPC or scene object");
        target.SetText("");
        Add(target, 260, 848, 360, 32);
        Button(assign, "Save & attach", 635, 848, 170, [this] { Attach(); });
        Button(testGame, "Test in game", 1080, 848, 210, [this] {
            if (!Attach()) return;
            testGameRequested = true;
            panel.Hide(*path);
            owner->SetVisible(false);
        });
        testGame.primary = true;
        heading(gameHint, "Choose the object that should speak this conversation.", 32, 890, 1260);
        heading(voiceHint, "In game: approach the NPC (within 3 m), press Interact (E by default). Use arrows + Enter; Esc closes.", 32, 910, 1260);
        name.Create("Conversation name");
        name.SetDescription("Name: ");
        Add(name, 88, 76, 730, 30);
        display.Create("In-game dialogue display");display.SetText("");
        display.SetTooltip("Project-wide game dialogue layout. Create or edit a display to change its fonts, artwork, colours and position in the UI editor.");
        Add(display,846,76,270,30);
        display.OnSelect([this](auto& e){
            if(sync)return;
            if(e.iValue==1){EditDisplay(true);return;}
            const auto chosen=e.iValue>=2&&size_t(e.iValue-2)<displays.size()?displays[size_t(e.iValue-2)]:DialogueDisplayAssignment{};
            UseDisplay(chosen);ReloadDisplays();
        });
        Button(editDisplay,"Customise display",1125,76,165,[this]{EditDisplay();});
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
                status = advanced ? "Select a reply outcome, then Add response."
                                  : "Select a player reply, then Add response to write the NPC's answer.";
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
        Add(tree, 32, 184, 800, 310);
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
            {
                L()->text = e.sValue;
                if (!L()->voice.empty()) status = "Text updated. The attached recording is unchanged; add a replacement clip if needed.";
            }
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
        Add(help, 24, 938, 1270, 24);
        const std::pair<wi::gui::Widget *, const char *> tips[] = {
            {&library, "Choose a saved conversation, then Open. Save or Discard any current edits first."},
            {&newDoc, "Create a text-only conversation with one opening NPC line. Add replies and responses as needed."},
            {&load, "Open the conversation chosen in the list. Its preview starts at the opening line."},
            {&discard, "Reload the last saved conversation and discard unsaved text, connections and voice choices."},
            {&save, "Save conversation text, branching and voice references in this project. Use Save & attach below for an NPC."},
            {&undoButton, "Undo the last conversation edit."}, {&redoButton, "Redo an undone conversation edit."},
            {&advancedBox, "Show conditions, outcomes, actions, one-time replies and presentation controls in this workspace."},
            {&name, "The name shown in this project's conversation library."},
            {&tree, "Select a spoken line or player reply to edit it. Click its arrow to expand the connected responses."},
            {&addLine, "Select a player reply, then Add response to create and connect the NPC's answer. A line without replies can continue to another NPC line."},
            {&addReply, "Add a player choice beneath the selected NPC line. Select that reply and Add response to write its answer."},
            {&remove, "Delete the selected reply, condition, action or NPC line. The opening NPC line cannot be removed. Undo restores deletions."},
            {&speaker, "Name displayed above this NPC's spoken line in game."},
            {&text, "Edit the selected NPC speech or player reply. Voice recordings are optional and attached separately."},
            {&next, "Choose the NPC response this reply leads to, or End conversation. Add response creates a new connected line."},
            {&voice, "Attach a WAV or OGG from this project's audio folder to this NPC line. No voice means text-only; changing text does not generate audio."},
            {&importVoice, "Browse your computer for a recording. Copies the WAV/OGG into this project and attaches it to this line."},
            {&voiceFolder, "Open this project's Content/Audio/Dialogue folder. Add your own recordings there, then Refresh clips."},
            {&refreshVoices, "Rescan this project's audio folders after adding recordings outside the editor."},
            {&audition, "Play the selected NPC line and its attached recording in the preview below."},
            {&play, "Reset preview progress and private test flags. Valid text and branch edits already update the preview automatically."},
            {&permitBox, "Start the preview with the sample has_permit flag enabled. This changes preview state only."},
            {&target, "Choose the NPC or scene object players will approach to start this conversation. A selected scene object is chosen automatically."},
            {&assign, "Save the conversation, attach it to the chosen NPC and save the current level. Replaces that NPC's previous conversation."},
            {&testGame, "Save and attach to the chosen NPC, save the level, then launch the normal Test Game. Walk to the NPC and press Interact (E by default)."},
            {&showAdvanced, "Show this reply's conditional outcomes without removing any existing rules."},
            {&onceBox, "Hide this reply after it has been chosen. Its used state is included in game saves."},
            {&matchAllBox, "Require all conditions for this outcome; turn off to accept any matching condition."},
            {&kind, "Choose whether this rule reads a flag, value or inventory item, or writes a flag/value or gameplay event."},
            {&key, "The flag, value, item or registered gameplay event name used by this rule."},
            {&value, "Whole-number value compared or written by this rule."},
            {&addOutcome, "Add another conditional result for this reply. Outcomes are checked in order."},
            {&addCondition, "Add a condition to the selected outcome, or control whether the selected reply is shown."},
            {&addAction, "Add a flag/value update or gameplay event to the selected outcome."},
            {&textSize, "Set the in-game dialogue text size. Restart the preview to see the result."},
            {&opacity, "Set the in-game panel opacity. Editor controls remain opaque."}
        };
        for (const auto &tip : tips) tip.first->SetTooltip(tip.second);
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
        uiScale = std::clamp(std::min((width - 32) / 1320.f, (height - 64) / 990.f), .4f, 1.f);
        owner->SetSize({1320 * uiScale, 990 * uiScale});
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
        for (auto *label : {&help, &selectionLabel, &previewHeading, &previewHint, &appearanceHeading, &conversationHeading, &textHeading, &destinationHeading, &ruleSummary, &voiceHeading, &workflow, &voiceHint, &gameHeading, &gameHint, &exampleNote})
        {
            label->SetColor(wi::Color::Transparent());
            const bool caption = label == &help || label == &previewHint || label == &workflow || label == &voiceHint || label == &gameHint || label == &exampleNote;
            label->font.params.size = caption ? 13 : 16;
            label->font.params.bolden = caption ? 0 : .15f;
        }
        previewHint.font.params.color = theme::Muted;
        for (auto *input : std::vector<wi::gui::TextInputField *>{&name, &speaker, &text, &key, &value, &textSize, &opacity})
        {
            input->font.params.size = input->font_description.params.size = 14;
            input->SetColor(theme::Idle, wi::gui::IDLE);
        }
        for (auto *combo : {&library, &next, &kind, &voice, &target, &display})
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
    impl_->lifetime.reset();
    if (impl_ && impl_->path)
        impl_->panel.Hide(*impl_->path);
}
void DialogueEditorWindow::Create()
{
    wi::gui::Window::Create("Dialogue Editor", WindowControls::CLOSE | WindowControls::MOVE);
    // Keep native child coordinates on its unit-scale content transform. The
    // workspace fits the canvas, so only its conversation list should scroll.
    RemoveWidget(&scrollbar_vertical);
    RemoveWidget(&scrollbar_horizontal);
    scrollbar_vertical.Detach();
    scrollbar_horizontal.Detach();
    scrollbar_vertical.SetListLength(0);
    scrollbar_horizontal.SetListLength(0);
    scrollbar_vertical.SetVisible(false);
    scrollbar_horizontal.SetVisible(false);
    SetSize({1320, 990});
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
    ++i.generation;
    i.session = &s;
    i.path = &path;
    i.project = s.Projects().CurrentProject().projectId;
    i.root = s.Projects().CurrentProject().rootPath;
    i.panel.SetEmbeddedPreview();
    if(i.resumeAfterDisplay){
        i.resumeAfterDisplay=false;
        i.ReloadDisplays();
        i.ReloadLibrary();
        i.FitToCanvas(path);
        // Native Window::SetVisible shows all children. Reapply the current
        // basic/Advanced and selected-row visibility only after showing it.
        SetVisible(true);
        i.RefreshFields();
        i.RefreshTargets();
        i.refreshPreview=true;
        return;
    }
    i.ReloadDisplays();
    i.targets.clear();
    i.target.ClearItems();
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
    i.RefreshTargets();
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
    if (i.previewRevision != i.editRevision)
    {
        i.previewRevision = i.editRevision;
        if (i.preview.RefreshDocument(i.doc, i.previewError))
        {
            i.previewResult.clear();
            i.refreshPreview = true;
        }
    }
    if (i.targetRevision != s.Scenes().Revision()) i.RefreshTargets();
    const int targetIndex = i.target.GetSelected();
    const bool hasTarget = targetIndex > 0 && size_t(targetIndex) <= i.targets.size();
    i.assign.SetEnabled(hasTarget);
    i.testGame.SetEnabled(hasTarget && !s.Scenes().CurrentPath().empty());
    if (hasTarget)
    {
        const auto &npc = i.targets[targetIndex - 1];
        const bool attached = AssignedDialogue(s.Scenes().GetScene(), npc.entity) == i.doc.id;
        i.gameHint.SetText((attached ? "Attached to " : "Ready to attach to ") + npc.name +
            (s.Scenes().CurrentPath().empty() ? ". Save your level once before testing." : ". Save & attach keeps the conversation and level together."));
    }
    else i.gameHint.SetText("Choose the NPC or scene object that should speak this conversation. No assignment yet.");
    auto action = i.panel.TakeAction();
    if (!action.empty())
    {
        wi::backlog::post("Dialogue preview action: " + action);
        if (action == "@restart") i.Start();
        else if (action == "@close")
        {
            i.preview.Close();
            i.previewResult = "Preview stopped. Restart to try the conversation again.";
        }
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
        auto view = i.preview.View();
        if (!i.preview.Active())
        {
            view.active = true;
            view.speaker = "Conversation ended";
            view.text = i.previewResult.empty() ? "This branch ends here. Restart to try another reply, or add a response to continue it." : i.previewResult;
            view.voiceKey = i.doc.id + ":preview-ended";
            view.replies = {{"@restart", "Restart conversation"}};
        }
        if (i.panel.Show(view, i.project, i.root, path, viewport, i.status))
            i.panel.EmbedInWindow(path, *this);
    }
    else
        i.panel.Layout(path, {GetPos().x + 24 * i.uiScale, GetPos().y + control_size + 640 * i.uiScale,
                              1270 * i.uiScale, 190 * i.uiScale});
    i.previewHint.SetText(!i.previewError.empty() ? "Complete the edit to update the preview: " + i.previewError
                        : i.preview.Active() ? "Live preview: edits update automatically. Restart resets your test progress."
                                            : "This branch has ended. The preview stays here so you can restart or keep editing.");
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
    section(18, 840, 1276, 100);
    wi::gui::Window::Render(canvas, cmd);
}
bool DialogueEditorWindow::TakeTestGameRequest()
{
    const bool requested = impl_->testGameRequested;
    impl_->testGameRequested = false;
    return requested;
}
bool DialogueEditorWindow::TakeDisplayEditorRequest(std::string& id,std::string& path)
{
    if(impl_->displayRequest.screenId.empty())return false;
    id=impl_->displayRequest.screenId;path=impl_->displayRequest.path;impl_->displayRequest={};return true;
}
} // namespace renegade::studio
