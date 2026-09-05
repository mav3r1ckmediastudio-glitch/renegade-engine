#include "renegade/bridge/ScreenAuthoringSession.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    struct TemporaryDirectory
    {
        fs::path path;
        ~TemporaryDirectory()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    };

    int Fail(const std::string& message)
    {
        std::cerr << "RenegadeScreenAuthoringSessionTests: " << message << '\n';
        return 1;
    }

    ScreenDocument MakeDocument(const StableId& projectId)
    {
        ScreenDocument document;
        document.envelope = CreateDocumentEnvelope(
            projectId,
            RuntimeScreenDocumentType,
            "Content/Screens/Main.renegade-screen",
            "Gate 8D authoring fixture");
        document.designWidth = 1280.0f;
        document.designHeight = 720.0f;
        document.actions = {{RuntimeScreenPlayAction}, {RuntimeScreenQuitAction}};

        ScreenWidget title;
        title.id = GenerateStableId();
        title.kind = ScreenWidgetKind::Text;
        title.name = "Title";
        title.rect = {240.0f, 100.0f, 800.0f, 100.0f};
        title.text = "RENEGADE";
        title.style = MakeScreenWidgetStyleTemplate(title.kind, title.rect.height);

        ScreenWidget play;
        play.id = GenerateStableId();
        play.kind = ScreenWidgetKind::Button;
        play.name = "Play";
        play.rect = {440.0f, 330.0f, 400.0f, 76.0f};
        play.text = "PLAY";
        play.actionId = RuntimeScreenPlayAction;
        play.style = MakeScreenWidgetStyleTemplate(play.kind, play.rect.height);

        ScreenWidget quit;
        quit.id = GenerateStableId();
        quit.kind = ScreenWidgetKind::Button;
        quit.name = "Quit";
        quit.rect = {440.0f, 440.0f, 400.0f, 76.0f};
        quit.text = "QUIT";
        quit.actionId = RuntimeScreenQuitAction;
        quit.layoutMode = ScreenLayoutMode::Anchored;
        quit.anchors = {
            0.5f, 0.0f, 0.5f, 0.0f,
            -200.0f, 440.0f, 200.0f, 516.0f,
        };
        quit.style = MakeScreenWidgetStyleTemplate(quit.kind, quit.rect.height);

        document.widgets = {title, play, quit};
        document.focusOrder = {play.id, quit.id};
        return document;
    }

    ScreenWidgetCreatorEdit CreatorEditFor(const ScreenWidget& widget)
    {
        ScreenWidgetCreatorEdit edit;
        edit.resourcePath = widget.resourcePath;
        edit.actionId = widget.actionId;
        edit.parentId = widget.parentId;
        edit.layoutMode = widget.layoutMode;
        edit.anchors = widget.anchors;
        edit.style = widget.style;
        return edit;
    }

    bool SameRect(const ScreenRect& left, const ScreenRect& right)
    {
        return left.x == right.x && left.y == right.y &&
            left.width == right.width && left.height == right.height;
    }
}

int main()
{
    using namespace renegade::bridge;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade screen authoring " + std::to_string(unique))};
    const fs::path screenPath =
        temporary.path / "Content/Screens/Main.renegade-screen";
    fs::create_directories(screenPath.parent_path());

    const StableId projectId = GenerateStableId();
    ScreenDocument source = MakeDocument(projectId);
    const StableId titleId = source.widgets[0].id;
    const StableId playId = source.widgets[1].id;
    const StableId quitId = source.widgets[2].id;
    const ScreenAnchors originalQuitAnchors = source.widgets[2].anchors;
    std::string error;
    if (!WriteScreenDocument(screenPath.generic_u8string(), source, error))
        return Fail("fixture write failed: " + error);

    // Gate 8C baseline: basic Inspector edits, anchored geometry, history and
    // transactional Save/Open remain intact under Gate 8D.
    ScreenAuthoringSession session;
    if (!session.Open(screenPath.generic_u8string(), projectId, error) ||
        session.IsDirty() || session.CanUndo() || session.CanRedo())
    {
        return Fail("Open did not establish a clean authoring session: " + error);
    }

    ScreenWidgetAuthoringEdit titleEdit;
    titleEdit.name = "Main Title";
    titleEdit.text = "THE PAGANACHT";
    titleEdit.resolvedRect = {220.0f, 90.0f, 840.0f, 110.0f};
    titleEdit.visible = true;
    titleEdit.enabled = true;
    if (!session.UpdateWidget(titleId, titleEdit, error) ||
        !session.IsDirty() || !session.CanUndo())
    {
        return Fail("absolute widget edit did not enter history: " + error);
    }

    ScreenWidgetAuthoringEdit quitEdit;
    quitEdit.name = "Exit Game";
    quitEdit.text = "EXIT";
    quitEdit.resolvedRect = {420.0f, 450.0f, 440.0f, 80.0f};
    quitEdit.visible = true;
    quitEdit.enabled = true;
    if (!session.UpdateWidget(quitId, quitEdit, error))
        return Fail("anchored widget edit failed: " + error);
    ScreenRect movedQuit;
    if (!ResolveScreenWidgetRect(session.Document(), quitId, movedQuit, error) ||
        movedQuit.x != 420.0f || movedQuit.y != 450.0f ||
        movedQuit.width != 440.0f || movedQuit.height != 80.0f ||
        session.FindWidget(quitId)->layoutMode != ScreenLayoutMode::Anchored ||
        session.FindWidget(quitId)->anchors.minimumX != originalQuitAnchors.minimumX ||
        session.FindWidget(quitId)->anchors.maximumX != originalQuitAnchors.maximumX)
    {
        return Fail("anchored edit did not preserve anchors/resolved layout");
    }

    const std::size_t undoBeforeNoOp = session.UndoCount();
    if (!session.UpdateWidget(quitId, quitEdit, error) ||
        session.UndoCount() != undoBeforeNoOp)
    {
        return Fail("identical edit polluted Screen history");
    }
    ScreenWidgetAuthoringEdit invalidEdit = quitEdit;
    invalidEdit.resolvedRect.width = -1.0f;
    if (session.UpdateWidget(quitId, invalidEdit, error) ||
        session.UndoCount() != undoBeforeNoOp)
    {
        return Fail("invalid basic edit entered Screen history");
    }
    if (!session.Undo(error) || session.FindWidget(quitId)->text != "QUIT" ||
        !session.Redo(error) || session.FindWidget(quitId)->text != "EXIT")
    {
        return Fail("basic Undo/Redo did not restore exact Screen snapshots: " + error);
    }
    if (!session.Save(error) || session.IsDirty())
        return Fail("baseline Save did not mark the Screen clean: " + error);

    ScreenAuthoringSession authored;
    if (!authored.Open(screenPath.generic_u8string(), projectId, error) ||
        authored.FindWidget(titleId)->text != "THE PAGANACHT" ||
        authored.FindWidget(quitId)->text != "EXIT")
    {
        return Fail("baseline Save/Open did not preserve authored content: " + error);
    }

    // Gate 8D foundation creator transactions.
    ScreenWidget creatorText;
    creatorText.kind = ScreenWidgetKind::Text;
    creatorText.name = "Subtitle";
    creatorText.rect = {300.0f, 220.0f, 680.0f, 70.0f};
    creatorText.visible = true;
    creatorText.enabled = false;
    creatorText.text = "CREATOR TEXT";
    creatorText.style = MakeScreenWidgetStyleTemplate(
        creatorText.kind, creatorText.rect.height);
    StableId creatorTextId;
    if (!authored.CreateWidget(creatorText, false, creatorTextId, error) ||
        !IsValidStableId(creatorTextId))
    {
        return Fail("Text creation failed: " + error);
    }

    ScreenWidget creatorButton;
    creatorButton.kind = ScreenWidgetKind::Button;
    creatorButton.name = "Continue";
    creatorButton.rect = {440.0f, 550.0f, 400.0f, 76.0f};
    creatorButton.visible = true;
    creatorButton.enabled = true;
    creatorButton.text = "CONTINUE";
    creatorButton.actionId = RuntimeScreenPlayAction;
    creatorButton.style = MakeScreenWidgetStyleTemplate(
        creatorButton.kind, creatorButton.rect.height);
    StableId creatorButtonId;
    if (!authored.CreateWidget(creatorButton, false, creatorButtonId, error) ||
        authored.Document().focusOrder.back() != creatorButtonId)
    {
        return Fail("Button creation did not establish focus identity: " + error);
    }

    StableId duplicateButtonId;
    if (!authored.DuplicateWidget(creatorButtonId, duplicateButtonId, error) ||
        duplicateButtonId == creatorButtonId ||
        authored.FindWidget(duplicateButtonId)->actionId != RuntimeScreenPlayAction)
    {
        return Fail("Button duplication lost stable/action identity: " + error);
    }
    auto sourceFocus = std::find(
        authored.Document().focusOrder.begin(), authored.Document().focusOrder.end(),
        creatorButtonId);
    auto copyFocus = std::find(
        authored.Document().focusOrder.begin(), authored.Document().focusOrder.end(),
        duplicateButtonId);
    if (sourceFocus == authored.Document().focusOrder.end() ||
        copyFocus != sourceFocus + 1)
    {
        return Fail("duplicated Button was not inserted after source focus entry");
    }

    if (!authored.MoveWidgetToBack(duplicateButtonId, error) ||
        authored.Document().widgets.front().id != duplicateButtonId ||
        !authored.MoveWidgetToFront(duplicateButtonId, error) ||
        authored.Document().widgets.back().id != duplicateButtonId)
    {
        return Fail("layer transactions did not preserve authored order: " + error);
    }

    ScreenWidget creatorImage;
    creatorImage.kind = ScreenWidgetKind::Image;
    creatorImage.name = "Artwork";
    creatorImage.rect = {80.0f, 80.0f, 320.0f, 180.0f};
    creatorImage.visible = false;
    creatorImage.enabled = false;
    creatorImage.style = MakeScreenWidgetStyleTemplate(
        creatorImage.kind, creatorImage.rect.height);
    creatorImage.style.normal.background = {60, 70, 80, 100};
    creatorImage.style.hover = creatorImage.style.normal;
    creatorImage.style.pressed = creatorImage.style.normal;
    creatorImage.style.focused = creatorImage.style.normal;
    creatorImage.style.disabled = creatorImage.style.normal;
    StableId creatorImageId;
    if (!authored.CreateWidget(creatorImage, true, creatorImageId, error) ||
        authored.Document().widgets.front().id != creatorImageId ||
        authored.FindWidget(creatorImageId)->visible ||
        !authored.FindWidget(creatorImageId)->resourcePath.empty())
    {
        return Fail("hidden/unassigned Image creation failed: " + error);
    }

    if (!authored.DeleteWidget(duplicateButtonId, error) ||
        authored.FindWidget(duplicateButtonId) != nullptr ||
        std::find(authored.Document().focusOrder.begin(),
            authored.Document().focusOrder.end(), duplicateButtonId) !=
            authored.Document().focusOrder.end())
    {
        return Fail("deletion did not remove Button/focus identity: " + error);
    }
    if (!authored.Undo(error) || authored.FindWidget(duplicateButtonId) == nullptr ||
        !authored.Redo(error) || authored.FindWidget(duplicateButtonId) != nullptr)
    {
        return Fail("creator transaction Undo/Redo failed: " + error);
    }

    // Advanced style/state/typography transaction.
    auto styleEdit = CreatorEditFor(*authored.FindWidget(creatorButtonId));
    styleEdit.style.normal.background = {21, 22, 23, 24};
    styleEdit.style.hover.background = {1, 2, 3, 4};
    styleEdit.style.pressed.foreground = {5, 6, 7, 8};
    styleEdit.style.focused.imageTint = {9, 10, 11, 12};
    styleEdit.style.disabled.imageResourcePath = "Content/UI/disabled.png";
    styleEdit.style.borderColor = {13, 14, 15, 16};
    styleEdit.style.borderWidth = 3.0f;
    styleEdit.style.cornerRadius = 11.0f;
    styleEdit.style.opacity = 0.75f;
    styleEdit.style.text.fontSize = 33.0f;
    styleEdit.style.text.characterSpacing = 1.5f;
    styleEdit.style.text.lineSpacing = 2.5f;
    styleEdit.style.text.softness = 0.25f;
    styleEdit.style.text.bolden = 0.35f;
    styleEdit.style.text.shadowColor = {17, 18, 19, 20};
    styleEdit.style.text.shadowOffsetX = 2.0f;
    styleEdit.style.text.shadowOffsetY = 4.0f;
    styleEdit.style.text.shadowSoftness = 0.45f;
    styleEdit.style.text.shadowBolden = 0.55f;
    styleEdit.style.text.horizontalAlignment = ScreenHorizontalAlignment::Right;
    styleEdit.style.text.verticalAlignment = ScreenVerticalAlignment::Bottom;
    styleEdit.style.text.wrap = true;
    if (!authored.UpdateWidgetCreatorFields(creatorButtonId, styleEdit, error))
        return Fail("advanced style/typography transaction failed: " + error);
    const auto* styled = authored.FindWidget(creatorButtonId);
    if (!styled || styled->style.normal.background.red != 21 ||
        styled->style.hover.background.green != 2 ||
        styled->style.pressed.foreground.blue != 7 ||
        styled->style.focused.imageTint.alpha != 12 ||
        styled->style.disabled.imageResourcePath != "Content/UI/disabled.png" ||
        styled->style.borderWidth != 3.0f ||
        styled->style.cornerRadius != 11.0f ||
        styled->style.opacity != 0.75f ||
        styled->style.text.fontSize != 33.0f ||
        styled->style.text.horizontalAlignment != ScreenHorizontalAlignment::Right ||
        styled->style.text.verticalAlignment != ScreenVerticalAlignment::Bottom ||
        !styled->style.text.wrap)
    {
        return Fail("advanced authored style/typography values were not exact");
    }

    // Parent/layout authority preserves resolved geometry and rejects cycles.
    ScreenRect textBeforeParent;
    if (!ResolveScreenWidgetRect(authored.Document(), creatorTextId,
            textBeforeParent, error))
        return Fail("could not resolve Text before reparent: " + error);
    auto textParentEdit = CreatorEditFor(*authored.FindWidget(creatorTextId));
    textParentEdit.parentId = creatorImageId;
    textParentEdit.layoutMode = ScreenLayoutMode::Anchored;
    textParentEdit.anchors.minimumX = 0.25f;
    textParentEdit.anchors.minimumY = 0.25f;
    textParentEdit.anchors.maximumX = 0.25f;
    textParentEdit.anchors.maximumY = 0.25f;
    if (!authored.UpdateWidgetCreatorFields(creatorTextId, textParentEdit, error))
        return Fail("legal anchored reparent failed: " + error);
    ScreenRect textAfterParent;
    if (!ResolveScreenWidgetRect(authored.Document(), creatorTextId,
            textAfterParent, error) || !SameRect(textBeforeParent, textAfterParent))
    {
        return Fail("reparent/layout-mode change moved resolved geometry");
    }

    auto selfParent = CreatorEditFor(*authored.FindWidget(creatorImageId));
    selfParent.parentId = creatorImageId;
    if (authored.UpdateWidgetCreatorFields(creatorImageId, selfParent, error) ||
        error.find("cannot parent itself") == std::string::npos)
    {
        return Fail("self-parent mutation was not rejected by authority");
    }
    auto descendantParent = CreatorEditFor(*authored.FindWidget(creatorImageId));
    descendantParent.parentId = creatorTextId;
    if (authored.UpdateWidgetCreatorFields(creatorImageId, descendantParent, error) ||
        error.find("descendants") == std::string::npos)
    {
        return Fail("descendant-parent cycle was not rejected by authority");
    }

    // Add a Button child so reusable subtree duplication proves both parent-ID
    // remapping and focus-order remapping.
    ScreenRect buttonBeforeParent;
    if (!ResolveScreenWidgetRect(authored.Document(), creatorButtonId,
            buttonBeforeParent, error))
        return Fail("could not resolve Button before reparent: " + error);
    auto buttonParentEdit = CreatorEditFor(*authored.FindWidget(creatorButtonId));
    buttonParentEdit.parentId = creatorImageId;
    if (!authored.UpdateWidgetCreatorFields(creatorButtonId, buttonParentEdit, error))
        return Fail("component Button reparent failed: " + error);
    ScreenRect buttonAfterParent;
    if (!ResolveScreenWidgetRect(authored.Document(), creatorButtonId,
            buttonAfterParent, error) || !SameRect(buttonBeforeParent, buttonAfterParent))
    {
        return Fail("component Button reparent changed resolved geometry");
    }

    const std::size_t widgetsBeforeTreeCopy = authored.Document().widgets.size();
    const std::size_t undoBeforeTreeCopy = authored.UndoCount();
    StableId duplicateRootId;
    if (!authored.DuplicateWidgetTree(creatorImageId, duplicateRootId, error) ||
        !IsValidStableId(duplicateRootId) || duplicateRootId == creatorImageId ||
        authored.Document().widgets.size() != widgetsBeforeTreeCopy + 3)
    {
        return Fail("nested reusable component duplication failed: " + error);
    }
    const auto* duplicateRoot = authored.FindWidget(duplicateRootId);
    if (!duplicateRoot || !duplicateRoot->parentId.empty())
        return Fail("duplicated component root lost external-parent contract");

    StableId duplicateTextId;
    StableId duplicateComponentButtonId;
    for (const auto& widget : authored.Document().widgets)
    {
        if (widget.parentId != duplicateRootId) continue;
        if (widget.kind == ScreenWidgetKind::Text) duplicateTextId = widget.id;
        if (widget.kind == ScreenWidgetKind::Button)
            duplicateComponentButtonId = widget.id;
    }
    if (!IsValidStableId(duplicateTextId) ||
        !IsValidStableId(duplicateComponentButtonId) ||
        duplicateTextId == creatorTextId ||
        duplicateComponentButtonId == creatorButtonId ||
        authored.FindWidget(duplicateComponentButtonId)->actionId !=
            RuntimeScreenPlayAction)
    {
        return Fail("component descendants were not remapped with fresh IDs/action");
    }
    sourceFocus = std::find(authored.Document().focusOrder.begin(),
        authored.Document().focusOrder.end(), creatorButtonId);
    copyFocus = std::find(authored.Document().focusOrder.begin(),
        authored.Document().focusOrder.end(), duplicateComponentButtonId);
    if (sourceFocus == authored.Document().focusOrder.end() ||
        copyFocus != sourceFocus + 1)
    {
        return Fail("component Button copy was not inserted after source focus entry");
    }

    if (!authored.Undo(error) || authored.FindWidget(duplicateRootId) != nullptr ||
        authored.UndoCount() != undoBeforeTreeCopy ||
        !authored.Redo(error) || authored.FindWidget(duplicateRootId) == nullptr)
    {
        return Fail("component duplication did not round-trip through Undo/Redo: " + error);
    }

    // Symbolic action identity authoring; routing remains Story Flow-owned.
    if (!authored.AddAction("open_audio_settings", error))
        return Fail("custom action creation failed: " + error);
    if (!authored.RenameAction(RuntimeScreenPlayAction, "start_game", error))
        return Fail("action rename failed: " + error);
    if (authored.FindWidget(playId)->actionId != "start_game" ||
        authored.FindWidget(creatorButtonId)->actionId != "start_game" ||
        authored.FindWidget(duplicateComponentButtonId)->actionId != "start_game")
    {
        return Fail("action rename did not update every Button reference atomically");
    }
    if (authored.DeleteAction("start_game", error) ||
        error.find("still referenced") == std::string::npos)
    {
        return Fail("referenced action deletion was not rejected");
    }
    if (!authored.DeleteAction("open_audio_settings", error))
        return Fail("unreferenced action deletion failed: " + error);

    // Focus order authoring is independent of visual layer order.
    const auto focusBeforeMove = std::find(authored.Document().focusOrder.begin(),
        authored.Document().focusOrder.end(), creatorButtonId);
    if (focusBeforeMove == authored.Document().focusOrder.begin() ||
        focusBeforeMove == authored.Document().focusOrder.end())
    {
        return Fail("fixture Button did not have a movable focus position");
    }
    const StableId precedingFocusId = *(focusBeforeMove - 1);
    if (!authored.MoveButtonFocusEarlier(creatorButtonId, error))
        return Fail("focus move earlier failed: " + error);
    const auto focusAfterEarlier = std::find(authored.Document().focusOrder.begin(),
        authored.Document().focusOrder.end(), creatorButtonId);
    if (focusAfterEarlier == authored.Document().focusOrder.end() ||
        focusAfterEarlier + 1 == authored.Document().focusOrder.end() ||
        *(focusAfterEarlier + 1) != precedingFocusId)
    {
        return Fail("focus move earlier did not swap adjacent Buttons");
    }
    if (!authored.MoveButtonFocusLater(creatorButtonId, error))
        return Fail("focus move later failed: " + error);

    ScreenRect directBefore;
    if (!ResolveScreenWidgetRect(authored.Document(), creatorTextId, directBefore, error)) return Fail("direct geometry fixture failed: " + error);
    const std::size_t directUndo = authored.UndoCount();
    const auto* directWidget = authored.FindWidget(creatorTextId);
    authored.BeginCoalescedEdit();
    ScreenWidgetAuthoringEdit directEdit{directWidget->name, directWidget->text, {directBefore.x + 20.0f, directBefore.y + 10.0f, directBefore.width + 20.0f, directBefore.height + 10.0f}, directWidget->visible, directWidget->enabled};
    if (!authored.UpdateWidget(creatorTextId, directEdit, error)) return Fail("direct frame one failed: " + error);
    directEdit.resolvedRect.x += 20.0f;
    if (!authored.UpdateWidget(creatorTextId, directEdit, error)) return Fail("direct frame two failed: " + error);
    authored.EndCoalescedEdit();
    if (authored.UndoCount() != directUndo + 1) return Fail("direct gesture polluted Undo history");
    if (!authored.Undo(error)) return Fail("direct Undo failed: " + error);
    ScreenRect directUndone;
    if (!ResolveScreenWidgetRect(authored.Document(), creatorTextId, directUndone, error) || !SameRect(directBefore, directUndone)) return Fail("direct Undo geometry mismatch");
    if (!authored.Redo(error)) return Fail("direct Redo failed: " + error);

    if (!authored.Save(error) || authored.IsDirty())
        return Fail("advanced Gate 8D state did not save transactionally: " + error);

    ScreenAuthoringSession persisted;
    if (!persisted.Open(screenPath.generic_u8string(), projectId, error))
        return Fail("advanced Gate 8D Screen did not reopen: " + error);
    const auto* persistedButton = persisted.FindWidget(creatorButtonId);
    const auto* persistedText = persisted.FindWidget(creatorTextId);
    const auto* persistedDuplicateButton =
        persisted.FindWidget(duplicateComponentButtonId);
    if (!persistedButton || !persistedText || !persistedDuplicateButton ||
        !persisted.FindWidget(duplicateRootId) ||
        persistedButton->style.hover.background.green != 2 ||
        persistedButton->style.text.fontSize != 33.0f ||
        persistedButton->actionId != "start_game" ||
        persistedButton->parentId != creatorImageId ||
        persistedText->parentId != creatorImageId ||
        persistedText->layoutMode != ScreenLayoutMode::Anchored ||
        persistedDuplicateButton->parentId != duplicateRootId ||
        persistedDuplicateButton->actionId != "start_game" ||
        std::find(persisted.Document().focusOrder.begin(),
            persisted.Document().focusOrder.end(), duplicateComponentButtonId) ==
            persisted.Document().focusOrder.end())
    {
        return Fail("advanced style/binding/component/action/focus state did not persist");
    }

    std::cout << "PASS: Gate 8D Screen authoring preserves basic and advanced edits, creator transactions, style states, typography, parent/layout geometry, action/focus identity, reusable component remapping, history and transactional Save/Open\n";
    return 0;
}
