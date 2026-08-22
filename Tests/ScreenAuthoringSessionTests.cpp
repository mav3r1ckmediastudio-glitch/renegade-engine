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

    // Gate 8D: every creator operation must be one validated Screen mutation
    // using the same history and persistence seam as Gate 8C Inspector edits.
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
    // hidden. The shared renderer already treats an empty resource as an
    // authored surface; validation must not force Image visibility.
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

    std::cout << "PASS: Gate 8D Screen authoring preserves validated edits, creator transactions, layer/focus identity, hidden Images, history and transactional Save/Open\n";
    return 0;
}
