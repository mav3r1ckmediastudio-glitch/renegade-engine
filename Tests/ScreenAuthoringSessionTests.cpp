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
            "Gate 8C authoring fixture");
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

    ScreenAuthoringSession session;
    if (!session.Open(screenPath.generic_u8string(), projectId, error) ||
        !session.IsLoaded() || session.IsDirty() || session.CanUndo() ||
        session.CanRedo())
    {
        return Fail("Open did not establish a clean authoring session: " + error);
    }

    ScreenRect titleRect;
    if (!ResolveScreenWidgetRect(session.Document(), titleId, titleRect, error))
        return Fail("could not resolve title before edit: " + error);
    ScreenWidgetAuthoringEdit titleEdit;
    titleEdit.name = "Main Title";
    titleEdit.text = "THE PAGANACHT";
    titleEdit.resolvedRect = {220.0f, 90.0f, 840.0f, 110.0f};
    titleEdit.visible = true;
    titleEdit.enabled = true;
    if (!session.UpdateWidget(titleId, titleEdit, error) ||
        !session.IsDirty() || !session.CanUndo() || session.CanRedo())
    {
        return Fail("absolute widget edit did not enter history: " + error);
    }
    const auto* title = session.FindWidget(titleId);
    if (!title || title->name != "Main Title" ||
        title->text != "THE PAGANACHT" ||
        title->rect.x != 220.0f || title->rect.width != 840.0f)
    {
        return Fail("absolute widget edit was not applied exactly");
    }

    ScreenRect quitRect;
    if (!ResolveScreenWidgetRect(session.Document(), quitId, quitRect, error))
        return Fail("could not resolve anchored button before edit: " + error);
    ScreenWidgetAuthoringEdit quitEdit;
    quitEdit.name = "Exit Game";
    quitEdit.text = "EXIT";
    quitEdit.resolvedRect = {420.0f, 450.0f, 440.0f, 80.0f};
    quitEdit.visible = true;
    quitEdit.enabled = true;
    if (!session.UpdateWidget(quitId, quitEdit, error))
        return Fail("anchored widget edit failed: " + error);
    ScreenRect movedQuit;
    if (!ResolveScreenWidgetRect(
            session.Document(), quitId, movedQuit, error) ||
        movedQuit.x != 420.0f || movedQuit.y != 450.0f ||
        movedQuit.width != 440.0f || movedQuit.height != 80.0f ||
        session.FindWidget(quitId)->layoutMode != ScreenLayoutMode::Anchored ||
        session.FindWidget(quitId)->anchors.minimumX !=
            originalQuitAnchors.minimumX ||
        session.FindWidget(quitId)->anchors.minimumY !=
            originalQuitAnchors.minimumY ||
        session.FindWidget(quitId)->anchors.maximumX !=
            originalQuitAnchors.maximumX ||
        session.FindWidget(quitId)->anchors.maximumY !=
            originalQuitAnchors.maximumY)
    {
        return Fail("anchored edit did not preserve anchors and resolved layout");
    }

    const std::size_t undoBeforeNoOp = session.UndoCount();
    if (!session.UpdateWidget(quitId, quitEdit, error) ||
        session.UndoCount() != undoBeforeNoOp)
    {
        return Fail("identical edit polluted Screen history");
    }

    ScreenWidgetAuthoringEdit invalid = quitEdit;
    invalid.resolvedRect.width = -1.0f;
    if (session.UpdateWidget(quitId, invalid, error) ||
        session.UndoCount() != undoBeforeNoOp)
    {
        return Fail("invalid edit entered Screen history");
    }

    if (!session.Undo(error) || session.FindWidget(quitId)->text != "QUIT" ||
        !session.Undo(error) || session.FindWidget(titleId)->text != "RENEGADE" ||
        !session.Redo(error) || session.FindWidget(titleId)->text != "THE PAGANACHT" ||
        !session.Redo(error) || session.FindWidget(quitId)->text != "EXIT")
    {
        return Fail("Undo/Redo did not restore exact Screen snapshots: " + error);
    }

    if (!session.Save(error) || session.IsDirty())
        return Fail("transactional Save did not mark the Screen clean: " + error);

    ScreenAuthoringSession reopened;
    if (!reopened.Open(screenPath.generic_u8string(), projectId, error))
        return Fail("saved Screen did not reopen: " + error);
    ScreenRect reopenedQuit;
    if (!ResolveScreenWidgetRect(
            reopened.Document(), quitId, reopenedQuit, error) ||
        reopened.FindWidget(titleId)->text != "THE PAGANACHT" ||
        reopened.FindWidget(quitId)->text != "EXIT" ||
        reopenedQuit.x != 420.0f || reopenedQuit.width != 440.0f)
    {
        return Fail("saved Screen did not preserve authored content/layout");
    }

    // Gate 8D foundation: creation/deletion/duplication/layering all use the
    // same validated history and persistence seam as Gate 8C Inspector edits.
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
    if (!reopened.CreateWidget(
            creatorText, false, creatorTextId, error) ||
        !IsValidStableId(creatorTextId) ||
        !reopened.FindWidget(creatorTextId) ||
        reopened.FindWidget(creatorTextId)->text != "CREATOR TEXT")
    {
        return Fail("Gate 8D Text creation did not commit a valid widget: " + error);
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
    if (!reopened.CreateWidget(
            creatorButton, false, creatorButtonId, error) ||
        reopened.Document().focusOrder.back() != creatorButtonId ||
        reopened.FindWidget(creatorButtonId)->actionId != RuntimeScreenPlayAction)
    {
        return Fail("Gate 8D Button creation did not preserve action/focus identity: " + error);
    }

    StableId duplicateButtonId;
    if (!reopened.DuplicateWidget(
            creatorButtonId, duplicateButtonId, error) ||
        duplicateButtonId == creatorButtonId ||
        reopened.FindWidget(duplicateButtonId) == nullptr ||
        reopened.FindWidget(duplicateButtonId)->actionId != RuntimeScreenPlayAction)
    {
        return Fail("Gate 8D Button duplication lost stable/action identity: " + error);
    }
    const auto duplicateFocus = std::find(
        reopened.Document().focusOrder.begin(),
        reopened.Document().focusOrder.end(), duplicateButtonId);
    const auto creatorFocus = std::find(
        reopened.Document().focusOrder.begin(),
        reopened.Document().focusOrder.end(), creatorButtonId);
    if (duplicateFocus == reopened.Document().focusOrder.end() ||
        creatorFocus == reopened.Document().focusOrder.end() ||
        duplicateFocus != creatorFocus + 1)
    {
        return Fail("Gate 8D duplicated Button was not inserted after its source in focus order");
    }

    if (!reopened.MoveWidgetToBack(duplicateButtonId, error) ||
        reopened.Document().widgets.front().id != duplicateButtonId ||
        !reopened.MoveWidgetToFront(duplicateButtonId, error) ||
        reopened.Document().widgets.back().id != duplicateButtonId)
    {
        return Fail("Gate 8D layer transactions did not preserve authored order: " + error);
    }

    // Images may exist before a resource is chosen and may be deliberately
    // hidden. The shared renderer treats an empty resource as an authored
    // surface; validation must not force Image visibility.
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
    if (!reopened.CreateWidget(
            creatorImage, true, creatorImageId, error) ||
        reopened.Document().widgets.front().id != creatorImageId ||
        reopened.FindWidget(creatorImageId)->visible ||
        !reopened.FindWidget(creatorImageId)->resourcePath.empty())
    {
        return Fail("Gate 8D hidden/unassigned Image creation was rejected: " + error);
    }

    if (!reopened.DeleteWidget(duplicateButtonId, error) ||
        reopened.FindWidget(duplicateButtonId) != nullptr ||
        std::find(
            reopened.Document().focusOrder.begin(),
            reopened.Document().focusOrder.end(), duplicateButtonId) !=
            reopened.Document().focusOrder.end())
    {
        return Fail("Gate 8D deletion did not remove duplicated Button/focus identity: " + error);
    }

    if (!reopened.Undo(error) || reopened.FindWidget(duplicateButtonId) == nullptr ||
        !reopened.Redo(error) || reopened.FindWidget(duplicateButtonId) != nullptr)
    {
        return Fail("Gate 8D creator transactions did not participate in Screen Undo/Redo: " + error);
    }

    if (!reopened.Save(error) || reopened.IsDirty())
        return Fail("Gate 8D creator state did not save transactionally: " + error);

    ScreenAuthoringSession creatorReopened;
    if (!creatorReopened.Open(screenPath.generic_u8string(), projectId, error) ||
        !creatorReopened.FindWidget(creatorTextId) ||
        !creatorReopened.FindWidget(creatorButtonId) ||
        !creatorReopened.FindWidget(creatorImageId) ||
        creatorReopened.FindWidget(creatorImageId)->visible ||
        creatorReopened.FindWidget(creatorButtonId)->actionId != RuntimeScreenPlayAction ||
        std::find(
            creatorReopened.Document().focusOrder.begin(),
            creatorReopened.Document().focusOrder.end(), creatorButtonId) ==
            creatorReopened.Document().focusOrder.end())
    {
        return Fail("Gate 8D creator transactions did not survive Save/Open: " + error);
    }

    // Gate 8D advanced appearance: one mutation carries every schema-v2 style
    // family through the same history/persistence authority.
    auto advancedButtonEdit = CreatorEditFor(
        *creatorReopened.FindWidget(creatorButtonId));
    advancedButtonEdit.style.hover.background = {1, 2, 3, 4};
    advancedButtonEdit.style.pressed.foreground = {5, 6, 7, 8};
    advancedButtonEdit.style.focused.imageTint = {9, 10, 11, 12};
    advancedButtonEdit.style.disabled.imageResourcePath =
        "Content/UI/disabled.png";
    advancedButtonEdit.style.borderColor = {13, 14, 15, 16};
    advancedButtonEdit.style.borderWidth = 3.0f;
    advancedButtonEdit.style.cornerRadius = 11.0f;
    advancedButtonEdit.style.opacity = 0.75f;
    advancedButtonEdit.style.text.fontSize = 33.0f;
    advancedButtonEdit.style.text.characterSpacing = 1.5f;
    advancedButtonEdit.style.text.lineSpacing = 2.5f;
    advancedButtonEdit.style.text.softness = 0.25f;
    advancedButtonEdit.style.text.bolden = 0.35f;
    advancedButtonEdit.style.text.shadowColor = {17, 18, 19, 20};
    advancedButtonEdit.style.text.shadowOffsetX = 2.0f;
    advancedButtonEdit.style.text.shadowOffsetY = 4.0f;
    advancedButtonEdit.style.text.shadowSoftness = 0.45f;
    advancedButtonEdit.style.text.shadowBolden = 0.55f;
    advancedButtonEdit.style.text.horizontalAlignment =
        ScreenHorizontalAlignment::Right;
    advancedButtonEdit.style.text.verticalAlignment =
        ScreenVerticalAlignment::Bottom;
    advancedButtonEdit.style.text.wrap = true;
    if (!creatorReopened.UpdateWidgetCreatorFields(
            creatorButtonId, advancedButtonEdit, error))
    {
        return Fail("advanced style/typography transaction failed: " + error);
    }
    const auto* styledButton = creatorReopened.FindWidget(creatorButtonId);
    if (!styledButton || styledButton->style.hover.background.red != 1 ||
        styledButton->style.pressed.foreground.green != 6 ||
        styledButton->style.focused.imageTint.blue != 11 ||
        styledButton->style.disabled.imageResourcePath !=
            "Content/UI/disabled.png" ||
        styledButton->style.borderWidth != 3.0f ||
        styledButton->style.cornerRadius != 11.0f ||
        styledButton->style.opacity != 0.75f ||
        styledButton->style.text.fontSize != 33.0f ||
        styledButton->style.text.horizontalAlignment !=
            ScreenHorizontalAlignment::Right ||
        styledButton->style.text.verticalAlignment !=
            ScreenVerticalAlignment::Bottom ||
        !styledButton->style.text.wrap)
    {
        return Fail("advanced style/typography values were not committed exactly");
    }

    // Legal reparenting and layout-mode changes preserve resolved design-space
    // geometry. Parent authority rejects self/descendant cycles before Runtime
    // validation ever sees them.
    ScreenRect textBeforeParent;
    if (!ResolveScreenWidgetRect(
            creatorReopened.Document(), creatorTextId, textBeforeParent, error))
        return Fail("could not resolve component child before reparent: " + error);
    auto textParentEdit = CreatorEditFor(
        *creatorReopened.FindWidget(creatorTextId));
    textParentEdit.parentId = creatorImageId;
    textParentEdit.layoutMode = ScreenLayoutMode::Anchored;
    textParentEdit.anchors.minimumX = 0.25f;
    textParentEdit.anchors.minimumY = 0.25f;
    textParentEdit.anchors.maximumX = 0.25f;
    textParentEdit.anchors.maximumY = 0.25f;
    if (!creatorReopened.UpdateWidgetCreatorFields(
            creatorTextId, textParentEdit, error))
    {
        return Fail("legal anchored reparent failed: " + error);
    }
    ScreenRect textAfterParent;
    if (!ResolveScreenWidgetRect(
            creatorReopened.Document(), creatorTextId, textAfterParent, error) ||
        !SameRect(textBeforeParent, textAfterParent))
    {
        return Fail("reparent/layout-mode change moved resolved Screen geometry");
    }

    auto selfParent = CreatorEditFor(*creatorReopened.FindWidget(creatorImageId));
    selfParent.parentId = creatorImageId;
    if (creatorReopened.UpdateWidgetCreatorFields(
            creatorImageId, selfParent, error) ||
        error.find("cannot parent itself") == std::string::npos)
    {
        return Fail("self-parent mutation was not rejected by authoring authority");
    }

    auto descendantParent = CreatorEditFor(
        *creatorReopened.FindWidget(creatorImageId));
    descendantParent.parentId = creatorTextId;
    if (creatorReopened.UpdateWidgetCreatorFields(
            creatorImageId, descendantParent, error) ||
        error.find("descendants") == std::string::npos)
    {
        return Fail("descendant-parent cycle was not rejected by authoring authority");
    }

    // Put a Button inside the same reusable component subtree so duplication
    // proves stable ID remapping and focus-order insertion together.
    ScreenRect buttonBeforeParent;
    if (!ResolveScreenWidgetRect(
            creatorReopened.Document(), creatorButtonId, buttonBeforeParent, error))
        return Fail("could not resolve component Button before reparent: " + error);
    auto buttonParentEdit = CreatorEditFor(
        *creatorReopened.FindWidget(creatorButtonId));
    buttonParentEdit.parentId = creatorImageId;
    if (!creatorReopened.UpdateWidgetCreatorFields(
            creatorButtonId, buttonParentEdit, error))
    {
        return Fail("component Button reparent failed: " + error);
    }
    ScreenRect buttonAfterParent;
    if (!ResolveScreenWidgetRect(
            creatorReopened.Document(), creatorButtonId, buttonAfterParent, error) ||
        !SameRect(buttonBeforeParent, buttonAfterParent))
    {
        return Fail("component Button reparent changed resolved geometry");
    }

    const std::size_t widgetsBeforeTreeCopy =
        creatorReopened.Document().widgets.size();
    const std::size_t undoBeforeTreeCopy = creatorReopened.UndoCount();
    StableId duplicateRootId;
    if (!creatorReopened.DuplicateWidgetTree(
            creatorImageId, duplicateRootId, error) ||
        !IsValidStableId(duplicateRootId) || duplicateRootId == creatorImageId ||
        creatorReopened.Document().widgets.size() != widgetsBeforeTreeCopy + 3)
    {
        return Fail("nested reusable component duplication failed: " + error);
    }
    const auto* duplicateRoot = creatorReopened.FindWidget(duplicateRootId);
    if (!duplicateRoot || !duplicateRoot->parentId.empty())
        return Fail("duplicated component root lost its external parent contract");

    StableId duplicateTextId;
    StableId duplicateComponentButtonId;
    for (const auto& widget : creatorReopened.Document().widgets)
    {
        if (widget.parentId != duplicateRootId) continue;
        if (widget.kind == ScreenWidgetKind::Text)
            duplicateTextId = widget.id;
        if (widget.kind == ScreenWidgetKind::Button)
            duplicateComponentButtonId = widget.id;
    }
    if (!IsValidStableId(duplicateTextId) ||
        !IsValidStableId(duplicateComponentButtonId) ||
        duplicateTextId == creatorTextId ||
        duplicateComponentButtonId == creatorButtonId ||
        creatorReopened.FindWidget(duplicateComponentButtonId)->actionId !=
            RuntimeScreenPlayAction)
    {
        return Fail("component descendants were not remapped with fresh stable IDs/action identity");
    }

    const auto originalComponentFocus = std::find(
        creatorReopened.Document().focusOrder.begin(),
        creatorReopened.Document().focusOrder.end(), creatorButtonId);
    const auto duplicateComponentFocus = std::find(
        creatorReopened.Document().focusOrder.begin(),
        creatorReopened.Document().focusOrder.end(), duplicateComponentButtonId);
    if (originalComponentFocus == creatorReopened.Document().focusOrder.end() ||
        duplicateComponentFocus == creatorReopened.Document().focusOrder.end() ||
        duplicateComponentFocus != originalComponentFocus + 1)
    {
        return Fail("component Button copy was not inserted after source focus entry");
    }

    if (!creatorReopened.Undo(error) ||
        creatorReopened.FindWidget(duplicateRootId) != nullptr ||
        creatorReopened.UndoCount() + 1 != undoBeforeTreeCopy ||
        !creatorReopened.Redo(error) ||
        creatorReopened.FindWidget(duplicateRootId) == nullptr)
    {
        return Fail("component duplication did not round-trip through Undo/Redo: " + error);
    }

    // Gate 8D owns symbolic action identity only. Rename updates every Button
    // reference atomically; Story Flow destination routing stays outside Screen.
    if (!creatorReopened.AddAction("open_audio_settings", error))
        return Fail("custom Screen action creation failed: " + error);
    if (!creatorReopened.RenameAction(
            RuntimeScreenPlayAction, "start_game", error))
    {
        return Fail("Screen action rename failed: " + error);
    }
    if (creatorReopened.FindWidget(playId)->actionId != "start_game" ||
        creatorReopened.FindWidget(creatorButtonId)->actionId != "start_game" ||
        creatorReopened.FindWidget(duplicateComponentButtonId)->actionId !=
            "start_game")
    {
        return Fail("action rename did not update all Button references atomically");
    }
    if (creatorReopened.DeleteAction("start_game", error) ||
        error.find("still referenced") == std::string::npos)
    {
        return Fail("referenced Screen action deletion was not rejected");
    }
    if (!creatorReopened.DeleteAction("open_audio_settings", error))
        return Fail("unreferenced custom Screen action could not be deleted: " + error);

    // Focus order is creator-authorable independently of visual layer order.
    const auto focusBeforeMove = std::find(
        creatorReopened.Document().focusOrder.begin(),
        creatorReopened.Document().focusOrder.end(), creatorButtonId);
    if (focusBeforeMove == creatorReopened.Document().focusOrder.begin() ||
        focusBeforeMove == creatorReopened.Document().focusOrder.end())
    {
        return Fail("fixture Button did not have a movable focus position");
    }
    const StableId precedingFocusId = *(focusBeforeMove - 1);
    if (!creatorReopened.MoveButtonFocusEarlier(creatorButtonId, error))
        return Fail("creator focus-order move earlier failed: " + error);
    const auto focusAfterEarlier = std::find(
        creatorReopened.Document().focusOrder.begin(),
        creatorReopened.Document().focusOrder.end(), creatorButtonId);
    if (focusAfterEarlier == creatorReopened.Document().focusOrder.end() ||
        focusAfterEarlier + 1 == creatorReopened.Document().focusOrder.end() ||
        *(focusAfterEarlier + 1) != precedingFocusId)
    {
        return Fail("creator focus-order move earlier did not swap adjacent Buttons");
    }
    if (!creatorReopened.MoveButtonFocusLater(creatorButtonId, error))
        return Fail("creator focus-order move later failed: " + error);

    if (!creatorReopened.Save(error) || creatorReopened.IsDirty())
        return Fail("advanced Gate 8D state did not save transactionally: " + error);

    ScreenAuthoringSession advancedReopened;
    if (!advancedReopened.Open(screenPath.generic_u8string(), projectId, error))
        return Fail("advanced Gate 8D Screen did not reopen: " + error);
    const auto* persistedButton = advancedReopened.FindWidget(creatorButtonId);
    const auto* persistedText = advancedReopened.FindWidget(creatorTextId);
    const auto* persistedDuplicateRoot = advancedReopened.FindWidget(duplicateRootId);
    if (!persistedButton || !persistedText || !persistedDuplicateRoot ||
        persistedButton->style.hover.background.red != 1 ||
        persistedButton->style.text.fontSize != 33.0f ||
        persistedButton->actionId != "start_game" ||
        persistedButton->parentId != creatorImageId ||
        persistedText->parentId != creatorImageId ||
        persistedText->layoutMode != ScreenLayoutMode::Anchored ||
        advancedReopened.FindWidget(duplicateComponentButtonId) == nullptr ||
        advancedReopened.FindWidget(duplicateComponentButtonId)->parentId !=
            duplicateRootId ||
        advancedReopened.FindWidget(duplicateComponentButtonId)->actionId !=
            "start_game" ||
        std::find(
            advancedReopened.Document().focusOrder.begin(),
            advancedReopened.Document().focusOrder.end(),
            duplicateComponentButtonId) == advancedReopened.Document().focusOrder.end())
    {
        return Fail("advanced Gate 8D style/binding/component/action/focus state did not survive Save/Open");
    }

    std::cout << "PASS: Gate 8D Screen authoring preserves basic and advanced edits, creator transactions, style states, typography, parent/layout geometry, action/focus identity, reusable component remapping, history and transactional Save/Open\n";
    return 0;
}
